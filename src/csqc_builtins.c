/*
 csqc_builtins.c -- client PR1VM builtins (our csprogs.dat).

 Builtins for the client instance: numbers are baked from TF2003 csdefs.qc (= #N),
 arguments are read from vm->globals[OFS_PARM0..], return value in OFS_RETURN,
 strings via CSQC_Client_GetString/PR1VM_ClientSetString on the active instance.

 Implemented here: dprint/ftos/registercommand/tokenize/argv plus the layers added
 incrementally (drawstring/getstatf/read builtins/sprintf).
*/

#ifndef CLIENTONLY
#include "qwsvdef.h"
#include "quakedef.h"	// client.h (cls: netchan/fteprotocolextensions/state) with needed types
#include <time.h>		// csqc_calltimeofday (#231)
#include <stdlib.h>		// strtod (#81/#117)
#include <ctype.h>		// tolower (#494 crc16 insensitive, #480/481)
#include <math.h>		// libm math (#471-475/#532)
#include <string.h>		// strlen/strncmp/strcasecmp (#228-230)
#ifndef _WIN32
#include <strings.h>		// strcasecmp/strncasecmp (#229/230; on MSVC these are macros in q_shared.h)
#endif
#include "keys.h"		// Key_KeynumToString/Key_StringToKeynum
#include "qsound.h"		// S_LocalSoundWithVol (#177)
#include "cl_tent.h"		// CL_CreateBeam (#428-431)
#include "gl_model.h"		// custom_model_*/Mod_CustomModel (#431 no-op)
#include "crc.h"		// CRC_Init/CRC_ProcessByte/CRC_Value (#494 crc16, #639 digest_hex)
#include "sha1.h"		// SHA1Init/SHA1Update/SHA1Final reentrant API (#639 digest_hex)
#include "screen.h"		// SCR_CenterPrint (#338 cprint)
#include "pr1vm.h"
#include "csqc_client.h"	// accessors to client state/output
#include "utils.h"		// HexToInt (#476/#477 strlennocol/strdecolorize)
#include "sbar.h"		// Sbar_ColorForMap (topcolor_rgb/bottomcolor_rgb)
#include "net.h"		// NET_AdrToString (serverkey "ip")

static pr1vm_t *CSQCVM_Active (void)
{
	return PR1VM_Active ();
}

// Reuse the pure float/vector bodies of the server builtins on the client instance.
// These bodies do not touch strings/edict/sv state; arguments/return are read via
// the G_* macros (pr_globals) - the attach in PR1VM_ExecuteProgram points
// pr_globals at the executing (client) VM globals, so the call is correct.
// Non-static server PF_* are declared in pr_cmds.c; extern prototypes here.
extern void PF_random (void);
extern void PF_normalize (void);
extern void PF_vlen (void);
extern void PF_vectoyaw (void);
extern void PF_vectoangles (void);
extern void PF_rint (void);
extern void PF_floor (void);
extern void PF_ceil (void);
extern void PF_fabs (void);
extern void PF_sin (void);
extern void PF_cos (void);
extern void PF_sqrt (void);
extern void PF_min (void);
extern void PF_max (void);
extern void PF_bound (void);
extern void PF_traceon (void);
extern void PF_traceoff (void);

// Dedicated token context for the client VM (#441 tokenize / #442 argv). Server PR1
// uses its own pr1_tokencontext (pr_cmds.c); here we keep a separate one so we do not
// share the engine's global token buffer (Cmd_TokenizeString) used by the console.
static tokenizecontext_t csqc_tokencontext;

static char *CSQCVM_Str (int ofs)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return NULL;
	return CSQC_Client_GetString (vm, *(int *)&vm->globals[ofs]);
}

static void CSQCVM_SetRetStr (char *s)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_RETURN], s);
}

// Varargs concatenation following the PR1 convention (3 float slots per argument),
// like the server PF_VarString (pr_cmds.c). Used for error/objerror/localcmd/cprint/
// print (first=0) and infoadd (first=2).
static char *CSQCVM_VarString (int first)
{
	pr1vm_t *vm = CSQCVM_Active ();
	static char out[2048];
	int i;

	out[0] = 0;
	if (!vm)
		return out;
	for (i = first; i < vm->argc; i++)
	{
		char *s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (s)
			strlcat (out, s, sizeof (out));
	}
	return out;
}

// Entity values in the client VM are raw int bit offsets (N*edict_size), the same
// convention as the interpreter's entity opcodes (OP_STORE_ENT/PR1VM_ProgToEdict)
// and FTE (G_EDICT/G_INT). All builtins must read entity args and write entity
// returns through these helpers; a float cast/assignment corrupts the bits.
// Defined below (entity section); forward-declared for the #347/#459/te_beam users.
static int csqc_ent_of (pr1vm_t *vm, int parmofs);
static float *csqc_ent_field (pr1vm_t *vm, int entnum, const char *name);
static float *csqc_ent_ofs (pr1vm_t *vm, int entnum, int fldofs);	// access via cached field offset
static void csqc_ret_entity (pr1vm_t *vm, int entnum);
static void csqc_add_one_entity (int e);	// arena edict -> ezq entity_t

/*
void(string s, ...) dprint = #25
*/
static void csqc_dprint (void)
{
	char *s = CSQCVM_Str (OFS_PARM0);
	if (s)
		Con_Printf ("%s", s);
}

/*
 Port of Q_ftoa for #26 ftos: float -> string without losing significant digits
 ("infinite decimal places"), trimming trailing zeros.
*/
static void csqc_q_ftoa (char *str, size_t maxlen, float in)
{
	unsigned int i = *((unsigned int *)&in);
	int signbit = (i & 0x80000000u) >> 31;
	int exp = (int)((i & 0x7F800000u) >> 23) - 127;
	int mantissa = (i & 0x007FFFFFu);
	char buf[64];
	char *p;

	if (exp == 128)
	{
		snprintf (buf, sizeof (buf), "%s%s", signbit ? "-" : "",
			mantissa == 0 ? "1.#INF" : "1.#NAN");
		strlcpy (str, buf, maxlen);
		return;
	}
	exp = -exp;
	exp = (int)(exp * 0.30102999957f);	// base 2 -> base 10
	exp += 8;
	if (exp <= 0)
		snprintf (buf, sizeof (buf), "%.0f", in);
	else
	{
		char fmt[16];
		snprintf (fmt, sizeof (fmt), "%%.%if", exp);
		snprintf (buf, sizeof (buf), fmt, in);
		// trim trailing zeros and dot (like Q_ftoa)
		for (p = buf + strlen (buf) - 1; p > buf && *p == '0'; p--)
			*p = '\0';
		if (*p == '.')
			*p = '\0';
	}
	strlcpy (str, buf, maxlen);
}

/*
 string(float val) ftos = #26 - FTE parity (PF_ftos): integer value -> "%d";
 otherwise Q_ftoa (fractional part is not lost).
*/
static void csqc_ftos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float v;
	char buf[64];
	if (!vm)
		return;
	v = vm->globals[OFS_PARM0];
	if (v >= -2147483648.0f && v <= 2147483647.0f && v == (float)(int)v)
		snprintf (buf, sizeof (buf), "%d", (int)v);
	else
		csqc_q_ftoa (buf, sizeof (buf), v);
	CSQCVM_SetRetStr (buf);
}

/*
void(string cmdname) registercommand = #352
*/
static void csqc_registercommand (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *cmd = CSQCVM_Str (OFS_PARM0);
	if (vm && cmd)
		CSQC_Client_RegisterCommand (cmd);
}

/*
 float(string varname) cvar = #45

 Returns the engine cvar value by name (0 if there is no such cvar). Needed by the
 module for diagnostic switches (e.g. csqc_dbg) and config.
*/
static void csqc_cvar (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	vm->globals[OFS_RETURN] = (vm && name && name[0]) ? Cvar_Value (name) : 0;
}

/*
 void(vector vang) makevectors = #1
 FTE parity (PF_cs_makevectors): from the angle vector (pitch,yaw,roll) writes the
 module's v_forward/v_right/v_up. Internal globals are resolved by
 CSQC_Client_MakeVectors (no-op if there is no declaration). The classic low number
 #1 is now available to modules (the server set is not loaded into the client).
*/
static void csqc_makevectors (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_MakeVectors (&vm->globals[OFS_PARM0]);
}

/*
 void(vector dir) vectorvectors = #432 - FTE parity (PF_vectorvectors):
 normalized dir -> module v_forward, orthogonal v_right/v_up. Body is
 CSQC_Client_VectorVectors.
*/
static void csqc_vectorvectors (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_VectorVectors (&vm->globals[OFS_PARM0]);
}

/*
 float() random = #7 - FTE parity (PF_random). Returns a value in (0,1):
 (rand&0x7fff)/0x8000 + 0.5/0x8000 - never 0 or 1 (unlike the server ezq PF_random,
 which can return 1.0). FTE optional: argc==1 -> *x; argc>=2 -> a + r*(b-a). This is
 a client-side wrapper; the shared server PF_random is left untouched.
*/
static void csqc_random (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float r;
	if (!vm)
		return;
	r = (float)(rand () & 0x7fff) / 0x8000 + (0.5f / 0x8000);
	if (vm->argc == 1)
		r *= vm->globals[OFS_PARM0];
	else if (vm->argc >= 2)
		r = vm->globals[OFS_PARM0] + r * (vm->globals[OFS_PARM1] - vm->globals[OFS_PARM0]);
	vm->globals[OFS_RETURN] = r;
}

/*
 float(vector v [, optional entity reference]) vectoyaw = #13 - FTE parity
 (PF_vectoyaw): yaw = (int)(atan2*180/pi), <0 -> +360. The FTE optional entity is
 the gravity axis; the client does not track an axis (no gravitydir), so the identity
 axis is used (FTE default without gravitydir).
*/
static void csqc_vectoyaw (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *v;
	float x, y, yaw;
	if (!vm)
		return;
	v = &vm->globals[OFS_PARM0];
	x = v[0];
	y = v[1];
	if (y == 0 && x == 0)
		yaw = 0;
	else
	{
		yaw = (float)(int)(atan2 (y, x) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}
	vm->globals[OFS_RETURN] = yaw;
}

/*
 vector(vector fwd [, optional vector up]) vectoangles = #51 - FTE parity
 (PF_vectoangles -> VectorAngles, meshpitch=1). Optional up -> roll.
 meshpitch/r_meshroll are ignored here (=1).
*/
static void csqc_vectoangles (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const float *forward;
	float *up;
	float result[3];

	if (!vm)
		return;
	forward = &vm->globals[OFS_PARM0];
	up = (vm->argc >= 2) ? &vm->globals[OFS_PARM0 + 3] : NULL;

	CSQC_VectorAngles (forward, up, result);
	vm->globals[OFS_RETURN] = result[0];
	vm->globals[OFS_RETURN + 1] = result[1];
	vm->globals[OFS_RETURN + 2] = result[2];
}

/*
float(string s) tokenize = #441
*/
static void csqc_tokenize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	if (s)
		Cmd_TokenizeStringEx (&csqc_tokencontext, s);
	vm->globals[OFS_RETURN] = Cmd_ArgcEx (&csqc_tokencontext);
}

/*
string(float n) argv = #442
*/
static void csqc_argv (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int n;
	if (!vm)
		return;
	n = (int)vm->globals[OFS_PARM0];
	CSQCVM_SetRetStr (Cmd_ArgvEx (&csqc_tokencontext, n));
}

/*
 string(string s1, optional string s2, ...) strcat = #115
 Concatenation of the passed strings (up to vm->argc arguments).
*/
static void csqc_strcat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[2048];
	int n, i, len = 0;

	if (!vm)
		return;
	n = vm->argc;
	if (n <= 0)
		n = 1;
	buf[0] = 0;
	// PR1 params are 3 float slots per argument (like PF_VarString: OFS_PARM0 + i*3);
	// argc = argument count.
	for (i = 0; i < n && i < 16; i++)
	{
		char *s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (s)
			len += snprintf (buf + len, sizeof (buf) - len, "%s", s);
		if (len >= (int)sizeof (buf) - 1)
			break;
	}
	CSQCVM_SetRetStr (buf);
}

/*
 float(string s1, string sub, optional float startidx) strstrofs = #221 - FTE parity
 (PF_strstrofs): returns the substring position (0-based) or -1; start outside
 [0,len] (and not 0) -> -1.
*/
static void csqc_strstrofs (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *hay, *needle, *p;
	int start, len;

	if (!vm)
		return;
	hay = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	needle = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM1]);
	start = (vm->argc > 2) ? (int)vm->globals[OFS_PARM2] : 0;
	if (!hay)
		hay = "";
	if (!needle)
		needle = "";
	len = strlen (hay);
	if (start != 0 && (start < 0 || start > len))
	{
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	p = strstr (hay + start, needle);
	vm->globals[OFS_RETURN] = p ? (p - hay) : -1;
}

/*
 string(float ccase, float redalpha, float redchars, string str, ...) strconv = #224
 Port of FTE PF_strconv plus chrconv_number/chrconv_punct/chrchar_alpha: bulk case/
 colour conversion. ccase 0 same/1 lower/2 upper; redalpha 0 same/1 white/2 red/
 5 alternate/6 alternate-alternate; redchars is the same for digits. The string
 argument is a vararg starting at position 3.
 */
static int csqc_chrconv_number (int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 5:
	case 6:
	case 0:
		break;
	case 1:
		base = '0';
		break;
	case 2:
		base = '0' + 128;
		break;
	case 3:
		base = '0' - 30;
		break;
	case 4:
		base = '0' + 128 - 30;
		break;
	}
	return i + base;
}

static int csqc_chrconv_punct (int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 0:
		break;
	case 1:
		base = 0;
		break;
	case 2:
		base = 128;
		break;
	}
	return i + base;
}

static int csqc_chrchar_alpha (int i, int basec, int baset, int convc, int convt, int charnum)
{
	i -= baset + basec;
	switch (convt)
	{
	default:
	case 0:
		break;
	case 1:
		baset = 0;
		break;
	case 2:
		baset = 128;
		break;
	case 5:
	case 6:
		baset = 128 * ((charnum & 1) == (convt - 5));
		break;
	}
	switch (convc)
	{
	default:
	case 0:
		break;
	case 1:
		basec = 'a';
		break;
	case 2:
		basec = 'A';
		break;
	}
	return i + basec + baset;
}

static void csqc_strconv (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int ccase, redalpha, rednum, len, i;
	const unsigned char *string;
	unsigned char resbuf[2048];
	unsigned char *result = resbuf;

	if (!vm)
		return;
	ccase = (int)vm->globals[OFS_PARM0];
	redalpha = (int)vm->globals[OFS_PARM1];
	rednum = (int)vm->globals[OFS_PARM2];
	string = (const unsigned char *)CSQCVM_VarString (3);
	len = strlen ((const char *)string);
	if (len >= (int)sizeof (resbuf))
		len = sizeof (resbuf) - 1;

	for (i = 0; i < len; i++, string++, result++)
	{
		if (*string >= '0' && *string <= '9')
			*result = csqc_chrconv_number (*string, '0', rednum);
		else if (*string >= '0' + 128 && *string <= '9' + 128)
			*result = csqc_chrconv_number (*string, '0' + 128, rednum);
		else if (*string >= '0' + 128 - 30 && *string <= '9' + 128 - 30)
			*result = csqc_chrconv_number (*string, '0' + 128 - 30, rednum);
		else if (*string >= '0' - 30 && *string <= '9' - 30)
			*result = csqc_chrconv_number (*string, '0' - 30, rednum);
		else if (*string >= 'a' && *string <= 'z')
			*result = csqc_chrchar_alpha (*string, 'a', 0, ccase, redalpha, i);
		else if (*string >= 'A' && *string <= 'Z')
			*result = csqc_chrchar_alpha (*string, 'A', 0, ccase, redalpha, i);
		else if (*string >= 'a' + 128 && *string <= 'z' + 128)
			*result = csqc_chrchar_alpha (*string, 'a', 128, ccase, redalpha, i);
		else if (*string >= 'A' + 128 && *string <= 'Z' + 128)
			*result = csqc_chrchar_alpha (*string, 'A', 128, ccase, redalpha, i);
		else if ((*string & 127) < 16 || !redalpha)
			*result = *string;
		else if (*string < 128)
			*result = csqc_chrconv_punct (*string, 0, redalpha);
		else
			*result = csqc_chrconv_punct (*string, 128, redalpha);
	}
	*result = 0;
	CSQCVM_SetRetStr ((char *)resbuf);
}

/*
 float(float property, ...) getproperty = #309

 Full FTE read parity (PF_R_GetViewFlag): reads the current engine render state
 (r_refdef/cl/vid), not "values set by the module via #303". The VF_* numbers come
 from the module's csdefs.qc. The set-flags (DRAWWORLD etc.) are absent from the FTE
 getter list -> default 0; we also return 0 here.
 */
#define CSQC_VF_MIN		1	// viewport top-left (x,y)
#define CSQC_VF_MIN_X		2
#define CSQC_VF_MIN_Y		3
#define CSQC_VF_SIZE		4	// viewport width/height
#define CSQC_VF_SIZE_X		5
#define CSQC_VF_SIZE_Y		6
#define CSQC_VF_VIEWPORT	7	// (width, height)
#define CSQC_VF_FOV		8	// (fov_x, fov_y)
#define CSQC_VF_FOV_X		9
#define CSQC_VF_FOV_Y		10
#define CSQC_VF_ORIGIN		11
#define CSQC_VF_ORIGIN_X	12
#define CSQC_VF_ORIGIN_Y	13
#define CSQC_VF_ORIGIN_Z	14
#define CSQC_VF_ANGLES		15
#define CSQC_VF_ANGLES_X	16
#define CSQC_VF_ANGLES_Y	17
#define CSQC_VF_ANGLES_Z	18
#define CSQC_VF_CL_VIEWANGLES	33
#define CSQC_VF_CL_VIEWANGLES_X	34
#define CSQC_VF_CL_VIEWANGLES_Y	35
#define CSQC_VF_CL_VIEWANGLES_Z	36
#define CSQC_VF_AFOV		203
#define CSQC_VF_SCREENVSIZE	204
#define CSQC_VF_SCREENPSIZE	205

static void csqc_getproperty (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float prop;
	float *r;

	if (!vm)
		return;
	prop = vm->globals[OFS_PARM0];
	r = &vm->globals[OFS_RETURN];
	r[0] = r[1] = r[2] = 0;
	if (cls.state != ca_active)
		return;

	switch ((int)prop)
	{
	case CSQC_VF_SCREENVSIZE:
	case CSQC_VF_SCREENPSIZE:
		// "virtual"/"physical" size; in ezquake without OS scaling they are the same.
		r[0] = vid.width;
		r[1] = vid.height;
		break;
	case CSQC_VF_FOV:
		r[0] = r_refdef.fov_x;
		r[1] = r_refdef.fov_y;
		break;
	case CSQC_VF_FOV_X:
		r[0] = r_refdef.fov_x;
		break;
	case CSQC_VF_FOV_Y:
		r[0] = r_refdef.fov_y;
		break;
	case CSQC_VF_AFOV:
		// FTE reads r_refdef.afov; ezquake has none - approximate with the fov cvar.
		r[0] = Cvar_Value ("fov");
		break;
	case CSQC_VF_ORIGIN:
		VectorCopy (r_refdef.vieworg, r);
		break;
	case CSQC_VF_ORIGIN_X:
		r[0] = r_refdef.vieworg[0];
		break;
	case CSQC_VF_ORIGIN_Y:
		r[0] = r_refdef.vieworg[1];
		break;
	case CSQC_VF_ORIGIN_Z:
		r[0] = r_refdef.vieworg[2];
		break;
	case CSQC_VF_ANGLES:
		VectorCopy (r_refdef.viewangles, r);
		break;
	case CSQC_VF_ANGLES_X:
		r[0] = r_refdef.viewangles[0];
		break;
	case CSQC_VF_ANGLES_Y:
		r[0] = r_refdef.viewangles[1];
		break;
	case CSQC_VF_ANGLES_Z:
		r[0] = r_refdef.viewangles[2];
		break;
	case CSQC_VF_CL_VIEWANGLES:
		VectorCopy (cl.viewangles, r);
		break;
	case CSQC_VF_CL_VIEWANGLES_X:
		r[0] = cl.viewangles[0];
		break;
	case CSQC_VF_CL_VIEWANGLES_Y:
		r[0] = cl.viewangles[1];
		break;
	case CSQC_VF_CL_VIEWANGLES_Z:
		r[0] = cl.viewangles[2];
		break;
	case CSQC_VF_VIEWPORT:	// FTE: grect.width/height
		r[0] = r_refdef.vrect.width;
		r[1] = r_refdef.vrect.height;
		break;
	case CSQC_VF_MIN:
		r[0] = r_refdef.vrect.x;
		r[1] = r_refdef.vrect.y;
		break;
	case CSQC_VF_MIN_X:
		r[0] = r_refdef.vrect.x;
		break;
	case CSQC_VF_MIN_Y:
		r[0] = r_refdef.vrect.y;
		break;
	case CSQC_VF_SIZE:
		r[0] = r_refdef.vrect.width;
		r[1] = r_refdef.vrect.height;
		break;
	case CSQC_VF_SIZE_X:
		r[0] = r_refdef.vrect.width;
		break;
	case CSQC_VF_SIZE_Y:
		r[0] = r_refdef.vrect.height;
		break;
	default:
		// set-flags (DRAWWORLD/PERSPECTIVE/...) and no-analog/DP-legacy -> 0
		// (not in the FTE getter list; its default returns 0).
		break;
	}
}

/*
 vector(vector v) unproject = #310 / vector(vector v) project = #311.
 Screen<->world via the engine matrices (R_GetModelviewMatrix/R_GetProjectionMatrix/
 R_GetViewport). Failure/degenerate matrix -> '0 0 0'.
 */
static void csqc_project (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float sx = 0, sy = 0, sz = 0;
	if (!vm)
		return;
	if (!CSQC_Client_Project (&vm->globals[OFS_PARM0], &sx, &sy, &sz))
		sx = sy = sz = 0;
	vm->globals[OFS_RETURN + 0] = sx;
	vm->globals[OFS_RETURN + 1] = sy;
	vm->globals[OFS_RETURN + 2] = sz;
}

static void csqc_unproject (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float world[3] = { 0, 0, 0 };
	if (!vm)
		return;
	if (!CSQC_Client_Unproject (vm->globals[OFS_PARM0], vm->globals[OFS_PARM0 + 1],
		vm->globals[OFS_PARM0 + 2], world))
		world[0] = world[1] = world[2] = 0;
	vm->globals[OFS_RETURN + 0] = world[0];
	vm->globals[OFS_RETURN + 1] = world[1];
	vm->globals[OFS_RETURN + 2] = world[2];
}

/*
 void() clearscene = #300 / void(float mask) addentities = #301 /
 float(float property, ...) setproperty = #303 / void() renderscene = #304
 The module does not do its own 3D rendering (the engine draws); the HUD is on top.
 */
static void csqc_clearscene (void)
{
	// clearscene resets the view properties (#303 setproperty) and the rentity list.
	CSQC_Client_ResetViewProps ();
	if (CSQC_Client_SceneActive ())
		CL_ClearScene ();
}
/*
 void(float mask) addentities = #301.
 FTE PF_R_AddEntityMask: mask&1 (MASK_DELTA=MASK_ENGINE) - the engine scene
 (CL_EmitEntities: world/players/entities); mask&2 (MASK_STDVIEWMODEL) - the engine
 view model (CL_LinkViewModel); then walk the CSQC arena edicts for the whole mask
 (`drawmask & mask`).
 */
static void csqc_addentities (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int mask, e;
	if (!vm || !CSQC_Client_SceneActive ())
		return;
	mask = (int)vm->globals[OFS_PARM0];
	if (mask & 1)
		CL_EmitEntitiesKeepScene ();	// merge without CL_ClearScene
	if (mask & 2)
		CSQC_Client_LinkViewModel ();
	for (e = 1; e < vm->num_edicts; e++)
	{
		float *dm;
		if (!CSQC_Client_EntUsed (e))
			continue;
		dm = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_DRAWMASK));
		if (!dm || !((int)dm[0] & mask))
			continue;
		csqc_add_one_entity (e);
	}
}
/*
 float(float property, ...) setproperty = #303 (view subset).
 Handles VF_MIN/SIZE/VIEWPORT/FOV/ORIGIN/ANGLES (and _X/_Y/_Z); the values are
 applied to r_refdef after V_CalcRefdef when CSQC is active (1-frame lag -
 CSQC_UpdateView in the HUD phase). Other properties are no-op (FTE default).
 */
static void csqc_setproperty (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int prop, words, i;
	float args[6];
	if (!vm)
		return;
	prop = (int)vm->globals[OFS_PARM0];
	// after property: (argc-1) QC args, 3 words each; VF_VIEWPORT = vector+vector
	// (position+size) = 6 words.
	words = (vm->argc - 1) * 3;
	if (words < 0)
		words = 0;
	if (words > 6)
		words = 6;
	for (i = 0; i < words; i++)
		args[i] = vm->globals[OFS_PARM0 + 3 + i];
	// 1 for a recognized VF_*, 0 for an unknown one.
	vm->globals[OFS_RETURN] = CSQC_Client_SetViewProperty (prop, words, args) ? 1 : 0;
}
static void csqc_renderscene (void)
{
	// #304 renderscene performs the frame's 3D rendering (like FTE
	// PF_R_RenderScene -> R_RenderView). Outside takeover it is a no-op (the engine draws).
	if (CSQC_Client_SceneActive ())
		CSQC_Client_RenderScene ();
}

/*
 float(vector position, string text, vector size, vector rgb,
       float alpha, float drawflag) drawstring = #326

 Dual signature of FTE PF_CL_drawcolouredstring: with `argc >= 6` the extended form
 (rgb=P3, alpha=P4, flag=P5), otherwise legacy DP (white color, alpha=P3, flag=P4 at
 argc>=5). Draws the string in the ezquake 2D overlay. PR1 params are 3 words per
 argument: pos=0..2, text=3, size=6..8, rgb=9..11 (0..1 -> bytes), alpha=12,
 drawflag=15. Scale is size.x/8 (ezq font is uniform-only; size.y is not applied).
 Color is set explicitly (Draw_SetColor) - independent of scr_coloredText.
 */
static void csqc_drawstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int r, gg, b;
	float scale, alpha;
	char *s;
	if (!vm)
		return;
	g = vm->globals;
	s = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 3]);
	if (!s)
	{
		// null string -> -1.
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	if (vm->argc >= 6)
	{
		r = (int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f);
		gg = (int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f);
		b = (int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f);
		alpha = g[OFS_PARM0 + 12];
	}
	else
	{
		// legacy-DP: (pos, text, size, alpha [, flag]) - white color.
		r = gg = b = 255;
		alpha = g[OFS_PARM0 + 9];
	}
	// size.x -> scale (8px cell); 0 => 1.
	// drawfontscale: common x-multiplier of the text.
	scale = ((g[OFS_PARM0 + 6] > 0) ? g[OFS_PARM0 + 6] / 8.0f : 1) * CSQC_Client_DrawFontScaleX (vm);
	CSQC_Client_DrawText (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1], s, r, gg, b, alpha, scale);
}

/*
 float(float stnum) getstati = #330
 FTE PF_cs_getstat_int: G_INT(OFS_RETURN) = stats[stnum] - raw int bits are returned
 (not a float). The numeric value is read by the module via getstatf (#331).
 0..31 - cl.stats, 32..255 - ext storage (CSQC_Client_GetStatInt).
*/
static void csqc_getstati (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	*(int *)&vm->globals[OFS_RETURN] = CSQC_Client_GetStatInt ((int)vm->globals[OFS_PARM0]);
}

/*
 float(float stnum, optional float firstbit, optional float bitcount) getstatf = #331
 FTE parity (PF_cs_getstat_float):
   without extra args - the float stat value (statsf, from stat wire 79);
   with firstbit/bitcount - bit extraction from the int stat value (getstatbits).
*/
static void csqc_getstatf (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int stnum;
	if (!vm)
		return;
	stnum = (int)vm->globals[OFS_PARM0];
	if (stnum < 0 || stnum >= MAX_EXTENDED_CL_STATS)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	if (vm->argc > 1)
	{
		// Exact int (not the float path): large ints lose low bits in float32
		// (FTE reads stats[] as int).
		int val = CSQC_Client_GetStatInt (stnum);
		int first = (int)vm->globals[OFS_PARM1];
		int count = (vm->argc > 2) ? (int)vm->globals[OFS_PARM2] : 1;
		if (first < 0)
			first = 0;
		if (count < 0)
			count = 0;
		if (count > 31)	// FTE does (1<<count); clamp to avoid UB
			count = 31;
		vm->globals[OFS_RETURN] = (float)((((unsigned int)val) & (((1u << count) - 1u) << first)) >> first);
	}
	else
		vm->globals[OFS_RETURN] = CSQC_Client_GetStatFloat (stnum);
}

/*
 string(float firststnum) getstats = #332
 FTE parity (PF_cs_getstat_string under PEXT_CSQC): statsstr[stnum], received via
 stat wire 78 (svc_fte_updatestatstring). The legacy packed-int variant (4 int stats,
 old engines) is not implemented - our client is always on FTE_PEXT_CSQC (the module
 only runs on it).
*/
static void csqc_getstats (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr ((char *)CSQC_Client_GetStatString ((int)vm->globals[OFS_PARM0]));
}

// ----------------------------------------------------------------------------
// 2D graphics. Param layout is 3-word cells from OFS_PARM0. draw* return values
// follow FTE (pr_menu.c): drawpic 1/0 (pic found), drawfill/drawsubpic 1,
// drawcharacter 1 (0 for null char, -1).

/*
float(vector position, float character, vector size, vector rgb, float alpha,
     optional float drawflag) drawcharacter = #320
*/
static void csqc_drawcharacter (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	float scale;
	if (!vm)
		return;
	g = vm->globals;
	scale = ((g[OFS_PARM0 + 6] > 0) ? g[OFS_PARM0 + 6] / 8.0f : 1) * CSQC_Client_DrawFontScaleX (vm);
	CSQC_Client_DrawCharacter (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1], (int)g[OFS_PARM0 + 3],
		(int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 12], scale);
	// null char -> -1, otherwise 1.
	vm->globals[OFS_RETURN] = (g[OFS_PARM0 + 3] == 0) ? -1 : 1;
}

/*
float(vector position, string pic, vector size, vector rgb, float alpha,
     optional float drawflag) drawpic = #322
 Returns pic found (1) / not found (0), FTE PF_CL_drawpic.
*/
static void csqc_drawpic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	char *name;
	if (!vm)
		return;
	g = vm->globals;
	name = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 3]);
	vm->globals[OFS_RETURN] = CSQC_Client_DrawPic (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1],
		g[OFS_PARM0 + 6], g[OFS_PARM0 + 7], name,
		(int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 12]) ? 1 : 0;
}

/*
void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb,
     float alpha, optional float drawflag) drawsubpic = #328
 Returns always 1 (FTE PF_CL_drawsubpic).
*/
static void csqc_drawsubpic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	char *name;
	if (!vm)
		return;
	g = vm->globals;
	name = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 6]);
	CSQC_Client_DrawSubPic (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1],
		g[OFS_PARM0 + 3], g[OFS_PARM0 + 4], name,
		g[OFS_PARM0 + 9], g[OFS_PARM0 + 10], g[OFS_PARM0 + 12], g[OFS_PARM0 + 13],
		(int)(bound (0, g[OFS_PARM0 + 15], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 16], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 17], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 18]);
	vm->globals[OFS_RETURN] = 1;
}

/*
float(vector position, vector size, vector rgb, float alpha,
     optional float drawflag) drawfill = #323
 Returns always 1 (FTE PF_CL_drawfill).
*/
static void csqc_drawfill (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	CSQC_Client_DrawFill (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1],
		g[OFS_PARM0 + 3], g[OFS_PARM0 + 4],
		(int)(bound (0, g[OFS_PARM0 + 6], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 7], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 8], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 9]);
	vm->globals[OFS_RETURN] = 1;
}

/*
void(float width, vector pos1, vector pos2, vector rgb, float alpha,
     optional float drawflag) drawline = #315
FTE PF_CL_drawline: width is ignored (hairline).
*/
static void csqc_drawline (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	CSQC_Client_DrawLine (g[OFS_PARM0 + 3], g[OFS_PARM0 + 4], g[OFS_PARM0 + 6], g[OFS_PARM0 + 7],
		1,
		(int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 12]);
}

/*
float(string text, float usecolours, optional vector fontsize) stringwidth = #327
*/
static void csqc_stringwidth (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *text;
	if (!vm)
		return;
	text = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	// drawfontscale: multiply size.x before passing (inside it is /8) - metric is
	// consistent with drawstring.
	vm->globals[OFS_RETURN] = CSQC_Client_StringWidth (text ? text : "",
		vm->globals[OFS_PARM0 + 3] != 0,
		vm->globals[OFS_PARM0 + 6] * CSQC_Client_DrawFontScaleX (vm));
}

/*
 string(string name, optional float trywad) precache_pic = #317
 Returns name if the pic loaded (trywad is ignored), otherwise "".
*/
static void csqc_precache_pic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	if (name && CSQC_Client_PrecachePic (name))
		CSQCVM_SetRetStr (name);
	else
		CSQCVM_SetRetStr ("");
}

/*
 Additional 2D graphics (#316/#318/#319/#321/#324/#325/#329). FTE reference - the
 pr_menu.c PF_CL_* builtins (iscachedpic, drawgetimagesize, freepic, drawrawstring,
 drawsetcliparea, drawresetcliparea, drawrotpic_dp). Implementations are in
 CSQC_Client_* (csqc_client.c/.h).
*/

/* float(string name) iscachedpic = #316 - pic already in cache (no load) */
static void csqc_iscachedpic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	vm->globals[OFS_RETURN] = (name && CSQC_Client_IsCachedPic (name)) ? 1 : 0;
}

/* vector(string picname) drawgetimagesize = #318 - (w,h,0) of the loaded pic */
static void csqc_drawgetimagesize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	float w = 0, h = 0;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	if (name && CSQC_Client_PicSize (name, &w, &h))
	{
		vm->globals[OFS_RETURN + 0] = w;
		vm->globals[OFS_RETURN + 1] = h;
		vm->globals[OFS_RETURN + 2] = 0;
	}
	else
	{
		vm->globals[OFS_RETURN + 0] = 0;
		vm->globals[OFS_RETURN + 1] = 0;
		vm->globals[OFS_RETURN + 2] = 0;
	}
}

/* void(string name) freepic = #319 - no-op (FTE: empty body; pics are shared) */
static void csqc_freepic (void)
{
	/* no-op (FTE parity: shader/pic may be used elsewhere) */
}

/*
 void(vector position, string text, vector scale, vector rgb, float alpha,
      optional float flag) drawrawstring = #321
 Layout like drawstring #326; "raw" text (no &c-prefix/parsing).
*/
static void csqc_drawrawstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int r, gg, b;
	float scale;
	char *s;
	if (!vm)
		return;
	g = vm->globals;
	s = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 3]);
	if (!s)
		return;
	r = (int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f);
	gg = (int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f);
	b = (int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f);
	scale = ((g[OFS_PARM0 + 6] > 0) ? g[OFS_PARM0 + 6] / 8.0f : 1) * CSQC_Client_DrawFontScaleX (vm);
	CSQC_Client_DrawRawText (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1], s,
		r, gg, b, g[OFS_PARM0 + 12], scale);
}

/* void(float x, float y, float width, float height) drawsetcliparea = #324 */
static void csqc_drawsetcliparea (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_SetClipArea (vm->globals[OFS_PARM0 + 0], vm->globals[OFS_PARM0 + 3],
		vm->globals[OFS_PARM0 + 6], vm->globals[OFS_PARM0 + 9]);
	vm->globals[OFS_RETURN] = 1;
}

/* void() drawresetcliparea = #325 */
static void csqc_drawresetcliparea (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_ResetClipArea ();
	vm->globals[OFS_RETURN] = 1;
}

/*
void(vector pivot, string picname, vector size, vector mins, float angle,
     vector rgb, float alpha, optional float flag) drawrotpic_dp = #329
 No-op: the ezq 2D path has no GL rotation of a textured quad.
*/
static void csqc_drawrotpic_dp (void)
{
	/* no-op (no GL rotation of a 2D quad in ezq) */
}

/*
 string(string fmt, ...) sprintf = #627
 Mini QC formatter with width/flags/precision - a subset of FTE PF_sprintf_internal.
 Parses %[flags][width][.precision]conv; flags - 0 + ' ' #; width/precision are
 literal only (no '*'/'%$'). Conversions d i u x X c s f g e + v ('x y z');
 %o/%p/%S/%E/%F/%G/%V/length are unsupported. Unknown conversion is verbatim.
 Arguments come from the param slots (OFS_PARM0 + 3*n), bounded by vm->argc; strings
 via CSQC_Client_GetString with offset validation.
*/
static char *csqc_sprintf_put_int (char *f, int v)
{
	char tmp[12];
	int n = 0;

	if (v <= 0)
	{
		*f++ = '0';
		return f;
	}
	while (v > 0 && n < (int)sizeof (tmp))
	{
		tmp[n++] = (char)('0' + v % 10);
		v /= 10;
	}
	while (n > 0)
		*f++ = tmp[--n];
	return f;
}

static void csqc_sprintf (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[2048];
	char tmp[512];
	const char *fmt, *p;
	int pn = 1;		// argument number after fmt (base = OFS_PARM0 + pn*3)
	size_t o = 0;

	if (!vm)
		return;
	fmt = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	if (!fmt)
		fmt = "";

	for (p = fmt; *p && o < sizeof (buf) - 1; p++)
	{
		char conv, formatbuf[32], *f;
		const char *dir;
		int flags = 0, width = -1, prec = -1, haswidth = 0;

		if (*p != '%')
		{
			buf[o++] = *p;
			continue;
		}
		dir = p;
		p++;
		if (*p == '%')
		{
			buf[o++] = '%';
			continue;
		}

		// flags: # alternate, 0 zeropad, - left, ' ' space, + sign
		for (;; p++)
		{
			if (*p == '#' || *p == '0' || *p == '-' || *p == ' ' || *p == '+')
				flags |= 1 << (*p == '#' ? 0 : *p == '0' ? 1 : *p == '-' ? 2 : *p == ' ' ? 3 : 4);
			else
				break;
		}
		// width - literal number only (no '*')
		if (*p >= '1' && *p <= '9')
		{
			int nd = 0;
			width = 0;
			haswidth = 1;
			while (*p >= '0' && *p <= '9' && nd < 9)
				width = width * 10 + (*p++ - '0'), nd++;
			while (*p >= '0' && *p <= '9')
				p++;
			if (width > 2047)
				width = 2047;
		}
		// precision - literal number only
		if (*p == '.')
		{
			int nd = 0;
			p++;
			prec = 0;
			while (*p >= '0' && *p <= '9' && nd < 9)
				prec = prec * 10 + (*p++ - '0'), nd++;
			while (*p >= '0' && *p <= '9')
				p++;
			if (prec > 2047)
				prec = 2047;
		}
		conv = *p;
		if (!conv)
			break;

		// build the C format: %[#][0][-][ ][+][width][.prec]
		f = formatbuf;
		*f++ = '%';
		if (conv != 's' && conv != 'c' && (flags & 1))
			*f++ = '#';
		if (flags & 2) *f++ = '0';
		if (flags & 4) *f++ = '-';
		if (flags & 8) *f++ = ' ';
		if (flags & 16) *f++ = '+';
		if (haswidth)
			f = csqc_sprintf_put_int (f, width);
		if (prec >= 0)
		{
			*f++ = '.';
			f = csqc_sprintf_put_int (f, prec);
		}

		if (conv == 'v')
		{
			double x = 0, y = 0, z = 0;
			char vfmt[192];
			*f++ = 'g';
			*f = 0;
			if (pn < vm->argc)
			{
				x = (double)vm->globals[OFS_PARM0 + pn * 3];
				y = (double)vm->globals[OFS_PARM0 + pn * 3 + 1];
				z = (double)vm->globals[OFS_PARM0 + pn * 3 + 2];
			}
			pn++;
			snprintf (vfmt, sizeof (vfmt), "%s %s %s", formatbuf, formatbuf, formatbuf);
			snprintf (tmp, sizeof (tmp), vfmt, x, y, z);
		}
		else if (conv == 's')
		{
			const char *s = "";
			*f++ = 's';
			*f = 0;
			if (pn < vm->argc)
			{
				int off = *(int *)&vm->globals[OFS_PARM0 + pn * 3];
				char *gs = CSQC_Client_GetString (vm, off);
				// Validation: a non-negative offset must lie in the module's string
				// area; negative offsets are temporary tables.
				if (gs && off >= 0 && (unsigned)off >= (unsigned)vm->progs->numstrings)
					gs = NULL;
				if (gs)
					s = gs;
			}
			pn++;
			snprintf (tmp, sizeof (tmp), formatbuf, s);
		}
		else
		{
			int argok = (pn < vm->argc);
			float av = argok ? vm->globals[OFS_PARM0 + pn * 3] : 0;

			switch (conv)
			{
			case 'd':
			case 'i':
				*f++ = 'd';
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (int)av);
				pn++;
				break;
			case 'u':
				*f++ = 'u';
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (unsigned)(int)av);
				pn++;
				break;
			case 'x':
			case 'X':
				*f++ = conv;
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (unsigned)(int)av);
				pn++;
				break;
			case 'c':
				*f++ = 'c';
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (int)av);
				pn++;
				break;
			case 'f':
			case 'e':
			case 'g':
				*f++ = conv;
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (double)av);
				pn++;
				break;
			default:
				// unknown conversion - verbatim directive
				{
					const char *q;
					for (q = dir; q <= p && o < sizeof (buf) - 1; q++)
						buf[o++] = *q;
				}
				continue;
			}
		}

		{
			size_t l = strlen (tmp);
			if (o + l >= sizeof (buf))
				l = sizeof (buf) - 1 - o;
			memcpy (buf + o, tmp, l);
			o += l;
		}
	}
	buf[o] = 0;
	CSQCVM_SetRetStr (buf);
}

/*
 void(string evname, string evargs, ...) sendevent = #359
 Writes the real clcfte_qcrequest (81) wire contract:
   [byte 81] then up to 6 args "[byte type][value]", then [byte 0 (ev_void
   terminator)] and [string evname].
 Types: 's'=1 ev_string+string, 'f'=2 ev_float+float, 'v'=3 ev_vector+3 floats,
 'i'=8 ev_integer+long (raw bits from the float slot), 'e'=4 ev_entity+entity
 (arena edict -> server number from the .entnum field; invalid/freed -> world(0);
 wire like MSG_WriteEntity). Unknown character (incl. '\0') - break (the rest is
 not sent; 'u'/'F'/'I'/'p' are not used by the module). Guards: active connection +
 negotiated FTE_PEXT_CSQC + cl_pext_csqc (a server without CSQC would otherwise
 drop the client).
 Seat byte: the wire writes 200+csqc_playerseat only when seat>0; ezq is single-seat
 (seat==0, no splitscreen/playerview) -> the byte is not written.
*/
#define CSQC_EV_VOID	0
#define CSQC_EV_STRING	1
#define CSQC_EV_FLOAT	2
#define CSQC_EV_VECTOR	3
#define CSQC_EV_ENTITY	4
#define CSQC_EV_INTEGER	8

/* Entity wire like MSG_WriteEntity. An invalid number (incl. negative) degenerates
   to world(0) - the module must not crash the client; the Host_EndGame on
   entnum>MAX_EDICTS is not ported here. */
static void csqc_sendevent_write_entity (int entnum)
{
	if (entnum < 0)
		entnum = 0;
	if (entnum >= 0x8000)
	{
		MSG_WriteShort (&cls.netchan.message, (entnum >> 8) | 0x8000);
		MSG_WriteByte (&cls.netchan.message, entnum & 0xff);
	}
	else
		MSG_WriteShort (&cls.netchan.message, entnum);
}

static void csqc_sendevent (void)
{
	extern cvar_t cl_pext_csqc;
	pr1vm_t *vm = CSQCVM_Active ();
	const char *evname, *argtypes;
	char c;
	int i;

	if (!vm)
		return;
	// sendevent requires only an active connection, not ca_active.
	// CSQC_Client_ConnectCheck calls CSQC_Init from CL_MakeActive before
	// cls.state = ca_active, so events from CSQC_Init must reach the server.
	if (!cls.state)
		return;
	if (!cl_pext_csqc.value)
		return;
#ifdef PROTOCOL_VERSION_FTE
	if (!(cls.fteprotocolextensions & FTE_PEXT_CSQC))
		return;
#endif

	evname = CSQCVM_Str (OFS_PARM0);
	argtypes = CSQCVM_Str (OFS_PARM1);
	if (!evname || !argtypes)
		return;

	MSG_WriteByte (&cls.netchan.message, clcfte_qcrequest);

	for (i = 0; i < 6; i++)
	{
		int base = OFS_PARM2 + i * 3;
		c = argtypes[i];
		if (c == 's')
		{
			char *s = CSQC_Client_GetString (vm, *(int *)&vm->globals[base]);
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_STRING);
			MSG_WriteString (&cls.netchan.message, s ? s : "");
		}
		else if (c == 'f')
		{
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_FLOAT);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base]);
		}
		else if (c == 'v')
		{
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_VECTOR);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base + 0]);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base + 1]);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base + 2]);
		}
		else if (c == 'i')
		{
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_INTEGER);
			MSG_WriteLong (&cls.netchan.message, *(int *)&vm->globals[base]);
		}
		else if (c == 'e')
		{
			// arena edict -> server number from the .entnum field;
			// invalid/empty -> world(0).
			int slot = csqc_ent_of (vm, base);
			float *f = (slot > 0 && CSQC_Client_EntUsed (slot))
				? csqc_ent_field (vm, slot, "entnum") : NULL;
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_ENTITY);
			csqc_sendevent_write_entity (f ? (int)(*f) : 0);
		}
		else
			break;
	}

	MSG_WriteByte (&cls.netchan.message, CSQC_EV_VOID);
	MSG_WriteString (&cls.netchan.message, evname);
}

/*
 The read* minimum: they read from the current network message. Remove is no longer
 stream-based - the module takes identity from self.entnum.
*/
static void csqc_readbyte (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadByte is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadByte ();
}

/*
 float() readchar = #361
 Signed byte from the current network message.
*/
static void csqc_readchar (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3426-3434)
	{
		CSQC_Client_Abort ("PF_ReadChar is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadChar ();
}

static void csqc_readshort (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadShort is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadShort ();
}

static void csqc_readlong (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadLong is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadLong ();
}

/*
 float() readcoord = #364 / string() readstring = #366
 Coordinate/string from the current network message (needed for the cgamepacket echo
 and the typed 76-payload).
*/
static void csqc_readcoord (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadCoord is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadCoord ();
}

/*
 float() readangle = #365
 Angle from the current network message.
*/
static void csqc_readangle (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadAngle is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadAngle ();
}

static void csqc_readstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadString is not valid at this time");
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	s = MSG_ReadString ();
	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_RETURN], s);
}

/*
 float() readfloat = #367
 Full float from the current network message.
*/
static void csqc_readfloat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadFloat is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadFloat ();
}

/*
 PEXT2_REPLACEMENTDELTAS is not in qwprot - a local constant under #ifndef. The
 qwprot submodule and CL_SupportedFTEExtensions2 are left untouched: the client does
 not announce the bit, so this branch is forward-compat only.
*/
#ifndef PEXT2_REPLACEMENTDELTAS
#define PEXT2_REPLACEMENTDELTAS 0x00000008
#endif

/*
 Edict number from the current stream - PEXT2-aware, FTE parity (MSGCL_ReadEntity):
 with PEXT2_REPLACEMENTDELTAS - big entity (short; bit 0x8000 -> (hi & 0x7fff)<<8 |
 byte), otherwise a plain short (unsigned short).
*/
static int CSQC_Client_ReadEntityNum (void)
{
	if (cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
	{
		int num = MSG_ReadShort ();
		if (num & 0x8000)
			num = ((num & 0x7fff) << 8) | MSG_ReadByte ();
		return num;
	}
	return (unsigned short)MSG_ReadShort ();
}

static void csqc_readentitynum (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// read not valid at this time
	{
		CSQC_Client_Abort ("PF_ReadEntityNum is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = CSQC_Client_ReadEntityNum ();
}

/*
 Input/interface builtins. #340/#341 operate in the QC/DP key domain - input/output
 is translated via CSQC_Client_QCToKeynum/CSQC_Client_KeynumToQC.

 string(float keynum) keynumtostring = #340
 QC code -> key name (Key_KeynumToString for the ezq internal keynum).
 CSQCVM_SetRetStr deep-copies into the instance temp ring.
*/
static void csqc_keynumtostring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr (Key_KeynumToString (CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0])));
}

/*
 float(string keyname) stringtokeynum = #341
 Key name -> QC code; empty string/no such name -> -1
 (Key_StringToKeynum gives -1, KeynumToQC keeps -1).
*/
static void csqc_stringtokeynum (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = name ? CSQC_Client_KeynumToQC (Key_StringToKeynum (name)) : -1;
}

/*
 float() isdemo = #349
 0 - not demo; 1 - normal demo; 2 - MVD/QTV viewing (cls.mvdplayback: 1=MVD, 2=QTV).
 Matches FTE PF_cl_playingdemo semantics (DPB_NONE=0, DPB_MVD=2, otherwise 1).
*/
static void csqc_isdemo (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = cls.mvdplayback ? 2 : (cls.demoplayback ? 1 : 0);
}

/*
 string(string key) serverkey = #354
 FTE PF_cl_serverkey_internal: synthetic keys ip/maxplayers/protocol/dlstate, then
 fallback to serverinfo. Deviation: protocol is a simplified string (no QW/ZQ/FTE
 parsing), dlstate is only the percent (FTE is a multi-field string).
*/
static void csqc_serverkey (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *key = CSQCVM_Str (OFS_PARM0);
	char buf[64];
	const char *v = NULL;
	if (!vm)
		return;
	if (!key)
		key = "";
	if (!strcmp (key, "ip"))				// ip key
	{
		if (cls.demoplayback)
			v = cls.demoname;
		else
			v = NET_AdrToString (cls.netchan.remote_address);
	}
	else if (!strcmp (key, "maxplayers"))			// maxplayers key
		snprintf (buf, sizeof (buf), "%d", cl.sv_maxclients), v = buf;
	else if (!strcmp (key, "protocol"))			// protocol key (simplified)
		v = cls.demoplayback ? "QuakeWorld demo" : "QuakeWorld";
	else if (!strcmp (key, "dlstate"))			// dlstate key (simplified)
	{
		if (!cls.download)
			v = "";
		else
			snprintf (buf, sizeof (buf), "%d", (int)cls.downloadpercent), v = buf;
	}
	else
		v = Info_ValueForKey (cl.serverinfo, key);
	CSQCVM_SetRetStr ((char *)(v ? v : ""));
}

/*
 string(float playernum, string keyname) getplayerkeyvalue = #348
 Player scoreboard/userinfo values (cl.players[pnum]). Numeric keys
 frags/ping/userid/spectator - via formatting; name/team/topcolor/bottomcolor and
 others - from userinfo (like FTE PF_cs_getplayerkey_internal). Empty slot / outside
 [0, MAX_CLIENTS) -> "" (empty string). Deviation: pnum<0 (scoreboard fragsort index)
 is unsupported -> "".
*/
/*
 void(float sens) setsensitivityscaler = #346
 Temporary mouse sensitivity multiplier (zoom analog of FTE
 PF_cs_setsensitivityscaler). The value is applied to sensitivity by in_sdl2.c.
*/
static void csqc_setsensitivityscaler (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_SetSensitivityScale (vm->globals[OFS_PARM0]);
}

/*
 float(float inputsequencenum) getinputstate = #345
 Fills the input_* globals from the sent usercmd history. seq is a mirror of
 cls.netchan.outgoing_sequence; valid range (servercommandframe, clientcommandframe]
 (the engine does not check the range), with a paused guard. Returns 0 if seq is
 outside the history ring (64) or paused. clientcommandframe is the last sent seq,
 so #345(clientcommandframe) returns the live pending frame outside CSQC_Input_Frame.
*/
static void csqc_getinputstate (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	unsigned int seq;
	if (!vm)
		return;
	seq = (unsigned int)vm->globals[OFS_PARM0];
	vm->globals[OFS_RETURN] = CSQC_Client_ApplyInput (seq);
}

/*
 void(entity ent) runstandardplayerphysics = #347
 FTE semantics - PM_PlayerMove over the input_* globals (the module calls
 getinputstate(seq) before), solid set world+ent+players, ent fields
 (.mins/.maxs/.gravity/.pmove_flags/.flags), writing .flags/.pmove_flags plus the
 deprecated pmove_org/vel/onground. Deviations (missing fields in ezq pmove) are in
 csqc_client.c. The entity argument is raw int bits (csqc_ent_of): a float read
 would turn the spawn() value (int bits N*edict_size) into 0.
*/
static void csqc_runstandardplayerphysics (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int entnum;
	if (!vm)
		return;
	entnum = csqc_ent_of (vm, OFS_PARM0);
	CSQC_Client_RunPlayerPhysics (entnum);
}

/*
 entity(float entnum) edict_num = #459
 Entity value by number - raw int bits N*edict_size (like self in SetEntityContext
 and spawn); FTE PF_edict_for_num writes G_INT(OFS_RETURN), not a float. Outside the
 arena range -> 0 (world).
*/
static void csqc_edict_num (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int entnum;
	if (!vm)
		return;
	entnum = (int)vm->globals[OFS_PARM0];
	if (entnum < 0 || vm->max_edicts <= 0 || entnum >= vm->max_edicts)
	{
		csqc_ret_entity (vm, 0);
		return;
	}
	csqc_ret_entity (vm, entnum);
}

/*
 string buffers #460-469 (DP). Storage lives in csqc_client (deep-copy); the
 builtins are thin wrappers (ABI i*3, string returns via CSQCVM_SetRetStr).
*/
static int CSQCVM_ArgInt (int idx)
{
	pr1vm_t *vm = CSQCVM_Active ();
	return (vm && vm->argc > idx) ? (int)vm->globals[OFS_PARM0 + idx * 3] : 0;
}

static char *CSQCVM_ArgStr (int idx)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm || vm->argc <= idx)
		return NULL;
	return CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + idx * 3]);
}

// strbuf() buf_create = #460
static void csqc_buf_create (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		vm->globals[OFS_RETURN] = CSQC_Client_BufCreate ();
}

// void(strbuf bufhandle) buf_del = #461
static void csqc_buf_del (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufDel (CSQCVM_ArgInt (0));
}

// float(strbuf bufhandle) buf_getsize = #462
static void csqc_buf_getsize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		vm->globals[OFS_RETURN] = CSQC_Client_BufGetSize (CSQCVM_ArgInt (0));
}

// void(strbuf bufhandle_from, strbuf bufhandle_to) buf_copy = #463
static void csqc_buf_copy (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufCopy (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1));
}

// void(strbuf bufhandle, float sortprefixlen, float backward) buf_sort = #464
static void csqc_buf_sort (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufSort (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1), CSQCVM_ArgInt (2) != 0);
}

// string(strbuf bufhandle, string glue) buf_implode = #465
static void csqc_buf_implode (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[8192];
	char *glue;
	if (!vm)
		return;
	glue = CSQCVM_ArgStr (1);
	if (!CSQC_Client_BufImplode (CSQCVM_ArgInt (0), glue ? glue : "", buf, sizeof (buf)))
		buf[0] = 0;
	CSQCVM_SetRetStr (buf);
}

// string(strbuf bufhandle, float string_index) bufstr_get = #466
static void csqc_bufstr_get (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[2048];
	if (!vm)
		return;
	if (!CSQC_Client_BufGet (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1), buf, sizeof (buf)))
		buf[0] = 0;
	CSQCVM_SetRetStr (buf);
}

// void(strbuf bufhandle, float string_index, string str) bufstr_set = #467
static void csqc_bufstr_set (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	if (!vm)
		return;
	s = CSQCVM_ArgStr (2);
	CSQC_Client_BufSet (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1), s ? s : "");
}

// float(strbuf bufhandle, string str, float order) bufstr_add = #468
static void csqc_bufstr_add (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	if (!vm)
		return;
	s = CSQCVM_ArgStr (1);
	vm->globals[OFS_RETURN] = CSQC_Client_BufAdd (CSQCVM_ArgInt (0), s ? s : "",
		CSQCVM_ArgInt (2));
}

// void(strbuf bufhandle, float string_index) bufstr_free = #469
static void csqc_bufstr_free (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufFree (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1));
}

/*
 void(string soundname, optional float channel, optional float volume) localsound = #177
 FTE PF_cl_localsound = S_LocalSound2(name, chan, vol): local sound.
 ezquake: S_LocalSoundWithVol (precache by name, channel local -1).
 Deviation: channel is ignored; vol 0..1 (default 1).
*/
static void csqc_localsound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	char *name;
	float vol;
	if (!vm)
		return;
	g = vm->globals;
	name = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0]);
	vol = (vm->argc > 2) ? g[OFS_PARM0 + 6] : 1;
	if (name && name[0])
		S_LocalSoundWithVol (name, vol);
}

/*
 float(vector org, float radius, vector lightcolours, optional float style, ...)
 dynamiclight_add = #305
 ezquake: CL_AllocDlight + fields (lt_custom, color=lightcolours*255, radius, 0.1s).
 style/cubemap/pflags have no analog (no-op). Return is the slot index.
*/
static void csqc_dynamiclight_add (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	dlight_t *dl;
	if (!vm)
		return;
	g = vm->globals;
	dl = CL_AllocDlight (0);
	if (!dl)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	VectorCopy (&g[OFS_PARM0], dl->origin);
	dl->radius = g[OFS_PARM0 + 3];
	dl->die = cl.time + 0.1f;
	dl->type = lt_custom;
	dl->bubble = 0;
	dl->color[0] = (byte)bound (0, g[OFS_PARM0 + 6] * 255.0f, 255);
	dl->color[1] = (byte)bound (0, g[OFS_PARM0 + 7] * 255.0f, 255);
	dl->color[2] = (byte)bound (0, g[OFS_PARM0 + 8] * 255.0f, 255);
	vm->globals[OFS_RETURN] = (float)(int)(dl - cl_dlights) + 1;
}

/*
 Particles #335-337. ezquake has no effect-name registry - a mini registry
 (name -> palette colour/base count). #335 returns an effect handle (idx+1, -1 if
 none); #336/#337 spawn R_RunParticleEffect (approximation).
*/
typedef struct { const char *name; int color; int count; } csqc_peffect_t;
static const csqc_peffect_t s_peffects[] = {
	{ "blood",		73, 24 },
	{ "explosion",	226, 32 },
	{ "spark",		0,  10 },
	{ "gunshot",	0,  16 },
	{ "smoke",		0,  8 },
};
#define CSQC_NPEFFECTS	((int)(sizeof (s_peffects) / sizeof (s_peffects[0])))

// float(string effectname) particleeffectnum = #335
static void csqc_particleeffectnum (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	int i;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	for (i = 0; name && i < CSQC_NPEFFECTS; i++)
		if (!strcmp (name, s_peffects[i].name))
		{
			vm->globals[OFS_RETURN] = i + 1;
			return;
		}
	vm->globals[OFS_RETURN] = -1;
}

static const csqc_peffect_t *csqc_peffect_byhandle (int h)
{
	return (h >= 1 && h <= CSQC_NPEFFECTS) ? &s_peffects[h - 1] : NULL;
}

// void(float effectnum, entity ent, vector start, vector end) trailparticles = #336
static void csqc_trailparticles (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	const csqc_peffect_t *e;
	vec3_t d;
	float len, step;
	int i, n;
	if (!vm)
		return;
	g = vm->globals;
	e = csqc_peffect_byhandle ((int)g[OFS_PARM0]);
	if (!e)
		return;
	d[0] = g[OFS_PARM0 + 9] - g[OFS_PARM0 + 6];
	d[1] = g[OFS_PARM0 + 10] - g[OFS_PARM0 + 7];
	d[2] = g[OFS_PARM0 + 11] - g[OFS_PARM0 + 8];
	len = sqrt (d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
	n = (len > 8) ? bound (1, (int)(len / 8.0f), 40) : 1;
	for (i = 0; i <= n; i++)
	{
		vec3_t p;
		step = (n) ? (float)i / n : 0;
		p[0] = g[OFS_PARM0 + 6] + d[0] * step;
		p[1] = g[OFS_PARM0 + 7] + d[1] * step;
		p[2] = g[OFS_PARM0 + 8] + d[2] * step;
		R_RunParticleEffect (p, vec3_origin, e->color, 1);
	}
}

// void(float effectnum, vector origin, optional vector dir, optional float count)
// pointparticles = #337
static void csqc_pointparticles (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	const csqc_peffect_t *e;
	int count;
	vec3_t dir;
	if (!vm)
		return;
	g = vm->globals;
	e = csqc_peffect_byhandle ((int)g[OFS_PARM0]);
	if (!e)
		return;
	count = (vm->argc > 3) ? (int)g[OFS_PARM0 + 9] : e->count;
	dir[0] = (vm->argc > 2) ? g[OFS_PARM0 + 6] : 0;
	dir[1] = (vm->argc > 2) ? g[OFS_PARM0 + 7] : 0;
	dir[2] = (vm->argc > 2) ? g[OFS_PARM0 + 8] : 0;
	R_RunParticleEffect (&g[OFS_PARM0 + 3], dir, e->color, bound (1, count, 4096));
}

/*
 The approximated te_* group (particles/explosions/spikes #405-427, except #426).
 Approximation via R_RunParticleEffect/R_ParticleExplosion/R_BlobExplosion/
 CL_ExplosionSprite (palette colours, bbox/directions approximate). Unmappable ones
 (#426 etc.) are not registered.
*/
static unsigned int s_te_rnd = 1;
static float csqc_te_rand01 (void)
{
	s_te_rnd = s_te_rnd * 1103515245u + 12345u;
	return (float)((s_te_rnd >> 8) & 0xffff) / 65535.0f;
}

static void csqc_te_bbox_effect (float *mn, float *mx, float *vel, int how, int color)
{
	int i;
	for (i = 0; i < how && i < 512; i++)
	{
		vec3_t p;
		p[0] = mn[0] + (mx[0] - mn[0]) * csqc_te_rand01 ();
		p[1] = mn[1] + (mx[1] - mn[1]) * csqc_te_rand01 ();
		p[2] = mn[2] + (mx[2] - mn[2]) * csqc_te_rand01 ();
		R_RunParticleEffect (p, vel, color, 1);
	}
}

// #405 te_blood
static void csqc_te_blood (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	R_RunParticleEffect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], 73,
		bound (1, (int)g[OFS_PARM0 + 6], 4096));
}
// #406 te_bloodshower
static void csqc_te_bloodshower (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	vec3_t vel = { 0, 0, -100 };
	if (!vm)
		return;
	g = vm->globals;
	csqc_te_bbox_effect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], vel,
		bound (1, (int)g[OFS_PARM0 + 7], 4096), 73);
}
// #407 te_explosionrgb
static void csqc_te_explosionrgb (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_ParticleExplosion (&vm->globals[OFS_PARM0]);
}
// #408 te_particlecube
static void csqc_te_particlecube (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	csqc_te_bbox_effect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], &g[OFS_PARM0 + 6],
		bound (1, (int)g[OFS_PARM0 + 9], 4096), (int)g[OFS_PARM0 + 12]);
}
// #409/#410 rain/snow
static void csqc_te_rain (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	csqc_te_bbox_effect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], &g[OFS_PARM0 + 6],
		bound (1, (int)g[OFS_PARM0 + 9], 4096), (int)g[OFS_PARM0 + 12]);
}
// #411 te_spark
static void csqc_te_spark (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	R_RunParticleEffect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], 0,
		bound (1, (int)g[OFS_PARM0 + 6], 4096));
}
// #412-415 quad effects (org in w0) - white particles
static void csqc_te_quad (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	R_RunParticleEffect (&vm->globals[OFS_PARM0], vec3_origin, 255, 12);
}
// #416/#417 flash
static void csqc_te_smallflash (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CL_ExplosionSprite (&vm->globals[OFS_PARM0]);
}
static void csqc_te_customflash (void)
{
	csqc_te_smallflash ();
}
// #418 te_gunshot
static void csqc_te_gunshot (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int count;
	if (!vm)
		return;
	g = vm->globals;
	count = (vm->argc > 1) ? (int)g[OFS_PARM0 + 3] : 20;
	R_RunParticleEffect (&g[OFS_PARM0], vec3_origin, 0, bound (1, count, 4096));
}
// #419/420/423/424 spikes - coloured particles
static void csqc_te_spike_color (int color)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_RunParticleEffect (&vm->globals[OFS_PARM0], vec3_origin, color, 10);
}
static void csqc_te_spike (void) { csqc_te_spike_color (255); }
static void csqc_te_superspike (void) { csqc_te_spike_color (255); }
static void csqc_te_wizspike (void) { csqc_te_spike_color (0); }
static void csqc_te_knightspike (void) { csqc_te_spike_color (0); }
// #421/#427 explosion
static void csqc_te_explosion (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_ParticleExplosion (&vm->globals[OFS_PARM0]);
}
static void csqc_te_explosion2 (void) { csqc_te_explosion (); }
// #422 tarexplosion / #425 lavasplash
static void csqc_te_tarexplosion (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CL_ExplosionSprite (&vm->globals[OFS_PARM0]);
}
static void csqc_te_lavasplash (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_BlobExplosion (&vm->globals[OFS_PARM0]);
}

/*
 Beams #428-431 (te_lightning1/2/3, te_beam): CL_CreateBeam(type, ent, start, end).
 own-entity -> entnum (raw int bits via csqc_ent_of). Approximation. If the effect
 model is missing (e.g. progs/beam.mdl) - Con_Printf warning and no-op, without a
 host error/disconnect (Mod_CustomModel(crash=false); CL_CreateBeam itself loads with
 crash=true and drops the connection).
*/
static custom_model_id_t CSQC_BeamModelId (int type)
{
	switch (type)
	{
	case 1: return custom_model_bolt;
	case 2: return custom_model_bolt2;
	case 3: return custom_model_bolt3;
	case 4:
	default: return custom_model_beam;
	}
}

static const char *CSQC_BeamModelName (int type)
{
	switch (type)
	{
	case 1: return "progs/bolt.mdl";
	case 2: return "progs/bolt2.mdl";
	case 3: return "progs/bolt3.mdl";
	case 4:
	default: return "progs/beam.mdl";
	}
}

static void csqc_te_beam_type (int type)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int entnum;
	if (!vm)
		return;
	g = vm->globals;
	if (!Mod_CustomModel (CSQC_BeamModelId (type), false))
	{
		Con_Printf ("CSQC: te_beam type %d: %s not found - effect skipped\n",
			type, CSQC_BeamModelName (type));
		return;
	}
	entnum = csqc_ent_of (vm, OFS_PARM0);
	CL_CreateBeam (type, entnum, &g[OFS_PARM0 + 3], &g[OFS_PARM0 + 6]);
}
static void csqc_te_lightning1 (void) { csqc_te_beam_type (1); }
static void csqc_te_lightning2 (void) { csqc_te_beam_type (2); }
static void csqc_te_lightning3 (void) { csqc_te_beam_type (3); }
static void csqc_te_beam (void) { csqc_te_beam_type (4); }

static void csqc_getplayerkeyvalue (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int pnum;
	char *key;
	char buf[64];
	player_info_t *pi;
	char *v = NULL;

	if (!vm)
		return;
	pnum = (int)vm->globals[OFS_PARM0];
	key = CSQCVM_Str (OFS_PARM1);
	if (pnum < 0 || pnum >= MAX_CLIENTS || !key || !key[0])
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	pi = &cl.players[pnum];
	if (!pi->name[0])
	{
		CSQCVM_SetRetStr ("");	// empty slot - no player
		return;
	}
	// FTE PF_cs_getplayerkey_internal keys: to the existing
	// frags/ping/userid/spectator/name + userinfo add pl,
	// activetime, ignored, viewentity, topcolor_rgb/bottomcolor_rgb.
	if (!strcmp (key, "frags"))
		snprintf (buf, sizeof (buf), "%d", pi->frags), v = buf;
	else if (!strcmp (key, "ping"))
		snprintf (buf, sizeof (buf), "%d", pi->ping), v = buf;
	else if (!strcmp (key, "userid"))
		snprintf (buf, sizeof (buf), "%d", pi->userid), v = buf;
	else if (!strcmp (key, "spectator"))
		snprintf (buf, sizeof (buf), "%d", (int)pi->spectator), v = buf;
	else if (!strcmp (key, "pl"))				// packet loss
		snprintf (buf, sizeof (buf), "%d", (int)pi->pl), v = buf;
	else if (!strcmp (key, "activetime"))			// realtime - entertime
		snprintf (buf, sizeof (buf), "%f", cls.realtime - pi->entertime), v = buf;
	else if (!strcmp (key, "ignored"))
		snprintf (buf, sizeof (buf), "%d", (int)pi->ignored), v = buf;
	else if (!strcmp (key, "viewentity"))			// DP-compat: pnum+1
		snprintf (buf, sizeof (buf), "%d", pnum + 1), v = buf;
	else if (!strcmp (key, "topcolor_rgb") || !strcmp (key, "bottomcolor_rgb"))
	{
		// palette colour (real_*); DP-RGB (col>=16) is unsupported.
		// Format is "'r g b'" (%g).
		int col = (key[0] == 't') ? (int)pi->real_topcolor : (int)pi->real_bottomcolor;
		if (col < 16)
		{
			int pal = Sbar_ColorForMap (col);
			snprintf (buf, sizeof (buf), "'%g %g %g'",
				host_basepal[pal * 3 + 0] / 255.0,
				host_basepal[pal * 3 + 1] / 255.0,
				host_basepal[pal * 3 + 2] / 255.0);
			v = buf;
		}
	}
	else if (!strcmp (key, "name"))
		v = pi->name;
	else
		v = Info_ValueForKey (pi->userinfo, key);	// team/topcolor/bottomcolor/...
	CSQCVM_SetRetStr (v ? v : "");
}

/*
 void(float usecursor, optional string cursorimage, optional vector hotspot,
      optional float scale) setcursormode = #343
 FTE: releases/grabs the mouse and configures the cursor. ezquake: full implementation
 - while usecursor=1 and the module is active in-game, the mouse is not given to the
 OS cursor and SCR_DrawCursor draws the module's cursor (image/hotspot/scale); at 0
 the mouse is returned to the engine.
 ABI/scale like FTE: hotspot is a vector (w6..8), scale is read from hotspot.z (w8),
 a separate float arg (w9) is ignored; scale <= 0 -> native cursor size.
*/
static void csqc_setcursormode (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	CSQC_Client_SetCursorMode (g[OFS_PARM0] != 0,
		vm->argc > 1 ? CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM1]) : NULL,
		vm->argc > 2 ? g[OFS_PARM2]     : 0,
		vm->argc > 2 ? g[OFS_PARM2 + 1] : 0,
		vm->argc > 2 ? g[OFS_PARM2 + 2] : 0);
}

/*
 vector() getmousepos = #344
 Position of the CSQC cursor in ezquake 2D overlay coordinates (GetCursorPos); z = 0.
 FTE: with an absolute cursor - position, otherwise deltas with reset. Deviation:
 always the position (the module is in absolute mode).
*/
static void csqc_getmousepos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float x = 0, y = 0;
	if (!vm)
		return;
	CSQC_Client_GetCursorPos (&x, &y);
	vm->globals[OFS_RETURN] = x;
	vm->globals[OFS_RETURN + 1] = y;
	vm->globals[OFS_RETURN + 2] = 0;
}

/*
 float(float x, float y) pow = #97 (client handler; the server PF_pow is static;
 pure math)
*/
static void csqc_pow (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = pow (vm->globals[OFS_PARM0], vm->globals[OFS_PARM1]);
}

/*
 vector() randomvec = #91 (client handler, like PF_randomvec)
*/
static void csqc_randomvec (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *r;
	if (!vm)
		return;
	r = &vm->globals[OFS_RETURN];
	do {
		r[0] = (rand () & 0x7fff) * (2.0 / 0x7fff) - 1.0;
		r[1] = (rand () & 0x7fff) * (2.0 / 0x7fff) - 1.0;
		r[2] = (rand () & 0x7fff) * (2.0 / 0x7fff) - 1.0;
	} while (DotProduct (r, r) >= 1);
}

/*
 Math builtins. Bodies are pure float/string functions without edict/engine state
 (the exceptions #494 crc16 / #519 gettimef are noted in their bodies).
*/

/*
float(float x) asin = #471; float(float x) acos = #472
float(float x) atan = #473; float(float a, float b) atan2 = #474; float(float x) tan = #475
*/
static void csqc_asin (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = asin (vm->globals[OFS_PARM0]);
}
static void csqc_acos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = acos (vm->globals[OFS_PARM0]);
}
static void csqc_atan (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = atan (vm->globals[OFS_PARM0]);
}
static void csqc_atan2 (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = atan2 (vm->globals[OFS_PARM0], vm->globals[OFS_PARM1]);
}
static void csqc_tan (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = tan (vm->globals[OFS_PARM0]);
}

/*
 float(float x, optional float base) log = #532
 log(x); with a 2nd argument - log_base(x) = log(x)/log(base) (PF_Logarithm).
*/
static void csqc_log (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	double r;
	if (!vm)
		return;
	r = log (vm->globals[OFS_PARM0]);
	if (vm->argc > 1)
		r /= log (vm->globals[OFS_PARM1]);
	vm->globals[OFS_RETURN] = (float)r;
}

/*
float(float v) anglemod = #102 - in [0,360) (PF_anglemod).
*/
static void csqc_anglemod (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float v;
	if (!vm)
		return;
	v = vm->globals[OFS_PARM0];
	while (v >= 360)
		v -= 360;
	while (v < 0)
		v += 360;
	vm->globals[OFS_RETURN] = v;
}

/*
float(float a, float n) mod = #245 - a - n*(int)(a/n); division by 0 -> warning + 0.
*/
static void csqc_mod (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float a, n;
	if (!vm)
		return;
	a = vm->globals[OFS_PARM0];
	n = vm->globals[OFS_PARM1];
	if (n == 0)
	{
		Con_Printf ("CSQC mod: mod by zero\n");
		vm->globals[OFS_RETURN] = 0;
	}
	else
		vm->globals[OFS_RETURN] = a - n * (float)(int)(a / n);
}

/*
 float(float number, float quantity) bitshift = #218
 quantity<0 -> shift right by -quantity, otherwise left (PF_bitshift).
*/
static void csqc_bitshift (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int mask, shift;
	if (!vm)
		return;
	mask = (int)vm->globals[OFS_PARM0];
	shift = (int)vm->globals[OFS_PARM1];
	if (shift < 0)
		mask >>= -shift;
	else
		mask <<= shift;
	vm->globals[OFS_RETURN] = mask;
}

/*
 float(float insensitive, string str, ...) crc16 = #494
 CRC16 (CCITT, poly 0x1021, init/xor 0xffff/0x0000 - same as ezq CRC_*);
 insensitive -> lowercase letters before counting. Strings from the 1st argument are
 concatenated. Return is the crc value.
*/
static void csqc_crc16 (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	unsigned short crc;
	char buf[4096];
	int i, insens, len = 0;
	const char *s;

	if (!vm)
		return;
	insens = (int)vm->globals[OFS_PARM0];
	buf[0] = 0;
	for (i = 1; i < vm->argc; i++)
	{
		s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (s)
			len += snprintf (buf + len, sizeof (buf) - len, "%s", s);
		if (len >= (int)sizeof (buf) - 1)
			break;
	}
	CRC_Init (&crc);
	for (i = 0; i < len; i++)
		CRC_ProcessByte (&crc, insens ? tolower ((int)(unsigned char)buf[i]) : (unsigned char)buf[i]);
	vm->globals[OFS_RETURN] = CRC_Value (crc);
}

/*
 float(optional float timer) gettimef = #519 - time in seconds (float).
 FTE PF_gettimed: timer 0/none - realtime (frame), 1 - wall-clock with ms precision,
 5 - sim-time (cl.time); otherwise -> realtime.
 Deviation: mode0 = cls.realtime (scales with cl_demospeed).
 sys.h is not included (dllfunction_t conflict after quakedef) - local prototype.
*/
double Sys_DoubleTime (void);
static void csqc_gettimef (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int timer;
	if (!vm)
		return;
	timer = (vm->argc > 0) ? (int)vm->globals[OFS_PARM0] : 0;
	switch (timer)
	{
	case 1:
		vm->globals[OFS_RETURN] = (float)((double)(long long)(Sys_DoubleTime () * 1000.0) / 1000.0);
		break;
	case 5:
		vm->globals[OFS_RETURN] = (float)cl.time;
		break;
	default:
		vm->globals[OFS_RETURN] = (float)cls.realtime;
		break;
	}
}

/*
 int/hex conversions (#259-262). ABI: int-typed params/returns are passed as 4 bytes
 of a bit value (like strings), not as a float - read/write via *(int *)&globals[...].
*/

/*
string(int input) itos = #260 - "%d".
*/
static void csqc_itos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[32];
	if (!vm)
		return;
	snprintf (buf, sizeof (buf), "%d", *(int *)&vm->globals[OFS_PARM0]);
	CSQCVM_SetRetStr (buf);
}

/*
int(string input) stoi = #259 - atoi (returns int bits).
*/
static void csqc_stoi (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	*(int *)&vm->globals[OFS_RETURN] = atoi (s ? s : "");
}

/*
string(int input) htos = #262 - "%08x" (always 8 chars, no prefix).
*/
static void csqc_htos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[32];
	if (!vm)
		return;
	snprintf (buf, sizeof (buf), "%08x", *(unsigned int *)&vm->globals[OFS_PARM0]);
	CSQCVM_SetRetStr (buf);
}

/*
int(string input) stoh = #261 - strtoul base 16 (returns int bits).
*/
static void csqc_stoh (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	*(int *)&vm->globals[OFS_RETURN] = (int)strtoul (s ? s : "", NULL, 16);
}

/*
 cvar metadata (#482/#495/#518). Deviations: ezq cvar_t has no description (#518 is
 always "" and HASDESCRIPTION is not set); there is no FTE PRIVATE analog
 (NOTFROMSERVER/NOUNSAFEEXPAND), so it is not set.
*/

/*
 string(string cvarname) cvar_defstring = #482
 FTE: FindOrGet (creates if missing), returns the default value ("" if none).
 ezq: Cvar_Find / Cvar_Create, returns cvar_t.defaultvalue.
 Deviation: for registercvar(name,value) FTE returns "" (a PF_registercvar quirk);
 ezq honestly returns defaultvalue, since mimicking the quirk would break the module's
 defaults (csqc_vm etc.).
*/
static void csqc_cvar_defstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	cvar_t *v;
	if (!vm)
		return;
	v = (name && name[0]) ? Cvar_Find (name) : NULL;
	if (!v && name && name[0])
		v = Cvar_Create (name, "", 0);
	CSQCVM_SetRetStr ((v && v->defaultvalue) ? v->defaultvalue : "");
}

/*
 float(string cvarname) cvar_type = #495
 FTE flags: EXISTS=1 SAVED=2 PRIVATE=4 ENGINE=8 HASDESCRIPTION=16 READONLY=32.
 Mapping to ezq: SAVED = CVAR_ARCHIVE|CVAR_USER_ARCHIVE; ENGINE = not
 CVAR_USER_CREATED/MOD_CREATED; READONLY = CVAR_ROM. The cvar need not exist.
*/
static void csqc_cvar_type (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	cvar_t *v;
	int ret = 0;
	if (!vm)
		return;
	v = (name && name[0]) ? Cvar_Find (name) : NULL;
	if (v)
	{
		ret |= 1;	// EXISTS
		if (v->flags & (CVAR_ARCHIVE | CVAR_USER_ARCHIVE))
			ret |= 2;	// SAVED
		if (v->flags & CVAR_ROM)
			ret |= 32;	// READONLY
		if (!(v->flags & (CVAR_USER_CREATED | CVAR_MOD_CREATED)))
			ret |= 8;	// ENGINE
	}
	vm->globals[OFS_RETURN] = ret;
}

/*
 string(string cvarname) cvar_description = #518
 FTE returns the cvar description; ezq cvar_t has none - always "".
*/
static void csqc_cvar_description (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr ("");
}

/*
 Simple string builtins. ASCII semantics (the FTE UTF-8 branches are out of scope).
 String returns use the temp ring (SetRetStr).
*/

/*
 float(string str, optional float index) str2chr = #222 - char code; index<0 - from
 the end; outside [0,len) -> 0.
*/
static void csqc_str2chr (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	int len, idx;
	if (!vm)
		return;
	s = s ? s : "";
	len = strlen (s);
	idx = (vm->argc > 1) ? (int)vm->globals[OFS_PARM1] : 0;
	if (idx < 0)
		idx = len + idx;
	if (idx < 0 || idx >= len)
		vm->globals[OFS_RETURN] = 0;
	else
		vm->globals[OFS_RETURN] = (float)(unsigned char)s[idx];
}

/*
string(float chr, ...) chr2str = #223 - string from char codes (each argument).
*/
static void csqc_chr2str (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[128];
	int i, n = 0;
	if (!vm)
		return;
	for (i = 0; i < vm->argc && i < 64 && n < (int)sizeof (buf) - 1; i++)
		buf[n++] = (char)(int)vm->globals[OFS_PARM0 + i * 3];
	buf[n] = 0;
	CSQCVM_SetRetStr (buf);
}

/*
 string(float pad, string str1, ...) strpad = #225 - pads the concatenated strings to
 width |pad|: pad>0 - right, pad<0 - left (PF_strpad).
*/
static void csqc_strpad (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[4096], *d;
	int pad, len = 0, i;
	const char *s;

	if (!vm)
		return;
	pad = (int)vm->globals[OFS_PARM0];
	buf[0] = 0;
	d = buf;
	for (i = 1; i < vm->argc; i++)
	{
		s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (!s)
			continue;
		len = strlen (s);
		if (d - buf + len >= (int)sizeof (buf) - 1)
			len = (int)sizeof (buf) - 1 - (d - buf);
		memcpy (d, s, len);
		d += len;
		*d = 0;
	}
	if (pad < 0)
	{
		pad = -pad - (int)(d - buf);
		if (pad > (int)sizeof (buf) - 1 - (d - buf))
			pad = (int)sizeof (buf) - 1 - (d - buf);
		if (pad > 0)
		{
			memmove (buf + pad, buf, (size_t)(d - buf) + 1);
			memset (buf, ' ', pad);
		}
	}
	else
	{
		pad -= (int)(d - buf);
		if (pad < 0)
			pad = 0;
		if (d - buf + pad >= (int)sizeof (buf) - 1)
			pad = (int)sizeof (buf) - 1 - (d - buf);
		memset (d, ' ', pad);
		d[pad] = 0;
	}
	CSQCVM_SetRetStr (buf);
}

/*
 string(infostring old, string key, string value) infoadd = #226
 string(infostring info, string key) infoget = #227
 QW infostring \key\value; FTE Info_*. Info_SetValueForStarKey has no prototype in
 common.h - local extern.
*/
void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize);
static void csqc_infoadd (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *info, *key, *val;
	char buf[1024];
	if (!vm)
		return;
	info = CSQCVM_Str (OFS_PARM0);
	key = CSQCVM_Str (OFS_PARM1);
	val = CSQCVM_VarString (2);
	strlcpy (buf, info ? info : "", sizeof (buf));
	Info_SetValueForStarKey (buf, key ? key : "", val ? val : "", sizeof (buf));
	CSQCVM_SetRetStr (buf);
}
static void csqc_infoget (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *info = CSQCVM_Str (OFS_PARM0);
	char *key = CSQCVM_Str (OFS_PARM1);
	if (!vm)
		return;
	CSQCVM_SetRetStr (Info_ValueForKey (info ? info : "", key ? key : ""));
}

/*
 float(string s1, string s2, optional float len, optional float s1ofs, optional float s2ofs)
 strcmp/strncmp = #228 (PF_strncmp)
*/
static void csqc_strncmp (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *a, *b;
	int len, aofs, bofs;
	int alen, blen;
	if (!vm)
		return;
	a = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	b = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	if (vm->argc <= 2)
	{
		vm->globals[OFS_RETURN] = strcmp (a, b);
		return;
	}
	len = (int)vm->globals[OFS_PARM2];
	aofs = (vm->argc > 3) ? (int)vm->globals[OFS_PARM3] : 0;
	bofs = (vm->argc > 4) ? (int)vm->globals[OFS_PARM4] : 0;
	alen = strlen (a);
	blen = strlen (b);
	if (aofs < 0 || (aofs && aofs > alen))
		aofs = alen;
	if (bofs < 0 || (bofs && bofs > blen))
		bofs = blen;
	vm->globals[OFS_RETURN] = strncmp (a + aofs, b, len);
}

/*
float(string s1, string s2) strcasecmp = #229
 float(string s1, string s2, float len, optional float s1ofs, optional float s2ofs)
 strncasecmp = #230 (PF_strncasecmp)
*/
static void csqc_strncasecmp (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *a, *b;
	int len, aofs, bofs;
	int alen, blen;
	if (!vm)
		return;
	a = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	b = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	if (vm->argc <= 2)
	{
		vm->globals[OFS_RETURN] = strcasecmp (a, b);
		return;
	}
	len = (int)vm->globals[OFS_PARM2];
	aofs = (vm->argc > 3) ? (int)vm->globals[OFS_PARM3] : 0;
	bofs = (vm->argc > 4) ? (int)vm->globals[OFS_PARM4] : 0;
	alen = strlen (a);
	blen = strlen (b);
	if (aofs < 0 || (aofs && aofs > alen))
		aofs = alen;
	if (bofs < 0 || (bofs && bofs > blen))
		bofs = blen;
	vm->globals[OFS_RETURN] = strncasecmp (a + aofs, b + bofs, len);
}

/*
string(string s) strtolower = #480 / strtoupper = #481 - ASCII (FTE is unicode).
*/
static void csqc_strtolower (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s = CSQCVM_Str (OFS_PARM0);
	char buf[8192];
	int i, n;
	if (!vm)
		return;
	n = strlen (s ? s : "");
	if (n >= (int)sizeof (buf))
		n = (int)sizeof (buf) - 1;
	for (i = 0; i < n; i++)
		buf[i] = tolower ((int)(unsigned char)s[i]);
	buf[n] = 0;
	CSQCVM_SetRetStr (buf);
}
static void csqc_strtoupper (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s = CSQCVM_Str (OFS_PARM0);
	char buf[8192];
	int i, n;
	if (!vm)
		return;
	n = strlen (s ? s : "");
	if (n >= (int)sizeof (buf))
		n = (int)sizeof (buf) - 1;
	for (i = 0; i < n; i++)
		buf[i] = toupper ((int)(unsigned char)s[i]);
	buf[n] = 0;
	CSQCVM_SetRetStr (buf);
}

/*
 Colour markup stripping for #476 strlennocol / #477 strdecolorize.
 FTE parity: `&cRGB` (valid 3-hex) / `&r` and the `^` colour/state codes: q3 colours
 `^0-9`, `^xRRGGBB`, `^&XX` (extended FG/BG), states `^b/^d/^m/^a/^h/^s/^r`, escape
 `^^`, plus the FTE behaviour for unknown/dangling `^`. Out of scope: links `^[..^]`,
 charset `u8:`/`k8:`, `^Uxxxx`/`^{xxxx}`. Return is the result length in bytes;
 out==NULL is allowed (count only).
*/
static int CSQCVM_IsExtCode (char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == '-';
}

static int CSQCVM_StripColor (const char *in, char *out, size_t outsize)
{
	size_t n = 0;

	if (!in)
		in = "";
	for (; *in; in++)
	{
		if (in[0] == '&' && in[1] == 'c'
			&& HexToInt (in[2]) >= 0 && HexToInt (in[3]) >= 0 && HexToInt (in[4]) >= 0)
		{
			in += 4;
			continue;
		}
		if (in[0] == '&' && in[1] == 'r')
		{
			in += 1;
			continue;
		}
		if (in[0] == '^')
		{
			char c1 = in[1];

			if (c1 >= '0' && c1 <= '9')			// ^0..^9 q3 colour
			{
				in += 1;
				continue;
			}
			if (c1 == 'x')						// ^xRGB valid -> strip 5; invalid -> strip "^x"
			{
				if (HexToInt (in[2]) >= 0 && HexToInt (in[3]) >= 0 && HexToInt (in[4]) >= 0)
					in += 4;
				else
					in += 1;
				continue;
			}
			if (c1 == '&')						// ^&XX extended FG/BG
			{
				if (CSQCVM_IsExtCode (in[2]) && CSQCVM_IsExtCode (in[3]))
				{
					in += 3;
					continue;
				}
				// invalid: '^' stays a literal, '&' is handled on the next iteration
			}
			else if (c1 == 'b' || c1 == 'd' || c1 == 'm' || c1 == 'a'
				|| c1 == 'h' || c1 == 's' || c1 == 'r')
			{
				in += 1;
				continue;
			}
			else if (c1 == '^')					// ^^ -> ^
			{
				if (out && outsize && n + 1 < outsize)
					out[n] = '^';
				n++;
				in += 1;
				continue;
			}
			// unknown / end / out-of-scope: '^' literal, next char processed normally
		}
		if (out && outsize && n + 1 < outsize)
			out[n] = *in;
		n++;
	}
	if (out && outsize)
		out[(n < outsize) ? n : outsize - 1] = 0;
	return (int)n;
}

/*
 float(string s) strlennocol = #476 - FTE parity (PF_strlennocol): string length
 without colour codes.
*/
static void csqc_strlennocol (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (float)CSQCVM_StripColor (CSQCVM_Str (OFS_PARM0), NULL, 0);
}

/*
 string(string s) strdecolorize = #477 - FTE parity (PF_strdecolorize): string with
 colour codes removed.
*/
static void csqc_strdecolorize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[8192];

	if (!vm)
		return;
	CSQCVM_StripColor (CSQCVM_Str (OFS_PARM0), buf, sizeof (buf));
	CSQCVM_SetRetStr (buf);
}

/*
 string(string input, string token) instr = #206 - FTE parity (PF_instr): first
 occurrence of the variadic tail (from param 1) in input; returns the remaining
 substring from the match position, or "" if not found.
*/
static void csqc_instr (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s1, *s2, *sub;

	if (!vm)
		return;
	s1 = CSQCVM_Str (OFS_PARM0);
	s2 = CSQCVM_VarString (1);
	if (!s1 || !s2)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	sub = strstr (s1, s2);
	CSQCVM_SetRetStr (sub ? (char *)sub : "");
}

/*
 string(string search, string replace, string subject) strreplace = #484
 string(string search, string replace, string subject) strireplace = #485
 (4096-byte buffer, non-recursive replacement).
*/
static void csqc_strreplace (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *search, *replace, *sub;
	char buf[4096], *result = buf;
	int searchlen, replacelen;
	if (!vm)
		return;
	search = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	replace = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	sub = CSQCVM_Str (OFS_PARM2) ? CSQCVM_Str (OFS_PARM2) : "";
	searchlen = strlen (search);
	replacelen = strlen (replace);
	if (searchlen)
	{
		while (*sub && result < buf + sizeof (buf) - replacelen - 2)
		{
			if (!strncmp (sub, search, searchlen))
			{
				sub += searchlen;
				memcpy (result, replace, replacelen);
				result += replacelen;
			}
			else
				*result++ = *sub++;
		}
		*result = 0;
	}
	else
		strlcpy (buf, sub, sizeof (buf));
	CSQCVM_SetRetStr (buf);
}
static void csqc_strireplace (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *search, *replace, *sub;
	char buf[4096], *result = buf;
	int searchlen, replacelen;
	if (!vm)
		return;
	search = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	replace = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	sub = CSQCVM_Str (OFS_PARM2) ? CSQCVM_Str (OFS_PARM2) : "";
	searchlen = strlen (search);
	replacelen = strlen (replace);
	if (searchlen)
	{
		while (*sub && result < buf + sizeof (buf) - replacelen - 2)
		{
			if (!strncasecmp (sub, search, searchlen))
			{
				sub += searchlen;
				memcpy (result, replace, replacelen);
				result += replacelen;
			}
			else
				*result++ = *sub++;
		}
		*result = 0;
	}
	else
		strlcpy (buf, sub, sizeof (buf));
	CSQCVM_SetRetStr (buf);
}

/*
 cvar/exec/error handlers. Client handlers (strings via CSQC_Client_GetString, no
 server mirrors). #28 coredump / #31 eprint are entity debugging.
*/

/*
 void(string err, ...) error = #10 - FTE parity (PF_error):
 developer!=0 - non-fatal (print; FTE debug-breaks, we print and continue);
 developer==0 - fatal (abort via host_error). Deviation: no FTE stack dump, no
 self/edict dump.
*/
static void csqc_error (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_VarString (0);
	if (!vm)
		return;
	Con_Printf ("CSQC error: %s\n", s ? s : "");
	if (!developer.value && vm->host_error)
		vm->host_error (vm, s ? s : "error");
}

/*
 void(string err, ...) objerror = #11
 FTE parity (PF_objerror): fatality depends on the developer cvar. developer!=0 -
 non-fatal: print to console, the module continues (FTE debug_trace is not
 reproducible). developer==0 - fatal: print + client disconnect (CSQC_Client_Abort).
 Deviation from FTE: no self/edict dump (ED_Print) before the message.
*/
static void csqc_objerror (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_VarString (0);
	if (!vm)
		return;
	Con_Printf ("CSQC objerror: %s\n", s ? s : "");
	if (!developer.value)
		CSQC_Client_Abort (s ? s : "objerror");
}

/*
 void(string str) localcmd = #46
 Executes the string as an engine command - like server-sent commands (svc_stufftext):
 via cbuf_svc with the cl_remote_capabilities filter, not the unrestricted cbuf_main.
 Deviation from FTE: FTE uses RESTRICT_INSECURE (exec-level), ezq an allowlist.
*/
static void csqc_localcmd (void)
{
	char *s = CSQCVM_VarString (0);
	if (s && s[0])
		Cbuf_AddTextEx (&cbuf_svc, s);
}

/*
 void(string cvarname, string value) cvar_set = #72
 Like the server PF_cvar_set: if the cvar is missing - a warning.
*/
/*
 void(string cvarname, string value) cvar_set = #72 - FTE parity (PF_cvar_set): FTE
 uses FindOrGet - a missing cvar is created. Deviation: the CVAR_NOTFROMSERVER guard
 is not reproducible (client).
*/
static void csqc_cvar_set (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name, *val;
	cvar_t *var;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	val = CSQCVM_Str (OFS_PARM1);
	if (!name || !name[0])
		return;
	var = Cvar_Find (name);
	if (!var)
		var = Cvar_Create (name, "", 0);	// FindOrGet: create if missing
	if (var)
		Cvar_Set (var, val ? val : "");
}

/*
 float(string name, string value) registercvar = #93
 Creates an engine variable (shared namespace) if it does not exist; returns 1/0.
*/
static void csqc_registercvar (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name, *value;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	value = CSQCVM_Str (OFS_PARM1);
	if (!name || !name[0])
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	if (Cvar_Find (name))
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	Cvar_Create (name, value ? value : "", 0);
	vm->globals[OFS_RETURN] = 1;
}

/*
 float(string ext) checkextension = #99
 FTE parity: PF_checkextension looks up the name in the extension list and returns
 whether it is supported. Here a static mirror table of the subset implemented in
 ezq (a no-op is not advertised; the effect tables with stubs are the exception).
 Names use the exact FTE spellings, including the leading '_' of _DP_TE_*. EXT_CSQC is
 a special case (protocol, not builtins): 1 when the CSQC session is active.
 Deliberately NOT advertised (FTE advertises, ezq is a no-op): FRIK_FILE,
 FTE_QC_INTCONV, _DP_TE_FLAMEJET/_DP_TE_PLASMABURN, DP_QC_STRINGBUFFERS,
 DP_QC_FS_SEARCH(_PACKFILE), DP_QC_GETSURFACE, DP_QC_FINDCHAIN(FLOAT)/FINDFLAGS/
 FINDCHAINFLAGS, DP_QC_COPYENTITY, DP_QC_WHICHPACK, DP_QC_URI_ESCAPE,
 KRIMZON_SV_PARSECLIENTCOMMAND.
*/
static void csqc_checkextension (void)
{
	static const char *supported[] = {
		"DP_QC_SINCOSSQRTPOW",
		"DP_QC_MINMAXBOUND",
		"DP_QC_RANDOMVEC",
		"DP_REGISTERCVAR",
		"DP_QC_CVAR_STRING",
		"DP_QC_CVAR_DEFSTRING",
		"DP_QC_CVAR_TYPE",
		"DP_QC_EDICT_NUM",
		"DP_QC_ETOS",
		"DP_QC_FINDFLOAT",
		"DP_QC_STRFTIME",
		"DP_QC_STRREPLACE",
		"DP_QC_TOKENIZEBYSEPARATOR",
		"DP_QC_SPRINTF",
		"DP_QC_STRING_CASE_FUNCTIONS",
		"DP_QC_STRINGCOLORFUNCTIONS",
		"DP_QC_CRC16",
		"DP_QC_ASINACOSATANATAN2TAN",
		"DP_QC_CHANGEPITCH",
		"DP_QC_VECTORVECTORS",
		"DP_QC_TRACEBOX",
		"DP_QC_ENTITYDATA",
		"EXT_BITSHIFT",
		"DP_TE_BLOOD",
		"_DP_TE_BLOODSHOWER",
		"DP_TE_EXPLOSIONRGB",
		"DP_TE_PARTICLECUBE",
		"DP_TE_PARTICLERAIN",
		"DP_TE_PARTICLESNOW",
		"DP_TE_SPARK",
		"DP_TE_SMALLFLASH",
		"DP_TE_CUSTOMFLASH",
		"_DP_TE_QUADEFFECTS1",
		"FTE_STRINGS",
		"DP_TE_STANDARDEFFECTBUILTINS",
		"FTE_TE_STANDARDEFFECTBUILTINS",
		"FTE_QC_DIGEST_SHA1",	// #639 digest_hex (SHA1 only; SHA224/384/512 not implemented)
		NULL
	};
	pr1vm_t *vm = CSQCVM_Active ();
	char *ext;
	int i;
	if (!vm)
		return;
	ext = CSQCVM_Str (OFS_PARM0);
	vm->globals[OFS_RETURN] = 0;
	if (!ext)
		return;
	if (!strcmp (ext, "EXT_CSQC"))		// protocol, not builtins
	{
		vm->globals[OFS_RETURN] = (cls.fteprotocolextensions & FTE_PEXT_CSQC) ? 1 : 0;
		return;
	}
	for (i = 0; supported[i]; i++)
		if (!strcmp (supported[i], ext))
		{
			vm->globals[OFS_RETURN] = 1;
			return;
		}
}

/*
 void() calltimeofday = #231
 Like the server PF_calltimeofday: if the module has a "timeofday" function - fill
 its args (sec/min/hour/day/mon/year) with local time and call it.
*/
static void csqc_calltimeofday (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	dfunction_t *f;
	time_t t;
	struct tm *ltm;
	if (!vm)
		return;
	f = PR1VM_FindFunction (vm, "timeofday");
	if (!f)
		return;
	t = time (NULL);
	ltm = localtime (&t);
	if (!ltm)
		return;
	vm->globals[OFS_PARM0] = (float)ltm->tm_sec;
	vm->globals[OFS_PARM1] = (float)ltm->tm_min;
	vm->globals[OFS_PARM2] = (float)ltm->tm_hour;
	vm->globals[OFS_PARM3] = (float)ltm->tm_mday;
	vm->globals[OFS_PARM4] = (float)(ltm->tm_mon + 1);
	vm->globals[OFS_PARM5] = (float)(ltm->tm_year + 1900);
	PR1VM_ExecuteProgram (vm, (func_t)(f - vm->functions));
}

/*
 String/conversion handlers. Client handlers on per-instance strings
 (PR1VM_Get/SetString). #118/#119: no GC - strzone deep-copies into the per-instance
 ring (PR1VM_ClientSetString), strunzone is a no-op.
*/

/*
string(vector v) vtos = #27 - FTE parity (PF_vtos): "'%f %f %f'".
*/
static void csqc_vtos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[64];
	if (!vm)
		return;
	snprintf (buf, sizeof (buf), "'%f %f %f'",
		vm->globals[OFS_PARM0], vm->globals[OFS_PARM0 + 1], vm->globals[OFS_PARM0 + 2]);
	CSQCVM_SetRetStr (buf);
}

/*
float(string s) stof = #81
*/
static void csqc_stof (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (float)strtod (s ? s : "", NULL);
}

/*
float(string s) strlen = #114
*/
static void csqc_strlen (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = s ? (float)strlen (s) : 0;
}

/*
 string(string s, float start, float count) substring = #116 - FTE parity
 (PF_substring): negative start/length from the end, strict clamp
 (start>=slen || length<=0 -> "").
*/
static void csqc_substring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	char buf[2048];
	int start, len, l;
	if (!vm)
		return;
	if (!s)
		s = "";
	start = (int)vm->globals[OFS_PARM1];
	len = (int)vm->globals[OFS_PARM2];
	l = strlen (s);
	if (start < 0)
		start = l + start;
	if (len < 0)
		len = l - start + (len + 1);
	if (start < 0)
		start = 0;
	if (start >= l || len <= 0 || l == 0)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	l -= start;
	if (len > l)
		len = l;
	strlcpy (buf, s + start, (size_t)len + 1);
	CSQCVM_SetRetStr (buf);
}

/*
 vector(string s) stov = #117 - FTE (PF_VarString(0), parsed from the vtos format;
 `'`-stop). The prototype is not variadic, so extra args from the module are
 unreachable.
*/
static void csqc_stov (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *p = CSQCVM_VarString (0);
	double v[3];
	int i;
	char *end;
	if (!vm)
		return;
	if (!p)
		p = "";
	for (i = 0; i < 3; i++)
	{
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\'' || *p == '"')
			p++;
		v[i] = strtod (p, &end);
		if (end == p)
		{
			v[i] = 0;
			while (*p && *p != ' ')
				p++;
		}
		else
			p = end;
	}
	vm->globals[OFS_RETURN] = (float)v[0];
	vm->globals[OFS_RETURN + 1] = (float)v[1];
	vm->globals[OFS_RETURN + 2] = (float)v[2];
}

/*
 string(string s) strzone = #118
 Deviation (no GC on the client): deep-copy into the per-instance ring
 (PR1VM_ClientSetString).
*/
static void csqc_strzone (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	CSQCVM_SetRetStr (s ? s : "");
}

/*
 void(string s) strunzone = #119
 Deviation: no-op (no GC/persistent pool on the client).
*/
static void csqc_strunzone (void)
{
	/* no-op (classic ring without GC) */
}

/*
string(string varname) cvar_string = #448
*/
static void csqc_cvar_string (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	cvar_t *var;
	if (!vm)
		return;
	if (!name)
		name = "";
	// FTE returns latched_string if the value is latched. PF_Cvar_FindOrGet
	// (autocreate) and the CVAR_NOUNSAFEEXPAND flag are FTE-specific (not in ezq).
	var = Cvar_Find (name);
	CSQCVM_SetRetStr (var ? (var->latchedString ? var->latchedString : var->string) : "");
}

/*
 Client subsystems. Best-effort on the ezquake client APIs.
*/

/*
 void() breakpoint = #6
 Debugger: no-op on the client (the engine has no QC debugger).
*/
static void csqc_breakpoint (void)
{
	/* no-op */
}

/*
 void(entity e, float chan, string samp, float vol, float atten) sound = #8
 Deviation: no positional sound at the entity (no origin field without the arena);
 precache + local playback like #177 (volume vol).
*/
static void csqc_sound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM2);
	float vol = 1;
	if (!vm)
		return;
	if (vm->argc > 3)
		vol = vm->globals[OFS_PARM0 + 9];
	if (n && n[0])
	{
		S_PrecacheSound (n);
		if (vol > 0)
			S_LocalSoundWithVol (n, vol);
	}
}

/*
 void(string str) precache_sound = #19/#76 - FTE parity (PF_cs_PrecacheSound): local
 precache + queue a missing sound for download. void - OFS_RETURN is not written
 (the module does not rely on the return value).
*/
static void csqc_precache_sound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM0);
	if (vm && n && n[0])
	{
		S_PrecacheSound (n);
		// FTE parity: queue the missing sound (`sound/<name>`). Caller-side guard on
		// `cls.download`: ezq download is single-slot, so a second request would clobber
		// the in-flight one. `*` = sexed sound (not downloadable).
		if (n[0] != '*' && !cls.download)
			CL_CheckOrDownloadFile (va ("sound/%s", n));
	}
}

static void csqc_precache_model (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM0);
	if (vm && n && n[0])
	{
		int idx = CSQC_Client_ModelIndex (n);
		// ModelIndex now returns a stable index even for a missing model (NULL
		// placeholder), so "file missing" is detected by the model being not loaded,
		// not by index==0. FTE parity (PF_cs_PrecacheModel_Internal): queue the model
		// for download. Same single-slot `cls.download` guard as precache_sound.
		if (!CSQC_Client_ModelForIndex (idx) && n[0] != '*' && !cls.download)
			CL_CheckOrDownloadFile (n);
	}
	CSQCVM_SetRetStr (n ? n : "");
}

/*
 float(string modelname, optional float queryonly) getmodelindex = #200.
 Model index in the CSQC registry (name->Mod_ForName); FTE PF_getmodelindex.
 queryonly!=0 - only look up an already registered one (no load); otherwise register.
 Deviation: a single registry on top of Mod_ForName (FTE has a separate index space).
*/
static void csqc_getmodelindex (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n;
	int queryonly;
	if (!vm)
		return;
	n = CSQCVM_Str (OFS_PARM0);
	queryonly = (vm->argc > 1) ? (int)vm->globals[OFS_PARM1] : 0;
	vm->globals[OFS_RETURN] = (float)(queryonly
		? CSQC_Client_ModelIndexKnown (n) : CSQC_Client_ModelIndex (n));
}

/*
 string(float mdlindex) modelnameforindex = #334.
 FTE parity (PF_cs_ModelnameForIndex): reverse index resolution.
 Deviation: ezq has a single positive CSQC registry (getmodelindex returns exactly
 its index), while FTE has csqc slots < 0 and server-precache >= 0. Hence the order:
 CSQC registry -> server cl.model_name[idx]; idx<0 -> "".
*/
static void csqc_modelnameforindex (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *name;
	int idx;

	if (!vm)
		return;
	idx = (int)vm->globals[OFS_PARM0];
	name = CSQC_Client_ModelNameForIndex (idx);
	if (!name && idx >= 0 && idx < MAX_MODELS)
		name = cl.model_name[idx];
	CSQCVM_SetRetStr ((char *)(name ? name : ""));
}

/*
 float(string) precache_file (#68/#77) - FTE parity (PF_cs_precachefile ->
 CL_CheckOrEnqueDownloadFile): true=file present -> 1; false=queued for download -> 0.
 Here - CL_CheckOrDownloadFile (the same contract: true if present/not downloading,
 otherwise sends download and false). Guard: if a download is already in flight
 (cls.download) we do not start a second one -> 0.
*/
static void csqc_precache_file (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n;

	if (!vm)
		return;
	n = CSQCVM_Str (OFS_PARM0);
	if (!n || !n[0])
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	if (cls.download)	// another resource is already downloading - do not interrupt
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	vm->globals[OFS_RETURN] = CL_CheckOrDownloadFile (n) ? 1.0f : 0.0f;
}

/*
 void(vector pos, string samp, float vol, float atten) ambientsound = #74
 Deviation: no positional 3D - precache + local playback (vol).
*/
static void csqc_ambientsound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM1);
	float vol = 1;
	if (!vm)
		return;
	if (vm->argc > 2)
		vol = vm->globals[OFS_PARM0 + 6];
	if (n && n[0])
	{
		S_PrecacheSound (n);
		if (vol > 0)
			S_LocalSoundWithVol (n, vol);
	}
}

/*
void(vector pos, vector dir, float colour, float count) particle = #48
*/
static void csqc_particle (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	R_RunParticleEffect (&vm->globals[OFS_PARM0],
		&vm->globals[OFS_PARM0 + 3],
		(int)vm->globals[OFS_PARM0 + 6],
		(int)vm->globals[OFS_PARM0 + 9]);
}

/*
 void(float lightstyle, string stylestring, optional vector rgb) lightstyle = #35
 Deviation: the client does not style lights - no-op.
*/
static void csqc_lightstyle (void)
{
	/* no-op */
}

/*
 void(float pause) setpause = #531
 Deviation: no server pause on the client - no-op.
*/
static void csqc_setpause (void)
{
	/* no-op */
}

/*
 Basic entities on the arena. PR1 entity value = entnum*edict_size (int bits). Field
 types (eprint): code 1 = ev_string, 2 = ev_float, 3 = ev_vector, 4 = ev_entity.
*/

static int csqc_ent_of (pr1vm_t *vm, int parmofs)
{
	if (!vm)
		return 0;
	return CSQC_Client_EntNum (vm, *(int *)&vm->globals[parmofs]);
}

static float *csqc_ent_slot (pr1vm_t *vm, int entnum)
{
	if (!vm || !vm->game_edicts)
		return NULL;
	if (entnum < 0 || entnum >= vm->num_edicts)
		return NULL;
	return (float *)((byte *)vm->game_edicts + (size_t)entnum * vm->edict_size);
}

static float *csqc_ent_field (pr1vm_t *vm, int entnum, const char *name)
{
	int ofs;
	float *slot;
	if ((ofs = CSQC_Client_FindField (vm, name)) < 0)
		return NULL;
	slot = csqc_ent_slot (vm, entnum);
	return slot ? &slot[ofs] : NULL;
}

// Access a field via the cached offset (CSQC_Client_FieldOfs) - no fielddefs scan
// per call (hot path in addentities).
static float *csqc_ent_ofs (pr1vm_t *vm, int entnum, int fldofs)
{
	float *slot;
	if (fldofs < 0)
		return NULL;
	slot = csqc_ent_slot (vm, entnum);
	return slot ? &slot[fldofs] : NULL;
}

static void csqc_ret_entity (pr1vm_t *vm, int entnum)
{
	*(int *)&vm->globals[OFS_RETURN] = entnum * vm->edict_size;
}

/*
 arena edict -> ezq entity_t -> cl_visents (#301 arena / #302). FTE
 CopyCSQCEdictToEntity uses .modelindex; here the model is taken from the `.model`
 string via Mod_ForName. .predraw is called before reading fields, .renderflags maps
 to ent.renderfx (a subset of CSQCRF_*). .scale is not applied.
*/
static void csqc_add_one_entity (int e)
{
	pr1vm_t *vm = CSQCVM_Active ();
	entity_t ent;
	float *slot, *f;
	char *mname;
	model_t *model;
	int ofs;
	int playernum = -1;	// network player index (colormap 1..MAX_CLIENTS)

	if (!vm || e <= 0 || !CSQC_Client_EntUsed (e))
		return;

	// .predraw. Return != PREDRAW_AUTOADD(0) or removal of the edict -> do not add.
	// The function-valued field is raw int bits (EV_FUNCTION): read as int, not float
	// (otherwise denormal -> 0).
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_PREDRAW))) && *(int *)&f[0] > 0)
	{
		qbool removed = false;
		float pret = CSQC_Client_CallPredraw (e, *(int *)&f[0], &removed);
		if (removed || pret != 0)
			return;
	}

	slot = csqc_ent_slot (vm, e);
	if (!slot)
		return;
	// model: .modelindex with fallback to the .model string
	model = NULL;
	if ((ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELINDEX)) >= 0)
	{
		int mi = (int)slot[ofs];
		if (mi > 0)
			model = CSQC_Client_ModelForIndex (mi);
	}
	if (!model)
	{
		if ((ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODEL)) < 0)
			return;
		mname = CSQC_Client_GetString (vm, (string_t)*(int *)&slot[ofs]);
		if (!mname || !mname[0])
			return;
		model = Mod_ForName (mname, false);
	}
	if (!model)
		return;

	memset (&ent, 0, sizeof (ent));
	ent.model = model;
	ent.colormap = vid.colormap;
	// .colormap as player index (1..MAX_CLIENTS) -> team translation table +
	// scoreboard (player skin), same as the engine player render. The modhint guard
	// mirrors the engine: heads/gibs (h_player) stay unskinned. FTE maps the index to
	// playerindex/topcolour; values > MAX_CLIENTS (DP colormap) fall back to
	// vid.colormap / NULL scoreboard.
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_COLORMAP))))
	{
		int cm = (int)f[0];
		if (cm > 0 && cm <= MAX_CLIENTS && model->modhint == MOD_PLAYER)
		{
			ent.colormap = cl.players[cm - 1].translations;
			ent.scoreboard = &cl.players[cm - 1];
			playernum = cm - 1;
		}
	}
	ent.oldframe = ent.frame;
	ent.framelerp = -1;
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN))))		VectorCopy (f, ent.origin);
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ANGLES))))		VectorCopy (f, ent.angles);
	// player render pitch = -viewangles/3, same as the engine player render and the
	// FTE CSQC bridge. Render-side only: does not touch the module's .angles (used by
	// #347/prediction). Roll is left 0 (the FTE CSQC bridge also 0).
	if (playernum >= 0)
		ent.angles[PITCH] = -ent.angles[PITCH] / 3;
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_FRAME))))		ent.frame = ent.oldframe = (int)f[0];
	// player frame interpolation - exact engine formula (CL_LinkPlayers). Without it
	// CSQC renders the raw frame while the engine lerps, so the pose jumps when
	// toggling csqc_delta. FTE has no jump because engine and CSQC share cl.lerpplayers.
	if (playernum >= 0)
	{
		centity_t *cent = &cl_entities[playernum + 1];
		if (cent->frametime >= 0 && cent->frametime <= cl.time)
		{
			ent.oldframe = cent->oldframe;
			ent.framelerp = (cl.time - cent->frametime) * 10;
		}
		else
		{
			ent.oldframe = ent.frame;
			ent.framelerp = -1;
		}
	}
	// cl_deadbodyFilter for CSQC-owned players. The engine player filter
	// (CL_LinkPlayers) is skipped for owned players, so apply the same semantics
	// here. TF exception for mode 3 (!cl.teamfortress) mirrors the engine.
	if (playernum >= 0 && model->modhint == MOD_PLAYER)
	{
		int i = ent.frame;
		if (cl_deadbodyfilter.value == 3 && !cl.teamfortress)
		{
			if (ISDEAD(i))
				return;
		}
		if (cl_deadbodyfilter.value == 2)
		{
			if (ISDEAD(i))
				return;
		}
		else if (cl_deadbodyfilter.value == 1)
		{
			if (i == 49 || i == 60 || i == 69 || i == 84 || i == 93 || i == 102)
				return;
		}
	}
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SKIN))))		ent.skinnum = (int)f[0];
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_EFFECTS))))	ent.effects = (int)f[0];
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ALPHA))))		ent.alpha = f[0];
	// .scale: uniform render scale (FTE remaps 0 to 1). 0 stays 0 here - the render
	// helper/culling treat 0 as unscaled.
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SCALE))))		ent.scale = f[0];
	// .renderflags (CSQCRF_*) -> ent.renderfx (RF_*). Map the available subset;
	// DEPTHHACK/EXTERNALMODEL/FIRSTPERSON/USEAXIS have no direct ezq analog.
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_RENDERFLAGS))))
	{
		int rflags = (int)f[0];
		if (rflags & 1)		// CSQCRF_VIEWMODEL
			ent.renderfx |= RF_WEAPONMODEL;
		if (rflags & 32)	// CSQCRF_NOSHADOW
			ent.renderfx |= RF_NOSHADOW;
		if (rflags & 8)		// CSQCRF_ADDITIVE
			ent.renderfx |= RF_ADDITIVEBLEND;
	}

	// PVS/leaf + bbox culling, FTE EdictInFatPVS parity.
	// R_CSQC_BeginCull marks the leaves/frustum for this frame (once per frame).
	R_CSQC_BeginCull ();
	if (!R_CSQC_EntityVisible (&ent))
		return;

	CL_AddEntity (&ent);
}

/* void(entity ent) addentity = #302 */
static void csqc_addentity (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm || !CSQC_Client_SceneActive ())
		return;
	csqc_add_one_entity (csqc_ent_of (vm, OFS_PARM0));
}

/* entity() spawn = #14 */
static void csqc_spawn (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	csqc_ret_entity (vm, CSQC_Client_EntAlloc (vm));
}

/* void(entity e) remove = #15 */
static void csqc_remove (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_EntFree (vm, csqc_ent_of (vm, OFS_PARM0));
}

/* void(entity e, vector org) setorigin = #2 */
static void csqc_setorigin (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *f, *o;
	int e;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
	if (!f)
		return;
	o = &vm->globals[OFS_PARM0 + 3];
	f[0] = o[0]; f[1] = o[1]; f[2] = o[2];
}

/*
 FTE parity: copy the model bbox into the arena edict fields. FTE csqc_setmodel
 copies model->mins/maxs and size; PF_cs_SetSize writes .size = maxs-mins.
 model == NULL -> zero out.
*/
static void csqc_model_bbox (pr1vm_t *vm, int e, model_t *model)
{
	float *fmn, *fmx, *fsz;

	if (!vm || e <= 0)
		return;
	fmn = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MINS));
	fmx = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MAXS));
	fsz = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SIZE));
	if (model)
	{
		if (fmn) { fmn[0] = model->mins[0]; fmn[1] = model->mins[1]; fmn[2] = model->mins[2]; }
		if (fmx) { fmx[0] = model->maxs[0]; fmx[1] = model->maxs[1]; fmx[2] = model->maxs[2]; }
		if (fsz)
		{
			fsz[0] = model->maxs[0] - model->mins[0];
			fsz[1] = model->maxs[1] - model->mins[1];
			fsz[2] = model->maxs[2] - model->mins[2];
		}
	}
	else
	{
		if (fmn) { fmn[0] = fmn[1] = fmn[2] = 0; }
		if (fmx) { fmx[0] = fmx[1] = fmx[2] = 0; }
		if (fsz) { fsz[0] = fsz[1] = fsz[2] = 0; }
	}
}

/* void(entity e, string m) setmodel = #3 */
static void csqc_setmodel (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	int e, ofs, idx;
	float *slot;
	model_t *model;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	s = CSQCVM_Str (OFS_PARM0 + 3);
	slot = csqc_ent_slot (vm, e);
	if (!slot || !s)
		return;
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODEL);
	if (ofs >= 0)
		PR1VM_ClientSetString (vm, (string_t *)&slot[ofs], s);
	// .modelindex from the CSQC registry (arena edicts are rendered by index)
	idx = CSQC_Client_ModelIndex (s);
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELINDEX);
	if (ofs >= 0)
		slot[ofs] = (float)idx;
	// bbox from the model + .modelflags.
	model = idx ? CSQC_Client_ModelForIndex (idx) : NULL;
	csqc_model_bbox (vm, e, model);
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELFLAGS);
	if (ofs >= 0 && model)
		slot[ofs] = (float)model->flags;
}

/* void(entity e, float mdlindex) setmodelindex = #333 */
static void csqc_setmodelindex (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *f, *slot;
	int e, idx, ofs;
	const char *name;
	model_t *model;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	idx = (int)vm->globals[OFS_PARM0 + 3];
	f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELINDEX));
	if (f)
		f[0] = (float)idx;
	// Resolve the CSQC registry -> .model + bbox. An unresolved/non-positive index ->
	// .model is left untouched (FTE early-return parity). Single positive index space.
	model = (idx > 0) ? CSQC_Client_ModelForIndex (idx) : NULL;
	if (!model)
		return;
	name = CSQC_Client_ModelNameForIndex (idx);
	slot = csqc_ent_slot (vm, e);
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODEL);
	if (ofs >= 0 && slot && name)
		PR1VM_ClientSetString (vm, (string_t *)&slot[ofs], (char *)name);
	csqc_model_bbox (vm, e, model);
}

/* void(entity e, vector min, vector max) setsize = #4 */
static void csqc_setsize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *fmin, *fmax, *fsz, *mn, *mx;
	int e;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	fmin = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MINS));
	fmax = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MAXS));
	if (!fmin || !fmax)
		return;
	mn = &vm->globals[OFS_PARM0 + 3];
	mx = &vm->globals[OFS_PARM0 + 6];
	fmin[0] = mn[0]; fmin[1] = mn[1]; fmin[2] = mn[2];
	fmax[0] = mx[0]; fmax[1] = mx[1]; fmax[2] = mx[2];
	// .size = maxs - mins
	fsz = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SIZE));
	if (fsz)
	{
		fsz[0] = mx[0] - mn[0];
		fsz[1] = mx[1] - mn[1];
		fsz[2] = mx[2] - mn[2];
	}
}

/* entity(entity e) nextent = #47 - module arena (network ones are not "used") */
static void csqc_nextent (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	if (e < 0)
		e = 0;
	for (e++; e < vm->num_edicts; e++)
		if (CSQC_Client_EntUsed (e))
		{
			csqc_ret_entity (vm, e);
			return;
		}
	csqc_ret_entity (vm, 0);
}

/* entity(entity start, .string fld, string match) find = #18 (string fields) */
static void csqc_find (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, f;
	char *s, *t;
	float *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = *(int *)&vm->globals[OFS_PARM0 + 3];
	s = CSQCVM_Str (OFS_PARM0 + 6);
	if (f < 0 || f >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_Find: bad field reference");
		return;
	}
	if (e < 0)
		e = 0;
	if (s)
		for (e++; e < vm->num_edicts; e++)
		{
			if (!CSQC_Client_EntUsed (e))
				continue;
			slot = csqc_ent_slot (vm, e);
			if (!slot)
				continue;
			t = CSQC_Client_GetString (vm, *(int *)&slot[f]);
			if (t && !strcmp (t, s))
			{
				csqc_ret_entity (vm, e);
				return;
			}
		}
	csqc_ret_entity (vm, 0);
}

/* SOLID/FL/MOVE constants (csdefs.qc parity). Placed above csqc_findradius, which
   uses solid/FL_FINDABLE_NONSOLID. */
#define CSQC_SOLID_NOT		0
#define CSQC_SOLID_TRIGGER	1
#define CSQC_SOLID_BSP		4
#define CSQC_FL_MONSTER		32
#define CSQC_FL_FINDABLE_NONSOLID	16384
#define CSQC_FL_ONGROUND	512
#define CSQC_MOVE_NOMONSTERS	1
#define CSQC_MOVE_MISSILE	2
#define CSQC_MOVE_HITMODEL	4
#define CSQC_MOVE_TRIGGERS	16
#define CSQC_MOVE_EVERYTHING	32
#define CSQC_MOVE_LAGGED	64

/* entity(vector org, float rad) findradius = #22 - arena; chain if the field exists */
static void csqc_findradius (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *org, *o, *fld, rad, d;
	float *chslot;
	int e, chain_ofs, prev;
	if (!vm)
		return;
	org = &vm->globals[OFS_PARM0];
	rad = vm->globals[OFS_PARM0 + 3];
	chain_ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	prev = 0;	// chain head; world(0) is the terminator
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		o = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
		if (!o)
			continue;
		// non-solid is skipped unless FL_FINDABLE_NONSOLID is set.
		fld = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SOLID));
		if (fld && (int)*fld == CSQC_SOLID_NOT)
		{
			fld = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_FLAGS));
			if (!fld || !((int)*fld & CSQC_FL_FINDABLE_NONSOLID))
				continue;
		}
		d = (o[0]-org[0])*(o[0]-org[0]) + (o[1]-org[1])*(o[1]-org[1]) + (o[2]-org[2])*(o[2]-org[2]);
		if (d > rad * rad)
			continue;
		// ent.v.chain = chain; chain = ent -> return the last (head), the other
		// matches are reachable by walking `.chain`.
		if (chain_ofs >= 0)
		{
			chslot = csqc_ent_slot (vm, e);
			if (chslot)
				*(int *)&chslot[chain_ofs] = prev * vm->edict_size;
		}
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/*
 Entity search/copy (#400/#402/#403/#449/#450). Arena fields are float-word offsets
 (ddef_t.ofs), PR1 entity value = slot*edict_size. The chain link writes the previous
 entity value into the chainfield (terminator world(0)), same as csqc_findradius (#22).
*/

/* void(entity from, entity to) copyentity = #400 */
static void csqc_copyentity (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int from, to;
	float *src, *dst;
	if (!vm)
		return;
	from = csqc_ent_of (vm, OFS_PARM0);
	if (vm->argc <= 1)
		to = CSQC_Client_EntAlloc (vm);
	else
		to = csqc_ent_of (vm, OFS_PARM0 + 3);
	// free source/dest is fatal (readonly/fieldsize have no ezq-arena counterpart:
	// no readonly, single edict_size).
	if (from <= 0 || !CSQC_Client_EntUsed (from))
	{
		CSQC_Client_Abort ("PF_copyentity: source is free");
		return;
	}
	if (to <= 0 || !CSQC_Client_EntUsed (to))
	{
		CSQC_Client_Abort ("PF_copyentity: destination is free");
		return;
	}
	src = csqc_ent_slot (vm, from);
	dst = csqc_ent_slot (vm, to);
	if (!src || !dst)
		return;
	memcpy (dst, src, vm->edict_size);
	// no client linking (per-frame culling). FTE returns dest; the module declares void.
	csqc_ret_entity (vm, to);
}

/* entity(.string field, string match, .entity chainfield) findchain = #402 */
static void csqc_findchain (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int f, cf, e, prev;
	char *s, *t;
	float *slot;
	if (!vm)
		return;
	f = *(int *)&vm->globals[OFS_PARM0];
	s = CSQCVM_Str (OFS_PARM0 + 3);
	cf = (vm->argc > 2) ? *(int *)&vm->globals[OFS_PARM0 + 6]
		: CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	if (f < 0 || f >= vm->progs->entityfields || cf < 0 || cf >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindChain: bad field reference");
		return;
	}
	prev = 0;
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		t = CSQC_Client_GetString (vm, *(int *)&slot[f]);
		if (!t)
			continue;
		if (strcmp (t, s ? s : ""))
			continue;
		*(int *)&slot[cf] = prev * vm->edict_size;
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/* entity(.float fld, float match, .entity chainfield) findchainfloat = #403 */
static void csqc_findchainfloat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int f, cf, e, prev;
	float match, *slot;
	if (!vm)
		return;
	f = *(int *)&vm->globals[OFS_PARM0];
	match = vm->globals[OFS_PARM0 + 3];
	cf = (vm->argc > 2) ? *(int *)&vm->globals[OFS_PARM0 + 6]
		: CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	if (f < 0 || f >= vm->progs->entityfields || cf < 0 || cf >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindChain: bad field reference");
		return;
	}
	prev = 0;
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		// float equality (not int bits, unlike findfloat #98).
		if (slot[f] != match)
			continue;
		*(int *)&slot[cf] = prev * vm->edict_size;
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/* entity(entity start, .float fld, float match) findflags = #449 */
static void csqc_findflags (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, f;
	float match, *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = *(int *)&vm->globals[OFS_PARM0 + 3];
	match = vm->globals[OFS_PARM0 + 6];
	if (f < 0 || f >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindFlags: bad field reference");
		return;
	}
	if (e < 0)
		e = 0;
	for (e++; e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		if (*(int *)&slot[f] & *(int *)&match)
		{
			csqc_ret_entity (vm, e);
			return;
		}
	}
	csqc_ret_entity (vm, 0);
}

/* entity(.float fld, float match, .entity chainfield) findchainflags = #450 */
static void csqc_findchainflags (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int f, cf, e, prev;
	float match, *slot;
	if (!vm)
		return;
	f = *(int *)&vm->globals[OFS_PARM0];
	match = vm->globals[OFS_PARM0 + 3];
	cf = (vm->argc > 2) ? *(int *)&vm->globals[OFS_PARM0 + 6]
		: CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	if (f < 0 || f >= vm->progs->entityfields || cf < 0 || cf >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindChain: bad field reference");
		return;
	}
	prev = 0;
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		if (!(*(int *)&slot[f] & *(int *)&match))
			continue;
		*(int *)&slot[cf] = prev * vm->edict_size;
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/* void() changeyaw = #49 - no-op (no server physics) */
static void csqc_changeyaw (void)
{
	/* no-op */
}

/* void(entity e) makestatic = #69 - no-op (client does not track static ents) */
static void csqc_makestatic (void)
{
	/* no-op */
}

/* string(entity e, string key) infokey = #80 - serverinfo (client has no per-ent userinfo) */
static void csqc_infokey (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *key = CSQCVM_Str (OFS_PARM0 + 3);
	if (!vm)
		return;
	CSQCVM_SetRetStr (Info_ValueForKey (cl.serverinfo, key ? key : ""));
}

/* float(entity e) checkbottom = #40 - 0 (no server floor) */
static void csqc_checkbottom (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 0;
}

/* void(entity e) eprint = #31 - print the slot fields to the console (via fielddefs) */
static void csqc_eprint (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, i, ofs;
	float *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	slot = csqc_ent_slot (vm, e);
	if (!slot)
	{
		Con_Printf ("eprint: bad entity %d\n", e);
		return;
	}
	Con_Printf ("eprint entity %d\n", e);
	for (i = 0; i < vm->progs->numfielddefs; i++)
	{
		char *fn = CSQC_Client_GetString (vm, vm->fielddefs[i].s_name);
		ofs = vm->fielddefs[i].ofs;
		if (!fn)
			continue;
		switch (vm->fielddefs[i].type)
		{
		case 1:	/* ev_string */
			Con_Printf ("  .%s = \"%s\"\n", fn,
				CSQC_Client_GetString (vm, *(int *)&slot[ofs]) ? CSQC_Client_GetString (vm, *(int *)&slot[ofs]) : "");
			break;
		case 2:	/* ev_float */
			Con_Printf ("  .%s = %g\n", fn, slot[ofs]);
			break;
		case 3:	/* ev_vector */
			Con_Printf ("  .%s = '%g %g %g'\n", fn, slot[ofs], slot[ofs + 1], slot[ofs + 2]);
			break;
		case 4:	/* ev_entity */
			Con_Printf ("  .%s = ent %d\n", fn, (int)slot[ofs]);
			break;
		}
	}
}

/* void() coredump = #28 - module header + arena usage */
static void csqc_coredump (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	Con_Printf ("coredump (CSQC): funcs %d globals %d fields %d pool_used %d\n",
		vm->progs->numfunctions, vm->progs->numglobals, vm->progs->numfielddefs,
		CSQC_Client_EntUsedCount ());
}

/*
 World traces/physics. traceline/tracebox account for the pool entity layer (MOVE_*
 dispatch + forent/owner-ignore + player mirror, see csqc_trace_ents below);
 walkmove/droptofloor stay world-only (server semantics: the move to floor/step is not
 blocked by entities). Deviations: AABB instead of hull, HITMODEL->bbox, TRIGGERS
 without the brush world, no LAGGED, the entity-hit plane is not written, movement is
 its own trace without a full PM_PlayerMove.
*/

static trace_t csqc_trace_fallback (vec3_t end)
{
	trace_t tr;
	memset (&tr, 0, sizeof (tr));
	tr.fraction = 1;
	VectorCopy (end, tr.endpos);
	return tr;
}

static trace_t csqc_world_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	cmodel_t *clip;
	hull_t *hull;
	trace_t tr;
	vec3_t offset, sl, el;
	qbool box = (mins != NULL && maxs != NULL);

	clip = cl.clipmodels[1];	// world clip model (BSP hulls)
	if (!clip)
		return csqc_trace_fallback (end);

	if (!box || (mins[0] == 0 && mins[1] == 0 && mins[2] == 0 &&
		maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0))
	{
		hull = &clip->hulls[0];
		return CM_HullTrace (hull, start, end);
	}

	/* box: hull[1] (player-clip) with an offset by the passed mins/maxs */
	hull = &clip->hulls[1];
	VectorSubtract (hull->clip_mins, mins, offset);
	VectorSubtract (start, offset, sl);
	VectorSubtract (end, offset, el);
	tr = CM_HullTrace (hull, sl, el);
	VectorAdd (tr.endpos, offset, tr.endpos);
	return tr;
}

/*
 Q1 content conversion (CM_HullPointContents -> CONTENTS_*, -1..-6) into the
 FTECONTENTS domain, like FTE `tr->contents`. Index -1-q1. For an entity hit: local
 csqc/AABB -> 0 (no surface-contents); a network brush (ssqc) -> content from `.skin`
 (Q1) or FTECONTENTS_SOLID by default (a plain solid brush).
*/
static const unsigned int s_q1_to_fte_contents[7] =
{
	0x00000000u,	// CONTENTS_EMPTY (-1)
	0x00000001u,	// CONTENTS_SOLID (-2)
	0x00000020u,	// CONTENTS_WATER (-3)
	0x00000010u,	// CONTENTS_SLIME (-4)
	0x00000008u,	// CONTENTS_LAVA  (-5)
	0x80000000u,	// CONTENTS_SKY   (-6)
	0x00000001u		// STRIPPED       (-7)
};

static void csqc_store_trace (pr1vm_t *vm, trace_t *tr)
{
	int o;
	// offsets from the cache (resolved at load), not a globaldefs scan.
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_FRACTION)) >= 0)
		vm->globals[o] = tr->fraction;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_ALLSOLID)) >= 0)
		vm->globals[o] = tr->allsolid;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_STARTSOLID)) >= 0)
		vm->globals[o] = tr->startsolid;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_INOPEN)) >= 0)
		vm->globals[o] = tr->inopen;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_INWATER)) >= 0)
		vm->globals[o] = tr->inwater;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_PLANE_DIST)) >= 0)
		vm->globals[o] = tr->plane.dist;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_ENDPOS)) >= 0)
	{
		vm->globals[o] = tr->endpos[0];
		vm->globals[o + 1] = tr->endpos[1];
		vm->globals[o + 2] = tr->endpos[2];
	}
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_PLANE_NORMAL)) >= 0)
	{
		vm->globals[o] = tr->plane.normal[0];
		vm->globals[o + 1] = tr->plane.normal[1];
		vm->globals[o + 2] = tr->plane.normal[2];
	}
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_ENT)) >= 0)
	{
		// entity value = slot*edict_size (int bits); 0 - world.
		*(int *)&vm->globals[o] = (tr->e.entnum > 0) ? tr->e.entnum * vm->edict_size : 0;
	}
	// trace_networkentity - the ssqc number of the hit entity; for own spawn
	// entities/world - 0. Not an arena slot.
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_NETWORKENTITY)) >= 0)
		vm->globals[o] = (float)CSQC_Client_EntityEntNum (vm, tr->e.entnum);
	// trace_endcontents - FTECONTENTS domain. World -> Q1 leaf conversion at
	// trace_endpos (a miss -> 0). Local csqc/AABB -> 0 (no surface-contents).
	// Network brush (ssqc) -> `.skin`/`SOLID` - best-effort. Otherwise 0.
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_ENDCONTENTS)) >= 0)
	{
		unsigned int c = 0u;
		if (tr->e.entnum > 0 && vm->game_edicts && vm->edict_size > 0)
		{
			int ssqc = CSQC_Client_EntityEntNum (vm, tr->e.entnum);
			if (ssqc > 0)
			{
				int fskin = CSQC_Client_FieldOfs (vm, CSQC_FLD_SKIN);
				int fsol = CSQC_Client_FieldOfs (vm, CSQC_FLD_SOLID);
				float *base = (float *)((byte *)vm->game_edicts + (size_t)tr->e.entnum * vm->edict_size);
				int skin = (fskin >= 0) ? (int)base[fskin] : 0;
				int sol = (fsol >= 0) ? (int)base[fsol] : 0;
				if (skin <= -1 && skin >= -6)
					c = s_q1_to_fte_contents[-1 - skin];
				else if (sol == CSQC_SOLID_BSP)
					c = 0x00000001u;	// FTECONTENTS_SOLID (plain solid brush)
			}
		}
		else if (cl.clipmodels[1])
		{
			hull_t *h = &cl.clipmodels[1]->hulls[0];
			int q1 = CM_HullPointContents (h, h->firstclipnode, tr->endpos);
			if (q1 >= -6 && q1 <= -1)
				c = s_q1_to_fte_contents[-1 - q1];
		}
		vm->globals[o] = (float)c;
	}
}

/* Segment vs AABB (slab); returns t in [0,1], false if no intersection. */
static qbool csqc_ray_aabb (vec3_t start, vec3_t dir, vec3_t bmin, vec3_t bmax, float *tout)
{
	float tmin = 0, tmax = 1;
	int i;
	for (i = 0; i < 3; i++)
	{
		float d = dir[i];
		float t1, t2, tmp;
		if (d > -1e-8 && d < 1e-8)
		{
			if (start[i] < bmin[i] || start[i] > bmax[i])
				return false;
			continue;
		}
		t1 = (bmin[i] - start[i]) / d;
		t2 = (bmax[i] - start[i]) / d;
		if (t1 > t2) { tmp = t1; t1 = t2; t2 = tmp; }
		if (t1 > tmin) tmin = t1;
		if (t2 < tmax) tmax = t2;
		if (tmin > tmax)
			return false;
	}
	*tout = tmin;
	return (tmin >= 0 && tmin <= 1);
}

/*
 MOVE_* dispatch + forent/owner-ignore over the pool entities. After the world trace,
 check entities (origin/mins/maxs) and override if closer. AABB approximation (no
 hull/movetype semantics; .solid/.flags/.owner as in csdefs.qc). moveflags - 3rd arg of
 traceline / 5th of tracebox (MOVE_* mask); forent - the entity slot that the trace
 (and its owner) does not hit. boxmin/boxmax != NULL - tracebox: the entity AABB is
 expanded by the box (swept approximation). SOLID/FL/MOVE constants are defined above.
*/

static void csqc_trace_ents (pr1vm_t *vm, vec3_t start, vec3_t end,
	int moveflags, int forent, vec3_t boxmin, vec3_t boxmax, trace_t *tr)
{
	vec3_t dir, bmin, bmax;
	float t;
	int e, i;
	int ofs_o, ofs_mn, ofs_mx, ofs_sol, ofs_fl, ofs_own;
	qbool nomon, everything, triggers, missile;

	if (!vm || !vm->game_edicts || tr->fraction <= 0 || vm->edict_size <= 0)
		return;
	nomon = !!(moveflags & CSQC_MOVE_NOMONSTERS);
	everything = !!(moveflags & CSQC_MOVE_EVERYTHING);
	triggers = !!(moveflags & CSQC_MOVE_TRIGGERS);
	missile = !!(moveflags & CSQC_MOVE_MISSILE);
	// NOMONSTERS does not exit immediately: FTE keeps SOLID_BSP (see the filter below).
	if (forent < 0 || forent >= vm->num_edicts)
		forent = 0;	// world - no forent checks

	ofs_o = CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN);
	ofs_mn = CSQC_Client_FieldOfs (vm, CSQC_FLD_MINS);
	ofs_mx = CSQC_Client_FieldOfs (vm, CSQC_FLD_MAXS);
	ofs_sol = CSQC_Client_FieldOfs (vm, CSQC_FLD_SOLID);
	ofs_fl = CSQC_Client_FieldOfs (vm, CSQC_FLD_FLAGS);
	ofs_own = CSQC_Client_FieldOfs (vm, CSQC_FLD_OWNER);
	if (ofs_o < 0 || ofs_mn < 0 || ofs_mx < 0)
		return;	// module has no geometry fields - entity layer unavailable

	for (i = 0; i < 3; i++)
		dir[i] = end[i] - start[i];

	for (e = 1; e < vm->num_edicts; e++)
	{
		float *base, *org, *mn, *mx;
		int solf, flf;
		float inflate = 0;

		if (!CSQC_Client_EntUsed (e))
			continue;
		base = (float *)((byte *)vm->game_edicts + (size_t)e * vm->edict_size);
		org = base + ofs_o;
		mn = base + ofs_mn;
		mx = base + ofs_mx;

		// forent/owner-ignore: does not hit forent, its .owner and any entity whose
		// .owner == forent.
		if (e == forent)
			continue;
		if (ofs_own >= 0)
		{
			int own;
			own = (int)base[ofs_own];
			if (own != 0 && own / vm->edict_size == forent)
				continue;	// ent whose owner == forent
		}
		if (forent > 0 && ofs_own >= 0)
		{
			float *fbase = (float *)((byte *)vm->game_edicts + (size_t)forent * vm->edict_size);
			int fo = (int)fbase[ofs_own];
			if (fo != 0 && fo / vm->edict_size == e)
				continue;	// forent.owner == ent
		}

		solf = (ofs_sol >= 0) ? (int)base[ofs_sol] : CSQC_SOLID_NOT;
		flf = (ofs_fl >= 0) ? (int)base[ofs_fl] : 0;

		// FTE filter: SOLID_NOT - always skipped; a trigger is hit only with
		// MOVE_TRIGGERS/MOVE_EVERYTHING AND FL_FINDABLE_NONSOLID.
		if (solf == CSQC_SOLID_NOT)
			continue;
		if (solf == CSQC_SOLID_TRIGGER)
		{
			if (!(flf & CSQC_FL_FINDABLE_NONSOLID))
				continue;
			if (!everything && !triggers)
				continue;
		}
		// MOVE_NOMONSTERS keeps SOLID_BSP.
		if (nomon && solf != CSQC_SOLID_BSP)
			continue;
		// MISSILE: monsters with an increased size (+-15).
		if (missile && (flf & CSQC_FL_MONSTER))
			inflate = 15;

		for (i = 0; i < 3; i++)
		{
			bmin[i] = org[i] + mn[i] - inflate + (boxmin ? boxmin[i] : 0);
			bmax[i] = org[i] + mx[i] + inflate + (boxmax ? boxmax[i] : 0);
		}
		if (!csqc_ray_aabb (start, dir, bmin, bmax, &t))
			continue;
		if (t > tr->fraction)
			continue;
		tr->fraction = t;
		for (i = 0; i < 3; i++)
			tr->endpos[i] = start[i] + dir[i] * t;
		tr->e.entnum = e;
		tr->allsolid = false;
		tr->startsolid = false;
	}
}

/* void(vector v1, vector v2, float flags, entity forent) traceline = #16 */
static void csqc_traceline (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	trace_t tr;
	int moveflags, forent;
	if (!vm)
		return;
	// PR1 ABI: each param is a 3-word block (param_index*3). traceline:
	// v1@0 v2@3 flags@6 forent@9.
	moveflags = (int)vm->globals[OFS_PARM0 + 6];
	forent = csqc_ent_of (vm, OFS_PARM0 + 9);
	tr = csqc_world_trace (&vm->globals[OFS_PARM0], NULL, NULL,
		&vm->globals[OFS_PARM0 + 3]);
	csqc_trace_ents (vm, &vm->globals[OFS_PARM0], &vm->globals[OFS_PARM0 + 3],
		moveflags, forent, NULL, NULL, &tr);
	csqc_store_trace (vm, &tr);
}

/* void(vector v1, vector mins, vector maxs, vector v2, float flags, entity forent) tracebox = #90 */
static void csqc_tracebox (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	trace_t tr;
	int moveflags, forent;
	if (!vm)
		return;
	// PR1 ABI: each param is a 3-word block (param_index*3). tracebox:
	// v1@0 mins@3 maxs@6 v2@9 flags@12 forent@15.
	moveflags = (int)vm->globals[OFS_PARM0 + 12];
	forent = csqc_ent_of (vm, OFS_PARM0 + 15);
	tr = csqc_world_trace (&vm->globals[OFS_PARM0],
		&vm->globals[OFS_PARM0 + 3], &vm->globals[OFS_PARM0 + 6],
		&vm->globals[OFS_PARM0 + 9]);
	csqc_trace_ents (vm, &vm->globals[OFS_PARM0], &vm->globals[OFS_PARM0 + 9],
		moveflags, forent, &vm->globals[OFS_PARM0 + 3], &vm->globals[OFS_PARM0 + 6], &tr);
	csqc_store_trace (vm, &tr);
}

/* float(vector org) pointcontents = #41 */
static void csqc_pointcontents (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	hull_t *hull;
	if (!vm)
		return;
	if (!cl.clipmodels[1])
	{
		vm->globals[OFS_RETURN] = CONTENTS_EMPTY;
		return;
	}
	hull = &cl.clipmodels[1]->hulls[0];
	vm->globals[OFS_RETURN] = CM_HullPointContents (hull, hull->firstclipnode,
		&vm->globals[OFS_PARM0]);
}
/* float(float yaw, float dist) walkmove = #32 (self, own trace) */
static void csqc_walkmove (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float yaw, dist, rad;
	vec3_t start, end;
	trace_t tr;
	float *org = NULL;
	int ofs, entnum;

	if (!vm)
		return;
	yaw = vm->globals[OFS_PARM0];
	dist = vm->globals[OFS_PARM1];
	ofs = PR1VM_FindGlobal (vm, "self");
	if (ofs >= 0)
	{
		entnum = CSQC_Client_EntNum (vm, *(int *)&vm->globals[ofs]);
		org = csqc_ent_ofs (vm, entnum, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
	}
	if (!org)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	rad = yaw * (M_PI / 180.0);
	VectorCopy (org, start);
	end[0] = org[0] + cos (rad) * dist;
	end[1] = org[1] + sin (rad) * dist;	// yaw 0 = +x, yaw 90 = +y
	end[2] = org[2];
	tr = csqc_world_trace (start, NULL, NULL, end);
	if (tr.fraction < 1)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	VectorCopy (end, org);
	vm->globals[OFS_RETURN] = 1;
}

/* float() droptofloor = #34 (self; trace down to the ground) */
static void csqc_droptofloor (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	vec3_t start, end;
	trace_t tr;
	float *org = NULL;
	int ofs, entnum;

	if (!vm)
		return;
	ofs = PR1VM_FindGlobal (vm, "self");
	if (ofs >= 0)
	{
		entnum = CSQC_Client_EntNum (vm, *(int *)&vm->globals[ofs]);
		org = csqc_ent_ofs (vm, entnum, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
	}
	if (!org)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	VectorCopy (org, start);
	end[0] = org[0]; end[1] = org[1]; end[2] = org[2] - 512;
	tr = csqc_world_trace (start, NULL, NULL, end);
	if (tr.fraction >= 1 || tr.fraction <= 0)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	org[0] = tr.endpos[0]; org[1] = tr.endpos[1]; org[2] = tr.endpos[2];
	// landing on the ground sets FL_ONGROUND and groundentity (world = world(0);
	// the ezq world trace does not hit entities - a subset).
	{
		float *ff = csqc_ent_ofs (vm, entnum, CSQC_Client_FieldOfs (vm, CSQC_FLD_FLAGS));
		float *gf = csqc_ent_field (vm, entnum, "groundentity");
		if (ff)
			*ff = (float)((int)*ff | CSQC_FL_ONGROUND);
		if (gf)
			*(int *)gf = (tr.e.entnum > 0) ? tr.e.entnum * vm->edict_size : 0;
	}
	vm->globals[OFS_RETURN] = 1;
}

/* void(float step) movetogoal = #67 - no-op (the module has no goalentity field) */
static void csqc_movetogoal (void)
{
	/* no-op (no .goalentity) */
}

/*
 Strings/tokenization. A separate span store for #514/#479/#515/#516; the existing
 #441/#442 work via the Cmd context and are unchanged (separate mechanisms).
*/
#define CSQC_TOK_MAX 128
static int s_tokn = 0;
static int s_tok_start[CSQC_TOK_MAX];
static int s_tok_end[CSQC_TOK_MAX];

// Console tokenization (spans): spaces/tabs are separators; "..." is one token.
static void csqc_tok_console_spans (const char *s)
{
	int i = 0, len = s ? (int)strlen (s) : 0, n = 0;
	s_tokn = 0;
	while (i < len && n < CSQC_TOK_MAX)
	{
		while (i < len && (s[i] == ' ' || s[i] == '\t'))
			i++;
		if (i >= len)
			break;
		s_tok_start[n] = i;
		if (s[i] == '"')
		{
			i++;
			while (i < len && s[i] != '"')
				i++;
			if (i < len)
				i++;
		}
		else
		{
			while (i < len && s[i] != ' ' && s[i] != '\t')
				i++;
		}
		s_tok_end[n] = i;
		n++;
	}
	s_tokn = n;
}

// tokenizebyseparator: split by any separator (empty tokens are kept),
// spans like FTE.
static void csqc_tok_sep_spans (const char *s, const char *sep[], int nsep)
{
	int i, len, tokstart, n, si;
	int seplen[7];

	s_tokn = 0;
	if (!s || !*s)
		return;
	len = (int)strlen (s);
	for (si = 0; si < nsep && si < 7; si++)
		seplen[si] = (int)strlen (sep[si]);
	i = 0;
	tokstart = 0;
	n = 0;
	for (;;)
	{
		int found = -1;
		if (i >= len)
			found = -2;			// end of string
		else
		{
			for (si = 0; si < nsep && si < 7; si++)
				if (!strncmp (s + i, sep[si], seplen[si]))
				{
					found = si;
					break;
				}
		}
		if (found >= 0)
		{
			if (n < CSQC_TOK_MAX)
			{
				s_tok_start[n] = tokstart;
				s_tok_end[n] = i;
				n++;
			}
			i += seplen[found];
			tokstart = i;
			if (n >= CSQC_TOK_MAX)
				break;
		}
		else if (found == -2)
		{
			if (n < CSQC_TOK_MAX)
			{
				s_tok_start[n] = tokstart;
				s_tok_end[n] = len;
				n++;
			}
			break;
		}
		else
			i++;
	}
	s_tokn = n;
}

/* string(float uselocaltime, string format, ...) strftime = #478 */
static void csqc_strftime (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *p;
	char buf[2048];
	int i, o = 0;
	time_t t;
	struct tm *tm;
	if (!vm)
		return;
	t = time (NULL);
	tm = (vm->globals[OFS_PARM0] != 0) ? localtime (&t) : gmtime (&t);
	for (i = 1; i < vm->argc && o < (int)sizeof (buf) - 1; i++)
	{
		p = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (!p)
			continue;
		// msvc compatibility (like FTE): %R/%F
		if (!strcmp (p, "%R"))
			p = "%H:%M";
		else if (!strcmp (p, "%F"))
			p = "%Y-%m-%d";
		o += snprintf (buf + o, sizeof (buf) - o, "%s", p);
	}
	if (!o)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	{
		char out[512];
		if (strftime (out, sizeof (out), buf, tm))
			CSQCVM_SetRetStr (out);
		else
			CSQCVM_SetRetStr (buf);	// invalid format - return as is
	}
}

/* float(string str) tokenize_console = #514 - like #441 (Cmd) + spans */
static void csqc_tokenize_console (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	if (s)
		Cmd_TokenizeStringEx (&csqc_tokencontext, s);
	vm->globals[OFS_RETURN] = Cmd_ArgcEx (&csqc_tokencontext);
	csqc_tok_console_spans (s);
}

/* float(string s, string sep1, ...) tokenizebyseparator = #479 */
static void csqc_tokenizebyseparator (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	const char *sep[7];
	int nsep = 0, i;
	if (!vm)
		return;
	for (i = 1; i < vm->argc && nsep < 7; i++)
		sep[nsep++] = CSQCVM_Str (OFS_PARM0 + i * 3);
	csqc_tok_sep_spans (s, sep, nsep);
	vm->globals[OFS_RETURN] = s_tokn;
}

/* float(float idx) argv_start_index = #515 / argv_end_index = #516 */
static void csqc_argv_start_index (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int idx = (int)vm->globals[OFS_PARM0];
	if (!vm)
		return;
	if (idx < 0)
		idx += s_tokn;
	if ((unsigned int)idx >= (unsigned int)s_tokn)
		vm->globals[OFS_RETURN] = -1;
	else
		vm->globals[OFS_RETURN] = s_tok_start[idx];
}
static void csqc_argv_end_index (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int idx = (int)vm->globals[OFS_PARM0];
	if (!vm)
		return;
	if (idx < 0)
		idx += s_tokn;
	if ((unsigned int)idx >= (unsigned int)s_tokn)
		vm->globals[OFS_RETURN] = -1;
	else
		vm->globals[OFS_RETURN] = s_tok_end[idx];
}

/*
 Input/keyboard/menu. ezq uses keybindings[]/Key_* (keys.h); there are no bindmaps/
 modifiers/mode enumeration - no-op/approximations.
*/

/* string(float keynum) getkeybind = #342 - bound command or "" (input is a QC code) */
static void csqc_getkeybind (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int keynum = CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0]);
	char *b;
	if (!vm)
		return;
	if (keynum < 0 || keynum >= UNKNOWN + 256)
		b = NULL;
	else
		b = keybindings[keynum];
	CSQCVM_SetRetStr (b ? b : "");
}

/* void(float keynum, string binding, optional float bindmap) setkeybind = #630 (input is a QC code) */
static void csqc_setkeybind (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int keynum = CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0]);
	char *binding = CSQCVM_Str (OFS_PARM1);
	if (!vm)
		return;
	if (keynum >= 0 && keynum < UNKNOWN + 256)
		Key_SetBinding (keynum, binding ? binding : "");
}

/* #520 keynumtostring_omgwtf / #609 keynumtostring_menu - like #340 (QC domain) */
static void csqc_keynumtostring_menu (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr (Key_KeynumToString (CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0])));
}

/* float(string key) stringtokeynum_menu = #614 - like #341 (QC domain) */
static void csqc_stringtokeynum_menu (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (name && name[0]) ? CSQC_Client_KeynumToQC (Key_StringToKeynum (name)) : -1;
}

/*
 string(string command, optional float bindmap) findkeysforcommand = #521
 string(string command, optional float bindmap) findkeysforcommand_dp = #610
 FTE parity: scan keybindings[]; return QC codes in the FTE format
 ` 'code' 'code'...`.
*/
static void csqc_findkeysforcommand (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *cmd = CSQCVM_Str (OFS_PARM0);
	char buf[512];
	int i, o = 0;
	if (!vm)
		return;
	buf[0] = 0;
	if (cmd && cmd[0])
	{
		for (i = 0; i < UNKNOWN + 256 && o < (int)sizeof (buf) - 8; i++)
		{
			if (keybindings[i] && !strcmp (keybindings[i], cmd))
				o += snprintf (buf + o, sizeof (buf) - o, " '%d'", CSQC_Client_KeynumToQC (i));
		}
	}
	CSQCVM_SetRetStr (buf);
}

/* void(float trg) setmousetarget = #603 - no-op (cursor via #343) */
static void csqc_setmousetarget (void)
{
	/* no-op (no separate mousetarget; cursor is #343) */
}

/* float() getmousetarget = #604 - 2 if the CSQC cursor is active, otherwise 1 */
static void csqc_getmousetarget (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = CSQC_Client_CSQCCursor () ? 2 : 1;
}

/* vector(float vidmode, optional float forfullscreen) getresolution = #608 -
   returns the current resolution (mode list is not enumerated) */
static void csqc_getresolution (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = vid.width;
	vm->globals[OFS_RETURN + 1] = vid.height;
	vm->globals[OFS_RETURN + 2] = 0;
}

/* vector() getbindmaps = #631 - no bindmaps: (0,0,0) */
static void csqc_getbindmaps (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = 0;
	vm->globals[OFS_RETURN + 1] = 0;
	vm->globals[OFS_RETURN + 2] = 0;
}

/* float(vector bindmaps) setbindmaps = #632 - no-op, returns 1 */
static void csqc_setbindmaps (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 1;
}

/*
 Sound. Implemented: #483 pointsound (S_PrecacheSound + S_StartSound(0,0,...) with
 origin - positional by origin). No-op (no analog in ezq): #351 SetListener (audio
 listener is fixed), #371 deltalisten (EXT_CSQC_1 prediction), #533 getsoundtime /
 #534 soundlength (no channel timings/sample length).
*/

/*
 void(vector origin, string sample, float volume, float attenuation,
      optional float pitchpct) pointsound = #483
 FTE: the 5th arg is pitch in percent (100 = normal), stored as the playback rate.
 ezq sound has no pitch/rate (the channel mixer has no rate) - the argument is
 accepted for call identity and ignored.
*/
static void csqc_pointsound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *org;
	char *sample;
	sfx_t *sfx;
	float pitchpct = 0;
	if (!vm)
		return;
	org = &vm->globals[OFS_PARM0];
	sample = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + 3]);
	if (vm->argc >= 5)
		pitchpct = vm->globals[OFS_PARM0 + 12] * 0.01f;	// only for call parity
	(void)pitchpct;
	if (!sample || !sample[0])
		return;
	sfx = S_PrecacheSound (sample);
	if (sfx)
		S_StartSound (0, 0, sfx, org, vm->globals[OFS_PARM0 + 6], vm->globals[OFS_PARM0 + 9]);
}

/* #351 SetListener / #371 deltalisten */
/*
 void(vector origin, vector forward, vector right, vector up) setlistener = #351
 The module sets the audio listener; cl_main.c uses it in S_Update while the module
 is active (otherwise the engine's view). FTE parity for sound prediction.
*/
static void csqc_setlistener (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_SetListener (&vm->globals[OFS_PARM0 + 0], &vm->globals[OFS_PARM0 + 3],
		&vm->globals[OFS_PARM0 + 6], &vm->globals[OFS_PARM0 + 9]);
}
/*
 float(string modelname, float(float isnew) updatecallback, float flags) deltalisten = #371
 Real registration (FTE PF_DeltaListen) - a callback registry by modelindex; the
 engine calls them on entity updates (the player_state bridge in
 CSQC_Client_DeltaPlayers). modelname="*" - all models; func<=0/invalid - removal.
*/
static void csqc_deltalisten (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *model;
	int func;
	if (!vm)
		return;
	model = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	func = *(int *)&vm->globals[OFS_PARM1];
	if (func < 0 || func >= vm->progs->numfunctions)
		func = 0;	// invalid pointer - removal/no-op
	CSQC_Client_DeltaListen (model, func, (int)vm->globals[OFS_PARM2]);
	vm->globals[OFS_RETURN] = 0;
}

/* #533 getsoundtime / #534 soundlength - no-op (no channel timings/length) */
static void csqc_getsoundtime (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = -1;
}
static void csqc_soundlength (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 0;
}

/*
 Entity reflection (#496-500). Works over vm->fielddefs[] (ddef_t: name/type/ofs in
 arena words, etype_t ev_*).
*/
static ddef_t *csqc_fielddef (pr1vm_t *vm, unsigned int fidx)
{
	if (!vm || !vm->fielddefs || !vm->progs)
		return NULL;
	if (fidx >= (unsigned int)vm->progs->numfielddefs)
		return NULL;
	return &vm->fielddefs[fidx];
}

/* float() numentityfields = #496 */
static void csqc_numentityfields (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm || !vm->progs)
		return;
	vm->globals[OFS_RETURN] = vm->progs->numfielddefs;
}

/* string(float fieldnum) entityfieldname = #497 */
static void csqc_entityfieldname (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	char *s;
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	if (!f)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	s = CSQC_Client_GetString (vm, f->s_name);
	CSQCVM_SetRetStr (s ? s : "");
}

/* float(float fieldnum) entityfieldtype = #498 (full ddef_t.type) */
static void csqc_entityfieldtype (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	vm->globals[OFS_RETURN] = f ? (float)f->type : 0;
}

/* string(float fieldnum, entity ent) getentityfieldstring = #499 */
static void csqc_getentityfieldstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	float *slot;
	int slotidx, type, ofs;
	char buf[512];
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	slotidx = csqc_ent_of (vm, OFS_PARM1);
	if (!f || slotidx < 0)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	slot = csqc_ent_slot (vm, slotidx);
	if (!slot)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	type = f->type & 0xff;
	ofs = f->ofs;
	switch (type)
	{
	case ev_string:
		{
			char *s = CSQC_Client_GetString (vm, *(int *)&slot[ofs]);
			CSQCVM_SetRetStr (s ? s : "");
			return;
		}
	case ev_vector:
		snprintf (buf, sizeof (buf), "%g %g %g", slot[ofs], slot[ofs + 1], slot[ofs + 2]);
		break;
	case ev_entity:
		{
			int v = *(int *)&slot[ofs];
			snprintf (buf, sizeof (buf), "entity %d", (vm->edict_size > 0) ? v / vm->edict_size : v);
			break;
		}
	case ev_float:
	default:
		csqc_q_ftoa (buf, sizeof (buf), slot[ofs]);
		break;
	}
	CSQCVM_SetRetStr (buf);
}

/* float(float fieldnum, entity ent, string s) putentityfieldstring = #500 */
static void csqc_putentityfieldstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	float *slot;
	int slotidx, type, ofs;
	char *s = CSQCVM_Str (OFS_PARM2);
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	slotidx = csqc_ent_of (vm, OFS_PARM1);
	vm->globals[OFS_RETURN] = 0;
	if (!f || slotidx < 0)
		return;
	slot = csqc_ent_slot (vm, slotidx);
	if (!slot)
		return;
	type = f->type & 0xff;
	ofs = f->ofs;
	switch (type)
	{
	case ev_string:
		PR1VM_ClientSetString (vm, (string_t *)&slot[ofs], s ? s : "");
		break;
	case ev_vector:
		{
			float v[3] = { 0, 0, 0 };
			int n = 0;
			if (s)
				n = sscanf (s, "%f %f %f", &v[0], &v[1], &v[2]);
			if (n > 0)
				VectorCopy (v, &slot[ofs]);
			else
				return;
			break;
		}
	case ev_float:
	default:
		slot[ofs] = (s && s[0]) ? (float)atof (s) : 0;
		break;
	}
	vm->globals[OFS_RETURN] = 1;
}

/*
 BSP surfaces. All no-op: FTE reads brush-model geometry
 (surfaces/mesh/plane/texture); ezq has no such model geometry interface. Registered
 to avoid "Bad builtin".
*/
static void csqc_bsp_nop_vec (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = 0;
	vm->globals[OFS_RETURN + 1] = 0;
	vm->globals[OFS_RETURN + 2] = 0;
}

/*
 Introspection/console. Implemented: #294 checkcommand (cmd->1, cvar->3, no alias->0),
 #295 argescape (own quoting), #607 isfunction. No-op: #391/#392/#393/#394 (FTE
 multi-console has no ezq analog), #605 callfunction (reentrant exec from a builtin
 is unsupported).
*/

/* float(string name) checkcommand = #294 */
static void csqc_checkcommand (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	if (name && name[0] && Cmd_Exists (name))
		vm->globals[OFS_RETURN] = 1;
	else if (name && name[0] && Cvar_Find (name))
		vm->globals[OFS_RETURN] = 3;
	else
		vm->globals[OFS_RETURN] = 0;
}

/* string(string s) argescape = #295 - wraps in quotes with escaping */
static void csqc_argescape (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s = CSQCVM_Str (OFS_PARM0);
	char buf[4096];
	int i, n, need = 0;
	if (!vm)
		return;
	s = s ? s : "";
	n = strlen (s);
	for (i = 0; i < n; i++)
		if (s[i] == ' ' || s[i] == '\t' || s[i] == '"' || s[i] == '\\' || s[i] == ';')
		{
			need = 1;
			break;
		}
	if (!need)
	{
		CSQCVM_SetRetStr ((char *)s);
		return;
	}
	{
		char *d = buf;
		*d++ = '"';
		for (i = 0; i < n && d < buf + sizeof (buf) - 3; i++)
		{
			if (s[i] == '"' || s[i] == '\\')
				*d++ = '\\';
			*d++ = s[i];
		}
		*d++ = '"';
		*d = 0;
	}
	CSQCVM_SetRetStr (buf);
}

/* float(string name) isfunction = #607 - the function exists in the module */
static void csqc_isfunction (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (name && name[0] && PR1VM_FindFunction (vm, name)) ? 1 : 0;
}

/*
 void(vector anglechange, optional float seat) CL_RotateMoves = #638 - FTE parity
 (PF_cl_RotateMoves): rotates the angles of unacknowledged usercmd. Body is
 CSQC_Client_RotateMoves (s_inhist). Invalid seat -> 0; for a valid seat FTE leaves
 the return value unset (void) - we leave it as is.
*/
static void csqc_cl_rotatemoves (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int seat;

	if (!vm)
		return;
	seat = (vm->argc > 1) ? (int)vm->globals[OFS_PARM1] : 0;
	if (seat != 0)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	CSQC_Client_RotateMoves (&vm->globals[OFS_PARM0], seat);
}

/*
 Lights/decals/skins. All no-op/approximations: ezq has no FTE decal/skin file
 subsystems (`Mod_*Skin`, `CL_AddDecal`), readback pics or `lfield_*` constants for
 `cl_dlights[]`. Registered so the module does not hit "Bad builtin".
*/
static void csqc_light_nop_ret0 (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 0;
}

/*
 System/VM remainder. Fully implemented: #98 findfloat (pool walk by float field,
 like FTE PF_FindFloat). #92 getlight - approximation (no light sample -> 0). No-op
 (server-world/no analog): #64 tracetoss, #240 checkpvs, #278 terrain_edit,
 #279 touchtriggers, #504 getentity.
*/

/*
 entity(entity start, .float fld, float match) findfloat = #98
 (findentity in FTE). Returns: the next used slot after start with an equal float
 field value; none - world (0).
*/
static void csqc_findfloat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, f;
	float match, *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = *(int *)&vm->globals[OFS_PARM0 + 3];
	match = vm->globals[OFS_PARM0 + 6];
	if (f < 0 || f >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindFloat: bad field reference");
		return;
	}
	if (e < 0)
		e = 0;
	for (e++; e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		// FTE compares raw 32-bit values (((int*)ed->v)[f] == G_INT(PARM2)), not
		// float (difference - -0.0/NaN).
		if (*(int *)&slot[f] == *(int *)&match)
		{
			csqc_ret_entity (vm, e);
			return;
		}
	}
	csqc_ret_entity (vm, 0);
}

/* vector(vector org) getlight = #92 - approximation: no static light sample -> 0 */
static void csqc_getlight_approx (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = 0;
	vm->globals[OFS_RETURN + 1] = 0;
	vm->globals[OFS_RETURN + 2] = 0;
}

/* no-op: #64/#240/#278/#279 - server-world/no analog */
static void csqc_vmrest_nop (void)
{
	/* no-op */
}

/*
 __variant(float entnum, float fieldnum) getentity = #504.
 FTE PF_getentity: interpolated state of an engine-network entity by server number.
 Implementation is CSQC_Client_GetEntity (csqc_client.c); we write 3 return words
 here (float fields occupy the first). Fields without an ezq source - FTE default.
*/
static void csqc_getentity (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float out[3];
	if (!vm)
		return;
	CSQC_Client_GetEntity ((int)vm->globals[OFS_PARM0], (int)vm->globals[OFS_PARM1], out);
	vm->globals[OFS_RETURN + 0] = out[0];
	vm->globals[OFS_RETURN + 1] = out[1];
	vm->globals[OFS_RETURN + 2] = out[2];
}

/*
 Simple system/VM builtins. The entity value in our classic mode = slot*edict_size;
 slot = ent_of (see #459/#512 - slot-index semantics, deviation from FTE "entnum").
*/

/* string(entity ent) etos = #65 - "entity <slot>" */
static void csqc_etos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int slot;
	char buf[32];
	if (!vm)
		return;
	slot = csqc_ent_of (vm, OFS_PARM0);
	snprintf (buf, sizeof (buf), "entity %d", slot > 0 ? slot : 0);
	CSQCVM_SetRetStr (buf);
}

/* void(string s, ...) print = #339 - console (Con_Printf). */
static void csqc_print (void)
{
	char *s = CSQCVM_VarString (0);
	if (s && s[0])
		Con_Printf ("%s", s);
}

/* void(string s, ...) cprint = #338 - center screen (SCR_CenterPrint, like FTE). */
static void csqc_cprint (void)
{
	char *s = CSQCVM_VarString (0);
	SCR_CenterPrint (s ? s : "");
}

/* float() isserver = #350 - is the server running? (the ezq client includes one).
   Deviation: no 0.5 distinction (no sv.allocated_client_slots in ezq). */
static void csqc_isserver (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (sv.state != ss_dead) ? 1 : 0;
}

/* float(entity ent) wasfreed = #353 - slot freed (remove). */
static void csqc_wasfreed (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int slot;
	if (!vm)
		return;
	slot = csqc_ent_of (vm, OFS_PARM0);
	vm->globals[OFS_RETURN] = (slot > 0 && !CSQC_Client_EntUsed (slot)) ? 1 : 0;
}

/* float(entity ent) num_for_edict = #512 - slot index (paired with #459). */
static void csqc_num_for_edict (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int slot;
	if (!vm)
		return;
	slot = csqc_ent_of (vm, OFS_PARM0);
	vm->globals[OFS_RETURN] = slot > 0 ? slot : 0;
}

/* #63 changepitch - no-op (moving angles to idealpitch is server mechanics). */
static void csqc_changepitch (void)
{
	/* no-op (like changeyaw #49) */
}

/*
 #639 digest_hex (FTE parity, PF_digest_hex): hash the varargs concatenation (from
 parm1) into a lowercase-hex tempstring. Supported names (exact uppercase match):
 "SHA1", "MD4", "CRC16". MD5/SHA-2 are not implemented in ezq - any other name (or a
 lowercase one) returns "" like FTE. ezq bin2hex is UPPERCASE - not usable, hence the
 local helper.
*/
/* MD4 context mirror of md4.c (md4.c has no header and is shared with mvdsv - do
   not touch). UINT4 == unsigned int on LP64/x86_64; the RSA MD4 layout is stable,
   keep in sync with md4.c. */
typedef struct {
	unsigned int state[4];
	unsigned int count[2];
	unsigned char buffer[64];
} csqc_md4_ctx_t;

extern void MD4Init (csqc_md4_ctx_t *);
extern void MD4Update (csqc_md4_ctx_t *, unsigned char *, unsigned int);
extern void MD4Final (unsigned char[16], csqc_md4_ctx_t *);

static void csqc_digest_to_hex (char *out, const unsigned char *digest, int len)
{
	static const char hex[] = "0123456789abcdef";
	int i;
	for (i = 0; i < len; i++)
	{
		out[i * 2 + 0] = hex[digest[i] >> 4];
		out[i * 2 + 1] = hex[digest[i] & 0xf];
	}
	out[i * 2] = 0;
}

static void csqc_digest_hex (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	unsigned char digest[DIGEST_SIZE];	// max digest here is SHA1 (20 bytes)
	char *name, *data, out[DIGEST_SIZE * 2 + 1];
	int len = 0;

	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	data = CSQCVM_VarString (1);
	if (!name)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	if (!strcmp (name, "SHA1"))
	{
		SHA1_CTX ctx;
		SHA1Init (&ctx);
		SHA1Update (&ctx, (unsigned char *)data, strlen (data));
		SHA1Final (digest, &ctx);
		len = DIGEST_SIZE;
	}
	else if (!strcmp (name, "MD4"))
	{
		csqc_md4_ctx_t ctx;
		MD4Init (&ctx);
		MD4Update (&ctx, (unsigned char *)data, (unsigned int)strlen (data));
		MD4Final (digest, &ctx);
		len = 16;
	}
	else if (!strcmp (name, "CRC16"))
	{
		unsigned short crc;
		int i;
		CRC_Init (&crc);
		for (i = 0; i < (int)strlen (data); i++)
			CRC_ProcessByte (&crc, (unsigned char)data[i]);
		crc = CRC_Value (crc);
		digest[0] = crc & 0xff;		// little-endian bytes, like FTE hash_crc16
		digest[1] = (crc >> 8) & 0xff;
		len = 2;
	}
	if (len)
	{
		csqc_digest_to_hex (out, digest, len);
		CSQCVM_SetRetStr (out);
	}
	else
		CSQCVM_SetRetStr ("");	// MD5/SHA-2/unknown - not implemented
}

/* #355 getentitytoken - deprecated/not needed: returns "". */
static void csqc_nop_str (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr ("");
}

void CSQCVM_RegisterBuiltins (pr1vm_t *vm)
{
	// #1 makevectors (FTE parity) - before the CSQC-specific ones.
	PR1VM_RegisterBuiltin (vm, 1, (builtin_t)csqc_makevectors); // #1 void() makevectors (QUAKE)

	// Reuse pure float/vector server PF_* (see the externs above): the attach in
	// PR1VM_ExecuteProgram points pr_globals at the executing VM.
	// #7/#13/#51 are client wrappers (FTE parity of return/args); the server
	// PF_random/PF_vectoyaw/PF_vectoangles are shared with the server - untouched.
	PR1VM_RegisterBuiltin (vm, 7,   (builtin_t)csqc_random); // #7 float() random (QUAKE)
	PR1VM_RegisterBuiltin (vm, 9,   (builtin_t)PF_normalize); // #9 vector(vector in) normalize (QUAKE)
	PR1VM_RegisterBuiltin (vm, 12,  (builtin_t)PF_vlen); // #12 float(vector v) vlen (QUAKE)
	PR1VM_RegisterBuiltin (vm, 13,  (builtin_t)csqc_vectoyaw); // #13 float(vector v) vectoyaw (QUAKE)
	PR1VM_RegisterBuiltin (vm, 29,  (builtin_t)PF_traceon); // #29 void() traceon (QUAKE)
	PR1VM_RegisterBuiltin (vm, 30,  (builtin_t)PF_traceoff); // #30 void() traceoff (QUAKE)
	PR1VM_RegisterBuiltin (vm, 36,  (builtin_t)PF_rint); // #36 float(float f) rint (QUAKE)
	PR1VM_RegisterBuiltin (vm, 37,  (builtin_t)PF_floor); // #37 float(float f) floor (QUAKE)
	PR1VM_RegisterBuiltin (vm, 38,  (builtin_t)PF_ceil); // #38 float(float f) ceil (QUAKE)
	PR1VM_RegisterBuiltin (vm, 43,  (builtin_t)PF_fabs); // #43 float(float f) fabs (QUAKE)
	PR1VM_RegisterBuiltin (vm, 51,  (builtin_t)csqc_vectoangles); // #51 vector(vector v) vectoangles (QUAKE)
	PR1VM_RegisterBuiltin (vm, 60,  (builtin_t)PF_sin); // #60 float(float angle) sin (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 61,  (builtin_t)PF_cos); // #61 float(float angle) cos (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 62,  (builtin_t)PF_sqrt); // #62 float(float value) sqrt (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 94,  (builtin_t)PF_min); // #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
	PR1VM_RegisterBuiltin (vm, 95,  (builtin_t)PF_max); // #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
	PR1VM_RegisterBuiltin (vm, 96,  (builtin_t)PF_bound); // #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
	// #97 pow / #91 randomvec - their pr_cmds.c bodies are static: light client
	// handlers (pure math, read/write vm->globals).
	PR1VM_RegisterBuiltin (vm, 97,  (builtin_t)csqc_pow); // #97 float(float value) pow (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 91,  (builtin_t)csqc_randomvec); // #91 vector() randomvec (DP_QC_RANDOMVEC)

	// cvar/exec/errors (#10/#11/#46/#72/#93/#99/#231; #28 coredump / #31 eprint).
	PR1VM_RegisterBuiltin (vm, 10,  (builtin_t)csqc_error); // #10 void(string errortext) error (QUAKE)
	PR1VM_RegisterBuiltin (vm, 11,  (builtin_t)csqc_objerror); // #11 void(string errortext) onjerror (QUAKE)
	PR1VM_RegisterBuiltin (vm, 46,  (builtin_t)csqc_localcmd); // #46 void(string str) localcmd (QUAKE)
	PR1VM_RegisterBuiltin (vm, 72,  (builtin_t)csqc_cvar_set); // #72 void(string cvarname, string valuetoset) cvar_set (QUAKE)
	PR1VM_RegisterBuiltin (vm, 93,  (builtin_t)csqc_registercvar); // #93 void(string cvarname, string defaultvalue) registercvar (DP_QC_REGISTERCVAR)
	PR1VM_RegisterBuiltin (vm, 99,  (builtin_t)csqc_checkextension); // #99 float(string extname) checkextension (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 231, (builtin_t)csqc_calltimeofday); // #231 void() calltimeofday

	// strings/conversions (#118/#119 - ring/no-op).
	PR1VM_RegisterBuiltin (vm, 27,  (builtin_t)csqc_vtos); // #27 string(vector f) vtos (QUAKE)
	PR1VM_RegisterBuiltin (vm, 81,  (builtin_t)csqc_stof); // #81 float(string s) stof (FRIK_FILE or QW_ENGINE)
	PR1VM_RegisterBuiltin (vm, 114, (builtin_t)csqc_strlen); // #114 float(string str) strlen (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 116, (builtin_t)csqc_substring); // #116 string(string str, float start, float length) substring (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 117, (builtin_t)csqc_stov); // #117 vector(string str) stov (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 118, (builtin_t)csqc_strzone); // #118 string(string str) dupstring (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 119, (builtin_t)csqc_strunzone); // #119 void(string str) freestring (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 448, (builtin_t)csqc_cvar_string); // #448 string(float n) cvar_string (DP_QC_CVAR_STRING)

	// client subsystems.
	PR1VM_RegisterBuiltin (vm, 6,   (builtin_t)csqc_breakpoint); // #6 void() debugbreak (QUAKE)
	PR1VM_RegisterBuiltin (vm, 8,   (builtin_t)csqc_sound); // #8 void(entity e, float chan, string samp, float vol, float atten) sound (QUAKE)
	PR1VM_RegisterBuiltin (vm, 19,  (builtin_t)csqc_precache_sound); // #19 void(string str) precache_sound (QUAKE)
	PR1VM_RegisterBuiltin (vm, 20,  (builtin_t)csqc_precache_model); // #20 void(string str) precache_model (QUAKE)
	PR1VM_RegisterBuiltin (vm, 35,  (builtin_t)csqc_lightstyle); // #35 void(float lightstyle, string stylestring) lightstyle (QUAKE)
	PR1VM_RegisterBuiltin (vm, 48,  (builtin_t)csqc_particle); // #48 void(vector org, vector dir, float colour, float count) particle (QUAKE)
	PR1VM_RegisterBuiltin (vm, 68,  (builtin_t)csqc_precache_file); // #68 void(string s) precache_file (QUAKE) (don't support)
	PR1VM_RegisterBuiltin (vm, 74,  (builtin_t)csqc_ambientsound); // #74 void (vector pos, string samp, float vol, float atten) ambientsound (QUAKE)
	PR1VM_RegisterBuiltin (vm, 75,  (builtin_t)csqc_precache_model); // #75 void(string str) precache_model2 (QUAKE)
	PR1VM_RegisterBuiltin (vm, 76,  (builtin_t)csqc_precache_sound); // #76 void(string str) precache_sound2 (QUAKE)
	PR1VM_RegisterBuiltin (vm, 77,  (builtin_t)csqc_precache_file); // #77 void(string str) precache_file2 (QUAKE)
	PR1VM_RegisterBuiltin (vm, 531, (builtin_t)csqc_setpause); // #531 ?

	// basic entities on the arena.
	PR1VM_RegisterBuiltin (vm, 2,   (builtin_t)csqc_setorigin); // #2 void(entity e, vector org) setorigin (QUAKE)
	PR1VM_RegisterBuiltin (vm, 3,   (builtin_t)csqc_setmodel); // #3 void(entity e, string modl) setmodel (QUAKE)
	PR1VM_RegisterBuiltin (vm, 4,   (builtin_t)csqc_setsize); // #4 void(entity e, vector mins, vector maxs) setsize (QUAKE)
	PR1VM_RegisterBuiltin (vm, 14,  (builtin_t)csqc_spawn); // #14 entity() spawn (QUAKE)
	PR1VM_RegisterBuiltin (vm, 15,  (builtin_t)csqc_remove); // #15 void(entity e) remove (QUAKE)
	PR1VM_RegisterBuiltin (vm, 18,  (builtin_t)csqc_find); // #18 entity(entity start, .string fld, string match) findstring (QUAKE)
	PR1VM_RegisterBuiltin (vm, 22,  (builtin_t)csqc_findradius); // #22 entity(vector org, float rad) findradius (QUAKE)
	PR1VM_RegisterBuiltin (vm, 28,  (builtin_t)csqc_coredump); // #28 void(void) coredump (QUAKE)
	PR1VM_RegisterBuiltin (vm, 31,  (builtin_t)csqc_eprint); // #31 void(entity e) eprint (QUAKE)
	PR1VM_RegisterBuiltin (vm, 40,  (builtin_t)csqc_checkbottom); // #40 float(entity e) checkbottom (QUAKE)
	PR1VM_RegisterBuiltin (vm, 47,  (builtin_t)csqc_nextent); // #47 entity(entity e) nextent (QUAKE)
	PR1VM_RegisterBuiltin (vm, 49,  (builtin_t)csqc_changeyaw); // #49 void() changeyaw (QUAKE)
	PR1VM_RegisterBuiltin (vm, 69,  (builtin_t)csqc_makestatic); // #69 void(entity e) makestatic (QUAKE)
	PR1VM_RegisterBuiltin (vm, 80,  (builtin_t)csqc_infokey); // #80 string(entity e, string keyname) infokey (QW_ENGINE) (don't support)

	// world traces/physics (world-only).
	PR1VM_RegisterBuiltin (vm, 16,  (builtin_t)csqc_traceline); // #16 void(vector v1, vector v2, float nomonst, entity forent) traceline (QUAKE)
	PR1VM_RegisterBuiltin (vm, 90,  (builtin_t)csqc_tracebox); // #90 void(vector start, vector mins, vector maxs, vector end, float nomonsters, entity ent) tracebox
	PR1VM_RegisterBuiltin (vm, 41,  (builtin_t)csqc_pointcontents); // #41 float(vector org) pointcontents (QUAKE)
	PR1VM_RegisterBuiltin (vm, 32,  (builtin_t)csqc_walkmove); // #32 float(float yaw, float dist) walkmove (QUAKE)
	PR1VM_RegisterBuiltin (vm, 34,  (builtin_t)csqc_droptofloor); // #34 float() droptofloor
	PR1VM_RegisterBuiltin (vm, 67,  (builtin_t)csqc_movetogoal); // #67 void(float step) movetogoal (QUAKE)

	PR1VM_RegisterBuiltin (vm, 25, (builtin_t)csqc_dprint); // #25 void(string s, ...) dprint (QUAKE)
	PR1VM_RegisterBuiltin (vm, 26, (builtin_t)csqc_ftos); // #26 string(float f) ftos (QUAKE)
	PR1VM_RegisterBuiltin (vm, 45, (builtin_t)csqc_cvar); // #45 float(string cvarname) cvar (QUAKE)

	// input/interface: #340 keynumtostring, #341 stringtokeynum.
	PR1VM_RegisterBuiltin (vm, 340, (builtin_t)csqc_keynumtostring); // #340 string(float keynum) keynumtostring (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 341, (builtin_t)csqc_stringtokeynum); // #341 float(string keyname) stringtokeynum (EXT_CSQC)
	// #349 isdemo, #354 serverkey.
	PR1VM_RegisterBuiltin (vm, 349, (builtin_t)csqc_isdemo); // #349 float() isdemo (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 354, (builtin_t)csqc_serverkey); // #354 string(string key) serverkey;
	// #343 setcursormode (full: module cursor in the CSQC overlay).
	PR1VM_RegisterBuiltin (vm, 343, (builtin_t)csqc_setcursormode); // #343 void(float usecursor, optional string cursorimage, optional vector hotspot, optional float scale) setcursormode
	// #344 getmousepos (read path of the CSQC cursor position).
	PR1VM_RegisterBuiltin (vm, 344, (builtin_t)csqc_getmousepos); // #344 vector() getmousepos
	// #348 getplayerkeyvalue.
	PR1VM_RegisterBuiltin (vm, 348, (builtin_t)csqc_getplayerkeyvalue); // #348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
	// #346 setsensitivityscaler.
	PR1VM_RegisterBuiltin (vm, 346, (builtin_t)csqc_setsensitivityscaler); // #346 void(float sens) setsensitivityscaler (EXT_CSQC)
	// #345 getinputstate.
	PR1VM_RegisterBuiltin (vm, 345, (builtin_t)csqc_getinputstate); // #345 float(float framenum) getinputstate (EXT_CSQC)
	// #347 runstandardplayerphysics.
	PR1VM_RegisterBuiltin (vm, 347, (builtin_t)csqc_runstandardplayerphysics); // #347 void() runstandardplayerphysics (EXT_CSQC)
	// #459 edict_num.
	PR1VM_RegisterBuiltin (vm, 459, (builtin_t)csqc_edict_num); // #459 entity(float entnum) edict_num
	// #460-469 string-buffers.
	PR1VM_RegisterBuiltin (vm, 460, (builtin_t)csqc_buf_create); // #460 float() buf_create
	PR1VM_RegisterBuiltin (vm, 461, (builtin_t)csqc_buf_del); // #461 void(float bufhandle) buf_del
	PR1VM_RegisterBuiltin (vm, 462, (builtin_t)csqc_buf_getsize); // #462 float(float bufhandle) buf_getsize
	PR1VM_RegisterBuiltin (vm, 463, (builtin_t)csqc_buf_copy); // #463 void(float bufhandle_from, float bufhandle_to) buf_copy
	PR1VM_RegisterBuiltin (vm, 464, (builtin_t)csqc_buf_sort); // #464 void(float bufhandle, float sortpower, float backward) buf_sort
	PR1VM_RegisterBuiltin (vm, 465, (builtin_t)csqc_buf_implode); // #465 string(float bufhandle, string glue) buf_implode
	PR1VM_RegisterBuiltin (vm, 466, (builtin_t)csqc_bufstr_get); // #466 string(float bufhandle, float string_index) bufstr_get
	PR1VM_RegisterBuiltin (vm, 467, (builtin_t)csqc_bufstr_set); // #467 void(float bufhandle, float string_index, string str) bufstr_set
	PR1VM_RegisterBuiltin (vm, 468, (builtin_t)csqc_bufstr_add); // #468 float(float bufhandle, string str, float order) bufstr_add
	PR1VM_RegisterBuiltin (vm, 469, (builtin_t)csqc_bufstr_free); // #469 void(float bufhandle, float string_index) bufstr_free
	// #177 localsound, #305 dynamiclight_add.
	PR1VM_RegisterBuiltin (vm, 177, (builtin_t)csqc_localsound); // #177 void(string soundname, optional float channel, optional float volume) localsound
	PR1VM_RegisterBuiltin (vm, 305, (builtin_t)csqc_dynamiclight_add); // #305 float(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)
	// #335-337 particles (mini registry).
	PR1VM_RegisterBuiltin (vm, 335, (builtin_t)csqc_particleeffectnum); // #335 float(string effectname) particleeffectnum (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 336, (builtin_t)csqc_trailparticles); // #336 void(float effectnum, entity ent, vector start, vector end) trailparticles (EXT_CSQC),
	PR1VM_RegisterBuiltin (vm, 337, (builtin_t)csqc_pointparticles); // #337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)
	// the approximated te_* group (#405-427, except #426).
	PR1VM_RegisterBuiltin (vm, 405, (builtin_t)csqc_te_blood); // #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
	PR1VM_RegisterBuiltin (vm, 406, (builtin_t)csqc_te_bloodshower); // #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
	PR1VM_RegisterBuiltin (vm, 407, (builtin_t)csqc_te_explosionrgb); // #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
	PR1VM_RegisterBuiltin (vm, 408, (builtin_t)csqc_te_particlecube); // #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
	PR1VM_RegisterBuiltin (vm, 409, (builtin_t)csqc_te_rain); // #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
	PR1VM_RegisterBuiltin (vm, 410, (builtin_t)csqc_te_rain); // #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
	PR1VM_RegisterBuiltin (vm, 411, (builtin_t)csqc_te_spark); // #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
	PR1VM_RegisterBuiltin (vm, 412, (builtin_t)csqc_te_quad); // #412 void(vector org) te_gunshotquad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 413, (builtin_t)csqc_te_quad); // #413 void(vector org) te_spikequad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 414, (builtin_t)csqc_te_quad); // #414 void(vector org) te_superspikequad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 415, (builtin_t)csqc_te_quad); // #415 void(vector org) te_explosionquad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 416, (builtin_t)csqc_te_smallflash); // #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
	PR1VM_RegisterBuiltin (vm, 417, (builtin_t)csqc_te_customflash); // #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
	PR1VM_RegisterBuiltin (vm, 418, (builtin_t)csqc_te_gunshot); // #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 419, (builtin_t)csqc_te_spike); // #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 420, (builtin_t)csqc_te_superspike); // #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 421, (builtin_t)csqc_te_explosion); // #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 422, (builtin_t)csqc_te_tarexplosion); // #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 423, (builtin_t)csqc_te_wizspike); // #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 424, (builtin_t)csqc_te_knightspike); // #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 425, (builtin_t)csqc_te_lavasplash); // #425 void(vector org) te_lavasplash  (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 427, (builtin_t)csqc_te_explosion2); // #427 void(vector org, float color, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
	// beams #428-431.
	PR1VM_RegisterBuiltin (vm, 428, (builtin_t)csqc_te_lightning1); // #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 429, (builtin_t)csqc_te_lightning2); // #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 430, (builtin_t)csqc_te_lightning3); // #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 431, (builtin_t)csqc_te_beam); // #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 115, (builtin_t)csqc_strcat); // #115 string(string str1, string str2, ...) strcat (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 221, (builtin_t)csqc_strstrofs); // #221 float(string s1, string sub) strstrofs (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 352, (builtin_t)csqc_registercommand); // #352 void(string cmdname) registercommand (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 441, (builtin_t)csqc_tokenize); // #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
	PR1VM_RegisterBuiltin (vm, 442, (builtin_t)csqc_argv); // #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)

	// math: pure C, no state.
	PR1VM_RegisterBuiltin (vm, 471, (builtin_t)csqc_asin); // #471 float(float s) asin
	PR1VM_RegisterBuiltin (vm, 472, (builtin_t)csqc_acos); // #472 float(float c) acos
	PR1VM_RegisterBuiltin (vm, 473, (builtin_t)csqc_atan); // #473 float(float t) atan
	PR1VM_RegisterBuiltin (vm, 474, (builtin_t)csqc_atan2); // #474 float(float c, float s) atan2
	PR1VM_RegisterBuiltin (vm, 475, (builtin_t)csqc_tan); // #475 float(float a) tan
	PR1VM_RegisterBuiltin (vm, 532, (builtin_t)csqc_log); // #532 float(float value, optional float base) log
	PR1VM_RegisterBuiltin (vm, 102, (builtin_t)csqc_anglemod); // #102 float(float value) anglemod
	PR1VM_RegisterBuiltin (vm, 245, (builtin_t)csqc_mod); // #245 float(float a, float b) mod
	PR1VM_RegisterBuiltin (vm, 218, (builtin_t)csqc_bitshift); // #218 bitshift (EXT_DIMENSION_PLANES)
	PR1VM_RegisterBuiltin (vm, 494, (builtin_t)csqc_crc16); // #494 float(float caseinsensitive, string s, ...) crc16
	PR1VM_RegisterBuiltin (vm, 519, (builtin_t)csqc_gettimef); // #519 float(optional float timetype) gettime

	// int/hex conversions: #259-262.
	PR1VM_RegisterBuiltin (vm, 259, (builtin_t)csqc_stoi); // #259 int(string) stoi
	PR1VM_RegisterBuiltin (vm, 260, (builtin_t)csqc_itos); // #260 string(int) itos
	PR1VM_RegisterBuiltin (vm, 261, (builtin_t)csqc_stoh); // #261 int(string) stoh
	PR1VM_RegisterBuiltin (vm, 262, (builtin_t)csqc_htos); // #262 string(int) htos

	// cvar metadata: #482/#495/#518.
	PR1VM_RegisterBuiltin (vm, 482, (builtin_t)csqc_cvar_defstring); // #482 string(string s) cvar_defstring
	PR1VM_RegisterBuiltin (vm, 495, (builtin_t)csqc_cvar_type); // #495 float(string name) cvar_type
	PR1VM_RegisterBuiltin (vm, 518, (builtin_t)csqc_cvar_description); // #518 string(string cvarname) cvar_description

	// simple strings: #222/223/225/226/227/228/229/230/480/481/484/485.
	PR1VM_RegisterBuiltin (vm, 222, (builtin_t)csqc_str2chr); // #222 float(string str, float index) str2chr (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 223, (builtin_t)csqc_chr2str); // #223 string(float chr, ...) chr2str (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 225, (builtin_t)csqc_strpad); // #225 string strpad(float pad, string str1, ...) strpad (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 226, (builtin_t)csqc_infoadd); // #226 string(string old, string key, string value) infoadd
	PR1VM_RegisterBuiltin (vm, 227, (builtin_t)csqc_infoget); // #227 string(string info, string key) infoget
	PR1VM_RegisterBuiltin (vm, 228, (builtin_t)csqc_strncmp); // #228 float(string s1, string s2) strcmp (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 229, (builtin_t)csqc_strncasecmp); // #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 230, (builtin_t)csqc_strncasecmp); // #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 480, (builtin_t)csqc_strtolower); // #480 string(string s) strtolower
	PR1VM_RegisterBuiltin (vm, 481, (builtin_t)csqc_strtoupper); // #481 string(string s) strlennocol
	PR1VM_RegisterBuiltin (vm, 484, (builtin_t)csqc_strreplace); // #484 string(string search, string replace, string subject) strreplace
	PR1VM_RegisterBuiltin (vm, 485, (builtin_t)csqc_strireplace); // #485 string(string search, string replace, string subject) strireplace

	// simple system/VM: #65/#338/#339/#350/#353/#512 + no-op #63/#355.
	PR1VM_RegisterBuiltin (vm, 65,  (builtin_t)csqc_etos); // #65 string(entity ent) etos (DP_QC_ETOS)
	PR1VM_RegisterBuiltin (vm, 338, (builtin_t)csqc_cprint); // #338 void(string s) cprint (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 339, (builtin_t)csqc_print); // #339 void(string s) print (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 350, (builtin_t)csqc_isserver); // #350 float() isserver (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 353, (builtin_t)csqc_wasfreed); // #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)
	PR1VM_RegisterBuiltin (vm, 512, (builtin_t)csqc_num_for_edict); // #512 float(entity ent) num_for_edict
	PR1VM_RegisterBuiltin (vm, 63,  (builtin_t)csqc_changepitch); // #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
	PR1VM_RegisterBuiltin (vm, 355, (builtin_t)csqc_nop_str); // #355 string() getentitytoken;

	// strings/tokenization: #478/#514/#479/#515/#516.
	PR1VM_RegisterBuiltin (vm, 478, (builtin_t)csqc_strftime); // #478 string(float uselocaltime, string format, ...) strftime
	PR1VM_RegisterBuiltin (vm, 514, (builtin_t)csqc_tokenize_console); // #514 float(string str) tokenize_console
	PR1VM_RegisterBuiltin (vm, 479, (builtin_t)csqc_tokenizebyseparator); // #479 float(string s, string separator1, ...) tokenizebyseparator
	PR1VM_RegisterBuiltin (vm, 515, (builtin_t)csqc_argv_start_index); // #515 float(float idx) argv_start_index
	PR1VM_RegisterBuiltin (vm, 516, (builtin_t)csqc_argv_end_index); // #516 float(float idx) argv_end_index

	// input/keyboard/menu: #342/#520/#521/#603/#604/#608/#609/#610/#614/#630/#631/#632.
	PR1VM_RegisterBuiltin (vm, 342, (builtin_t)csqc_getkeybind); // #342 string(float keynum) getkeybind (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 520, (builtin_t)csqc_keynumtostring_menu); // #520 string(float keynum) keynumtostring_omgwtf
	PR1VM_RegisterBuiltin (vm, 521, (builtin_t)csqc_findkeysforcommand); // #521 string(string command, optional float bindmap) findkeysforcommand
	PR1VM_RegisterBuiltin (vm, 603, (builtin_t)csqc_setmousetarget); // #603 void(float trg) setmousetarget
	PR1VM_RegisterBuiltin (vm, 604, (builtin_t)csqc_getmousetarget); // #604 float() getmousetarget
	PR1VM_RegisterBuiltin (vm, 608, (builtin_t)csqc_getresolution); // #608 vector(float vidmode, optional float forfullscreen) getresolution
	PR1VM_RegisterBuiltin (vm, 609, (builtin_t)csqc_keynumtostring_menu); // #609 string(float keynum) keynumtostring_menu
	PR1VM_RegisterBuiltin (vm, 610, (builtin_t)csqc_findkeysforcommand); // #610 string(string command, optional float bindmap) findkeysforcommand_dp
	PR1VM_RegisterBuiltin (vm, 614, (builtin_t)csqc_stringtokeynum_menu); // #614 float(string key) stringtokeynum_menu
	PR1VM_RegisterBuiltin (vm, 630, (builtin_t)csqc_setkeybind); // #630 void(float keynum, string binding, optional float bindmap) setkeybind
	PR1VM_RegisterBuiltin (vm, 631, (builtin_t)csqc_getbindmaps); // #631 vector() getbindmaps
	PR1VM_RegisterBuiltin (vm, 632, (builtin_t)csqc_setbindmaps); // #632 float(vector bindmaps) setbindmaps

	// system/VM remainder: #98 + #92-approx + no-op #64/#240/#278/#279.
	PR1VM_RegisterBuiltin (vm, 98,  (builtin_t)csqc_findfloat); // #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
	PR1VM_RegisterBuiltin (vm, 92,  (builtin_t)csqc_getlight_approx); // #92 vector(vector org) getlight (DP_QC_GETLIGHT)
	PR1VM_RegisterBuiltin (vm, 64,  (builtin_t)csqc_vmrest_nop); // #64 void(entity ent, entity ignore) tracetoss (DP_QC_TRACETOSS)
	PR1VM_RegisterBuiltin (vm, 240, (builtin_t)csqc_vmrest_nop); // #240 float(vector viewpos, entity entity) checkpvs
	PR1VM_RegisterBuiltin (vm, 278, (builtin_t)csqc_vmrest_nop); // #278 void(float action, vector pos, float radius, float quant) terrain_edit
	PR1VM_RegisterBuiltin (vm, 279, (builtin_t)csqc_vmrest_nop); // #279 void() touchtriggers
	PR1VM_RegisterBuiltin (vm, 504, (builtin_t)csqc_getentity); // #504 __variant(float entnum, float fieldnum) getentity

	// sound: #483 + no-op #351/#371/#533/#534.
	PR1VM_RegisterBuiltin (vm, 483, (builtin_t)csqc_pointsound); // #483 void(vector origin, string sample, float volume, float attenuation) pointsound
	PR1VM_RegisterBuiltin (vm, 351, (builtin_t)csqc_setlistener); // #351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 371, (builtin_t)csqc_deltalisten); // #371 float(string modelname, float flags) deltalisten  (EXT_CSQC_1)
	PR1VM_RegisterBuiltin (vm, 533, (builtin_t)csqc_getsoundtime); // #533 float(entity e, float channel) getsoundtime
	PR1VM_RegisterBuiltin (vm, 534, (builtin_t)csqc_soundlength); // #534 float(string sample) soundlength

	// lights/decals/skins: all no-op (no analogs in ezq).
	PR1VM_RegisterBuiltin (vm, 372, (builtin_t)csqc_light_nop_ret0); // #372 __variant(float lno, float fld) dynamiclight_get
	PR1VM_RegisterBuiltin (vm, 373, (builtin_t)csqc_light_nop_ret0); // #373 void(float lno, float fld, __variant value) dynamiclight_set
	PR1VM_RegisterBuiltin (vm, 375, (builtin_t)csqc_light_nop_ret0); // #375 void(string shadername, vector origin, vector up, vector side, vector rgb, float alpha) adddecal
	PR1VM_RegisterBuiltin (vm, 376, (builtin_t)csqc_light_nop_ret0); // #376 void(entity e, string skinfilename, optional string skindata) setcustomskin
	PR1VM_RegisterBuiltin (vm, 377, (builtin_t)csqc_light_nop_ret0); // #377 ?
	PR1VM_RegisterBuiltin (vm, 378, (builtin_t)csqc_light_nop_ret0); // #378 ?
	PR1VM_RegisterBuiltin (vm, 379, (builtin_t)csqc_light_nop_ret0); // #379 ?
	PR1VM_RegisterBuiltin (vm, 501, (builtin_t)csqc_light_nop_ret0); // #501 void(float to, string s, float sz) WritePicture

	// introspection/console: #294/#295/#607 + no-op #391-394/#605.
	PR1VM_RegisterBuiltin (vm, 294, (builtin_t)csqc_checkcommand); // #294 float(string name) checkcommand
	PR1VM_RegisterBuiltin (vm, 295, (builtin_t)csqc_argescape); // #295 string(string s) argescape
	PR1VM_RegisterBuiltin (vm, 607, (builtin_t)csqc_isfunction); // #607 float(string s) isfunction
	PR1VM_RegisterBuiltin (vm, 391, (builtin_t)csqc_nop_str); // #391 string(string conname, string field, optional string newvalue) con_getset
	PR1VM_RegisterBuiltin (vm, 392, (builtin_t)csqc_vmrest_nop); // #392 void(string conname, string messagefmt, ...) con_printf
	PR1VM_RegisterBuiltin (vm, 393, (builtin_t)csqc_vmrest_nop); // #393 void(string conname, vector pos, vector size, float fontsize) con_draw
	PR1VM_RegisterBuiltin (vm, 394, (builtin_t)csqc_light_nop_ret0); // #394 float(string conname, float inevtype, float parama, float paramb, float paramc) con_input
	PR1VM_RegisterBuiltin (vm, 605, (builtin_t)csqc_vmrest_nop); // #605 void(.../*, string funcname*/) callfunction

	// BSP surfaces: all no-op (no geometry interface).
	PR1VM_RegisterBuiltin (vm, 434, (builtin_t)csqc_light_nop_ret0); // #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 435, (builtin_t)csqc_bsp_nop_vec); // #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 436, (builtin_t)csqc_bsp_nop_vec); // #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 437, (builtin_t)csqc_nop_str); // #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 438, (builtin_t)csqc_light_nop_ret0); // #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 439, (builtin_t)csqc_bsp_nop_vec); // #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 486, (builtin_t)csqc_bsp_nop_vec); // #486 vector(entity e, float s, float n, float a) getsurfacepointattribute
	PR1VM_RegisterBuiltin (vm, 628, (builtin_t)csqc_light_nop_ret0); // #628 float(entity e, float s) getsurfacenumtriangles
	PR1VM_RegisterBuiltin (vm, 629, (builtin_t)csqc_bsp_nop_vec); // #629 vector(entity e, float s, float n) getsurfacetriangle

	// entity reflection: #496-500 over the module's fielddefs.
	PR1VM_RegisterBuiltin (vm, 496, (builtin_t)csqc_numentityfields); // #496 float() numentityfields
	PR1VM_RegisterBuiltin (vm, 497, (builtin_t)csqc_entityfieldname); // #497 string(float fieldnum) entityfieldname
	PR1VM_RegisterBuiltin (vm, 498, (builtin_t)csqc_entityfieldtype); // #498 float(float fieldnum) entityfieldtype
	PR1VM_RegisterBuiltin (vm, 499, (builtin_t)csqc_getentityfieldstring); // #499 string(float fieldnum, entity ent) getentityfieldstring
	PR1VM_RegisterBuiltin (vm, 500, (builtin_t)csqc_putentityfieldstring); // #500 float(float fieldnum, entity ent, string s) putentityfieldstring

	// visual layer B (2D overlay).
	PR1VM_RegisterBuiltin (vm, 300, (builtin_t)csqc_clearscene); // #300 void() clearscene (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 301, (builtin_t)csqc_addentities); // #301 void(float mask) addentities (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 303, (builtin_t)csqc_setproperty); // #303 float(float property, ...) setproperty (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 304, (builtin_t)csqc_renderscene); // #304 void() renderscene (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 309, (builtin_t)csqc_getproperty); // #309 vector/float(float property) getproperty (EXT_CSQC_1)
	PR1VM_RegisterBuiltin (vm, 326, (builtin_t)csqc_drawstring); // #326 float(vector position, string text, vector size, vector rgb, float alpha, float drawflag) drawstring
	PR1VM_RegisterBuiltin (vm, 315, (builtin_t)csqc_drawline); // #315 void(float width, vector pos1, vector pos2) drawline (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 317, (builtin_t)csqc_precache_pic); // #317 string(string name, float trywad) precache_pic (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 320, (builtin_t)csqc_drawcharacter); // #320 float(vector position, float character, vector scale, vector rgb, float alpha [, float flag]) drawcharacter (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 322, (builtin_t)csqc_drawpic); // #322 float(vector position, string pic, vector size, vector rgb, float alpha [, float flag]) drawpic (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 323, (builtin_t)csqc_drawfill); // #323 float(vector position, vector size, vector rgb, float alpha [, float flag]) drawfill (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 327, (builtin_t)csqc_stringwidth); // #327 float(string text, float usecolours, optional vector fontsize) stringwidth
	PR1VM_RegisterBuiltin (vm, 328, (builtin_t)csqc_drawsubpic); // #328 void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb, float alpha, optional float drawflag) drawsubpic
	// additional 2D graphics: #316/#318/#319/#321/#324/#325 + no-op #329.
	PR1VM_RegisterBuiltin (vm, 316, (builtin_t)csqc_iscachedpic); // #316 float(string name) iscachedpic (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 318, (builtin_t)csqc_drawgetimagesize); // #318 vector(string picname) draw_getimagesize (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 319, (builtin_t)csqc_freepic); // #319 void(string name) freepic (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 321, (builtin_t)csqc_drawrawstring); // #321 float(vector position, string text, vector scale, vector rgb, float alpha [, float flag]) drawstring (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 324, (builtin_t)csqc_drawsetcliparea); // #324 void(float x, float y, float width, float height) drawsetcliparea (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 325, (builtin_t)csqc_drawresetcliparea); // #325 void(void) drawresetcliparea (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 329, (builtin_t)csqc_drawrotpic_dp); // #329 void(vector pivot, string picname, vector size, vector mins, float angle, vector rgb, float alpha, optional float drawflag) drawrotpic_dp
	PR1VM_RegisterBuiltin (vm, 330, (builtin_t)csqc_getstati); // #330 int(float stnum) getstati (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 331, (builtin_t)csqc_getstatf); // #331 float(float stnum) getstatf (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 332, (builtin_t)csqc_getstats); // #332 string(float firststnum) getstats (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 359, (builtin_t)csqc_sendevent); // #359 void(string evname, string evargs, ...) (EXT_CSQC_1)
	PR1VM_RegisterBuiltin (vm, 627, (builtin_t)csqc_sprintf); // #627 string(string fmt, ...) sprintf

	// read* minimum (full set #360-368).
	PR1VM_RegisterBuiltin (vm, 360, (builtin_t)csqc_readbyte); // #360 float() readbyte (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 361, (builtin_t)csqc_readchar); // #361 float() readchar (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 362, (builtin_t)csqc_readshort); // #362 float() readshort (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 363, (builtin_t)csqc_readlong); // #363 float() readlong (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 364, (builtin_t)csqc_readcoord); // #364 float() readcoord (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 365, (builtin_t)csqc_readangle); // #365 float() readangle (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 366, (builtin_t)csqc_readstring); // #366 string() readstring (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 367, (builtin_t)csqc_readfloat); // #367 float() readfloat (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 368, (builtin_t)csqc_readentitynum); // #368 float() readentitynum (EXT_CSQC)

	// VOID stubs - type-correct no-op.
	PR1VM_RegisterBuiltin (vm, 111, (builtin_t)csqc_vmrest_nop); // #111 void(float fnum) fclose (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 113, (builtin_t)csqc_vmrest_nop); // #113 void(float fnum, string str) fputs (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 204, (builtin_t)csqc_vmrest_nop); // #204 void(float prnum, __variant newval, string varname) externset - no-op in classic v6: variant write to another program (no v6 types)
	PR1VM_RegisterBuiltin (vm, 207, (builtin_t)csqc_vmrest_nop); // #207 void(entity portal, float state) openportal
	PR1VM_RegisterBuiltin (vm, 210, (builtin_t)csqc_vmrest_nop); // #210 void() fork
	PR1VM_RegisterBuiltin (vm, 211, (builtin_t)csqc_vmrest_nop); // #211 void() abort (FTE_MULTITHREADED)
	PR1VM_RegisterBuiltin (vm, 212, (builtin_t)csqc_vmrest_nop); // #212 void() sleep
	PR1VM_RegisterBuiltin (vm, 215, (builtin_t)csqc_vmrest_nop); // #215 215 (FTE_PEXT_HEXEN2)
	PR1VM_RegisterBuiltin (vm, 216, (builtin_t)csqc_vmrest_nop); // #216 216 (FTE_PEXT_HEXEN2)
	PR1VM_RegisterBuiltin (vm, 217, (builtin_t)csqc_vmrest_nop); // #217 217 (FTE_PEXT_HEXEN2)
	PR1VM_RegisterBuiltin (vm, 219, (builtin_t)csqc_vmrest_nop); // #219 te_lightningblood void(vector org) (FTE_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 234, (builtin_t)csqc_light_nop_ret0); // #234 float(entity ent) isbackbuffered
	PR1VM_RegisterBuiltin (vm, 235, (builtin_t)csqc_vmrest_nop); // #235 void(vector angle) rotatevectorsbyangle
	PR1VM_RegisterBuiltin (vm, 236, (builtin_t)csqc_vmrest_nop); // #236 void(vector fwd, vector right, vector up) rotatevectorsbyvectors
	PR1VM_RegisterBuiltin (vm, 239, (builtin_t)csqc_vmrest_nop); // #239 void te_bloodqw(vector org[, float count]) (FTE_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 271, (builtin_t)csqc_vmrest_nop); // #271 void(float skel, float bonenum, vector org) skel_set_bone
	PR1VM_RegisterBuiltin (vm, 272, (builtin_t)csqc_vmrest_nop); // #272 void(float skel, float bonenum, vector org) skel_mul_bone
	PR1VM_RegisterBuiltin (vm, 273, (builtin_t)csqc_vmrest_nop); // #273 void(float skel, float startbone, float endbone, vector org) skel_mul_bone
	PR1VM_RegisterBuiltin (vm, 274, (builtin_t)csqc_vmrest_nop); // #274 void(float skeldst, float skelsrc, float startbone, float entbone) skel_copybones
	PR1VM_RegisterBuiltin (vm, 275, (builtin_t)csqc_vmrest_nop); // #275 void(float skel) skel_delete
	PR1VM_RegisterBuiltin (vm, 283, (builtin_t)csqc_vmrest_nop); // #283 void(entity ent, float bonenum, vector org, optional vector angorfwd, optional vector right, optional vector up) skel_set_bone_world
	PR1VM_RegisterBuiltin (vm, 288, (builtin_t)csqc_vmrest_nop); // #288 void(hashtable table) hash_destroytab
	PR1VM_RegisterBuiltin (vm, 289, (builtin_t)csqc_vmrest_nop); // #289 void(hashtable table, string name, __variant value, optional float typeandflags) hash_add
	PR1VM_RegisterBuiltin (vm, 293, (builtin_t)csqc_vmrest_nop); // #293 void() hash_getcb
	PR1VM_RegisterBuiltin (vm, 302, (builtin_t)csqc_addentity); // #302 void(entity ent) addentity (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 306, (builtin_t)csqc_vmrest_nop); // #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 307, (builtin_t)csqc_vmrest_nop); // #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 308, (builtin_t)csqc_vmrest_nop); // #308 void() R_EndPolygon (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 333, (builtin_t)csqc_setmodelindex); // #333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 385, (builtin_t)csqc_vmrest_nop); // #385 void(__variant *ptr) memfree - no-op in classic v6: typed pointer (no v6 pointer model)
	PR1VM_RegisterBuiltin (vm, 386, (builtin_t)csqc_vmrest_nop); // #386 void(__variant *dst, __variant *src, int size) memcpy - no-op in classic v6: typed pointer (no v6 pointer model)
	PR1VM_RegisterBuiltin (vm, 387, (builtin_t)csqc_vmrest_nop); // #387 void(__variant *dst, int val, int size) memfill8 - no-op in classic v6: typed pointer (no v6 pointer model)
	PR1VM_RegisterBuiltin (vm, 389, (builtin_t)csqc_vmrest_nop); // #389 void(__variant *dst, float ofs, __variant val) memsetval - no-op in classic v6: typed pointer + runtime value type
	PR1VM_RegisterBuiltin (vm, 400, (builtin_t)csqc_copyentity); // #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
	PR1VM_RegisterBuiltin (vm, 404, (builtin_t)csqc_vmrest_nop); // #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
	PR1VM_RegisterBuiltin (vm, 426, (builtin_t)csqc_vmrest_nop); // #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 432, (builtin_t)csqc_vectorvectors); // #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
	PR1VM_RegisterBuiltin (vm, 433, (builtin_t)csqc_vmrest_nop); // #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
	PR1VM_RegisterBuiltin (vm, 443, (builtin_t)csqc_vmrest_nop); // #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
	PR1VM_RegisterBuiltin (vm, 445, (builtin_t)csqc_vmrest_nop); // #445 void	search_end(float handle) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 457, (builtin_t)csqc_vmrest_nop); // #457 void(vector org, vector vel, float howmany) te_flamejet
	PR1VM_RegisterBuiltin (vm, 488, (builtin_t)csqc_vmrest_nop); // #488 void(string name)
	PR1VM_RegisterBuiltin (vm, 489, (builtin_t)csqc_vmrest_nop); // #489 void(string name, string URI)
	PR1VM_RegisterBuiltin (vm, 491, (builtin_t)csqc_vmrest_nop); // #491 void(string name, float x, float y)
	PR1VM_RegisterBuiltin (vm, 492, (builtin_t)csqc_vmrest_nop); // #492 void(string name, float w, float h)
	PR1VM_RegisterBuiltin (vm, 502, (builtin_t)csqc_vmrest_nop); // #502 void(float effectindex, entity own, vector org_from, vector org_to, vector dir_from, vector dir_to, float countmultiplier, optional float flags) boxparticles
	PR1VM_RegisterBuiltin (vm, 517, (builtin_t)csqc_vmrest_nop); // #517 void(strbuf strbuf) buf_cvarlist
	PR1VM_RegisterBuiltin (vm, 529, (builtin_t)csqc_vmrest_nop); // #529 void(string s) loadfromdata
	PR1VM_RegisterBuiltin (vm, 530, (builtin_t)csqc_vmrest_nop); // #530 void(string s) loadfromfile
	PR1VM_RegisterBuiltin (vm, 540, (builtin_t)csqc_vmrest_nop); // #540 void(entity e, float physics_enabled) physics_enable
	PR1VM_RegisterBuiltin (vm, 541, (builtin_t)csqc_vmrest_nop); // #541 void(entity e, vector force, vector relative_ofs) physics_addforce
	PR1VM_RegisterBuiltin (vm, 542, (builtin_t)csqc_vmrest_nop); // #542 void(entity e, vector torque) physics_addtorque
	PR1VM_RegisterBuiltin (vm, 606, (builtin_t)csqc_vmrest_nop); // #606 void(filestream fh, entity e) writetofile
	PR1VM_RegisterBuiltin (vm, 613, (builtin_t)csqc_vmrest_nop); // #613 void(entity e, string s) parseentitydata
	PR1VM_RegisterBuiltin (vm, 615, (builtin_t)csqc_vmrest_nop); // #615 void() resethostcachemasks
	PR1VM_RegisterBuiltin (vm, 616, (builtin_t)csqc_vmrest_nop); // #616 void(float mask, float fld, string str, float op) sethostcachemaskstring
	PR1VM_RegisterBuiltin (vm, 617, (builtin_t)csqc_vmrest_nop); // #617 void(float mask, float fld, float num, float op) sethostcachemasknumber
	PR1VM_RegisterBuiltin (vm, 618, (builtin_t)csqc_vmrest_nop); // #618 void() resorthostcache
	PR1VM_RegisterBuiltin (vm, 619, (builtin_t)csqc_vmrest_nop); // #619 void(float fld, float descending) sethostcachesort
	PR1VM_RegisterBuiltin (vm, 620, (builtin_t)csqc_vmrest_nop); // #620 void() refreshhostcache
	PR1VM_RegisterBuiltin (vm, 623, (builtin_t)csqc_vmrest_nop); // #623 void(string key) addwantedhostcachekey
	PR1VM_RegisterBuiltin (vm, 650, (builtin_t)csqc_vmrest_nop); // #650 void() fcopy
	PR1VM_RegisterBuiltin (vm, 651, (builtin_t)csqc_vmrest_nop); // #651 void() frename
	PR1VM_RegisterBuiltin (vm, 652, (builtin_t)csqc_vmrest_nop); // #652 void() fremove
	PR1VM_RegisterBuiltin (vm, 654, (builtin_t)csqc_vmrest_nop); // #654 void() rmtree
	PR1VM_RegisterBuiltin (vm, 741, (builtin_t)csqc_vmrest_nop); // #741 void() controller_rumble
	PR1VM_RegisterBuiltin (vm, 742, (builtin_t)csqc_vmrest_nop); // #742 void() controller_rumbletriggers
	// FLOAT0 stubs - type-correct no-op.
	PR1VM_RegisterBuiltin (vm, 110, (builtin_t)csqc_light_nop_ret0); // #110 float(string strname, float accessmode) fopen (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 200, (builtin_t)csqc_getmodelindex); // #200 float(string modelname, optional float queryonly) getmodelindex
	PR1VM_RegisterBuiltin (vm, 201, (builtin_t)csqc_light_nop_ret0); // #201 __variant(float prnum, string funcname, ...) externcall - no-op in classic v6: variant exchange between VMs (no v6 types)
	PR1VM_RegisterBuiltin (vm, 202, (builtin_t)csqc_light_nop_ret0); // #202 float(string progsname) addprogs
	PR1VM_RegisterBuiltin (vm, 203, (builtin_t)csqc_light_nop_ret0); // #203 __variant(float prnum, string varname) externvalue - no-op in classic v6: variant read of another program (no v6 types)
	PR1VM_RegisterBuiltin (vm, 205, (builtin_t)csqc_light_nop_ret0); // #205 float() externrefcall
	PR1VM_RegisterBuiltin (vm, 206, (builtin_t)csqc_instr); // #206 string(string input, string token) instr
	PR1VM_RegisterBuiltin (vm, 237, (builtin_t)csqc_light_nop_ret0); // #237 float(float mdlindex, string skinname) skinforname
	PR1VM_RegisterBuiltin (vm, 238, (builtin_t)csqc_light_nop_ret0); // #238 float(string shadername, optional string defaultshader, ...) shaderforname
	PR1VM_RegisterBuiltin (vm, 242, (builtin_t)csqc_light_nop_ret0); // #242 void(string dest, string content) sendpacket
	PR1VM_RegisterBuiltin (vm, 263, (builtin_t)csqc_light_nop_ret0); // #263 float(float modlindex) skel_create
	PR1VM_RegisterBuiltin (vm, 264, (builtin_t)csqc_light_nop_ret0); // #264 float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone, optional float addition) skel_build
	PR1VM_RegisterBuiltin (vm, 265, (builtin_t)csqc_light_nop_ret0); // #265 float(float skel) skel_get_numbones
	PR1VM_RegisterBuiltin (vm, 267, (builtin_t)csqc_light_nop_ret0); // #267 float(float skel, float bonenum) skel_get_boneparent
	PR1VM_RegisterBuiltin (vm, 268, (builtin_t)csqc_light_nop_ret0); // #268 float(float skel, string tagname) skel_get_boneidx
	PR1VM_RegisterBuiltin (vm, 276, (builtin_t)csqc_light_nop_ret0); // #276 float(float modidx, string framename) frameforname
	PR1VM_RegisterBuiltin (vm, 277, (builtin_t)csqc_light_nop_ret0); // #277 float(float modidx, float framenum) frameduration
	PR1VM_RegisterBuiltin (vm, 281, (builtin_t)csqc_light_nop_ret0); // #281 (FTE_QC_RAGDOLL)
	PR1VM_RegisterBuiltin (vm, 282, (builtin_t)csqc_light_nop_ret0); // #282 (FTE_QC_RAGDOLL) skel_mmap - no-op in classic v6: native pointer to skeleton data (no v6 pointer)
	PR1VM_RegisterBuiltin (vm, 286, (builtin_t)csqc_light_nop_ret0); // #286 float(float resourcetype, float tryload, string resourcename) resourcestatus
	PR1VM_RegisterBuiltin (vm, 287, (builtin_t)csqc_light_nop_ret0); // #287 hashtable(float tabsize, optional float defaulttype) hash_createtab
	PR1VM_RegisterBuiltin (vm, 290, (builtin_t)csqc_light_nop_ret0); // #290 __variant(hashtable table, string name, optional __variant deflt, optional float requiretype, optional float index) hash_get
	PR1VM_RegisterBuiltin (vm, 291, (builtin_t)csqc_light_nop_ret0); // #291 __variant(hashtable table, string name) hash_delete
	PR1VM_RegisterBuiltin (vm, 356, (builtin_t)csqc_light_nop_ret0); // #356 float(string s) findfont
	PR1VM_RegisterBuiltin (vm, 357, (builtin_t)csqc_light_nop_ret0); // #357 float(string fontname, string fontmaps, string sizes, float slot, optional float fix_scale, optional float fix_voffset) loadfont
	PR1VM_RegisterBuiltin (vm, 384, (builtin_t)csqc_light_nop_ret0); // #384 __variant*(int size) memalloc - no-op in classic v6: typed pointer (no v6 pointer model)
	PR1VM_RegisterBuiltin (vm, 388, (builtin_t)csqc_light_nop_ret0); // #388 __variant(__variant *dst, float ofs) memgetval - no-op in classic v6: typed pointer + runtime value type
	PR1VM_RegisterBuiltin (vm, 390, (builtin_t)csqc_light_nop_ret0); // #390 __variant*(__variant *base, float ofs) memptradd - no-op in classic v6: native pointer arithmetic (no v6 pointer)
	PR1VM_RegisterBuiltin (vm, 402, (builtin_t)csqc_findchain); // #402 entity(.string field, string match, .entity chainfield) findchain (DP_QC_FINDCHAIN)
	PR1VM_RegisterBuiltin (vm, 403, (builtin_t)csqc_findchainfloat); // #403 entity(.float fld, float match, .entity chainfield) findchainfloat (DP_QC_FINDCHAINFLOAT)
	PR1VM_RegisterBuiltin (vm, 444, (builtin_t)csqc_light_nop_ret0); // #444 float	search_begin(string pattern, float caseinsensitive, float quiet) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 446, (builtin_t)csqc_light_nop_ret0); // #446 float	search_getsize(float handle) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 449, (builtin_t)csqc_findflags); // #449 entity(entity start, .float fld, float match) findflags (DP_QC_FINDFLAGS)
	PR1VM_RegisterBuiltin (vm, 450, (builtin_t)csqc_findchainflags); // #450 entity(.float fld, float match, .entity chainfield) findchainflags (DP_QC_FINDCHAINFLAGS)
	PR1VM_RegisterBuiltin (vm, 451, (builtin_t)csqc_light_nop_ret0); // #451 float(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
	PR1VM_RegisterBuiltin (vm, 476, (builtin_t)csqc_strlennocol); // #476 float(string s) strlennocol
	PR1VM_RegisterBuiltin (vm, 487, (builtin_t)csqc_light_nop_ret0); // #487 float(string name)
	PR1VM_RegisterBuiltin (vm, 490, (builtin_t)csqc_light_nop_ret0); // #490 float(string name, float key, float eventtype)
	PR1VM_RegisterBuiltin (vm, 513, (builtin_t)csqc_light_nop_ret0); // #513 float(string uril, float id) uri_get
	PR1VM_RegisterBuiltin (vm, 535, (builtin_t)csqc_light_nop_ret0); // #535 float(string filename, strbuf bufhandle) buf_loadfile
	PR1VM_RegisterBuiltin (vm, 536, (builtin_t)csqc_light_nop_ret0); // #536 float(filestream filehandle, strbuf bufhandle, optional float startpos, optional float numstrings) buf_writefile
	PR1VM_RegisterBuiltin (vm, 537, (builtin_t)csqc_light_nop_ret0); // #537 float() bufstr_find
	PR1VM_RegisterBuiltin (vm, 611, (builtin_t)csqc_light_nop_ret0); // #611 float(float type) gethostcachevalue
	PR1VM_RegisterBuiltin (vm, 621, (builtin_t)csqc_light_nop_ret0); // #621 float(float fld, float hostnr) gethostcachenumber
	PR1VM_RegisterBuiltin (vm, 622, (builtin_t)csqc_light_nop_ret0); // #622 float(string key) gethostcacheindexforkey
	PR1VM_RegisterBuiltin (vm, 638, (builtin_t)csqc_cl_rotatemoves); // #638 void(vector anglechange, optional float seat) CL_RotateMoves (FTE)
	PR1VM_RegisterBuiltin (vm, 640, (builtin_t)csqc_light_nop_ret0); // #640 float() V_CalcRefdef
	PR1VM_RegisterBuiltin (vm, 653, (builtin_t)csqc_light_nop_ret0); // #653 float() fexists
	PR1VM_RegisterBuiltin (vm, 740, (builtin_t)csqc_light_nop_ret0); // #740 float() controller_query
	// STRING stubs - type-correct no-op.
	PR1VM_RegisterBuiltin (vm, 112, (builtin_t)csqc_nop_str); // #112 string(float fnum) fgets (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 224, (builtin_t)csqc_strconv); // #224 string(float ccase, float redalpha, float redchars, string str, ...) strconv (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 266, (builtin_t)csqc_nop_str); // #266 string(float skel, float bonenum) skel_get_bonename
	PR1VM_RegisterBuiltin (vm, 284, (builtin_t)csqc_nop_str); // #284 string(float modidx, float framenum) frametoname
	PR1VM_RegisterBuiltin (vm, 285, (builtin_t)csqc_nop_str); // #285 string(float modidx, float skin) skintoname
	PR1VM_RegisterBuiltin (vm, 292, (builtin_t)csqc_nop_str); // #292 string(hashtable table, float idx) hash_getkey
	PR1VM_RegisterBuiltin (vm, 334, (builtin_t)csqc_modelnameforindex); // #334 string(float mdlindex) modelnameforindex (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 374, (builtin_t)csqc_nop_str); // #374 string(float efnum, float body) particleeffectquery
	PR1VM_RegisterBuiltin (vm, 447, (builtin_t)csqc_nop_str); // #447 string	search_getfilename(float handle, float num) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 477, (builtin_t)csqc_strdecolorize); // #477 string(string s) strdecolorize
	PR1VM_RegisterBuiltin (vm, 503, (builtin_t)csqc_nop_str); // #503 string(string filename) whichpack
	PR1VM_RegisterBuiltin (vm, 510, (builtin_t)csqc_nop_str); // #510 string(string in) uri_escape
	PR1VM_RegisterBuiltin (vm, 511, (builtin_t)csqc_nop_str); // #511 string(string in) uri_unescape
	PR1VM_RegisterBuiltin (vm, 612, (builtin_t)csqc_nop_str); // #612 string(float type, float hostnr) gethostcachestring
	PR1VM_RegisterBuiltin (vm, 624, (builtin_t)csqc_nop_str); // #624 string() getextresponse
	PR1VM_RegisterBuiltin (vm, 625, (builtin_t)csqc_nop_str); // #625 string(string dnsname, optional float defport) netaddress_resolve
	PR1VM_RegisterBuiltin (vm, 626, (builtin_t)csqc_nop_str); // #626 string() getgamedirinfo
	PR1VM_RegisterBuiltin (vm, 639, (builtin_t)csqc_digest_hex); // #639 string(string digest, string data, ...) digest_hex
	// VECTOR stubs - type-correct no-op.
	PR1VM_RegisterBuiltin (vm, 244, (builtin_t)csqc_bsp_nop_vec); // #244 vector(entity ent, float tagnum) rotatevectorsbytag
	PR1VM_RegisterBuiltin (vm, 269, (builtin_t)csqc_bsp_nop_vec); // #269 vector(float skel, float bonenum) skel_get_bonerel
	PR1VM_RegisterBuiltin (vm, 270, (builtin_t)csqc_bsp_nop_vec); // #270 vector(float skel, float bonenum) skel_get_boneabs
	PR1VM_RegisterBuiltin (vm, 310, (builtin_t)csqc_unproject); // #310 vector (vector v) unproject (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 311, (builtin_t)csqc_project); // #311 vector (vector v) project (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 452, (builtin_t)csqc_bsp_nop_vec); // #452 vector(entity ent, float tagindex) gettaginfo (DP_MD3_TAGSINFO)
	PR1VM_RegisterBuiltin (vm, 493, (builtin_t)csqc_bsp_nop_vec); // #493 vector(string name)
}

#endif // !CLIENTONLY
