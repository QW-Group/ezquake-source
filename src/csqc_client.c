/*
csqc_client.c -- client-side PR1VM wrapper for our csprogs.dat (CSQC).

Loads the local csprogs.dat into a static client PR1VM instance when the server
offers CSQC (*csprogs / *csprogssize + FTE_PEXT_CSQC), registers the client
builtins and runs CSQC_Init; then CSQC_WorldLoaded on entering the world and
CSQC_UpdateView(w,h,menushown) each frame. registercommand -> Cmd_AddCommand;
executing a command -> CSQC_ConsoleCommand; on disconnect -> CSQC_Shutdown +
PR1VM_UnLoad and removal of the commands.
*/

#ifndef CLIENTONLY
#include "quakedef.h"	// client.h (cl.stats), draw.h, vid.h, common.h (Cmd_*)
#include "keys.h"		// key_dest / key_menu
#include "pr1vm.h"
#include "csqc_client.h"
#include "pmove.h"		// playermove_t/pmove/movevars/PM_PlayerMove (#347)
#include "common_draw.h"	// CachePic_Find/Remove, Draw_EnableScissorRectangle/DisableScissor
#include "r_texture.h"		// R_LoadPicImage/TEX_ALPHA (#318)
#include "r_matrix.h"		// R_Project3DCoordinates/R_Get*Matrix (#310/#311)
#include "gl_model.h"		// model_t mins/maxs (#504 getentity)
#include "r_renderer.h"		// R_RendererDescription (CSQC_RendererRestarted)
#include "input.h"		// CL_SendClientCommand (enablecsqc/disablecsqc)
#include "version.h"		// VERSION_NUM (CSQC_Init enginever)

// CSQC API level the engine reports to the module in CSQC_Init (FTE parity:
// CSQC_API_VERSION is 1.0).
#ifndef CSQC_API_VERSION
#define CSQC_API_VERSION	1.0f
#endif

// Entity pool (slot != server number): CSQC_MAX_NUM is the upper bound of server
// numbers (number->slot map), CSQC_MAX_EDICTS is the arena edict-slot pool size
// (slot 0 = world, not managed). The module's .entnum field = server number;
// module-spawned entities have no number (.entnum=0).
#define CSQC_MAX_NUM	4096
#define CSQC_MAX_EDICTS	4096

// Untrusted-csprogs validator caps (client-only; ADR 0031). CSQC_MAX_BUILTINS is
// the upper bound of client builtin slots (max registered builtin number is 742,
// csqc_builtins.c; first_statement == -num is a builtin). CSQC_MAX_ENTITYFIELDS
// bounds entityfields so edict_size (entityfields*4, pr_edict.c) and the
// CSQC_MAX_EDICTS-slot arena (max ~64 MB) cannot overflow (real module: 109).
// Both leave generous headroom over the real module.
#define CSQC_MAX_BUILTINS		1024
#define CSQC_MAX_ENTITYFIELDS	4096

// Client string tables + temp ring of the client VM instance. Kept outside the
// shared pr1vm_t (the core only holds pointers to them in vm->) so the shared
// core carries no client data/logic. Ring size = number of unique temp strings
// alive until their slot is overwritten.
#define CSQC_TEMP_STRINGS		64
#define CSQC_TEMP_STRING_SIZE	2048
typedef struct csqc_strpool_s
{
	char	*strtbl[MAX_PRSTR];
	char	*newstrtbl[MAX_PRSTR];
	int		numstr;
	// Temp strings deep-copy into the next ring slot: each call gets its own
	// stable buffer (the builtin result aliases neither the source nor past
	// results; the slot is overwritten by later calls).
	char	tmpstr[CSQC_TEMP_STRINGS][CSQC_TEMP_STRING_SIZE];
	int		tmpstr_cur;
} csqc_strpool_t;

typedef struct csqc_client_state_s
{
	pr1vm_t		vm;
	qbool		loaded;		// module loaded into the instance
	qbool		inited;		// CSQC_Init called
	// Cached csqc_dbg cvar pointer (the module registers it in CSQC_Init via
	// registercvar; resolved after init so Cvar_Find is not called per entity).
	cvar_t		*csqc_dbg_cvar;
	qbool		errored;	// PR_RunError on the client instance (frames disabled)
	qbool		mayread;	// module may read the net message - parse callbacks only
						// (CSQC_Ent_Update/CSQC_Parse_Event; FTE csqc_mayread)
	qbool		world_done;	// CSQC_WorldLoaded called
	qbool		enable_sent;	// enablecsqc/disablecsqc already sent to the server
	qbool		enable_value;	// last sent state (true=enablecsqc)
	qbool		seen[CSQC_MAX_NUM];	// known CSQC entities (isnew for Ent_Update)
	int			func_init, func_world, func_update, func_console, func_shutdown;
	int			func_entupdate, func_entremove, func_parseevent;
	int			func_parseprint, func_parsecp;	// CSQC_Parse_Print / CSQC_Parse_CenterPrint
	int			func_parsedamage;	// CSQC_Parse_Damage (or -1)
	int			func_eventsound;	// CSQC_Event_Sound (or -1)
	int			func_parsesetangles;	// CSQC_Parse_SetAngles (or -1)
	int			func_rr;	// CSQC_RendererRestarted (or -1)
	int			func_entspawn;	// CSQC_Ent_Spawn (or -1; FTE parity)
	int			func_input;		// CSQC_Input_Frame (or -1)
	int			func_inputevent;	// CSQC_InputEvent (or -1)
	int			func_startframe;	// CSQC StartFrame (or -1)
	int			func_endframe;		// CSQC EndFrame (or -1)
	int			global_time;	// offset of the time global (or -1)
	int			global_gamespeed;	// offset of the gamespeed global (or -1)
	int			global_self;	// offset of the self global (or -1)
	int			global_other;	// offset of the other global (or -1; think-loop)
	int			global_physics_mode;	// offset of the physics_mode global (or -1)
	int			field_entnum;	// float word of the .entnum field in entvars (or -1)
	// #347: standard physics field offsets (or -1).
	int			f_origin, f_velocity, f_angles, f_mins, f_maxs;
	int			f_movetype, f_flags, f_gravity, f_pmove_flags;
	int			f_modelindex, f_skin;	// #371 player/delta bridge (raw state fields)
	int			f_frame, f_effects, f_colormap, f_drawmask;	// #371 bridge (raw state fields)
	int			f_think, f_nextthink;	// think-loop: .think/.nextthink fields (or -1)
	// Player mirroring is removed (the entity environment matches FTE without
	// server-side player emission - ezquake does not fabricate player entities).
	// Only player_localentnum is published (FTE).
	int			g_localentnum;	// module global player_localentnum (or -1)
	// input_* globals for CSQC_Input_Frame (or -1 if the module did not declare them).
	int			in_timelength, in_angles, in_movevalues, in_buttons, in_impulse;
	int			in_sequence;	// input_sequence (#345) or -1
	// Prediction window globals (or -1).
	int			g_ccframe;		// clientcommandframe
	int			g_scframe;		// servercommandframe
	// Deprecated pmove_org/pmove_vel/pmove_onground globals (or -1; #347 writes them).
	int			p_org, p_vel, p_onground;
	// #1 makevectors: module v_forward/v_right/v_up globals (or -1).
	int			g_vfwd, g_vright, g_vup;
	int			g_view_angles;	// view_angles global (or -1)
	// Simulated FTE-level globals (or -1): frametime/cltime/maxclients/
	// player_localnum/intermission.
	int			g_frametime, g_cltime, g_maxclients, g_player_localnum, g_intermission;
	// csprogs download (no valid local file): download *csprogsname from the
	// server into csprogsvers/<crc>.dat (as FTE does); load once a valid file
	// appears (see CSQC_Client_Update).
	qbool		csprogs_dl_pending;
	// Timeout by absence of progress rather than a flat 20 s from start.
	double		csprogs_dl_lastprogress;	// time of last downloadpercent growth
	int			csprogs_dl_percent;			// last seen cls.downloadpercent
	qbool		csprogs_dl_started;			// our download has opened (cls.download)
	char		csprogs_dl_localname[MAX_OSPATH];	// cls.downloadname of our file (gate)
	unsigned	csprogs_crc;	// *csprogs (md4 Com_BlockChecksum) / 0 if absent
	int			csprogs_size;	// *csprogssize
	char		csprogs_dl_path[MAX_QPATH];	// local file after download
	int			numcmds;	// number of module-registered commands
	int			maxcmds;	// cmds capacity (dynamic)
	char		**cmds;		// Q_malloc: module command names (removed on unload)
	// Edict arena of the client instance. Q_malloc; freed in Disconnect/Load-start;
	// bound into vm->edicts/game_edicts (entity opcodes).
	edict_t		*edicts;
	byte		*game_edicts;
	// Heap-owned csprogs.dat buffer (FS_LoadHeapFile). vm->progs/strings/globals
	// reference it, so it must outlive the module; freed on unload (Disconnect)
	// and on load failure.
	byte		*module_data;
	// Client string tables of the instance (see csqc_strpool_t): on load
	// vm->strtbl/newstrtbl/numstr point here.
	csqc_strpool_t strpool;
} csqc_client_state_t;

static csqc_client_state_t s_csqc;

// Client PR1VM helpers (client parts live outside shared core files):
// LoadClientV6 + CSQCSmoke are implemented here.
static qbool PR1VM_LoadClientV6 (pr1vm_t *vm, const byte *data, int filesize);
#ifdef CSQC_DEBUG
static void PR1VM_CSQCSmoke_f (void);
static void CSQC_Client_ProgsCheck_f (void);
static void CSQC_Client_NetProbe_f (void);
#endif

/*
=================
PR1VM_ClientSetString

Client wrapper over the shared PR1VM_SetString (core): temp strings deep-copy into
the per-instance ring (stable buffer), then the core registers the pointer in
vm->strtbl. Overflow is client policy (silent bail). Strings from the module's
string area are passed to the core without a copy (offset). Named PR1VM- because it
works with the PR1 VM (as opposed to PR2).
=================
*/
void PR1VM_ClientSetString (pr1vm_t *vm, int *address, char *s)
{
	csqc_strpool_t *pool;
	char *dst;

	if (!address)
		return;

	if (!s || !s[0])
	{
		*address = 0;
		return;
	}

	pool = (csqc_strpool_t *)vm->host_udata;
	if (!pool || !vm->strings || !vm->strtbl || !vm->numstr)
		return;

	// Already inside the module string area - the core writes the offset itself.
	if (s >= vm->strings && s < vm->strings + vm->progs->numstrings)
	{
		PR1VM_SetString (vm, (string_t *)address, s);
		return;
	}

	// Temp string: deep-copy into the next ring slot (the buffer is stable for the
	// instance; the slot is overwritten by later calls).
	dst = pool->tmpstr[pool->tmpstr_cur];
	pool->tmpstr_cur = (pool->tmpstr_cur + 1) % CSQC_TEMP_STRINGS;
	strlcpy (dst, s, CSQC_TEMP_STRING_SIZE);

	if (*vm->numstr + 1 >= MAX_PRSTR)
		return;	// client: no fatal

	PR1VM_SetString (vm, (string_t *)address, dst);
}

// #345: ring buffer of sent usercmds (written from CL_SendCmd).
// seq = mirror of cls.netchan.outgoing_sequence (the client message number at
// write time; Netchan_Transmit increments AFTER writing the header, so during
// CL_SendCmd outgoing_sequence still equals the current cmd's number).
// Size 64 = UPDATE_BACKUP (prediction window).
#define CSQC_INHIST	64
typedef struct { unsigned int seq; usercmd_t cmd; } csqc_inrec_t;
static csqc_inrec_t s_inhist[CSQC_INHIST];
static unsigned int s_last_seq;	// seq of the last write (0 = none yet)
// The "live" clientcommandframe = seq of the last built cmd (FTE cl.movesequence;
// cl_input.c sets it at build). NOT the next outgoing_sequence: Netchan_Transmit
// increments after sending, so in the render phase outgoing_sequence is already
// N+1 while FTE ccframe stays N ("movesequence+1 ... still pending"). Updated in
// CSQC_Client_InputFrame.
static unsigned int s_ccframe;	// 0 = no cmd built yet

// #460-469: string-buffer pool (DP). Strings are deep-copied (survive frames).
#define CSQC_MAX_BUFS	64
typedef struct
{
	qbool	inuse;
	int		num;
	int		cap;
	char	**str;
} csqc_buf_t;
static csqc_buf_t s_bufs[CSQC_MAX_BUFS];

// #346 setsensitivityscaler: temporary mouse-sensitivity multiplier (the FTE
// in_sensitivityscale zoom analog). Stored by the module; applied in in_sdl2.c.
static float s_sens_scale = 1;

// #343 setcursormode: the module's cursor state. While usecursor=1 and the module
// is active in-game (CSQC_Client_CSQCCursor), the mouse is free (vid_sdl2 does not
// hand it to the OS cursor) and SCR_DrawCursor draws the module's cursor.
typedef struct
{
	qbool	usecursor;
	char	cursorimage[MAX_QPATH];
	float	hotspot[2];
	float	scale;
} csqc_cursormode_t;
static csqc_cursormode_t s_cursormode;

// Client edict arena: an arbitrary slot pool; the server number lives in .entnum
// (map s_numslot: number->slot). Slot 0 = world. s_own - entity created by the
// module (spawn); remove is allowed only for owned entities.
static qbool s_used[CSQC_MAX_EDICTS];
static qbool s_own[CSQC_MAX_EDICTS];
static int s_numslot[CSQC_MAX_NUM];
// Reverse map slot->number. Invariant: s_numslot[N] must not outlive the slot's
// release - otherwise the engine would take a stale slot.
static int s_slotnum[CSQC_MAX_EDICTS];

// Extended CSQC stats 32..255 (clientstat/pointerstat from mvdsv). Standard 0..31
// live in cl.stats[] (client struct); extended ones are stored here (see
// CSQC_Client_GetStat/SetStat). Stat wire 78/79 stores float/string stats:
// statsf - exact value (accepted both from 79 and from the int path svc_updatestat),
// statss - string (Q_strdup, freed in CSQC_Client_Disconnect).
static int s_csqc_stat[MAX_EXTENDED_CL_STATS];
static float s_csqc_statsf[MAX_EXTENDED_CL_STATS];
static char *s_csqc_statss[MAX_EXTENDED_CL_STATS];

/*
=================
CSQC_Client_GetStat / SetStat / GetScreenSize / DrawText / RegisterCommand
Accessors for csqc_builtins.c and cl_parse.c (see csqc_client.h).
=================
*/
float CSQC_Client_GetStat (int idx)
{
	if (idx >= 0 && idx < 32)
		return (float)cl.stats[idx];
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
		return (float)s_csqc_stat[idx];
	return 0;
}

int CSQC_Client_GetStatInt (int idx)
{
	if (idx >= 0 && idx < 32)
		return cl.stats[idx];
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
		return s_csqc_stat[idx];
	return 0;
}

float CSQC_Client_GetStatFloat (int idx)
{
	if (idx >= 0 && idx < 32)
		return (float)cl.stats[idx];
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
		return s_csqc_statsf[idx];
	return 0;
}

const char *CSQC_Client_GetStatString (int idx)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS && s_csqc_statss[idx])
		return s_csqc_statss[idx];
	return "";
}

void CSQC_Client_SetStat (int idx, int value)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
	{
		s_csqc_stat[idx] = value;
		// The server keeps an int cache in sync when emitting a float stat as an
		// int - getstatf must see the same value.
		s_csqc_statsf[idx] = (float)value;
	}
}

void CSQC_Client_SetStatFloat (int idx, float value)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
	{
		// FTE CL_SetStatNumeric parity: int=(int)fvalue.
		s_csqc_statsf[idx] = value;
		s_csqc_stat[idx] = (int)value;
	}
}

void CSQC_Client_SetStatString (int idx, const char *s)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
	{
		Q_free (s_csqc_statss[idx]);
		s_csqc_statss[idx] = Q_strdup (s ? s : "");
	}
}

void CSQC_Client_GetScreenSize (int *w, int *h)
{
	if (w)
		*w = vid.width;
	if (h)
		*h = vid.height;
}

// Translate FTE ^-markup into &cRGB runs. Supported: ^0-9 (q3), ^xRGB (3 hex),
// ^&XY extended FG (consolecolours[16] palette; BG is not expressible in the draw
// path), ^d (reset), ^s/^r (color stack, depth 4 as FTE extstack), consume
// ^b/^h/^m/^a (flags are not drawn), ^^ (literal); unknown/dangling ^ is a literal
// (FTE messedup); &c/&r are copied through.
// Out of scope: links ^[..^], charset `u8:`/`k8:`, ^Uxxxx/^{xxxx}, ezquakemess, and
// the visual blink/halfalpha/2nd-charset/BG effects. Palettes are FTE
// consolecolours, quantized 16 levels/channel (&c nibble); ^8 (half-alpha white) is
// drawn white - alpha deviation. out==NULL only counts the length.
static const char *csqc_q3_nibbles[10] = {
	"000", "F55", "5F5", "FF5", "55F", "5FF", "F5F", "FFF", "FFF", "BBB"
};

// ^&XY extended FG: X/Y are an index into consolecolours[16], quantized 4 bits
// per channel (like the q3 table above).
static const char *csqc_console_nibbles[16] = {
	"000", "00B", "0B0", "0BB", "B00", "B0B", "B50", "BBB",
	"555", "55F", "5F5", "5FF", "F55", "F5F", "FF5", "FFF"
};

static int csqc_hexval (int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

// ^&XY code character: 0-9, A-F (FTE isextendedcode). Returns the consolecolours
// index 0-15 or -1 ('-'/invalid).
static int csqc_ext_index (int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int CSQC_Client_TranslateMarkup (const char *in, char *out, size_t outsize)
{
	size_t n = 0;
	char curfg[3];		// current FG (&c nibbles)
	int have_col = 0;	// 0 = default/white (=&r), 1 = colored
	char stackfg[4][3];	// ^s/^r stack (FTE extstack, depth 4)
	int stackcol[4];
	int sp = 0;

	if (!in)
		in = "";
	// PUT: copies a byte, safe for out==NULL (length only) and overflow.
#define PUT(ch) do { if (out && outsize && n + 1 < outsize) out[n] = (char)(ch); n++; } while (0)
	// SETFG: remember the current FG (for ^s/^r).
#define SETFG(p) do { curfg[0] = (p)[0]; curfg[1] = (p)[1]; curfg[2] = (p)[2]; have_col = 1; } while (0)
	for (; *in; in++)
	{
		if (in[0] == '^' && in[1] >= '0' && in[1] <= '9')
		{
			const char *p = csqc_q3_nibbles[in[1] - '0'];
			PUT ('&'); PUT ('c'); PUT (p[0]); PUT (p[1]); PUT (p[2]);
			SETFG (p);
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == 'x')
		{
			if (csqc_hexval (in[2]) >= 0 && csqc_hexval (in[3]) >= 0 && csqc_hexval (in[4]) >= 0)
			{
				PUT ('&'); PUT ('c'); PUT (in[2]); PUT (in[3]); PUT (in[4]);
				SETFG (in + 2);
				in += 4;
			}
			else
			{
				// Invalid hex: skip "^x" entirely (FTE str+=2) - 'x' is not drawn.
				in += 1;
			}
			continue;
		}
		if (in[0] == '^' && in[1] == '&')
		{
			// ^&XY extended FG/BG: implement FG (BG is not expressible in the draw
			// path - deviation); Y is ignored.
			if ((csqc_ext_index (in[2]) >= 0 || in[2] == '-') &&
				(csqc_ext_index (in[3]) >= 0 || in[3] == '-'))
			{
				if (in[2] == '-')
				{
					// default FG = white (FTE COLOR_WHITE)
					PUT ('&'); PUT ('r');
					have_col = 0;
				}
				else
				{
					const char *p = csqc_console_nibbles[csqc_ext_index (in[2])];
					PUT ('&'); PUT ('c'); PUT (p[0]); PUT (p[1]); PUT (p[2]);
					SETFG (p);
				}
				in += 3;
			}
			else
			{
				// invalid: '^' literal, '&' handled on the next iteration (FTE messedup)
				PUT (*in);
			}
			continue;
		}
		if (in[0] == '^' && in[1] == 'd')
		{
			PUT ('&'); PUT ('r');
			have_col = 0;
			in += 1;
			continue;
		}
		if (in[0] == '^' && (in[1] == 'b' || in[1] == 'h' || in[1] == 'm' || in[1] == 'a'))
		{
			// FTE toggle blink/halfalpha/2nd charset; the flags are not expressible in
			// the draw path - consume the code (width parity), effect not drawn.
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == 's')
		{
			// push the stack (FTE extstack); store only the color.
			if (sp < (int)(sizeof (stackcol) / sizeof (stackcol[0])))
			{
				stackfg[sp][0] = curfg[0];
				stackfg[sp][1] = curfg[1];
				stackfg[sp][2] = curfg[2];
				stackcol[sp] = have_col;
				sp++;
			}
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == 'r')
		{
			// pop the stack: restore the color.
			if (sp > 0)
			{
				sp--;
				if (stackcol[sp])
				{
					const char *p = stackfg[sp];
					PUT ('&'); PUT ('c'); PUT (p[0]); PUT (p[1]); PUT (p[2]);
					SETFG (p);
				}
				else
				{
					PUT ('&'); PUT ('r');
					have_col = 0;
				}
			}
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == '^')
		{
			PUT ('^');
			in += 1;
			continue;
		}
		// Unknown/dangling '^' is a literal (FTE messedup): '^' goes to out, the next
		// character is handled on the next iteration.
		PUT (*in);
	}
	if (out && outsize)
		out[(n < outsize) ? n : outsize - 1] = 0;
	return (int)n;
#undef PUT
#undef SETFG
}

void CSQC_Client_DrawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale)
{
	extern cvar_t scr_coloredText;
	static char buf[16384];
	int prefix;
	float saved;
	if (!text)
		return;
	// Font scale from size.x (scale=size.x/8; 0 => 1). The module's color is passed
	// as an &cRRGGBB code to the engine. To make it independent of the user's
	// scr_coloredText, temporarily enable it for the draw.
	saved = scr_coloredText.value;
	Cvar_SetValue (&scr_coloredText, 1);
	// &cRGB is 3 hex digits (channel*16), not &cRRGGBB.
	prefix = snprintf (buf, sizeof (buf), "&c%X%X%X",
		(bound (0, r, 255)) / 16, (bound (0, g, 255)) / 16, (bound (0, b, 255)) / 16);
	if (prefix < 0)
		prefix = 0;
	// Module ^-markup -> &cRGB runs after the base color (see translator above).
	CSQC_Client_TranslateMarkup (text, buf + prefix, sizeof (buf) - prefix);
	// FTE drawcolouredstring parity: alpha is applied; color=NULL -> color comes
	// from the &c codes.
	Draw_SColoredAlphaString (x, y, buf, NULL, 0, 0, (scale > 0) ? scale : 1,
		bound (0, alpha, 1), true);
	Cvar_SetValue (&scr_coloredText, saved);
}

// Color for the 2D draw helpers (rgb 0..255 bytes, alpha 0..1).
static color_t CSQC_Client_Color (int r, int g, int b, float alpha)
{
	return RGBA_TO_COLOR ((byte)bound (0, r, 255), (byte)bound (0, g, 255),
		(byte)bound (0, b, 255), (byte)bound (0, (int)(alpha * 255.0f + 0.5f), 255));
}

/*
#324 drawsetcliparea / #325 drawresetcliparea - geometric clipping (hardware
GL-scissor is not applicable to ezq's deferred 2D pipeline). The clip rectangle is
stored in CSQC draw coordinates; rectangular primitives (pic/subpic/fill) are
intersected with it, text/lines are only skipped if fully outside (lines inside are
not cut) - a parity deviation.
*/
static qbool s_clip_on = false;
static float s_clip_x, s_clip_y, s_clip_w, s_clip_h;

// Intersects the dest-rect (x,y,w,h) with the active clip. false = empty/outside.
static qbool CSQC_Client_ClipDest (float *x, float *y, float *w, float *h)
{
	float x0, y0, x1, y1;
	if (!s_clip_on)
		return true;
	x0 = *x; y0 = *y;
	x1 = *x + *w; y1 = *y + *h;
	if (x1 <= s_clip_x || x0 >= s_clip_x + s_clip_w ||
		y1 <= s_clip_y || y0 >= s_clip_y + s_clip_h)
		return false;
	x0 = (x0 > s_clip_x) ? x0 : s_clip_x;
	y0 = (y0 > s_clip_y) ? y0 : s_clip_y;
	x1 = (x1 < s_clip_x + s_clip_w) ? x1 : s_clip_x + s_clip_w;
	y1 = (y1 < s_clip_y + s_clip_h) ? y1 : s_clip_y + s_clip_h;
	*x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
	return true;
}

/*
=================
2D graphics. Coordinates/sizes are raw video pixels (as DrawText). drawpic: the
rgb tint is ignored (only alpha); scale = size / native size.
=================
*/
void CSQC_Client_DrawFill (float x, float y, float w, float h, int r, int g, int b, float alpha)
{
	if (w <= 0 || h <= 0)
		return;
	if (!CSQC_Client_ClipDest (&x, &y, &w, &h))
		return;
	Draw_AlphaRectangleRGB (x, y, w, h, 1, true, CSQC_Client_Color (r, g, b, alpha));
}

qbool CSQC_Client_DrawPic (float x, float y, float w, float h, const char *name, int r, int g, int b, float alpha)
{
	mpic_t *pic;
	float sx, sy, a = bound (0, alpha, 1);
	float dx, dy, dw, dh, srcx, srcy, srcw, srch;
	if (!name || !name[0])
		return false;
	pic = Draw_CachePicSafe (name, false, false);
	if (!pic)
		return false;
	// FTE parity #322: a negative size means mirroring. Signed scale_x/y flips the
	// texture (texcoord bound to the vertex in R_DrawImage); clip is not applied
	// (a negative dest breaks the intersection).
	if (w < 0 || h < 0)
	{
		sx = w / (float)pic->width;
		sy = h / (float)pic->height;
		if (r == 255 && g == 255 && b == 255)
			Draw_SAlphaSubPic2 (x, y, pic, 0, 0, pic->width, pic->height, sx, sy, a);
		else
			Draw_SColoredSubPic2 (x, y, pic, 0, 0, pic->width, pic->height, sx, sy,
				bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
		return true;
	}
	if (w == 0 || h == 0)
		return true;
	// Clip: intersect dest with the active area, recompute the source (the
	// "whole pic -> dest" property is preserved).
	dx = x; dy = y; dw = w; dh = h;
	if (!CSQC_Client_ClipDest (&dx, &dy, &dw, &dh))
		return true;
	sx = w / (float)pic->width;
	sy = h / (float)pic->height;
	srcx = (dx - x) / sx;
	srcy = (dy - y) / sy;
	srcw = dw / sx;
	srch = dh / sy;
	if (r == 255 && g == 255 && b == 255)
		Draw_SAlphaSubPic2 (dx, dy, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, sx, sy, a);
	else
		Draw_SColoredSubPic2 (dx, dy, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, sx, sy,
			bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
	return true;
}

void CSQC_Client_DrawSubPic (float x, float y, float w, float h, const char *name, float srcx, float srcy, float srcw, float srch, int r, int g, int b, float alpha)
{
	mpic_t *pic;
	float a = bound (0, alpha, 1);
	float dx, dy, dw, dh, nsx, nsy, nsw, nsh, ssx, ssy;
	if (!name || !name[0] || srcw <= 0 || srch <= 0)
		return;
	pic = Draw_CachePicSafe (name, false, false);
	if (!pic)
		return;
	// FTE parity #328: a negative size on one axis means mirroring (signed scale).
	if (w < 0 || h < 0)
	{
		ssx = w / srcw;
		ssy = h / srch;
		if (r == 255 && g == 255 && b == 255)
			Draw_SAlphaSubPic2 (x, y, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, ssx, ssy, a);
		else
			Draw_SColoredSubPic2 (x, y, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, ssx, ssy,
				bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
		return;
	}
	if (w == 0 || h == 0)
		return;
	// Clip as in DrawPic: dest is intersected, source via affine mapping.
	dx = x; dy = y; dw = w; dh = h;
	if (!CSQC_Client_ClipDest (&dx, &dy, &dw, &dh))
		return;
	ssx = w / srcw;
	ssy = h / srch;
	nsx = srcx + ((dx - x) / w) * srcw;
	nsy = srcy + ((dy - y) / h) * srch;
	nsw = (dw / w) * srcw;
	nsh = (dh / h) * srch;
	if (r == 255 && g == 255 && b == 255)
		Draw_SAlphaSubPic2 (dx, dy, pic, (int)nsx, (int)nsy, (int)nsw, (int)nsh, ssx, ssy, a);
	else
		Draw_SColoredSubPic2 (dx, dy, pic, (int)nsx, (int)nsy, (int)nsw, (int)nsh, ssx, ssy,
			bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
}

void CSQC_Client_DrawCharacter (float x, float y, int ch, int r, int g, int b, float alpha, float scale)
{
	extern cvar_t scr_coloredText;
	static char buf[8];
	float saved;
	int c = ch & 0xff;
	if (c <= 0)
		return;
	// One default-font character with an &cRGB color (as DrawText).
	saved = scr_coloredText.value;
	Cvar_SetValue (&scr_coloredText, 1);
	snprintf (buf, sizeof (buf), "&c%X%X%X%c",
		(bound (0, r, 255)) / 16, (bound (0, g, 255)) / 16, (bound (0, b, 255)) / 16, c);
	// alpha as in FTE drawcharacter.
	Draw_SColoredAlphaString (x, y, buf, NULL, 0, 0, (scale > 0) ? scale : 1,
		bound (0, alpha, 1), true);
	Cvar_SetValue (&scr_coloredText, saved);
}

void CSQC_Client_DrawLine (float x1, float y1, float x2, float y2, float width, int r, int g, int b, float alpha)
{
	if (width <= 0)
		return;
	Draw_AlphaLineRGB (x1, y1, x2, y2, width, CSQC_Client_Color (r, g, b, alpha));
}

float CSQC_Client_StringWidth (const char *text, qbool usecolours, float fontsize_x)
{
	static char wbuf[16384];
	float scale;
	if (!text)
		return 0;
	// Scale from size.x (as DrawText; 0 => 1) - the same metric drawstring draws:
	// Draw_StringLength/Colors.
	scale = (fontsize_x > 0) ? fontsize_x / 8.0f : 1;
	if (usecolours)
	{
		// ^-codes do not count as width characters (FTE #327 with markup stripped) -
		// same translation as DrawText, then Draw_StringLengthColors skips &c/&r.
		CSQC_Client_TranslateMarkup (text, wbuf, sizeof (wbuf));
		return Draw_StringLengthColors (wbuf, -1, scale, true);
	}
	// usecolours=0: FTE #327 keeps the markup (keepmarkup) and counts its characters - original.
	return Draw_StringLength (text, -1, scale, true);
}

/*
=================
CSQC_Client_DrawFontScaleX

x multiplier of the `drawfontscale` (vector) global of the active VM - shared by
CSQC and MenuQC (handlers are reused). The offset is resolved via PR1VM_FindGlobal
with a lazy per-VM cache: when the VM changes (CSQC <-> menu-VM) the offset is
re-resolved.

FTE reference (PR_CL_BeginString):
  if (drawfontscale && (drawfontscale[0] || drawfontscale[1])) szx *= [0];
i.e. no global -> no scale; both components zero -> no scale (text is not squashed
to 0). The y component is not honored here (ezq font is uniform).
=================
*/
static pr1vm_t *s_drawfontscale_vm = NULL;
static int s_drawfontscale_ofs = -1;

float CSQC_Client_DrawFontScaleX (pr1vm_t *vm)
{
	float x, y;

	if (!vm)
		return 1.0f;
	if (s_drawfontscale_vm != vm)
	{
		s_drawfontscale_vm = vm;
		s_drawfontscale_ofs = PR1VM_FindGlobal (vm, "drawfontscale");
	}
	if (s_drawfontscale_ofs < 0)
		return 1.0f;
	x = vm->globals[s_drawfontscale_ofs + 0];
	y = vm->globals[s_drawfontscale_ofs + 1];
	if (x == 0.0f && y == 0.0f)
		return 1.0f;
	return x;
}

qbool CSQC_Client_PrecachePic (const char *name)
{
	if (!name || !name[0])
		return false;
	return Draw_CachePicSafe (name, false, false) != NULL;
}

// Additional 2D graphics (#316/#318/#319/#321/#324/#325/#329).

qbool CSQC_Client_IsCachedPic (const char *name)
{
	if (!name || !name[0])
		return false;
	return CachePic_Find (name, false) != NULL;
}

qbool CSQC_Client_PicSize (const char *name, float *w, float *h)
{
	mpic_t *pic;
	const char *ext;
	char path[MAX_QPATH];
	if (!name || !name[0])
		return false;
	// #318 FTE parity (PF_CL_drawgetimagesize): R2D_SafeCachePic + R_GetShaderSizes.
	// FTE resolves the name via Image_GetTexture extension-fallback
	// (r_imageextensions + COM_DefaultExtension(".lmp")) - a bare name also resolves.
	// ".lmp" is read from the header directly (ezq Draw_CachePicSafe on a .lmp path
	// returns an unrelated size).
	ext = COM_FileExtension (name);
	if (!ext || !ext[0])
	{
		strlcpy (path, name, sizeof (path));
		COM_DefaultExtension (path, ".lmp", sizeof (path));
		name = path;
		ext = COM_FileExtension (name);
	}
	if (!strcasecmp (ext, "lmp"))
	{
		// .lmp header (qpic_t): two LE ints (see SwapPic/LittleLong). We do not pull
		// in wad.h (it requires texture_t) - read the header directly.
		byte *data = FS_LoadTempFile ((char *)name, NULL);
		int iw, ih;
		if (!data)
			return false;
		memcpy (&iw, data, sizeof (iw));
		memcpy (&ih, data + sizeof (iw), sizeof (ih));
		if (w)
			*w = (float)LittleLong (iw);
		if (h)
			*h = (float)LittleLong (ih);
		return true;
	}
	pic = R_LoadPicImage (name, NULL, 0, 0, TEX_ALPHA);
	if (!pic)
		return false;
	if (w)
		*w = (float)pic->width;
	if (h)
		*h = (float)pic->height;
	return true;
}

void CSQC_Client_DrawRawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale)
{
	char one[2];
	const char *p;
	float xx;
	if (!text)
		return;
	// "Raw" output: each character is drawn as a single colored glyph - there is no
	// place to build an &cRGB within one string, so an & in the module text is
	// output literally (as FTE drawrawstring). Color and alpha are applied.
	xx = x;
	for (p = text; *p; p++)
	{
		one[0] = *p;
		one[1] = 0;
		CSQC_Client_DrawCharacter (xx, y, (int)(unsigned char)*p, r, g, b, alpha, scale);
		xx += Draw_StringLength (one, 1, (scale > 0) ? scale : 1, true);
	}
}

/*
#324 drawsetcliparea / #325 drawresetcliparea - geometric clip (state), no hardware
scissor/flush (see the comment at CSQC_Client_ClipDest).
*/
void CSQC_Client_SetClipArea (float x, float y, float w, float h)
{
	s_clip_x = x;
	s_clip_y = y;
	s_clip_w = (w > 0) ? w : 0;
	s_clip_h = (h > 0) ? h : 0;
	s_clip_on = true;
}

void CSQC_Client_ResetClipArea (void)
{
	s_clip_on = false;
}

void CSQC_Client_SetCursorMode (qbool usecursor, const char *image,
	float hotspot_x, float hotspot_y, float scale)
{
	// Store the parameters; the effect is enabled by CSQC_Client_CSQCCursor() itself
	// - while usecursor=1 and the module is active in-game, the ezquake mouse
	// mechanics do not hand the mouse to the OS cursor (vid_sdl2.c), and
	// SCR_DrawCursor draws the module's cursor.
	s_cursormode.usecursor = usecursor;
	s_cursormode.cursorimage[0] = 0;
	if (image)
		strlcpy (s_cursormode.cursorimage, image, sizeof (s_cursormode.cursorimage));
	s_cursormode.hotspot[0] = hotspot_x;
	s_cursormode.hotspot[1] = hotspot_y;
	s_cursormode.scale = scale;
}

qbool CSQC_Client_CSQCCursor (void)
{
	// The module cursor only applies in the game frame (key_game): when the engine
	// console/menu is open their own cursor/mouse take priority.
	return s_cursormode.usecursor && s_csqc.loaded && !s_csqc.errored
		&& key_dest == key_game;
}

// FTE parity: mouse position/delta are returned in vid.conwidth units (as Draw_*
// and the IE_MOUSEABS contract), while cursor_x/y and mx/my are in render-2D
// (VID_RenderWidth2D). FTE scales by *vid.width/vid.pixelwidth. The conversion
// reference is SCR_UpdateCursor.
static float CSQC_Client_CursorScaleX (void)
{
	int rw = VID_RenderWidth2D ();
	return (rw > 0) ? (float)vid.conwidth / (float)rw : 1.0f;
}

static float CSQC_Client_CursorScaleY (void)
{
	int rh = VID_RenderHeight2D ();
	return (rh > 0) ? (float)vid.conheight / (float)rh : 1.0f;
}

void CSQC_Client_ScaleCursorDelta (float *x, float *y)
{
	if (x)
		*x *= CSQC_Client_CursorScaleX ();
	if (y)
		*y *= CSQC_Client_CursorScaleY ();
}

void CSQC_Client_GetCursorPos (float *x, float *y)
{
	extern double cursor_x, cursor_y;	// pointer coordinates in render-2D
	if (x)
		*x = (float)cursor_x * CSQC_Client_CursorScaleX ();
	if (y)
		*y = (float)cursor_y * CSQC_Client_CursorScaleY ();
}

void CSQC_Client_SetSensitivityScale (float scale)
{
	// #346: sensitivity multiplier (may be 0); default 1.
	s_sens_scale = scale;
}

float CSQC_Client_SensitivityScale (void)
{
	// Inactive module - no effect (default 1).
	if (!s_csqc.loaded || s_csqc.errored)
		return 1;
	return s_sens_scale;
}

void CSQC_Client_DrawCursor (void)
{
	extern double cursor_x, cursor_y;
	mpic_t *pic;
	float scale, x, y;

	if (!CSQC_Client_CSQCCursor ())
		return;
	// FTE: scale <= 0 -> 1; hotspot is the cursor "tip" in image pixels (multiplied
	// by the scale), so the position points at the click point. The cursor position
	// is in vid.conwidth units (as Draw_*), while cursor_x/y is in render-2D; the
	// hotspot stays in pixels (FTE in_generic.c).
	scale = (s_cursormode.scale > 0) ? s_cursormode.scale : 1;
	x = (float)cursor_x * CSQC_Client_CursorScaleX () - s_cursormode.hotspot[0] * scale;
	y = (float)cursor_y * CSQC_Client_CursorScaleY () - s_cursormode.hotspot[1] * scale;

	if (s_cursormode.cursorimage[0])
	{
		pic = Draw_CachePicSafe (s_cursormode.cursorimage, false, false);
		if (!pic)
			pic = Draw_CachePicSafe (s_cursormode.cursorimage, false, true);	// tga/png
		if (pic)
		{
			Draw_SColoredSubPic2 (x, y, pic, 0, 0, pic->width, pic->height,
				scale, scale, 255, 255, 255, 1);
			return;
		}
	}
	// No image - default crosshair (visually like the ezquake cursor).
	{
		color_t c = RGBA_TO_COLOR (0, 255, 0, 255);
		float s = scale;
		Draw_AlphaLineRGB (x + 4 * s, y + 4 * s, x + 16 * s, y + 16 * s, 2 * s, c);
		Draw_AlphaLineRGB (x, y, x + 8 * s, y, 2 * s, c);
		Draw_AlphaLineRGB (x, y, x, y + 8 * s, 2 * s, c);
		Draw_AlphaLineRGB (x + 8 * s, y, x, y + 8 * s, 2 * s, c);
	}
}

static void CSQC_Client_ConsoleCommand_f (void);

void CSQC_Client_RegisterCommand (const char *cmd)
{
	int i;
	char *name;

	if (!cmd || !cmd[0])
		return;
	for (i = 0; i < s_csqc.numcmds; i++)
		if (!strcmp (s_csqc.cmds[i], cmd))
			return;					// already registered

	// FTE parity, unlimited: a dynamic list instead of a fixed cap (FTE
	// PF_cs_registercommand -> Cmd_AddCommandD with no limit).
	if (s_csqc.numcmds >= s_csqc.maxcmds)
	{
		int newmax = s_csqc.maxcmds ? s_csqc.maxcmds * 2 : 16;
		s_csqc.cmds = (char **)Q_realloc (s_csqc.cmds, newmax * sizeof (s_csqc.cmds[0]));
		s_csqc.maxcmds = newmax;
	}

	// Cmd_AddRemCommand copies the name into a Q_malloc block (unlike
	// Cmd_AddCommand, which holds a pointer to the name and allocates the node in
	// the hunk). The node/name survive Host_ClearMemory and are removed correctly by
	// RemoveCommand. Our own copy is needed to remove the command on module unload.
	name = Q_strdup (cmd);
	if (!Cmd_AddRemCommand (name, CSQC_Client_ConsoleCommand_f))
	{
		Q_free (name);
		return;
	}
	s_csqc.cmds[s_csqc.numcmds] = name;
	s_csqc.numcmds++;
}

/*
=================
CSQC_Client_RegisterCommands

Registers client debug commands for PR1VM (csqc_smoke, etc.). Called from
CL_InitLocal (cl_main.c) - commands available in the client console.
=================
*/
void CSQC_Client_RegisterCommands (void)
{
#ifdef CSQC_DEBUG
	Cmd_AddCommand ("csqc_smoke", PR1VM_CSQCSmoke_f);	// PR1VM debug
	Cmd_AddCommand ("csqc_progscheck", CSQC_Client_ProgsCheck_f);	// debug canary
	Cmd_AddCommand ("csqc_netprobe", CSQC_Client_NetProbe_f);	// FTE-CSQC receive canary
#endif
}

/*
=================
host callbacks of the client instance
=================
*/
static void CSQC_Client_HostPrint (pr1vm_t *vm, const char *msg)
{
	(void)vm;
	Con_Printf ("%s\n", msg);
}

static void CSQC_Client_HostError (pr1vm_t *vm, const char *msg)
{
	Con_Printf ("CSQC (PR1VM) program error: %s\n", msg);
	s_csqc.errored = true;
	// Do not return into the interpreter - unwind back to the outermost active
	// setjmp in PR1VM_ExecuteProgram (the client VM always has abortbuf_valid set).
	// Frames stay disabled (errored); reload on the next ConnectCheck.
	if (vm && vm->abortbuf_valid && vm->abortbuf)
		longjmp (*vm->abortbuf, 1);
	// No abort-buffer: fall back to the previous behavior (return; the caller
	// PR_RunError then takes the fatal path).
}

/*
=================
CSQC_Client_Abort

Fatal module error (FTE CSQC_Abort -> Host_EndGame parity): print the reason and
disconnect the client from the server (back to the menu), then Host_Abort (longjmp
into Host_Frame) - do not return into the executing VM. errored is set BEFORE
CL_Disconnect so CSQC_Client_Disconnect does not call func_shutdown re-entrantly
(we are inside the executing VM).
=================
*/
void CSQC_Client_Abort (const char *msg)
{
	Con_Printf ("CSQC (PR1VM) fatal: %s\n", msg ? msg : "fatal");
	s_csqc.errored = true;
	CL_Disconnect ();
	Host_Abort ();
}

/*
=================
CSQC_Client_GetString

Bounded string reads of the client VM (untrusted downloaded csprogs.dat): a
positive offset must lie inside the module's string block; anything beyond is a
forged value and returns NULL (callers map it to ""/skip). The shared PR1VM_GetString
stays unbounded (server/map strings live past numstrings), so the client bound
lives here and is installed as vm->get_string.
=================
*/
char *CSQC_Client_GetString (pr1vm_t *vm, int num)
{
	if (num >= 0 && (!vm || !vm->progs || num >= vm->progs->numstrings))
		return NULL;
	return PR1VM_GetString (vm, num);
}

/*
=================
Internal helpers
=================
*/
// Client map-uptime and previous cl.time for frametime (FTE: frametime =
// bound(0, cl.time - cl.lasttime, 0.1), cltime = realtime - cl.mapstarttime).
static double s_mapstarttime;
static double s_prev_cltime;

// Simulated server time of the module. FTE: *csqcg.time = cl.servertime; if the
// server does not send STAT_TIME/svc_time - client map-uptime (same time domain as
// cltime).
static double CSQC_Client_TimeNow (void)
{
	if (cl.servertime_works)
		return cl.servertime;
	return cls.realtime - s_mapstarttime;
}

static void CSQC_Client_SetTime (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (s_csqc.global_time >= 0)
		vm->globals[s_csqc.global_time] = (float)CSQC_Client_TimeNow ();
}

// FTE CSQC_StateOp parity: the client module's OP_STATE uses its own field/global
// offsets (not the fixed entvars_t/globalvars_t). self is an arena slot (the engine
// writes raw = slot*edict_size); fields via the module's resolve
// (f_nextthink/f_frame/f_think), time via global_time.
static void CSQC_Client_StateOp (pr1vm_t *vm, float frame, func_t func)
{
	float *v;
	int slot = 0;

	if (s_csqc.global_self >= 0)
		slot = CSQC_Client_EntNum (vm, *(int *)&vm->globals[s_csqc.global_self]);
	if (slot < 0 || slot >= vm->max_edicts)
		return;
	v = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);

	if (s_csqc.f_nextthink >= 0)
		v[s_csqc.f_nextthink] = ((s_csqc.global_time >= 0)
			? vm->globals[s_csqc.global_time] : 0.0f) + 0.1f;
	if (s_csqc.f_frame >= 0 && frame != v[s_csqc.f_frame])
		v[s_csqc.f_frame] = frame;
	if (s_csqc.f_think >= 0)
		*(int *)&v[s_csqc.f_think] = (int)func;
}

static qbool CSQC_Client_Exec (int fidx)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (fidx <= 0 || fidx >= vm->progs->numfunctions)
		return false;
	CSQC_Client_SetTime ();
	PR1VM_ExecuteProgram (vm, (func_t)fidx);
	return !s_csqc.errored;
}

/* As CSQC_Client_Exec, but returns the module's G_FLOAT(OFS_RETURN) (for the
 * delta callback: return != 0 = "the engine does not draw the entity"). */
static qbool CSQC_Client_ExecRet (int fidx, float *ret)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (ret)
		*ret = 0;
	if (fidx <= 0 || fidx >= vm->progs->numfunctions)
		return false;
	CSQC_Client_SetTime ();
	PR1VM_ExecuteProgram (vm, (func_t)fidx);
	if (ret)
		*ret = vm->globals[OFS_RETURN];
	return !s_csqc.errored;
}

static void CSQC_Client_ClearCommands (void)
{
	int i;
	for (i = 0; i < s_csqc.numcmds; i++)
	{
		Cmd_RemoveCommand (s_csqc.cmds[i]);
		Q_free (s_csqc.cmds[i]);
	}
	s_csqc.numcmds = 0;
	if (s_csqc.cmds)
	{
		Q_free (s_csqc.cmds);
		s_csqc.cmds = NULL;
	}
	s_csqc.maxcmds = 0;
}

/*
=================
CSQC_Client_ConsoleCommand_f

A command registered by the module via registercommand. Rebuild the full line
("name arg1 arg2 ...") and call CSQC_ConsoleCommand(string cmd).
=================
*/
static void CSQC_Client_ConsoleCommand_f (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	const char *line;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.func_console <= 0)
		return;

	if (Cmd_Argc () > 1)
		line = va ("%s %s", Cmd_Argv (0), Cmd_Args ());
	else
		line = Cmd_Argv (0);

	CSQC_Client_SetTime ();
	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM0], (char *)line);
	vm->globals[OFS_RETURN] = 0;
	PR1VM_ExecuteProgram (vm, (func_t)s_csqc.func_console);
}

/*
=================
CSQC_Client_Active
=================
*/
int CSQC_Client_Active (void)
{
	return (s_csqc.loaded && !s_csqc.errored) ? 1 : 0;
}

/*
=================
renderscene takeover

The module owns the 3D scene as in FTE: when the module is active, CSQC_UpdateView
is called in the 3D phase (SCR_UpdateScreenPlayerView) instead of R_RenderView();
#300 clearscene / #301 addentities fill cl_visents; #304 renderscene runs
R_RenderView(). The s_scene_rendered flag guards against a black screen: if the
module did not call renderscene, the engine draws the frame itself (fallback).
=================
*/
static qbool s_scene_rendered = false;
static qbool s_scene_viewmodel = false;	// #301 mask&MASK_STDVIEWMODEL requested

qbool CSQC_Client_SceneActive (void)
{
	return s_csqc.loaded && s_csqc.inited && !s_csqc.errored && s_csqc.func_update > 0;
}

void CSQC_Client_BeginScene (void)
{
	s_scene_rendered = false;
	s_scene_viewmodel = false;
}

// #301 mask&2: the module requested the engine viewmodel in the scene (FTE CL_LinkViewModel).
void CSQC_Client_LinkViewModel (void)
{
	s_scene_viewmodel = true;
}

qbool CSQC_Client_SceneViewModel (void)
{
	return s_scene_viewmodel;
}

void CSQC_Client_RenderScene (void)
{
	s_scene_rendered = true;
	R_RenderView ();
}

qbool CSQC_Client_SceneRendered (void)
{
	return s_scene_rendered;
}

/*
=================
CSQC_Client_CallPredraw

Call the arena edict's .predraw on #301/#302 (FTE PF_R_AddEntityMask): self =
slot*edict_size, execute, return G_FLOAT(OFS_RETURN). The module decides via the
return whether to auto-add (PREDRAW_AUTOADD=0) or skip (!=0). If predraw removed
the edict or execution failed - *removed=1 (do not add). self is restored (as FTE
`*csqcg.self = oldself`). .entnum is untouched (unlike SetContextSlot - FTE also
does not rewrite it in addentities).
=================
*/
float CSQC_Client_CallPredraw (int slot, int fidx, qbool *removed)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;
	int oldself = 0;

	if (removed)
		*removed = false;
	if (!vm || !vm->game_edicts || fidx <= 0)
		return 0;
	if (!CSQC_Client_EntUsed (slot))
	{
		if (removed)
			*removed = true;
		return 0;
	}

	if (s_csqc.global_self >= 0)
	{
		oldself = *(int *)&vm->globals[s_csqc.global_self];
		*(int *)&vm->globals[s_csqc.global_self] = (int)slot * vm->edict_size;
	}

	if (CSQC_Client_Exec (fidx))
		ret = vm->globals[OFS_RETURN];
	else if (removed)
		*removed = true;

	if (s_csqc.global_self >= 0)
		*(int *)&vm->globals[s_csqc.global_self] = oldself;

	if (removed && !CSQC_Client_EntUsed (slot))
		*removed = true;
	return ret;
}

/*
=================
CSQC model registry

#20/#75 precache_model registers a model (name->model_t*, 1-based index); #200
getmodelindex / #333 setmodelindex and the `.modelindex` field work with this index
(deviation from FTE: FTE has a separate index space for csqc-only models; here it is
a single registry over Mod_ForName). The index is module-opaque.

precache_model re-trigger: the name is registered even when the file is missing
(Mod_ForName returns NULL) - the slot holds a NULL placeholder, but the index stays
stable. After a successful download the placeholder is filled in
CSQC_Client_ModelDownloadFinished (drop-in without the module re-precaching).
=================
*/
#define CSQC_MAX_MODELS 512
static struct model_s *s_models[CSQC_MAX_MODELS];
static char s_modelnames[CSQC_MAX_MODELS][MAX_QPATH];
static int s_nmodels;

int CSQC_Client_ModelIndexKnown (const char *name)
{
	int i;
	if (!name || !name[0])
		return 0;
	for (i = 0; i < s_nmodels; i++)
		if (!strcmp (s_modelnames[i], name))
			return i + 1;
	return 0;
}

int CSQC_Client_ModelIndex (const char *name)
{
	struct model_s *m;
	int idx = CSQC_Client_ModelIndexKnown (name);
	if (idx || !name || !name[0])
		return idx;
	if (s_nmodels >= CSQC_MAX_MODELS)
		return 0;
	// precache_model re-trigger: register the name even if the file is missing
	// (Mod_ForName == NULL) so the returned index stays stable; the slot then holds
	// a NULL placeholder until the model is loaded after a successful download
	// (CSQC_Client_ModelDownloadFinished). "!= 0" therefore means "registered", not "loaded".
	m = Mod_ForName (name, false);
	strlcpy (s_modelnames[s_nmodels], name, MAX_QPATH);
	s_models[s_nmodels] = m;
	return ++s_nmodels;
}

struct model_s *CSQC_Client_ModelForIndex (int idx)
{
	return (idx >= 1 && idx <= s_nmodels) ? s_models[idx - 1] : NULL;
}

/* #334 modelnameforindex: reverse lookup of a CSQC registry index. */
const char *CSQC_Client_ModelNameForIndex (int idx)
{
	return (idx >= 1 && idx <= s_nmodels) ? s_modelnames[idx - 1] : NULL;
}

void CSQC_Client_ModelReset (void)
{
	memset (s_modelnames, 0, sizeof (s_modelnames));
	memset (s_models, 0, sizeof (s_models));
	s_nmodels = 0;
}

/*
=================
precache_model re-trigger: reload-on-download (FTE CL_DownloadFinished).

Called from CL_FinishDownload after a successful download. `downloadname` is
cls.downloadname (="<gamedir>/<file>"), so the gamedir prefix is stripped and the
rest matched against the CSQC model registry. A matching slot is (re)loaded via
Mod_ForName: a NULL placeholder becomes the loaded model, so its stable index turns
render-usable without the module re-calling precache_model. A no-op when the registry
is empty (no CSQC module / nothing precached).
=================
*/
void CSQC_Client_ModelDownloadFinished (const char *downloadname)
{
	const char *raw;
	int i, prefix;

	if (!downloadname || !downloadname[0] || s_nmodels <= 0)
		return;

	prefix = (int)strlen (cls.gamedir);
	if (prefix <= 0 || strncmp (downloadname, cls.gamedir, prefix) || downloadname[prefix] != '/')
		return;
	raw = downloadname + prefix + 1;

	for (i = 0; i < s_nmodels; i++)
	{
		if (strcmp (s_modelnames[i], raw))
			continue;
		s_models[i] = Mod_ForName (s_modelnames[i], false);
	}
}

/*
=================
CSQC_Client_ValidateFile

Validates a local csprogs file against the server keys: size == *csprogssize and
(if *csprogs is set) Com_BlockChecksum == crc (the same md4 as mvdsv's
Com_BlockChecksum). Analog of FTE CSQC_ValidateMainCSProgs.
=================
*/
static qbool CSQC_Client_ValidateData (byte *data, int filesize, int size, unsigned crc)
{
	if (!data)
		return false;
	if (size > 0 && filesize != size)
		return false;
	if (crc && Com_BlockChecksum (data, filesize) != crc)
		return false;
	return true;
}

static qbool CSQC_Client_ValidateFile (const char *path, int size, unsigned crc)
{
	byte *data;
	int filesize;
	qbool ok;

	if (!path || !path[0])
		return false;
	// Heap buffer + Q_free, not the low hunk. ValidateFile is also called every
	// frame in CSQC_Client_Update while a download is in progress; Hunk_AllocName
	// would accumulate csprogs copies until the next map.
	data = (byte *)FS_LoadHeapFile (path, &filesize);
	ok = CSQC_Client_ValidateData (data, filesize, size, crc);
	Q_free (data);
	return ok;
}

/*
=================
CSQC_Client_FindMainProgs

Find a valid local csprogs using FTE semantics (CSQC_FindMainProgs): 1) the
csprogsvers/<crc>.dat cache, 2) *csprogsname (+ fallback to csprogs.dat). When a
valid name-file is found and crc is set, write a copy into the csprogsvers/<crc>.dat
cache (write-back, as FTE) so later connects take the cache instead of re-downloading.
Returns true and fills pathbuf with the path for CSQC_Client_Load.

anycsqc (FTE parity): promiscuous mode - do not check size/crc of the local
candidate (server with anycsqc/corrupt *csprogs, or demoplayback); write-back to the
crc cache is not done in this case.
=================
*/
static qbool CSQC_Client_FindMainProgs (char *pathbuf, size_t bufsz,
	const char *name, int sizep, unsigned crc, qbool anycsqc)
{
	extern void Sys_mkdir (const char *path);
	char buf[MAX_QPATH];
	const char *cands[3];
	int nc = 0;
	int i;

	if (crc)
	{
		snprintf (buf, sizeof (buf), "csprogsvers/%x.dat", crc);
		if (CSQC_Client_ValidateFile (buf, sizep, crc))
		{
			strlcpy (pathbuf, buf, bufsz);
			return true;
		}
	}

	if (name && name[0])
		cands[nc++] = name;
	if (!name || !name[0] || strcmp (name, "csprogs.dat"))
		cands[nc++] = "csprogs.dat";

	for (i = 0; i < nc; i++)
	{
		byte *data;
		int len;

		// Load the candidate once (heap) - validation and write-back from one
		// buffer; previously two HunkFile loads per candidate.
		data = (byte *)FS_LoadHeapFile (cands[i], &len);
		if (!CSQC_Client_ValidateData (data, len, anycsqc ? 0 : sizep, anycsqc ? 0 : crc))
		{
			Q_free (data);
			continue;
		}

		strlcpy (pathbuf, cands[i], bufsz);
		// FTE write-back: copy a valid name-file into the cache for later. With
		// anycsqc/demo the crc is not confirmed - do not write the cache.
		if (crc && !anycsqc && !cls.demoplayback)
		{
			char dest[MAX_OSPATH], dir[MAX_OSPATH];
			char *slash;
			FILE *f;
			snprintf (dest, sizeof (dest), "%s/csprogsvers/%x.dat",
				cls.gamedir, crc);
			strlcpy (dir, dest, sizeof (dir));
			slash = strrchr (dir, '/');
			if (slash && slash != dir)
			{
				*slash = 0;
				Sys_mkdir (dir);
			}
			f = fopen (dest, "wb");
			if (f)
			{
				fwrite (data, 1, len, f);
				fclose (f);
				Con_Printf ("CSQC: cached csprogsvers/%x.dat\n", crc);
			}
		}
		Q_free (data);
		return true;
	}
	return false;
}

/*
=================
CSQC_Client_StartDownload

Requests the csprogs download from the server. The server serves the file under
*csprogsname (mvdsv SV_LoadCSQC), but we save it into a separate cache folder
csprogsvers/<crc>.dat (as FTE does) so different servers do not overwrite each
other. ezquake CL_CheckOrDownloadFile cannot separate the remote/local name - so we
repeat its startup steps with a different local path.
=================
*/
static void CSQC_Client_StartDownload (const char *remote, const char *localrel)
{
	extern void Sys_mkdir (const char *path);
	char dir[MAX_OSPATH];
	char *slash;

	if (cls.state < ca_connected || cls.demoplayback)
		return;

	snprintf (cls.downloadname, sizeof (cls.downloadname), "%s/%s", cls.gamedir, localrel);
	cls.downloadmethod = DL_QW;
	cls.downloadstarttime = Sys_DoubleTime ();
	COM_StripExtension (cls.downloadname, cls.downloadtempname, sizeof (cls.downloadtempname));
	strlcat (cls.downloadtempname, ".tmp", sizeof (cls.downloadtempname));

	// the destination directory (e.g. csprogsvers/) must exist
	strlcpy (dir, cls.downloadname, sizeof (dir));
	slash = strrchr (dir, '/');
	if (slash && slash != dir)
	{
		*slash = 0;
		Sys_mkdir (dir);
	}

	Com_Printf ("CSQC: downloading %s -> %s\n", remote, localrel);
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va ("download \"%s\"", remote));
	cls.downloadnumber++;
	// Start the no-progress window and remember our downloadname (cls.download is
	// shared by all downloads - count progress only against our own file).
	s_csqc.csprogs_dl_lastprogress = Sys_DoubleTime ();
	s_csqc.csprogs_dl_percent = 0;
	s_csqc.csprogs_dl_started = false;
	strlcpy (s_csqc.csprogs_dl_localname, cls.downloadname, sizeof (s_csqc.csprogs_dl_localname));
}

/*
=================
CSQC_Client_FreeArena / AllocArena

Client edict arena: direct entnum -> slot map (PR1 entity value = N*edict_size).
Q_malloc (not hunk); freed in Disconnect and at the start of Load (guards against a
repeated call).
=================
*/
static void CSQC_Client_FreeArena (void)
{
	if (s_csqc.edicts)
	{
		Q_free (s_csqc.edicts);
		s_csqc.edicts = NULL;
	}
	if (s_csqc.game_edicts)
	{
		Q_free (s_csqc.game_edicts);
		s_csqc.game_edicts = NULL;
	}
}

static void CSQC_Client_AllocArena (pr1vm_t *vm)
{
	int i;

	CSQC_Client_FreeArena ();
	if (!vm || vm->edict_size <= 0)
		return;

	// Reset occupancy/number-map when (re)allocating the arena.
	memset (s_used, 0, sizeof (s_used));
	memset (s_own, 0, sizeof (s_own));
	memset (s_numslot, 0, sizeof (s_numslot));
	memset (s_slotnum, 0, sizeof (s_slotnum));

	s_csqc.game_edicts = (byte *)Q_malloc ((size_t)CSQC_MAX_EDICTS * vm->edict_size);
	s_csqc.edicts = (edict_t *)Q_malloc (sizeof (edict_t) * CSQC_MAX_EDICTS);
	memset (s_csqc.game_edicts, 0, (size_t)CSQC_MAX_EDICTS * vm->edict_size);
	memset (s_csqc.edicts, 0, sizeof (edict_t) * CSQC_MAX_EDICTS);
	for (i = 0; i < CSQC_MAX_EDICTS; i++)
		s_csqc.edicts[i].v = (entvars_t *)(s_csqc.game_edicts + (size_t)i * vm->edict_size);

	vm->edicts = s_csqc.edicts;
	vm->game_edicts = s_csqc.game_edicts;
	vm->num_edicts = CSQC_MAX_EDICTS;
	vm->max_edicts = CSQC_MAX_EDICTS;
	vm->state = 0;	// client instance; the "world" OP_ADDRESS guard is inactive (world not written)
	vm->fieldofs_patch = NULL;	// FTE csprogs uses raw field offsets.
}

/*
=================
CSQC_Client_FindField

Finds a module field by name in fielddefs (see PR1VM_FindFunction). Returns the
field offset in float words from the start of entvars (ddef_t.ofs) or -1.
=================
*/
int CSQC_Client_FindField (pr1vm_t *vm, const char *name)
{
	int i;

	if (!vm || !vm->fielddefs || !name)
		return -1;
	for (i = 0; i < vm->progs->numfielddefs; i++)
	{
		const char *s = CSQC_Client_GetString (vm, vm->fielddefs[i].s_name);
		if (s && s[0] && !strcmp (s, name))
			return vm->fielddefs[i].ofs;
	}
	return -1;
}

/*
=================
Hot-path offset cache

Resolution of the traced globals and entity fields read every frame
(csqc_store_trace, csqc_add_one_entity, csqc_addentities) is done once at module
load (CSQC_Client_OffsetCacheResolve). Previously each call invoked PR1VM_FindGlobal
/ CSQC_Client_FindField - a linear strcmp scan of globaldefs/fielddefs.

FTE reference: globals - csqcg; entity fields - the overlay csqcentvars_t.
=================
*/
static const char *s_traceg_names[CSQC_TRACEG_COUNT] =
{
	"trace_fraction", "trace_allsolid", "trace_startsolid", "trace_inopen",
	"trace_inwater", "trace_plane_dist", "trace_endpos", "trace_plane_normal",
	"trace_ent", "trace_networkentity", "trace_endcontents"
};

static const char *s_fieldcache_names[CSQC_FLD_COUNT] =
{
	"predraw", "modelindex", "model", "colormap", "origin", "angles",
	"frame", "skin", "effects", "alpha", "scale", "renderflags",
	"size", "mins", "maxs", "modelflags", "chain", "solid", "flags",
	"owner", "drawmask"
};

static int s_traceg[CSQC_TRACEG_COUNT];
static int s_fieldcache[CSQC_FLD_COUNT];

static void CSQC_Client_OffsetCacheReset (void)
{
	int i;

	for (i = 0; i < CSQC_TRACEG_COUNT; i++)
		s_traceg[i] = -1;
	for (i = 0; i < CSQC_FLD_COUNT; i++)
		s_fieldcache[i] = -1;
}

static void CSQC_Client_OffsetCacheResolve (pr1vm_t *vm)
{
	int i;

	for (i = 0; i < CSQC_TRACEG_COUNT; i++)
		s_traceg[i] = PR1VM_FindGlobal (vm, s_traceg_names[i]);
	for (i = 0; i < CSQC_FLD_COUNT; i++)
		s_fieldcache[i] = CSQC_Client_FindField (vm, s_fieldcache_names[i]);
}

int CSQC_Client_TraceGlobal (pr1vm_t *vm, int id)
{
	(void)vm;
	return (id >= 0 && id < CSQC_TRACEG_COUNT) ? s_traceg[id] : -1;
}

int CSQC_Client_FieldOfs (pr1vm_t *vm, int id)
{
	(void)vm;
	return (id >= 0 && id < CSQC_FLD_COUNT) ? s_fieldcache[id] : -1;
}

/*
=================
CSQC_Client_SetEntityContext

Sets the entity context for CSQC_Ent_Update/Remove: self = entnum*edict_size (PR1
entity value) and writes the float entnum into the arena's .entnum field. The module
then reads self.entnum.
=================
*/
static void CSQC_Client_SetContextSlot (pr1vm_t *vm, unsigned slot, unsigned number)
{
	float *s;

	if (!vm || !vm->game_edicts)
		return;
	// self = slot*edict_size (PR1 entity value, int bits); .entnum (module field) =
	// server number (owned spawn entities do not get a number - it stays 0).
	if (s_csqc.global_self >= 0)
		*(int *)&vm->globals[s_csqc.global_self] = (int)slot * vm->edict_size;
	if (s_csqc.field_entnum >= 0 && slot < CSQC_MAX_EDICTS)
	{
		s = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size + s_csqc.field_entnum * 4);
		s[0] = (float)number;
	}
}

/*
=================
CSQC_Client_EntityEntNum

ssqc number of the touched entity - the arena edict's module field `.entnum`: the
server number for network entities (CSQC_Client_SetContextSlot), 0 for owned spawn
entities. Source of FTE trace_networkentity parity (FTE `tr->entnum` =
`touch->number` only for network ssqc brushes). Out of range / no field - 0.
=================
*/
int CSQC_Client_EntityEntNum (pr1vm_t *vm, int slot)
{
	float *s;

	if (!vm || !vm->game_edicts || s_csqc.field_entnum < 0)
		return 0;
	if (slot <= 0 || slot >= CSQC_MAX_EDICTS || slot >= vm->num_edicts)
		return 0;
	s = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size + s_csqc.field_entnum * 4);
	return (int)s[0];
}

/*
=================
CSQC_Client_RunEntSpawn

Hook for a new CSQC entity (FTE parity). The engine zeroes self (self=0=world), puts
the server number into PARM0, calls CSQC_Ent_Spawn; the module creates/configures the
entity (usually spawn(); self.entnum = entnum) and returns it in self. Read self ->
arena slot (self/edict_size). Returns a valid occupied slot or 0 (world/invalid; no
fallback, as FTE ent=NULL).
=================
*/
static int CSQC_Client_RunEntSpawn (pr1vm_t *vm, unsigned int entnum)
{
	int selfval, slot;

	if (!vm || s_csqc.func_entspawn <= 0 || s_csqc.global_self < 0 || vm->edict_size <= 0)
		return 0;
	*(int *)&vm->globals[s_csqc.global_self] = 0;
	vm->globals[OFS_PARM0] = (float)entnum;
	if (!CSQC_Client_Exec (s_csqc.func_entspawn))
		return 0;
	selfval = *(int *)&vm->globals[s_csqc.global_self];
	slot = CSQC_Client_EntNum (vm, selfval);
	return (slot > 0 && slot < CSQC_MAX_EDICTS && s_used[slot]) ? slot : 0;
}

/* After CSQC_Ent_Update the module may change self; remap number->slot onto the new
   valid slot (0 = world/removed). */
static void CSQC_Client_RemapAfterUpdate (pr1vm_t *vm, unsigned int entnum)
{
	int selfval, slot;

	if (!vm || s_csqc.func_entspawn <= 0 || s_csqc.global_self < 0 || vm->edict_size <= 0)
		return;
	selfval = *(int *)&vm->globals[s_csqc.global_self];
	slot = CSQC_Client_EntNum (vm, selfval);
	if (slot < 0 || slot >= CSQC_MAX_EDICTS || !s_used[slot])
		slot = 0;
	CSQC_Client_MapNumber ((int)entnum, slot);
}

/*
=================
CSQC_Client_EntAlloc / EntFree (entity pool)

Module entities (builtin spawn) take any free pool slot (first free from 1) and are
marked s_own (remove allowed only for owned ones). Network slots are allocated from
the same pool (without s_own) and held by the number->slot map in ParseEntities. PR1
entity value = slot*edict_size.
=================
*/
static int CSQC_Client_AllocSlot (pr1vm_t *vm)
{
	int i;
	for (i = 1; i < CSQC_MAX_EDICTS; i++)
		if (!s_used[i])
		{
			s_used[i] = true;
			s_own[i] = false;
			// FTE parity: zero the slot fields on (re)use - otherwise the module
			// sees remnants of a previous entity. FTE: QC_ClearEdict / ED_AllocIndex.
			if (vm && vm->game_edicts && vm->edict_size > 0)
				memset ((byte *)vm->game_edicts + (size_t)i * vm->edict_size, 0, vm->edict_size);
			return i;
		}
	Con_Printf ("CSQC_Client_AllocSlot: pool full (%d)\n", CSQC_MAX_EDICTS - 1);
	return 0;
}

int CSQC_Client_EntAlloc (struct pr1vm_s *v)
{
	pr1vm_t *vm = (pr1vm_t *)v;
	int slot;
	slot = CSQC_Client_AllocSlot (vm);
	if (slot)
		s_own[slot] = true;	// spawn entity: .entnum not written (0)
	return slot;
}

void CSQC_Client_EntFree (struct pr1vm_s *v, int entnum)
{
	pr1vm_t *vm = (pr1vm_t *)v;
	float *s;

	if (!vm || !vm->game_edicts)
		return;
	if (entnum <= 0 || entnum >= CSQC_MAX_EDICTS || !s_used[entnum])
		return;
	if (!s_own[entnum])
		return;	// network entity - do not touch
	// Drop the reverse slot->N map, otherwise s_numslot[N] stays valid on the freed
	// slot, which may be reused.
	if (s_slotnum[entnum] > 0 && s_slotnum[entnum] < CSQC_MAX_NUM
		&& s_numslot[s_slotnum[entnum]] == entnum)
		s_numslot[s_slotnum[entnum]] = 0;
	s_slotnum[entnum] = 0;
	s_used[entnum] = false;
	s_own[entnum] = false;
	s = (float *)((byte *)vm->game_edicts + (size_t)entnum * vm->edict_size);
	memset (s, 0, vm->edict_size);
}

/* internal network path (ParseEntities): a slot without s_own.
   vm is needed to zero the slot fields. */
int CSQC_Client_NetAllocSlot (struct pr1vm_s *v)
{
	return CSQC_Client_AllocSlot ((pr1vm_t *)v);
}

void CSQC_Client_NetFreeSlot (int slot, int number)
{
	if (slot > 0 && slot < CSQC_MAX_EDICTS && s_used[slot])
	{
		s_used[slot] = false;
		s_own[slot] = false;
	}
	if (slot > 0 && slot < CSQC_MAX_EDICTS)
		s_slotnum[slot] = 0;	// reverse map does not outlive the free
	if (number > 0 && number < CSQC_MAX_NUM && s_numslot[number] == slot)
		s_numslot[number] = 0;
}

/* access/diagnostics: walk the pool and fields */
qbool CSQC_Client_EntUsed (int entnum)
{
	return (entnum > 0 && entnum < CSQC_MAX_EDICTS) ? s_used[entnum] : false;
}

int CSQC_Client_EntSpawnBase (void)
{
	return 1;	// first usable pool slot (0 = world)
}

int CSQC_Client_EntUsedCount (void)
{
	int i, n = 0;
	for (i = 1; i < CSQC_MAX_EDICTS; i++)
		if (s_used[i])
			n++;
	return n;
}

/*
=================
CSQC_Client_EntNum

Convert a raw PR1 entity value (slot*edict_size) into a pool slot. The client
VM executes untrusted csprogs and a builtin entity argument is not dereferenced
by the VM (unlike an opcode pointer), so an out-of-range value is clamped to
world (0) instead of faulting. FTE parity: PF_etos/PF_wasfreed go through
ProgsToEdict, which reports "Bad entity index" and falls back to edict 0. Same
bound as the opcode predicate PR1VM_ClientBadEdict; on the client instance
num_edicts == max_edicts == CSQC_MAX_EDICTS (see CSQC_Client_AllocArena).
=================
*/
int CSQC_Client_EntNum (struct pr1vm_s *v, int raw)
{
	pr1vm_t *vm = (pr1vm_t *)v;
	int idx;

	if (!vm || vm->edict_size <= 0)
		return 0;
	idx = raw / vm->edict_size;
	if (raw < 0 || idx >= vm->max_edicts)
	{
		Con_DPrintf ("CSQC: bad entity value %d\n", raw);
		return 0;	// world
	}
	return idx;
}

int CSQC_Client_NumToSlot (int number)
{
	return (number > 0 && number < CSQC_MAX_NUM) ? s_numslot[number] : 0;
}

int CSQC_Client_MapNumber (int number, int slot)
{
	if (number > 0 && number < CSQC_MAX_NUM)
		s_numslot[number] = slot;
	if (slot > 0 && slot < CSQC_MAX_EDICTS)
		s_slotnum[slot] = (number > 0 && number < CSQC_MAX_NUM) ? number : 0;
	return slot;
}

/*
=================
#371 deltalisten: engine bridge player_state -> arena-edict (FTE path
`CSQC_DeltaPlayer`/`CSQC_PlayerStateToCSQC`). The bridge gives the module the
authoritative (no-lerp) player state: `self`/`.entnum` = pnum+1, fields
origin/velocity/angles (+modelindex/skin). The module callback is invoked as
CSQC_Ent_Update (PARM0 = isnew) once per new acked frame (cl.parsecount).
=================
*/
static int s_delta_func[MAX_MODELS];
static int s_delta_flags[MAX_MODELS];
// Personal player-bridge mapping (pnum -> arena slot), to distinguish ownership
// from svc76 (CSQC_Client_NumToSlot). num = pnum+1 (server entnum of the player).
static int s_player_slot[MAX_CLIENTS];
// delta-entity bridge: packet entity number -> arena slot + "seen this frame".
static int s_delta_slot[CSQC_MAX_NUM];
static byte s_delta_seen[CSQC_MAX_NUM];
// MASK_DELTA: callback returned !=0 -> the engine does not draw the entity (the module does).
static byte s_delta_player_owned[MAX_CLIENTS];
static byte s_delta_ent_owned[CSQC_MAX_NUM];
// Whether at least one deltalisten is registered (func>0). If not, the Delta* paths
// do not memset the 4096 arrays and do not scan packet entities each frame (FTE:
// deltafunction[] empty -> CSQC_DeltaUpdate does nothing).
static qbool s_delta_any;

static void CSQC_Client_DeltaReset (void)
{
	s_delta_any = false;
	memset (s_delta_func, 0, sizeof (s_delta_func));
	memset (s_delta_flags, 0, sizeof (s_delta_flags));
	memset (s_player_slot, 0, sizeof (s_player_slot));
	memset (s_delta_slot, 0, sizeof (s_delta_slot));
	memset (s_delta_seen, 0, sizeof (s_delta_seen));
	memset (s_delta_player_owned, 0, sizeof (s_delta_player_owned));
	memset (s_delta_ent_owned, 0, sizeof (s_delta_ent_owned));
}

// Getters for CL_LinkPlayers/CL_LinkPacketEntities (cl_ents.c).
qbool CSQC_Client_DeltaPlayerOwned (int pnum)
{
	return (pnum >= 0 && pnum < MAX_CLIENTS && s_delta_player_owned[pnum]) ? true : false;
}

qbool CSQC_Client_DeltaEntityOwned (int number)
{
	return (number > 0 && number < CSQC_MAX_NUM && s_delta_ent_owned[number]) ? true : false;
}

void CSQC_Client_DeltaListen (const char *model, int func, int flags)
{
	int i;
	if (!model)
		return;
	if (!strcmp (model, "*"))
	{
		for (i = 0; i < MAX_MODELS; i++)
		{
			s_delta_func[i] = (func > 0) ? func : 0;
			s_delta_flags[i] = flags;
		}
		if (func > 0)
			s_delta_any = true;	// at least one listener - Delta* are active
		return;
	}
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!cl.model_name[i][0])
			break;
		if (!strcmp (cl.model_name[i], model))
		{
			s_delta_func[i] = (func > 0) ? func : 0;
			s_delta_flags[i] = flags;
			if (func > 0)
				s_delta_any = true;
			break;
		}
	}
}

static void CSQC_Client_DeltaPlayers (pr1vm_t *vm)
{
	int pnum;

	if (!vm || !vm->game_edicts || !vm->edict_size)
		return;
	if (!s_delta_any)
		return;		// no listeners - nothing to do
	if (cls.demoplayback || cls.mvdplayback)
		return;		// prediction - live game only
	memset (s_delta_player_owned, 0, sizeof (s_delta_player_owned));
	for (pnum = 0; pnum < MAX_CLIENTS; pnum++)
	{
		player_state_t *st = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[pnum];
		int num = pnum + 1;
		int slot = s_player_slot[pnum];
		int func = 0, isnew = 0;

		if (st->messagenum == cl.parsecount && st->modelindex > 0
			&& st->modelindex < MAX_MODELS)
			func = s_delta_func[st->modelindex];

		if (!func)
		{
			// entity absent / no listener - remove it if it was there
			if (slot)
			{
				if (s_csqc.func_entremove > 0)
				{
					CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);
					CSQC_Client_Exec (s_csqc.func_entremove);
				}
				CSQC_Client_NetFreeSlot (slot, num);
				s_player_slot[pnum] = 0;
			}
			continue;
		}

		// svc76 already owns the number - do not overwrite (FTE csqcent[]-guard)
		if (!slot && CSQC_Client_NumToSlot (num))
			continue;

		if (!slot)
		{
			slot = CSQC_Client_NetAllocSlot (vm);
			if (!slot)
				continue;
			CSQC_Client_MapNumber (num, slot);
			s_player_slot[pnum] = slot;
			isnew = 1;
		}

		CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);

		// player_state fields (no-lerp: raw values, as FTE RSES_NOLERP).
		{
			float *base = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);
			if (s_csqc.f_origin >= 0)
				VectorCopy (st->origin, base + s_csqc.f_origin);
			if (s_csqc.f_velocity >= 0)
				VectorCopy (st->velocity, base + s_csqc.f_velocity);
			if (s_csqc.f_angles >= 0)
			{
				// The server sends viewangles only in demos; for the local player use
				// fresh cl.viewangles (updated from usercmd).
				const float *ang = (pnum == cl.playernum) ? cl.viewangles : st->viewangles;
				VectorCopy (ang, base + s_csqc.f_angles);
			}
			if (s_csqc.f_modelindex >= 0)
				base[s_csqc.f_modelindex] = (float)st->modelindex;
			if (s_csqc.f_skin >= 0)
				base[s_csqc.f_skin] = (float)st->skinnum;
			// Player render fields. FTE CSQC_PlayerStateToCSQC fills
			// frame/skin/colormap; engine player render uses frame/effects/translations.
			if (s_csqc.f_frame >= 0)
				base[s_csqc.f_frame] = (float)st->frame;
			if (s_csqc.f_effects >= 0)
				base[s_csqc.f_effects] = (float)st->effects;
			if (s_csqc.f_colormap >= 0)
				base[s_csqc.f_colormap] = (float)(pnum + 1);	// player index
			if (s_csqc.f_drawmask >= 0)
				base[s_csqc.f_drawmask] = 1;	// MASK_DELTA
		}

		vm->globals[OFS_PARM0] = isnew ? 1 : 0;
		{
			// callback return !=0 -> the engine does not draw this player (the module does).
			float pret = 0;
			if (CSQC_Client_ExecRet (func, &pret) && pret != 0)
				s_delta_player_owned[pnum] = 1;
		}
		if (s_csqc.errored)
			return;
	}
}

/*
=================
#371 delta-entity bridge (FTE CSQC_DeltaStart/Update/End): packet entities of the
frame (entity_state_t) with a model-registered callback are handed to the module as
CSQC_Ent_Update (self/.entnum, PARM0 = isnew). Ones missing from the frame go through
the remove path. RSES_NOLERP/NOROTATE: raw state (no interpolation); NOTRAILS/NOLIGHTS
are invalid (ezq CSQC has no trails/dynamic light).
=================
*/
static void CSQC_Client_DeltaEntities (pr1vm_t *vm)
{
	packet_entities_t *pack;
	int i, num;

	if (!vm || !vm->game_edicts || !vm->edict_size)
		return;
	if (!s_delta_any)
		return;		// no listeners - do not memset/scan 4096 each frame
	if (cls.demoplayback || cls.mvdplayback)
		return;
	if (!cl.validsequence)
		return;

	memset (s_delta_seen, 0, sizeof (s_delta_seen));
	memset (s_delta_ent_owned, 0, sizeof (s_delta_ent_owned));
	pack = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;

	for (i = 0; i < pack->num_entities; i++)
	{
		entity_state_t *es = &pack->entities[i];
		int slot, func, isnew = 0;
		float *base;

		num = es->number;
		if (num <= 0 || num >= CSQC_MAX_NUM)
			continue;
		if (es->modelindex <= 0 || es->modelindex >= MAX_MODELS)
			continue;
		func = s_delta_func[es->modelindex];
		if (!func)
			continue;

		s_delta_seen[num] = 1;
		slot = s_delta_slot[num];
		if (!slot)
		{
			// svc76 already owns the number - do not overwrite
			if (CSQC_Client_NumToSlot (num))
				continue;
			slot = CSQC_Client_NetAllocSlot (vm);
			if (!slot)
				continue;
			CSQC_Client_MapNumber (num, slot);
			s_delta_slot[num] = slot;
			isnew = 1;
		}

		CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);
		base = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);
		if (s_csqc.f_origin >= 0)
			VectorCopy (es->origin, base + s_csqc.f_origin);
		if (s_csqc.f_angles >= 0)
			VectorCopy (es->angles, base + s_csqc.f_angles);
		if (s_csqc.f_modelindex >= 0)
			base[s_csqc.f_modelindex] = (float)es->modelindex;
		if (s_csqc.f_frame >= 0)
			base[s_csqc.f_frame] = (float)es->frame;
		if (s_csqc.f_skin >= 0)
			base[s_csqc.f_skin] = (float)es->skinnum;
		if (s_csqc.f_effects >= 0)
			base[s_csqc.f_effects] = (float)es->effects;
		if (s_csqc.f_drawmask >= 0)
			base[s_csqc.f_drawmask] = 1;	// MASK_DELTA

		vm->globals[OFS_PARM0] = isnew ? 1 : 0;
		{
			// callback return !=0 -> the engine does not draw this packet entity.
			float pret = 0;
			if (CSQC_Client_ExecRet (func, &pret) && pret != 0)
				s_delta_ent_owned[num] = 1;
		}
		if (s_csqc.errored)
			return;
	}

	// ones missing this frame - remove path
	for (num = 1; num < CSQC_MAX_NUM; num++)
	{
		int slot = s_delta_slot[num];
		if (slot && !s_delta_seen[num])
		{
			if (s_csqc.func_entremove > 0)
			{
				CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);
				CSQC_Client_Exec (s_csqc.func_entremove);
			}
			CSQC_Client_NetFreeSlot (slot, num);
			s_delta_slot[num] = 0;
		}
	}
}

/*
=================
player_localentnum (FTE)
=================

Publish the module global player_localentnum (number of the observed player) each
frame before CSQC_UpdateView. FTE publishes the global always; but ezquake does NOT
fabricate player entities (the entity environment is what the server sent via svc76
plus own spawns, as FTE in the absence of server player emission / player-delta).
The player mirror is removed - self/play are N/A until the mod emits players.
*/
void CSQC_Client_UpdateLocalEntnum (void)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored || s_csqc.g_localentnum < 0)
		return;
	vm->globals[s_csqc.g_localentnum] = (cl.viewplayernum >= 0) ? cl.viewplayernum + 1 : 0;
}

/*
=================
view/listener/view_angles + project/unproject.

- `view_angles` - module global, published each frame (FTE); value is the engine's
  view angles (cl.viewangles).
- `#351 setlistener` - the module sets the audio listener; cl_main.c uses it in
  S_Update while the module is active (otherwise as usual).
- `#303 setproperty` (VF_* subset) - the module's view-origin/angles/vrect/fov;
  applied to r_refdef after V_CalcRefdef (cl_view.c) when CSQC is active. One-frame
  lag: CSQC_UpdateView is called in the HUD phase (after 3D rendering) - documented
  difference from FTE.
- `#310/#311 project/unproject` - screen<->world via the engine matrices
  (R_GetModelviewMatrix/R_GetProjectionMatrix/R_GetViewport).
=================
*/
#define CSQC_VFP_MIN		1
#define CSQC_VFP_MIN_X		2
#define CSQC_VFP_MIN_Y		3
#define CSQC_VFP_SIZE		4
#define CSQC_VFP_SIZE_X		5
#define CSQC_VFP_SIZE_Y		6
#define CSQC_VFP_VIEWPORT	7
#define CSQC_VFP_FOV		8
#define CSQC_VFP_FOVX		9
#define CSQC_VFP_FOVY		10
#define CSQC_VFP_ORIGIN		11
#define CSQC_VFP_ORIGIN_X	12
#define CSQC_VFP_ORIGIN_Y	13
#define CSQC_VFP_ORIGIN_Z	14
#define CSQC_VFP_ANGLES		15
#define CSQC_VFP_ANGLES_X	16
#define CSQC_VFP_ANGLES_Y	17
#define CSQC_VFP_ANGLES_Z	18
// set flags (FTE). Values are as in FTE; VF_PERSPECTIVE is recognized (return 1)
// but not visually implemented - ezq rendering has no isometry.
#define CSQC_VFP_DRAWWORLD	19
#define CSQC_VFP_DRAWENGINESBAR	20
#define CSQC_VFP_DRAWCROSSHAIR	21
#define CSQC_VFP_PERSPECTIVE	200

static qbool s_listener_on;
static vec3_t s_listener_org, s_listener_fwd, s_listener_rht, s_listener_up;

static qbool s_vp_on;
static qbool s_vp_origin_set, s_vp_angles_set, s_vp_vrect_set, s_vp_fovx_set, s_vp_fovy_set;
static vec3_t s_vp_origin, s_vp_angles;
static int s_vp_x, s_vp_y, s_vp_w, s_vp_h;
static float s_vp_fovx, s_vp_fovy;
// set flags (FTE clearscene defaults).
static qbool s_vp_drawsbar = false;
static qbool s_vp_drawcrosshair = false;

static void CSQC_Client_ViewPropsReset (void)
{
	s_vp_on = false;
	s_vp_origin_set = s_vp_angles_set = s_vp_vrect_set = false;
	s_vp_fovx_set = s_vp_fovy_set = false;
	// FTE clearscene: sbar/crosshair off.
	s_vp_drawsbar = false;
	s_vp_drawcrosshair = false;
}

static void CSQC_Client_ViewReset (void)
{
	s_listener_on = false;
	VectorClear (s_listener_org);
	VectorClear (s_listener_fwd);
	VectorClear (s_listener_rht);
	VectorClear (s_listener_up);
	CSQC_Client_ViewPropsReset ();
}

// #300 clearscene: FTE resets the view properties (the module calls clearscene every
// CSQC_UpdateView; without the reset the #303 override would "stick" between frames).
void CSQC_Client_ResetViewProps (void)
{
	CSQC_Client_ViewPropsReset ();
}

// #351 setlistener(origin, forward, right, up)
void CSQC_Client_SetListener (const float *origin, const float *forward, const float *right, const float *up)
{
	VectorCopy (origin, s_listener_org);
	VectorCopy (forward, s_listener_fwd);
	VectorCopy (right, s_listener_rht);
	VectorCopy (up, s_listener_up);
	s_listener_on = true;
}

qbool CSQC_Client_ListenerActive (void)
{
	return s_listener_on && s_csqc.loaded && !s_csqc.errored;
}

void CSQC_Client_GetListener (float *origin, float *forward, float *right, float *up)
{
	VectorCopy (s_listener_org, origin);
	VectorCopy (s_listener_fwd, forward);
	VectorCopy (s_listener_rht, right);
	VectorCopy (s_listener_up, up);
}

// #303 setproperty: VF_* subset (view). args are the sequential float arguments
// after property (a vector is 3 values, a scalar is 1).
qbool CSQC_Client_SetViewProperty (int prop, int argc, const float *args)
{
	qbool handled = true;
	switch (prop)
	{
	case CSQC_VFP_ORIGIN:
		if (argc >= 3) { VectorCopy (args, s_vp_origin); s_vp_origin_set = true; }
		break;
	case CSQC_VFP_ORIGIN_X: s_vp_origin[0] = args[0]; s_vp_origin_set = true; break;
	case CSQC_VFP_ORIGIN_Y: s_vp_origin[1] = args[0]; s_vp_origin_set = true; break;
	case CSQC_VFP_ORIGIN_Z: s_vp_origin[2] = args[0]; s_vp_origin_set = true; break;
	case CSQC_VFP_ANGLES:
		if (argc >= 3) { VectorCopy (args, s_vp_angles); s_vp_angles_set = true; }
		break;
	case CSQC_VFP_ANGLES_X: s_vp_angles[0] = args[0]; s_vp_angles_set = true; break;
	case CSQC_VFP_ANGLES_Y: s_vp_angles[1] = args[0]; s_vp_angles_set = true; break;
	case CSQC_VFP_ANGLES_Z: s_vp_angles[2] = args[0]; s_vp_angles_set = true; break;
	case CSQC_VFP_VIEWPORT:
		// position vector (PARM1) + size vector (PARM2), i.e. 6 words
		// (VF_VIEWPORT = "vector+vector"). Previously ezq read args[0..1] as the
		// size, losing position/size.
		if (argc >= 6)
		{
			s_vp_x = (int)args[0]; s_vp_y = (int)args[1];
			s_vp_w = (int)args[3]; s_vp_h = (int)args[4];
			s_vp_vrect_set = true;
		}
		break;
	case CSQC_VFP_MIN:
		if (argc >= 2) { s_vp_x = (int)args[0]; s_vp_y = (int)args[1]; s_vp_vrect_set = true; }
		break;
	case CSQC_VFP_MIN_X: s_vp_x = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_MIN_Y: s_vp_y = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_SIZE:
		if (argc >= 2) { s_vp_w = (int)args[0]; s_vp_h = (int)args[1]; s_vp_vrect_set = true; }
		break;
	case CSQC_VFP_SIZE_X: s_vp_w = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_SIZE_Y: s_vp_h = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_FOV:
		if (argc >= 2) { s_vp_fovx = args[0]; s_vp_fovy = args[1]; s_vp_fovx_set = s_vp_fovy_set = true; }
		break;
	case CSQC_VFP_FOVX: s_vp_fovx = args[0]; s_vp_fovx_set = true; break;
	case CSQC_VFP_FOVY: s_vp_fovy = args[0]; s_vp_fovy_set = true; break;
	case CSQC_VFP_DRAWWORLD:
		// recognized (return 1); the world under takeover is always drawn by
		// R_RenderView - there is no RDF_NOWORLDMODEL analog.
		break;
	case CSQC_VFP_DRAWENGINESBAR:
		if (argc >= 1) s_vp_drawsbar = (args[0] != 0);
		break;
	case CSQC_VFP_DRAWCROSSHAIR:
		if (argc >= 1) s_vp_drawcrosshair = (args[0] != 0);
		break;
	case CSQC_VFP_PERSPECTIVE:
		// flag recognized (return 1); isometry is not implemented in the ezq
		// renderer (no r_refdef.useperspective analog).
		break;
	default:
		handled = false;	// no analog - FTE default returns 0
		break;
	}
	s_vp_on = s_vp_origin_set || s_vp_angles_set || s_vp_vrect_set || s_vp_fovx_set || s_vp_fovy_set;
	// FTE applies the view flags in the same frame (the setter writes r_refdef at
	// once); ezq previously deferred to V_CalcRefdef => 1-frame lag.
	CSQC_Client_ApplyViewProps ();
	return handled;
}

// Applied after V_CalcRefdef (cl_view.c), only when a CSQC module is active.
void CSQC_Client_ApplyViewProps (void)
{
	if (!s_vp_on || !s_csqc.loaded || s_csqc.errored)
		return;
	if (s_vp_origin_set)
		VectorCopy (s_vp_origin, r_refdef.vieworg);
	if (s_vp_angles_set)
		VectorCopy (s_vp_angles, r_refdef.viewangles);
	if (s_vp_vrect_set)
	{
		r_refdef.vrect.x = s_vp_x;
		r_refdef.vrect.y = s_vp_y;
		r_refdef.vrect.width = s_vp_w;
		r_refdef.vrect.height = s_vp_h;
	}
	if (s_vp_fovx_set)
		r_refdef.fov_x = s_vp_fovx;
	if (s_vp_fovy_set)
		r_refdef.fov_y = s_vp_fovy;
}

/*
=================
FTE parity: gate of the engine sbar/HUD and crosshair. FTE clearscene sets
r_refdef.drawsbar/drawcrosshair = false, the module returns them via #303
setproperty(VF_DRAWENGINESBAR/VF_DRAWCROSSHAIR, 1). Under takeover
(CSQC_Client_SceneActive) cl_screen.c asks these accessors; outside takeover the
gate is not applied (engine HUD/crosshair as before).
=================
*/
qbool CSQC_Client_DrawEngineSbar (void)
{
	return s_vp_drawsbar;
}

qbool CSQC_Client_DrawCrosshairFlag (void)
{
	return s_vp_drawcrosshair;
}

// Publish the view_angles global (before CSQC_UpdateView).
void CSQC_Client_PublishViewAngles (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored || s_csqc.g_view_angles < 0)
		return;
	vm->globals[s_csqc.g_view_angles + 0] = cl.viewangles[0];
	vm->globals[s_csqc.g_view_angles + 1] = cl.viewangles[1];
	vm->globals[s_csqc.g_view_angles + 2] = cl.viewangles[2];
}

/*
#311 project / #310 unproject - FTE semantics:
clip = (model*proj) * v (our composition is equivalent to FTE proj*modelview),
NDC -> screen with a Y flip and r_refdef.vrect, FTE depth (sign when w<0).
No guards (FTE parity); degenerate cases yield NaN/Inf - diagnostic only.
*/
qbool CSQC_Client_Project (const float *world, float *sx, float *sy, float *sz)
{
	float model[16], proj[16], a[16], v[4], clip[4], sum;
	float rx, ry, rw, rh;
	int i, j, k;

	R_GetModelviewMatrix (model);
	R_GetProjectionMatrix (proj);

	// a = model * proj (row vector)
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
		{
			sum = 0;
			for (k = 0; k < 4; k++)
				sum += model[i * 4 + k] * proj[k * 4 + j];
			a[i * 4 + j] = sum;
		}
	v[0] = world[0]; v[1] = world[1]; v[2] = world[2]; v[3] = 1;
	for (j = 0; j < 4; j++)
	{
		sum = 0;
		for (k = 0; k < 4; k++)
			sum += v[k] * a[k * 4 + j];
		clip[j] = sum;
	}
	clip[0] /= clip[3];	// FTE: no guard (degenerate w -> NaN/Inf)
	clip[1] /= clip[3];
	clip[2] /= clip[3];

	rx = r_refdef.vrect.x;
	ry = r_refdef.vrect.y;
	rw = r_refdef.vrect.width;
	rh = r_refdef.vrect.height;
	*sx = (1 + clip[0]) / 2 * rw + rx;
	*sy = (1 - (1 + clip[1]) / 2) * rh + ry;
	*sz = clip[2];
	if (clip[3] < 0)
		*sz = -*sz;
	return true;
}

// Inverse 4x4 (row-major) by Gauss-Jordan elimination.
static qbool csqc_mat4_invert (const float *m, float *out)
{
	float a[4][8];
	int i, j, k;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			a[i][j] = m[i * 4 + j];
			a[i][j + 4] = (i == j) ? 1.0f : 0.0f;
		}
	}
	for (i = 0; i < 4; i++)
	{
		int piv = i;
		for (k = i + 1; k < 4; k++)
			if (fabs (a[k][i]) > fabs (a[piv][i]))
				piv = k;
		if (fabs (a[piv][i]) < 1e-12f)
			return false;
		if (piv != i)
			for (j = 0; j < 8; j++)
			{
				float t = a[i][j]; a[i][j] = a[piv][j]; a[piv][j] = t;
			}
		{
			float d = a[i][i];
			for (j = 0; j < 8; j++)
				a[i][j] /= d;
		}
		for (k = 0; k < 4; k++)
		{
			float f;
			if (k == i)
				continue;
			f = a[k][i];
			for (j = 0; j < 8; j++)
				a[k][j] -= f * a[i][j];
		}
	}
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			out[i * 4 + j] = a[i][j + 4];
	return true;
}

// #310 unproject(screen x, y, depth) -> world (FTE screen->NDC mapping).
qbool CSQC_Client_Unproject (float sx, float sy, float sz, float *world)
{
	float model[16], proj[16], a[16], inv[16], v[4], res[4], sum, tx, ty;
	int i, j, k;

	R_GetModelviewMatrix (model);
	R_GetProjectionMatrix (proj);
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
		{
			sum = 0;
			for (k = 0; k < 4; k++)
				sum += model[i * 4 + k] * proj[k * 4 + j];
			a[i * 4 + j] = sum;
		}
	if (!csqc_mat4_invert (a, inv))
		return false;

	tx = (sx - r_refdef.vrect.x) / r_refdef.vrect.width;
	ty = (sy - r_refdef.vrect.y) / r_refdef.vrect.height;
	ty = 1 - ty;
	v[0] = tx * 2 - 1;
	v[1] = ty * 2 - 1;
	v[2] = sz * 2 - 1;
	if (v[2] >= 1)
		v[2] = 0.999999f;
	v[3] = 1;
	for (j = 0; j < 4; j++)
	{
		sum = 0;
		for (k = 0; k < 4; k++)
			sum += v[k] * inv[k * 4 + j];
		res[j] = sum;
	}
	// FTE: divide by res[3] with no guard
	world[0] = res[0] / res[3];
	world[1] = res[1] / res[3];
	world[2] = res[2] / res[3];
	return true;
}

/*
=================
CSQC_Client_GetEntity

#504 getentity - FTE PF_getentity: read interpolated state of non-csqc
(engine-networked) entities by server number. ezq has no cl.lerpents/cl.lerpplayers;
the source is cl_entities[] (current entity_state_t + per-frame lerp data) and, for
players, player_state_t / player bbox / player colours. "Active" = present in the
current packet (player: playerstate.messagenum == cl.parsecount; map: cent->sequence
== cl.validsequence), the analog of FTE "le->sequence == cl.lerpentssequence".

Origin and angles are interpolated with the same lerp data the renderer uses
(cent->old_origin/current.origin, old_angles/current.angles, startlerp/deltalerp),
so even the local player and non-drawn entities (whose lerp_origin is not written by
CL_LinkPlayers) get the lerped values. Player-angle convention follows FTE: the
local player's pitch is model-space (-viewangles[0]/3), remote players keep the raw
packet angles. GE_MAXENTS is the runtime equivalent of FTE cl.maxlerpents (highest
packet entity number + headroom).

out[3] is always zeroed then filled (float fields use out[0]; vector fields use all
three). Fields with no ezq data source return the FTE default (0, or '1 1 1' for
GLOWMOD/RTCOLOUR) - documented deviation.
=================
*/
#define CSQC_GE_MAXENTS		(-1)
#define CSQC_GE_ACTIVE		0
#define CSQC_GE_ORIGIN		1
#define CSQC_GE_FORWARD		2
#define CSQC_GE_RIGHT		3
#define CSQC_GE_UP		4
#define CSQC_GE_SCALE		5
#define CSQC_GE_ORIGINANDVECTORS 6
#define CSQC_GE_ALPHA		7
#define CSQC_GE_COLORMOD	8
#define CSQC_GE_PANTSCOLOR	9
#define CSQC_GE_SHIRTCOLOR	10
#define CSQC_GE_SKIN		11
#define CSQC_GE_MINS		12
#define CSQC_GE_MAXS		13
#define CSQC_GE_ABSMIN		14
#define CSQC_GE_ABSMAX		15
#define CSQC_GE_LIGHT		16
#define CSQC_GE_MODELINDEX	200
#define CSQC_GE_EFFECTS		202
#define CSQC_GE_FRAME		203
#define CSQC_GE_ANGLES		204
#define CSQC_GE_GLOWMOD		208
#define CSQC_GE_RTCOLOUR	213

// Lerp helpers for #504 - mirror the renderer's interpolation. The lerp data
// (old_origin/current.origin, old_angles/current.angles, startlerp/deltalerp) is
// filled by CL_SetupPacketEntity (map entities) and SetupPlayerEntity (players).
extern cvar_t cl_nolerp, cl_lerp_monsters;
extern qbool cl_nolerp_on_entity_flag;
extern qbool NewLerp_AbleModel (int idx);

static qbool CSQC_Client_EntityIsMonster (int modelindex)
{
	int i;
	if (!cl_lerp_monsters.value)
		return false;
	for (i = 1; i < 17; i++)
		if (modelindex == cl_modelindices[mi_monster1 + i - 1])
			return true;
	return false;
}

static void CSQC_Client_EntityLerp (const centity_t *cent, vec3_t org, vec3_t ang)
{
	double time = cls.mvdplayback ? cls.demotime : cl.time;
	float lerp;

	// Same gates as the renderer: no-lerp cvars/flag (not in demos, not for
	// monsters) and a non-positive lerp delta fall back to the packet state.
	if (((cl_nolerp.value || cl_nolerp_on_entity_flag) && !cls.mvdplayback &&
		 !CSQC_Client_EntityIsMonster (cent->current.modelindex)) ||
		cent->deltalerp <= 0)
	{
		VectorCopy (cent->current.origin, org);
		VectorCopy (cent->current.angles, ang);
		return;
	}

	lerp = min (max ((float)((time - cent->startlerp) / cent->deltalerp), 0.0f), 1.0f);

	if (NewLerp_AbleModel (cent->current.modelindex))
	{
		float d = time - cent->startlerp;

		if (d >= 2 * cent->deltalerp)	// entity looks stopped - stay at last lerp
			VectorCopy (cent->lerp_origin, org);
		else
			VectorMA (cent->old_origin, d, cent->velocity, org);
	}
	else
	{
		VectorInterpolate (cent->old_origin, lerp, cent->current.origin, org);
	}
	AngleInterpolate (cent->old_angles, lerp, cent->current.angles, ang);
}

void CSQC_Client_GetEntity (int entnum, int fldnum, float out[3])
{
	centity_t *cent;
	entity_state_t *es;
	player_state_t *ps;
	qbool is_player = false;
	qbool active;
	int pnum = -1, modelindex;
	const model_t *model;
	vec3_t org, ang;

	if (out)
		out[0] = out[1] = out[2] = 0;

	if (cls.state != ca_active)
		return;

	if (fldnum == CSQC_GE_MAXENTS)
	{
		// Runtime equivalent of FTE cl.maxlerpents: highest packet-entity number
		// seen this frame + headroom (FTE grows its lerp array in steps of 16).
		packet_entities_t *pack;
		int i, mx = 0;

		if (cl.validsequence)
		{
			pack = &cl.frames[cl.parsecount & UPDATE_MASK].packet_entities;
			for (i = 0; i < pack->num_entities; i++)
				mx = max (mx, pack->entities[i].number + 1);
		}
		mx = max (mx, MAX_CLIENTS);
		out[0] = (float)min (CL_MAX_EDICTS, mx + 16);
		return;
	}

	if (entnum < 0 || entnum >= CL_MAX_EDICTS)
		return;		// invalid entity -> 0 (FTE: "not valid")

	cent = &cl_entities[entnum];
	es = &cent->current;

	// Players are tracked through playerinfo (SetupPlayerEntity: cent->sequence =
	// state->messagenum) and are "present" when playerstate was updated this frame
	// (same test as CL_LinkPlayers); map entities use the packet-entity frame
	// sequence (CL_SetupPacketEntity: cent->sequence = cl.validsequence).
	ps = (entnum >= 1 && entnum <= MAX_CLIENTS)
		? &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[entnum - 1] : NULL;
	if (ps && ps->messagenum == cl.parsecount)
		is_player = true, pnum = entnum - 1;

	active = is_player
		? true
		: (cent->sequence != 0 && cent->sequence == cl.validsequence);
	if (!active)
		return;

	modelindex = es->modelindex;

	// Interpolated origin + angles: same lerp data as the renderer, applied here so
	// the local player / non-drawn entities are lerped too (their cent->lerp_origin
	// is not written by CL_LinkPlayers).
	CSQC_Client_EntityLerp (cent, org, ang);

	// Local player (FTE parity): the server does not send the local player its own
	// viewangles (playerstate.viewangles is demo-only), so use the client's own
	// angles - the same source as the #371 bridge. FTE's #504 for the local player
	// returns the *model* pitch (le->angles[0] = simangles[0]*0.333*r_meshpitch;
	// r_meshpitch=-1 in QW), i.e. exactly the renderer convention (-viewangles[0]/3).
	// Remote players keep the raw packet angles (FTE and ezq remote readings match),
	// so only the local player gets the transform.
	if (is_player && pnum == cl.playernum)
	{
		VectorCopy (cl.viewangles, ang);
		ang[0] = -ang[0] / 3;
	}

	switch (fldnum)
	{
	case CSQC_GE_ACTIVE:
		out[0] = 1;
		break;
	case CSQC_GE_ORIGIN:
		VectorCopy (org, out);
		break;
	case CSQC_GE_ANGLES:
		VectorCopy (ang, out);
		break;
	case CSQC_GE_FORWARD:
	case CSQC_GE_RIGHT:
	case CSQC_GE_UP:
		AngleVectors (ang,
			(fldnum == CSQC_GE_FORWARD) ? out : NULL,
			(fldnum == CSQC_GE_RIGHT) ? out : NULL,
			(fldnum == CSQC_GE_UP) ? out : NULL);
		break;
	case CSQC_GE_ORIGINANDVECTORS:
		VectorCopy (org, out);
		CSQC_Client_MakeVectors (ang);	// module v_forward/v_right/v_up
		break;
	case CSQC_GE_MINS:
	case CSQC_GE_MAXS:
	case CSQC_GE_ABSMIN:
	case CSQC_GE_ABSMAX:
		{
			vec3_t mn, mx;
			if (is_player)
			{
				// FTE uses ps->szmins/szmaxs (hull); ezq keeps the prediction hull.
				extern vec3_t player_mins, player_maxs;
				VectorCopy (player_mins, mn);
				VectorCopy (player_maxs, mx);
			}
			else
			{
				// FTE decodes es->solidsize; ezq has none - approximate with the
				// model bounding box.
				model = (modelindex > 0 && modelindex < MAX_MODELS)
					? cl.model_precache[modelindex] : NULL;
				if (model)
				{
					VectorCopy (model->mins, mn);
					VectorCopy (model->maxs, mx);
				}
				else
					VectorClear (mn), VectorClear (mx);
			}
			if (fldnum == CSQC_GE_MINS)
				VectorCopy (mn, out);
			else if (fldnum == CSQC_GE_MAXS)
				VectorCopy (mx, out);
			else if (fldnum == CSQC_GE_ABSMIN)
				VectorAdd (org, mn, out);
			else
				VectorAdd (org, mx, out);
		}
		break;
	case CSQC_GE_SCALE:
		out[0] = 1;		// no scale in ezq state (FTE default 16/16) - deviation
		break;
	case CSQC_GE_ALPHA:
#ifdef FTE_PEXT_TRANS
		out[0] = es->trans / 255.0f;
#else
		out[0] = 1;
#endif
		break;
	case CSQC_GE_COLORMOD:
#ifdef FTE_PEXT_COLOURMOD
		out[0] = es->colourmod[0] / 8.0f;
		out[1] = es->colourmod[1] / 8.0f;
		out[2] = es->colourmod[2] / 8.0f;
#endif
		break;
	case CSQC_GE_PANTSCOLOR:
		out[0] = is_player ? (float)cl.players[pnum].bottomcolor
			: (float)(es->colormap & 15);
		break;
	case CSQC_GE_SHIRTCOLOR:
		out[0] = is_player ? (float)cl.players[pnum].topcolor
			: (float)((es->colormap >> 4) & 15);
		break;
	// For players the source is player_state (FTE PF_getentity player branch) -
	// SetupPlayerEntity does not copy skinnum/effects into cent->current.
	case CSQC_GE_SKIN:
		out[0] = (float)(is_player ? ps->skinnum : es->skinnum);
		break;
	case CSQC_GE_LIGHT:
		out[0] = 0;
		break;
	case CSQC_GE_MODELINDEX:
		out[0] = (float)(is_player ? ps->modelindex : es->modelindex);
		break;
	case CSQC_GE_EFFECTS:
		out[0] = (float)(is_player ? ps->effects : es->effects);
		break;
	case CSQC_GE_FRAME:
		out[0] = (float)(is_player ? ps->frame : es->frame);
		break;
	default:
		// GE_MODELINDEX2/GE_FATNESS/GE_DRAWFLAGS/GE_ABSLIGHT/GE_GLOWSIZE/
		// GE_GLOWCOLOUR/GE_RTSTYLE/GE_RTPFLAGS/GE_RTRADIUS/GE_TAGENTITY/
		// GE_TAGINDEX/GE_GRAVITYDIR/GE_TRAILEFFECTNUM - no ezq data source
		// (documented deviation); FTE default (vec3 defaults to '1 1 1' for the
		// two glow/rt colour fields, 0 otherwise).
		if (fldnum == CSQC_GE_GLOWMOD || fldnum == CSQC_GE_RTCOLOUR)
			out[0] = out[1] = out[2] = 1;
		break;
	}
}

/*
=================
PR1VM_LumpFits / PR1VM_StmtWords / PR1VM_ValidateClientV6

A downloaded csprogs.dat is server-supplied and must not be trusted. The server PR1
core is only ever fed a locally-installed, CRC-checked progs, so its loader trusts
the header; the client loader must not. Validate the header lump ranges against the
file size and the operand/field ranges the interpreter indexes
(`vm->globals[st->a/b/c]`, `parm_start/locals`, `first_statement`) before
PR1VM_LoadData byte-swaps and walks the lumps.
=================
*/
static qbool PR1VM_LumpFits (int ofs, int num, int elemsize, int filesize, const char *name)
{
	// Bound by division (num <= (filesize - ofs) / elemsize) instead of the
	// product ofs + num*elemsize, which wraps on a 32-bit size_t (i686) and would
	// let a crafted num slip past. Same result on 32/64-bit.
	if (ofs < (int)sizeof (dprograms_t) || ofs > filesize || num < 0 || elemsize <= 0 ||
		num > (filesize - ofs) / elemsize)
	{
		Con_Printf ("CSQC: csprogs.dat rejected: bad %s lump (ofs=%d num=%d)\n",
			name, ofs, num);
		return false;
	}
	return true;
}

// Width (in float words) of operand a/b/c (which 0/1/2) for the ops that read or
// write 3 words; all others are single-word. Branch ops (OP_GOTO/OP_IF/OP_IFNOT)
// store a *statement delta* in one operand (handled at the call site, not here).
static int PR1VM_StmtWords (int op, int which)
{
	switch (op)
	{
	case OP_DONE:
	case OP_RETURN:
		return which == 0 ? 3 : 1;
	case OP_MUL_V:
		return (which == 0 || which == 1) ? 3 : 1;
	case OP_ADD_V:
	case OP_SUB_V:
		return 3;
	case OP_MUL_FV:
		return (which == 1 || which == 2) ? 3 : 1;
	case OP_MUL_VF:
		return (which == 0 || which == 2) ? 3 : 1;
	case OP_EQ_V:
	case OP_NE_V:
		return (which == 0 || which == 1) ? 3 : 1;
	case OP_NOT_V:
		return which == 0 ? 3 : 1;
	case OP_STORE_V:
		// Both operands are vectors: a is the source (a->vector[0..2]), b the
		// destination (b->vector[0..2]) - matches pr_exec.c OP_STORE_V.
		return (which == 0 || which == 1) ? 3 : 1;
	case OP_LOAD_V:
		return which == 2 ? 3 : 1;
	case OP_STOREP_V:
		// a is the vector source (OPA->_vector[0..2]); b is the edict pointer
		// (runtime-bounded), c unused. Matches pr_exec.c OP_STOREP_V.
		return which == 0 ? 3 : 1;
	default:
		return 1;
	}
}

static qbool PR1VM_ValidateClientV6 (const byte *data, int filesize)
{
	dprograms_t h;
	const dstatement_t *st;
	const dfunction_t *fn;
	const ddef_t *dd;
	int i, j;

	if (!data || filesize < (int)sizeof (h))
	{
		Con_Printf ("PR1VM_LoadClientV6: file too small (%d bytes)\n", filesize);
		return false;
	}

	memcpy (&h, data, sizeof (h));
	for (i = 0; i < (int)(sizeof (h) / sizeof (int)); i++)
		((int *) &h)[i] = LittleLong (((int *) &h)[i]);

	if (h.version != PROG_VERSION)
	{
		Con_Printf ("PR1VM_LoadClientV6: not a QW v6 progs (version=%d)\n", h.version);
		return false;
	}

	if (!PR1VM_LumpFits (h.ofs_statements, h.numstatements, sizeof (dstatement_t), filesize, "statements") ||
		!PR1VM_LumpFits (h.ofs_globaldefs, h.numglobaldefs, sizeof (ddef_t), filesize, "globaldefs") ||
		!PR1VM_LumpFits (h.ofs_fielddefs, h.numfielddefs, sizeof (ddef_t), filesize, "fielddefs") ||
		!PR1VM_LumpFits (h.ofs_functions, h.numfunctions, sizeof (dfunction_t), filesize, "functions") ||
		!PR1VM_LumpFits (h.ofs_strings, h.numstrings, 1, filesize, "strings") ||
		!PR1VM_LumpFits (h.ofs_globals, h.numglobals, sizeof (float), filesize, "globals"))
		return false;

	if (h.numstatements < 1 || h.numfunctions < 1 || h.numstrings < 1 ||
		h.numglobals < RESERVED_OFS ||
		h.entityfields <= 0 || h.entityfields > CSQC_MAX_ENTITYFIELDS)
	{
		Con_Printf ("CSQC: csprogs.dat rejected: bad counts (stmt=%d func=%d str=%d glob=%d ef=%d)\n",
			h.numstatements, h.numfunctions, h.numstrings, h.numglobals, h.entityfields);
		return false;
	}

	// statements: every operand indexes vm->globals[st->a/b/c]; vector ops use 3.
	// Branch deltas (OP_GOTO->a, OP_IF/OP_IFNOT->b) are targets, not globals -
	// bounded at run time in PR1VM_ExecuteProgram.
	st = (const dstatement_t *) ((const byte *) data + h.ofs_statements);
	for (i = 0; i < h.numstatements; i++)
	{
		int op = (int)(unsigned short) LittleShort ((short) st[i].op);
		int ops[3];

		if (op > OP_BITOR)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: bad opcode %d at statement %d\n", op, i);
			return false;
		}

		ops[0] = (int) LittleShort (st[i].a);
		ops[1] = (int) LittleShort (st[i].b);
		ops[2] = (int) LittleShort (st[i].c);
		for (j = 0; j < 3; j++)
		{
			if ((op == OP_GOTO && j == 0) || ((op == OP_IF || op == OP_IFNOT) && j == 1))
				continue;
			if (ops[j] < 0 || ops[j] > h.numglobals - PR1VM_StmtWords (op, j))
			{
				Con_Printf ("CSQC: csprogs.dat rejected: statement %d operand out of range\n", i);
				return false;
			}
		}
	}

	// defs: every def offset + its type width must stay inside the lump the
	// engine writes through (globaldefs -> globals, fielddefs -> entvars);
	// width is 3 for a vector def, 1 otherwise. ofs is unsigned (ddef_t).
	dd = (const ddef_t *) ((const byte *) data + h.ofs_globaldefs);
	for (i = 0; i < h.numglobaldefs; i++)
	{
		int ofs = (int)(unsigned short) LittleShort ((short) dd[i].ofs);
		int type = (int)(unsigned short) LittleShort ((short) dd[i].type) & ~DEF_SAVEGLOBAL;
		int width = (type == ev_vector) ? 3 : 1;

		if (ofs + width > h.numglobals)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: globaldef %d out of range (ofs=%d w=%d glob=%d)\n",
				i, ofs, width, h.numglobals);
			return false;
		}
	}
	dd = (const ddef_t *) ((const byte *) data + h.ofs_fielddefs);
	for (i = 0; i < h.numfielddefs; i++)
	{
		int ofs = (int)(unsigned short) LittleShort ((short) dd[i].ofs);
		int type = (int)(unsigned short) LittleShort ((short) dd[i].type) & ~DEF_SAVEGLOBAL;
		int width = (type == ev_vector) ? 3 : 1;

		if (ofs + width > h.entityfields)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: fielddef %d out of range (ofs=%d w=%d ef=%d)\n",
				i, ofs, width, h.entityfields);
			return false;
		}
	}

	// functions: parm_start/locals within globals, first_statement within statements
	fn = (const dfunction_t *) ((const byte *) data + h.ofs_functions);
	for (i = 0; i < h.numfunctions; i++)
	{
		int first = LittleLong (fn[i].first_statement);
		int parm = LittleLong (fn[i].parm_start);
		int loc = LittleLong (fn[i].locals);
		int nparm = LittleLong (fn[i].numparms);
		int sum = 0;
		int k;

		// first_statement: builtins are negative (|first| <= builtin slots),
		// real code is [0, numstatements). INT_MIN would make OP_CALL's
		// i = -first a negative builtin index; #0:name (first == 0 for i > 0) is
		// not supported (ADR 0020).
		if (first < -CSQC_MAX_BUILTINS ||
			(first > 0 && first >= h.numstatements) ||
			(first == 0 && i > 0))
		{
			Con_Printf ("CSQC: csprogs.dat rejected: function %d first_statement out of range (%d)\n",
				i, first);
			return false;
		}
		if (nparm < 0 || nparm > MAX_PARMS || parm < 0 || loc < 0 || parm + loc > h.numglobals)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: function %d parms/locals out of range\n", i);
			return false;
		}
		// parm_size: each 0..3 (byte) and the word sum must fit locals (fteqcc
		// invariant); EnterFunction copies sum(parm_size) words from parm_start.
		for (k = 0; k < nparm; k++)
		{
			int ps = (int) fn[i].parm_size[k];
			if (ps > 3)
			{
				Con_Printf ("CSQC: csprogs.dat rejected: function %d parm_size out of range (%d)\n",
					i, ps);
				return false;
			}
			sum += ps;
		}
		if (sum > loc)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: function %d parm_size sum %d > locals %d\n",
				i, sum, loc);
			return false;
		}
	}

	return true;
}

/*
=================
PR1VM_LoadClientV6

Client v6-loader (our csprogs.dat, classic QW version 6). No CRC check; errors ->
false + Con_Printf (no SV_Error). Structural validation before executing
server-supplied bytes. Implemented in the client file (client parts live outside
shared core files).
=================
*/
static qbool PR1VM_LoadClientV6 (pr1vm_t *vm, const byte *data, int filesize)
{
	if (!PR1VM_ValidateClientV6 (data, filesize))
		return false;

	PR1VM_LoadData (vm, (dprograms_t *)data);
	return true;
}

#ifdef CSQC_DEBUG
/*
=================
PR1VM_CSQCSmoke_f

Debug command: loads csprogs.dat (classic v6) from the current gamedir into a
static client instance, resolves CSQC functions and runs CSQC_WorldLoaded. Lives in
the client file (client parts live outside shared); registered from
CSQC_Client_RegisterCommands (cl_main.c: CL_InitLocal).
=================
*/
static pr1vm_t csqc_smoke_vm;
// Separate string pool for the csqc_smoke debug instance (its own vm).
static csqc_strpool_t csqc_smoke_strpool;

static void PR1VM_CSQCSmoke_f (void)
{
	byte *data;
	int filesize;
	pr1vm_t *vm = &csqc_smoke_vm;
	dfunction_t *f;
	func_t idx;

	// Developer-gated manual diagnostic (matches pr1vm_test_error).
	if (!developer.value)
		return;

	data = (byte *)FS_LoadHunkFile ("csprogs.dat", &filesize);
	if (!data)
	{
		Con_Printf ("csqc_smoke: couldn't load csprogs.dat from gamedir\n");
		return;
	}

	// cleanup (incl. Q_free of the builtin table), then reload
	PR1VM_UnLoad (vm);
	vm->get_string = CSQC_Client_GetString;	// bounded untrusted csprogs strings
	if (!PR1VM_LoadClientV6 (vm, data, filesize))
	{
		Con_Printf ("csqc_smoke: v6 load failed\n");
		return;
	}

	// Debug instance string tables: its own pool (back-pointer in host_udata).
	memset (&csqc_smoke_strpool, 0, sizeof (csqc_smoke_strpool));
	vm->host_udata = &csqc_smoke_strpool;
	vm->strtbl = csqc_smoke_strpool.strtbl;
	vm->newstrtbl = csqc_smoke_strpool.newstrtbl;
	vm->numstr = &csqc_smoke_strpool.numstr;

	Con_Printf ("csqc_smoke: client (v6): statements=%d functions=%d globals=%d"
		" (server PR1: statements=%d functions=%d)\n",
		vm->progs->numstatements, vm->progs->numfunctions, vm->progs->numglobals,
		progs ? progs->numstatements : -1, progs ? progs->numfunctions : -1);

	// client builtin table
	CSQCVM_RegisterBuiltins (vm);

	f = PR1VM_FindFunction (vm, "CSQC_Init");
	Con_Printf ("csqc_smoke: CSQC_Init %s\n", f ? "found" : "MISSING");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		vm->globals[OFS_PARM0] = 0;
		vm->globals[OFS_PARM1] = 0;
		vm->globals[OFS_PARM2] = 0;
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: CSQC_Init executed ok (registercommand builtins)\n");
	}
	f = PR1VM_FindFunction (vm, "CSQC_WorldLoaded");
	Con_Printf ("csqc_smoke: CSQC_WorldLoaded %s\n", f ? "found" : "MISSING");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: CSQC_WorldLoaded executed ok (server PR1 still alive)\n");
	}
	f = PR1VM_FindFunction (vm, "CSQC_ConsoleCommand");
	Con_Printf ("csqc_smoke: CSQC_ConsoleCommand %s\n", f ? "found" : "MISSING");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		vm->globals[OFS_PARM0] = 0;	// empty command
		vm->globals[OFS_RETURN] = -1;
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: CSQC_ConsoleCommand ok (ret=%.0f, tokenize/argv builtins)\n",
			vm->globals[OFS_RETURN]);
	}
	// weapon_name(0) -> ftos(0)="0" (builtin ftos + string return)
	f = PR1VM_FindFunction (vm, "weapon_name");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		vm->globals[OFS_PARM0] = 0;
		vm->globals[OFS_RETURN] = 0;
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: weapon_name(0) -> \"%s\" (ftos builtin)\n",
			CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_RETURN]));
	}
}

/*
=================
CSQC_Client_ProgsCheck_f

Debug canary (client console `csqc_progscheck`): verifies the load-time validator
(clean csprogs.dat -> accepted; synthetically corrupted copies -> rejected) and runs
the runtime-guard predicate unit tests (PR1VM_TestGuards_f). Engine-side, FTE has no
equivalent command - a recorded deviation.
=================
*/
static void CSQC_Client_ProgsCheck_f (void)
{
	byte *data;
	byte *buf;
	dprograms_t *h;
	int filesize;
	int pass = 0, fail = 0;

	// Developer-gated manual diagnostic (matches pr1vm_test_error).
	if (!developer.value)
		return;

	data = (byte *)FS_LoadHunkFile ("csprogs.dat", &filesize);
	if (!data || filesize < (int)sizeof (dprograms_t))
	{
		Con_Printf ("csqc_progscheck: no/short csprogs.dat in gamedir\n");
		return;
	}

#define PC_CHECK(name, cond) \
	do { if (cond) pass++; else { fail++; Con_Printf ("[CSQC-TEST] FAIL %s\n", name); } } while (0)

	// 1) the real file must pass validation
	PC_CHECK ("clean-accepted", PR1VM_ValidateClientV6 (data, filesize));

	// 2) corrupted copies must be rejected (scratch copy, LE-safe writes)
	buf = (byte *)Q_malloc (filesize);
	if (!buf)
	{
		Con_Printf ("csqc_progscheck: out of memory\n");
		return;
	}
	h = (dprograms_t *)buf;

	memcpy (buf, data, filesize);
	h->ofs_functions = LittleLong (filesize + 4096);
	PC_CHECK ("ofs-functions-oob", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	h->numstatements = LittleLong (0x7fffffff);
	PC_CHECK ("numstatements-huge", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	h->numglobals = LittleLong (0x7fffffff);
	PC_CHECK ("numglobals-huge", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	h->version = LittleLong (7);
	PC_CHECK ("bad-version", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	PC_CHECK ("tiny-size", !PR1VM_ValidateClientV6 (buf, 8));

	memset (buf, 0, filesize);
	PC_CHECK ("zeroed", !PR1VM_ValidateClientV6 (buf, filesize));

	// 2b) OP_STOREP_V vector source width: a == numglobals-1 must be rejected
	// (interpreter reads globals[a+1..a+2]), a == numglobals-3 must be accepted.
	{
		dstatement_t *st;
		int numglobals = LittleLong (((dprograms_t *) data)->numglobals);
		int ofs_st = LittleLong (((dprograms_t *) data)->ofs_statements);

		memcpy (buf, data, filesize);
		st = (dstatement_t *) (buf + ofs_st);
		st->op = (unsigned short) LittleShort ((short) OP_STOREP_V);
		st->a = (short) LittleShort ((short) (numglobals - 1));
		st->b = (short) LittleShort (0);
		st->c = (short) LittleShort (0);
		PC_CHECK ("storep_v-a-vector-oob", !PR1VM_ValidateClientV6 (buf, filesize));

		memcpy (buf, data, filesize);
		st->op = (unsigned short) LittleShort ((short) OP_STOREP_V);
		st->a = (short) LittleShort ((short) (numglobals - 3));
		st->b = (short) LittleShort (0);
		st->c = (short) LittleShort (0);
		PC_CHECK ("storep_v-a-vector-edge", PR1VM_ValidateClientV6 (buf, filesize));
	}

	// 2c) OP_STORE_V vector width: both a (source) and b (dest) are 3 words;
	// a == numglobals-1 must be rejected, a == b == numglobals-3 accepted.
	{
		dstatement_t *st;
		int numglobals = LittleLong (((dprograms_t *) data)->numglobals);
		int ofs_st = LittleLong (((dprograms_t *) data)->ofs_statements);

		memcpy (buf, data, filesize);
		st = (dstatement_t *) (buf + ofs_st);
		st->op = (unsigned short) LittleShort ((short) OP_STORE_V);
		st->a = (short) LittleShort ((short) (numglobals - 1));
		st->b = (short) LittleShort (0);
		st->c = (short) LittleShort (0);
		PC_CHECK ("store_v-a-vector-oob", !PR1VM_ValidateClientV6 (buf, filesize));

		memcpy (buf, data, filesize);
		st->op = (unsigned short) LittleShort ((short) OP_STORE_V);
		st->a = (short) LittleShort ((short) (numglobals - 3));
		st->b = (short) LittleShort ((short) (numglobals - 3));
		st->c = (short) LittleShort (0);
		PC_CHECK ("store_v-a-vector-edge", PR1VM_ValidateClientV6 (buf, filesize));
	}

	// 2d) defs: an offset at/past the lump end must be rejected (width-aware).
	// type is forced to ev_float (width 1) so the reject is the ofs bound, not
	// vector width.
	{
		ddef_t *dd;
		int numglobals = LittleLong (((dprograms_t *) data)->numglobals);
		int entityfields = LittleLong (((dprograms_t *) data)->entityfields);
		int numgd = LittleLong (((dprograms_t *) data)->numglobaldefs);
		int numfd = LittleLong (((dprograms_t *) data)->numfielddefs);
		int ofs_gd = LittleLong (((dprograms_t *) data)->ofs_globaldefs);
		int ofs_fd = LittleLong (((dprograms_t *) data)->ofs_fielddefs);

		if (numgd >= 1)
		{
			memcpy (buf, data, filesize);
			dd = (ddef_t *) (buf + ofs_gd);
			dd->type = (unsigned short) LittleShort ((short) ev_float);
			dd->ofs = (unsigned short) LittleShort ((short) numglobals);
			PC_CHECK ("globaldef-ofs-oob", !PR1VM_ValidateClientV6 (buf, filesize));
		}
		else
			Con_Printf ("csqc_progscheck: skip globaldef-ofs-oob (numglobaldefs=0)\n");

		if (numfd >= 1)
		{
			memcpy (buf, data, filesize);
			dd = (ddef_t *) (buf + ofs_fd);
			dd->type = (unsigned short) LittleShort ((short) ev_float);
			dd->ofs = (unsigned short) LittleShort ((short) entityfields);
			PC_CHECK ("fielddef-ofs-oob", !PR1VM_ValidateClientV6 (buf, filesize));
		}
		else
			Con_Printf ("csqc_progscheck: skip fielddef-ofs-oob (numfielddefs=0)\n");
	}

	// 2e) parm_size: a word count > 3 must be rejected.
	{
		dfunction_t *fnp;
		int numf = LittleLong (((dprograms_t *) data)->numfunctions);
		int ofs_fn = LittleLong (((dprograms_t *) data)->ofs_functions);

		if (numf >= 1)
		{
			memcpy (buf, data, filesize);
			fnp = (dfunction_t *) (buf + ofs_fn);
			fnp->numparms = LittleLong (1);
			fnp->parm_size[0] = 4;
			PC_CHECK ("parm_size-oob", !PR1VM_ValidateClientV6 (buf, filesize));
		}
		else
			Con_Printf ("csqc_progscheck: skip parm_size-oob (numfunctions=0)\n");
	}

	// 2f) first_statement: INT_MIN and 0 for i>0 rejected; -1 (builtin) accepted.
	{
		dfunction_t *fnp;
		int numf = LittleLong (((dprograms_t *) data)->numfunctions);
		int ofs_fn = LittleLong (((dprograms_t *) data)->ofs_functions);

		if (numf >= 2)
		{
			memcpy (buf, data, filesize);
			fnp = (dfunction_t *) (buf + ofs_fn);
			fnp[1].first_statement = LittleLong (INT_MIN);
			PC_CHECK ("firststmt-intmin", !PR1VM_ValidateClientV6 (buf, filesize));

			memcpy (buf, data, filesize);
			fnp = (dfunction_t *) (buf + ofs_fn);
			fnp[1].first_statement = LittleLong (0);
			PC_CHECK ("firststmt-zero-nonfirst", !PR1VM_ValidateClientV6 (buf, filesize));

			memcpy (buf, data, filesize);
			fnp = (dfunction_t *) (buf + ofs_fn);
			fnp[1].first_statement = LittleLong (-1);
			PC_CHECK ("firststmt-builtin-ok", PR1VM_ValidateClientV6 (buf, filesize));
		}
		else
			Con_Printf ("csqc_progscheck: skip first_statement cases (numfunctions<2)\n");
	}

	// 2g) lump-range division and entityfields cap. The 32-bit product wrap only
	// manifests on i686; the division keeps the result bitness-invariant, so the
	// same reject holds on 64-bit.
	{
		memcpy (buf, data, filesize);
		h = (dprograms_t *) buf;
		h->numstatements = LittleLong (0x40000000);
		PC_CHECK ("lump-division-wrap", !PR1VM_ValidateClientV6 (buf, filesize));

		memcpy (buf, data, filesize);
		h = (dprograms_t *) buf;
		h->entityfields = LittleLong (0x40000000);
		PC_CHECK ("entityfields-cap", !PR1VM_ValidateClientV6 (buf, filesize));
	}

	Q_free (buf);

	// 3) client string accessor: a positive offset at/beyond the module string
	// block must be rejected; in-range stays readable. The bound lives here (client
	// layer), not in the shared PR1VM_GetString.
	{
		pr1vm_t tvm;
		dprograms_t xh;
		int ns = LittleLong (((dprograms_t *) data)->numstrings);

		memset (&tvm, 0, sizeof (tvm));
		memset (&xh, 0, sizeof (xh));
		xh.numstrings = ns;
		tvm.strings = (char *) data;
		tvm.progs = &xh;
		PC_CHECK ("clientstr-in-range", CSQC_Client_GetString (&tvm, 0) != NULL);
		PC_CHECK ("clientstr-oob", CSQC_Client_GetString (&tvm, ns) == NULL);
		PC_CHECK ("clientstr-nullvm", CSQC_Client_GetString (NULL, 0) == NULL);
	}

#undef PC_CHECK

	// 3) runtime-guard predicate unit tests (synthetic instance)
	PR1VM_TestGuards_f ();

	// 4) entity-argument conversion bound (CSQC_Client_EntNum) - OOB must clamp to
	// world(0), never fault (FTE ProgsToEdict parity). Synthetic instance: the
	// helper only needs edict_size/max_edicts.
	{
		pr1vm_t tvm;
		int gpass = 0, gfail = 0;

		memset (&tvm, 0, sizeof (tvm));
		tvm.edict_size = 16;
		tvm.max_edicts = 4;

		PR1VM_GuardCheck ("ent_of-zero",     CSQC_Client_EntNum (&tvm, 0) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-3",        CSQC_Client_EntNum (&tvm, 3 * 16) == 3, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-over",     CSQC_Client_EntNum (&tvm, 4 * 16) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-negative", CSQC_Client_EntNum (&tvm, -1) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-huge",     CSQC_Client_EntNum (&tvm, 0x40000000) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-small",    CSQC_Client_EntNum (&tvm, 1) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-nullvm",   CSQC_Client_EntNum (NULL, 3 * 16) == 0, &gpass, &gfail);

		Con_Printf ("[CSQC-TEST] SUMMARY group=ent_of pass=%d fail=%d\n", gpass, gfail);
	}

	Con_Printf ("[CSQC-TEST] SUMMARY group=progscheck pass=%d fail=%d\n", pass, fail);
}
#endif

/*
=================
CSQC_Client_Load

Loads csprogs (path from gamedir; a local file or a freshly downloaded
csprogsvers/<crc>.dat) into the client instance and calls CSQC_Init. Returns true
on success; on failure prints the reason.
=================
*/
static qbool CSQC_Client_Load (const char *path)
{
	byte *data;
	int filesize;
	pr1vm_t *vm;
	dfunction_t *f;

	// Heap-owned buffer: PR1VM_LoadData does not copy it (vm->progs/... reference
	// data), so the data must live until the module is unloaded. Owned via
	// s_csqc.module_data, freed on a repeated Load, on load failure and in
	// CSQC_Client_Disconnect (after PR1VM_UnLoad).
	data = (byte *)FS_LoadHeapFile (path, &filesize);
	if (!data)
	{
		Con_Printf ("CSQC: server offers csprogs but %s not found locally\n", path);
		return false;
	}

	// Guard against a repeated Load (arena from a previous load) before memset.
	CSQC_Client_FreeArena ();
	// clear string-buffers on a new module load
	CSQC_Client_BufReset ();
	// #371: drop deltalisten registrations / the player-bridge map.
	CSQC_Client_DeltaReset ();
	CSQC_Client_ViewReset ();
	CSQC_Client_ModelReset ();	// CSQC model registry cleared on load

	// Free a buffer left from a previous load (its VM is dropped by the reset
	// below and no longer referenced after the resets above).
	Q_free (s_csqc.module_data);
	s_csqc.module_data = NULL;

	memset (&s_csqc, 0, sizeof (s_csqc));
	s_csqc.module_data = data;	// heap-owned; freed on load failure and unload
	s_csqc.func_init = s_csqc.func_world = s_csqc.func_update =
		s_csqc.func_console = s_csqc.func_shutdown = -1;
	s_csqc.func_entupdate = s_csqc.func_entremove = s_csqc.func_parseevent = -1;
	s_csqc.func_parseprint = s_csqc.func_parsecp = -1;
	s_csqc.func_parsedamage = -1;
	s_csqc.func_eventsound = -1;
	s_csqc.func_parsesetangles = -1;
	s_csqc.func_rr = -1;
	s_csqc.func_entspawn = -1;
	s_csqc.mayread = false;
	s_csqc.func_input = -1;
	s_csqc.func_inputevent = -1;
	s_csqc.func_startframe = s_csqc.func_endframe = -1;
	s_csqc.global_time = -1;
	s_csqc.global_gamespeed = -1;
	s_csqc.global_self = -1;
	s_csqc.global_other = -1;
	s_csqc.global_physics_mode = -1;
	s_csqc.field_entnum = -1;
	s_csqc.f_origin = s_csqc.f_velocity = s_csqc.f_angles = s_csqc.f_mins = s_csqc.f_maxs = -1;
	s_csqc.f_movetype = s_csqc.f_flags = s_csqc.f_gravity = s_csqc.f_pmove_flags = -1;
	s_csqc.f_modelindex = s_csqc.f_skin = -1;
	s_csqc.f_frame = s_csqc.f_effects = s_csqc.f_colormap = s_csqc.f_drawmask = -1;
	s_csqc.f_think = s_csqc.f_nextthink = -1;
	s_csqc.g_localentnum = -1;
	s_csqc.in_timelength = s_csqc.in_angles = s_csqc.in_movevalues = -1;
	s_csqc.in_buttons = s_csqc.in_impulse = -1;
	s_csqc.in_sequence = -1;
	s_csqc.g_ccframe = s_csqc.g_scframe = -1;
	s_csqc.p_org = s_csqc.p_vel = s_csqc.p_onground = -1;
	s_csqc.g_vfwd = s_csqc.g_vright = s_csqc.g_vup = -1;
	s_csqc.g_view_angles = -1;
	s_csqc.g_frametime = s_csqc.g_cltime = s_csqc.g_maxclients = -1;
	s_csqc.g_player_localnum = s_csqc.g_intermission = -1;
	CSQC_Client_OffsetCacheReset ();	// hot-path offsets, before resolve
	s_last_seq = 0;
	s_ccframe = 0;

	vm = &s_csqc.vm;
	vm->host_error = CSQC_Client_HostError;
	vm->host_print = CSQC_Client_HostPrint;
	vm->abortbuf_valid = true;	// client VM unwinds via the abort-stack
	vm->get_string = CSQC_Client_GetString;	// bounded untrusted csprogs strings
	vm->stateop = CSQC_Client_StateOp;	// OP_STATE via the module's field/global

	if (!PR1VM_LoadClientV6 (vm, data, filesize))
	{
		Con_Printf ("CSQC: %s load failed (v6)\n", path);
		Q_free (data);
		s_csqc.module_data = NULL;
		return false;
	}

	// Client instance string tables: vm->strtbl/newstrtbl/numstr -> the instance
	// pool; host_udata is the back-pointer for PR1VM_ClientSetString.
	vm->host_udata = &s_csqc.strpool;
	vm->strtbl = s_csqc.strpool.strtbl;
	vm->newstrtbl = s_csqc.strpool.newstrtbl;
	vm->numstr = &s_csqc.strpool.numstr;

	CSQCVM_RegisterBuiltins (vm);

	// client instance edict arena (edict_size known after load).
	CSQC_Client_AllocArena (vm);

	f = PR1VM_FindFunction (vm, "CSQC_Init");
	if (f)
		s_csqc.func_init = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_WorldLoaded");
	if (f)
		s_csqc.func_world = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_UpdateView");
	if (f)
		s_csqc.func_update = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_ConsoleCommand");
	if (f)
		s_csqc.func_console = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Shutdown");
	if (f)
		s_csqc.func_shutdown = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Ent_Update");
	if (f)
		s_csqc.func_entupdate = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Ent_Remove");
	if (f)
		s_csqc.func_entremove = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Ent_Spawn");
	if (f)
		s_csqc.func_entspawn = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Parse_Event");
	if (f)
		s_csqc.func_parseevent = (int)(f - vm->functions);
	// network print callbacks
	f = PR1VM_FindFunction (vm, "CSQC_Parse_Print");
	if (f)
		s_csqc.func_parseprint = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Parse_CenterPrint");
	if (f)
		s_csqc.func_parsecp = (int)(f - vm->functions);
	// network damage callback
	f = PR1VM_FindFunction (vm, "CSQC_Parse_Damage");
	if (f)
		s_csqc.func_parsedamage = (int)(f - vm->functions);
	// network sound callback
	f = PR1VM_FindFunction (vm, "CSQC_Event_Sound");
	if (f)
		s_csqc.func_eventsound = (int)(f - vm->functions);
	// network angles callback
	f = PR1VM_FindFunction (vm, "CSQC_Parse_SetAngles");
	if (f)
		s_csqc.func_parsesetangles = (int)(f - vm->functions);
	// renderer reinit callback
	f = PR1VM_FindFunction (vm, "CSQC_RendererRestarted");
	if (f)
		s_csqc.func_rr = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Input_Frame");
	if (f)
		s_csqc.func_input = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_InputEvent");
	if (f)
		s_csqc.func_inputevent = (int)(f - vm->functions);
	// CSQC think-loop - StartFrame/EndFrame.
	f = PR1VM_FindFunction (vm, "StartFrame");
	if (f)
		s_csqc.func_startframe = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "EndFrame");
	if (f)
		s_csqc.func_endframe = (int)(f - vm->functions);

	s_csqc.global_time = PR1VM_FindGlobal (vm, "time");
	// gamespeed (engine-set). QW/ezq has no cl.gamespeed, so publish 1 (0 on server
	// pause) - see CSQC_Client_Update.
	s_csqc.global_gamespeed = PR1VM_FindGlobal (vm, "gamespeed");
	// self global and the .entnum field (the engine writes them on entity calls).
	s_csqc.global_self = PR1VM_FindGlobal (vm, "self");
	// other (world for StartFrame/EndFrame/thinks; FTE CSQC_Event_Think) and
	// physics_mode (default 2).
	s_csqc.global_other = PR1VM_FindGlobal (vm, "other");
	s_csqc.global_physics_mode = PR1VM_FindGlobal (vm, "physics_mode");
	s_csqc.field_entnum = CSQC_Client_FindField (vm, "entnum");
	// #347: standard physics fields (if present in the module's schema).
	s_csqc.f_origin = CSQC_Client_FindField (vm, "origin");
	s_csqc.f_velocity = CSQC_Client_FindField (vm, "velocity");
	s_csqc.f_angles = CSQC_Client_FindField (vm, "angles");
	s_csqc.f_mins = CSQC_Client_FindField (vm, "mins");
	s_csqc.f_maxs = CSQC_Client_FindField (vm, "maxs");
	s_csqc.f_movetype = CSQC_Client_FindField (vm, "movetype");
	s_csqc.f_flags = CSQC_Client_FindField (vm, "flags");
	s_csqc.f_gravity = CSQC_Client_FindField (vm, "gravity");
	s_csqc.f_pmove_flags = CSQC_Client_FindField (vm, "pmove_flags");
	s_csqc.f_modelindex = CSQC_Client_FindField (vm, "modelindex");
	s_csqc.f_skin = CSQC_Client_FindField (vm, "skin");
	s_csqc.f_frame = CSQC_Client_FindField (vm, "frame");
	s_csqc.f_effects = CSQC_Client_FindField (vm, "effects");
	s_csqc.f_colormap = CSQC_Client_FindField (vm, "colormap");
	s_csqc.f_drawmask = CSQC_Client_FindField (vm, "drawmask");
	// think-loop: .think/.nextthink fields.
	s_csqc.f_think = CSQC_Client_FindField (vm, "think");
	s_csqc.f_nextthink = CSQC_Client_FindField (vm, "nextthink");
	s_csqc.g_localentnum = PR1VM_FindGlobal (vm, "player_localentnum");

	// input_* globals for CSQC_Input_Frame (input_timelength/angles/movevalues/
	// buttons/impulse). Resolve only those the module declared.
	s_csqc.in_timelength = PR1VM_FindGlobal (vm, "input_timelength");
	s_csqc.in_angles = PR1VM_FindGlobal (vm, "input_angles");
	s_csqc.in_movevalues = PR1VM_FindGlobal (vm, "input_movevalues");
	s_csqc.in_buttons = PR1VM_FindGlobal (vm, "input_buttons");
	s_csqc.in_impulse = PR1VM_FindGlobal (vm, "input_impulse");
	s_csqc.in_sequence = PR1VM_FindGlobal (vm, "input_sequence");
	// prediction window globals + deprecated pmove_*.
	s_csqc.g_ccframe = PR1VM_FindGlobal (vm, "clientcommandframe");
	s_csqc.g_scframe = PR1VM_FindGlobal (vm, "servercommandframe");
	s_csqc.p_org = PR1VM_FindGlobal (vm, "pmove_org");
	s_csqc.p_vel = PR1VM_FindGlobal (vm, "pmove_vel");
	s_csqc.p_onground = PR1VM_FindGlobal (vm, "pmove_onground");
	// #1 makevectors: v_forward/v_right/v_up write targets (FTE parity).
	s_csqc.g_vfwd = PR1VM_FindGlobal (vm, "v_forward");
	s_csqc.g_vright = PR1VM_FindGlobal (vm, "v_right");
	s_csqc.g_vup = PR1VM_FindGlobal (vm, "v_up");
	// view_angles global (published each frame).
	s_csqc.g_view_angles = PR1VM_FindGlobal (vm, "view_angles");
	// simulated globals.
	s_csqc.g_frametime = PR1VM_FindGlobal (vm, "frametime");
	s_csqc.g_cltime = PR1VM_FindGlobal (vm, "cltime");
	s_csqc.g_maxclients = PR1VM_FindGlobal (vm, "maxclients");
	s_csqc.g_player_localnum = PR1VM_FindGlobal (vm, "player_localnum");
	s_csqc.g_intermission = PR1VM_FindGlobal (vm, "intermission");

	// resolve the hot-path offset cache (traceline/addentities).
	CSQC_Client_OffsetCacheResolve (vm);

	s_csqc.loaded = true;

	Con_DPrintf ("CSQC: loaded %s (%d statements, crc=0x%x), funcs i=%d w=%d u=%d "
		"c=%d s=%d eu=%d er=%d pe=%d if=%d ie=%d time=%d\n",
		path, vm->progs->numstatements, (unsigned int)vm->progs->crc, s_csqc.func_init,
		s_csqc.func_world, s_csqc.func_update, s_csqc.func_console, s_csqc.func_shutdown,
		s_csqc.func_entupdate, s_csqc.func_entremove, s_csqc.func_parseevent,
		s_csqc.func_input, s_csqc.func_inputevent, s_csqc.global_time);
	// CSQC_Init(apiver, enginename, enginever) - FTE parity: apiver =
	// CSQC_API_VERSION, enginename = engine name, enginever = version number. The
	// module uses the arguments only as hints.
	if (s_csqc.func_init > 0)
	{
		vm->globals[OFS_PARM0] = CSQC_API_VERSION;
		PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM1], "ezQuake");
		vm->globals[OFS_PARM2] = VERSION_NUM;
		CSQC_Client_Exec (s_csqc.func_init);
		s_csqc.inited = !s_csqc.errored;
	}
	// Right after CSQC_Init notify the module about renderer (re)init - FTE parity
	// (CSQC_RendererRestarted(true)), before the first CSQC_WorldLoaded.
	CSQC_Client_RendererRestarted (R_RendererDescription ());
	// The module registered csqc_dbg via registercvar (#93) in CSQC_Init - cache the
	// pointer for the hot path (CSQC_Client_ParseEntities).
	s_csqc.csqc_dbg_cvar = Cvar_Find ("csqc_dbg");
	return true;
}

/*
=================
CSQC_Client_NotifyCSQC

FTE parity: tell the server whether our module receives the CSQC stream. enablecsqc
- module loaded and ready (after CSQC_WorldLoaded); disablecsqc - server offered
CSQC but the module did not start. Idempotent (a repeated state is not sent); a
no-op in demos / without a connection (CL_SendClientCommand).
=================
*/
static void CSQC_Client_NotifyCSQC (qbool enable)
{
#ifdef FTE_PEXT_CSQC
	if (!(cls.fteprotocolextensions & FTE_PEXT_CSQC))
		return;
#endif
	if (s_csqc.enable_sent && s_csqc.enable_value == enable)
		return;
	CL_SendClientCommand (true, enable ? "enablecsqc" : "disablecsqc");
	s_csqc.enable_sent = true;
	s_csqc.enable_value = enable;
	Con_DPrintf ("CSQC: %s sent\n", enable ? "enablecsqc" : "disablecsqc");
}

/*
=================
CSQC_Client_ConnectCheck

Called on entering the world (CL_MakeActive, before ca_active) - the moment when
all content (including csprogs.dat) is already available in FS (analog of FTE's
prespawn). If the server offers CSQC (*csprogssize) and the module is not loaded -
load it and call CSQC_Init.
=================
*/
void CSQC_Client_ConnectCheck (void)
{
	extern cvar_t cl_pext_csqc;
	extern cvar_t cl_download_csprogs;
	const char *name, *crcs;
	unsigned crc;
	int sizep;
	char path[MAX_QPATH];
	char *crcend;
	qbool anycsqc;

	// client map-uptime (FTE cltime = realtime-cl.mapstarttime) and frametime base
	// (cl.time - prev). Set on each world entry.
	s_mapstarttime = cls.realtime;
	s_prev_cltime = cl.time;

	// Master switch (analog of FTE cl_nocsqc): 0 - all CSQC disabled, the module is
	// not loaded, the client behaves as before.
	if (!cl_pext_csqc.value)
		return;

	sizep = (int)strtoul (Info_ValueForKey (cl.serverinfo, "*csprogssize"), NULL, 0);

	// anycsqc (FTE parity): the server allows loading a local csprogs without crc
	// check; a "corrupt" *csprogs (trailing garbage) FTE also treats as anycsqc. In
	// demos there is no check.
	anycsqc = atoi (Info_ValueForKey (cl.serverinfo, "anycsqc")) != 0;
	crcs = Info_ValueForKey (cl.serverinfo, "*csprogs");
	crc = (unsigned)strtoul (crcs, &crcend, 0);
	if (crcs[0] && *crcend)
	{
		Con_Printf ("CSQC: corrupt *csprogs key in serverinfo\n");
		anycsqc = true;
		crc = 0;
	}
	if (cls.demoplayback)
		anycsqc = true;

	if (sizep <= 0 && !anycsqc)
		return;		// ordinary server without CSQC (or server PR1 gate)

	name = Info_ValueForKey (cl.serverinfo, "*csprogsname");
	if (!name || !name[0])
		name = "csprogs.dat";

	// The module is loaded "from scratch" on EVERY world entry (first connect and
	// every map change): unload happens on exiting the world (CL_ClearState, before
	// Host_ClearMemory), here - loading fresh csprogs. A defensive unload in case of
	// paths without CL_ClearState (a double call is a safe no-op).
	if (s_csqc.loaded)
		CSQC_Client_Disconnect ();

	// Local candidates with FTE semantics (CSQC_FindMainProgs): 1) the
	// csprogsvers/<crc>.dat cache, 2) *csprogsname (+ fallback csprogs.dat); on a
	// valid name-file a copy is written back into the cache. anycsqc/demo - no
	// size/crc check.
	if (CSQC_Client_FindMainProgs (path, sizeof (path), name, sizep, crc, anycsqc))
	{
		if (!CSQC_Client_Load (path))
		{
			CSQC_Client_NotifyCSQC (false);
			return;
		}
		if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		{
			CSQC_Client_NotifyCSQC (false);
			return;
		}
		// Entering a new map: per-map state is clean (WorldLoaded/enablecsqc will be
		// this map's; the module is already new).
		memset (s_csqc.seen, 0, sizeof (s_csqc.seen));
		s_csqc.world_done = false;
		s_csqc.enable_sent = false;
		return;
	}

	// Demo/MVD: no local csprogs, download is unavailable in demos (StartDownload is
	// a no-op) - do not set pending (otherwise a false timeout).
	if (cls.demoplayback)
	{
		Con_Printf ("CSQC: no local csprogs for demo playback\n");
		return;
	}

	// csprogs download disabled by cvar (FTE parity: cl_download_csprogs): the
	// module is not loaded, tell the server disablecsqc.
	if (!cl_download_csprogs.value)
	{
		Con_Printf ("CSQC: not downloading %s (cl_download_csprogs 0)\n", name);
		CSQC_Client_NotifyCSQC (false);
		return;
	}

	// No valid local one - download from the server: the server serves *csprogsname,
	// we save into a separate csprogsvers/<crc>.dat folder (do not overwrite others).
	// Module load happens in CSQC_Client_Update once the file appears.
	s_csqc.csprogs_crc = crc;
	s_csqc.csprogs_size = sizep;
	if (crc)
		snprintf (s_csqc.csprogs_dl_path, sizeof (s_csqc.csprogs_dl_path), "csprogsvers/%x.dat", crc);
	else
		snprintf (s_csqc.csprogs_dl_path, sizeof (s_csqc.csprogs_dl_path), "%s", name);
	CSQC_Client_StartDownload (name, s_csqc.csprogs_dl_path);
	s_csqc.csprogs_dl_pending = true;
}

/*
=================
EXT_CSQC_1 prediction window - the module globals clientcommandframe /
servercommandframe.

- clientcommandframe = the "live" (last built) client frame = s_ccframe (FTE
  cl.movesequence). Set in CSQC_Client_InputFrame at cmd build and NOT recomputed
  after sending: Netchan_Transmit increments outgoing_sequence, but clientcommandframe
  stays the built cmd's number. This is what #345(clientcommandframe) asks for - the
  live pending frame; the next one does not exist yet.
- servercommandframe = the last server-acked client frame = cl.parsecount (analog of
  FTE QW ackedmovesequence).
- Prediction unavailable (0): demo/MVD, not ca_active, before the first accepted
  server frame (cl.validsequence == 0).
- The window (servercommandframe, clientcommandframe] is the module's contract (the
  engine does not check it; see ext_csqc_1 spec) - see CSQC_Client_ApplyInput.
=================
*/
static float CSQC_Client_ClientCmdFrame (void)
{
	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (cls.state != ca_active || cls.demoplayback || cls.mvdplayback)
		return 0;
	// The last built frame (FTE cl.movesequence), not the next outgoing_sequence -
	// otherwise in the render phase #345(clientcommandframe) would ask for a
	// not-yet-built seq and get 0 (see s_ccframe).
	return (float)s_ccframe;
}

static float CSQC_Client_ServerCmdFrame (void)
{
	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (cls.state != ca_active || cls.demoplayback || cls.mvdplayback)
		return 0;
	if (!cl.validsequence)
		return 0;	// no accepted server frame yet (prespawn)
	return (float)cl.parsecount;
}

static void CSQC_Client_PatchFrames (void)
{
	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.g_ccframe >= 0)
		s_csqc.vm.globals[s_csqc.g_ccframe] = CSQC_Client_ClientCmdFrame ();
	if (s_csqc.g_scframe >= 0)
		s_csqc.vm.globals[s_csqc.g_scframe] = CSQC_Client_ServerCmdFrame ();
}

/*
=================
CSQC think-loop - the per-frame module lifecycle in 3D takeover.

FTE parity (CSQC_DrawView; CSQC_Event_Think): StartFrame -> thinks
(.nextthink/.think, single-think NQ-style) -> EndFrame. StartFrame/EndFrame:
self/other = world, time = frame time. think: self = entity, other = world, time =
frame time (FTE overrides thinktime with the physics time), nextthink is zeroed
before the call. thinks run only when physics_mode != 0. Deviations:
World_Physics_Frame mode 2 (movetypes), customphysics and PR_RunThreads (PR1VM has
no sleep/fork -> no-op).
=================
*/
static void CSQC_Client_RunFrameThink (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	int mode, slot;
	float t, frame;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;

	// Base is the module time, window is the frame interval (analog of FTE
	// host_frametime).
	CSQC_Client_SetTime ();
	t = (s_csqc.global_time >= 0) ? vm->globals[s_csqc.global_time] : 0;
	frame = (float)cls.frametime;
	if (frame < 0)
		frame = 0;

	// physics_mode: default 2; 0 = "original csqc" - physics not run.
	mode = (s_csqc.global_physics_mode >= 0)
		? (int)vm->globals[s_csqc.global_physics_mode] : 2;

	// StartFrame: self/other = world.
	if (s_csqc.func_startframe > 0)
	{
		if (s_csqc.global_self >= 0)
			*(int *)&vm->globals[s_csqc.global_self] = 0;
		if (s_csqc.global_other >= 0)
			*(int *)&vm->globals[s_csqc.global_other] = 0;
		if (s_csqc.global_time >= 0)
			vm->globals[s_csqc.global_time] = t;
		PR1VM_ExecuteProgram (vm, (func_t)s_csqc.func_startframe);
		if (s_csqc.errored)
			return;
	}

	// PR_RunThreads: PR1VM has no sleep/fork -> no-op (FTE parity without threads).

	// thinks: mode1 (DP-compat) and mode2 (movetypes) - only thinks here (mode2
	// movetypes). Slot 0 = world, free/unoccupied slots are skipped.
	if (mode != 0 && s_csqc.f_think >= 0 && s_csqc.f_nextthink >= 0)
	{
		for (slot = 1; slot < CSQC_MAX_EDICTS; slot++)
		{
			float *base, nt;
			int thinkfunc;

			if (!s_used[slot])
				continue;
			base = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);
			nt = base[s_csqc.f_nextthink];
			if (nt <= 0 || nt > t + frame)
				continue;
			thinkfunc = *(int *)&base[s_csqc.f_think];
			base[s_csqc.f_nextthink] = 0;
			if (thinkfunc <= 0)
				continue;	// nextthink without think - skip (FTE logs a warning)
			// FTE CSQC_Event_Think: self=entity, other=world, time=frame time.
			if (s_csqc.global_self >= 0)
				*(int *)&vm->globals[s_csqc.global_self] = (int)slot * vm->edict_size;
			if (s_csqc.global_other >= 0)
				*(int *)&vm->globals[s_csqc.global_other] = 0;
			if (s_csqc.global_time >= 0)
				vm->globals[s_csqc.global_time] = t;
			PR1VM_ExecuteProgram (vm, (func_t)thinkfunc);
			if (s_csqc.errored)
				return;	// an edict may have removed itself - looping s_used is safe
		}
	}

	// EndFrame: self/other = world.
	if (s_csqc.func_endframe > 0)
	{
		if (s_csqc.global_self >= 0)
			*(int *)&vm->globals[s_csqc.global_self] = 0;
		if (s_csqc.global_other >= 0)
			*(int *)&vm->globals[s_csqc.global_other] = 0;
		if (s_csqc.global_time >= 0)
			vm->globals[s_csqc.global_time] = t;
		PR1VM_ExecuteProgram (vm, (func_t)s_csqc.func_endframe);
	}
}

/*
=================
CSQC_Client_NotMenu / CSQC_Client_KeynumToQC / CSQC_Client_QCToKeynum

FTE parity. FTE reference: notmenu (`!Key_Dest_Has(kdm_menu|kdm_cwindows)`); key
translation via MP_TranslateFTEtoQCCodes (FTE->QC) and MP_TranslateQCtoFTECodes
(QC->FTE). The internal ezq domain is keys.h (K_*, K_MOUSE1=200, K_MWHEELUP=244); the
QC/CSQC contract is DP numbering. The module receives/returns only QC codes; unknown
keys go through "natively" (negative value of its own keynum) - as the FTE default,
round-trip is preserved.
=================
*/
qbool CSQC_Client_NotMenu (void)
{
	// Any layered menu hides the game (FTE kdm_menu|kdm_cwindows); console/chat/
	// startup demo stay notmenu=1 (FTE does not exclude kdm_console).
	return !(key_dest == key_menu || key_dest == key_hudeditor
		|| key_dest == key_demo_controls || key_dest == key_startupdemo_menu);
}

int CSQC_Client_KeynumToQC (int keynum)
{
	switch (keynum)
	{
	case K_TAB:			return 9;
	case K_ENTER:		return 13;
	case K_ESCAPE:		return 27;
	case K_SPACE:		return 32;
	case K_BACKSPACE:	return 127;

	case K_UPARROW:		return 128;
	case K_DOWNARROW:	return 129;
	case K_LEFTARROW:	return 130;
	case K_RIGHTARROW:	return 131;

	case K_LALT:		return 132;
	case K_LCTRL:		return 133;
	case K_LSHIFT:		return 134;

	case K_F1:			return 135;
	case K_F2:			return 136;
	case K_F3:			return 137;
	case K_F4:			return 138;
	case K_F5:			return 139;
	case K_F6:			return 140;
	case K_F7:			return 141;
	case K_F8:			return 142;
	case K_F9:			return 143;
	case K_F10:			return 144;
	case K_F11:			return 145;
	case K_F12:			return 146;

	case K_INS:			return 147;
	case K_DEL:			return 148;
	case K_PGDN:		return 149;
	case K_PGUP:		return 150;
	case K_HOME:		return 151;
	case K_END:			return 152;
	case K_PAUSE:		return 153;

	case KP_NUMLOCK:	return 154;
	case K_CAPSLOCK:	return 155;
	case K_SCRLCK:		return 156;

	case KP_INS:		return 157;
	case KP_END:		return 158;
	case KP_DOWNARROW:	return 159;
	case KP_PGDN:		return 160;
	case KP_LEFTARROW:	return 161;
	case KP_5:			return 162;
	case KP_RIGHTARROW:	return 163;
	case KP_HOME:		return 164;
	case KP_UPARROW:	return 165;
	case KP_PGUP:		return 166;
	case KP_DEL:		return 167;
	case KP_SLASH:		return 168;
	case KP_STAR:		return 169;
	case KP_MINUS:		return 170;
	case KP_PLUS:		return 171;
	case KP_ENTER:		return 172;

	case K_PRINTSCR:	return 174;

	// mouse: DP interleaves the wheels between MOUSE3 and MOUSE4.
	case K_MOUSE1:		return 512;
	case K_MOUSE2:		return 513;
	case K_MOUSE3:		return 514;
	case K_MWHEELUP:	return 515;
	case K_MWHEELDOWN:	return 516;
	case K_MOUSE4:		return 517;
	case K_MOUSE5:		return 518;
	case K_MOUSE6:		return 519;
	case K_MOUSE7:		return 520;
	case K_MOUSE8:		return 521;

	case K_JOY1:		return 768;
	case K_JOY2:		return 769;
	case K_JOY3:		return 770;
	case K_JOY4:		return 771;

	// FTE K_AUX1..16 -> 800..815 (the module's csdefs gives 784..799 - a known
	// constant discrepancy, out of scope).
	case K_AUX1:		return 800;
	case K_AUX2:		return 801;
	case K_AUX3:		return 802;
	case K_AUX4:		return 803;
	case K_AUX5:		return 804;
	case K_AUX6:		return 805;
	case K_AUX7:		return 806;
	case K_AUX8:		return 807;
	case K_AUX9:		return 808;
	case K_AUX10:		return 809;
	case K_AUX11:		return 810;
	case K_AUX12:		return 811;
	case K_AUX13:		return 812;
	case K_AUX14:		return 813;
	case K_AUX15:		return 814;
	case K_AUX16:		return 815;

	default:
		if (keynum == -1)			// module passed "no key"
			return keynum;
		if (keynum < 0)				// already a native negative code
			return -keynum;
		if (keynum >= 0 && keynum < 128)	// printable/control ascii - identity
			return keynum;
		return -keynum;			// no QC equivalent - native code
	}
}

int CSQC_Client_QCToKeynum (int code)
{
	switch (code)
	{
	case 9:			return K_TAB;
	case 13:		return K_ENTER;
	case 27:		return K_ESCAPE;
	case 32:		return K_SPACE;
	case 127:		return K_BACKSPACE;

	case 128:		return K_UPARROW;
	case 129:		return K_DOWNARROW;
	case 130:		return K_LEFTARROW;
	case 131:		return K_RIGHTARROW;

	case 132:		return K_LALT;
	case 133:		return K_LCTRL;
	case 134:		return K_LSHIFT;

	case 135:		return K_F1;
	case 136:		return K_F2;
	case 137:		return K_F3;
	case 138:		return K_F4;
	case 139:		return K_F5;
	case 140:		return K_F6;
	case 141:		return K_F7;
	case 142:		return K_F8;
	case 143:		return K_F9;
	case 144:		return K_F10;
	case 145:		return K_F11;
	case 146:		return K_F12;

	case 147:		return K_INS;
	case 148:		return K_DEL;
	case 149:		return K_PGDN;
	case 150:		return K_PGUP;
	case 151:		return K_HOME;
	case 152:		return K_END;
	case 153:		return K_PAUSE;

	case 154:		return KP_NUMLOCK;
	case 155:		return K_CAPSLOCK;
	case 156:		return K_SCRLCK;

	case 157:		return KP_INS;
	case 158:		return KP_END;
	case 159:		return KP_DOWNARROW;
	case 160:		return KP_PGDN;
	case 161:		return KP_LEFTARROW;
	case 162:		return KP_5;
	case 163:		return KP_RIGHTARROW;
	case 164:		return KP_HOME;
	case 165:		return KP_UPARROW;
	case 166:		return KP_PGUP;
	case 167:		return KP_DEL;
	case 168:		return KP_SLASH;
	case 169:		return KP_STAR;
	case 170:		return KP_MINUS;
	case 171:		return KP_PLUS;
	case 172:		return KP_ENTER;

	case 174:		return K_PRINTSCR;

	case 512:		return K_MOUSE1;
	case 513:		return K_MOUSE2;
	case 514:		return K_MOUSE3;
	case 515:		return K_MWHEELUP;
	case 516:		return K_MWHEELDOWN;
	case 517:		return K_MOUSE4;
	case 518:		return K_MOUSE5;
	case 519:		return K_MOUSE6;
	case 520:		return K_MOUSE7;
	case 521:		return K_MOUSE8;

	case 768:		return K_JOY1;
	case 769:		return K_JOY2;
	case 770:		return K_JOY3;
	case 771:		return K_JOY4;

	case 800:		return K_AUX1;
	case 801:		return K_AUX2;
	case 802:		return K_AUX3;
	case 803:		return K_AUX4;
	case 804:		return K_AUX5;
	case 805:		return K_AUX6;
	case 806:		return K_AUX7;
	case 807:		return K_AUX8;
	case 808:		return K_AUX9;
	case 809:		return K_AUX10;
	case 810:		return K_AUX11;
	case 811:		return K_AUX12;
	case 812:		return K_AUX13;
	case 813:		return K_AUX14;
	case 814:		return K_AUX15;
	case 815:		return K_AUX16;

	default:
		if (code == -1)				// module passed "no key"
			return code;
		if (code < 0)				// native code - back
			return -code;
		if (code >= 0 && code < 128)	// printable/control ascii - identity
			return code;
		return -code;				// no ezq equivalent
	}
}

/*
=================
CSQC_Client_PublishSimGlobals

Simulated module globals - frametime, cltime, maxclients, player_localnum,
intermission. Published each frame before CSQC_UpdateView. frametime - client time
delta (FTE cl.time-cl.lasttime); cltime - client map-uptime; maxclients - serverinfo
(QW key).
=================
*/
static void CSQC_Client_PublishSimGlobals (void)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (s_csqc.g_frametime >= 0)
		vm->globals[s_csqc.g_frametime] = cl.paused
			? 0 : (float)bound (0, cl.time - s_prev_cltime, 0.1);
	if (s_csqc.g_cltime >= 0)
		vm->globals[s_csqc.g_cltime] = (float)(cls.realtime - s_mapstarttime);
	if (s_csqc.g_maxclients >= 0)
	{
		const char *mc = Info_ValueForKey (cl.serverinfo, "maxclients");
		vm->globals[s_csqc.g_maxclients] = (float)((mc && mc[0]) ? atoi (mc) : 0);
	}
	if (s_csqc.g_player_localnum >= 0)
		vm->globals[s_csqc.g_player_localnum] = (cl.viewplayernum >= 0) ? cl.viewplayernum : 0;
	if (s_csqc.g_intermission >= 0)
		vm->globals[s_csqc.g_intermission] = cl.intermission ? 1 : 0;

	s_prev_cltime = cl.time;
}

/*
=================
CSQC_Client_Update

Called each 2D frame (HUD phase, cl_screen.c). WorldLoaded - once after entering the
world; then CSQC_UpdateView(vid.width, vid.height, notmenu). If the module is waiting
for the csprogs download - load it when a valid file appears and continue as on world
entry.
=================
*/
void CSQC_Client_Update (void)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (cls.state != ca_active)
		return;

	// Clip state (#324/325) - per frame (the module sets/clears it in its frame).
	s_clip_on = false;

	// Waiting for a downloaded csprogs (a valid file appeared -> load).
	if (s_csqc.csprogs_dl_pending)
	{
		char path[MAX_QPATH];

		if (CSQC_Client_ValidateFile (s_csqc.csprogs_dl_path,
			s_csqc.csprogs_size, s_csqc.csprogs_crc))
		{
			strlcpy (path, s_csqc.csprogs_dl_path, sizeof (path));
			s_csqc.csprogs_dl_pending = false;
			if (!CSQC_Client_Load (path))
			{
				CSQC_Client_NotifyCSQC (false);
				return;
			}
			if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
			{
				CSQC_Client_NotifyCSQC (false);
				return;
			}
			// as on world entry: per-map state is clean
			memset (s_csqc.seen, 0, sizeof (s_csqc.seen));
			s_csqc.world_done = false;
			s_csqc.enable_sent = false;
		}
		else
		{
			// Timeout by absence of progress (FTE has no limit of its own - it
			// relies on the general download machine; a flat 20 s from start gave up
			// on slow links even though the download was running).
			double now = Sys_DoubleTime ();
			qbool ours = cls.download
				&& !strcmp (cls.downloadname, s_csqc.csprogs_dl_localname);

			if (ours)
			{
				int pct = (int)cls.downloadpercent;

				if (pct > s_csqc.csprogs_dl_percent)
				{
					s_csqc.csprogs_dl_percent = pct;
					s_csqc.csprogs_dl_lastprogress = now;
				}
				s_csqc.csprogs_dl_started = true;
			}
			else if (s_csqc.csprogs_dl_started)
			{
				// our download finished without a valid file (failure/cancel)
				s_csqc.csprogs_dl_pending = false;
				Con_Printf ("CSQC: csprogs download failed\n");
				CSQC_Client_NotifyCSQC (false);
			}
			else if (now - s_csqc.csprogs_dl_lastprogress > 20)
			{
				// no progress for longer than the window (incl. the download never started)
				s_csqc.csprogs_dl_pending = false;
				Con_Printf ("CSQC: csprogs download timed out (no progress)\n");
				CSQC_Client_NotifyCSQC (false);
			}
			return;
		}
	}

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;

	if (!s_csqc.world_done)
	{
		s_csqc.world_done = true;
		if (!CSQC_Client_Exec (s_csqc.func_world))
			return;
		// FTE: enablecsqc - after CSQC_WorldLoaded of each map (module ready).
		CSQC_Client_NotifyCSQC (true);
	}

	// per-frame CSQC think-loop (StartFrame/thinks/EndFrame) - FTE CSQC_DrawView
	// before CSQC_UpdateView.
	CSQC_Client_RunFrameThink ();

	// player_localentnum - publish before the module (builtin environment as FTE;
	// player entities are not fabricated - see CSQC_Client_UpdateLocalEntnum).
	CSQC_Client_UpdateLocalEntnum ();
	// prediction window to the module (before CSQC_UpdateView).
	CSQC_Client_PatchFrames ();
	// view_angles to the module (FTE).
	CSQC_Client_PublishViewAngles ();
	// gamespeed to the module. QW/ezq has no cl.gamespeed -> 1; on server pause 0
	// (as FTE).
	if (s_csqc.global_gamespeed >= 0)
		s_csqc.vm.globals[s_csqc.global_gamespeed] = (cl.paused & PAUSED_SERVER) ? 0 : 1;
	// frametime/cltime/maxclients/player_localnum/intermission.
	CSQC_Client_PublishSimGlobals ();

	// #371 deltalisten: player_state/entity_state -> arena-edict bridge each frame
	// (FTE model: CL_LinkPlayers/CL_LinkPacketEntities per-frame). The module gets
	// the authoritative (no-lerp) state of players and delta entities.
	CSQC_Client_DeltaPlayers (vm);
	if (!s_csqc.errored)
		CSQC_Client_DeltaEntities (vm);

	if (s_csqc.func_update > 0)
	{
		// FTE #351 semantics: the listener applies only if the module set it this
		// frame (otherwise the engine view; reset before UpdateView).
		s_listener_on = false;
		vm->globals[OFS_PARM0] = vid.width;
		vm->globals[OFS_PARM1] = vid.height;
		vm->globals[OFS_PARM2] = CSQC_Client_NotMenu () ? 1 : 0;	// FTE notmenu
		CSQC_Client_Exec (s_csqc.func_update);
	}

	// With an active CSQC cursor, send the absolute mouse position to the module
	// only when it changed since last frame (as FTE: events on movement).
	{
		static float ie_abs_lastx = -1, ie_abs_lasty = -1;
		float cmx = 0, cmy = 0;

		CSQC_Client_GetCursorPos (&cmx, &cmy);
		if (CSQC_Client_CSQCCursor ())
		{
			if (cmx != ie_abs_lastx || cmy != ie_abs_lasty)
			{
				ie_abs_lastx = cmx;
				ie_abs_lasty = cmy;
				CSQC_Client_InputEvent (IE_MOUSEABS, cmx, cmy, 0);
			}
		}
		else
		{
			ie_abs_lastx = ie_abs_lasty = -1;	// cursor removed - reset
		}
	}
}

/*
=================
CSQC_Client_ParseAllowed

Runtime gate of the CSQC parsers: FTE_PEXT_CSQC agreed and cl_pext_csqc enabled - the
same criterion as cl_parse.c for case 83/90. The gate in a function (not only in the
case) also covers "module not loaded / errored", see ParseEntities.
=================
*/
qbool CSQC_Client_ParseAllowed (void)
{
#ifdef FTE_PEXT_CSQC
	extern cvar_t cl_pext_csqc;
	return cl_pext_csqc.value && (cls.fteprotocolextensions & FTE_PEXT_CSQC);
#else
	return false;
#endif
}

/*
=================
CSQC_Client_MayRead

The module's read*-builtins are allowed only inside parse callbacks (CSQC_Ent_Update
/ CSQC_Parse_Event) - FTE csqc_mayread parity. Outside them - CSQC_Client_Abort (FTE
CSQC_Abort parity).
=================
*/
qbool CSQC_Client_MayRead (void)
{
	return s_csqc.mayread;
}

/*
=================
CSQC_Client_ParsePrint

CSQC_Parse_Print(string, float) - intercept of the network svc_print (chat and
ordinary). FTE: if the callback exists => the engine does not print its own (the
module decides whether to forward to #339 print). Returns whether the callback was
called.
=================
*/
qbool CSQC_Client_ParsePrint (const char *msg, int level)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parseprint <= 0)
		return false;

	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM0], (char *)(msg ? msg : ""));
	vm->globals[OFS_PARM1] = level;
	CSQC_Client_Exec (s_csqc.func_parseprint);
	return true;
}

/*
=================
CSQC_Client_ParseCenterPrint

CSQC_Parse_CenterPrint(string) - intercept of svc_centerprint/svc_finale. FTE: a
module return != 0 => the engine ignores the centerprint. Returns whether to
suppress the engine output.
=================
*/
qbool CSQC_Client_ParseCenterPrint (const char *msg)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parsecp <= 0)
		return false;

	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM0], (char *)(msg ? msg : ""));
	CSQC_Client_ExecRet (s_csqc.func_parsecp, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_ParseDamage

CSQC_Parse_Damage(float save, float take, vector inflictororg) - parse svc_damage
(V_ParseDamage). FTE: PARM0=save(dmg_save), PARM1=take(dmg_take), PARM2=source
vector; return !=0 => fully suppress the color shift/view kick. Returns whether to
suppress the engine damage effects.
=================
*/
qbool CSQC_Client_ParseDamage (float save, float take, const vec3_t source)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parsedamage <= 0)
		return false;

	vm->globals[OFS_PARM0] = save;
	vm->globals[OFS_PARM1] = take;
	vm->globals[OFS_PARM2 + 0] = source[0];
	vm->globals[OFS_PARM2 + 1] = source[1];
	vm->globals[OFS_PARM2 + 2] = source[2];
	CSQC_Client_ExecRet (s_csqc.func_parsedamage, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_EventSound

CSQC_Event_Sound(entnum, channel, soundname, vol, attenuation, pos, pitchmod, flags) -
parse svc_sound (CL_ParseStartSoundPacket / NQD_ParseStartSoundPacket). FTE:
PARM0=entnum..PARM5=pos, PARM6=pitchmod*100, PARM7=flags; self = csqc entity by
number or world. Return !=0 => the engine does not play the sound. self is set and
not restored (FTE parity).
=================
*/
qbool CSQC_Client_EventSound (int entnum, int channel, const char *name, float vol,
							  float atten, const vec3_t pos, float pitchmod, float flags)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;
	int slot;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_eventsound <= 0)
		return false;

	// self = arena entity by number or 0 (world).
	slot = CSQC_Client_NumToSlot (entnum);
	if (slot > 0 && slot < CSQC_MAX_EDICTS && CSQC_Client_EntUsed (slot))
		CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)entnum);
	else if (s_csqc.global_self >= 0)
		*(int *)&vm->globals[s_csqc.global_self] = 0;

	vm->globals[OFS_PARM0] = (float)entnum;
	vm->globals[OFS_PARM1] = (float)channel;
	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM2], (char *)(name ? name : ""));
	vm->globals[OFS_PARM3] = vol;
	vm->globals[OFS_PARM4] = atten;
	vm->globals[OFS_PARM5 + 0] = pos[0];
	vm->globals[OFS_PARM5 + 1] = pos[1];
	vm->globals[OFS_PARM5 + 2] = pos[2];
	vm->globals[OFS_PARM6] = pitchmod * 100.0f;
	vm->globals[OFS_PARM7] = flags;

	CSQC_Client_ExecRet (s_csqc.func_eventsound, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_ParseSetAngles

CSQC_Parse_SetAngles(vector angles, float isdelta) - parse svc_setangle (live/QW demo,
NQ demo). FTE: PARM0+0..2 = angle vector (3 words), PARM1 = isdelta; return !=0 => the
engine does not apply its own angle (the MVD DPB_MVD branch does not call the hook).
Returns whether to suppress the engine angle application.
=================
*/
qbool CSQC_Client_ParseSetAngles (const float *angles, float isdelta)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parsesetangles <= 0)
		return false;

	vm->globals[OFS_PARM0 + 0] = angles[0];
	vm->globals[OFS_PARM0 + 1] = angles[1];
	vm->globals[OFS_PARM0 + 2] = angles[2];
	vm->globals[OFS_PARM1] = isdelta;
	CSQC_Client_ExecRet (s_csqc.func_parsesetangles, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_RendererRestarted

CSQC_RendererRestarted(string rendererdescription) - engine callback on renderer
reinit (vid_restart/vid_reload, VID_Startup) and on module load (CSQC_Client_Load,
after CSQC_Init). FTE: PARM0 = renderer description string; the return is not read
(there is no suppress semantics).

The string is persistent (unlike the temp callbacks): the module may store it in a
global, while the PR1VM_ClientSetString ring overwrites slots. So keep our own
reusable copy and register it in strtbl directly (PR1VM_SetString) - the offset is
stable between calls, and a GL pointer (glGetString) is not reused after vid_restart.
=================
*/
static char s_rr_desc[256];

void CSQC_Client_RendererRestarted (const char *desc)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_rr <= 0)
		return;

	strlcpy (s_rr_desc, desc ? desc : "", sizeof (s_rr_desc));
	PR1VM_SetString (vm, (string_t *)&vm->globals[OFS_PARM0], s_rr_desc);
	CSQC_Client_Exec (s_csqc.func_rr);
}

/*
=================
CSQC_Client_ParseEntities

Parsing of svc_fte_csqcentities(76)/sized(92): for each entity - a short entnum,
bit 0x8000 = remove, 0 = end. Update: CSQC_Ent_Update(isnew) - the module reads the
payload from the current message (read*); the entity context (self/.entnum) is set
by the engine before the call. Remove: CSQC_Ent_Remove with self/.entnum (no builtin
stream). Sized (92, only mvdsv under sv_csqcdebug): before each update entity's
payload there is a short length - a skip guard against desync.
=================
*/
/*
=================
CSQC_Client_SizedRewind

FTE sized-message guard (pr_csqc.c:9696-9713): after handling a payload, align
msg_readcount to the declared end payload_start+payload_len, for both under-read
(pad) and over-read (rewind). MSG_ReadSkip is forward-only in ezq, so over-read is
rewound by writing msg_readcount directly.
=================
*/
void CSQC_Client_SizedRewind (int payload_start, int payload_len)
{
	int used = msg_readcount - payload_start;

	if (used < payload_len)
		MSG_ReadSkip (payload_len - used);
	else if (used > payload_len)
		msg_readcount = payload_start + payload_len;
}

/*
=================
CSQC_Client_ParsePrecacheMsg

FTE svcfte_precache (77): [short idx|type][string name]. Register the index->name
mapping without downloading (model/sound); particle/unused are drained.
=================
*/
void CSQC_Client_ParsePrecacheMsg (void)
{
	int code = (unsigned short)MSG_ReadShort ();
	const char *name = MSG_ReadString ();
	int ptype = code & SVCFTE_PC_TYPE;
	int pidx = code & ~SVCFTE_PC_TYPE;

	if (!name)
		return;

	if (ptype == SVCFTE_PC_MODEL)
	{
		if (pidx >= 1 && pidx < MAX_MODELS)
			strlcpy (cl.model_name[pidx], name, sizeof (cl.model_name[pidx]));
	}
	else if (ptype == SVCFTE_PC_SOUND)
	{
		if (pidx >= 1 && pidx < MAX_SOUNDS)
			strlcpy (cl.sound_name[pidx], name, sizeof (cl.sound_name[pidx]));
	}
}

/*
=================
CSQC_Client_DrainTrailMsg

FTE svcfte_trailparticles (80): [ent short][short effect][coord x6]. Drained only
(no visual). Entity is read as a short: ezq does not negotiate
PEXT2_REPLACEMENTDELTAS, so FTE MSGCL_ReadEntity uses the plain short form.
=================
*/
void CSQC_Client_DrainTrailMsg (void)
{
	int i;

	MSG_ReadShort ();				// entity number
	MSG_ReadShort ();				// effect index
	for (i = 0; i < 6; i++)
		MSG_ReadCoord ();
}

/*
=================
CSQC_Client_DrainPointMsg

FTE svcfte_pointparticles (81) / pointparticles1 (82). 81: [short effect][coord x6]
[short count]; 82 (compact): [short effect][coord x3].
=================
*/
void CSQC_Client_DrainPointMsg (qbool compact)
{
	int i;

	MSG_ReadShort ();				// effect index
	for (i = 0; i < (compact ? 3 : 6); i++)
		MSG_ReadCoord ();
	if (!compact)
		MSG_ReadShort ();			// count
}

/*
=================
CSQC_Client_DrainTempEntSizedMsg

FTE svcfte_temp_entity_sized (91): [short len][payload]. Drained by length with the
sized guard (no module temp-entity parse in this change).
=================
*/
void CSQC_Client_DrainTempEntSizedMsg (void)
{
	int payload_len = MSG_ReadShort ();
	int payload_start = msg_readcount;

	CSQC_Client_SizedRewind (payload_start, payload_len);
}

#ifdef CSQC_DEBUG
static void CSQC_Client_NetProbe_f (void);

static void CSQC_NetProbe_WriteShort (byte *buf, int *n, int v)
{
	buf[(*n)++] = (byte)(v & 0xff);
	buf[(*n)++] = (byte)((v >> 8) & 0xff);
}

/*
=================
CSQC_Client_NetProbe_f

Debug canary (client console `csqc_netprobe`): feeds synthetic wire bodies through
the same receive helpers cl_parse.c uses for the FTE-CSQC messages absent from
qwprot (77/80/81/82/91) and the sized entities drain (92), and checks exact byte
consumption / no badread. No server or module required.
=================
*/
#define NETPROBE_RUN(label, call, expect) \
	do { \
		sizebuf_t _s = net_message; \
		int _rc = msg_readcount; \
		qbool _bad = msg_badread; \
		net_message.data = buf; \
		net_message.cursize = n; \
		msg_readcount = 0; \
		msg_badread = false; \
		call; \
		PR1VM_GuardCheck (label, !msg_badread && msg_readcount == (expect), &pass, &fail); \
		net_message = _s; \
		msg_readcount = _rc; \
		msg_badread = _bad; \
	} while (0)

static void CSQC_Client_NetProbe_f (void)
{
	int pass = 0, fail = 0;
	byte buf[128];
	int n;

	// 77 precache: [short idx|type][string], model index 5.
	{
		const char *nm = "progs/test.mdl";
		n = 0;
		CSQC_NetProbe_WriteShort (buf, &n, 5 | SVCFTE_PC_MODEL);
		memcpy (buf + n, nm, strlen (nm) + 1);
		n += (int)strlen (nm) + 1;
		NETPROBE_RUN ("netprobe 77 precache", CSQC_Client_ParsePrecacheMsg (), n);
	}
	// 80 trail: [ent short][short effect][coord x6].
	{
		int i;
		n = 0;
		CSQC_NetProbe_WriteShort (buf, &n, 3);
		CSQC_NetProbe_WriteShort (buf, &n, 7);
		for (i = 0; i < 6; i++)
			CSQC_NetProbe_WriteShort (buf, &n, 100 + i);
		NETPROBE_RUN ("netprobe 80 trail", CSQC_Client_DrainTrailMsg (), n);
	}
	// 81 point (full): [short effect][coord x6][short count].
	{
		int i;
		n = 0;
		CSQC_NetProbe_WriteShort (buf, &n, 7);
		for (i = 0; i < 6; i++)
			CSQC_NetProbe_WriteShort (buf, &n, 200 + i);
		CSQC_NetProbe_WriteShort (buf, &n, 4);
		NETPROBE_RUN ("netprobe 81 point", CSQC_Client_DrainPointMsg (false), n);
	}
	// 82 point1 (compact): [short effect][coord x3].
	{
		int i;
		n = 0;
		CSQC_NetProbe_WriteShort (buf, &n, 7);
		for (i = 0; i < 3; i++)
			CSQC_NetProbe_WriteShort (buf, &n, 300 + i);
		NETPROBE_RUN ("netprobe 82 point1", CSQC_Client_DrainPointMsg (true), n);
	}
	// 91 temp_entity_sized: [short len][payload len].
	{
		n = 0;
		CSQC_NetProbe_WriteShort (buf, &n, 4);
		buf[n++] = 1; buf[n++] = 2; buf[n++] = 3; buf[n++] = 4;
		NETPROBE_RUN ("netprobe 91 tent-sized", CSQC_Client_DrainTempEntSizedMsg (), n);
	}
	// 91 sized guard, over-read: module-less path drains exactly len.
	{
		n = 0;
		CSQC_NetProbe_WriteShort (buf, &n, 2);
		buf[n++] = 9; buf[n++] = 9;
		NETPROBE_RUN ("netprobe 91 sized-guard", CSQC_Client_DrainTempEntSizedMsg (), n);
	}
	// 92 csqcentities_sized (walked without module): [ent][len][payload]...[0].
	{
		n = 0;
		CSQC_NetProbe_WriteShort (buf, &n, 1);
		CSQC_NetProbe_WriteShort (buf, &n, 3);
		buf[n++] = 1; buf[n++] = 2; buf[n++] = 3;
		CSQC_NetProbe_WriteShort (buf, &n, 0);
		NETPROBE_RUN ("netprobe 92 entities", CSQC_Client_ParseEntities (true), n);
	}

	Con_Printf ("[CSQC-NETPROBE] pass=%d fail=%d\n", pass, fail);
}

#undef NETPROBE_RUN
#endif // CSQC_DEBUG

void CSQC_Client_ParseEntities (qbool sized)
{
	pr1vm_t *vm = &s_csqc.vm;
	unsigned int entnum;
	qbool removeflag;
	qbool ready;

	// Runtime gate (as cl_parse.c case 83/90) + a live module. Without the gate or
	// module, 76/92 are not treated as CSQC. A sized stream can be walked without a
	// module (only the wire fields entnum/len are needed); a non-sized one cannot -
	// the payload length is unknown without executing the module, so this is a
	// protocol error (as FTE Host_EndGame), not a silent desync.
	ready = CSQC_Client_ParseAllowed ()
		&& s_csqc.loaded && s_csqc.inited && !s_csqc.errored
		&& (s_csqc.func_entupdate > 0 || s_csqc.func_entremove > 0);
	if (!ready && !sized)
	{
		Host_Error ("CSQC_Client_ParseEntities: svc_fte_csqcentities without CSQC\n");
		return;
	}

	for (;;)
	{
		int payload_start = 0;
		int payload_len = -1;

		entnum = (unsigned short)MSG_ReadShort ();
		removeflag = !!(entnum & 0x8000);
		entnum &= ~0x8000u;
		if ((!entnum && !removeflag) || msg_badread)
			break;
		if (entnum >= (unsigned int)(sizeof (s_csqc.seen) / sizeof (s_csqc.seen[0])))
		{
			// Dynamic growth is NOT implemented - the number->slot map is fixed
			// (CSQC_MAX_NUM/CSQC_MAX_EDICTS = 4096). A number >= 4096 is unreachable
			// from mvdsv (MAX_EDICTS=2048), this path is purely defensive; the rest
			// of the datagram is not drained - a documented limitation (FTE
			// `CSQC_EntityCheck` grows csqcent[]).
			break;
		}

		if (removeflag)
		{
			int slot;

			// remove-0 (world) is unconditionally fatal (FTE). Host_Error (message +
			// Host_Abort).
			if (!entnum)
				Host_Error ("CSQC_Client_ParseEntities: cannot remove world\n");

			slot = CSQC_Client_NumToSlot ((int)entnum);
			if (slot)
			{
				if (ready && s_csqc.func_entremove > 0)
				{
					// context (self=slot, .entnum=number), without a builtin stream.
					CSQC_Client_SetContextSlot (vm, (unsigned)slot, entnum);
					CSQC_Client_Exec (s_csqc.func_entremove);
				}
				// The engine frees the slot unconditionally (the callback is optional).
				// ezq deviation from FTE: when a callback exists FTE hands the free to
				// the module - we do not change that.
				CSQC_Client_NetFreeSlot (slot, (int)entnum);
			}
			s_csqc.seen[entnum] = false;
			continue;
		}

		// Update. Sized: [len short][payload]. payload_start is AFTER the length
		// (otherwise used includes the 2 length bytes and skip falls short by 2; FTE
		// takes packetstart after ReadShort).
		if (sized)
		{
			payload_len = MSG_ReadShort ();
			payload_start = msg_readcount;
		}

		if (!ready || s_csqc.func_entupdate <= 0)
		{
			// No update module/callback: read out the payload so the stream is not
			// desynced. Non-sized does not know the length - fatal (see above).
			if (sized && payload_len > 0)
				MSG_ReadSkip (payload_len);
			else if (!sized)
				Host_Error ("CSQC_Client_ParseEntities: update without CSQC\n");
			continue;
		}

		vm->globals[OFS_PARM0] = s_csqc.seen[entnum] ? 0 : 1;
		s_csqc.seen[entnum] = true;

		// number->slot; a new number gets a pool slot or CSQC_Ent_Spawn, context
		// (self=slot, .entnum=number).
		{
			int slot = CSQC_Client_NumToSlot ((int)entnum);
			if (!slot)
			{
				if (s_csqc.func_entspawn > 0)
				{
					// The module creates/configures the entity itself; an invalid self
					// (0/world) -> no slot (as FTE ent=NULL).
					slot = CSQC_Client_RunEntSpawn (vm, entnum);
					if (slot)
						CSQC_Client_MapNumber ((int)entnum, slot);
				}
				else
				{
					slot = CSQC_Client_NetAllocSlot (vm);
					if (!slot)
					{
						Con_Printf ("CSQC: pool full, entity %u dropped\n", entnum);
						break;	// pathological (pool 4095); desync impossible while reading
					}
					CSQC_Client_MapNumber ((int)entnum, slot);
					// number -> pool slot diagnostic; printing is limited so server
					// remove/update churn does not flood the console (<=32 lines per
					// session at csqc_dbg>=3).
					{
						static int s_dbg_lines = 0;
						cvar_t *dbg = s_csqc.csqc_dbg_cvar;	// cached (resolved after CSQC_Init)
						if (dbg && dbg->value >= 3)
						{
							if (s_dbg_lines < 32)
							{
								Con_Printf ("CSQC ent num %u -> slot %d\n", entnum, slot);
								s_dbg_lines++;
							}
						}
						else
							s_dbg_lines = 0;
					}
				}
			}
			if (slot)
				CSQC_Client_SetContextSlot (vm, (unsigned)slot, entnum);
			else if (s_csqc.global_self >= 0)
				*(int *)&vm->globals[s_csqc.global_self] = 0;	// FTE: self = NULL/world
		}

		s_csqc.mayread = true;	// read* context of the module (FTE csqc_mayread parity)
		CSQC_Client_Exec (s_csqc.func_entupdate);
		s_csqc.mayread = false;

		if (s_csqc.errored)
		{
			// Module crashed mid-message - cannot leave, or the rest of the list will
			// be parsed as svc opcodes. Non-sized (76) does not know the payload
			// length -> resync impossible, fatal (FTE Host_EndGame). Sized (92): read
			// out the current payload and then drain the rest.
			if (!sized)
				Host_Error ("CSQC_Client_ParseEntities: update module error\n");
			if (payload_len >= 0)
				CSQC_Client_SizedRewind (payload_start, payload_len);
			ready = false;
			continue;
		}

		// A Spawn module may change self in Update - remap number->slot onto the new
		// valid slot (0 = removed/world).
		CSQC_Client_RemapAfterUpdate (vm, entnum);

		// Size guard: align to payload_start+payload_len (rewind over-read / pad under-read).
		if (payload_len >= 0)
			CSQC_Client_SizedRewind (payload_start, payload_len);
	}
}

/*
=================
CSQC_Client_ParseEvent

Parsing of svc_fte_cgamepacket(83): the module itself reads the event name and
payload (CSQC_Parse_Event) via read*-builtins from the current message. Guard as in
ParseEntities - without a module, a foreign CSQC multicast (echo) does not crash the
client.
=================
*/
void CSQC_Client_ParseEvent (qbool sized)
{
	qbool ready = CSQC_Client_ParseAllowed ()
		&& s_csqc.loaded && s_csqc.inited && !s_csqc.errored
		&& s_csqc.func_parseevent > 0;

	// FTE CSQC_ParseGamePacket: the sized-stream caller (cl_parse.c case 90) drains
	// by length itself - here it is enough to return; non-sized (case 83) has no
	// length, so without a module/callback this is a protocol error (FTE
	// Host_EndGame) - otherwise the rest of the payload would be parsed as svc
	// opcodes and misparse.
	if (!ready)
	{
		if (!sized)
			Host_Error ("CSQC_Client_ParseEvent: cgamepacket without CSQC\n");
		return;
	}
	s_csqc.mayread = true;	// read* context of the module (FTE csqc_mayread parity)
	CSQC_Client_Exec (s_csqc.func_parseevent);
	s_csqc.mayread = false;
	// The module may have crashed mid-payload. Sized - caller drains by length;
	// non-sized does not know the length -> fatal (as FTE Host_EndGame).
	if (s_csqc.errored && !sized)
		Host_Error ("CSQC_Client_ParseEvent: cgamepacket module error\n");
}

/*
=================
CSQC_Client_InputFrame

CSQC_Input_Frame: called before each usercmd is sent (CL_SendCmd, cl_input.c). FTE
mechanics (CSQC_Input_Frame + cs_set/get_input_state) on the QW set of module-declared
input_* globals (input_sequence/timelength/angles/movevalues/buttons/impulse): the
engine fills them from cmd, runs CSQC_Input_Frame, then writes the changes back into
cmd.

Differences from FTE:
- usercmd.angles in ezquake is float degrees (not short), conversion done by
  MSG_WriteAngle16 in MSG_WriteDeltaUsercmd - copied directly here;
- input_timelength is multiplied by gamespeed; ezq has no cl.gamespeed -> 1 (0 on
  server pause), i.e. in QW behavior this is a no-op;
- the FTE globals lightlevel/weapon/servertime/clienttime/cursor/VR and the
  joy/accel/focus InputEvent types (CSIE_*) are not declared in the QW module - N/A.
=================
*/
void CSQC_Client_InputFrame (usercmd_t *cmd)
{
	pr1vm_t *vm = &s_csqc.vm;

	// live clientcommandframe = seq of the current built cmd (FTE cl.movesequence).
	// Set before the guard - track even without a module; the render phase
	// (PatchFrames) then returns this same value, not the incremented
	// outgoing_sequence.
	s_ccframe = (unsigned int)cls.netchan.outgoing_sequence;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored || s_csqc.func_input <= 0)
		return;

	// prediction window before CSQC_Input_Frame (clientcommandframe = current cmd).
	CSQC_Client_PatchFrames ();

	CSQC_Client_SetTime ();

	// input_sequence = seq of the current cmd (FTE cs_set_input_state); same
	// numbering as clientcommandframe.
	if (s_csqc.in_sequence >= 0)
		vm->globals[s_csqc.in_sequence] = CSQC_Client_ClientCmdFrame ();

	// cmd -> input_* globals (only those declared by the module).
	// input_timelength = msec/1000 * gamespeed; ezq gamespeed is 1 (0 on server
	// pause) - see CSQC_Client_Update.
	if (s_csqc.in_timelength >= 0)
		vm->globals[s_csqc.in_timelength] = cmd->msec / 1000.0f
			* ((cl.paused & PAUSED_SERVER) ? 0.0f : 1.0f);
	if (s_csqc.in_angles >= 0)
	{
		vm->globals[s_csqc.in_angles + 0] = cmd->angles[0];
		vm->globals[s_csqc.in_angles + 1] = cmd->angles[1];
		vm->globals[s_csqc.in_angles + 2] = cmd->angles[2];
	}
	if (s_csqc.in_movevalues >= 0)
	{
		vm->globals[s_csqc.in_movevalues + 0] = cmd->forwardmove;
		vm->globals[s_csqc.in_movevalues + 1] = cmd->sidemove;
		vm->globals[s_csqc.in_movevalues + 2] = cmd->upmove;
	}
	if (s_csqc.in_buttons >= 0)
		vm->globals[s_csqc.in_buttons] = cmd->buttons;
	if (s_csqc.in_impulse >= 0)
		vm->globals[s_csqc.in_impulse] = cmd->impulse;

	if (!CSQC_Client_Exec (s_csqc.func_input))
		return;		// errored - frames disabled, cmd untouched

	// input_* globals -> cmd (write only what the module changed).
	if (s_csqc.in_timelength >= 0)
	{
		int msec = (int)(vm->globals[s_csqc.in_timelength] * 1000.0f);
		if (msec < 1)
			msec = 1;
		else if (msec > 255)
			msec = 255;
		cmd->msec = (byte)msec;
	}
	if (s_csqc.in_angles >= 0)
	{
		cmd->angles[0] = vm->globals[s_csqc.in_angles + 0];
		cmd->angles[1] = vm->globals[s_csqc.in_angles + 1];
		cmd->angles[2] = vm->globals[s_csqc.in_angles + 2];
	}
	if (s_csqc.in_movevalues >= 0)
	{
		cmd->forwardmove = (short)vm->globals[s_csqc.in_movevalues + 0];
		cmd->sidemove = (short)vm->globals[s_csqc.in_movevalues + 1];
		cmd->upmove = (short)vm->globals[s_csqc.in_movevalues + 2];
	}
	if (s_csqc.in_buttons >= 0)
		cmd->buttons = (byte)vm->globals[s_csqc.in_buttons];
	if (s_csqc.in_impulse >= 0)
		cmd->impulse = (byte)vm->globals[s_csqc.in_impulse];
}

/*
=================
CSQC_Client_RecordInput / CSQC_Client_ApplyInput

#345: history of sent usercmds. CL_SendCmd records each sent cmd
(CSQC_Client_RecordInput); builtin #345(seq) asks for it and fills the input_*
globals (CSQC_Client_ApplyInput).

seq = mirror of cls.netchan.outgoing_sequence (the client message number at write
time; Netchan_Transmit increments after writing the header). This is the number the
server acknowledges (servercommandframe = incoming_acknowledged = cl.parsecount) -
the window (servercommandframe, clientcommandframe] is consistent in one numbering.
Difference from FTE: here a ring history (64) + records only the live CL_SendCmd
path (demos/MVD are not recorded). clientcommandframe = s_ccframe = the last
recorded seq, so #345(clientcommandframe) hits the ring (the live pending frame is
available outside CSQC_Input_Frame, as FTE movesequence). The NQ ackedmovesequence
mechanism (PEXT2_PREDINFO) is unreachable (not for QW).
=================
*/
void CSQC_Client_RecordInput (usercmd_t *cmd)
{
	unsigned int seq;

	seq = (unsigned int)cls.netchan.outgoing_sequence;
	s_last_seq = seq;
	s_inhist[seq % CSQC_INHIST].seq = seq;
	s_inhist[seq % CSQC_INHIST].cmd = *cmd;
	if (s_csqc.loaded && !s_csqc.errored)
	{
		if (s_csqc.in_sequence >= 0)
			s_csqc.vm.globals[s_csqc.in_sequence] = seq;
		if (s_csqc.g_ccframe >= 0)
			s_csqc.vm.globals[s_csqc.g_ccframe] = seq;
	}
}

static void CSQC_Client_FillInputFromCmd (usercmd_t *cmd)
{
	pr1vm_t *vm = &s_csqc.vm;

	// x gamespeed as in Input_Frame / FTE cs_set_input_state; ezq gamespeed is 1
	// (0 on server pause) - a QW no-op.
	if (s_csqc.in_timelength >= 0)
		vm->globals[s_csqc.in_timelength] = cmd->msec / 1000.0f
			* ((cl.paused & PAUSED_SERVER) ? 0.0f : 1.0f);
	if (s_csqc.in_angles >= 0)
	{
		vm->globals[s_csqc.in_angles + 0] = cmd->angles[0];
		vm->globals[s_csqc.in_angles + 1] = cmd->angles[1];
		vm->globals[s_csqc.in_angles + 2] = cmd->angles[2];
	}
	if (s_csqc.in_movevalues >= 0)
	{
		vm->globals[s_csqc.in_movevalues + 0] = cmd->forwardmove;
		vm->globals[s_csqc.in_movevalues + 1] = cmd->sidemove;
		vm->globals[s_csqc.in_movevalues + 2] = cmd->upmove;
	}
	if (s_csqc.in_buttons >= 0)
		vm->globals[s_csqc.in_buttons] = cmd->buttons;
	if (s_csqc.in_impulse >= 0)
		vm->globals[s_csqc.in_impulse] = cmd->impulse;
}

int CSQC_Client_ApplyInput (unsigned int seq)
{
	unsigned int i;
	csqc_inrec_t *r;

	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (!seq)
		return 0;
	// paused guard as FTE - on server pause the window frames are not applied. The
	// range (servercommandframe, clientcommandframe] is not checked by the engine
	// (module contract; see ext_csqc_1 spec) - here only ring liveness.
	if ((cl.paused & PAUSED_SERVER) && seq >= (unsigned)CSQC_Client_ServerCmdFrame ())
		return 0;
	for (i = 0; i < CSQC_INHIST; i++)
	{
		r = &s_inhist[i];
		if (r->seq == seq)
		{
			CSQC_Client_FillInputFromCmd (&r->cmd);
			if (s_csqc.in_sequence >= 0)
				s_csqc.vm.globals[s_csqc.in_sequence] = seq;
			return 1;
		}
	}
	return 0;
}

/*
=================
CSQC_VectorAngles

Port of FTE VectorAngles with optional up->roll, meshpitch=false (r_meshpitch/
r_meshroll are not applied - the same deviation as #51 vectoangles). forward is the
direction; up may be NULL; result[3] = (pitch, yaw, roll). Shared helper for #51
(csqc_builtins.c) and #638 CL_RotateMoves.
=================
*/
void CSQC_VectorAngles (const float *forward, const float *up, float *result)
{
	float yaw, pitch, roll;

	if (forward[1] == 0 && forward[0] == 0)
	{
		if (forward[2] > 0)
		{
			pitch = -M_PI * 0.5f;
			yaw = up ? (float)atan2 (-up[1], -up[0]) : 0;
		}
		else
		{
			pitch = M_PI * 0.5f;
			yaw = up ? (float)atan2 (up[1], up[0]) : 0;
		}
		roll = 0;
	}
	else
	{
		float cp, sp, cy, sy;
		yaw = (float)atan2 (forward[1], forward[0]);
		pitch = -(float)atan2 (forward[2], sqrt (forward[0] * forward[0] + forward[1] * forward[1]));
		if (up)
		{
			float tleft[3], tup[3];
			cp = (float)cos (pitch); sp = (float)sin (pitch);
			cy = (float)cos (yaw); sy = (float)sin (yaw);
			tleft[0] = -sy; tleft[1] = cy; tleft[2] = 0;
			tup[0] = sp * cy; tup[1] = sp * sy; tup[2] = cp;
			roll = -(float)atan2 (up[0] * tleft[0] + up[1] * tleft[1] + up[2] * tleft[2],
				up[0] * tup[0] + up[1] * tup[1] + up[2] * tup[2]);
		}
		else
			roll = 0;
	}
	pitch *= (float)(180 / M_PI);
	yaw *= (float)(180 / M_PI);
	roll *= (float)(180 / M_PI);
	if (pitch < 0) pitch += 360;
	if (yaw < 0) yaw += 360;
	if (roll < 0) roll += 360;
	result[0] = pitch; result[1] = yaw; result[2] = roll;
}

/*
CSQC_VectorTransform - port of FTE VectorTransform for a matrix3x4 without
translation (in #638 the 4th matrix column = 0).
*/
static void CSQC_VectorTransform (const float *in, float mat[3][4], float *out)
{
	out[0] = DotProduct (in, mat[0]) + mat[0][3];
	out[1] = DotProduct (in, mat[1]) + mat[1][3];
	out[2] = DotProduct (in, mat[2]) + mat[2][3];
}

/*
=================
CSQC_Client_RotateMoves

#638 CL_RotateMoves (FTE PF_cl_RotateMoves): rotate the angles of sent but not yet
acked usercmds (seq > servercommandframe) by the anglechange delta, order as FTE:
AngleVectorsFLU(anglechange) -> frame forward/up -> VectorTransform -> VectorAngles.
usercmd.angles in ezq is float degrees, so no SHORT2ANGLE/ANGLE2SHORT (in FTE
cmd.angles is short). Returns 0 on an invalid seat (single-seat: only 0 is valid).
=================
*/
int CSQC_Client_RotateMoves (float *anglechange, int seat)
{
	int i;
	float mat[3][4];
	vec3_t of, ou, nf, nu, a;
	unsigned int ack;

	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (seat != 0)
		return 0;

	AngleVectorsFLU (anglechange, mat[0], mat[1], mat[2]);
	mat[0][3] = mat[1][3] = mat[2][3] = 0;

	ack = (unsigned int)CSQC_Client_ServerCmdFrame ();
	for (i = 0; i < CSQC_INHIST; i++)
	{
		csqc_inrec_t *r = &s_inhist[i];
		if (!r->seq || r->seq <= ack)
			continue;			// empty and acked slots are untouched

		VectorCopy (r->cmd.angles, a);
		AngleVectors (a, of, NULL, ou);
		CSQC_VectorTransform (of, mat, nf);
		CSQC_VectorTransform (ou, mat, nu);
		CSQC_VectorAngles (nf, nu, a);
		VectorCopy (a, r->cmd.angles);
	}
	return 1;
}

/*
=================
CSQC_Client_MakeVectors

#1 makevectors (FTE PF_cs_makevectors parity): from an angle vector write the
module's v_forward/v_right/v_up (globals, resolved in Load). If the module did not
declare them - no-op (offset -1).
=================
*/
void CSQC_Client_MakeVectors (float *ang)
{
	pr1vm_t *vm = &s_csqc.vm;
	float *f, *r, *u;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.g_vfwd < 0 || s_csqc.g_vright < 0 || s_csqc.g_vup < 0)
		return;		// module did not declare v_forward/v_right/v_up
	f = &vm->globals[s_csqc.g_vfwd];
	r = &vm->globals[s_csqc.g_vright];
	u = &vm->globals[s_csqc.g_vup];
	AngleVectors (ang, f, r, u);
}

/*
=================
CSQC_Client_VectorVectors

#432 vectorvectors (FTE PF_vectorvectors parity): normalize the given direction into
the module's v_forward and build orthogonal v_right/v_up via FTE VVPerpendicularVector
+ CrossProduct. Do not use ezq PerpendicularVector - it has a different edge case for
(0,0,z). Module without the globals - no-op.
=================
*/
void CSQC_Client_VectorVectors (float *dir)
{
	pr1vm_t *vm = &s_csqc.vm;
	float *f, *r, *u;
	float right[3];

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.g_vfwd < 0 || s_csqc.g_vright < 0 || s_csqc.g_vup < 0)
		return;		// module did not declare v_forward/v_right/v_up
	f = &vm->globals[s_csqc.g_vfwd];
	r = &vm->globals[s_csqc.g_vright];
	u = &vm->globals[s_csqc.g_vup];

	VectorCopy (dir, f);
	VectorNormalize (f);

	if (!f[0] && !f[1])
	{
		right[0] = 0;
		right[1] = f[2] ? -1 : 0;
		right[2] = 0;
	}
	else
	{
		right[0] = f[1];
		right[1] = -f[0];
		right[2] = 0;
		VectorNormalize (right);
	}
	VectorCopy (right, r);
	CrossProduct (right, f, u);
}

/*
=================
CSQC_Client_HasInputEvent / CSQC_Client_InputEvent

Delivery of input events to the module (CSQC_InputEvent). Called from keys.c
(keys/clicks/wheel at key_dest == key_game) and in_sdl2.c (mouse: MOUSEDELTA in the
ordinary mode; MOUSEABS - from CSQC_Client_Update at CSQCCursor). A module return
!= 0 means "event handled" (the engine does not apply it).
=================
*/
qbool CSQC_Client_HasInputEvent (void)
{
	return s_csqc.loaded && s_csqc.inited && !s_csqc.errored
		&& s_csqc.func_inputevent > 0;
}

int CSQC_Client_InputEvent (int evtype, float a, float b, float c)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!CSQC_Client_HasInputEvent ())
		return 0;
	// The module works in the QC/DP key domain; translate the internal ezq keynum ->
	// QC for key events (mouse/deltas - no translation).
	if (evtype == IE_KEYDOWN || evtype == IE_KEYUP)
		a = CSQC_Client_KeynumToQC ((int)a);
	// Parameters of the module function (4 float) - as CSQC_UpdateView.
	vm->globals[OFS_PARM0] = evtype;
	vm->globals[OFS_PARM1] = a;
	vm->globals[OFS_PARM2] = b;
	vm->globals[OFS_PARM3] = c;
	if (!CSQC_Client_Exec (s_csqc.func_inputevent))
		return 0;
	return (int)vm->globals[OFS_RETURN];
}

/*
=================
CSQC_Client_RunPlayerPhysics

#347 runstandardplayerphysics(ent): FTE semantics (PF_cs_runplayerphysics) on the
ezquake client PM path (PM_PlayerMove, as cl_pred.c):

- the movement command comes from the input_* globals (the module calls
  getinputstate(seq) before #347); fallback - the last recorded usercmd (if input_*
  are not declared);
- the solid set is rebuilt inside the call (CL_SetSolidEntities + Players); entity
  fields .mins/.maxs/.gravity/.pmove_flags/.flags; writes .origin/.velocity/.angles,
  .flags (FL_ONGROUND), .pmove_flags (PMF_JUMP_HELD);
- chunks <=50 ms (as cl_pred.c) + deprecated pmove_org/vel/onground.

Deviations from FTE (ezq pmove has no corresponding fields):
- skipent: FTE-PM does not read it (only trace helpers), and CL_SetSolidPlayers
  excludes the local player -> moot;
- .gravitydir: FTE default -z; directional requires porting the PM core (out of
  #347) - unused for the QW mod;
- onladder -> PMF_LADDER: FTE detects only Q2/Q3, dead in QW -> unreachable on FTE too;
- .waterlevel: PM computes and uses it internally, but does not write it to the field
  - neither ezq nor FTE client #347 writes it; .groundent is not in csdefs.
A box entity maps onto the global player_mins/maxs (other players' box is the same).
=================
*/
#define CSQC_MV_WALK	3	// MOVETYPE_* (FTE numbering)
#define CSQC_MV_FLY		5
#define CSQC_MV_NOCLIP	8
#define CSQC_PMF_JUMP_HELD	1
#define CSQC_FL_ONGROUND	512

void CSQC_Client_RunPlayerPhysics (int entnum)
{
	extern vec3_t player_mins, player_maxs;
	pr1vm_t *vm = &s_csqc.vm;
	float *base, *o, *v;
	vec3_t saved_mins, saved_maxs;
	int msecs, mt, i;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (entnum <= 0 || entnum >= CSQC_MAX_EDICTS || cls.state != ca_active)
		return;		// slot 0 = world (FTE: readonly guard)
	if (cls.demoplayback || cls.mvdplayback)
		return;		// live game only (physents from cl)

	base = (float *)((byte *)vm->game_edicts + (size_t)entnum * vm->edict_size);

	memset (&pmove, 0, sizeof (pmove));

	// --- input from input_* (fallback - last recorded usercmd) ---
	if (s_csqc.in_timelength >= 0 || s_csqc.in_angles >= 0 || s_csqc.in_movevalues >= 0)
	{
		int m = (s_csqc.in_timelength >= 0)
			? (int)(vm->globals[s_csqc.in_timelength] * 1000.0f) : 1;
		if (m < 1)
			m = 1;
		else if (m > 255)
			m = 255;
		pmove.cmd.msec = (byte)m;
		if (s_csqc.in_angles >= 0)
		{
			VectorCopy (&vm->globals[s_csqc.in_angles], pmove.cmd.angles);
			VectorCopy (pmove.cmd.angles, pmove.angles);
		}
		if (s_csqc.in_movevalues >= 0)
		{
			pmove.cmd.forwardmove = (short)vm->globals[s_csqc.in_movevalues + 0];
			pmove.cmd.sidemove = (short)vm->globals[s_csqc.in_movevalues + 1];
			pmove.cmd.upmove = (short)vm->globals[s_csqc.in_movevalues + 2];
		}
		if (s_csqc.in_buttons >= 0)
			pmove.cmd.buttons = (byte)vm->globals[s_csqc.in_buttons];
		if (s_csqc.in_impulse >= 0)
			pmove.cmd.impulse = (byte)vm->globals[s_csqc.in_impulse];
	}
	else
	{
		if (!s_last_seq)
			return;
		pmove.cmd = s_inhist[s_last_seq % CSQC_INHIST].cmd;
		VectorCopy (pmove.cmd.angles, pmove.angles);
	}

	// --- entity state ---
	if (s_csqc.f_origin >= 0)
		VectorCopy (base + s_csqc.f_origin, pmove.origin);
	else
		VectorClear (pmove.origin);
	if (s_csqc.f_velocity >= 0)
		VectorCopy (base + s_csqc.f_velocity, pmove.velocity);
	else
		VectorClear (pmove.velocity);
	if (s_csqc.f_flags >= 0)
		pmove.onground = (((int)base[s_csqc.f_flags]) & CSQC_FL_ONGROUND) != 0;

	// box ent -> global player_mins/maxs (ezq PM/SetSolidPlayers use the global
	// box); save and restore after the call.
	VectorCopy (player_mins, saved_mins);
	VectorCopy (player_maxs, saved_maxs);
	if (s_csqc.f_mins >= 0 && s_csqc.f_maxs >= 0)
	{
		float *mn = base + s_csqc.f_mins, *mx = base + s_csqc.f_maxs;
		if (mn[0] || mn[1] || mn[2] || mx[0] || mx[1] || mx[2])
		{
			VectorCopy (mn, player_mins);
			VectorCopy (mx, player_maxs);
		}
	}

	mt = (s_csqc.f_movetype >= 0) ? (int)base[s_csqc.f_movetype] : CSQC_MV_WALK;
	switch (mt)
	{
	case CSQC_MV_FLY:
		pmove.pm_type = PM_FLY;
		break;
	case CSQC_MV_NOCLIP:
		pmove.pm_type = PM_SPECTATOR;
		break;
	default:
		pmove.pm_type = PM_NORMAL;
		break;
	}
	pmove.jump_held = (s_csqc.f_pmove_flags >= 0)
		? (((int)base[s_csqc.f_pmove_flags] & CSQC_PMF_JUMP_HELD) != 0) : false;
	pmove.jump_msec = 0;
	pmove.waterjumptime = 0;

	movevars.entgravity = cl.entgravity;
	movevars.maxspeed = cl.maxspeed;
	movevars.bunnyspeedcap = cl.bunnyspeedcap;
	if (s_csqc.f_gravity >= 0 && base[s_csqc.f_gravity] != 0)
		movevars.entgravity = base[s_csqc.f_gravity];

	// solid set: world+BSP ents (rebuilt after memset) + players (cl_pred).
	CL_SetSolidEntities ();
	CL_SetSolidPlayers (cl.playernum);

	// --- chunks <=50 ms ---
	msecs = pmove.cmd.msec;
	if (msecs <= 0)
		msecs = 1;
	while (msecs > 0)
	{
		int step = (msecs > 50) ? 50 : msecs;
		pmove.cmd.msec = step;
		PM_PlayerMove ();
		msecs -= step;
	}

	// --- result back into the entity ---
	o = (s_csqc.f_origin >= 0) ? base + s_csqc.f_origin : NULL;
	v = (s_csqc.f_velocity >= 0) ? base + s_csqc.f_velocity : NULL;
	if (o)
		VectorCopy (pmove.origin, o);
	if (v)
		VectorCopy (pmove.velocity, v);
	if (s_csqc.f_angles >= 0)
		VectorCopy (pmove.angles, base + s_csqc.f_angles);
	if (s_csqc.f_flags >= 0)
	{
		float *fl = base + s_csqc.f_flags;
		*fl = (float)(pmove.onground
			? (((int)*fl) | CSQC_FL_ONGROUND)
			: (((int)*fl) & ~CSQC_FL_ONGROUND));
	}
	if (s_csqc.f_pmove_flags >= 0)
		base[s_csqc.f_pmove_flags] = (float)(pmove.jump_held ? CSQC_PMF_JUMP_HELD : 0);

	// deprecated globals (read by the fo-module).
	if (s_csqc.p_org >= 0)
		for (i = 0; i < 3; i++)
			vm->globals[s_csqc.p_org + i] = pmove.origin[i];
	if (s_csqc.p_vel >= 0)
		for (i = 0; i < 3; i++)
			vm->globals[s_csqc.p_vel + i] = pmove.velocity[i];
	if (s_csqc.p_onground >= 0)
		vm->globals[s_csqc.p_onground] = pmove.onground ? 1 : 0;

	VectorCopy (saved_mins, player_mins);
	VectorCopy (saved_maxs, player_maxs);
}

/*
=================
string-buffers (#460-469). Storage - s_bufs (handle = idx+1).
=================
*/
static csqc_buf_t *CSQC_Client_BufAt (int handle)
{
	if (handle < 1 || handle > CSQC_MAX_BUFS || !s_bufs[handle - 1].inuse)
		return NULL;
	return &s_bufs[handle - 1];
}

static void CSQC_Client_BufClear (csqc_buf_t *b)
{
	int i;
	for (i = 0; i < b->num; i++)
	{
		if (b->str[i])
			Q_free (b->str[i]);
	}
	Q_free (b->str);
	b->str = NULL;
	b->num = b->cap = 0;
}

int CSQC_Client_BufCreate (void)
{
	int i;
	for (i = 0; i < CSQC_MAX_BUFS; i++)
	{
		if (!s_bufs[i].inuse)
		{
			memset (&s_bufs[i], 0, sizeof (s_bufs[i]));
			s_bufs[i].inuse = true;
			return i + 1;
		}
	}
	return 0;
}

void CSQC_Client_BufDel (int handle)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	if (b)
	{
		CSQC_Client_BufClear (b);
		b->inuse = false;
	}
}

int CSQC_Client_BufGetSize (int handle)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	return b ? b->num : 0;
}

static int CSQC_Client_BufPush (csqc_buf_t *b, const char *s)
{
	char **ns;
	int idx;
	if (b->num >= b->cap)
	{
		int ncap = b->cap ? b->cap * 2 : 8;
		ns = (char **)Q_realloc (b->str, sizeof (char *) * ncap);
		if (!ns)
			return -1;
		b->str = ns;
		b->cap = ncap;
	}
	idx = b->num;
	b->str[idx] = Q_strdup (s ? s : "");
	b->num++;
	return idx;
}

int CSQC_Client_BufAdd (int handle, const char *s, int order)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	int idx, i;
	if (!b)
		return -1;
	idx = CSQC_Client_BufPush (b, s);
	if (idx < 0)
		return -1;
	// order > 0 - insert at the position (not past the end of the list).
	if (order > 0 && order < idx)
	{
		char *tmp = b->str[idx];
		for (i = idx; i > order; i--)
			b->str[i] = b->str[i - 1];
		b->str[order] = tmp;
	}
	return idx;
}

int CSQC_Client_BufGet (int handle, int idx, char *out, size_t max)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	if (!b || idx < 0 || idx >= b->num || !out || max < 1)
		return 0;
	strlcpy (out, b->str[idx], max);
	return 1;
}

int CSQC_Client_BufSet (int handle, int idx, const char *s)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	char *c;
	if (!b || idx < 0 || idx >= b->num)
		return 0;
	c = Q_strdup (s ? s : "");
	if (!c)
		return 0;
	Q_free (b->str[idx]);
	b->str[idx] = c;
	return 1;
}

int CSQC_Client_BufFree (int handle, int idx)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	int i;
	if (!b || idx < 0 || idx >= b->num)
		return 0;
	Q_free (b->str[idx]);
	for (i = idx; i < b->num - 1; i++)
		b->str[i] = b->str[i + 1];
	b->num--;
	return 1;
}

int CSQC_Client_BufCopy (int from, int to)
{
	csqc_buf_t *f = CSQC_Client_BufAt (from);
	csqc_buf_t *t = CSQC_Client_BufAt (to);
	int i;
	if (!f || !t)
		return 0;
	CSQC_Client_BufClear (t);
	for (i = 0; i < f->num; i++)
		CSQC_Client_BufPush (t, f->str[i]);
	return 1;
}

int CSQC_Client_BufSort (int handle, int prefixlen, int backward)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	int i, j;
	if (!b)
		return 0;
	(void)prefixlen;	// sort by the whole string (prefix semantics not emulated)
	for (i = 0; i < b->num; i++)
	{
		for (j = i + 1; j < b->num; j++)
		{
			int cmp = strcmp (b->str[i], b->str[j]);
			if ((!backward && cmp > 0) || (backward && cmp < 0))
			{
				char *t = b->str[i];
				b->str[i] = b->str[j];
				b->str[j] = t;
			}
		}
	}
	return 1;
}

int CSQC_Client_BufImplode (int handle, const char *glue, char *out, size_t max)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	size_t o = 0;
	int i;
	if (!b || !out || max < 1)
		return 0;
	out[0] = 0;
	for (i = 0; i < b->num && o + 1 < max; i++)
	{
		if (i && glue)
		{
			size_t gl = strlen (glue);
			if (o + gl < max - 1)
			{
				memcpy (out + o, glue, gl);
				o += gl;
				out[o] = 0;
			}
		}
		{
			size_t l = strlen (b->str[i]);
			if (o + l >= max)
				l = max - 1 - o;
			memcpy (out + o, b->str[i], l);
			o += l;
			out[o] = 0;
		}
	}
	return 1;
}

void CSQC_Client_BufReset (void)
{
	int i;
	for (i = 0; i < CSQC_MAX_BUFS; i++)
	{
		if (s_bufs[i].inuse)
		{
			CSQC_Client_BufClear (&s_bufs[i]);
			s_bufs[i].inuse = false;
		}
	}
}


/*
=================
CSQC_Client_Disconnect
=================
*/
void CSQC_Client_Disconnect (void)
{
	int i;

	if (s_csqc.loaded)
	{
		if (s_csqc.inited && !s_csqc.errored)
			CSQC_Client_Exec (s_csqc.func_shutdown);
		PR1VM_UnLoad (&s_csqc.vm);
	}
	// Heap-owned csprogs buffer: the VM no longer references it after UnLoad;
	// free it here, before the state is cleared (also covers a loaded module that
	// failed after PR1VM_LoadClientV6, e.g. CSQC_Init errored).
	Q_free (s_csqc.module_data);
	s_csqc.module_data = NULL;
	CSQC_Client_ClearCommands ();
	// edict arena before memset (the pointers are still in place).
	CSQC_Client_FreeArena ();
	// Reset the module cursor (#343): on a new connect the state is clean.
	s_cursormode.usecursor = false;
	s_cursormode.cursorimage[0] = 0;
	s_cursormode.scale = 0;
	// #346: sensitivity back to default.
	s_sens_scale = 1;
	// clear string-buffers (deep-copy strings)
	CSQC_Client_BufReset ();
	// #371: drop deltalisten registrations / the player-bridge map.
	CSQC_Client_DeltaReset ();
	CSQC_Client_ViewReset ();
	s_scene_rendered = false;	// takeover scene reset
	CSQC_Client_ModelReset ();	// CSQC model registry
	memset (&s_csqc, 0, sizeof (s_csqc));
	memset (s_csqc_stat, 0, sizeof (s_csqc_stat));
	memset (s_csqc_statsf, 0, sizeof (s_csqc_statsf));
	// Stat wire 78/79: string stats are deep copies (Q_strdup).
	for (i = 0; i < MAX_EXTENDED_CL_STATS; i++)
	{
		Q_free (s_csqc_statss[i]);
		s_csqc_statss[i] = NULL;
	}
	s_csqc.func_init = s_csqc.func_world = s_csqc.func_update =
		s_csqc.func_console = s_csqc.func_shutdown = -1;
	s_csqc.func_entupdate = s_csqc.func_entremove = s_csqc.func_parseevent = -1;
	s_csqc.func_parseprint = s_csqc.func_parsecp = -1;
	s_csqc.func_parsedamage = -1;
	s_csqc.func_eventsound = -1;
	s_csqc.func_parsesetangles = -1;
	s_csqc.func_rr = -1;
	s_csqc.func_entspawn = -1;
	s_csqc.mayread = false;
	s_csqc.func_input = -1;
	s_csqc.func_inputevent = -1;
	s_csqc.func_startframe = s_csqc.func_endframe = -1;
	s_csqc.global_time = -1;
	s_csqc.global_gamespeed = -1;
	s_csqc.global_self = -1;
	s_csqc.global_other = -1;
	s_csqc.global_physics_mode = -1;
	s_csqc.field_entnum = -1;
	s_csqc.f_origin = s_csqc.f_velocity = s_csqc.f_angles = s_csqc.f_mins = s_csqc.f_maxs = -1;
	s_csqc.f_movetype = s_csqc.f_flags = s_csqc.f_gravity = s_csqc.f_pmove_flags = -1;
	s_csqc.f_modelindex = s_csqc.f_skin = -1;
	s_csqc.f_frame = s_csqc.f_effects = s_csqc.f_colormap = s_csqc.f_drawmask = -1;
	s_csqc.f_think = s_csqc.f_nextthink = -1;
	s_csqc.g_localentnum = -1;
	s_csqc.in_timelength = s_csqc.in_angles = s_csqc.in_movevalues = -1;
	s_csqc.in_buttons = s_csqc.in_impulse = -1;
	s_csqc.in_sequence = -1;
	s_csqc.g_ccframe = s_csqc.g_scframe = -1;
	s_csqc.p_org = s_csqc.p_vel = s_csqc.p_onground = -1;
	s_csqc.g_vfwd = s_csqc.g_vright = s_csqc.g_vup = -1;
	s_csqc.g_view_angles = -1;
	s_csqc.g_frametime = s_csqc.g_cltime = s_csqc.g_maxclients = -1;
	s_csqc.g_player_localnum = s_csqc.g_intermission = -1;
	CSQC_Client_OffsetCacheReset ();	// hot-path offsets
	s_last_seq = 0;
	s_ccframe = 0;
}

#endif // !CLIENTONLY
