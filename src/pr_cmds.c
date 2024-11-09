/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. 

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	
*/

#ifndef CLIENTONLY
#include "qwsvdef.h"

static tokenizecontext_t pr1_tokencontext;

#define	RETURN_EDICT(e) (((int *)pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))
#define	RETURN_STRING(s) (PR1_SetString(&((int *)pr_globals)[OFS_RETURN], s))

/*
===============================================================================
 
						BUILT-IN FUNCTIONS
 
===============================================================================
*/

char *PF_VarString (int	first)
{
	int		i;
	static char out[2048];

	out[0] = 0;
	for (i=first ; i<pr_argc ; i++)
	{
		strlcat (out, G_STRING((OFS_PARM0+i*3)), sizeof(out));
	}
	return out;
}


/*
=================
PF_errror
 
This is a TERMINAL error, which will kill off the entire server.
Dumps self.
 
error(value)
=================
*/
void PF_error (void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf ("======SERVER ERROR in %s:\n%s\n", PR1_GetString(pr_xfunction->s_name) ,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);

	SV_Error ("Program error (PF_error)");
}

/*
=================
PF_objerror
 
Dumps out self, then an error message.  The program is aborted and self is
removed, but the level can continue.
 
objerror(value)
=================
*/
void PF_objerror (void)
{
	char	*s;
	edict_t	*ed;

	s = PF_VarString(0);
	Con_Printf ("======OBJECT ERROR in %s:\n%s\n", PR1_GetString(pr_xfunction->s_name),s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
	ED_Free (ed);

	SV_Error ("Program error (PF_objerror)");
}



/*
==============
PF_makevectors
 
Writes new values for v_forward, v_up, and v_right based on angles
makevectors(vector)
==============
*/
void PF_makevectors (void)
{
	AngleVectors (G_VECTOR(OFS_PARM0), PR_GLOBAL(v_forward), PR_GLOBAL(v_right), PR_GLOBAL(v_up));
}

/*
=================
PF_setorigin
 
This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).  Directly changing origin will not set internal links correctly, so clipping would be messed up.  This should be called when an object is spawned, and then only if it is teleported.
 
setorigin (entity, origin)
=================
*/
void PF_setorigin (void)
{
	edict_t	*e;
	float	*org;

	e = G_EDICT(OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v->origin);
	SV_AntilagReset (e);
	SV_LinkEdict (e, false);
}


/*
=================
PF_setsize
 
the size box is rotated by the current angle
 
setsize (entity, minvector, maxvector)
=================
*/
void PF_setsize (void)
{
	edict_t	*e;
	float	*min, *max;

	e = G_EDICT(OFS_PARM0);
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	VectorCopy (min, e->v->mins);
	VectorCopy (max, e->v->maxs);
	VectorSubtract (max, min, e->v->size);
	SV_LinkEdict (e, false);
}


/*
=================
PF_setmodel
 
setmodel(entity, model)
Also sets size, mins, and maxs for inline bmodels
=================
*/
static void PF_setmodel (void)
{
	int			i;
	edict_t		*e;
	char		*m, **check;
	cmodel_t	*mod;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);

// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; i < MAX_MODELS && *check ; i++, check++)
		if (!strcmp(*check, m))
			goto ok;
	PR_RunError ("PF_setmodel: no precache: %s\n", m);
ok:

	e->v->model = G_INT(OFS_PARM1);
	e->v->modelindex = i;

// if it is an inline model, get the size information for it
	if (m[0] == '*')
	{
		mod = CM_InlineModel (m);
		VectorCopy (mod->mins, e->v->mins);
		VectorCopy (mod->maxs, e->v->maxs);
		VectorSubtract (mod->maxs, mod->mins, e->v->size);
		SV_LinkEdict (e, false);
	}
	else if (pr_nqprogs)
	{
		// hacks to make NQ progs happy
		if (!strcmp(PR1_GetString(e->v->model), "maps/b_explob.bsp"))
		{
			VectorClear (e->v->mins);
			VectorSet (e->v->maxs, 32, 32, 64);
		}
		else
		{
			// FTE does this, so we do, too; I'm not sure if it makes a difference
			VectorSet (e->v->mins, -16, -16, -16);
			VectorSet (e->v->maxs, 16, 16, 16);
		}
		VectorSubtract (e->v->maxs, e->v->mins, e->v->size);
		SV_LinkEdict (e, false);
	}
}

/*
=================
PF_bprint
 
broadcast print to everyone on server
 
bprint(value)
=================
*/
void PF_bprint (void)
{
	char		*s;
	int			level;

	if (pr_nqprogs)
	{
		level = PRINT_HIGH;
		s = PF_VarString(0);
	}
	else
	{
		level = G_FLOAT(OFS_PARM0);
		s = PF_VarString(1);
	}

	SV_BroadcastPrintf (level, "%s", s);
}

#define SPECPRINT_CENTERPRINT	0x1
#define SPECPRINT_SPRINT	0x2
#define SPECPRINT_STUFFCMD	0x4
/*
=================
PF_sprint
 
single print to a specific client
 
sprint(clientent, value)
=================
*/
void PF_sprint (void)
{
	char		*s;
	client_t	*client, *cl;
	int			entnum;
	int			level;
	int			i;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (pr_nqprogs)
	{
		level = PRINT_HIGH;
		s = PF_VarString(1);
	}
	else
	{
		level = G_FLOAT(OFS_PARM1);
		s = PF_VarString(2);
	}

	if (entnum < 1 || entnum > MAX_CLIENTS)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	client = &svs.clients[entnum-1];

	SV_ClientPrintf (client, level, "%s", s);

	//bliP: spectator print ->
	if ((int)sv_specprint.value & SPECPRINT_SPRINT)
	{
		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		{
			if (!cl->state || !cl->spectator)
				continue;

			if ((cl->spec_track == entnum) && (cl->spec_print & SPECPRINT_SPRINT))
				SV_ClientPrintf (cl, level, "%s", s);
		}
	}
	//<-
}



/*
=================
PF_centerprint
 
single print to a specific client
 
centerprint(clientent, value)
=================
*/
void PF_centerprint (void)
{
	char		*s;
	int			entnum;
	client_t	*cl, *spec;
	int			i;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);

	if (entnum < 1 || entnum > MAX_CLIENTS)
	{
		Con_Printf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum-1];

	ClientReliableWrite_Begin (cl, svc_centerprint, 2 + strlen(s));
	ClientReliableWrite_String (cl, s);

	if (sv.mvdrecording)
	{
		if (MVDWrite_Begin (dem_single, entnum - 1, 2 + strlen(s)))
		{
			MVD_MSG_WriteByte (svc_centerprint);
			MVD_MSG_WriteString (s);
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
PF_normalize
 
vector normalize(vector)
=================
*/
void PF_normalize (void)
{
	float	*value1;
	vec3_t	newvalue;
	float	nuw;

	value1 = G_VECTOR(OFS_PARM0);

	nuw = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	nuw = sqrt(nuw);

	if (nuw == 0)
		newvalue[0] = newvalue[1] = newvalue[2] = 0;
	else
	{
		nuw = 1/nuw;
		newvalue[0] = value1[0] * nuw;
		newvalue[1] = value1[1] * nuw;
		newvalue[2] = value1[2] * nuw;
	}

	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_vlen
 
scalar vlen(vector)
=================
*/
void PF_vlen (void)
{
	float	*value1;
	float	nuw;

	value1 = G_VECTOR(OFS_PARM0);

	nuw = value1[0] * value1[0] + value1[1] * value1[1] + value1[2]*value1[2];
	nuw = sqrt(nuw);

	G_FLOAT(OFS_RETURN) = nuw;
}

/*
=================
PF_vectoyaw
 
float vectoyaw(vector)
=================
*/
void PF_vectoyaw (void)
{
	float	*value1;
	float	yaw;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
		yaw = 0;
	else
	{
		yaw = /*(int)*/ (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}


/*
=================
PF_vectoangles
 
vector vectoangles(vector)
=================
*/
void PF_vectoangles (void)
{
	float	*value1;
	float	forward;
	float	yaw, pitch;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0)
	{
		yaw = 0;
		if (value1[2] > 0)
			pitch = 90;
		else
			pitch = 270;
	}
	else
	{
		yaw = /*(int)*/ (atan2(value1[1], value1[0]) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0]*value1[0] + value1[1]*value1[1]);
		pitch = /*(int)*/ (atan2(value1[2], forward) * 180 / M_PI);
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(OFS_RETURN+0) = pitch;
	G_FLOAT(OFS_RETURN+1) = yaw;
	G_FLOAT(OFS_RETURN+2) = 0;
}

/*
=================
PF_Random
 
Returns a number from 0<= num < 1
 
random()
=================
*/
void PF_random (void)
{
	float		num;

	num = (rand ()&0x7fff) / ((float)0x7fff);

	G_FLOAT(OFS_RETURN) = num;
}


/*
=================
PF_particle

particle(origin, dir, color, count [,replacement_te [,replacement_count]]) = #48
For NQ progs (or as a QW extension)
=================
*/
static void PF_particle (void)
{
	float	*org, *dir;
	float	color, count;
	int		replacement_te;
	int		replacement_count = 0 /* suppress compiler warning */;
			
	org = G_VECTOR(OFS_PARM0);
	dir = G_VECTOR(OFS_PARM1);
	color = G_FLOAT(OFS_PARM2);
	count = G_FLOAT(OFS_PARM3);

	// Progs should provide a tempentity code and particle count for the case
	// when a client doesn't support svc_particle
	if (pr_argc >= 5)
	{
		replacement_te = G_FLOAT(OFS_PARM4);
		replacement_count = (pr_argc >= 6) ? G_FLOAT(OFS_PARM5) : 1;
	}
	else
	{
		// To aid porting of NQ mods, if the extra arguments are not provided, try
		// to figure out what progs want by inspecting color and count
		if (count == 255)
		{
			replacement_te = TE_EXPLOSION;		// count is not used
		}
		else if (color == 73)
		{
			replacement_te = TE_BLOOD;
			replacement_count = 1;	// FIXME: use count / <some value>?
		}
		else if (color == 225)
		{
			replacement_te = TE_LIGHTNINGBLOOD;	// count is not used
		}
		else
		{
			replacement_te = 0;		// don't send anything
		}
	}

	SV_StartParticle (org, dir, color, count, replacement_te, replacement_count);
}

/*
=================
PF_ambientsound
 
=================
*/
void PF_ambientsound (void)
{
	char		**check;
	char		*samp;
	float		*pos;
	float 		vol, attenuation;
	int			i, soundnum;

	pos = G_VECTOR (OFS_PARM0);
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

	// check to see if samp was properly precached
	for (soundnum=0, check = sv.sound_precache ; *check ; check++, soundnum++)
		if (!strcmp(*check,samp))
			break;

	if (!*check)
	{
		Con_Printf ("no precache: %s\n", samp);
		return;
	}

	// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon,svc_spawnstaticsound);
	for (i=0 ; i<3 ; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol*255);
	MSG_WriteByte (&sv.signon, attenuation*64);

}

/*
=================
PF_sound
 
Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.
 
Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.
 
An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.
 
=================
*/
void PF_sound (void)
{
	char		*sample;
	int			channel;
	edict_t		*entity;
	int 		volume;
	float attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);

	SV_StartSound (entity, channel, sample, volume, attenuation);
}

/*
=================
PF_break
 
break()
=================
*/
void PF_break (void)
{
	Con_Printf ("break statement\n");
	*(int *)-4 = 0;	// dump to debugger
	//	PR_RunError ("break statement");
}

/*
=================
PF_traceline
 
Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.
 
traceline (vector1, vector2, tryents)
=================
*/
void PF_traceline (void)
{
	float	*v1, *v2;
	trace_t	trace;
	int		nomonsters;
	edict_t	*ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	if (sv_antilag.value == 2)
		nomonsters |= MOVE_LAGGED;

	trace = SV_Trace (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	PR_GLOBAL(trace_allsolid) = trace.allsolid;
	PR_GLOBAL(trace_startsolid) = trace.startsolid;
	PR_GLOBAL(trace_fraction) = trace.fraction;
	PR_GLOBAL(trace_inwater) = trace.inwater;
	PR_GLOBAL(trace_inopen) = trace.inopen;
	VectorCopy (trace.endpos, PR_GLOBAL(trace_endpos));
	VectorCopy (trace.plane.normal, PR_GLOBAL(trace_plane_normal));
	PR_GLOBAL(trace_plane_dist) =  trace.plane.dist;	
	if (trace.e.ent)
		PR_GLOBAL(trace_ent) = EDICT_TO_PROG(trace.e.ent);
	else
		PR_GLOBAL(trace_ent) = EDICT_TO_PROG(sv.edicts);
}

/*
=================
PF_checkpos
 
Returns true if the given entity can move to the given position from it's
current position by walking or rolling.
FIXME: make work...
scalar checkpos (entity, vector)
=================
*/
void PF_checkpos (void)
{}

//============================================================================

// Unlike Quake's Mod_LeafPVS, CM_LeafPVS returns a pointer to static data
// uncompressed at load time, so it's safe to store for future use
static byte	*checkpvs;

int PF_newcheckclient (int check)
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

	for ( ;  ; i++)
	{
		if (i == MAX_CLIENTS+1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->e.free)
			continue;
		if (ent->v->health <= 0)
			continue;
		if ((int)ent->v->flags & FL_NOTARGET)
			continue;

	// anything that is a client, or has a client as an enemy
		break;
	}

// get the PVS for the entity
	VectorAdd (ent->v->origin, ent->v->view_ofs, org);
	checkpvs = CM_LeafPVS (CM_PointInLeaf(org));

	return i;
}


/*
=================
PF_checkclient

Returns a client (or object that has a client enemy) that would be a
valid target.

If there are more than one valid options, they are cycled each frame

If (self.origin + self.viewofs) is not in the PVS of the current target,
it is not returned at all.

entity checkclient() = #17
=================
*/
#define	MAX_CHECK	16
static void PF_checkclient (void)
{
	edict_t	*ent, *self;
	int		l;
	vec3_t	vieworg;
	
// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1)
	{
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

// return check if it might be visible	
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->e.free || ent->v->health <= 0)
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd (self->v->origin, self->v->view_ofs, vieworg);
	l = CM_Leafnum(CM_PointInLeaf(vieworg)) - 1;
	if ( (l<0) || !(checkpvs[l>>3] & (1<<(l&7)) ) )
	{
		RETURN_EDICT(sv.edicts);
		return;
	}

// might be able to see it
	RETURN_EDICT(ent);
}

//============================================================================


/*
=================
PF_stuffcmd
 
Sends text over to the client's execution buffer
 
stuffcmd (clientent, value)
=================
*/
void PF_stuffcmd (void)
{
	int		entnum;
	char	*str;
	client_t	*cl, *spec;
	char	*buf;
	int j;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError ("Parm 0 not a client");
	str = G_STRING(OFS_PARM1);

	cl = &svs.clients[entnum-1];

	if (!strncmp(str, "disconnect\n", MAX_STUFFTEXT))
	{
		// so long and thanks for all the fish
		cl->drop = true;
		return;
	}

	buf = cl->stufftext_buf;
	if (strlen(buf) + strlen(str) >= MAX_STUFFTEXT)
		PR_RunError ("stufftext buffer overflow");
	strlcat (buf, str, MAX_STUFFTEXT);

	if( strchr( buf, '\n' ) )
	{
		ClientReliableWrite_Begin (cl, svc_stufftext, 2+strlen(buf));
		ClientReliableWrite_String (cl, buf);
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin ( dem_single, cl - svs.clients, 2+strlen(buf)))
			{
				MVD_MSG_WriteByte (svc_stufftext);
				MVD_MSG_WriteString (buf);
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
		//<-
	
		buf[0] = 0;
	}
}

/*
=================
PF_localcmd
 
Sends text over to the client's execution buffer
 
localcmd (string)
=================
*/
void PF_localcmd (void)
{
	char	*str;

	str = G_STRING(OFS_PARM0);

	if (pr_nqprogs && !strcmp(str, "restart\n"))
	{
		Cbuf_AddText (va("map %s\n", sv.mapname));
		return;
	}

	Cbuf_AddText (str);
}

#define MAX_PR_STRING_SIZE	2048
#define MAX_PR_STRINGS		64

int		pr_string_index = 0;
char	pr_string_buf[MAX_PR_STRINGS][MAX_PR_STRING_SIZE];
char	*pr_string_temp = pr_string_buf[0];

void PF_SetTempString(void)
{
	pr_string_temp = pr_string_buf[(++pr_string_index) & (MAX_PR_STRINGS - 1)];
}


/*
=================
PF_tokenize
 
float tokenize(string)
=================
*/

void PF_tokenize (void)
{
	const char *str;

	str = G_STRING(OFS_PARM0);
	Cmd_TokenizeStringEx(&pr1_tokencontext, str);
	G_FLOAT(OFS_RETURN) = Cmd_ArgcEx(&pr1_tokencontext);
}

/*
=================
PF_argc
 
returns number of tokens (must be executed after PF_Tokanize!)
 
float argc(void)
=================
*/

void PF_argc (void)
{
	G_FLOAT(OFS_RETURN) = (float) Cmd_ArgcEx(&pr1_tokencontext);
}

/*
=================
PF_argv
 
returns token requested by user (must be executed after PF_Tokanize!)
 
string argc(float)
=================
*/

void PF_argv (void)
{
	int num;

	num = (int) G_FLOAT(OFS_PARM0);

//	if (num < 0 ) num = 0;
//	if (num > Cmd_Argc()-1) num = Cmd_Argc()-1;

	if (num < 0 || num >= Cmd_ArgcEx(&pr1_tokencontext)) {
		RETURN_STRING("");
	}
	else {
		snprintf(pr_string_temp, MAX_PR_STRING_SIZE, "%s", Cmd_ArgvEx(&pr1_tokencontext, num));
		RETURN_STRING(pr_string_temp);
		PF_SetTempString();
	}
}

/*
=================
PF_substr
 
string substr(string str, float start, float len)
=================
*/

void PF_substr (void)
{
	char *s;
	int start, len, l;

	s = G_STRING(OFS_PARM0);
	start = (int) G_FLOAT(OFS_PARM1);
	len = (int) G_FLOAT(OFS_PARM2);
	l = strlen(s);

	if (start >= l || !len || !*s)
	{
		PR_SetTmpString(&G_INT(OFS_RETURN), "");
		return;
	}

	s += start;
	l -= start;

	if (len > l + 1)
		len = l + 1;

	strlcpy(pr_string_temp, s, len + 1);

	PR1_SetString(&G_INT(OFS_RETURN), pr_string_temp);

	PF_SetTempString();
}

/*
=================
PF_strcat
 
string strcat(string str1, string str2)
=================
*/

void PF_strcat (void)
{
	strlcpy(pr_string_temp, PF_VarString(0), MAX_PR_STRING_SIZE);
	PR1_SetString(&G_INT(OFS_RETURN), pr_string_temp);

	PF_SetTempString();
}

/*
=================
PF_strlen
 
float strlen(string str)
=================
*/

void PF_strlen (void)
{
	G_FLOAT(OFS_RETURN) = (float) strlen(G_STRING(OFS_PARM0));
}

/*
=================
PF_strzone

ZQ_QC_STRINGS
string newstr (string str [, float size])

The 'size' parameter is not QSG but an MVDSV extension
=================
*/

void PF_strzone (void)
{
	char *s;
	int i, size;

	s = G_STRING(OFS_PARM0);

	for (i = 0; i < MAX_PRSTR; i++)
	{
		if (!pr_newstrtbl[i] || pr_newstrtbl[i] == pr_strings)
			break;
	}

	if (i == MAX_PRSTR)
		PR_RunError("strzone: out of string memory");

	size = strlen(s) + 1;
	if (pr_argc == 2 && (int) G_FLOAT(OFS_PARM1) > size)
		size = (int) G_FLOAT(OFS_PARM1);

	pr_newstrtbl[i] = (char *) Q_malloc(size);
	strlcpy(pr_newstrtbl[i], s, size);

	G_INT(OFS_RETURN) = -(i+MAX_PRSTR);
}

/*
=================
PF_strunzone

ZQ_QC_STRINGS
void strunzone (string str)
=================
*/

void PF_strunzone (void)
{
	int num;

	num = G_INT(OFS_PARM0);
	if (num > - MAX_PRSTR)
		PR_RunError("strunzone: not a dynamic string");

	if (num <= -(MAX_PRSTR * 2))
		PR_RunError ("strunzone: bad string");

	num = - (num + MAX_PRSTR);

	if (pr_newstrtbl[num] == pr_strings)
		return;	// allow multiple strunzone on the same string (like free in C)

	Q_free(pr_newstrtbl[num]);
	pr_newstrtbl[num] = pr_strings;
}

void PF_clear_strtbl(void)
{
	int i;

	for (i = 0; i < MAX_PRSTR; i++)
	{
		if (pr_newstrtbl[i] && pr_newstrtbl[i] != pr_strings)
		{
			Q_free(pr_newstrtbl[i]);
			pr_newstrtbl[i] = NULL;
		}
	}
}

//FTE_CALLTIMEOFDAY
void PF_calltimeofday (void)
{
	extern func_t ED_FindFunctionOffset (char *name);
	func_t f;

	if ((f = ED_FindFunctionOffset ("timeofday")))
	{
		date_t date;

		SV_TimeOfDay(&date, "%a %b %d, %H:%M:%S %Y");

		G_FLOAT(OFS_PARM0) = (float)date.sec;
		G_FLOAT(OFS_PARM1) = (float)date.min;
		G_FLOAT(OFS_PARM2) = (float)date.hour;
		G_FLOAT(OFS_PARM3) = (float)date.day;
		G_FLOAT(OFS_PARM4) = (float)date.mon;
		G_FLOAT(OFS_PARM5) = (float)date.year;
		PR_SetTmpString(&G_INT(OFS_PARM6), date.str);

		PR_ExecuteProgram(f);
	}
}

/*
=================
PF_cvar

float cvar (string)
=================
*/
void PF_cvar (void)
{
	char	*str;

	str = G_STRING(OFS_PARM0);

	if (!strcasecmp(str, "pr_checkextension")) {
		// we do support PF_checkextension
		G_FLOAT(OFS_RETURN) = 1.0;
		return;
	}

	if (pr_nqprogs && !pr_globals[35]/* deathmatch */
		&& (!strcmp(str, "timelimit") || !strcmp(str, "samelevel"))
	)
	{
		// workaround for NQ progs bug: timelimit and samelevel are checked in SP/coop
		G_FLOAT(OFS_RETURN) = 0.0;
		return;
	}

	G_FLOAT(OFS_RETURN) = Cvar_Value (str);
}

/*
=================
PF_cvar_set
 
float cvar (string)
=================
*/
void PF_cvar_set (void)
{
	char	*var_name, *val;
	cvar_t	*var;

	var_name = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	var = Cvar_Find(var_name);
	if (!var)
	{
		Con_Printf ("PF_cvar_set: variable %s not found\n", var_name);
		return;
	}

	Cvar_Set (var, val);
}

/*
=================
PF_findradius
 
Returns a chain of entities that have origins within a spherical area
 
findradius (origin, radius)
=================
*/
static void PF_findradius (void)
{
	int			i, j, numtouch;
	edict_t		*touchlist[MAX_EDICTS], *ent, *chain;
	float		rad, rad_2, *org;
	vec3_t		mins, maxs, eorg;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);
	rad_2 = rad * rad;

	for (i = 0; i < 3; i++)
	{
		mins[i] = org[i] - rad - 1;		// enlarge the bbox a bit
		maxs[i] = org[i] + rad + 1;
	}

	numtouch = SV_AreaEdicts (mins, maxs, touchlist, sv.max_edicts, AREA_SOLID);
	numtouch += SV_AreaEdicts (mins, maxs, &touchlist[numtouch], sv.max_edicts - numtouch, AREA_TRIGGERS);

	chain = (edict_t *)sv.edicts;

// touch linked edicts
	for (i = 0; i < numtouch; i++)
	{
		ent = touchlist[i];
		if (ent->v->solid == SOLID_NOT)
			continue;	// FIXME?

		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (ent->v->origin[j] + (ent->v->mins[j] + ent->v->maxs[j]) * 0.5);
		if (DotProduct(eorg, eorg) > rad_2)
			continue;

		ent->v->chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}


/*
=========
PF_dprint
=========
*/
void PF_dprint (void)
{
	Con_Printf ("%s",PF_VarString(0));
}

void PF_ftos (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);

	if (v == (int)v)
		snprintf (pr_string_temp, MAX_PR_STRING_SIZE, "%d",(int)v);
	else
		snprintf (pr_string_temp, MAX_PR_STRING_SIZE, "%5.1f",v);
	PR1_SetString(&G_INT(OFS_RETURN), pr_string_temp);
	PF_SetTempString();
}

void PF_fabs (void)
{
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

void PF_vtos (void)
{
	snprintf (pr_string_temp, MAX_PR_STRING_SIZE, "'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	PR1_SetString(&G_INT(OFS_RETURN), pr_string_temp);

	PF_SetTempString();
}

void PF_Spawn (void)
{
	edict_t	*ed;
	ed = ED_Alloc();
	RETURN_EDICT(ed);
}

void PF_Remove (void)
{
	edict_t	*ed;

	ed = G_EDICT(OFS_PARM0);
	ED_Free (ed);
}


// entity (entity start, .string field, string match) find = #5;
void PF_Find (void)
{
	int		e;
	int		f;
	char	*s, *t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);
	if (!s)
		PR_RunError ("PF_Find: bad search string");

	for (e++ ; e < sv.num_edicts ; e++)
	{
		ed = EDICT_NUM(e);
		if (ed->e.free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s))
		{
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

void PR_CheckEmptyString (char *s)
{
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}

void PF_precache_file (void)
{	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

void PF_precache_sound (void)
{
	char	*s;
	int		i;

	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i=0 ; i<MAX_SOUNDS ; i++)
	{
		if (!sv.sound_precache[i])
		{
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_sound: overflow");
}

void PF_precache_model (void)
{
	char	*s;
	int		i;

	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i=0 ; i<MAX_MODELS ; i++)
	{
		if (!sv.model_precache[i])
		{
			sv.model_precache[i] = s;
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_model: overflow");
}

static void PF_precache_vwep_model (void)
{
	char	*s;
	int		i;
	
	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");
		
	s = G_STRING(OFS_PARM0);
	PR_CheckEmptyString (s);

	// the strings are transferred via the stufftext mechanism, hence the stringency
	if (strchr(s, '"') || strchr(s, ';') || strchr(s, '\n'  ) || strchr(s, '\t') || strchr(s, ' '))
		PR_RunError ("Bad string\n");

	for (i = 0; i < MAX_VWEP_MODELS; i++)
	{
		if (!sv.vw_model_name[i]) {
			sv.vw_model_name[i] = s;
			G_INT(OFS_RETURN) = i;
			return;
		}
	}
	PR_RunError ("PF_precache_vwep_model: overflow");
}


void PF_coredump (void)
{
	ED_PrintEdicts ();
}

void PF_traceon (void)
{
	pr_trace = true;
}

void PF_traceoff (void)
{
	pr_trace = false;
}

void PF_eprint (void)
{
	ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

/*
===============
PF_walkmove
 
float(float yaw, float dist) walkmove
===============
*/
void PF_walkmove (void)
{
	edict_t	*ent;
	float	yaw, dist;
	vec3_t	move;
	dfunction_t	*oldf;
	int 	oldself;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);

	if ( !( (int)ent->v->flags & (FL_ONGROUND|FL_FLY|FL_SWIM) ) )
	{
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw*M_PI*2 / 360;

	move[0] = cos(yaw)*dist;
	move[1] = sin(yaw)*dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);


	// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

/*
===============
PF_droptofloor
 
void() droptofloor
===============
*/
void PF_droptofloor (void)
{
	edict_t		*ent;
	vec3_t		end;
	trace_t		trace;

	ent = PROG_TO_EDICT(pr_global_struct->self);

	VectorCopy (ent->v->origin, end);
	end[2] -= 256;

	trace = SV_Trace (ent->v->origin, ent->v->mins, ent->v->maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid)
		G_FLOAT(OFS_RETURN) = 0;
	else
	{
		VectorCopy (trace.endpos, ent->v->origin);
		SV_LinkEdict (ent, false);
		ent->v->flags = (int)ent->v->flags | FL_ONGROUND;
		ent->v->groundentity = EDICT_TO_PROG(trace.e.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

/*
===============
PF_lightstyle
 
void(float style, string value) lightstyle
===============
*/
void PF_lightstyle (void)
{
	int		style;
	char	*val;
	client_t	*client;
	int			j;

	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	// change the string in sv
	sv.lightstyles[style] = val;

	// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j=0, client = svs.clients ; j<MAX_CLIENTS ; j++, client++)
		if ( client->state == cs_spawned )
		{
			ClientReliableWrite_Begin (client, svc_lightstyle, strlen(val)+3);
			ClientReliableWrite_Char (client, style);
			ClientReliableWrite_String (client, val);
		}
	if (sv.mvdrecording)
	{
		if (MVDWrite_Begin( dem_all, 0, strlen(val)+3))
		{
			MVD_MSG_WriteByte(svc_lightstyle);
			MVD_MSG_WriteChar(style);
			MVD_MSG_WriteString(val);
		}
	}
}

void PF_rint (void)
{
	float	f;
	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int)(f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int)(f - 0.5);
}
void PF_floor (void)
{
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}
void PF_ceil (void)
{
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}


/*
=============
PF_checkbottom
=============
*/
void PF_checkbottom (void)
{
	edict_t	*ent;

	ent = G_EDICT(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_CheckBottom (ent);
}

/*
=============
PF_pointcontents
=============
*/
void PF_pointcontents (void)
{
	float	*v;

	v = G_VECTOR(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_PointContents (v);
}

/*
=============
PF_nextent
 
entity nextent(entity)
=============
*/
void PF_nextent (void)
{
	int		i;
	edict_t	*ent;

	i = G_EDICTNUM(OFS_PARM0);
	while (1)
	{
		i++;
		if (i == sv.num_edicts)
		{
			RETURN_EDICT(sv.edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->e.free)
		{
			RETURN_EDICT(ent);
			return;
		}
	}
}

/*
=============
PF_aim
 
Pick a vector for the player to shoot along
vector aim(entity, missilespeed)
=============
*/
void PF_aim (void)
{
	VectorCopy (PR_GLOBAL(v_forward), G_VECTOR(OFS_RETURN));
}

/*
==============
PF_changeyaw
 
This was a major timewaster in progs, so it was converted to C
==============
*/
void PF_changeyaw (void)
{
	edict_t		*ent;
	float		ideal, current, move, speed;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	current = anglemod( ent->v->angles[1] );
	ideal = ent->v->ideal_yaw;
	speed = ent->v->yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current)
	{
		if (move >= 180)
			move = move - 360;
	}
	else
	{
		if (move <= -180)
			move = move + 360;
	}
	if (move > 0)
	{
		if (move > speed)
			move = speed;
	}
	else
	{
		if (move < -speed)
			move = -speed;
	}

	ent->v->angles[1] = anglemod (current + move);
}

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

sizebuf_t *WriteDest (void)
{
	int		dest;
	//	int		entnum;
	//	edict_t	*ent;

	dest = G_FLOAT(OFS_PARM0);
	switch (dest)
	{
	case MSG_BROADCAST:
		return &sv.datagram;

	case MSG_ONE:
		SV_Error("Shouldn't be at MSG_ONE");
#if 0
		ent = PROG_TO_EDICT(PR_GLOBAL(msg_entity));
		entnum = NUM_FOR_EDICT(ent);
		if (entnum < 1 || entnum > MAX_CLIENTS)
			PR_RunError ("WriteDest: not a client");
		return &svs.clients[entnum-1].netchan.message;
#endif

	case MSG_ALL:
		return &sv.reliable_datagram;

	case MSG_INIT:
		if (sv.state != ss_loading)
			PR_RunError ("PF_Write_*: MSG_INIT can only be written in spawn functions");
		return &sv.signon;

	case MSG_MULTICAST:
		return &sv.multicast;

	default:
		PR_RunError ("WriteDest: bad destination");
		break;
	}

	return NULL;
}

static client_t *Write_GetClient(void)
{
	int		entnum;
	edict_t	*ent;

	ent = PROG_TO_EDICT(PR_GLOBAL(msg_entity));
	entnum = NUM_FOR_EDICT(ent);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError ("WriteDest: not a client");
	return &svs.clients[entnum-1];
}


#ifdef WITH_NQPROGS
static byte nqp_buf_data[1024] /* must be large enough for svc_finale text */;
static sizebuf_t nqp_buf;
static qbool nqp_ignore_this_frame;
static int nqp_expect;

void NQP_Reset (void)
{
	nqp_ignore_this_frame = false;
	nqp_expect = 0;
	SZ_Init (&nqp_buf, nqp_buf_data, sizeof(nqp_buf_data));
}

static void NQP_Flush (int count)
{
// FIXME, we make no distinction reliable or not
	assert (count <= nqp_buf.cursize);
	SZ_Write (&sv.reliable_datagram, nqp_buf_data, count);
	memmove (nqp_buf_data, nqp_buf_data + count, nqp_buf.cursize - count);
	nqp_buf.cursize -= count;
}

static void NQP_Skip (int count)
{
	assert (count <= nqp_buf.cursize);
	memmove (nqp_buf_data, nqp_buf_data + count, nqp_buf.cursize - count);
	nqp_buf.cursize -= count;
}

static void NQP_Process (void)
{
	int cmd;

	if (nqp_ignore_this_frame) {
		SZ_Clear (&nqp_buf);
		return;
	}

	while (1) {
		if (nqp_expect) {
			if (nqp_buf.cursize >= nqp_expect) {
				NQP_Flush (nqp_expect);
				nqp_expect = 0;
			}
			else
				break;
		}

		if (!nqp_buf.cursize)
			break;

		nqp_expect = 0;

		cmd = nqp_buf_data[0];
		if (cmd == svc_killedmonster || cmd == svc_foundsecret || cmd == svc_sellscreen)
			nqp_expect = 1;
		else if (cmd == svc_updatestat) {
			NQP_Skip (1);
			MSG_WriteByte (&sv.reliable_datagram, svc_updatestatlong);
			nqp_expect = 5;
		}
		else if (cmd == svc_print) {
			byte *p = (byte *)memchr (nqp_buf_data + 1, 0, nqp_buf.cursize - 1);
			if (!p)
				goto waitformore;
			MSG_WriteByte(&sv.reliable_datagram, svc_print);
			MSG_WriteByte(&sv.reliable_datagram, PRINT_HIGH);
			MSG_WriteString(&sv.reliable_datagram, (char *)(nqp_buf_data + 1));
			NQP_Skip(p - nqp_buf_data + 1);
		}
		else if (cmd == nq_svc_setview) {
			if (nqp_buf.cursize < 3)
				goto waitformore;
			NQP_Skip (3);		// TODO: make an extension for this
		}
		else if (cmd == svc_updatefrags)
			nqp_expect = 4;
		else if (cmd == nq_svc_updatecolors) {
			if (nqp_buf.cursize < 3)
				goto waitformore;
			MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
			MSG_WriteByte (&sv.reliable_datagram, nqp_buf_data[1]);
			MSG_WriteString (&sv.reliable_datagram, "topcolor");
			MSG_WriteString (&sv.reliable_datagram, va("%i", min(nqp_buf_data[2] & 15, 13)));
			MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
			MSG_WriteByte (&sv.reliable_datagram, nqp_buf_data[1]);
			MSG_WriteString (&sv.reliable_datagram, "bottomcolor");
			MSG_WriteString (&sv.reliable_datagram, va("%i", min((nqp_buf_data[2] >> 4)& 15, 13)));
			NQP_Skip (3);
		}
		else if (cmd == nq_svc_updatename) {
			int slot;
			byte *p;
			if (nqp_buf.cursize < 3)
				goto waitformore;
			slot = nqp_buf_data[1];
			p = (byte *)memchr (nqp_buf_data + 2, 0, nqp_buf.cursize - 2);
			if (!p)
				goto waitformore;
			MSG_WriteByte (&sv.reliable_datagram, svc_setinfo);
			MSG_WriteByte (&sv.reliable_datagram, slot);
			MSG_WriteString (&sv.reliable_datagram, "name");
			MSG_WriteString (&sv.reliable_datagram, (char *)nqp_buf_data + 2);
			MSG_WriteByte (&sv.reliable_datagram, svc_updateping);
			MSG_WriteByte (&sv.reliable_datagram, slot);
			MSG_WriteShort (&sv.reliable_datagram, 0);
			// We expect bots to set their name when they enter the game and never change it
			MSG_WriteByte (&sv.reliable_datagram, svc_updateentertime);
			MSG_WriteByte (&sv.reliable_datagram, slot);
			MSG_WriteFloat (&sv.reliable_datagram, 0);
			NQP_Skip ((p - nqp_buf_data) + 1);
		}
		else if (cmd == svc_cdtrack) {
			if (nqp_buf.cursize < 3)
				goto waitformore;
			NQP_Flush (2);
			NQP_Skip (1);
		}
		else if (cmd == svc_finale) {
			byte *p = (byte *)memchr (nqp_buf_data + 1, 0, nqp_buf.cursize - 1);
			if (!p)
				goto waitformore;
			nqp_expect = (p - nqp_buf_data) + 1;
		}
		else if (cmd == svc_intermission) {
			int i;
			NQP_Flush (1);
			for (i = 0; i < 3; i++)
				MSG_WriteCoord (&sv.reliable_datagram, svs.clients[0].edict->v->origin[i]);
			for (i = 0; i < 3; i++)
				MSG_WriteAngle (&sv.reliable_datagram, svs.clients[0].edict->v->angles[i]);
		}
		else if (cmd == nq_svc_cutscene) {
			byte *p = (byte *)memchr (nqp_buf_data + 1, 0, nqp_buf.cursize - 1);
			if (!p)
				goto waitformore;
			MSG_WriteByte (&sv.reliable_datagram, svc_stufftext);
			MSG_WriteString (&sv.reliable_datagram, "//cutscene\n"); // ZQ extension
			NQP_Skip (p - nqp_buf_data + 1);
		}
		else if (nqp_buf_data[0] == svc_temp_entity) {
			if (nqp_buf.cursize < 2)
				break;

switch (nqp_buf_data[1]) {
  case TE_SPIKE:
  case TE_SUPERSPIKE:
  case TE_EXPLOSION:
  case TE_TAREXPLOSION:
  case TE_WIZSPIKE:
  case TE_KNIGHTSPIKE:
  case TE_LAVASPLASH:
  case TE_TELEPORT:
		nqp_expect = 8;
		break;
  case TE_GUNSHOT:
		if (nqp_buf.cursize < 8)
			goto waitformore;
		NQP_Flush (2);
		MSG_WriteByte (&sv.reliable_datagram, 1);
		NQP_Flush (6);
		break;

  case TE_LIGHTNING1:
  case TE_LIGHTNING2:
  case TE_LIGHTNING3:
		nqp_expect = 16;
	  break;
  case NQ_TE_BEAM:
  {		int entnum;
		if (nqp_buf.cursize < 16)
			goto waitformore;
		MSG_WriteByte (&sv.reliable_datagram, svc_temp_entity);
		MSG_WriteByte (&sv.reliable_datagram, TE_LIGHTNING1);
		entnum = nqp_buf_data[2] + nqp_buf_data[3]*256;
		if ((unsigned int)entnum > 1023)
			entnum = 0;
		MSG_WriteShort (&sv.reliable_datagram, (short)(entnum - 1288));
		NQP_Skip (4);
		nqp_expect = 12;
		break;
  }

  case NQ_TE_EXPLOSION2:
		nqp_expect = 10;
		break;
  default:
		Con_Printf ("WARNING: progs.dat sent an unsupported svc_temp_entity: %i\n", nqp_buf_data[1]);
	    goto ignore;
}

		}
		else {
			Con_Printf ("WARNING: progs.dat sent an unsupported svc: %i\n", cmd);
ignore:
			nqp_ignore_this_frame = true;
			break;
		}
	}
waitformore:;
}

#else // !WITH_NQPROGS
#define NQP_Process()
sizebuf_t nqp_buf;	// dummy
#endif


void PF_WriteByte (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteByte (&nqp_buf, G_FLOAT(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Byte(cl, G_FLOAT(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 1))
			{
				MVD_MSG_WriteByte(G_FLOAT(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteChar (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteByte (&nqp_buf, G_FLOAT(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Char(cl, G_FLOAT(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 1))
			{
				MVD_MSG_WriteByte(G_FLOAT(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteChar (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteShort (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteShort (&nqp_buf, G_FLOAT(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, G_FLOAT(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 2))
			{
				MVD_MSG_WriteShort(G_FLOAT(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteShort (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteLong (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteLong (&nqp_buf, G_FLOAT(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 4);
		ClientReliableWrite_Long(cl, G_FLOAT(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 4))
			{
				MVD_MSG_WriteLong(G_FLOAT(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteLong (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteAngle (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteAngle (&nqp_buf, G_FLOAT(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
#ifdef FTE_PEXT_FLOATCOORDS
		int size = msg_anglesize;
#else
		int size = 1;
#endif
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, size);
		ClientReliableWrite_Angle(cl, G_FLOAT(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, size))
			{
				MVD_MSG_WriteAngle(G_FLOAT(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteAngle (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteCoord (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteCoord (&nqp_buf, G_FLOAT(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
#ifdef FTE_PEXT_FLOATCOORDS
		int size = msg_coordsize;
#else
		int size = 2;
#endif
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, size);
		ClientReliableWrite_Coord(cl, G_FLOAT(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, size))
			{
				MVD_MSG_WriteCoord(G_FLOAT(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteCoord (WriteDest(), G_FLOAT(OFS_PARM1));
}

void PF_WriteString (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteString (&nqp_buf, G_STRING(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1+strlen(G_STRING(OFS_PARM1)));
		ClientReliableWrite_String(cl, G_STRING(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 1 + strlen(G_STRING(OFS_PARM1))))
			{
				MVD_MSG_WriteString(G_STRING(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteString (WriteDest(), G_STRING(OFS_PARM1));
}


void PF_WriteEntity (void)
{
	if (pr_nqprogs)
	{
		if (G_FLOAT(OFS_PARM0) == MSG_ONE || G_FLOAT(OFS_PARM0) == MSG_INIT)
			return;	// we don't support this
		MSG_WriteShort (&nqp_buf, G_EDICTNUM(OFS_PARM1));
		NQP_Process ();
		return;
	}

	if (G_FLOAT(OFS_PARM0) == MSG_ONE)
	{
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, G_EDICTNUM(OFS_PARM1));
		if (sv.mvdrecording)
		{
			if (MVDWrite_Begin(dem_single, cl - svs.clients, 2))
			{
				MVD_MSG_WriteShort(G_EDICTNUM(OFS_PARM1));
			}
		}
	}
	else
		MSG_WriteShort (WriteDest(), G_EDICTNUM(OFS_PARM1));
}

//=============================================================================

int SV_ModelIndex (char *name);

void PF_makestatic (void)
{
	entity_state_t* s;
	edict_t	*ent;

	ent = G_EDICT(OFS_PARM0);
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
#ifdef FTE_PEXT_TRANS
	s->trans = ent->xv.alpha >= 1.0f ? 0 : bound(0, (byte)(ent->xv.alpha * 254.0), 254);
#endif
	++sv.static_entity_count;

	// throw the entity away now
	ED_Free (ent);
}

//=============================================================================

/*
==============
PF_setspawnparms
==============
*/
void PF_setspawnparms (void)
{
	edict_t	*ent;
	int		i;
	client_t	*client;

	ent = G_EDICT(OFS_PARM0);
	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > MAX_CLIENTS)
		PR_RunError ("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i-1);

	for (i=0 ; i< NUM_SPAWN_PARMS ; i++)
		(&PR_GLOBAL(parm1))[i] = client->spawn_parms[i];
}

/*
==============
PF_changelevel
==============
*/
void PF_changelevel (void)
{
	char	*s;
	static	int	last_spawncount;

	// make sure we don't issue two changelevels
	if (svs.spawncount == last_spawncount)
		return;
	last_spawncount = svs.spawncount;

	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("map %s\n",s));
}


/*
==============
PF_logfrag
 
logfrag (killer, killee)
==============
*/
void PF_logfrag (void)
{
	edict_t	*ent1, *ent2;
	int		e1, e2;
	char	*s;
	// -> scream
	time_t		t;
	struct tm	*tblock;
	// <-

	ent1 = G_EDICT(OFS_PARM0);
	ent2 = G_EDICT(OFS_PARM1);

	e1 = NUM_FOR_EDICT(ent1);
	e2 = NUM_FOR_EDICT(ent2);

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
	SZ_Print (&svs.log[svs.logsequence&1], s);
	SV_Write_Log(FRAG_LOG, 1, s);
	//	SV_Write_Log(MOD_FRAG_LOG, 1, "\n==== PF_logfrag ===={\n");
	//	SV_Write_Log(MOD_FRAG_LOG, 1, va("%d\n", time(NULL)));
	//	SV_Write_Log(MOD_FRAG_LOG, 1, s);
	//	SV_Write_Log(MOD_FRAG_LOG, 1, "}====================\n");
}

/*
==============
PF_infokey

string(entity e, string key) infokey
==============
*/
void PF_infokey (void)
{
	edict_t	*e;
	int		e1;
	char	*value;
	char	*key;
	static	char ov[256];
	client_t *cl;

	e = G_EDICT(OFS_PARM0);
	e1 = NUM_FOR_EDICT(e);
	key = G_STRING(OFS_PARM1);
	cl = &svs.clients[e1-1];

	if (e1 == 0)
	{
		if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
			value = Info_Get(&_localinfo_, key);
	}
	else if (e1 <= MAX_CLIENTS)
	{
		value = ov;
		if (!strncmp(key, "ip", 3))
			strlcpy(ov, NET_BaseAdrToString (cl->netchan.remote_address), sizeof(ov));
		else if (!strncmp(key, "realip", 7))
			strlcpy(ov, NET_BaseAdrToString (cl->realip), sizeof(ov));
		else if (!strncmp(key, "download", 9))
			//snprintf(ov, sizeof(ov), "%d", cl->download != NULL ? (int)(100*cl->downloadcount/cl->downloadsize) : -1);
			snprintf(ov, sizeof(ov), "%d", cl->file_percent ? cl->file_percent : -1); //bliP: file percent
		else if (!strncmp(key, "ping", 5))
			snprintf(ov, sizeof(ov), "%d", SV_CalcPing (cl));
		else if (!strncmp(key, "*userid", 8))
			snprintf(ov, sizeof(ov), "%d", svs.clients[e1 - 1].userid);
		else if (!strncmp(key, "login", 6))
			value = cl->login;
		else
			value = Info_Get (&cl->_userinfo_ctx_, key);
	}
	else
		value = "";

	strlcpy(pr_string_temp, value, MAX_PR_STRING_SIZE);
	RETURN_STRING(pr_string_temp);
	PF_SetTempString();
}

/*
==============
PF_stof
 
float(string s) stof
==============
*/
void PF_stof (void)
{
	char	*s;

	s = G_STRING(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = Q_atof(s);
}


/*
==============
PF_multicast
 
void(vector where, float set) multicast
==============
*/
void PF_multicast (void)
{
	float	*o;
	int		to;

	o = G_VECTOR(OFS_PARM0);
	to = G_FLOAT(OFS_PARM1);

	SV_Multicast (o, to);
}

//DP_QC_SINCOSSQRTPOW
//float sin(float x) = #60
void PF_sin (void)
{
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}

//DP_QC_SINCOSSQRTPOW
//float cos(float x) = #61
void PF_cos (void)
{
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}

//DP_QC_SINCOSSQRTPOW
//float sqrt(float x) = #62
void PF_sqrt (void)
{
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}

//DP_QC_SINCOSSQRTPOW
//float pow(float x, float y) = #97;
static void PF_pow (void)
{
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

//DP_QC_MINMAXBOUND
//float min(float a, float b, ...) = #94
void PF_min (void)
{
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc >= 3)
	{
		int i;
		float f = G_FLOAT(OFS_PARM0);
		for (i = 1;i < pr_argc;i++)
			if (G_FLOAT((OFS_PARM0+i*3)) < f)
				f = G_FLOAT((OFS_PARM0+i*3));
		G_FLOAT(OFS_RETURN) = f;
	}
	else
		Sys_Error("min: must supply at least 2 floats");
}

//DP_QC_MINMAXBOUND
//float max(float a, float b, ...) = #95
void PF_max (void)
{
	// LordHavoc: 3+ argument enhancement suggested by FrikaC
	if (pr_argc == 2)
		G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	else if (pr_argc >= 3)
	{
		int i;
		float f = G_FLOAT(OFS_PARM0);
		for (i = 1;i < pr_argc;i++)
			if (G_FLOAT((OFS_PARM0+i*3)) > f)
				f = G_FLOAT((OFS_PARM0+i*3));
		G_FLOAT(OFS_RETURN) = f;
	}
	else
		Sys_Error("max: must supply at least 2 floats");
}

//DP_QC_MINMAXBOUND
//float bound(float min, float value, float max) = #96
void PF_bound (void)
{
	G_FLOAT(OFS_RETURN) = bound(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
}

/*
=================
PF_tracebox

Like traceline but traces a box of the size specified
(NOTE: currently the hull size can only be one of the sizes used in the map
for bmodel collisions, entity collisions will pay attention to the exact size
specified however, this is a collision code limitation in quake itself,
and will be fixed eventually).

DP_QC_TRACEBOX

void(vector v1, vector mins, vector maxs, vector v2, float nomonsters, entity ignore) tracebox = #90;
=================
*/
static void PF_tracebox (void)
{
        float       *v1, *v2, *mins, *maxs;
        edict_t     *ent;
        int          nomonsters;
        trace_t      trace;

        v1 = G_VECTOR(OFS_PARM0);
        mins = G_VECTOR(OFS_PARM1);
        maxs = G_VECTOR(OFS_PARM2);
        v2 = G_VECTOR(OFS_PARM3);
        nomonsters = G_FLOAT(OFS_PARM4);
        ent = G_EDICT(OFS_PARM5);

        trace = SV_Trace (v1, mins, maxs, v2, nomonsters, ent);

        PR_GLOBAL(trace_allsolid) = trace.allsolid;
        PR_GLOBAL(trace_startsolid) = trace.startsolid;
        PR_GLOBAL(trace_fraction) = trace.fraction;
        PR_GLOBAL(trace_inwater) = trace.inwater;
        PR_GLOBAL(trace_inopen) = trace.inopen;
        VectorCopy (trace.endpos, PR_GLOBAL(trace_endpos));
        VectorCopy (trace.plane.normal, PR_GLOBAL(trace_plane_normal));
        PR_GLOBAL(trace_plane_dist) =  trace.plane.dist;
        if (trace.e.ent)
                PR_GLOBAL(trace_ent) = EDICT_TO_PROG(trace.e.ent);
        else
                PR_GLOBAL(trace_ent) = EDICT_TO_PROG(sv.edicts);
}

/*
=================
PF_randomvec

DP_QC_RANDOMVEC
vector randomvec() = #91
=================
*/
static void PF_randomvec (void)
{
	vec3_t temp;

	do {
		temp[0] = (rand() & 0x7fff) * (2.0 / 0x7fff) - 1.0;
		temp[1] = (rand() & 0x7fff) * (2.0 / 0x7fff) - 1.0;
		temp[2] = (rand() & 0x7fff) * (2.0 / 0x7fff) - 1.0;
	} while (DotProduct(temp, temp) >= 1);

	VectorCopy (temp, G_VECTOR(OFS_RETURN));
}

/*
=================
PF_cvar_string

QSG_CVARSTRING DP_QC_CVAR_STRING
string cvar_string(string varname) = #103;
=================
*/
static void PF_cvar_string (void)
{
	char	*str;
	cvar_t	*var;

	str = G_STRING(OFS_PARM0);
	var = Cvar_Find(str);
	if (!var) {
		G_INT(OFS_RETURN) = 0;
		return;
	}

	strlcpy (pr_string_temp, var->string, MAX_PR_STRING_SIZE);
	RETURN_STRING(pr_string_temp);
	PF_SetTempString();
}

// DP_REGISTERCVAR
// float(string name, string value) registercvar = #93;
// DarkPlaces implementation has an undocumented feature where you can add cvar flags
// as a third parameter;  I don't see how that can be useful so it's not implemented
void PF_registercvar (void)
{
	char *name, *value;

	name = G_STRING(OFS_PARM0);
	value = G_STRING(OFS_PARM0);

	if (Cvar_Find(name)) {
		G_INT(OFS_RETURN) = 0;
		return;
	}

	Cvar_Create (name, value, 0);
	G_INT(OFS_RETURN) = 1;
}

// ZQ_PAUSE
// void(float pause) setpause = #531;
void PF_setpause (void)
{
	int pause;

	pause = G_FLOAT(OFS_PARM0) ? 1 : 0;

	if (pause != (sv.paused & 1))
		SV_TogglePause (NULL, 1);
}


/*
==============
PF_checkextension

float checkextension(string extension) = #99;
==============
*/
static void PF_checkextension (void)
{
	static char *supported_extensions[] = {
		"DP_CON_SET",               // http://wiki.quakesrc.org/index.php/DP_CON_SET
		"DP_QC_CVAR_STRING",		// http://wiki.quakesrc.org/index.php/DP_QC_CVAR_STRING
		"DP_QC_MINMAXBOUND",        // http://wiki.quakesrc.org/index.php/DP_QC_MINMAXBOUND
		"DP_QC_RANDOMVEC",			// http://wiki.quakesrc.org/index.php/DP_QC_RANDOMVEC
		"DP_QC_SINCOSSQRTPOW",      // http://wiki.quakesrc.org/index.php/DP_QC_SINCOSSQRTPOW
		"DP_QC_TRACEBOX",			// http://wiki.quakesrc.org/index.php/DP_QC_TRACEBOX
		"DP_REGISTERCVAR",			// http://wiki.quakesrc.org/index.php/DP_REGISTERCVAR
		"FTE_CALLTIMEOFDAY",        // http://wiki.quakesrc.org/index.php/FTE_CALLTIMEOFDAY
		"QSG_CVARSTRING",			// http://wiki.quakesrc.org/index.php/QSG_CVARSTRING
		"ZQ_CLIENTCOMMAND",			// http://wiki.quakesrc.org/index.php/ZQ_CLIENTCOMMAND
		"ZQ_ITEMS2",                // http://wiki.quakesrc.org/index.php/ZQ_ITEMS2
		"ZQ_MOVETYPE_NOCLIP",       // http://wiki.quakesrc.org/index.php/ZQ_MOVETYPE_NOCLIP
		"ZQ_MOVETYPE_FLY",          // http://wiki.quakesrc.org/index.php/ZQ_MOVETYPE_FLY
		"ZQ_MOVETYPE_NONE",         // http://wiki.quakesrc.org/index.php/ZQ_MOVETYPE_NONE
		"ZQ_PAUSE",					// http://wiki.quakesrc.org/index.php/ZQ_PAUSE
		"ZQ_QC_STRINGS",			// http://wiki.quakesrc.org/index.php/ZQ_QC_STRINGS
		"ZQ_QC_TOKENIZE",           // http://wiki.quakesrc.org/index.php/ZQ_QC_TOKENIZE
		"ZQ_VWEP",
		NULL
	};
	char **pstr, *extension;
	extension = G_STRING(OFS_PARM0);

	for (pstr = supported_extensions; *pstr; pstr++) {
		if (!strcasecmp(*pstr, extension)) {
			G_FLOAT(OFS_RETURN) = 1.0;	// supported
			return;
		}
	}

	G_FLOAT(OFS_RETURN) = 0.0;	// not supported
}



void PF_Fixme (void)
{
	PR_RunError ("unimplemented bulitin");
}



static builtin_t std_builtins[] =
{
PF_Fixme,		//#0
PF_makevectors,	// void(entity e)	makevectors 		= #1;
PF_setorigin,	// void(entity e, vector o) setorigin	= #2;
PF_setmodel,	// void(entity e, string m) setmodel	= #3;
PF_setsize,	// void(entity e, vector min, vector max) setsize = #4;
PF_Fixme,	// void(entity e, vector min, vector max) setabssize = #5;
PF_break,	// void() break						= #6;
PF_random,	// float() random						= #7;
PF_sound,	// void(entity e, float chan, string samp) sound = #8;
PF_normalize,	// vector(vector v) normalize			= #9;
PF_error,	// void(string e) error				= #10;
PF_objerror,	// void(string e) objerror				= #11;
PF_vlen,	// float(vector v) vlen				= #12;
PF_vectoyaw,	// float(vector v) vectoyaw		= #13;
PF_Spawn,	// entity() spawn						= #14;
PF_Remove,	// void(entity e) remove				= #15;
PF_traceline,	// float(vector v1, vector v2, float tryents) traceline = #16;
PF_checkclient,	// entity() clientlist					= #17;
PF_Find,	// entity(entity start, .string fld, string match) find = #18;
PF_precache_sound,	// void(string s) precache_sound		= #19;
PF_precache_model,	// void(string s) precache_model		= #20;
PF_stuffcmd,	// void(entity client, string s)stuffcmd = #21;
PF_findradius,	// entity(vector org, float rad) findradius = #22;
PF_bprint,	// void(string s) bprint				= #23;
PF_sprint,	// void(entity client, string s) sprint = #24;
PF_dprint,	// void(string s) dprint				= #25;
PF_ftos,	// void(string s) ftos				= #26;
PF_vtos,	// void(string s) vtos				= #27;
PF_coredump,
PF_traceon,
PF_traceoff,		//#30
PF_eprint,	// void(entity e) debug print an entire entity
PF_walkmove, // float(float yaw, float dist) walkmove
PF_Fixme, // float(float yaw, float dist) walkmove
PF_droptofloor,
PF_lightstyle,
PF_rint,
PF_floor,
PF_ceil,
PF_Fixme,
PF_checkbottom,		//#40
PF_pointcontents,
PF_Fixme,
PF_fabs,
PF_aim,
PF_cvar,
PF_localcmd,
PF_nextent,
PF_particle,
PF_changeyaw,
PF_Fixme,		//#50
PF_vectoangles,

PF_WriteByte,
PF_WriteChar,
PF_WriteShort,
PF_WriteLong,
PF_WriteCoord,
PF_WriteAngle,
PF_WriteString,
PF_WriteEntity,		//#59

PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,
PF_Fixme,

SV_MoveToGoal,
PF_precache_file,
PF_makestatic,

PF_changelevel,		//#70
PF_Fixme,

PF_cvar_set,
PF_centerprint,

PF_ambientsound,

PF_precache_model,
PF_precache_sound,		// precache_sound2 is different only for qcc
PF_precache_file,

PF_setspawnparms,

PF_logfrag,

PF_infokey,		//#80
PF_stof,
PF_multicast,
};

#define num_std_builtins (sizeof(std_builtins)/sizeof(std_builtins[0]))

static struct { int num; builtin_t func; } ext_builtins[] =
{
{60, PF_sin},			//float(float f) sin = #60;
{61, PF_cos},			//float(float f) cos = #61;
{62, PF_sqrt},			//float(float f) sqrt = #62;

{84, PF_tokenize},		// float(string s) tokenize
{85, PF_argc},			// float() argc
{86, PF_argv},			// string(float n) argv

{90, PF_tracebox},		// void (vector v1, vector mins, vector maxs, vector v2, float nomonsters, entity ignore) tracebox
{91, PF_randomvec},		// vector() randomvec
////
{93, PF_registercvar},	// float(string name, string value) registercvar
{94, PF_min},			// float(float a, float b, ...) min
{95, PF_max},			// float(float a, float b, ...) max
{96, PF_bound},			// float(float min, float value, float max) bound
{97, PF_pow},			// float(float x, float y) pow
////
{99, PF_checkextension},// float(string name) checkextension
////
{103, PF_cvar_string},	// string(string varname) cvar_string
////
{114, PF_strlen},		// float(string s) strlen
{115, PF_strcat},		// string(string s1, string s2, ...) strcat
{116, PF_substr},		// string(string s, float start, float count) substr
//{117, PF_stov},			// vector(string s) stov
{118, PF_strzone},		// string(string s) strzone
{119, PF_strunzone},	// void(string s) strunzone
{231, PF_calltimeofday},// void() calltimeofday
{448, PF_cvar_string},	// string(string varname) cvar_string
{531, PF_setpause},		//void(float pause) setpause
{532, PF_precache_vwep_model},	// float(string model) precache_vwep_model = #532;
};

#define num_ext_builtins (sizeof(ext_builtins)/sizeof(ext_builtins[0]))

builtin_t *pr_builtins = NULL;
int pr_numbuiltins = 0;

void PR_InitBuiltins (void)
{
	int i;

	if (pr_builtins)
		return; // We don't need reinit it.

	// Free old array.
	Q_free (pr_builtins);
	// We have at least iD builtins.
	pr_numbuiltins = num_std_builtins;
	// Find highest builtin number to see how much space we actually need.
	for (i = 0; i < num_ext_builtins; i++)
		pr_numbuiltins = max(ext_builtins[i].num + 1, pr_numbuiltins);
	// Allocate builtins array.
	pr_builtins = (builtin_t *) Q_malloc(pr_numbuiltins * sizeof(builtin_t));
	// Init new array to PF_Fixme().
	for (i = 0; i < pr_numbuiltins; i++)
		pr_builtins[i] = PF_Fixme;
	// Copy iD builtins in new array.
	memcpy (pr_builtins, std_builtins, num_std_builtins * sizeof(builtin_t));
	// Add QSG builtins or, probably, overwrite iD ones.
	for (i = 0; i < num_ext_builtins; i++)
	{
		assert (ext_builtins[i].num >= 0);
		pr_builtins[ext_builtins[i].num] = ext_builtins[i].func;
	}
}

#endif // CLIENTONLY