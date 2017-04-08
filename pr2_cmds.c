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

#ifdef USE_PR2

#include "qwsvdef.h"

#define SETUSERINFO_STAR          (1<<0) // allow set star keys

#ifdef SERVERONLY
#define Cbuf_AddTextEx(x, y) Cbuf_AddText(y)
#define Cbuf_ExecuteEx(x) Cbuf_Execute()
#endif

char	*pr2_ent_data_ptr;
vm_t	*sv_vm = NULL;

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
	if( sv_vm->type == VM_BYTECODE )
		QVM_StackTrace( (qvm_t *) sv_vm->hInst );

	Con_Printf("%s\n", string);

	SV_Error("Program error: %s", string);
}

void PR2_CheckEmptyString(char *s)
{
	if (!s || s[0] <= ' ')
		PR2_RunError("Bad string");
}

void PF2_GetApiVersion(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int = GAME_API_VERSION;
}

void PF2_GetEntityToken(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	pr2_ent_data_ptr = COM_Parse(pr2_ent_data_ptr);
	strlcpy((char*)VM_POINTER(base,mask,stack[0].string), com_token,  stack[1]._int);

	retval->_int= pr2_ent_data_ptr != NULL;
}

void PF2_DPrint(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	Con_DPrintf("%s", VM_POINTER(base,mask,stack[0].string));
}

void PF2_conprint(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	Sys_Printf("%s", VM_POINTER(base, mask, stack[0].string));
}

void PF2_Error(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	PR2_RunError((char *)VM_POINTER(base, mask, stack->string));
}

void PF2_Spawn(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int = NUM_FOR_EDICT( ED_Alloc() );
}

void PF2_Remove(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	ED_Free(EDICT_NUM(stack[0]._int));
}

void PF2_precache_sound(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int i;
	char*s;
	if (sv.state != ss_loading)
		PR2_RunError("PF_Precache_*: Precache can only be done in spawn "
		             "functions");
	s = (char *) VM_POINTER(base,mask,stack[0].string);
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

void PF2_precache_model(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int 	i;
	char 	*s;

	if (sv.state != ss_loading)
		PR2_RunError("PF_Precache_*: Precache can only be done in spawn "
		             "functions");

	s = (char *) VM_POINTER(base,mask,stack[0].string);
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

void PF2_precache_vwep_model(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int 	i;
	char 	*s;

	if (sv.state != ss_loading)
		PR2_RunError("PF_Precache_*: Precache can only be done in spawn "
		             "functions");

	s = (char *) VM_POINTER(base,mask,stack[0].string);
	PR2_CheckEmptyString(s);

	// the strings are transferred via the stufftext mechanism, hence the stringency
	if (strchr(s, '"') || strchr(s, ';') || strchr(s, '\n'  ) || strchr(s, '\t') || strchr(s, ' '))
		PR2_RunError ("Bad string\n");

	for (i = 0; i < MAX_VWEP_MODELS; i++)
	{
		if (!sv.vw_model_name[i]) {
			sv.vw_model_name[i] = s;
			retval->_int = i;
			return;
		}
	}
	PR2_RunError ("PF_precache_vwep_model: overflow");
}

/*
=================
PF2_setorigin
 
This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.
 
setorigin (entity, origin)
=================
*/

void PF2_setorigin(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	vec3_t origin;
	edict_t	*e;

	e = EDICT_NUM(stack[0]._int);

	origin[0] = stack[1]._float;
	origin[1] = stack[2]._float;
	origin[2] = stack[3]._float;

	VectorCopy(origin, e->v.origin);
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
void PF2_setsize(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	//vec3_t min, max;
	edict_t	*e ;

	e = EDICT_NUM(stack[0]._int);

	e->v.mins[0] = stack[1]._float;
	e->v.mins[1] = stack[2]._float;
	e->v.mins[2] = stack[3]._float;

	e->v.maxs[0] = stack[4]._float;
	e->v.maxs[1] = stack[5]._float;
	e->v.maxs[2] = stack[6]._float;

	VectorSubtract(e->v.maxs, e->v.mins, e->v.size);

	SV_LinkEdict(e, false);
}

/*
=================
PF2_setmodel
 
setmodel(entity, model)
Also sets size, mins, and maxs for inline bmodels
=================
*/
void PF2_setmodel(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	edict_t		*e;
	char		*m;
	char		**check;
	int		i;
	cmodel_t	*mod;

	e = EDICT_NUM(stack[0]._int);
	m = (char *) VM_POINTER(base,mask,stack[1].string);
	if(!m)
		m = "";
	// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++)
		if (!strcmp(*check, m))
			break;

	if (!*check)
		PR2_RunError("no precache: %s\n", m);

	PR2_SetEntityString(e, &e->v.model, m);
	e->v.modelindex = i;

	// if it is an inline model, get the size information for it
	if (m[0] == '*')
	{
		mod = CM_InlineModel (m);
		VectorCopy(mod->mins, e->v.mins);
		VectorCopy(mod->maxs, e->v.maxs);
		VectorSubtract(mod->maxs, mod->mins, e->v.size);
		SV_LinkEdict(e, false);
	}

}

/*
=================
PF2_bprint
 
broadcast print to everyone on server
 
bprint(value)
=================
*/
void PF2_bprint(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	SV_BroadcastPrintfEx(stack[0]._int, stack[2]._int, "%s", VM_POINTER(base,mask,stack[1].string));
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

void PF2_sprint(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	client_t *client, *cl;
	int entnum = stack[0]._int;
	int level = stack[1]._int;
	int flags = stack[3]._int; // using this atm just as hint to not put this message in mvd demo
	char *s = (char *) VM_POINTER(base,mask,stack[2].string);
	int i;

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
void PF2_centerprint(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	client_t *cl, *spec;
	int entnum = stack[0]._int;
	char *s = (char *) VM_POINTER(base,mask,stack[1].string);
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
void PF2_ambientsound(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	char	**check;
	int		i, soundnum;
	vec3_t 	pos;
	char 	*samp;
	float 	vol;
	float 	attenuation;

	pos[0] = stack[0]._float;
	pos[1] = stack[1]._float;
	pos[2] = stack[2]._float;

	samp = (char *) VM_POINTER(base,mask,stack[3].string);
	if( !samp )
		samp = "";
	vol 		= stack[4]._float;
	attenuation 	= stack[5]._float;

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
PF2_sound
 
Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.
 
Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.
 
An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.
 
=================
*/
void PF2_sound(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	edict_t         *entity         = EDICT_NUM(stack[0]._int);
	int             channel         = stack[1]._int;
	char            *sample         = (char *) VM_POINTER(base,mask,stack[2].string);
	int             volume          = stack[3]._float * 255;
	float           attenuation     = stack[4]._float;

	SV_StartSound(entity, channel, sample, volume, attenuation);
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
void PF2_traceline(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	trace_t	trace;
	edict_t	*ent;
	vec3_t v1, v2;
	int nomonsters, entnum;

	v1[0] = stack[0]._float;
	v1[1] = stack[1]._float;
	v1[2] = stack[2]._float;

	v2[0] = stack[3]._float;
	v2[1] = stack[4]._float;
	v2[2] = stack[5]._float;

	nomonsters = stack[6]._int;
	entnum = stack[7]._int;

	ent = EDICT_NUM(entnum);

	if (sv_antilag.value == 2)
	{
		//if (! (entnum >= 1 && entnum <= MAX_CLIENTS && svs.clients[entnum - 1].isBot))
			nomonsters |= MOVE_LAGGED;
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
 
void    trap_TraceCapsule( float v1_x, float v1_y, float v1_z, 
			float v2_x, float v2_y, float v2_z, 
			int nomonst, int edn ,
			float min_x, float min_y, float min_z, 
			float max_x, float max_y, float max_z);
 
=================
*/
void PF2_TraceCapsule(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	trace_t	trace;
	edict_t	*ent;
	vec3_t v1, v2, v3, v4;
	int nomonsters;//, entnum;

	v1[0] = stack[0]._float;
	v1[1] = stack[1]._float;
	v1[2] = stack[2]._float;

	v2[0] = stack[3]._float;
	v2[1] = stack[4]._float;
	v2[2] = stack[5]._float;

	nomonsters = stack[6]._int;

	ent = EDICT_NUM(stack[7]._int);

	v3[0] = stack[8]._float;
	v3[1] = stack[9]._float;
	v3[2] = stack[10]._float;

	v4[0] = stack[11]._float;
	v4[1] = stack[12]._float;
	v4[2] = stack[13]._float;

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

		if (ent->e->free)
			continue;
		if (ent->v.health <= 0)
			continue;
		if ((int) ent->v.flags & FL_NOTARGET)
			continue;

		// anything that is a client, or has a client as an enemy
		break;
	}

	// get the PVS for the entity
	VectorAdd (ent->v.origin, ent->v.view_ofs, org);
	checkpvs = CM_LeafPVS (CM_PointInLeaf(org));

	return i;
}

#define	MAX_CHECK	16

void PF2_checkclient(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
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
	if (ent->e->free || ent->v.health <= 0)
	{
		// RETURN_EDICT(sv.edicts);
		retval->_int = NUM_FOR_EDICT(sv.edicts);
		return;
	}

	// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd(self->v.origin, self->v.view_ofs, view);
	l = CM_Leafnum(CM_PointInLeaf(view)) - 1;
	if ((l < 0) || !(checkpvs[l >> 3] & (1 << (l & 7))))
	{
		retval->_int = NUM_FOR_EDICT(sv.edicts);
		return;

	}

	retval->_int = NUM_FOR_EDICT(ent);
	return;

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

void PF2_stuffcmd(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	char *str=NULL, *buf=NULL;
	client_t *cl, *spec;
	int entnum = stack[0]._int;
	int flags = stack[2]._int; // using this atm just as hint to not put this in mvd demo
	int j;

	str = (char *) VM_POINTER(base,mask,stack[1].string);
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
PF2_localcmd
 
Sends text over to the server's execution buffer
 
localcmd (string)
=================
*/
void PF2_localcmd(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	Cbuf_AddTextEx(&cbuf_server, (char *)VM_POINTER(base,mask,stack[0].string));
}

void PF2_executecmd(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
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

void PF2_readcmd (byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	char		*str;
	extern char outputbuf[];
	char		*buf;
	int 		sizebuff;
	extern 	redirect_t sv_redirected;
	redirect_t old;

	str = (char *) VM_POINTER(base,mask,stack[0].string);
	buf = (char *) VM_POINTER(base,mask,stack[1].string);
	sizebuff = stack[2]._int;

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

void PF2_redirectcmd(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	char		*str;
	int 		entnum;
	//	redirect_t old;

	extern redirect_t sv_redirected;

	str = (char *)VM_POINTER(base, mask, stack[1].string);
	if (sv_redirected) {
		Cbuf_AddTextEx(&cbuf_server, str);
		Cbuf_ExecuteEx(&cbuf_server);
		return;
	}

	entnum = NUM_FOR_EDICT((edict_t *)VM_POINTER(base, mask, stack[0]._int));

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
PF2_cvar
 
float   trap_cvar( const char *var );
=================
*/
void PF2_cvar(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float =  Cvar_Value((char *)VM_POINTER(base,mask,stack[0].string));
}

/*
=================
PF2_cvar_string
 
void trap_cvar_string( const char *var, char *buffer, int bufsize )
=================
*/
void PF2_cvar_string(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	intptr_t buff_off = stack[1]._int;
	intptr_t buffsize = stack[2]._int;

	if( ( buff_off )  &(~mask))
		return;

	if( ( buff_off + buffsize ) &(~mask))
		return;

	strlcpy((char *)VM_POINTER(base,mask,buff_off),
	        Cvar_String((char *)VM_POINTER(base,mask,stack[0].string)), buffsize);
}

/*
=================
PF2_cvar_set
 
void    trap_cvar_set( const char *var, const char *val );
=================
*/
void PF2_cvar_set(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	Cvar_SetByName((char *) VM_POINTER(base,mask,stack[0].string), (char *) VM_POINTER(base,mask,stack[1].string));
}
/*
=================
PF2_cvar_set_float
 
void    trap_cvar_set_float( const char *var, float val );
=================
*/
void PF2_cvar_set_float(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	Cvar_SetValueByName((char *)VM_POINTER(base,mask,stack[0].string), stack[1]._float);
}

/*
=================
PF2_FindRadius

Returns a chain of entities that have origins within a spherical area
gedict_t *findradius( gedict_t * start, vec3_t org, float rad );
=================
*/

void PF2_FindRadius( byte * base, uintptr_t mask, pr2val_t * stack, pr2val_t * retval )
{
	int     e,j;

	edict_t *ed;
	float* org;
	vec3_t	eorg;
	float rad;

	e = NUM_FOR_EDICT( (edict_t *) VM_POINTER( base, mask, stack[0]._int ) );
	org = (float *) VM_POINTER( base, mask, stack[1]._int );
	rad = stack[2]._float;

	for ( e++; e < sv.num_edicts; e++ )
	{
		ed = EDICT_NUM( e );

		if (ed->e->free)
			continue;
		if (ed->v.solid == SOLID_NOT)
			continue;
		for (j=0 ; j<3 ; j++)
			eorg[j] = org[j] - (ed->v.origin[j] + (ed->v.mins[j] + ed->v.maxs[j])*0.5);			
		if (VectorLength(eorg) > rad)
			continue;
		retval->_int = POINTER_TO_VM( base, mask, ed );
		return;
	}
	retval->_int = 0;
	return;
}

/*
===============
PF2_walkmove
 
float(float yaw, float dist) walkmove
===============
*/
void PF2_walkmove(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(int entn, float yaw, float dist)
{
	edict_t		*ent;
	float		yaw, dist;
	vec3_t		move;
	//	dfunction_t	*oldf;
	int			oldself;
	//	int			ret;
	//

	/*if( sv_vm->type == VM_BYTECODE)///FIXME !!! not worked yet
	{
		retval->_int =  0; 
		return;
	}*/
	//	ent = PROG_TO_EDICT(pr_global_struct->self);
	//	yaw = G_FLOAT(OFS_PARM0);
	//	dist = G_FLOAT(OFS_PARM1);
	ent  = EDICT_NUM(stack[0]._int);
	yaw  = stack[1]._float;
	dist = stack[2]._float;

	if (!((int) ent->v.flags & (FL_ONGROUND | FL_FLY | FL_SWIM)))
	{
		retval->_int =  0;
		return;

	}

	yaw = yaw * M_PI * 2 / 360;

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	//	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	retval->_int = SV_movestep(ent, move, true);


	// restore program state
	//	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
	return;
}

/*
===============
PF2_MoveToGoal
 
float(float dist) PF2_MoveToGoal
===============
*/

void PF2_MoveToGoal(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	edict_t		*ent, *goal;
	float		dist;

	//	dfunction_t	*oldf;
	int			oldself;

	ent  = PROG_TO_EDICT(pr_global_struct->self);
	goal = PROG_TO_EDICT(ent->v.goalentity);
	dist = stack[0]._float;

	if ( !( (int)ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		retval->_int =  0;
		return;
	}

	// if the next step hits the enemy, return immediately
	if ( PROG_TO_EDICT(ent->v.enemy) != sv.edicts && SV_CloseEnough (ent, goal, dist) )
		return;

	// save program state, because SV_movestep may call other progs
	//	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	// bump around...
	if ( (rand()&3)==1 || !SV_StepDirection (ent, ent->v.ideal_yaw, dist))
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
void PF2_droptofloor(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	ent = EDICT_NUM(stack[0]._int);

	VectorCopy(ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Trace(ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
	{
		retval->_int =  0;
		return;
	}
	else
	{
		VectorCopy(trace.endpos, ent->v.origin);
		SV_LinkEdict(ent, false);
		ent->v.flags = (int) ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(trace.e.ent);
		retval->_int =  1;
		return;
	}
}

/*
===============
PF2_lightstyle
 
void(int style, string value) lightstyle
===============
*/
void PF2_lightstyle(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	client_t	*client;
	int			j,style;
	char*	val;

	style 	= stack[0]._int;
	val	= (char *) VM_POINTER(base,mask,stack[1]._int);

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
PF2_checkbottom
=============
*/
void PF2_checkbottom(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int = SV_CheckBottom(EDICT_NUM(stack[0]._int));
}

/*
=============
PF2_pointcontents
=============
*/
void PF2_pointcontents(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	vec3_t v;
	v[0] = stack[0]._float;
	v[1] = stack[1]._float;
	v[2] = stack[2]._float;

	retval->_int = SV_PointContents(v);
}

/*
=============
PF2_nextent
 
entity nextent(entity)
=============
*/
void PF2_nextent(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int		i;
	edict_t	*ent;

	i = stack[0]._int;
	while (1)
	{
		i++;
		if (i >= sv.num_edicts)
		{
			retval->_int = 0;
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->e->free)
		{
			retval->_int = i;
			return;
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
void PF2_nextclient(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int		i;
	edict_t	*ent;

	i = NUM_FOR_EDICT((edict_t *) VM_POINTER(base,mask,stack[0]._int));;
	while (1)
	{
		i++;
		if (i < 1 || i > MAX_CLIENTS)
		{
			retval->_int = 0;
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->e->free) // actually that always true for clients edicts
		{
			if (svs.clients[i-1].state == cs_spawned) // client in game
			{
				retval->_int = POINTER_TO_VM(base,mask,ent);
				return;
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
void PF2_Find (byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int		e;
	int 		fofs;
	char*		str, *t;
	edict_t	*ed;

	e  = NUM_FOR_EDICT((edict_t *) VM_POINTER(base,mask,stack[0]._int));
	fofs = stack[1]._int;

	str = (char *) VM_POINTER(base,mask,stack[2].string);

	if(!str)
		PR2_RunError ("PF2_Find: bad search string");

	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->e->free)
			continue;

		if (!(intptr_t*)((byte*)ed + fofs))
			continue;

		t = (char *) VM_POINTER(base,mask,*(intptr_t*)((byte*)ed + fofs));

		if (!t)
			continue;

		if (!strcmp(t,str))
		{
			retval->_int = POINTER_TO_VM(base,mask,ed);
			return;
		}
	}
	retval->_int = 0;
	return;
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

void PF2_WriteByte(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to   = stack[0]._int;
	int data = stack[1]._int;

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

void PF2_WriteChar(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to   = stack[0]._int;
	int data = stack[1]._int;

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

void PF2_WriteShort(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to   = stack[0]._int;
	int data = stack[1]._int;

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

void PF2_WriteLong(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to   = stack[0]._int;
	int data = stack[1]._int;

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

void PF2_WriteAngle(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to     = stack[0]._int;
	float data = stack[1]._float;

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

void PF2_WriteCoord(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to     = stack[0]._int;
	float data = stack[1]._float;

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

void PF2_WriteString(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to     = stack[0]._int;
	char* data = (char *) VM_POINTER(base,mask,stack[1].string);

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


void PF2_WriteEntity(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int to     = stack[0]._int;
	int data   = stack[1]._int;

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
void PF2_makestatic(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	entity_state_t* s;
	edict_t	*ent;

	ent = EDICT_NUM(stack[0]._int);
	if (sv.static_entity_count >= sizeof(sv.static_entities) / sizeof(sv.static_entities[0])) {
		ED_Free (ent);
		return;
	}

	s = &sv.static_entities[sv.static_entity_count];
	memset(s, 0, sizeof(sv.static_entities[0]));
	s->number = sv.static_entity_count + 1;
	s->modelindex = SV_ModelIndex(PR_GetEntityString(ent->v.model));
	if (!s->modelindex) {
		ED_Free (ent);
		return;
	}
	s->frame = ent->v.frame;
	s->colormap = ent->v.colormap;
	s->skinnum = ent->v.skin;
	VectorCopy(ent->v.origin, s->origin);
	VectorCopy(ent->v.angles, s->angles);
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
void PF2_setspawnparms(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int			i;
	//edict_t		*ent;
	int			entnum=stack[0]._int;
	client_t	*client;

	//ent = EDICT_NUM(entnum);

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
void PF2_changelevel(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	static int last_spawncount;
	char *s = (char *) VM_POINTER(base,mask,stack[0].string);
	char *entfile = (char *) VM_POINTER(base,mask,stack[1].string);
	char expanded[MAX_QPATH];

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
void PF2_logfrag(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	//	edict_t	*ent1, *ent2;
	int		e1, e2;
	char	*s;
	// -> scream
	time_t		t;
	struct tm	*tblock;
	// <-

	//ent1 = G_EDICT(OFS_PARM0);
	//ent2 = G_EDICT(OFS_PARM1);

	e1 = stack[0]._int;
	e2 = stack[1]._int;

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
void PF2_infokey(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(int e1, char *key, char *valbuff, int sizebuff)
{
	static char ov[256];

	//	edict_t	*e;
	int		e1 	= stack[0]._int;
	char		*key	= (char *) VM_POINTER(base,mask,stack[1].string);
	char		*valbuff= (char *) VM_POINTER(base,mask,stack[2].string);
	char		*value;
	int		sizebuff= stack[3]._int;

	//	e = G_EDICT(OFS_PARM0);
	//	e1 = NUM_FOR_EDICT(e);
	//	key = G_STRING(OFS_PARM1);

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
			   )
				value = "yes";
		}
		else if (!strcmp(key, "date_str")) { // qqshka - qvm does't have any time builtin support, so add this
			date_t date;

			SV_TimeOfDay(&date);
			snprintf(ov, sizeof(ov), "%s", date.str);
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
void PF2_multicast(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(vec3_t o, int to)
{
	vec3_t o;
	int to;

	o[0] = stack[0]._float;
	o[1] = stack[1]._float;
	o[2] = stack[2]._float;
	to   = stack[3]._int;
	SV_Multicast(o, to);
}

/*
==============
PF2_disable_updates
 
void(entiny whom, float time) disable_updates
==============
*/
void PF2_disable_updates(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(int entnum, float time)
{
	client_t *client;
	int entnum = stack[0]._int;
	float time1   = stack[1]._float;

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

/*
==============
PR2_FlushSignon();
==============
*/
void PR2_FlushSignon(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	SV_FlushSignon();
}

/*
==============
PF2_cmdargc
==============
*/
void PF2_cmdargc(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int = Cmd_Argc();
}

/*
==============
PF2_cmdargv
==============
*/
void PF2_cmdargv(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(int arg, char *valbuff, int sizebuff)
{
	strlcpy((char *) VM_POINTER(base,mask,stack[1].string), Cmd_Argv(stack[0]._int), stack[2]._int);
}

/*
==============
PF2_cmdargs
==============
*/
void PF2_cmdargs(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(char *valbuff, int sizebuff)
{
	strlcpy((char *) VM_POINTER(base,mask,stack[0].string), Cmd_Args(), stack[1]._int);
}

/*
==============
PF2_tokenize
==============
*/
void PF2_tokenize(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(char *str)
{
	Cmd_TokenizeString((char *) VM_POINTER(base,mask,stack[0].string));
}

void PF2_fixme(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	PR2_RunError ("unimplemented bulitin");
}

void PF2_memset(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	memset(VM_POINTER(base, mask, stack[0].string), stack[1]._int, stack[2]._int);

	retval->_int = stack[0].string;
}

void PF2_memcpy(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	memcpy(VM_POINTER(base, mask, stack[0].string), VM_POINTER(base, mask, stack[1].string), stack[2]._int);

	retval->_int = stack[0].string;
}
void PF2_strncpy(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	strncpy((char *)VM_POINTER(base, mask, stack[0].string), (char *)VM_POINTER(base, mask, stack[1].string), stack[2]._int);

	retval->_int = stack[0].string;
}

void PF2_sin(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float=sin(stack[0]._float);
}

void PF2_cos(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float=cos(stack[0]._float);
}

void PF2_atan2(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float=atan2(stack[0]._float,stack[1]._float);
}

void PF2_sqrt(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float=sqrt(stack[0]._float);
}

void PF2_floor(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float=floor(stack[0]._float);
}
void PF2_ceil(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float=ceil(stack[0]._float);
}

void PF2_acos(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_float=acos(stack[0]._float);
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
/*
int	trap_FS_OpenFile(char*name, fileHandle_t* handle, fsMode_t fmode );
*/
//FIXME: read from paks
void PF2_FS_OpenFile(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	char *name=(char*)VM_POINTER(base,mask,stack[0].string);
	fileHandle_t* handle=(fileHandle_t*)VM_POINTER(base,mask,stack[1]._int);
	fsMode_t fmode = (fsMode_t) stack[2]._int;
	int i;

	if(pr2_num_open_files >= MAX_PR2_FILES)
	{
		retval->_int = -1;
		return ;
	}

	*handle = 0;
	for (i = 0; i < MAX_PR2_FILES; i++)
		if (!pr2_fopen_files[i].handle)
			break;
	if (i == MAX_PR2_FILES)	//too many already open
	{
		retval->_int = -1;
		return ;
	}

	if (FS_UnsafeFilename(name)) {
		// someone tried to be clever.
		retval->_int = -1;
		return ;
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
			retval->_int = -1;
			return ;
		}

		Con_DPrintf( "PF2_FS_OpenFile %s\n", name );

		retval->_int = VFS_GETLEN(pr2_fopen_files[i].handle);

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
			retval->_int = -1;
			return ;
		}
		Con_DPrintf( "PF2_FS_OpenFile %s\n", name );
		retval->_int = VFS_TELL(pr2_fopen_files[i].handle);

		break;
	default:
		retval->_int = -1;
		return ;

	}

	*handle = i+1;
	pr2_num_open_files++;
}
/*
void	trap_FS_CloseFile( fileHandle_t handle );
*/
void PF2_FS_CloseFile(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	fileHandle_t fnum =  stack[0]._int;
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

/*
int	trap_FS_SeekFile( fileHandle_t handle, int offset, int type );
*/

void PF2_FS_SeekFile(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	fileHandle_t fnum =  stack[0]._int;
	int offset = stack[1]._int;
	fsOrigin_t type = (fsOrigin_t) stack[2]._int;
	fnum--;

	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return;	//out of range

	if(!pr2_num_open_files)
		return;

	if(!(pr2_fopen_files[fnum].handle))
		return;
	if(type < 0 || type >= sizeof(seek_origin) / sizeof(seek_origin[0]))
		return;

	retval->_int = VFS_SEEK(pr2_fopen_files[fnum].handle, offset, seek_origin[type]);
}

/*
int	trap_FS_TellFile( fileHandle_t handle );
*/

void PF2_FS_TellFile(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	fileHandle_t fnum =  stack[0]._int;
	fnum--;

	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return;	//out of range

	if(!pr2_num_open_files)
		return;

	if(!(pr2_fopen_files[fnum].handle))
		return;

	retval->_int = VFS_TELL(pr2_fopen_files[fnum].handle);
}

/*
int	trap_FS_WriteFile( char*src, int quantity, fileHandle_t handle );
*/
void PF2_FS_WriteFile(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	char*dest;
	intptr_t memoffset = stack[0]._int;
	intptr_t quantity = stack[1]._int;
	fileHandle_t fnum =  stack[2]._int;
	fnum--;
	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return;	//out of range

	if(!pr2_num_open_files)
		return;

	if(!(pr2_fopen_files[fnum].handle))
		return;
	if( (memoffset) &(~mask))
		return;

	if( (memoffset+quantity) &(~mask))
		return;

	dest = (char*)VM_POINTER(base,mask,memoffset);
	retval->_int = VFS_WRITE(pr2_fopen_files[fnum].handle, dest, quantity);
}
/*
int	trap_FS_ReadFile( char*dest, int quantity, fileHandle_t handle );
*/
void PF2_FS_ReadFile(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	char*dest;
	intptr_t memoffset = stack[0]._int;
	intptr_t quantity = stack[1]._int;
	fileHandle_t fnum =  stack[2]._int;
	fnum--;
	if (fnum < 0 || fnum >= MAX_PR2_FILES)
		return;	//out of range

	if(!pr2_num_open_files)
		return;

	if(!(pr2_fopen_files[fnum].handle))
		return;
	if( (memoffset) &(~mask))
		return;

	if( (memoffset+quantity) &(~mask))
		return;

	dest = (char*)VM_POINTER(base,mask,memoffset);
	retval->_int = VFS_READ(pr2_fopen_files[fnum].handle, dest, quantity, NULL);
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

/*
int 	trap_FS_GetFileList(  const char *path, const char *extension, char *listbuf, int bufsize );
*/

static int GetFileList_Compare (const void *p1, const void *p2)
{
	return strcmp (*((char**)p1), *((char**)p2));
}

#define FILELIST_GAMEDIR_ONLY	(1<<0) // if set then search in gamedir only
#define FILELIST_WITH_PATH		(1<<1) // include path to file
#define FILELIST_WITH_EXT		(1<<2) // include extension of file

void PF2_FS_GetFileList(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
//	extern	searchpath_t *com_searchpaths; // evil, because this must be used in fs.c only...
	char	*gpath = NULL;

	char 	*list[MAX_DIRFILES];
	const 	int	list_cnt = sizeof(list) / sizeof(list[0]);

	dir_t	dir;

//	searchpath_t *search;
	char	netpath[MAX_OSPATH], *fullname;

	char	*path, *ext, *listbuff, *dirptr;

	intptr_t pathoffset         = stack[0]._int;
	intptr_t extoffset          = stack[1]._int;
	intptr_t listbuffoffset     = stack[2]._int;
	intptr_t buffsize           = stack[3]._int;
	intptr_t flags              = stack[4]._int;

	int numfiles = 0;
	int i, j;

	retval->_int = 0;

	if( ( listbuffoffset ) & (~mask))
		return;
	if( ( listbuffoffset + buffsize ) & (~mask))
		return;
	if( ( extoffset ) & (~mask))
		return;
	if( ( pathoffset ) & (~mask))
		return;

	memset(list, 0, sizeof(list));

	path = (char*)VM_POINTER(base,mask,pathoffset);
	ext  = (char*)VM_POINTER(base,mask,extoffset);;

	listbuff = (char*)VM_POINTER(base,mask,listbuffoffset);
	dirptr   = listbuff;
	*dirptr  = 0;

	if (strstr( path, ".." ) || strstr( path, "::" ))
		return; // do not allow relative paths

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

	retval->_int = numfiles;

	// free allocated mem
	for (i = 0; i < list_cnt; i++)
		Q_free(list[i]);
}

/*
  int trap_Map_Extension( const char* ext_name, int mapto)
  return:
    0 	success maping
   -1	not found
   -2	cannot map
*/
extern int pr2_numAPI;
void PF2_Map_Extension(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int mapto	= stack[1]._int;

	if( mapto <  pr2_numAPI)
	{

		retval->_int = -2;
		return;
	}

	retval->_int = -1;
}
////////////////////
//
// timewaster functions
//
////////////////////
void PF2_strcmp(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int=  strcmp( (char *) VM_POINTER(base,mask,stack[0].string),
	                       (char *) VM_POINTER(base,mask,stack[1].string));
}

void PF2_strncmp(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int=  strncmp( (char *) VM_POINTER(base,mask,stack[0].string),
	                        (char *) VM_POINTER(base,mask,stack[1].string),stack[2]._int);
}

void PF2_stricmp(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int=  strcasecmp( (char *) VM_POINTER(base,mask,stack[0].string),
	                           (char *) VM_POINTER(base,mask,stack[1].string));
}

void PF2_strnicmp(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	retval->_int=  strncasecmp( (char *) VM_POINTER(base,mask,stack[0].string),
	                            (char *) VM_POINTER(base,mask,stack[1].string),stack[2]._int);
}

void PF2_strlcpy(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{ // (char *dst, char *src, size_t siz)
	retval->_int = strlcpy( (char *) VM_POINTER(base,mask,stack[0].string), (char *) VM_POINTER(base,mask,stack[1].string), stack[2]._int );
}

void PF2_strlcat(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{ // (char *dst, char *src, size_t siz)
	retval->_int = strlcat( (char *) VM_POINTER(base,mask,stack[0].string), (char *) VM_POINTER(base,mask,stack[1].string), stack[2]._int );
}

/////////Bot Functions
extern cvar_t maxclients, maxspectators;
void PF2_Add_Bot( byte * base, uintptr_t mask, pr2val_t * stack, pr2val_t * retval )
{
	client_t *cl, *newcl = NULL;
	char   *name = (char *) VM_POINTER( base, mask, stack[0].string );
	int     bottomcolor = stack[1]._int;
	int     topcolor = stack[2]._int;
	char   *skin = (char *) VM_POINTER( base, mask, stack[3].string );
	int     edictnum;
	int     clients, spectators, i;
	extern char *shortinfotbl[];
	char   *s;
	edict_t *ent;
	eval_t *val;
	int old_self;
	char info[MAX_EXT_INFO_STRING];

	// count up the clients and spectators
	clients = 0;
	spectators = 0;
	for ( i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++ )
	{
		if ( cl->state == cs_free )
			continue;
		if ( cl->spectator )
			spectators++;
		else
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
		retval->_int = 0;
		return;
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
		retval->_int = 0;
		return;
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
	newcl->connection_started = realtime;
	strlcpy(newcl->name, name, sizeof(newcl->name));

	newcl->entgravity = 1.0;
	val = PR2_GetEdictFieldValue( ent, "gravity" );
	if ( val )
		val->_float = 1.0;
	sv_client->maxspeed = sv_maxspeed.value;
	val = PR2_GetEdictFieldValue( ent, "maxspeed" );
	if ( val )
		val->_float = sv_maxspeed.value;

	newcl->edict = ent;
	ent->v.colormap = edictnum;
	val = PR2_GetEdictFieldValue( ent, "isBot" );
	if( val )
		val->_int = 1;

	// restore client name.
	PR_SetEntityString(ent, ent->v.netname, newcl->name);

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
	retval->_int = edictnum;


	old_self = pr_global_struct->self;
	pr_global_struct->time = sv.time;
	pr_global_struct->self = EDICT_TO_PROG(newcl->edict);

	PR2_GameClientConnect(0);
	PR2_GamePutClientInServer(0);

	pr_global_struct->self = old_self;

}

void RemoveBot(client_t *cl)
{

	if( !cl->isBot )
		return;

	pr_global_struct->self = EDICT_TO_PROG(cl->edict);
	if ( sv_vm )
		PR2_GameClientDisconnect(0);

	cl->old_frags = 0;
	cl->edict->v.frags = 0.0;
	cl->name[0] = 0;
	cl->state = cs_free;

	Info_RemoveAll(&cl->_userinfo_ctx_);
	Info_RemoveAll(&cl->_userinfoshort_ctx_);

	SV_FullClientUpdate( cl, &sv.reliable_datagram );
	cl->isBot = 0;
}

void PF2_Remove_Bot( byte * base, uintptr_t mask, pr2val_t * stack, pr2val_t * retval )
{
	client_t *cl;
	int old_self;

	int     entnum = stack[0]._int;

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

void PF2_SetBotUserInfo( byte * base, uintptr_t mask, pr2val_t * stack, pr2val_t * retval )
{
	client_t *cl;
	int     entnum = stack[0]._int;
	char   *key = (char *) VM_POINTER( base, mask, stack[1].string );
	char   *value = (char *) VM_POINTER( base, mask, stack[2].string );
	int    flags = stack[3]._int;
	int     i;
	extern char *shortinfotbl[];

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

void PF2_SetBotCMD( byte * base, uintptr_t mask, pr2val_t * stack, pr2val_t * retval )
{
	client_t *cl;
	int     entnum = stack[0]._int;

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
	cl->botcmd.msec = stack[1]._int;
	cl->botcmd.angles[0] = stack[2]._float;
	cl->botcmd.angles[1] = stack[3]._float;
	cl->botcmd.angles[2] = stack[4]._float;
	cl->botcmd.forwardmove = stack[5]._int;
	cl->botcmd.sidemove = stack[6]._int;
	cl->botcmd.upmove = stack[7]._int;
	cl->botcmd.buttons = stack[8]._int;
	cl->botcmd.impulse = stack[9]._int;
	if ( cl->edict->v.fixangle)
	{
		VectorCopy(cl->edict->v.angles, cl->botcmd.angles);
		cl->botcmd.angles[PITCH] *= -3;
		cl->edict->v.fixangle = 0;
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
void PF2_QVMstrftime(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
//(char *valbuff, int sizebuff, char *fmt, int offset)
{
	char	*valbuff = (char *) VM_POINTER(base,mask,stack[0].string);
	int		sizebuff = stack[1]._int;
	char	*fmt = (char *) VM_POINTER(base,mask,stack[2].string);
	int		offset = stack[3]._int;

	struct tm *newtime;
	time_t long_time;

	retval->_int = 0;

	if (sizebuff <= 0 || !valbuff) {
		Con_DPrintf("PF2_QVMstrftime: wrong buffer\n");
		return;
	}

	time(&long_time);
	long_time += offset;
	newtime = localtime(&long_time);

	if (!newtime)
	{
		valbuff[0] = 0; // or may be better set to "#bad date#" ?
		return;
	}

	retval->_int = strftime(valbuff, sizebuff-1, fmt, newtime);

	if (!retval->_int) {
		valbuff[0] = 0; // or may be better set to "#bad date#" ?
		Con_DPrintf("PF2_QVMstrftime: buffer size too small\n");
		return;
	}
}

void PF2_makevectors(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	AngleVectors ((float *) VM_POINTER(base,mask,stack[0].string),
		pr_global_struct->v_forward,
		pr_global_struct->v_right,
		pr_global_struct->v_up);
}

// a la the ZQ_PAUSE QC extension
void PF2_setpause(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval)
{
	int pause;

	pause = stack[0]._int ? 1 : 0;

	if (pause != (sv.paused & 1))
		SV_TogglePause (NULL, 1);
}

void PF2_SetUserInfo( byte * base, uintptr_t mask, pr2val_t * stack, pr2val_t * retval )
{
	client_t *cl;
	int     entnum = stack[0]._int;
	char   *k = (char *) VM_POINTER( base, mask, stack[1].string );
	char   *v = (char *) VM_POINTER( base, mask, stack[2].string );
	char   key[MAX_KEY_STRING];
	char   value[MAX_KEY_STRING];
	int    flags = stack[3]._int;
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
		PF2_SetBotUserInfo( base, mask, stack, retval );
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

		if( PR2_UserInfoChanged() )
			return;
	}

	if ( flags & SETUSERINFO_STAR )
		Info_SetStar( &cl->_userinfo_ctx_, key, value );
	else
		Info_Set( &cl->_userinfo_ctx_, key, value );

	SV_ExtractFromUserinfo( cl, !strcmp( key, "name" ) );

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

//===========================================================================
// SysCalls
//===========================================================================

pr2_trapcall_t pr2_API[]=
    {
        PF2_GetApiVersion, 	//G_GETAPIVERSION
        PF2_DPrint,        	//G_DPRINT
        PF2_Error,         	//G_ERROR
        PF2_GetEntityToken,	//G_GetEntityToken,
        PF2_Spawn,		//G_SPAWN_ENT,
        PF2_Remove,		//G_REMOVE_ENT,
        PF2_precache_sound,	//G_PRECACHE_SOUND,
        PF2_precache_model,	//G_PRECACHE_MODEL,
        PF2_lightstyle,		//G_LIGHTSTYLE,
        PF2_setorigin,		//G_SETORIGIN,
        PF2_setsize,		//G_SETSIZE,
        PF2_setmodel,		//G_SETMODEL,
        PF2_bprint,		//G_BPRINT,
        PF2_sprint,		//G_SPRINT,
        PF2_centerprint,	//G_CENTERPRINT,
        PF2_ambientsound,	//G_AMBIENTSOUND,
        PF2_sound,		//G_SOUND,
        PF2_traceline,		//G_TRACELINE,
        PF2_checkclient,	//G_CHECKCLIENT,
        PF2_stuffcmd,		//G_STUFFCMD,
        PF2_localcmd,		//G_LOCALCMD,
        PF2_cvar,		//G_CVAR,
        PF2_cvar_set,		//G_CVAR_SET,
        PF2_FindRadius,		//G_FINDRADIUS
        PF2_walkmove,
        PF2_droptofloor,	//G_DROPTOFLOOR,
        PF2_checkbottom,	//G_CHECKBOTTOM,
        PF2_pointcontents,	//G_POINTCONTENTS,
        PF2_nextent,		//G_NEXTENT,
        PF2_fixme,		//G_AIM,
        PF2_makestatic,		//G_MAKESTATIC,
        PF2_setspawnparms,	//G_SETSPAWNPARAMS,
        PF2_changelevel,	//G_CHANGELEVEL,
        PF2_logfrag,		//G_LOGFRAG,
        PF2_infokey,		//G_GETINFOKEY,
        PF2_multicast,		//G_MULTICAST,
        PF2_disable_updates,	//G_DISABLEUPDATES,
        PF2_WriteByte,		//G_WRITEBYTE,
        PF2_WriteChar,		//G_WRITECHAR,
        PF2_WriteShort,		//G_WRITESHORT,
        PF2_WriteLong,		//G_WRITELONG,
        PF2_WriteAngle,		//G_WRITEANGLE,
        PF2_WriteCoord,		//G_WRITECOORD,
        PF2_WriteString,	//G_WRITESTRING,
        PF2_WriteEntity,	//G_WRITEENTITY,
        PR2_FlushSignon,	//G_FLUSHSIGNON,
        PF2_memset,		//g_memset,
        PF2_memcpy,		//g_memcpy,
        PF2_strncpy,	//g_strncpy,
        PF2_sin,		//g_sin,
        PF2_cos,		//g_cos,
        PF2_atan2,		//g_atan2,
        PF2_sqrt,		//g_sqrt,
        PF2_floor,		//g_floor,
        PF2_ceil,		//g_ceil,
        PF2_acos,		//g_acos,
        PF2_cmdargc,		//G_CMD_ARGC,
        PF2_cmdargv,		//G_CMD_ARGV
        PF2_TraceCapsule,
        PF2_FS_OpenFile,
        PF2_FS_CloseFile,
        PF2_FS_ReadFile,
        PF2_FS_WriteFile,
        PF2_FS_SeekFile,
        PF2_FS_TellFile,
        PF2_FS_GetFileList,
        PF2_cvar_set_float,
        PF2_cvar_string,
        PF2_Map_Extension,
        PF2_strcmp,
        PF2_strncmp,
        PF2_stricmp,
        PF2_strnicmp,
        PF2_Find,
        PF2_executecmd,
        PF2_conprint,
        PF2_readcmd,
        PF2_redirectcmd,
        PF2_Add_Bot,
        PF2_Remove_Bot,
        PF2_SetBotUserInfo,
        PF2_SetBotCMD,
		PF2_QVMstrftime,	//G_QVMstrftime
		PF2_cmdargs,		//G_CMD_ARGS
		PF2_tokenize,		//G_CMD_TOKENIZE
		PF2_strlcpy,		//g_strlcpy
		PF2_strlcat,		//g_strlcat
		PF2_makevectors,	//G_MAKEVECTORS
		PF2_nextclient,		//G_NEXTCLIENT
		PF2_precache_vwep_model,//G_PRECACHE_VWEP_MODEL
		PF2_setpause,		//G_SETPAUSE
		PF2_SetUserInfo,	//G_SETUSERINFO
		PF2_MoveToGoal,		//G_MOVETOGOAL
    };
int pr2_numAPI = sizeof(pr2_API)/sizeof(pr2_API[0]);

intptr_t sv_syscall(intptr_t arg, ...) //must passed ints
{
	intptr_t args[20];
	va_list argptr;
	pr2val_t ret;

	if( arg >= pr2_numAPI )
		PR2_RunError ("sv_syscall: Bad API call number");

	va_start(argptr, arg);
	args[0] =va_arg(argptr, intptr_t);
	args[1] =va_arg(argptr, intptr_t);
	args[2] =va_arg(argptr, intptr_t);
	args[3] =va_arg(argptr, intptr_t);
	args[4] =va_arg(argptr, intptr_t);
	args[5] =va_arg(argptr, intptr_t);
	args[6] =va_arg(argptr, intptr_t);
	args[7] =va_arg(argptr, intptr_t);
	args[8] =va_arg(argptr, intptr_t);
	args[9] =va_arg(argptr, intptr_t);
	args[10]=va_arg(argptr, intptr_t);
	args[11]=va_arg(argptr, intptr_t);
	args[12]=va_arg(argptr, intptr_t);
	args[13]=va_arg(argptr, intptr_t);
	args[14]=va_arg(argptr, intptr_t);
	args[15]=va_arg(argptr, intptr_t);
	args[16]=va_arg(argptr, intptr_t);
	args[17]=va_arg(argptr, intptr_t);
	args[18]=va_arg(argptr, intptr_t);
	args[19]=va_arg(argptr, intptr_t);
	va_end(argptr);

	pr2_API[arg] ( 0, (uintptr_t)~0, (pr2val_t*)args, &ret);

	return ret._int;
}

int sv_sys_callex(byte *data, unsigned int mask, int fn, pr2val_t*arg)
{
	pr2val_t ret;

	if( fn >= pr2_numAPI )
		PR2_RunError ("sv_sys_callex: Bad API call number");

	pr2_API[fn](data, mask, arg,&ret);
	return ret._int;
}

extern gameData_t *gamedata;
extern field_t *fields;

#define GAME_API_VERSION_MIN 8

void PR2_InitProg(void)
{
	extern cvar_t sv_pr2references;

	Cvar_SetValue(&sv_pr2references, 0.0f);

	if ( !sv_vm ) {
		PR1_InitProg();
		return;
	}

	PR2_FS_Restart();

	gamedata = (gameData_t *) VM_Call(sv_vm, GAME_INIT, (int) (sv.time * 1000),
	                                  (int) (Sys_DoubleTime() * 100000), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (!gamedata) {
		SV_Error("PR2_InitProg gamedata == NULL");
	}

	gamedata = (gameData_t *)PR2_GetString((intptr_t)gamedata);
	if (gamedata->APIversion < GAME_API_VERSION_MIN || gamedata->APIversion > GAME_API_VERSION) {
		if (GAME_API_VERSION_MIN == GAME_API_VERSION) {
			SV_Error("PR2_InitProg: Incorrect API version (%i should be %i)", gamedata->APIversion, GAME_API_VERSION);
		}
		else {
			SV_Error("PR2_InitProg: Incorrect API version (%i should be between %i and %i)", gamedata->APIversion, GAME_API_VERSION_MIN, GAME_API_VERSION);
		}
	}

	sv_vm->pr2_references = gamedata->APIversion >= 15 && (int)sv_pr2references.value;

	sv.edicts = (edict_t *)PR2_GetString((intptr_t)gamedata->ents);
	pr_global_struct = (globalvars_t*)PR2_GetString((intptr_t)gamedata->global);
	pr_globals = (float *) pr_global_struct;
	fields = (field_t*)PR2_GetString((intptr_t)gamedata->fields);
	pr_edict_size = gamedata->sizeofent;

	sv.max_edicts = MAX_EDICTS;
	if (gamedata->APIversion >= 14) {
		sv.max_edicts = min(sv.max_edicts, gamedata->maxentities);
	}
	else {
		sv.max_edicts = min(sv.max_edicts, 512);
	}
}
#endif /* USE_PR2 */
