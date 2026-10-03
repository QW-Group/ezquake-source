/*
csqc_client.h -- PR1VM client wrapper (our csprogs.dat).

Call sites from client code (cl_parse.c / cl_screen.c / cl_main.c) and
accessors used by builtins in csqc_builtins.c. The header is deliberately
dependency-free (primitives only) so it can be included from both server
and client translation units.
*/

#ifndef CSQC_CLIENT_H
#define CSQC_CLIENT_H

#include <stddef.h>	// size_t (buf API)
struct usercmd_s;	// ezquake usercmd_t (common.h -> protocol.h); kept dependency-free here
struct pr1vm_s;		// PR1 instance (pr1vm.h); opaque pointer only here

// Extended CSQC stats 32..255 live in a dedicated client store (stat index on the
// wire is always a byte, 0..255; FTE-parity MAX_CL_STATS=256). Standard 0..31 stay
// in cl.stats[].
#define MAX_EXTENDED_CL_STATS 256

// Access to client state/output (implemented in csqc_client.c):
float CSQC_Client_GetStat (int idx);				// 0..31 -> cl.stats, 32..255 -> ext stats (int)
void CSQC_Client_SetStat (int idx, int value);		// receive ext stats 32..255 (CL_SetStat)
// Stat wire 78/79 (float/string CSQC stats 32..255): received from
// svc_fte_updatestatfloat/string and exposed via #331 getstatf / #332 getstats.
// FTE keeps them in per-player statsf[]/statsstr[]; here they live in a single
// module store. GetStatInt returns the exact int value for #331 bit access:
// the float path would lose the low bits of large ints.
int CSQC_Client_GetStatInt (int idx);			// 0..31 -> cl.stats, 32..255 -> ext (int)
float CSQC_Client_GetStatFloat (int idx);			// 0..31 -> cl.stats, 32..255 -> statsf
const char *CSQC_Client_GetStatString (int idx);	// 32..255 -> statss, otherwise ""
void CSQC_Client_SetStatFloat (int idx, float value);		// svc_fte_updatestatfloat (79)
void CSQC_Client_SetStatString (int idx, const char *s);	// svc_fte_updatestatstring (78)
void CSQC_Client_GetScreenSize (int *w, int *h);	// vid.width/height (VF_SCREENVSIZE)
void CSQC_Client_DrawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale);
void CSQC_Client_RegisterCommand (const char *cmd);	// registercommand -> console binding
void CSQC_Client_Abort (const char *msg);			// fatal: disconnects the client (FTE CSQC_Abort)
// Bounded string reads for the client VM (untrusted csprogs): a positive offset
// beyond numstrings returns NULL. Installed as vm->get_string.
char *CSQC_Client_GetString (struct pr1vm_s *vm, int num);

// Entity pool (slot != server number). entnum helpers work on pool slots;
// network numbers are kept in a number->slot map (svc 76/92). Slot 0 = world.
int CSQC_Client_EntAlloc (struct pr1vm_s *vm);			// first free pool slot (own) / 0
int CSQC_Client_EntNum (struct pr1vm_s *vm, int raw);		// raw (N*edict_size) -> pool slot; OOB -> world(0)
void CSQC_Client_EntFree (struct pr1vm_s *vm, int slot);	// free own entity (network untouched)
int CSQC_Client_NetAllocSlot (struct pr1vm_s *vm);		// slot without s_own (network receive; vm zeroes the slot)
void CSQC_Client_NetFreeSlot (int slot, int number);		// free slot + numslot
int CSQC_Client_NumToSlot (int number);					// number->slot map / 0
int CSQC_Client_MapNumber (int number, int slot);		// write map entry (returns slot)
int CSQC_Client_EntityEntNum (struct pr1vm_s *vm, int slot);	// .entnum of an arena edict (ssqc number / 0)
// Pool/module field traversal and diagnostics.
qbool CSQC_Client_EntUsed (int slot);			// slot in use (network or spawn)
int CSQC_Client_EntSpawnBase (void);			// first usable slot (1)
int CSQC_Client_EntUsedCount (void);			// number of used pool slots
int CSQC_Client_FindField (struct pr1vm_s *vm, const char *name);	// field offset in float words / -1
// Hot-path offset cache: resolved once when the module loads; builtins read the
// array instead of scanning globaldefs/fielddefs by name every frame.
typedef enum
{
	CSQC_TRACEG_FRACTION = 0,
	CSQC_TRACEG_ALLSOLID,
	CSQC_TRACEG_STARTSOLID,
	CSQC_TRACEG_INOPEN,
	CSQC_TRACEG_INWATER,
	CSQC_TRACEG_PLANE_DIST,
	CSQC_TRACEG_ENDPOS,
	CSQC_TRACEG_PLANE_NORMAL,
	CSQC_TRACEG_ENT,
	CSQC_TRACEG_NETWORKENTITY,
	CSQC_TRACEG_ENDCONTENTS,
	CSQC_TRACEG_COUNT
} csqc_traceglobal_id_t;

typedef enum
{
	CSQC_FLD_PREDRAW = 0,
	CSQC_FLD_MODELINDEX,
	CSQC_FLD_MODEL,
	CSQC_FLD_COLORMAP,
	CSQC_FLD_ORIGIN,
	CSQC_FLD_ANGLES,
	CSQC_FLD_FRAME,
	CSQC_FLD_SKIN,
	CSQC_FLD_EFFECTS,
	CSQC_FLD_ALPHA,
	CSQC_FLD_SCALE,
	CSQC_FLD_RENDERFLAGS,
	CSQC_FLD_SIZE,
	CSQC_FLD_MINS,
	CSQC_FLD_MAXS,
	CSQC_FLD_MODELFLAGS,
	CSQC_FLD_CHAIN,
	CSQC_FLD_SOLID,
	CSQC_FLD_FLAGS,
	CSQC_FLD_OWNER,
	CSQC_FLD_DRAWMASK,
	CSQC_FLD_COUNT
} csqc_field_id_t;

int CSQC_Client_TraceGlobal (struct pr1vm_s *vm, int id);	// global offset (or -1)
int CSQC_Client_FieldOfs (struct pr1vm_s *vm, int id);		// field offset (or -1)
// FTE-style builtin environment: publishes player_localentnum every 2D frame
// before CSQC_UpdateView. Player entities are not fabricated (see csqc_client.c).
void CSQC_Client_UpdateLocalEntnum (void);

// #371 deltalisten: registry of per-model entity update callbacks (FTE
// PF_DeltaListen). The module callback runs as CSQC_Ent_Update: `self` and
// `.entnum` are set by the engine, PARM0 = isnew. name=="*" matches all models;
// func<=0 unregisters. The player listener receives the authoritative (no-lerp)
// player_state.
void CSQC_Client_DeltaListen (const char *model, int func, int flags);
// MASK_DELTA: a delta callback returning !=0 means the engine does not draw the
// entity (the module draws it via #301); getters for cl_ents.c.
qbool CSQC_Client_DeltaPlayerOwned (int pnum);
qbool CSQC_Client_DeltaEntityOwned (int number);

// View/listener/view_angles plus project/unproject.
// `#351 setlistener` sets the module's audio listener (used in cl_main.c S_Update).
void CSQC_Client_SetListener (const float *origin, const float *forward, const float *right, const float *up);
qbool CSQC_Client_ListenerActive (void);
void CSQC_Client_GetListener (float *origin, float *forward, float *right, float *up);
// `#303 setproperty` (VF_* subset): applied immediately (same-frame) and re-applied
// in V_CalcRefdef (cl_view.c). Returns whether the property was recognized (1/0).
qbool CSQC_Client_SetViewProperty (int prop, int argc, const float *args);
void CSQC_Client_ApplyViewProps (void);
void CSQC_Client_ResetViewProps (void);	// #300 clearscene: reset view properties
// Gates the engine sbar/HUD (#303 VF_DRAWENGINESBAR) and crosshair
// (VF_DRAWCROSSHAIR) behind the CSQC takeover; clearscene defaults both to false.
qbool CSQC_Client_DrawEngineSbar (void);
qbool CSQC_Client_DrawCrosshairFlag (void);
// `#310 project` / `#311 unproject`.
qbool CSQC_Client_Project (const float *world, float *sx, float *sy, float *sz);
qbool CSQC_Client_Unproject (float sx, float sy, float sz, float *world);
// `#504 getentity`: interpolated engine entity state by server number
// (cl_entities/lerp_origin + player-state). out[3] is filled (float -> out[0],
// vector -> all three); fields with no ezquake source fall back to the FTE default.
void CSQC_Client_GetEntity (int entnum, int fldnum, float out[3]);

// CSQC model registry (name -> model_t*, 1-based index for #200/#333 and the
// .modelindex field). IndexKnown only looks up (query-only); Index looks up or loads.
int CSQC_Client_ModelIndexKnown (const char *name);	// 0 if not registered
int CSQC_Client_ModelIndex (const char *name);		// 0 if it did not load
struct model_s *CSQC_Client_ModelForIndex (int idx);	// NULL if absent
const char *CSQC_Client_ModelNameForIndex (int idx);	// #334: reverse lookup (NULL if absent)
void CSQC_Client_ModelReset (void);
// Reload CSQC model(s) whose name matches a downloaded file (downloadname =
// cls.downloadname "<gamedir>/<file>"). Called from CL_FinishDownload.
void CSQC_Client_ModelDownloadFinished (const char *downloadname);

// CSQC-VM client lifecycle call sites:
int CSQC_Client_Active (void);			// module loaded and not in error
void CSQC_Client_ConnectCheck (void);	// after full serverinfo: load + CSQC_Init
void CSQC_Client_Disconnect (void);		// CSQC_Shutdown + unload + command removal
void CSQC_Client_Update (void);			// every 2D frame: WorldLoaded-once + UpdateView

// Renderscene takeover: when a CSQC module is active it owns the 3D scene as in
// FTE -- CSQC_UpdateView runs in the 3D phase, #300/#301 build cl_visents, and
// #304 renderscene calls R_RenderView(). Otherwise the engine path is used.
qbool CSQC_Client_SceneActive (void);	// module active && has CSQC_UpdateView
void CSQC_Client_BeginScene (void);		// reset the "renderscene was called this frame" flag
void CSQC_Client_RenderScene (void);	// #304 renderscene -> R_RenderView()
qbool CSQC_Client_SceneRendered (void);	// was renderscene called this frame
// #301 mask&2: the module requested the engine viewmodel in the CSQC scene (FTE CL_LinkViewModel).
void CSQC_Client_LinkViewModel (void);
qbool CSQC_Client_SceneViewModel (void);	// was the viewmodel requested this frame
// #301/#302: call .predraw on an arena edict (self=slot). Return = OFS_RETURN
// (PREDRAW_AUTOADD=0 -> add, !=0 -> skip); *removed is set when the edict was
// removed or execution failed inside predraw. The self context is restored.
float CSQC_Client_CallPredraw (int slot, int fidx, qbool *removed);

// CSQC_Input_Frame: runs before sending each usercmd (CL_SendCmd, cl_input.c).
// The engine fills the input_* globals from cmd, runs the module, and writes the
// changes back (see csqc_client.c).
void CSQC_Client_InputFrame (struct usercmd_s *cmd);

// #345: history of sent usercmd frames. seq mirrors cls.netchan.outgoing_sequence
// (the client message number at record time); servercommandframe = cl.parsecount
// (incoming_acknowledged) -- the prediction window is (servercommandframe,
// clientcommandframe] in one numbering (QW echoes the ack on netchan/frame).
void CSQC_Client_RecordInput (struct usercmd_s *cmd);	// record from CL_SendCmd
int CSQC_Client_ApplyInput (unsigned int seq);			// fill input_* by seq; 0/1
// #638 CL_RotateMoves: rotate angles of unacknowledged usercmd frames (seq > servercommandframe).
int CSQC_Client_RotateMoves (float *anglechange, int seat);
// FTE VectorAngles (up->roll, meshpitch=false): shared helper for #51 and #638.
void CSQC_VectorAngles (const float *forward, const float *up, float *result);
void CSQC_Client_RunPlayerPhysics (int entnum);			// #347 runstandardplayerphysics
// #1 makevectors: writes the module's v_forward/v_right/v_up from vector angles.
void CSQC_Client_MakeVectors (float *ang);
// #432 vectorvectors: writes the module's normalized v_forward and orthogonal
// v_right/v_up from a direction (FTE PF_vectorvectors).
void CSQC_Client_VectorVectors (float *dir);

// #460-469 string-buffers (DP). handle = 1-based; strings are deep-copied.
int CSQC_Client_BufCreate (void);
void CSQC_Client_BufDel (int handle);
int CSQC_Client_BufGetSize (int handle);
int CSQC_Client_BufAdd (int handle, const char *s, int order);
int CSQC_Client_BufGet (int handle, int idx, char *out, size_t max);
int CSQC_Client_BufSet (int handle, int idx, const char *s);
int CSQC_Client_BufFree (int handle, int idx);
int CSQC_Client_BufCopy (int from, int to);
int CSQC_Client_BufSort (int handle, int prefixlen, int backward);
int CSQC_Client_BufImplode (int handle, const char *glue, char *out, size_t max);
void CSQC_Client_BufReset (void);

// 2D graphics (draw.h/r_draw*; coordinates are raw video pixels, like DrawText).
// Helpers for csqc_builtins.c.
void CSQC_Client_DrawFill (float x, float y, float w, float h, int r, int g, int b, float alpha);
qbool CSQC_Client_DrawPic (float x, float y, float w, float h, const char *name, int r, int g, int b, float alpha);	// returns: pic found (#322)
void CSQC_Client_DrawSubPic (float x, float y, float w, float h, const char *name, float srcx, float srcy, float srcw, float srch, int r, int g, int b, float alpha);
void CSQC_Client_DrawCharacter (float x, float y, int ch, int r, int g, int b, float alpha, float scale);
void CSQC_Client_DrawLine (float x1, float y1, float x2, float y2, float width, int r, int g, int b, float alpha);
void CSQC_Client_DrawRawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale);	// no &c parsing (#321)
qbool CSQC_Client_IsCachedPic (const char *name);		// #316 iscachedpic
qbool CSQC_Client_PicSize (const char *name, float *w, float *h);	// #318 drawgetimagesize
void CSQC_Client_SetClipArea (float x, float y, float w, float h);	// #324
void CSQC_Client_ResetClipArea (void);				// #325
float CSQC_Client_StringWidth (const char *text, qbool usecolours, float fontsize_x);
// The active VM's `drawfontscale` (vector): the x component scales text for the
// draw/measure handlers. No global -> 1.0; guard `x==0 && y==0` (both zero =
// "scale disabled") -> 1.0. The y component is not applied (ezquake fonts are
// uniform).
float CSQC_Client_DrawFontScaleX (struct pr1vm_s *vm);
qbool CSQC_Client_PrecachePic (const char *name);

// #343 setcursormode: module cursor state and drawing (ABI parsing lives in
// csqc_builtins.c). While the CSQC cursor is active, the mouse logic honors
// CSQC_Client_CSQCCursor() (vid_sdl2.c) and SCR_DrawCursor draws the module's
// cursor (image/hotspot/scale). Here: mouse release/grab, the custom cursor, and
// the pointer position (#344).
void CSQC_Client_SetCursorMode (qbool usecursor, const char *image,
	float hotspot_x, float hotspot_y, float scale);
qbool CSQC_Client_CSQCCursor (void);		// usecursor=1 && module loaded && in game
void CSQC_Client_DrawCursor (void);			// draw the module cursor (SCR_DrawCursor)
void CSQC_Client_GetCursorPos (float *x, float *y);	// pointer position (#344)
void CSQC_Client_ScaleCursorDelta (float *x, float *y);	// render-2D -> vid.conwidth (mouse delta)
void CSQC_Client_SetSensitivityScale (float scale);	// #346 setsensitivityscaler
float CSQC_Client_SensitivityScale (void);	// sensitivity multiplier (1 when inactive)

// Input events to the module (CSQC_InputEvent): keys/mouse/wheel.
qbool CSQC_Client_HasInputEvent (void);		// module defines CSQC_InputEvent
int CSQC_Client_InputEvent (int evtype, float a, float b, float c);	// returns handled

// notmenu for CSQC_UpdateView (#300) and translation between ezquake's internal
// keynum and QC/DP key codes. Applied to CSQC_InputEvent and the keynum builtins.
qbool CSQC_Client_NotMenu (void);
int CSQC_Client_KeynumToQC (int keynum);
int CSQC_Client_QCToKeynum (int code);

// Event types (matching csdefs.qc IE_*, FTE CSIE_*).
#ifndef IE_KEYDOWN
#define IE_KEYDOWN	0
#define IE_KEYUP	1
#define IE_MOUSEDELTA	2
#define IE_MOUSEABS	3
#define IE_ACCELEROMETER 4
#define IE_FOCUS		5
#define IE_JOYAXIS		6
#endif

// CSQC wire numbers (svc_fte_updatestatstring/float 78/79, svc_fte_cgamepacket 83,
// svc_fte_cgamepacket_sized 90, svc_fte_csqcentities_sized 92, clcfte_qcrequest 81)
// come from qwprot src/protocol.h under #ifdef FTE_PEXT_CSQC.

// Runtime gate for the CSQC parsers: FTE_PEXT_CSQC negotiated and cl_pext_csqc
// enabled (as in cl_parse.c case 83/90). Without it, 76/92 must not be treated
// as CSQC.
qbool CSQC_Client_ParseAllowed (void);
// read* gate for the module -- true only inside parse callbacks
// (CSQC_Ent_Update/CSQC_Parse_Event); otherwise read* is fatal.
qbool CSQC_Client_MayRead (void);
// Parse svc_fte_csqcentities (76); the sized variant (92) is handled separately.
void CSQC_Client_ParseEntities (qbool sized);
// Parse svc_fte_cgamepacket (83) / sized (90): the module reads the name + payload.
// sized=true: the caller (cl_parse.c case 90) drains the remainder by length;
// sized=false (case 83) has no length -- with no live module/callback it is a
// protocol error.
void CSQC_Client_ParseEvent (qbool sized);

// Network print callbacks. ParsePrint is true if the module handled it (the
// engine suppresses its own print); ParseCenterPrint is true if the module
// returned != 0 (suppress the engine centerprint).
qbool CSQC_Client_ParsePrint (const char *msg, int level);
qbool CSQC_Client_ParseCenterPrint (const char *msg);

// Network damage callback. true if the module returned != 0 (suppress the engine's
// color-shift / view-kick).
qbool CSQC_Client_ParseDamage (float save, float take, const vec3_t source);

// Network sound callback. true if the module returned != 0 (the engine does not
// play the sound). self = the arena entity by entnum, or world.
qbool CSQC_Client_EventSound (int entnum, int channel, const char *name, float vol,
							  float atten, const vec3_t pos, float pitchmod, float flags);

// Network forced-angles callback. true if the module returned != 0 (the engine does
// not apply its own angle).
qbool CSQC_Client_ParseSetAngles (const float *angles, float isdelta);

// Engine callback CSQC_RendererRestarted(string rendererdescription) -- invoked on
// renderer reinitialization (vid_restart/vid_reload) and on module load. The return
// value is ignored.
void CSQC_Client_RendererRestarted (const char *desc);

// Register the builtin table of the client instance (implemented in csqc_builtins.c).
void CSQCVM_RegisterBuiltins (struct pr1vm_s *vm);

// Client wrapper over the shared PR1VM_SetString (core, pr_edict.c): temp strings
// are deep-copied into the client instance ring (stable buffer) and registered via
// vm->strtbl. Strings from the module area are passed to core without a copy.
// Works with the PR1 VM (unlike PR2). address is a string_t* (int*).
void PR1VM_ClientSetString (struct pr1vm_s *vm, int *address, char *s);

// Register client debug commands for PR1VM (csqc_smoke, etc.; csqc_client.c) --
// called from CL_InitLocal (cl_main.c).
void CSQC_Client_RegisterCommands (void);

#endif /* CSQC_CLIENT_H */
