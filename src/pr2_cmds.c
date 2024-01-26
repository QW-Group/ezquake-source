/*
 *  QW262
 *  Copyright (C) 2004  [sd] angel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  
 */

#ifndef CLIENTONLY
#ifdef USE_PR2

#include "qwsvdef.h"
#include "vm.h"
#include "vm_local.h"

#define SETUSERINFO_STAR          (1<<0) // allow set star keys

#ifdef SERVERONLY
#define Cbuf_AddTextEx(x, y) Cbuf_AddText(y)
#define Cbuf_ExecuteEx(x) Cbuf_Execute()
#endif

const char *pr2_ent_data_ptr;
vm_t *sv_vm = NULL;
extern gameData_t gamedata;

static int PASSFLOAT(float f)
{
	floatint_t fi;
	fi.f = f;
	return fi.i;
}

#if 0 // Provided for completness.
static float GETFLOAT(int i)
{
	floatint_t fi;
	fi.i = i;
	return fi.f;
}
#endif

int NUM_FOR_GAME_EDICT(byte *e)
{
	int b;

	b = (byte *)e - (byte *)sv.game_edicts;
	b /= pr_edict_size;

	if (b < 0 || b >= sv.num_edicts)
		SV_Error("NUM_FOR_GAME_EDICT: bad pointer");

	return b;
}

intptr_t PR2_EntityStringLocation(string_t offset, int max_size);
void static PR2_SetEntityString_model(edict_t *ed, string_t *target, char *s) {
	if (!sv_vm) {
		PR1_SetString(target, s);
		return;
	}

	switch (sv_vm->type) {
	case VMI_NONE:
		PR1_SetString(target, s);
		return;

	case VMI_NATIVE:
		if (sv_vm->pr2_references) {
			char **location =
					(char **)PR2_EntityStringLocation(*target, sizeof(char *));
			if (location) {
				*location = s;
			}
		}
#ifndef idx64
		else if (target) {
			*target = (string_t)s;
		}
#endif
		return;
	case VMI_BYTECODE:
	case VMI_COMPILED: {
		int off = VM_ExplicitPtr2VM(sv_vm, (byte *)s);

		if (sv_vm->pr2_references) {
			string_t *location =
					(string_t *)PR2_EntityStringLocation(*target, sizeof(string_t));

			if (location) {
				*location = off;
			}
		} else {
			*target = off;
		}
	}
		return;
	}

	*target = 0;
}

/*
============
PR2_RunError
 
Aborts the currently executing function
============
*/
void PR2_RunError(char *error, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start(argptr, error);
	vsnprintf(string, sizeof(string), error, argptr);
	va_end(argptr);

	sv_error = true;

	Con_Printf("%s\n", string);

	SV_Error("Program error: %s", string);
}

void PR2_CheckEmptyString(char *s)
{
	if (!s || s[0] <= ' ')
		PR2_RunError("Bad string");
}

void PF2_precache_sound(char *s)
{
	int i;

	if (sv.state != ss_loading)
		PR2_RunError("PF_Precache_*: Precache can only be done in spawn "
		             "functions");
	PR2_CheckEmptyString(s);

	for (i = 0; i < MAX_SOUNDS; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}

	PR2_RunError ("PF_precache_sound: overflow");
}

void PF2_precache_model(char *s)
{
	int 	i;

	if (sv.state != ss_loading)
		PR2_RunError("PF_Precache_*: Precache can only be done in spawn "
		             "functions");

	PR2_CheckEmptyString(s);

	for (i = 0; i < MAX_MODELS; i++)
	{
		if (!sv.model_precache[i])
		{
			sv.model_precache[i] = s;
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}

	PR2_RunError ("PF_precache_model: overflow");
}

intptr_t PF2_precache_vwep_model(char *s)
{
	int 	i;

	if (sv.state != ss_loading)
		PR2_RunError("PF_Precache_*: Precache can only be done in spawn "
		             "functions");

	PR2_CheckEmptyString(s);

	// the strings are transferred via the stufftext mechanism, hence the stringency
	if (strchr(s, '"') || strchr(s, ';') || strchr(s, '\n'  ) || strchr(s, '\t') || strchr(s, ' '))
		PR2_RunError ("Bad string\n");

	for (i = 0; i < MAX_VWEP_MODELS; i++)
	{
		if (!sv.vw_model_name[i]) {
			sv.vw_model_name[i] = s;
			return i;
		}
	}
	PR2_RunError ("PF_precache_vwep_model: overflow");
	return 0;
}

/*
=================
PF2_setorigin
 
This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.
 
setorigin (entity, origin)
=================
*/

void PF2_setorigin(edict_t *e, float x, float y, float z)
{
	vec3_t origin;

	origin[0] = x;
	origin[1] = y;
	origin[2] = z;

	VectorCopy(origin, e->v->origin);
	SV_AntilagReset(e);
	SV_LinkEdict(e, false);
}

/*
=================
PF2_setsize
 
the size box is rotated by the current angle
 
setsize (entity, minvector, maxvector)
=================
*/
void PF2_setsize(edict_t *e, float x1, float y1, float z1, float x2, float y2, float z2)
{
	// vec3_t min, max;
	e->v->mins[0] = x1;
	e->v->mins[1] = y1;
	e->v->mins[2] = z1;

	e->v->maxs[0] = x2;
	e->v->maxs[1] = y2;
	e->v->maxs[2] = z2;

	VectorSubtract(e->v->maxs, e->v->mins, e->v->size);

	SV_LinkEdict(e, false);
}

/*
=================
PF2_setmodel
 
setmodel(entity, model)
Also sets size, mins, and maxs for inline bmodels
=================
*/
void PF2_setmodel(edict_t *e, char *m)
{
	char		**check;
	int		i;
	cmodel_t	*mod;

	if (!m)
		m = "";
	// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
		if (!strcmp(*check, m))
			break;

	if (!*check)
		PR2_RunError("no precache: %s\n", m);

	PR2_SetEntityString_model(e, &e->v->model, m);
	e->v->modelindex = i;

	// if it is an inline model, get the size information for it
	if (m[0] == '*')
	{
		mod = CM_InlineModel (m);
		VectorCopy(mod->mins, e->v->mins);
		VectorCopy(mod->maxs, e->v->maxs);
		VectorSubtract(mod->maxs, mod->mins, e->v->size);
		SV_LinkEdict(e, false);
	}

}

/*
=================
PF2_sprint
 
single print to a specific client
 
sprint(clientent, value)
=================
*/

// trap_SPrint() flags
#define SPRINT_IGNOREINDEMO   (   1<<0) // do not put such message in mvd demo

void PF2_sprint(int entnum, int level, char *s, int flags)
{
	client_t *client, *cl;
	int i;

	if (gamedata.APIversion < 15)
		flags = 0;
	if (entnum < 1 || entnum > MAX_CLIENTS)
	{
		Con_Printf("tried to sprint to a non-client %d \n", entnum);
		return;
	}

	client = &svs.clients[entnum - 1];

	// do not print to client in such state
	if (client->state < cs_connected)
		return;

	if (flags & SPRINT_IGNOREINDEMO)
		SV_ClientPrintf2 (client, level, "%s", s); // this does't go to mvd demo
	else
		SV_ClientPrintf  (client, level, "%s", s); // this will be in mvd demo too

	//bliP: spectator print ->
	if ((int)sv_specprint.value & SPECPRINT_SPRINT)
	{
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		{
			if (!cl->state || !cl->spectator)
				continue;

			if ((cl->spec_track == entnum) && (cl->spec_print & SPECPRINT_SPRINT))
			{
				if (flags & SPRINT_IGNOREINDEMO)
					SV_ClientPrintf2 (cl, level, "%s", s); // this does't go to mvd demo
				else
					SV_ClientPrintf  (cl, level, "%s", s); // this will be in mvd demo too
			}
		}
	}
	//<-
}

/*
=================
PF2_centerprint

single print to a specific client

centerprint(clientent, value)
=================
*/
void PF2_centerprint(int entnum, char *s)
{
	client_t *cl, *spec;
	int i;

	if (entnum < 1 || entnum > MAX_CLIENTS)
	{
		Con_Printf("tried to centerprint to a non-client %d \n", entnum);
		return;
	}

	cl = &svs.clients[entnum - 1];

	ClientReliableWrite_Begin(cl, svc_centerprint, 2 + strlen(s));
	ClientReliableWrite_String(cl, s);

	if (sv.mvdrecording)
	{
		if (MVDWrite_Begin(dem_single, entnum - 1, 2 + strlen(s)))
		{
			MVD_MSG_WriteByte(svc_centerprint);
			MVD_MSG_WriteString(s);
		}
	}

	//bliP: spectator print ->
	if ((int)sv_specprint.value & SPECPRINT_CENTERPRINT)
	{
		for (i = 0, spec = svs.clients; i < MAX_CLIENTS; i++, spec++)
		{
			if (!cl->state || !spec->spectator)
				continue;

			if ((spec->spec_track == entnum) && (cl->spec_print & SPECPRINT_CENTERPRINT))
			{
				ClientReliableWrite_Begin (spec, svc_centerprint, 2 + strlen(s));
				ClientReliableWrite_String (spec, s);
			}
		}
	}
	//<-

}

/*
=================
PF2_ambientsound
 
=================
*/
void PF2_ambientsound(float x, float y, float z, char *samp, float vol,	float attenuation)
{
	char	**check;
	int		i, soundnum;
	vec3_t 	pos;

	pos[0] = x;
	pos[1] = y;
	pos[2] = z;

	if( !samp )
		samp = "";

	// check to see if samp was properly precached
	for (soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++)
		if (!strcmp(*check, samp))
			break;

	if (!*check)
	{
		Con_Printf("no precache: %s\n", samp);
		return;
	}

	// add an svc_spawnambient command to the level signon packet
	MSG_WriteByte(&sv.signon, svc_spawnstaticsound);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte(&sv.signon, soundnum);

	MSG_WriteByte(&sv.signon, vol * 255);
	MSG_WriteByte(&sv.signon, attenuation * 64);

}

/*
=================
PF2_traceline
 
Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.
 
traceline (vector1, vector2, tryents)
=================
*/
void PF2_traceline(float v1_x, float v1_y, float v1_z,
					float v2_x, float v2_y, float v2_z,
					int nomonsters, int entnum)
{
	trace_t	trace;
	edict_t	*ent;
	vec3_t v1, v2;

	ent = EDICT_NUM(entnum);
	v1[0] = v1_x;
	v1[1] = v1_y;
	v1[2] = v1_z;

	v2[0] = v2_x;
	v2[1] = v2_y;
	v2[2] = v2_z;

	if (sv_antilag.value == 2)
	{
		if (!(entnum >= 1 && entnum <= MAX_CLIENTS && svs.clients[entnum - 1].isBot)) {
			nomonsters |= MOVE_LAGGED;
		}
	}

	trace = SV_Trace(v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist = trace.plane.dist;

	if (trace.e.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.e.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

/*
=================
PF2_TraceCapsule
 
Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.
=================
*/
void PF2_TraceCapsule(float v1_x, float v1_y, float v1_z,
					float v2_x,	float v2_y, float v2_z,
					int nomonsters, edict_t *ent,
					float min_x, float min_y, float min_z,
					float max_x, float max_y, float max_z)
{
	trace_t	trace;
	vec3_t v1, v2, v3, v4;

	v1[0] = v1_x;
	v1[1] = v1_y;
	v1[2] = v1_z;

	v2[0] = v2_x;
	v2[1] = v2_y;
	v2[2] = v2_z;

	v3[0] = min_x;
	v3[1] = min_y;
	v3[2] = min_z;

	v4[0] = max_x;
	v4[1] = max_y;
	v4[2] = max_z;

	trace = SV_Trace(v1, v3, v4, v2, nomonsters, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist = trace.plane.dist;

	if (trace.e.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.e.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

/*
=================
PF2_checkclient
 
Returns a client (or object that has a client enemy) that would be a
valid target.
 
If there are more than one valid options, they are cycled each frame
 
If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.
 
name checkclient ()
=================
*/

static byte	*checkpvs;

int PF2_newcheckclient(int check)
{
	int		i;
	edict_t	*ent;
	vec3_t	org;

	// cycle to the next one
	if (check < 1)
		check = 1;
	if (check > MAX_CLIENTS)
		check = MAX_CLIENTS;

	if (check == MAX_CLIENTS)
		i = 1;
	else
		i = check + 1;

	for ( ; ; i++)
	{
		if (i == MAX_CLIENTS + 1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->e.free)
			continue;
		if (ent->v->health <= 0)
			continue;
		if ((int) ent->v->flags & FL_NOTARGET)
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

	// get the PVS for the entity
	VectorAdd (ent->v->origin, ent->v->view_ofs, org);
	checkpvs = CM_LeafPVS (CM_PointInLeaf(org));

	return i;
}

#define	MAX_CHECK	16

intptr_t PF2_checkclient(void)
{
	edict_t	*ent, *self;
	int		l;
	vec3_t	view;

	// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF2_newcheckclient(sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	// return check if it might be visible
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->e.free || ent->v->health <= 0)
	{
		// RETURN_EDICT(sv.edicts);
		return NUM_FOR_EDICT(sv.edicts);
	}

	// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd(self->v->origin, self->v->view_ofs, view);
	l = CM_Leafnum(CM_PointInLeaf(view)) - 1;
	if ((l < 0) || !(checkpvs[l >> 3] & (1 << (l & 7))))
	{
		return NUM_FOR_EDICT(sv.edicts);
	}

	return NUM_FOR_EDICT(ent);
}

//============================================================================
// modified by Tonik
/*
=================
PF2_stuffcmd

Sends text over to the client's execution buffer

stuffcmd (clientent, value)
=================
*/

// trap_stuffcmd() flags
#define STUFFCMD_IGNOREINDEMO   (   1<<0) // do not put in mvd demo
#define STUFFCMD_DEMOONLY       (   1<<1) // put in mvd demo only

void PF2_stuffcmd(int entnum, char *str, int flags)
{
	char *buf = NULL;
	client_t *cl, *spec;
	int j;

	if (gamedata.APIversion < 15)
		flags = 0;
	if( !str )
		PR2_RunError("PF2_stuffcmd: NULL pointer");

	// put in mvd demo only
	if (flags & STUFFCMD_DEMOONLY)
	{
		if (strchr( str, '\n' )) // we have \n trail
		{
			if (sv.mvdrecording)
			{
				if (MVDWrite_Begin(dem_all, 0, 2 + strlen(str)))
				{
					MVD_MSG_WriteByte(svc_stufftext);
					MVD_MSG_WriteString(str);
				}
			}
		}

		return; // do not send to client in any case
	}

	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR2_RunError("Parm 0 not a client");


	cl = &svs.clients[entnum - 1];
	if (!strncmp(str, "disconnect\n", MAX_STUFFTEXT))
	{
		// so long and thanks for all the fish
		cl->drop = true;
		return;
	}
	
	buf = cl->stufftext_buf;
	if (strlen(buf) + strlen(str) >= MAX_STUFFTEXT)
		PR2_RunError("stufftext buffer overflow");
	strlcat (buf, str, MAX_STUFFTEXT);

	if( strchr( buf, '\n' ) )
	{
		ClientReliableWrite_Begin(cl, svc_stufftext, 2 + strlen(buf));
		ClientReliableWrite_String(cl, buf);

		if (!(flags & STUFFCMD_IGNOREINDEMO)) // STUFFCMD_IGNOREINDEMO flag is NOT set
		{
			if (sv.mvdrecording)
			{
				if (MVDWrite_Begin(dem_single, cl - svs.clients, 2 + strlen(buf)))
				{
					MVD_MSG_WriteByte(svc_stufftext);
					MVD_MSG_WriteString(buf);
				}
			}
		}

		//bliP: spectator print ->
		if ((int)sv_specprint.value & SPECPRINT_STUFFCMD)
		{
			for (j = 0, spec = svs.clients; j < MAX_CLIENTS; j++, spec++)
			{
				if (!cl->state || !spec->spectator)
					continue;

				if ((spec->spec_track == entnum) && (cl->spec_print & SPECPRINT_STUFFCMD))
				{
					ClientReliableWrite_Begin (spec, svc_stufftext, 2+strlen(buf));
					ClientReliableWrite_String (spec, buf);
				}
			}
		}
		buf[0] = 0;
	}

}

/*
=================
PF2_executecmd
=================
*/
void PF2_executecmd(void)
{
	int old_other, old_self; // mod_consolecmd will be executed, so we need to store this

	old_self = pr_global_struct->self;
	old_other = pr_global_struct->other;

	Cbuf_ExecuteEx(&cbuf_server);

	pr_global_struct->self = old_self;
	pr_global_struct->other = old_other;
}

/*
=================
PF2_readcmd
 
void readmcmd (string str,string buff, int sizeofbuff)
=================
*/

void PF2_readcmd(char *str, char *buf, int sizebuff)
{
	extern char outputbuf[];
	extern 	redirect_t sv_redirected;
	redirect_t old;

	Cbuf_ExecuteEx(&cbuf_server);
	Cbuf_AddTextEx(&cbuf_server, str);

	old = sv_redirected;

	if (old != RD_NONE)
		SV_EndRedirect();

	SV_BeginRedirect(RD_MOD);
	Cbuf_ExecuteEx(&cbuf_server);

	strlcpy(buf, outputbuf, sizebuff);

	SV_EndRedirect();

	if (old != RD_NONE)
		SV_BeginRedirect(old);

}

/*
=================
PF2_redirectcmd
 
void redirectcmd (entity to, string str)
=================
*/

void PF2_redirectcmd(int entnum, char *str)
{

	extern redirect_t sv_redirected;

	if (sv_redirected) {
		Cbuf_AddTextEx(&cbuf_server, str);
		Cbuf_ExecuteEx(&cbuf_server);
		return;
	}

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		PR2_RunError("Parm 0 not a client");
	}

	SV_BeginRedirect((redirect_t)(RD_MOD + entnum));
	Cbuf_AddTextEx(&cbuf_server, str);
	Cbuf_ExecuteEx(&cbuf_server);
	SV_EndRedirect();
}

/*
=================
PF2_FindRadius

Returns a chain of entities that have origins within a spherical area
gedict_t *findradius( gedict_t * start, vec3_t org, float rad );
=================
*/

intptr_t PF2_FindRadius(int e, float *org, float rad)
{
	int j;

	edict_t *ed;
	vec3_t	eorg;

	for ( e++; e < sv.num_edicts; e++ )
	{
		ed = EDICT_NUM( e );

		if (ed->e.free)
			continue;
		if (ed->v->solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ed->v->origin[j] + (ed->v->mins[j] + ed->v->maxs[j])*0.5);
		if (VectorLength(eorg) > rad)
			continue;
		return VM_Ptr2VM((byte *)ed->v);
	}
	return 0;
}

/*
===============
PF2_walkmove
 
float(float yaw, float dist) walkmove
===============
*/
int PF2_walkmove(edict_t *ent, float yaw, float dist)
{
	vec3_t		move;
	int			oldself;
	int			ret;

	if (!((int) ent->v->flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
	{
		return 0;
	}

	yaw = yaw * M_PI * 2 / 360;

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	//	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	ret = SV_movestep(ent, move, true);

	// restore program state
	//	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
	return ret;
}

/*
===============
PF2_MoveToGoal
 
float(float dist) PF2_MoveToGoal
===============
*/

void PF2_MoveToGoal(float dist)
{
	edict_t		*ent, *goal;

	//	dfunction_t	*oldf;
	int			oldself;

	ent  = PROG_TO_EDICT(pr_global_struct->self);
	goal = PROG_TO_EDICT(ent->v->goalentity);

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		return;
	}

	// if the next step hits the enemy, return immediately
	if ( PROG_TO_EDICT(ent->v->enemy) != sv.edicts && SV_CloseEnough (ent, goal, dist) )
		return;

	// save program state, because SV_movestep may call other progs
	//	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	// bump around...
	if ( (rand()&3)==1 || !SV_StepDirection (ent, ent->v->ideal_yaw, dist))
	{
		SV_NewChaseDir (ent, goal, dist);
	}

	// restore program state
	//	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF2_droptofloor
 
void(entnum) droptofloor
===============
*/
int PF2_droptofloor(edict_t *ent)
{
	vec3_t		end;
	trace_t		trace;

	VectorCopy(ent->v->origin, end);
	end[2] -= 256;

	trace = SV_Trace(ent->v->origin, ent->v->mins, ent->v->maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
	{
		return 0;
	}
	else
	{
		VectorCopy(trace.endpos, ent->v->origin);
		SV_LinkEdict(ent, false);
		ent->v->flags = (int) ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(trace.e.ent);
		return 1;
	}
}

/*
===============
PF2_lightstyle
 
void(int style, string value) lightstyle
===============
*/
void PF2_lightstyle(int style, char *val)
{
	client_t	*client;
	int			j;

	// change the string in sv
	sv.lightstyles[style] = val;

	// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
		if (client->state == cs_spawned)
		{
			ClientReliableWrite_Begin(client, svc_lightstyle, strlen(val) + 3);
			ClientReliableWrite_Char(client, style);
			ClientReliableWrite_String(client, val);
		}

	if (sv.mvdrecording)
	{
		if (MVDWrite_Begin(dem_all, 0, strlen(val) + 3))
		{
			MVD_MSG_WriteByte(svc_lightstyle);
			MVD_MSG_WriteChar(style);
			MVD_MSG_WriteString(val);
		}
	}
}

/*
=============
PF2_pointcontents
=============
*/
int PF2_pointcontents(float x, float y, float z)
{
	vec3_t origin;

	origin[0] = x;
	origin[1] = y;
	origin[2] = z;

	return SV_PointContents(origin);
}

/*
=============
PF2_nextent

entity nextent(entity)
=============
*/
intptr_t PF2_nextent(int i)
{
	edict_t	*ent;

	while (1)
	{
		i++;
		if (i >= sv.num_edicts)
		{
			return 0;
		}
		ent = EDICT_NUM(i);
		if (!ent->e.free)
		{
			return i;
		}
	}
}

/*
=============
PF2_nextclient

fast walk over spawned clients
 
entity nextclient(entity)
=============
*/
intptr_t PF2_nextclient(int i)
{
	edict_t	*ent;

	while (1)
	{
		i++;
		if (i < 1 || i > MAX_CLIENTS)
		{
			return 0;
		}
		ent = EDICT_NUM(i);
		if (!ent->e.free) // actually that always true for clients edicts
		{
			if (svs.clients[i - 1].state == cs_spawned) // client in game
			{
				return VM_Ptr2VM((byte *)ent->v);
			}
		}
	}
}

/*
=============
PF2_find

entity find(start,fieldoff,str)
=============
*/
intptr_t PF2_Find(int e, int fofs, char *str)
{
	char *t;
	edict_t	*ed;

	if(!str)
		PR2_RunError ("PF2_Find: bad search string");

	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->e.free)
			continue;

		if (!(intptr_t *)((byte *)ed->v + fofs))
			continue;

		t = VM_ArgPtr(*(intptr_t *)((char *)ed->v + fofs));

		if (!t)
			continue;

		if (!strcmp(t,str))
		{
			return VM_Ptr2VM((byte *)ed->v);
		}
	}
	return 0;
}

/*
=============
PF2_aim ??????
 
Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
/*
==============
PF2_changeyaw ???
 
This was a major timewaster in progs, so it was converted to C
==============
*/

/*
===============================================================================
 
MESSAGE WRITING
 
===============================================================================
*/


#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string
#define	MSG_MULTICAST	4		// for multicast()


sizebuf_t *WriteDest2(int dest)
{
	//	int		entnum;
	//	int		dest;
	//	edict_t	*ent;

	//dest = G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		SV_Error("Shouldn't be at MSG_ONE");
#if 0
		ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > MAX_CLIENTS)
			PR2_RunError("WriteDest: not a client");
		return &svs.clients[entnum - 1].netchan.message;
#endif

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		if (sv.state != ss_loading)
			PR2_RunError("PF_Write_*: MSG_INIT can only be written in spawn "
			             "functions");
		return &sv.signon;

	case MSG_MULTICAST:
		return &sv.multicast;

	default:
		PR2_RunError ("WriteDest: bad destination");
		break;
	}

	return NULL;
}

static client_t *Write_GetClient(void)
{
	int		entnum;
	edict_t	*ent;

	ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
	entnum = NUM_FOR_EDICT(ent);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR2_RunError("WriteDest: not a client");
	return &svs.clients[entnum - 1];
}

void PF2_WriteByte(int to, int data)
{
	if (to == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Byte(cl, data);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 1))
			{
				MVD_MSG_WriteByte(data);
			}
		}
	}
	else
		MSG_WriteByte(WriteDest2(to), data);
}

void PF2_WriteChar(int to, int data)
{
	if (to == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Char(cl, data);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 1))
			{
				MVD_MSG_WriteByte(data);
			}
		}
	}
	else
		MSG_WriteChar(WriteDest2(to), data);
}

void PF2_WriteShort(int to, int data)
{
	if (to == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, data);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 2))
			{
				MVD_MSG_WriteShort(data);
			}
		}
	}
	else
		MSG_WriteShort(WriteDest2(to), data);
}

void PF2_WriteLong(int to, int data)
{
	if (to == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 4);
		ClientReliableWrite_Long(cl, data);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 4))
			{
				MVD_MSG_WriteLong(data);
			}
		}
	}
	else
		MSG_WriteLong(WriteDest2(to), data);
}

void PF2_WriteAngle(int to, float data)
{
	if (to == MSG_ONE)
	{
#ifdef FTE_PEXT_FLOATCOORDS
		int size = msg_anglesize;
#else
		int size = 1;
#endif
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, size);
		ClientReliableWrite_Angle(cl, data);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, size))
			{
				MVD_MSG_WriteAngle(data);
			}
		}
	}
	else
		MSG_WriteAngle(WriteDest2(to), data);
}

void PF2_WriteCoord(int to, float data)
{
	if (to == MSG_ONE)
	{
#ifdef FTE_PEXT_FLOATCOORDS
		int size = msg_coordsize;
#else
		int size = 2;
#endif
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, size);
		ClientReliableWrite_Coord(cl, data);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, size))
			{
				MVD_MSG_WriteCoord(data);
			}
		}
	}
	else
		MSG_WriteCoord(WriteDest2(to), data);
}

void PF2_WriteString(int to, char *data)
{
	if (to == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1 + strlen(data));
		ClientReliableWrite_String(cl, data);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 1 + strlen(data)))
			{
				MVD_MSG_WriteString(data);
			}
		}
	}
	else
		MSG_WriteString(WriteDest2(to), data);
}

void PF2_WriteEntity(int to, int data)
{
	if (to == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl,data );//G_EDICTNUM(OFS_PARM1)
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 2))
			{
				MVD_MSG_WriteShort(data);
			}
		}
	}
	else
		MSG_WriteShort(WriteDest2(to), data);
}

//=============================================================================

int SV_ModelIndex(char *name);

/*
==================
PF2_makestatic 
 
==================
*/
void PF2_makestatic(edict_t *ent)
{
	entity_state_t *s;

	if (sv.static_entity_count >= sizeof(sv.static_entities) / sizeof(sv.static_entities[0])) {
		ED_Free (ent);
		return;
	}

	s = &sv.static_entities[sv.static_entity_count];
	memset(s, 0, sizeof(sv.static_entities[0]));
	s->number = sv.static_entity_count + 1;
	s->modelindex = SV_ModelIndex(PR_GetEntityString(ent->v->model));
	if (!s->modelindex) {
		ED_Free (ent);
		return;
	}
	s->frame = ent->v->frame;
	s->colormap = ent->v->colormap;
	s->skinnum = ent->v->skin;
	VectorCopy(ent->v->origin, s->origin);
	VectorCopy(ent->v->angles, s->angles);
	++sv.static_entity_count;

	// throw the entity away now
	ED_Free(ent);
}

//=============================================================================

/*
==============
PF2_setspawnparms
==============
*/
void PF2_setspawnparms(int entnum)
{
	int			i;
	client_t	*client;

	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR2_RunError("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (entnum - 1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

/*
==============
PF2_changelevel
==============
*/
void PF2_changelevel(char *s, char *entfile)
{
	static int last_spawncount;
	char expanded[MAX_QPATH];

	if (gamedata.APIversion < 15)
		entfile = "";
	// check to make sure the level exists.
	// this is work around for bellow check about two changelevels,
	// which lock server in one map if we trying switch to map which does't exist
	snprintf(expanded, MAX_QPATH, "maps/%s.bsp", s);
	if (!FS_FLocateFile(expanded, FSLFRT_IFFOUND, NULL))
	{
		Sys_Printf ("Can't find %s\n", expanded);
		return;
	}

	// make sure we don't issue two changelevels
	// this check is evil and cause lock on one map, if /map command fail for some reason
	if (svs.spawncount == last_spawncount)
		return;
	last_spawncount = svs.spawncount;

	if (entfile && *entfile) {
		Cbuf_AddTextEx(&cbuf_server, va("map %s %s\n", s, entfile));
	}
	else {
		Cbuf_AddTextEx(&cbuf_server, va("map %s\n", s));
	}
}

/*
==============
PF2_logfrag
 
logfrag (killer, killee)
==============
*/
void PF2_logfrag(int e1, int e2)
{
	char *s;
	// -> scream
	time_t		t;
	struct tm	*tblock;

	if (e1 < 1 || e1 > MAX_CLIENTS || e2 < 1 || e2 > MAX_CLIENTS)
		return;

	// -> scream
	t = time (NULL);
	tblock = localtime (&t);

	//bliP: date check ->
	if (!tblock)
		s = va("%s\n", "#bad date#");
	else
		if ((int)frag_log_type.value) // need for old-style frag log file
			s = va("\\frag\\%s\\%s\\%s\\%s\\%d-%d-%d %d:%d:%d\\\n",
			       svs.clients[e1-1].name, svs.clients[e2-1].name,
			       svs.clients[e1-1].team, svs.clients[e2-1].team,
			       tblock->tm_year + 1900, tblock->tm_mon + 1, tblock->tm_mday,
			       tblock->tm_hour, tblock->tm_min, tblock->tm_sec);
		else
			s = va("\\%s\\%s\\\n",svs.clients[e1-1].name, svs.clients[e2-1].name);
	// <-

	SZ_Print(&svs.log[svs.logsequence & 1], s);
	SV_Write_Log(FRAG_LOG, 1, s);
}
/*
==============
PF2_getinfokey
 
string(entity e, string key) infokey
==============
*/
void PF2_infokey(int e1, char *key, char *valbuff, int sizebuff)
//(int e1, char *key, char *valbuff, int sizebuff)
{
	static char ov[256];
	char		*value;

	value = ov;

	if (e1 == 0)
	{
		if (key && key[0] == '\\') { // so we can check is server support such "hacks" via infokey(world, "\\realip") f.e.
			key++;

			value = "no";

			if (   !strcmp(key, "date_str")
				|| !strcmp(key, "ip") || !strncmp(key, "realip", 7) || !strncmp(key, "download", 9)
				|| !strcmp(key, "ping") || !strcmp(key, "*userid") || !strncmp(key, "login", 6)
				|| !strcmp(key, "*VIP") || !strcmp(key, "*state")
				|| !strcmp(key, "netname")
				|| !strcmp(key, "mapname") || !strcmp(key, "modelname")
				|| !strcmp(key, "version") || !strcmp(key, "servername")
			   )
				value = "yes";
		}
		else if (!strcmp(key, "date_str")) { // qqshka - qvm does't have any time builtin support, so add this
			date_t date;

			SV_TimeOfDay(&date, "%a %b %d, %H:%M:%S %Y");
			snprintf(ov, sizeof(ov), "%s", date.str);
		}
		else if (!strcmp(key, "mapname")) {
			value = sv.mapname;
		}
		else if (!strcmp(key, "modelname")) {
			value = sv.modelname;
		}
		else if (!strcmp(key, "version")) {
			value = VersionStringFull();
		}
		else if (!strcmp(key, "servername")) {
			value = SERVER_NAME;
		}
		else if ((value = Info_ValueForKey(svs.info, key)) == NULL || !*value)
			value = Info_Get(&_localinfo_, key);
	}
	else if (e1 > 0 && e1 <= MAX_CLIENTS)
	{
		client_t *cl = &svs.clients[e1-1];

		if (!strcmp(key, "ip"))
			strlcpy(ov, NET_BaseAdrToString(cl->netchan.remote_address), sizeof(ov));
		else if (!strncmp(key, "realip", 7))
			strlcpy(ov, NET_BaseAdrToString (cl->realip), sizeof(ov));
		else if (!strncmp(key, "download", 9))
			snprintf(ov, sizeof(ov), "%d", cl->file_percent ? cl->file_percent : -1); //bliP: file percent
		else if (!strcmp(key, "ping"))
			snprintf(ov, sizeof(ov), "%d", (int)SV_CalcPing(cl));
		else if (!strcmp(key, "*userid"))
			snprintf(ov, sizeof(ov), "%d", cl->userid);
		else if (!strncmp(key, "login", 6))
			value = cl->login;
		else if (!strcmp(key, "*VIP")) // qqshka: also located in userinfo, but this is more safe/secure way, imo
			snprintf(ov, sizeof(ov), "%d", cl->vip);
		else if (!strcmp(key, "netname"))
			value = cl->name;
		else if (!strcmp(key, "*state"))
		{
			switch (cl->state)
			{
				case cs_free:         value = "free"; break;
				case cs_zombie:       value = "zombie"; break;
				case cs_preconnected: value = "preconnected"; break;
				case cs_connected:    value = "connected"; break;
				case cs_spawned:      value = "spawned"; break;

				default: value = "unknown"; break;
			}
		}
		else
			value = Info_Get(&cl->_userinfo_ctx_, key);
	}
	else
		value = "";

	if ((int) strlen(value) > sizebuff)
		Con_DPrintf("PR2_infokey: buffer size too small\n");

	strlcpy(valbuff, value, sizebuff);
	//	RETURN_STRING(value);
}

/*
==============
PF2_multicast
 
void(vector where, float set) multicast
==============
*/
void PF2_multicast(float x, float y, float z, int to)
//(vec3_t o, int to)
{
	vec3_t o;

	o[0] = x;
	o[1] = y;
	o[2] = z;
	SV_Multicast(o, to);
}

/*
==============
PF2_disable_updates
 
void(entiny whom, float time) disable_updates
==============
*/
void PF2_disable_updates(int entnum, float time1)
//(int entnum, float time)
{
	client_t *client;

	//	entnum = G_EDICTNUM(OFS_PARM0);
	//	time1 = G_FLOAT(OFS_PARM1);

	if (entnum < 1 || entnum > MAX_CLIENTS)
	{
		Con_Printf("tried to disable_updates to a non-client\n");
		return;
	}

	client = &svs.clients[entnum - 1];

	client->disable_updates_stop = realtime + time1;
}

#define MAX_PR2_FILES 8

typedef struct
{
	char name[256];
	vfsfile_t  *handle;
	fsMode_t accessmode;
}
pr2_fopen_files_t;

pr2_fopen_files_t pr2_fopen_files[MAX_PR2_FILES];
int pr2_num_open_files = 0;

char* cmodes[]={"rb","r","wb","w","ab","a"};
// FIXME: read from paks
int PF2_FS_OpenFile(char *name, fileHandle_t *handle, fsMode_t fmode)
{
	int i, ret = -1;

	if(pr2_num_open_files >= MAX_PR2_FILES)
	{
		return -1;
	}

	*handle = 0;
	for (i = 0; i < MAX_PR2_FILES; i++)
		if (!pr2_fopen_files[i].handle)
			break;
	if (i == MAX_PR2_FILES)	//too many already open
	{
		return -1;
	}

	if (FS_UnsafeFilename(name)) {
		// someone tried to be clever.
		return -1;
	}
	strlcpy(pr2_fopen_files[i].name, name, sizeof(pr2_fopen_files[i].name));
	pr2_fopen_files[i].accessmode = fmode;
	switch(fmode)
	{
	case FS_READ_BIN:
	case FS_READ_TXT:
#ifndef SERVERONLY
		pr2_fopen_files[i].handle = FS_OpenVFS(name, cmodes[fmode], FS_ANY);
#else
		pr2_fopen_files[i].handle = FS_OpenVFS(name, cmodes[fmode], FS_GAME);
#endif

		if(!pr2_fopen_files[i].handle)
		{
			return -1;
		}

		Con_DPrintf( "PF2_FS_OpenFile %s\n", name );

		ret = VFS_GETLEN(pr2_fopen_files[i].handle);

		break;
	case FS_WRITE_BIN:
	case FS_WRITE_TXT:
	case FS_APPEND_BIN:
	case FS_APPEND_TXT:
		// well, perhaps we we should create path...
		//		FS_CreatePathRelative(name, FS_GAME_OS);
		pr2_fopen_files[i].handle = FS_OpenVFS(name, cmodes[fmode], FS_GAME_OS);
		if ( !pr2_fopen_files[i].handle )
		{
			return -1;
		}
		Con_DPrintf( "PF2_FS_OpenFile %s\n", name );
		ret = VFS_TELL(pr2_fopen_files[i].handle);

		break;
	default:
		return -1;
	}

	*handle = i+1;
	pr2_num_open_files++;
	return ret;
}

void PF2_FS_CloseFile(fileHandle_t fnum)
{
	fnum--;
	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return;	//out of range
	if(!pr2_num_open_files)
		return;
	if(!(pr2_fopen_files[fnum].handle))
		return;

	VFS_CLOSE(pr2_fopen_files[fnum].handle);

	pr2_fopen_files[fnum].handle = NULL;
	pr2_num_open_files--;
}

int seek_origin[]={SEEK_CUR,SEEK_END,SEEK_SET};

intptr_t PF2_FS_SeekFile(fileHandle_t fnum, intptr_t offset, fsOrigin_t type)
{
	fnum--;

	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return 0;//out of range

	if(!pr2_num_open_files)
		return 0;

	if(!(pr2_fopen_files[fnum].handle))
		return 0;
	if(type < 0 || type >= sizeof(seek_origin) / sizeof(seek_origin[0]))
		return 0;

	return VFS_SEEK(pr2_fopen_files[fnum].handle, offset, seek_origin[type]);
}

intptr_t PF2_FS_TellFile(fileHandle_t fnum)
{
	fnum--;

	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return 0;//out of range

	if(!pr2_num_open_files)
		return 0;

	if(!(pr2_fopen_files[fnum].handle))
		return 0;

	return VFS_TELL(pr2_fopen_files[fnum].handle);
}

intptr_t PF2_FS_WriteFile(char *dest, intptr_t quantity, fileHandle_t fnum)
{
	fnum--;
	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return 0;//out of range

	if(!pr2_num_open_files)
		return 0;

	if(!(pr2_fopen_files[fnum].handle))
		return 0;

	return VFS_WRITE(pr2_fopen_files[fnum].handle, dest, quantity);
}

intptr_t PF2_FS_ReadFile(char *dest, intptr_t quantity, fileHandle_t fnum)
{
	fnum--;
	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return 0;//out of range

	if(!pr2_num_open_files)
		return 0;

	if(!(pr2_fopen_files[fnum].handle))
		return 0;

	return VFS_READ(pr2_fopen_files[fnum].handle, dest, quantity, NULL);
}

void PR2_FS_Restart(void)
{
	int i;

	if(pr2_num_open_files)
	{
		for (i = 0; i < MAX_PR2_FILES; i++)
		{
			if(pr2_fopen_files[i].handle)
			{
				VFS_CLOSE(pr2_fopen_files[i].handle);
				pr2_num_open_files--;
				pr2_fopen_files[i].handle = NULL;
			}
		}
	}
	if(pr2_num_open_files)
		Sys_Error("PR2_fcloseall: pr2_num_open_files != 0");
	pr2_num_open_files = 0;
	memset(pr2_fopen_files,0,sizeof(pr2_fopen_files));
}

static int GetFileList_Compare (const void *p1, const void *p2)
{
	return strcmp (*((char**)p1), *((char**)p2));
}

#define FILELIST_GAMEDIR_ONLY	(1<<0) // if set then search in gamedir only
#define FILELIST_WITH_PATH		(1<<1) // include path to file
#define FILELIST_WITH_EXT		(1<<2) // include extension of file

intptr_t PF2_FS_GetFileList(char *path, char *ext,
			char *listbuff, intptr_t buffsize, intptr_t flags)
{
//	extern	searchpath_t *com_searchpaths; // evil, because this must be used in fs.c only...
	char	*gpath = NULL;

	char 	*list[MAX_DIRFILES];
	const 	int	list_cnt = sizeof(list) / sizeof(list[0]);

	dir_t	dir;

//	searchpath_t *search;
	char	netpath[MAX_OSPATH], *fullname;

	char	*dirptr;

	int numfiles = 0;
	int i, j;

	if (gamedata.APIversion < 15)
		flags = 0;

	memset(list, 0, sizeof(list));

	dirptr   = listbuff;
	*dirptr  = 0;

	if (strstr( path, ".." ) || strstr( path, "::" ))
		return 0; // do not allow relative paths

	// search through the path, one element at a time
	for (i = 0, gpath = NULL; i < list_cnt && ( gpath = FS_NextPath( gpath ) ); )
	{
		// if FILELIST_GAMEDIR_ONLY set then search in gamedir only
		if ((flags & FILELIST_GAMEDIR_ONLY) && strcmp(gpath, fs_gamedir))
			continue;

		snprintf (netpath, sizeof (netpath), "%s/%s", gpath, path);

		// reg exp search...
		dir = Sys_listdir(netpath, ext, SORT_NO);

		for (j = 0; i < list_cnt && dir.files[j].name[0]; j++, i++)
		{
			if (flags & FILELIST_WITH_PATH)
			{
				// with path
				snprintf (netpath, sizeof (netpath), "%s/%s", gpath, dir.files[j].name);
				fullname = netpath;

				// skip "./" prefix
				if (!strncmp(fullname, "./", sizeof("./") - 1))
					fullname += 2;
			}
			else
			{
				// just name
				fullname = dir.files[j].name;
			}

			// skip file extension
			if (!(flags & FILELIST_WITH_EXT)) {
#ifndef SERVERONLY
				COM_StripExtension(fullname, fullname, sizeof(fullname));
#else
				COM_StripExtension(fullname);
#endif
			}

			list[i] = Q_strdup(fullname); // a bit below we will free it
		}
	}

	// sort it, this will help exclude duplicates a bit below
	qsort (list, i, sizeof(list[0]), GetFileList_Compare);

	// copy
	for (i = 0; i < list_cnt && list[i]; i++)
	{
		size_t namelen = strlen(list[i]) + 1;

		if(dirptr + namelen > listbuff + buffsize)
			break;

		if (!*list[i])
			continue; // hrm, empty

		// simple way to exclude duplicates, since we sorted it above!
		if (i && !strcmp(list[i-1], list[i]))
			continue;

//		Con_Printf("%4d %s\n", i, list[i]);

		strlcpy(dirptr, list[i], namelen);
		dirptr += namelen;

		numfiles++;
	}

	// free allocated mem
	for (i = 0; i < list_cnt; i++)
		Q_free(list[i]);
	return numfiles;
}

/*
  int trap_Map_Extension( const char* ext_name, int mapto)
  return:
    0 	success maping
   -1	not found
   -2	cannot map
*/
intptr_t PF2_Map_Extension(char *name, int mapto)
{
	if (mapto < _G__LASTAPI)
	{

		return -2;
	}

	return -1;
}
/////////Bot Functions
extern cvar_t maxclients, maxspectators;
int PF2_Add_Bot(char *name, int bottomcolor, int topcolor, char *skin)
{
	client_t *cl, *newcl = NULL;
	int     edictnum;
	int     clients, i;
	extern char *shortinfotbl[];
	char   *s;
	edict_t *ent;
	eval_t *val;
	int old_self;
	char info[MAX_EXT_INFO_STRING];

	// count up the clients and spectators
	clients = 0;
	for ( i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++ )
	{
		if ( cl->state == cs_free )
			continue;
		if ( !cl->spectator )
			clients++;
	}

	// if at server limits, refuse connection
	if ((int)maxclients.value > MAX_CLIENTS )
		Cvar_SetValue( &maxclients, MAX_CLIENTS );
	if ((int)maxspectators.value > MAX_CLIENTS )
		Cvar_SetValue( &maxspectators, MAX_CLIENTS );
	if ((int)maxspectators.value + maxclients.value > MAX_CLIENTS )
		Cvar_SetValue( &maxspectators, MAX_CLIENTS - (int)maxclients.value );

	if ( clients >= ( int ) maxclients.value )
	{
		return 0;
	}
	for ( i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++ )
	{
		if ( cl->state == cs_free )
		{
			newcl = cl;
			break;
		}
	}
	if ( !newcl )
	{
		return 0;
	}

	memset(newcl, 0, sizeof(*newcl));
	edictnum = (newcl - svs.clients) + 1;
	ent = EDICT_NUM(edictnum);
	ED_ClearEdict(ent);

	memset(&newcl->_userinfo_ctx_, 0, sizeof(newcl->_userinfo_ctx_));
	memset(&newcl->_userinfoshort_ctx_, 0, sizeof(newcl->_userinfoshort_ctx_));

	snprintf( info, sizeof( info ),
	          "\\name\\%s\\topcolor\\%d\\bottomcolor\\%d\\emodel\\6967\\pmodel\\13845\\skin\\%s\\*bot\\1",
	          name, topcolor, bottomcolor, skin );

	newcl->_userinfo_ctx_.max      = MAX_CLIENT_INFOS;
	newcl->_userinfoshort_ctx_.max = MAX_CLIENT_INFOS;
	Info_Convert(&newcl->_userinfo_ctx_, info);

	newcl->state = cs_spawned;
	newcl->userid = SV_GenerateUserID();
	newcl->datagram.allowoverflow = true;
	newcl->datagram.data = newcl->datagram_buf;
	newcl->datagram.maxsize = sizeof( newcl->datagram_buf );
	newcl->spectator = 0;
	newcl->isBot = 1;
	SV_SetClientConnectionTime(newcl);
	strlcpy(newcl->name, name, sizeof(newcl->name));

	newcl->entgravity = 1.0;
	val = PR2_GetEdictFieldValue( ent, "gravity" ); // FIXME: do it similar to maxspeed
	if ( val )
		val->_float = 1.0;
	sv_client->maxspeed = sv_maxspeed.value;

	if (fofs_maxspeed)
		EdictFieldFloat(ent, fofs_maxspeed) = sv_maxspeed.value;

	newcl->edict = ent;
	ent->v->colormap = edictnum;
	val = PR2_GetEdictFieldValue(ent, "isBot"); // FIXME: do it similar to maxspeed
	if( val )
		val->_int = 1;

	// restore client name.
	PR_SetEntityString(ent, ent->v->netname, newcl->name);

	memset( newcl->stats, 0, sizeof( newcl->stats ) );
	SZ_InitEx (&newcl->netchan.message, newcl->netchan.message_buf, (int)sizeof(newcl->netchan.message_buf), true);
	SZ_Clear( &newcl->netchan.message );
	newcl->netchan.drop_count = 0;
	newcl->netchan.incoming_sequence = 1;

	// copy the most important userinfo into userinfoshort
	// {
	SV_ExtractFromUserinfo( newcl, true );

	for ( i = 0; shortinfotbl[i] != NULL; i++ )
	{
		s = Info_Get( &newcl->_userinfo_ctx_, shortinfotbl[i] );
		Info_SetStar( &newcl->_userinfoshort_ctx_, shortinfotbl[i], s );
	}

	// move star keys to infoshort
	Info_CopyStar( &newcl->_userinfo_ctx_, &newcl->_userinfoshort_ctx_ );

	// }

	newcl->disable_updates_stop = -1.0;	// Vladis

	SV_FullClientUpdate( newcl, &sv.reliable_datagram );

	old_self = pr_global_struct->self;
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(newcl->edict);

	PR2_GameClientConnect(0);
	PR2_GamePutClientInServer(0);

	pr_global_struct->self = old_self;
	return edictnum;
}

void RemoveBot(client_t *cl)
{

	if( !cl->isBot )
		return;

	pr_global_struct->self = EDICT_TO_PROG(cl->edict);
	if ( sv_vm )
		PR2_GameClientDisconnect(0);

	cl->old_frags = 0;
	cl->edict->v->frags = 0.0;
	cl->name[0] = 0;
	cl->state = cs_free;

	Info_RemoveAll(&cl->_userinfo_ctx_);
	Info_RemoveAll(&cl->_userinfoshort_ctx_);

	SV_FullClientUpdate( cl, &sv.reliable_datagram );
	cl->isBot = 0;
}

void PF2_Remove_Bot(int entnum)
{
	client_t *cl;
	int old_self;

	if ( entnum < 1 || entnum > MAX_CLIENTS )
	{
		Con_Printf( "tried to remove a non-botclient %d \n", entnum );
		return;
	}
	cl = &svs.clients[entnum - 1];
	if ( !cl->isBot )
	{
		Con_Printf( "tried to remove a non-botclient %d \n", entnum );
		return;
	}
	old_self = pr_global_struct->self; //save self

	pr_global_struct->self = entnum;
	RemoveBot(cl);
	pr_global_struct->self = old_self;

}

// FIXME: Why PR2_UserInfoChanged is not called here? Like for normal players.
//        Why we need this special handling in the first place?
void PF2_SetBotUserInfo(int entnum, char *key, char *value, int flags)
{
	client_t *cl;
	int     i;
	extern char *shortinfotbl[];

	if (gamedata.APIversion < 15)
		flags = 0;
	if (strstr(key, "&c") || strstr(key, "&r") || strstr(value, "&c") || strstr(value, "&r"))
		return;

	if ( entnum < 1 || entnum > MAX_CLIENTS )
	{
		Con_Printf( "tried to change userinfo a non-botclient %d \n", entnum );
		return;
	}
	cl = &svs.clients[entnum - 1];
	if ( !cl->isBot )
	{
		Con_Printf( "tried to change userinfo a non-botclient %d \n", entnum );
		return;
	}

	if ( flags & SETUSERINFO_STAR )
		Info_SetStar( &cl->_userinfo_ctx_, key, value );
	else
		Info_Set( &cl->_userinfo_ctx_, key, value );
	SV_ExtractFromUserinfo( cl, !strcmp( key, "name" ) );

	for ( i = 0; shortinfotbl[i] != NULL; i++ )
	{
		if ( key[0] == '_' || !strcmp( key, shortinfotbl[i] ) )
		{
			char *nuw = Info_Get( &cl->_userinfo_ctx_, key );

			Info_Set( &cl->_userinfoshort_ctx_, key, nuw );

			i = cl - svs.clients;
			MSG_WriteByte( &sv.reliable_datagram, svc_setinfo );
			MSG_WriteByte( &sv.reliable_datagram, i );
			MSG_WriteString( &sv.reliable_datagram, key );
			MSG_WriteString( &sv.reliable_datagram, nuw );
			break;
		}
	}
}

void PF2_SetBotCMD(int entnum, int msec, float a1, float a2, float a3,
				int forwardmove, int sidemove, int upmove, int buttons, int impulse)
{
	client_t *cl;

	if ( entnum < 1 || entnum > MAX_CLIENTS )
	{
		Con_Printf( "tried to set cmd a non-botclient %d \n", entnum );
		return;
	}
	cl = &svs.clients[entnum - 1];
	if ( !cl->isBot )
	{
		Con_Printf( "tried to set cmd a non-botclient %d \n", entnum );
		return;
	}
	cl->botcmd.msec = msec;
	cl->botcmd.angles[0] = a1;
	cl->botcmd.angles[1] = a2;
	cl->botcmd.angles[2] = a3;
	cl->botcmd.forwardmove = forwardmove;
	cl->botcmd.sidemove = sidemove;
	cl->botcmd.upmove = upmove;
	cl->botcmd.buttons = buttons;
	cl->botcmd.impulse = impulse;

	if (cl->edict->v->fixangle)
	{
		VectorCopy(cl->edict->v->angles, cl->botcmd.angles);
		cl->botcmd.angles[PITCH] *= -3;
		cl->edict->v->fixangle = 0;
	}
}

//=========================================
// some time support in QVM
//=========================================

/*
==============
PF2_QVMstrftime
==============
*/
int PF2_QVMstrftime(char *valbuff, int sizebuff, char *fmt, int offset)
{
	struct tm *newtime;
	time_t long_time;
	int ret;

	if (sizebuff <= 0 || !valbuff) {
		Con_DPrintf("PF2_QVMstrftime: wrong buffer\n");
		return 0;
	}

	time(&long_time);
	long_time += offset;
	newtime = localtime(&long_time);

	if (!newtime)
	{
		valbuff[0] = 0; // or may be better set to "#bad date#" ?
		return 0;
	}

	ret = strftime(valbuff, sizebuff-1, fmt, newtime);

	if (!ret) {
		valbuff[0] = 0; // or may be better set to "#bad date#" ?
		Con_DPrintf("PF2_QVMstrftime: buffer size too small\n");
		return 0;
	}
	return ret;
}

// a la the ZQ_PAUSE QC extension
void PF2_setpause(int pause)
{
	if (pause != (sv.paused & 1))
		SV_TogglePause (NULL, 1);
}

void PF2_SetUserInfo(int entnum, char *k, char *v, int flags)
{
	client_t *cl;
	char   key[MAX_KEY_STRING];
	char   value[MAX_KEY_STRING];
	char   s[MAX_KEY_STRING * 4];
	int     i;
	extern char *shortinfotbl[];

	if (strstr(k, "&c") || strstr(k, "&r") || strstr(v, "&c") || strstr(v, "&r"))
		return;

	if ( entnum < 1 || entnum > MAX_CLIENTS )
	{
		Con_Printf( "tried to change userinfo a non-client %d \n", entnum );
		return;
	}

	cl = &svs.clients[entnum - 1];

	// well, our API is weird
	if ( cl->isBot )
	{
		PF2_SetBotUserInfo(entnum, k, v, flags);
		return;
	}

	// tokenize

	snprintf( s, sizeof(s), "PF2_SetUserInfo \"%-.*s\" \"%-.*s\"", (int)sizeof(key), k, (int)sizeof(value), v );

	Cmd_TokenizeString( s );

	// well, PR2_UserInfoChanged() may call PF2_SetUserInfo() again, so we better save thouse
	strlcpy( key, Cmd_Argv(1), sizeof(key) );
	strlcpy( value, Cmd_Argv(2), sizeof(value) );

	if( sv_vm )
	{
		pr_global_struct->time = sv.time;
		pr_global_struct->self = EDICT_TO_PROG(cl->edict);

		if (PR2_UserInfoChanged(0))
			return;
	}

	if ( flags & SETUSERINFO_STAR )
		Info_SetStar( &cl->_userinfo_ctx_, key, value );
	else
		Info_Set( &cl->_userinfo_ctx_, key, value );

	SV_ExtractFromUserinfo( cl, !strcmp( key, "name" ) );
	PR2_UserInfoChanged(1);

	for ( i = 0; shortinfotbl[i] != NULL; i++ )
	{
		if ( !strcmp( key, shortinfotbl[i] ) )
		{
			char *nuw = Info_Get( &cl->_userinfo_ctx_, key );

			// well, here we do not have if ( flags & SETUSERINFO_STAR ) because shortinfotbl[] does't have any star key

			Info_Set( &cl->_userinfoshort_ctx_, key, nuw );

			i = cl - svs.clients;
			MSG_WriteByte( &sv.reliable_datagram, svc_setinfo );
			MSG_WriteByte( &sv.reliable_datagram, i );
			MSG_WriteString( &sv.reliable_datagram, key );
			MSG_WriteString( &sv.reliable_datagram, nuw );
			break;
		}
	}
}

void PF2_VisibleTo(int viewer, int first, int len, byte *visible)
{
	int e, last = first + len;
	edict_t *ent;
	edict_t *viewer_ent = EDICT_NUM(viewer);
	vec3_t org;
	byte *pvs;

	if (last > sv.num_edicts)
		last = sv.num_edicts;

	VectorAdd(viewer_ent->v->origin, viewer_ent->v->view_ofs, org);
	pvs = CM_FatPVS(org);

	for (e = first, ent = EDICT_NUM(e); e < last; e++, ent = NEXT_EDICT(ent))
	{
		int i;
		if (ent->e.num_leafs < 0 || ent->e.free
			|| (e >= 1 && e <= MAX_CLIENTS && svs.clients[e - 1].state != cs_spawned)) {
			continue; // Ignore free edicts or not active client.
		}
		for (i = 0; i < ent->e.num_leafs; i++) {
			if (pvs[ent->e.leafnums[i] >> 3] & (1 << (ent->e.leafnums[i]&7))) {
				visible[e - first] = true; // seems to be visible
				break;
			}
		}
	}
}

//===========================================================================
// SysCalls
//===========================================================================

#define VMV(x) _vmf(args[x]), _vmf(args[(x) + 1]), _vmf(args[(x) + 2])
#define VME(x) EDICT_NUM(args[x])
intptr_t PR2_GameSystemCalls(intptr_t *args) {
	switch (args[0]) {
	case G_GETAPIVERSION:
		return GAME_API_VERSION;
	case G_DPRINT:
		Con_DPrintf("%s", (const char *)VMA(1));
		return 0;
	case G_ERROR:
		PR2_RunError(VMA(1));
		return 0;
	case G_GetEntityToken:
		VM_CheckBounds(sv_vm, args[1], args[2]);
		pr2_ent_data_ptr = COM_Parse(pr2_ent_data_ptr);
		strlcpy(VMA(1), com_token, args[2]);
		return pr2_ent_data_ptr != NULL;
	case G_SPAWN_ENT:
		return NUM_FOR_EDICT(ED_Alloc());
	case G_REMOVE_ENT:
		ED_Free(VME(1));
		return 0;
	case G_PRECACHE_SOUND:
		PF2_precache_sound(VMA(1));
		return 0;
	case G_PRECACHE_MODEL:
		PF2_precache_model(VMA(1));
		return 0;
	case G_LIGHTSTYLE:
		PF2_lightstyle(args[1], VMA(2));
		return 0;
	case G_SETORIGIN:
		PF2_setorigin(VME(1), VMV(2));
		return 0;
	case G_SETSIZE:
		PF2_setsize(VME(1), VMV(2), VMV(5));
		return 0;
	case G_SETMODEL:
		PF2_setmodel(VME(1), VMA(2));
		return 0;
	case G_BPRINT: {
		int flags = args[3];
		if (gamedata.APIversion < 15)
			flags = 0;
		SV_BroadcastPrintfEx(args[1], flags, "%s", VMA(2));
	}
		return 0;
	case G_SPRINT:
		PF2_sprint(args[1], args[2], VMA(3), args[4]);
		return 0;
	case G_CENTERPRINT:
		PF2_centerprint(args[1], VMA(2));
		return 0;
	case G_AMBIENTSOUND:
		PF2_ambientsound(VMV(1), VMA(4), VMF(5), VMF(6));
		return 0;
	case G_SOUND:
		/*
		=================
		PF2_sound

		Each entity can have eight independant sound sources, like voice,
		weapon, feet, etc.

		Channel 0 is an auto-allocate channel, the others override anything
		already running on that entity/channel pair.

		An attenuation of 0 will play full volume everywhere in the level.
		Larger attenuations will drop off.
		void sound( gedict_t * ed, int channel, char *samp, float vol, float att )
		=================
		*/
		SV_StartSound(VME(1), args[2], VMA(3), VMF(4) * 255, VMF(5));
		return 0;
	case G_TRACELINE:
		PF2_traceline(VMV(1), VMV(4), args[7], args[8]);
		return 0;
	case G_CHECKCLIENT:
		return PF2_checkclient();
	case G_STUFFCMD:
		PF2_stuffcmd(args[1], VMA(2), args[3]);
		return 0;
	case G_LOCALCMD:
		/* =================
		Sends text over to the server's execution buffer

		localcmd (string)
		================= */
		Cbuf_AddTextEx(&cbuf_server, VMA(1));
		return 0;
	case G_CVAR:
		return PASSFLOAT(Cvar_Value(VMA(1)));
	case G_CVAR_SET:
		Cvar_SetByName(VMA(1), VMA(2));
		return 0;
	case G_FINDRADIUS:
		return PF2_FindRadius(NUM_FOR_GAME_EDICT(VMA(1)), (float *)VMA(2), VMF(3));
	case G_WALKMOVE:
		return PF2_walkmove(VME(1), VMF(2), VMF(3));
	case G_DROPTOFLOOR:
		return PF2_droptofloor(VME(1));
	case G_CHECKBOTTOM:
		return SV_CheckBottom(VME(1));
	case G_POINTCONTENTS:
		return PF2_pointcontents(VMV(1));
	case G_NEXTENT:
		return PF2_nextent(args[1]);
	case G_AIM:
		return 0;
	case G_MAKESTATIC:
		PF2_makestatic(VME(1));
		return 0;
	case G_SETSPAWNPARAMS:
		PF2_setspawnparms(args[1]);
		return 0;
	case G_CHANGELEVEL:
		PF2_changelevel(VMA(1), VMA(2));
		return 0;
	case G_LOGFRAG:
		PF2_logfrag(args[1], args[2]);
		return 0;
	case G_GETINFOKEY:
		VM_CheckBounds(sv_vm, args[3], args[4]);
		PF2_infokey(args[1], VMA(2), VMA(3), args[4]);
		return 0;
	case G_MULTICAST:
		PF2_multicast(VMV(1), args[4]);
		return 0;
	case G_DISABLEUPDATES:
		PF2_disable_updates(args[1], VMF(2));
		return 0;
	case G_WRITEBYTE:
		PF2_WriteByte(args[1], args[2]);
		return 0;
	case G_WRITECHAR:
		PF2_WriteChar(args[1], args[2]);
		return 0;
	case G_WRITESHORT:
		PF2_WriteShort(args[1], args[2]);
		return 0;
	case G_WRITELONG:
		PF2_WriteLong(args[1], args[2]);
		return 0;
	case G_WRITEANGLE:
		PF2_WriteAngle(args[1], VMF(2));
		return 0;
	case G_WRITECOORD:
		PF2_WriteCoord(args[1], VMF(2));
		return 0;
	case G_WRITESTRING:
		PF2_WriteString(args[1], VMA(2));
		return 0;
	case G_WRITEENTITY:
		PF2_WriteEntity(args[1], args[2]);
		return 0;
	case G_FLUSHSIGNON:
		SV_FlushSignon();
		return 0;
	case g_memset:
		VM_CheckBounds(sv_vm, args[1], args[3]);
		memset(VMA(1), args[2], args[3]);
		return args[1];
	case g_memcpy:
		VM_CheckBounds2(sv_vm, args[1], args[2], args[3]);
		memcpy(VMA(1), VMA(2), args[3]);
		return args[1];
	case g_strncpy:
		VM_CheckBounds2(sv_vm, args[1], args[2], args[3]);
		strncpy(VMA(1), VMA(2), args[3]);
		return args[1];
	case g_sin:
		return PASSFLOAT(sin(VMF(1)));
	case g_cos:
		return PASSFLOAT(cos(VMF(1)));
	case g_atan2:
		return PASSFLOAT(atan2(VMF(1), VMF(2)));
	case g_sqrt:
		return PASSFLOAT(sqrt(VMF(1)));
	case g_floor:
		return PASSFLOAT(floor(VMF(1)));
	case g_ceil:
		return PASSFLOAT(ceil(VMF(1)));
	case g_acos:
		return PASSFLOAT(acos(VMF(1)));
	case G_CMD_ARGC:
		return Cmd_Argc();
	case G_CMD_ARGV:
		VM_CheckBounds(sv_vm, args[2], args[3]);
		strlcpy(VMA(2), Cmd_Argv(args[1]), args[3]);
		return 0;
	case G_TraceCapsule:
		PF2_TraceCapsule(VMV(1), VMV(4), args[7], VME(8), VMV(9), VMV(12));
		return 0;
	case G_FSOpenFile:
		return PF2_FS_OpenFile(VMA(1), (fileHandle_t *)VMA(2), (fsMode_t)args[3]);
	case G_FSCloseFile:
		PF2_FS_CloseFile((fileHandle_t)args[1]);
		return 0;
	case G_FSReadFile:
		VM_CheckBounds(sv_vm, args[1], args[2]);
		return PF2_FS_ReadFile(VMA(1), args[2], (fileHandle_t)args[3]);
	case G_FSWriteFile:
		VM_CheckBounds(sv_vm, args[1], args[2]);
		return PF2_FS_WriteFile(VMA(1), args[2], (fileHandle_t)args[3]);
	case G_FSSeekFile:
		return PF2_FS_SeekFile((fileHandle_t)args[1], args[2], (fsOrigin_t)args[3]);
	case G_FSTellFile:
		return PF2_FS_TellFile((fileHandle_t)args[1]);
	case G_FSGetFileList:
		VM_CheckBounds(sv_vm, args[3], args[4]);
		return PF2_FS_GetFileList(VMA(1), VMA(2), VMA(3), args[4], args[5]);
	case G_CVAR_SET_FLOAT:
		Cvar_SetValueByName(VMA(1), VMF(2));
		return 0;
	case G_CVAR_STRING:
		VM_CheckBounds(sv_vm, args[2], args[3]);
		strlcpy(VMA(2), Cvar_String(VMA(1)), args[3]);
		return 0;
	case G_Map_Extension:
		return PF2_Map_Extension(VMA(1), args[2]);
	case G_strcmp:
		return strcmp(VMA(1), VMA(2));
	case G_strncmp:
		return strncmp(VMA(1), VMA(2), args[3]);
	case G_stricmp:
		return strcasecmp(VMA(1), VMA(2));
	case G_strnicmp:
		return strncasecmp(VMA(1), VMA(2), args[3]);
	case G_Find:
		return PF2_Find(NUM_FOR_GAME_EDICT(VMA(1)), args[2], VMA(3));
	case G_executecmd:
		PF2_executecmd();
		return 0;
	case G_conprint:
		Sys_Printf("%s", VMA(1));
		return 0;
	case G_readcmd:
		VM_CheckBounds(sv_vm, args[2], args[3]);
		PF2_readcmd(VMA(1), VMA(2), args[3]);
		return 0;
	case G_redirectcmd:
		PF2_redirectcmd(NUM_FOR_GAME_EDICT(VMA(1)), VMA(2));
		return 0;
	case G_Add_Bot:
		return PF2_Add_Bot(VMA(1), args[2], args[3], VMA(4));
	case G_Remove_Bot:
		PF2_Remove_Bot(args[1]);
		return 0;
	case G_SetBotUserInfo:
		PF2_SetBotUserInfo(args[1], VMA(2), VMA(3), args[4]);
		return 0;
	case G_SetBotCMD:
		PF2_SetBotCMD(args[1], args[2], VMV(3), args[6], args[7], args[8], args[9], args[10]);
		return 0;
	case G_QVMstrftime:
		VM_CheckBounds(sv_vm, args[1], args[2]);
		return PF2_QVMstrftime(VMA(1), args[2], VMA(3), args[4]);
	case G_CMD_ARGS:
		VM_CheckBounds(sv_vm, args[1], args[2]);
		strlcpy(VMA(1), Cmd_Args(), args[2]);
		return 0;
	case G_CMD_TOKENIZE:
		Cmd_TokenizeString(VMA(1));
		return 0;
	case g_strlcpy:
		VM_CheckBounds(sv_vm, args[1], args[3]);
		return strlcpy(VMA(1), VMA(2), args[3]);
	case g_strlcat:
		VM_CheckBounds(sv_vm, args[1], args[3]);
		return strlcat(VMA(1), VMA(2), args[3]);
	case G_MAKEVECTORS:
		AngleVectors(VMA(1), pr_global_struct->v_forward, pr_global_struct->v_right,
								 pr_global_struct->v_up);
		return 0;
	case G_NEXTCLIENT:
		return PF2_nextclient(NUM_FOR_GAME_EDICT(VMA(1)));
	case G_PRECACHE_VWEP_MODEL:
		return PF2_precache_vwep_model(VMA(1));
	case G_SETPAUSE:
		PF2_setpause(args[1]);
		return 0;
	case G_SETUSERINFO:
		PF2_SetUserInfo(args[1], VMA(2), VMA(3), args[4]);
		return 0;
	case G_MOVETOGOAL:
		PF2_MoveToGoal(VMF(1));
		return 0;
	case G_VISIBLETO:
		VM_CheckBounds(sv_vm, args[4], args[3]);
		memset(VMA(4), 0, args[3]); // Ensure same memory state on each run.
		PF2_VisibleTo(args[1], args[2], args[3], VMA(4));
		return 0;
	default:
		SV_Error("Bad game system trap: %ld", (long int)args[0]);
	}
	return 0;
}

#endif /* USE_PR2 */

#endif // !CLIENTONLY
