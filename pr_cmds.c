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

#include "qwsvdef.h"

#define	RETURN_EDICT(e)		(((int *) pr_globals)[OFS_RETURN] = EDICT_TO_PROG(e))
#define	RETURN_STRING(s)	(((int *) pr_globals)[OFS_RETURN] = PR_SetString(s))

cvar_t	sv_aim = {"sv_aim", "2"};

/********************************** SUPPORT **********************************/

#define STRINGTEMP_BUFFERS	16
#define STRINGTEMP_LENGTH	128
#define MAX_VARSTRING		256

static char *PR_GetTempString(void){
	char *s;
	static char pr_string_temp[STRINGTEMP_BUFFERS][STRINGTEMP_LENGTH];
	static int pr_string_tempindex = 0;

	s = pr_string_temp[pr_string_tempindex];
	pr_string_tempindex = (pr_string_tempindex + 1) % STRINGTEMP_BUFFERS;
	return s;
}

char *PF_VarString (int first) {
	int i;
	char *s, *out, *outend;
	static char pr_varstring_temp[MAX_VARSTRING];

	out = pr_varstring_temp;
	outend = pr_varstring_temp + sizeof(pr_varstring_temp) - 1;
	for (i = first; i < pr_argc && out < outend; i++) {
		s = G_STRING((OFS_PARM0 + i * 3));
		while (out < outend && *s)
			*out++ = *s++;
	}
	*out++ = 0;

	return pr_varstring_temp;
}

static void PR_CheckEmptyString (char *s) {
	if (s[0] <= ' ')
		PR_RunError ("Bad string");
}

//for PF_checkclient --->

#define	MAX_CHECK	16
static	byte	checkpvs[MAX_MAP_LEAFS / 8];

static int PF_newcheckclient (int check) {
	int i;
	byte *pvs;
	edict_t *ent;
	mleaf_t *leaf;
	vec3_t org;

	// cycle to the next one
	check = bound(1, check, MAX_CLIENTS);

	i = (check == MAX_CLIENTS) ? 1 : check + 1;

	for ( ;  ; i++) {
		if (i == MAX_CLIENTS + 1)
			i = 1;

		ent = EDICT_NUM(i);

		if (i == check)
			break;	// didn't find anything else

		if (ent->free)
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
	leaf = Mod_PointInLeaf (org, sv.worldmodel);
	pvs = Mod_LeafPVS (leaf, sv.worldmodel);
	memcpy (checkpvs, pvs, (sv.worldmodel->numleafs+7)>>3 );

	return i;
}

//for PF_checkclient <---

//for message writing --->

#define	MSG_BROADCAST	0		// unreliable to all
#define	MSG_ONE			1		// reliable to one (msg_entity)
#define	MSG_ALL			2		// reliable to all
#define	MSG_INIT		3		// write to the init string
#define	MSG_MULTICAST	4		// for multicast()

static sizebuf_t *WriteDest (void) {
	int dest;

	dest = G_FLOAT(OFS_PARM0);
	switch (dest) {
	case MSG_BROADCAST:
		return &sv.datagram;
	
	case MSG_ONE:
		Host_Error("Shouldn't be at MSG_ONE");
		
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

static client_t *Write_GetClient(void) {
	int entnum;
	edict_t	*ent;

	ent = PROG_TO_EDICT(pr_global_struct->msg_entity);
	entnum = NUM_FOR_EDICT(ent);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError ("WriteDest: not a client");
	return &svs.clients[entnum - 1];
}

//for message writing >--

/********************************** BUILTINS **********************************/

//void(string e) error = #10
//This is a TERMINAL error, which will kill off the entire server. Dumps self.
void PF_error (void) {
	char *s;
	edict_t	*ed;

	s = PF_VarString(0);
	Com_Printf ("======SERVER ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name) ,s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);

	Host_Error ("Program error");
}

//void(string e) objerror = #11
//Dumps out self, then an error message.  The program is aborted and self is removed, but the level can continue.
void PF_objerror (void) {
	char *s;
	edict_t	*ed;

	s = PF_VarString(0);
	Com_Printf ("======OBJECT ERROR in %s:\n%s\n", PR_GetString(pr_xfunction->s_name),s);
	ed = PROG_TO_EDICT(pr_global_struct->self);
	ED_Print (ed);
	ED_Free (ed);

	Host_Error ("Program error");
}

//void() coredump = #28
void PF_coredump (void) {
	ED_PrintEdicts ();
}

//void() traceon = #29
void PF_traceon (void) {
	pr_trace = true;
}

//void() traceoff = #30
void PF_traceoff (void) {
	pr_trace = false;
}

//void(entity e) debug print an entire entity = #31
void PF_eprint (void) {
	ED_PrintNum (G_EDICTNUM(OFS_PARM0));
}

//void(entity e, vector o) setorigin = #2
//This is the only valid way to move an object without using the physics of the world (setting velocity and waiting).
//Directly changing origin will not set internal links correctly, so clipping would be messed up.
//This should be called when an object is spawned, and then only if it is teleported.
void PF_setorigin (void) {
	edict_t	*e;
	float *org;

	e = G_EDICT(OFS_PARM0);
	org = G_VECTOR(OFS_PARM1);
	VectorCopy (org, e->v.origin);
	SV_LinkEdict (e, false);
}

//void(entity e, string m) setmodel = #3
//Also sets size, mins, and maxs for inline bmodels
void PF_setmodel (void) {
	edict_t	*e;
	char *m, **check;
	int i;
	model_t	*mod;

	e = G_EDICT(OFS_PARM0);
	m = G_STRING(OFS_PARM1);

	// check to see if model was properly precached
	for (i = 0, check = sv.model_precache; *check; i++, check++) {
		if (!strcmp(*check, m))
			break;
	}

	if (!*check)
		PR_RunError ("no precache: %s\n", m);

	e->v.model = PR_SetString(m);
	e->v.modelindex = i;

	// if it is an inline model, get the size information for it
	if (m[0] == '*') {
		mod = Mod_ForName (m, true);
		VectorCopy (mod->mins, e->v.mins);
		VectorCopy (mod->maxs, e->v.maxs);
		VectorSubtract (mod->maxs, mod->mins, e->v.size);
		SV_LinkEdict (e, false);
	}
}

//void(entity e, vector min, vector max) setsize = #4
//the size box is rotated by the current angle
void PF_setsize (void) {
	edict_t	*e;
	float *min, *max;

	e = G_EDICT(OFS_PARM0);
	min = G_VECTOR(OFS_PARM1);
	max = G_VECTOR(OFS_PARM2);
	VectorCopy (min, e->v.mins);
	VectorCopy (max, e->v.maxs);
	VectorSubtract (max, min, e->v.size);
	SV_LinkEdict (e, false);
}

//void(string s) bprint3 = #23
//broadcast print to everyone on server
void PF_bprint (void) {
	char *s;
	int level;

	level = G_FLOAT(OFS_PARM0);

	s = PF_VarString(1);
	SV_BroadcastPrintf (level, "%s", s);
}

//void(entity client, string s) sprint = #24
//single print to a specific client
void PF_sprint (void) {
	client_t *cl;
	int entnum, level, buflen, len, i;
	char *buf, *str;
	qboolean flush = false, flushboth = false;

	entnum = G_EDICTNUM(OFS_PARM0);
	level = G_FLOAT(OFS_PARM1);

	str = PF_VarString(2);

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		Com_Printf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum - 1];

	buf = cl->sprint_buf;
	buflen = strlen (buf);
	len = strlen (str);

	// flush the buffer if there's not enough space
	// also flush if sprint level has changed or if str is a colored message
	if (len >= sizeof(cl->sprint_buf) || str[0] == 1 || str[0] == 2)		// a colored message
		flushboth = true;
	else if (buflen + len >= sizeof(cl->sprint_buf) || level != cl->sprint_level)
		flush = true;

	if ((flush || flushboth) && buflen) {
		SV_ClientPrintf (cl, cl->sprint_level, "%s", buf);
		buf[0] = 0;
	}

	if (flushboth) {
		SV_ClientPrintf (cl, level, "%s", str);
		return;
	}

	strcat (buf, str);
	cl->sprint_level = level;
	buflen += len;

	// flush complete (\n terminated) strings
	for (i = buflen - 1; i >= 0; i--) {
		if (buf[i] == '\n') {
			buf[i] = 0;
			SV_ClientPrintf (cl, cl->sprint_level, "%s\n", buf);
			// move the remainder to buffer beginning
			strcpy (buf, buf + i + 1);
			return;
		}
	}
}

//void(string s) dprint = #25
void PF_dprint (void) {
	Com_Printf ("%s",PF_VarString(0));
}

//void(entity client, strings) centerprint = #73
//single print to a specific client
void PF_centerprint (void) {
	char *s;
	int entnum;
	client_t *cl;

	entnum = G_EDICTNUM(OFS_PARM0);
	s = PF_VarString(1);

	if (entnum < 1 || entnum > MAX_CLIENTS) {
		Com_Printf ("tried to sprint to a non-client\n");
		return;
	}

	cl = &svs.clients[entnum - 1];

	ClientReliableWrite_Begin (cl, svc_centerprint, 2 + strlen(s));
	ClientReliableWrite_String (cl, s);
}

//void(entity e) makevectors = #1
//Writes new values for v_forward, v_up, and v_right based on angles
void PF_makevectors (void) {
	AngleVectors (G_VECTOR(OFS_PARM0), pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

//vector(vector v) normalize = #9
void PF_normalize (void) {
	float *value1, length2;
	vec3_t newvalue;

	value1 = G_VECTOR(OFS_PARM0);

	if (!(length2 = DotProduct(value1, value1))) {
		VectorClear(newvalue);
	} else {
		length2 = 1 / sqrt(length2);
		VectorScale(value1, length2, newvalue);
	}

	VectorCopy (newvalue, G_VECTOR(OFS_RETURN));	
}

//float(vector v) vlen = #12
void PF_vlen (void) {
	float *value1, length2;

	value1 = G_VECTOR(OFS_PARM0);
	length2 = DotProduct(value1, value1);
	G_FLOAT(OFS_RETURN) = length2 ? sqrt(length2) : 0;
}

//float(vector v) vectoyaw = #13
void PF_vectoyaw (void) {
	float *value1, yaw;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0) {
		yaw = 0;
	} else {
		yaw = atan2(value1[1], value1[0]) * 180 / M_PI;
		if (yaw < 0)
			yaw += 360;
	}

	G_FLOAT(OFS_RETURN) = yaw;
}

//vector(vector v) vectoangles = #51
void PF_vectoangles (void) {
	float *value1, forward, yaw, pitch;

	value1 = G_VECTOR(OFS_PARM0);

	if (value1[1] == 0 && value1[0] == 0) {
		yaw = 0;
		pitch = (value1[2] > 0) ? 90 : 270;
	} else {
		yaw = atan2(value1[1], value1[0]) * 180 / M_PI;
		if (yaw < 0)
			yaw += 360;

		forward = sqrt (value1[0] * value1[0] + value1[1] * value1[1]);
		pitch = atan2(value1[2], forward) * 180 / M_PI;
		if (pitch < 0)
			pitch += 360;
	}

	G_FLOAT(OFS_RETURN + 0) = pitch;
	G_FLOAT(OFS_RETURN + 1) = yaw;
	G_FLOAT(OFS_RETURN + 2) = 0;
}

//float() random = #7
//Returns a number from 0 <= num < 1
void PF_random (void) {
	float num;
		
	num = (rand () & 0x7fff) / ((float) 0x7fff);

	G_FLOAT(OFS_RETURN) = num;
}

//float(float v) rint = #36
void PF_rint (void) {
	float f;

	f = G_FLOAT(OFS_PARM0);
	if (f > 0)
		G_FLOAT(OFS_RETURN) = (int) (f + 0.5);
	else
		G_FLOAT(OFS_RETURN) = (int) (f - 0.5);
}

//float(float v) floor = #37
void PF_floor (void) {
	G_FLOAT(OFS_RETURN) = floor(G_FLOAT(OFS_PARM0));
}

//float(float v) ceil = #38
void PF_ceil (void) {
	G_FLOAT(OFS_RETURN) = ceil(G_FLOAT(OFS_PARM0));
}

//void() ChangeYaw = #49
//This was a major timewaster in progs, so it was converted to C
void PF_changeyaw (void) {
	edict_t *ent;
	float ideal, current, move, speed;
		//FIXME, OPTIMISE + changepitch
	ent = PROG_TO_EDICT(pr_global_struct->self);
	current = anglemod(ent->v.angles[1]);
	ideal = ent->v.ideal_yaw;
	speed = ent->v.yaw_speed;

	if (current == ideal)
		return;
	move = ideal - current;
	if (ideal > current) {
		if (move >= 180)
			move = move - 360;
	} else {
		if (move <= -180)
			move = move + 360;
	}

	if (move > 0)
		move = min(move, speed);
	else
		move = max(move, -speed);

	ent->v.angles[1] = anglemod (current + move);
}
  
//void(vector pos, string samp, float vol, float atten) ambientsound = #74
void PF_ambientsound (void) {
	char **check, *samp;
	float *pos, vol, attenuation;
	int i, soundnum;

	pos = G_VECTOR (OFS_PARM0);			
	samp = G_STRING(OFS_PARM1);
	vol = G_FLOAT(OFS_PARM2);
	attenuation = G_FLOAT(OFS_PARM3);

	// check to see if samp was properly precached
	for (soundnum = 0, check = sv.sound_precache; *check; check++, soundnum++) {
		if (!strcmp(*check,samp))
			break;
	}

	if (!*check) {
		Com_Printf ("no precache: %s\n", samp);
		return;
	}

	// add an svc_spawnambient command to the level signon packet

	MSG_WriteByte (&sv.signon,svc_spawnstaticsound);
	for (i = 0; i < 3; i++)
		MSG_WriteCoord(&sv.signon, pos[i]);

	MSG_WriteByte (&sv.signon, soundnum);

	MSG_WriteByte (&sv.signon, vol * 255);
	MSG_WriteByte (&sv.signon, attenuation * 64);
}

/*
void(entity e, float chan, string samp) sound = #8

Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.

Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.

An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.
*/
void PF_sound (void) {
	char *sample;
	int channel, volume;
	edict_t *entity;
	float attenuation;

	entity = G_EDICT(OFS_PARM0);
	channel = G_FLOAT(OFS_PARM1);
	sample = G_STRING(OFS_PARM2);
	volume = G_FLOAT(OFS_PARM3) * 255;
	attenuation = G_FLOAT(OFS_PARM4);
	
	SV_StartSound (entity, channel, sample, volume, attenuation);
}

//void() break = #6
void PF_break (void) {
	Com_Printf ("break statement\n");
	*((int *) -4) = 0;	// dump to debugger
	//PR_RunError ("break statement");
}

/*
entity() clientlist = #17

Returns a client (or object that has a client enemy) that would be a valid target.
If there are more than one valid options, they are cycled each frame
If (self.origin + self.viewofs) is not in the PVS of the current target, it is not returned at all.
*/
void PF_checkclient (void) {
	edict_t	*ent, *self;
	mleaf_t	*leaf;
	int l;
	vec3_t view;

	// find a new check if on a new frame
	if (sv.time - sv.lastchecktime >= 0.1) {
		sv.lastcheck = PF_newcheckclient (sv.lastcheck);
		sv.lastchecktime = sv.time;
	}

	// return check if it might be visible	
	ent = EDICT_NUM(sv.lastcheck);
	if (ent->free || ent->v.health <= 0) {
		RETURN_EDICT(sv.edicts);
		return;
	}

	// if current entity can't possibly see the check entity, return 0
	self = PROG_TO_EDICT(pr_global_struct->self);
	VectorAdd (self->v.origin, self->v.view_ofs, view);
	leaf = Mod_PointInLeaf (view, sv.worldmodel);
	l = (leaf - sv.worldmodel->leafs) - 1;
	if (l < 0 || !(checkpvs[l >> 3] & (1 << (l & 7)))) {
		RETURN_EDICT(sv.edicts);
		return;
	}

	// might be able to see it
	RETURN_EDICT(ent);
}

//void(entity client, string s)stuffcmd = #21
//Sends text over to the client's execution buffer
void PF_stuffcmd (void) {
	int entnum, buflen, newlen, i;
	client_t *cl;
	char *buf, *str;

	entnum = G_EDICTNUM(OFS_PARM0);
	if (entnum < 1 || entnum > MAX_CLIENTS)
		PR_RunError ("Parm 0 not a client");
	cl = &svs.clients[entnum - 1];
	str = G_STRING(OFS_PARM1);	

	buf = cl->stufftext_buf;

	if (!strcmp(str, "disconnect\n")) {
		// so long and thanks for all the fish
		cl->drop = true;
		buf[0] = 0;
		return;
	}

	buflen = strlen (buf);
	newlen = strlen (str);

	if (buflen + newlen >= MAX_STUFFTEXT - 1) {
		// flush the buffer because there's no space left
		if (buflen) {
			ClientReliableWrite_Begin (cl, svc_stufftext, 2+buflen);
			ClientReliableWrite_String (cl, buf);
			buf[0] = 0;
		}
		if (newlen >= MAX_STUFFTEXT-1) {
			ClientReliableWrite_Begin (cl, svc_stufftext, 2+newlen);
			ClientReliableWrite_String (cl, str);
			return;
		}
	}

	strcat (buf, str);
	buflen += newlen;

	// flush complete (\n terminated) strings
	for (i = buflen - 1; i >= 0; i--) {
		if (buf[i] == '\n') {
			ClientReliableWrite_Begin (cl, svc_stufftext, 2 + (i + 1));
			ClientReliableWrite_SZ (cl, buf, i+1);
			ClientReliableWrite_Byte (cl, 0);
			// move the remainder to buffer beginning
			strcpy (buf, buf + i + 1);
			return;
		}
	}
}

//void(string s) localcmd = #46
//Sends text over to the client's execution buffer
void PF_localcmd (void) {
	char *str;
	
	str = G_STRING(OFS_PARM0);	
	Cbuf_AddText (str);
}

//float(string s) cvar = #45
void PF_cvar (void) {
	char *str;

	str = G_STRING(OFS_PARM0);

	//QC queries cvar("pr_checkextension") to see if an extension system is present
	if (!Q_strcasecmp(str, "pr_checkextension")) {
		G_FLOAT(OFS_RETURN) = 1.0;
		return;
	}

	G_FLOAT(OFS_RETURN) = Cvar_VariableValue (str);
}

//float cvar (string) = #72
void PF_cvar_set (void) {
	char *var_name, *val;
	cvar_t *var;

	var_name = G_STRING(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	if (!(var = Cvar_FindVar(var_name))) {
		Com_DPrintf ("PF_cvar_set: variable %s not found\n", var_name);
		return;
	}

	Cvar_Set (var, val);
}

//entity(vector org, float rad) findradius = #22
//Returns a chain of entities that have origins within a spherical area
void PF_findradius (void) {
	int i, j;
	float rad, *org;
	vec3_t eorg;
	edict_t *ent, *chain;

	chain = (edict_t *) sv.edicts;

	org = G_VECTOR(OFS_PARM0);
	rad = G_FLOAT(OFS_PARM1);

	ent = NEXT_EDICT(sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT(ent)) {
		if (ent->free)
			continue;
		if (ent->v.solid == SOLID_NOT)
			continue;
		for (j = 0; j < 3; j++)
			eorg[j] = org[j] - (ent->v.origin[j] + (ent->v.mins[j] + ent->v.maxs[j]) * 0.5);			
		if (DotProduct(eorg, eorg) > rad * rad)
			continue;

		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}
	RETURN_EDICT(chain);
}

//void(string s) ftos = #26
void PF_ftos (void) {
	char *tempstring;
	float v;
	int	i;

	v = G_FLOAT(OFS_PARM0);
	tempstring = PR_GetTempString();

	if (v == (int) v) {
		Q_snprintfz (tempstring, STRINGTEMP_LENGTH, "%d", (int) v);
	} else {
		Q_snprintfz (tempstring, STRINGTEMP_LENGTH, "%f", v);

		for (i = strlen(tempstring) - 1; i > 0 && tempstring[i] == '0'; i--)
			tempstring[i] = 0;
		if (tempstring[i] == '.')
			tempstring[i] = 0;
	}

	G_INT(OFS_RETURN) = PR_SetString(tempstring);
}

//float(float f) fabs = #43
void PF_fabs (void) {
	float	v;
	v = G_FLOAT(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = fabs(v);
}

//void(string s) vtos = #27
void PF_vtos (void) {
	char *s;

	s = PR_GetTempString();
	Q_snprintfz (s, STRINGTEMP_LENGTH,"'%5.1f %5.1f %5.1f'", G_VECTOR(OFS_PARM0)[0], G_VECTOR(OFS_PARM0)[1], G_VECTOR(OFS_PARM0)[2]);
	G_INT(OFS_RETURN) = PR_SetString(s);
}

//entity() spawn = #14
void PF_Spawn (void) {
	edict_t *ed;

	ed = ED_Alloc();
	RETURN_EDICT(ed);
}

//void(entity e) remove = #15
void PF_Remove (void) {
	edict_t	*ed;

	ed = G_EDICT(OFS_PARM0);
	ED_Free (ed);
}

//entity(entity start, .string fld, string match) find = #18;
void PF_Find (void) {
	int e, f;
	char *s, *t;
	edict_t	*ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_STRING(OFS_PARM2);

	for (e++ ; e < sv.num_edicts ; e++) {
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		t = E_STRING(ed,f);
		if (!t)
			continue;
		if (!strcmp(t,s)) {
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

//string(string s) precache_file = #68
void PF_precache_file (void) {
	// precache_file is only used to copy files with qcc, it does nothing
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
}

//void(string s) precache_sound = #19
void PF_precache_sound (void) {
	char *s;
	int i;

	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i = 0; i < MAX_SOUNDS; i++) {
		if (!sv.sound_precache[i]) {
			sv.sound_precache[i] = s;
			return;
		}
		if (!strcmp(sv.sound_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_sound: overflow");
}

//void(string s) precache_model = #20
void PF_precache_model (void) {
	char *s;
	int i;

	if (sv.state != ss_loading)
		PR_RunError ("PF_Precache_*: Precache can only be done in spawn functions");

	s = G_STRING(OFS_PARM0);
	G_INT(OFS_RETURN) = G_INT(OFS_PARM0);
	PR_CheckEmptyString (s);

	for (i = 1; i < MAX_MODELS; i++) {
		if (!sv.model_precache[i]) {
			sv.model_precache[i] = s;
			return;
		}
		if (!strcmp(sv.model_precache[i], s))
			return;
	}
	PR_RunError ("PF_precache_model: overflow");
}

//float(float yaw, float dist) walkmove = #32
void PF_walkmove (void) {
	edict_t	*ent;
	float yaw, dist;
	vec3_t move;
	dfunction_t *oldf;
	int oldself;

	ent = PROG_TO_EDICT(pr_global_struct->self);
	yaw = G_FLOAT(OFS_PARM0);
	dist = G_FLOAT(OFS_PARM1);

	if (!((int) ent->v.flags & (FL_ONGROUND|FL_FLY|FL_SWIM))) {
		G_FLOAT(OFS_RETURN) = 0;
		return;
	}

	yaw = yaw * M_PI * 2 / 360;

	move[0] = cos(yaw) * dist;
	move[1] = sin(yaw) * dist;
	move[2] = 0;

	// save program state, because SV_movestep may call other progs
	oldf = pr_xfunction;
	oldself = pr_global_struct->self;

	G_FLOAT(OFS_RETURN) = SV_movestep(ent, move, true);

	// restore program state
	pr_xfunction = oldf;
	pr_global_struct->self = oldself;
}

//float() droptofloor = #34
void PF_droptofloor (void) {
	edict_t *ent;
	vec3_t end;
	trace_t trace;

	ent = PROG_TO_EDICT(pr_global_struct->self);

	VectorCopy (ent->v.origin, end);
	end[2] -= 256;

	trace = SV_Move (ent->v.origin, ent->v.mins, ent->v.maxs, end, false, ent);

	if (trace.fraction == 1 || trace.allsolid) {
		G_FLOAT(OFS_RETURN) = 0;
	} else {
		VectorCopy (trace.endpos, ent->v.origin);
		SV_LinkEdict (ent, false);
		ent->v.flags = (int)ent->v.flags | FL_ONGROUND;
		ent->v.groundentity = EDICT_TO_PROG(trace.ent);
		G_FLOAT(OFS_RETURN) = 1;
	}
}

//void(float style, string value) lightstyle = #35
void PF_lightstyle (void) {
	int style, j;
	char *val;
	client_t *client;

	style = G_FLOAT(OFS_PARM0);
	val = G_STRING(OFS_PARM1);

	// change the string in sv
	sv.lightstyles[style] = val;

	// send message to all clients on this server
	if (sv.state != ss_active)
		return;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++) {
		if ( client->state == cs_spawned ) {
			ClientReliableWrite_Begin (client, svc_lightstyle, strlen(val)+3);
			ClientReliableWrite_Char (client, style);
			ClientReliableWrite_String (client, val);
		}
	}
}

/*
float(vector v1, vector v2, float tryents) traceline = #16

Used for use tracing and shot targeting
Traces are blocked by bbox and exact bsp entityes, and also slide box entities
if the tryents flag is set.
*/
void PF_traceline (void) {
	float *v1, *v2;
	trace_t trace;
	int nomonsters;
	edict_t *ent;

	v1 = G_VECTOR(OFS_PARM0);
	v2 = G_VECTOR(OFS_PARM1);
	nomonsters = G_FLOAT(OFS_PARM2);
	ent = G_EDICT(OFS_PARM3);

	trace = SV_Move (v1, vec3_origin, vec3_origin, v2, nomonsters, ent);

	pr_global_struct->trace_allsolid = trace.allsolid;
	pr_global_struct->trace_startsolid = trace.startsolid;
	pr_global_struct->trace_fraction = trace.fraction;
	pr_global_struct->trace_inwater = trace.inwater;
	pr_global_struct->trace_inopen = trace.inopen;
	VectorCopy (trace.endpos, pr_global_struct->trace_endpos);
	VectorCopy (trace.plane.normal, pr_global_struct->trace_plane_normal);
	pr_global_struct->trace_plane_dist =  trace.plane.dist;	
	if (trace.ent)
		pr_global_struct->trace_ent = EDICT_TO_PROG(trace.ent);
	else
		pr_global_struct->trace_ent = EDICT_TO_PROG(sv.edicts);
}

//float(entity e) checkbottom = #40
void PF_checkbottom (void) {
	edict_t	*ent;

	ent = G_EDICT(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_CheckBottom (ent);
}

//float(vector v) pointcontents = #41
void PF_pointcontents (void) {
	float *v;

	v = G_VECTOR(OFS_PARM0);

	G_FLOAT(OFS_RETURN) = SV_PointContents (v);	
}

//entity(entity e) nextent = #47
void PF_nextent (void) {
	int i;
	edict_t	*ent;

	i = G_EDICTNUM(OFS_PARM0);
	while (1) {
		i++;
		if (i == sv.num_edicts) {
			RETURN_EDICT(sv.edicts);
			return;
		}
		ent = EDICT_NUM(i);
		if (!ent->free) {
			RETURN_EDICT(ent);
			return;
		}
	}
}

//vector(entity e, float speed) aim = #44
//Pick a vector for the player to shoot along
void PF_aim (void) {
	edict_t *ent, *check, *bestent;
	vec3_t start, dir, end, bestdir;
	int i, j;
	trace_t	tr;
	float dist, bestdist, speed;
	char *noaim;

	ent = G_EDICT(OFS_PARM0);
	speed = G_FLOAT(OFS_PARM1);

	VectorCopy (ent->v.origin, start);
	start[2] += 20;

	// noaim option
	i = NUM_FOR_EDICT(ent);
	if (i > 0 && i < MAX_CLIENTS) {
		noaim = Info_ValueForKey (svs.clients[i - 1].userinfo, "noaim");
		if (atoi(noaim) > 0) {
			VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
			return;
		}
	}

	// try sending a trace straight
	VectorCopy (pr_global_struct->v_forward, dir);
	VectorMA (start, 2048, dir, end);
	tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
	if (tr.ent && tr.ent->v.takedamage == DAMAGE_AIM && (!teamplay.value || ent->v.team <=0 || ent->v.team != tr.ent->v.team)) {
		VectorCopy (pr_global_struct->v_forward, G_VECTOR(OFS_RETURN));
		return;
	}

	// try all possible entities
	VectorCopy (dir, bestdir);
	bestdist = sv_aim.value;
	bestent = NULL;

	check = NEXT_EDICT(sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, check = NEXT_EDICT(check)) {
		if (check->v.takedamage != DAMAGE_AIM)
			continue;
		if (check == ent)
			continue;
		if (teamplay.value && ent->v.team > 0 && ent->v.team == check->v.team)
			continue;	// don't aim at teammate
		for (j = 0; j < 3; j++)
			end[j] = check->v.origin[j] + 0.5 * (check->v.mins[j] + check->v.maxs[j]);
		VectorSubtract (end, start, dir);
		VectorNormalize (dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		if (dist < bestdist)
			continue;	// to far to turn
		tr = SV_Move (start, vec3_origin, vec3_origin, end, false, ent);
		if (tr.ent == check) {	
			// can shoot at this one
			bestdist = dist;
			bestent = check;
		}
	}

	if (bestent) {
		VectorSubtract (bestent->v.origin, ent->v.origin, dir);
		dist = DotProduct (dir, pr_global_struct->v_forward);
		VectorScale (pr_global_struct->v_forward, dist, end);
		end[2] = dir[2];
		VectorNormalize (end);
		VectorCopy (end, G_VECTOR(OFS_RETURN));	
	} else {
		VectorCopy (bestdir, G_VECTOR(OFS_RETURN));
	}
}

//void(float to, float f) WriteByte = #52
void PF_WriteByte (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Byte(cl, G_FLOAT(OFS_PARM1));
	} else {
		MSG_WriteByte (WriteDest(), G_FLOAT(OFS_PARM1));
	}
}

//void(float to, float f) WriteChar = #53
void PF_WriteChar (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Char(cl, G_FLOAT(OFS_PARM1));
	} else {
		MSG_WriteChar (WriteDest(), G_FLOAT(OFS_PARM1));
	}
}

//void(float to, float f) WriteShort = #54
void PF_WriteShort (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, G_FLOAT(OFS_PARM1));
	} else {
		MSG_WriteShort (WriteDest(), G_FLOAT(OFS_PARM1));
	}
}

//void(float to, float f) WriteLong = #55
void PF_WriteLong (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 4);
		ClientReliableWrite_Long(cl, G_FLOAT(OFS_PARM1));
	} else {
		MSG_WriteLong (WriteDest(), G_FLOAT(OFS_PARM1));
	}
}

//void(float to, float f) WriteCoord = #56
void PF_WriteCoord (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Coord(cl, G_FLOAT(OFS_PARM1));
	} else {
		MSG_WriteCoord (WriteDest(), G_FLOAT(OFS_PARM1));
	}
}

//void(float to, float f) WriteAngle = #57
void PF_WriteAngle (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1);
		ClientReliableWrite_Angle(cl, G_FLOAT(OFS_PARM1));
	} else {
		MSG_WriteAngle (WriteDest(), G_FLOAT(OFS_PARM1));
	}
}

//void(float to, string s) WriteString = #58
void PF_WriteString (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 1+strlen(G_STRING(OFS_PARM1)));
		ClientReliableWrite_String(cl, G_STRING(OFS_PARM1));
	} else {
		MSG_WriteString (WriteDest(), G_STRING(OFS_PARM1));
	}
}

//void(float to, entity e) WriteEntity = #59
void PF_WriteEntity (void) {
	if (G_FLOAT(OFS_PARM0) == MSG_ONE) {
		client_t *cl = Write_GetClient();
		ClientReliableCheckBlock(cl, 2);
		ClientReliableWrite_Short(cl, G_EDICTNUM(OFS_PARM1));
	} else {
		MSG_WriteShort (WriteDest(), G_EDICTNUM(OFS_PARM1));
	}
}

//void(entity e) makestatic = #69
void PF_makestatic (void) {
	edict_t	*ent;
	int i;

	ent = G_EDICT(OFS_PARM0);

	MSG_WriteByte (&sv.signon, svc_spawnstatic);

	MSG_WriteByte (&sv.signon, SV_ModelIndex(PR_GetString(ent->v.model)));

	MSG_WriteByte (&sv.signon, ent->v.frame);
	MSG_WriteByte (&sv.signon, ent->v.colormap);
	MSG_WriteByte (&sv.signon, ent->v.skin);
	for (i = 0; i < 3; i++) {
		MSG_WriteCoord(&sv.signon, ent->v.origin[i]);
		MSG_WriteAngle(&sv.signon, ent->v.angles[i]);
	}

	// throw the entity away now
	ED_Free (ent);
}

//void(entity e) setspawnparms = #78
void PF_setspawnparms (void) {
	edict_t	*ent;
	int i;
	client_t *client;

	ent = G_EDICT(OFS_PARM0);
	i = NUM_FOR_EDICT(ent);
	if (i < 1 || i > MAX_CLIENTS)
		PR_RunError ("Entity is not a client");

	// copy spawn parms out of the client_t
	client = svs.clients + (i - 1);

	for (i = 0; i < NUM_SPAWN_PARMS; i++)
		(&pr_global_struct->parm1)[i] = client->spawn_parms[i];
}

//void(string s) changelevel = #70
void PF_changelevel (void) {
	char *s;
	static int last_spawncount;

	// make sure we don't issue two changelevels
	if (svs.spawncount == last_spawncount)
		return;
	last_spawncount = svs.spawncount;

	s = G_STRING(OFS_PARM0);
	Cbuf_AddText (va("map %s\n", s));
}

//logfrag (string killer, string killee) = #79
void PF_logfrag (void) {
	edict_t	*ent1, *ent2;
	int e1, e2;
	char *s;

	ent1 = G_EDICT(OFS_PARM0);
	ent2 = G_EDICT(OFS_PARM1);

	e1 = NUM_FOR_EDICT(ent1);
	e2 = NUM_FOR_EDICT(ent2);

	if (e1 < 1 || e1 > MAX_CLIENTS || e2 < 1 || e2 > MAX_CLIENTS)
		return;

	s = va("\\%s\\%s\\\n", svs.clients[e1-1].name, svs.clients[e2-1].name);

	SZ_Print (&svs.log[svs.logsequence & 1], s);
	if (sv_fraglogfile) {
		fprintf (sv_fraglogfile, s);
		fflush (sv_fraglogfile);
	}
}

//string(entity e, string key) infokey = #80
void PF_infokey (void) {
	edict_t *e;
	int e1, ping;
	char *value, *key;
	static char ov[256];

	e = G_EDICT(OFS_PARM0);
	e1 = NUM_FOR_EDICT(e);
	key = G_STRING(OFS_PARM1);

	if (e1 == 0) {
		if ((value = Info_ValueForKey (svs.info, key)) == NULL || !*value)
			value = Info_ValueForKey(localinfo, key);
	} else if (e1 <= MAX_CLIENTS) {
		if (!strcmp(key, "ip")) {
			Q_strncpyz(ov, NET_BaseAdrToString (svs.clients[e1 - 1].netchan.remote_address), sizeof(ov));
			value = ov;
		} else if (!strcmp(key, "ping")) {
			ping = SV_CalcPing (&svs.clients[e1 - 1]);
			Q_snprintfz(ov, sizeof(ov), "%d", ping);
			value = ov;
		} else {
			value = Info_ValueForKey (svs.clients[e1 - 1].userinfo, key);
		}
	} else {
		value = "";
	}

	RETURN_STRING(value);
}

//float(string s) stof = #81
void PF_stof (void) {
	char *s;

	s = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = atof(s);
}

//void(vector where, float set) multicast = #82
void PF_multicast (void) {
	float *o;
	int to;

	o = G_VECTOR(OFS_PARM0);
	to = G_FLOAT(OFS_PARM1);
	SV_Multicast (o, to);
}

/***************************** BUILTIN EXTENSIONS *****************************/

//EXTENSION: DP_QC_COPYENTITY

//void(entity from, entity to) copyentity = #400
//copies data from one entity to another
void PF_copyentity (void) {
	edict_t *in, *out;

	in = G_EDICT(OFS_PARM0);
	out = G_EDICT(OFS_PARM1);

	memcpy(&out->v, &in->v, progs->entityfields * 4);
}

//EXTENSION: DP_QC_ETOS

//string(entity ent) etos = #65
void PF_etos (void) {
	char *s;

	s = PR_GetTempString();
	Q_snprintfz (s, STRINGTEMP_LENGTH, "entity %i", G_EDICTNUM(OFS_PARM0));
	G_INT(OFS_RETURN) = PR_SetString(s);
}

//EXTENSION: DP_QC_FINDCHAIN

//entity(string field, string match) findchain = #402
//chained search for strings in entity fields
void PF_findchain (void) {
	int i, f;
	char *s, *t;
	edict_t *ent, *chain;

	chain = (edict_t *) sv.edicts;

	f = G_INT(OFS_PARM0);
	s = G_STRING(OFS_PARM1);

	ent = NEXT_EDICT(sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT(ent)) {
		if (ent->free)
			continue;
		t = E_STRING(ent,f);
		if (!t)
			continue;
		if (strcmp(t, s))
			continue;

		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

//EXTENSION: DP_QC_FINDCHAINFLOAT

//entity(string field, float match) findchainfloat = #403
//chained search for float, int, and entity reference fields
void PF_findchainfloat (void) {
	int i, f;
	float s;
	edict_t	*ent, *chain;

	chain = (edict_t *) sv.edicts;

	f = G_INT(OFS_PARM0);
	s = G_FLOAT(OFS_PARM1);

	ent = NEXT_EDICT(sv.edicts);
	for (i = 1; i < sv.num_edicts; i++, ent = NEXT_EDICT(ent)) {
		if (ent->free)
			continue;
		if (E_FLOAT(ent, f) != s)
			continue;

		ent->v.chain = EDICT_TO_PROG(chain);
		chain = ent;
	}

	RETURN_EDICT(chain);
}

//EXTENSION: DP_QC_FINDFLOAT

//entity(entity start, float fld, float match) findfloat = #98
void PF_FindFloat (void) {
	int e, f;
	float s;
	edict_t *ed;

	e = G_EDICTNUM(OFS_PARM0);
	f = G_INT(OFS_PARM1);
	s = G_FLOAT(OFS_PARM2);

	for (e++; e < sv.num_edicts; e++) {
		ed = EDICT_NUM(e);
		if (ed->free)
			continue;
		if (E_FLOAT(ed,f) == s) {
			RETURN_EDICT(ed);
			return;
		}
	}

	RETURN_EDICT(sv.edicts);
}

//EXTENSION: DP_QC_MINMAXBOUND

//float(float a, floats) min = #94
void PF_min (void) {
	int i;
	float f;

	if (pr_argc == 2) {
		G_FLOAT(OFS_RETURN) = min(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	} else if (pr_argc >= 3) {
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < pr_argc; i++) {
			if (G_FLOAT((OFS_PARM0 + i * 3)) < f)
				f = G_FLOAT((OFS_PARM0 + i * 3));
		}
		G_FLOAT(OFS_RETURN) = f;
	} else {
		PR_RunError("PF_min: must supply at least 2 floats\n");
	}
}

//float(float a, floats) max = #95
void PF_max (void) {
	int i;
	float f;

	if (pr_argc == 2) {
		G_FLOAT(OFS_RETURN) = max(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
	} else if (pr_argc >= 3) {
		f = G_FLOAT(OFS_PARM0);
		for (i = 1; i < pr_argc; i++) {
			if (G_FLOAT((OFS_PARM0 + i * 3)) > f)
				f = G_FLOAT((OFS_PARM0 + i * 3));
		}
		G_FLOAT(OFS_RETURN) = f;
	} else {
		PR_RunError("PF_min: must supply at least 2 floats\n");
	}
}

//float(float minimum, float val, float maximum) bound = #96
void PF_bound (void) {
	G_FLOAT(OFS_RETURN) = bound(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1), G_FLOAT(OFS_PARM2));
}

//EXTENSION: DP_QC_RANDOMVEC

//vector() randomvec = #91
void PF_randomvec (void) {
	vec3_t temp;
	do {
		temp[0] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
		temp[1] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
		temp[2] = (rand() & 32767) * (2.0 / 32767.0) - 1.0;
	} while (DotProduct(temp, temp) >= 1);
	VectorCopy (temp, G_VECTOR(OFS_RETURN));
}

//EXTENSION: DP_QC_SINCOSSQRTPOW

//float(float f) sin = #60
void PF_sin (void) {
	G_FLOAT(OFS_RETURN) = sin(G_FLOAT(OFS_PARM0));
}

//float(float f) cos = #61
void PF_cos (void) {
	G_FLOAT(OFS_RETURN) = cos(G_FLOAT(OFS_PARM0));
}

//float(float f) sqrt = #62
void PF_sqrt (void) {
	G_FLOAT(OFS_RETURN) = sqrt(G_FLOAT(OFS_PARM0));
}

//float(float f, float f) pow = #97
void PF_pow (void) {
	G_FLOAT(OFS_RETURN) = pow(G_FLOAT(OFS_PARM0), G_FLOAT(OFS_PARM1));
}

//EXTENSION: DP_QC_VECTORVECTORS

//void(vector dir) vectorvectors = #432
//Writes new values for v_forward, v_up, and v_right based on the given forward vector
void PF_vectorvectors (void) {
	VectorCopy(G_VECTOR(OFS_PARM0), pr_global_struct->v_forward);
	VectorNormalize(pr_global_struct->v_forward);
	VectorVectors(pr_global_struct->v_forward, pr_global_struct->v_right, pr_global_struct->v_up);
}

//EXTENSION: FRIK_FILE

//float(string filename, float mode) fopen = #110
//void(float fhandle) fclose = #111
//string(float fhandle) fgets = #112
//void(float fhandle, string s) fputs = #113
//string(string s) strzone = #118
//void(string s) strunzone = #119

//float(string s) strlen = #114
void PF_strlen(void) {
	char *s;

	s = G_STRING(OFS_PARM0);
	G_FLOAT(OFS_RETURN) = strlen(s);
}

//string(string s1, string s2) strcat = #115
void PF_strcat(void) {
	char *s;

	s = PF_VarString(0);
	G_INT(OFS_RETURN) = PR_SetString(s);
}

//returns a section of a string as a tempstring
void PF_substring(void) {
	int i, start, length;
	char *s;
	static char string[MAX_VARSTRING];

	s = G_STRING(OFS_PARM0);
	start = G_FLOAT(OFS_PARM1);
	length = G_FLOAT(OFS_PARM2);

	for (i = 0; i < start && *s; i++, s++)
		;

	for (i = 0; i < MAX_VARSTRING - 1 && *s && i < length; i++, s++)
		string[i] = *s;
	string[i] = 0;

	G_INT(OFS_RETURN) = PR_SetString(string);
}


//vector(string s) stov = #117
//returns vector value from a string
void PF_stov(void) {
	int i;
	char *s;
	float *out;

	s = PF_VarString(0);
	out = G_VECTOR(OFS_RETURN);
	VectorClear(out);

	if (*s == '\'')
		s++;

	for (i = 0; i < 3; i++) {
		while (*s == ' ' || *s == '\t')
			s++;
		out[i] = atof (s);
		if (!out[i] && *s != '-' && *s != '+' && (*s < '0' || *s > '9'))
			break; // not a number
		while (*s && *s != ' ' && *s !='\t' && *s != '\'')
			s++;
		if (*s == '\'')
			break;
	}
}

//EXTENSION: KRIMZON_SV_PARSECLIENTCOMMAND

//void(entity e, string s) clientcommand = #440
//executes a command string as if it came from the specified client
void PF_clientcommand (void) {
	client_t *temp_client;
	int i;

	//find client for this entity
	i = NUM_FOR_EDICT(G_EDICT(OFS_PARM0)) - 1;
	if (i < 0 || i >= MAX_CLIENTS)
		PR_RunError("PF_clientcommand: entity is not a client");

	temp_client = sv_client;
	sv_client = &svs.clients[i];
	if (sv_client->state == cs_connected || sv_client->state == cs_spawned)
		SV_ExecuteUserCommand (G_STRING(OFS_PARM1), true);
	sv_client = temp_client;
}

static char **tokens = NULL;
static int max_tokens, num_tokens = 0;

//float(string s) tokenize = #441
//takes apart a string into individal words (access them with argv), returns number of tokens
void PF_tokenize (void) {
	int i;
	char *data, *str;

	str = G_STRING(OFS_PARM0);

	if (tokens) {
		for (i = 0; i < num_tokens; i++)
			Z_Free(tokens[i]);
		Z_Free(tokens);
		num_tokens = 0;
	}

	tokens = Z_Malloc(strlen(str) * sizeof(char *));
	max_tokens = strlen(str);

	for (data = str; (data = COM_Parse(data)) && num_tokens < max_tokens; num_tokens++)
		tokens[num_tokens] = CopyString(com_token);

	G_FLOAT(OFS_RETURN) = num_tokens;
}

//string(float n) argv = #442
//returns a word from the tokenized string (returns nothing for an invalid index)
void PF_argv (void) {
	int token_num;

	token_num = G_FLOAT(OFS_PARM0);

	if (token_num >= 0 && token_num < num_tokens)
		G_INT(OFS_RETURN) = PR_SetString(tokens[token_num]);
	else
		G_INT(OFS_RETURN) = PR_SetString("");
}

/****************************** EXTENSION SYSTEM ******************************/

static char *ENGINE_EXTENSIONS[] = {
	"DP_HALFLIFE_MAP",
	"DP_HALFLIFE_MAP_CVAR",
	"DP_QC_COPYENTITY",
	"DP_QC_ETOS",
	"DP_QC_FINDCHAIN",
	"DP_QC_FINDCHAINFLOAT",
	"DP_QC_FINDFLOAT",
	"DP_QC_MINMAXBOUND",
	"DP_QC_RANDOMVEC",
	"DP_QC_SINCOSSQRTPOW",
	"DP_QC_VECTORVECTORS",
	//"FRIK_FILE",		//incomplete
	"KRIMZON_SV_PARSECLIENTCOMMAND",
	NULL,
};

//float(string extension) checkextension = #99
//returns true if the extension is supported by the server
void PF_checkextension (void) {
	char **s, *extension;
	qboolean supported = false;

	extension = G_STRING(OFS_PARM0);

	for (s = ENGINE_EXTENSIONS; *s; s++) {
		if (!Q_strcasecmp(*s, extension)) {
			supported = true;
			break;
		}
	}

	G_FLOAT(OFS_RETURN) = supported;
}

//=============================================================================

#define EMPTY_BUILTIN_X10	NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL
#define EMPTY_BUILTIN_X20	EMPTY_BUILTIN_X10, EMPTY_BUILTIN_X10
#define EMPTY_BUILTIN_X50	EMPTY_BUILTIN_X20, EMPTY_BUILTIN_X20, EMPTY_BUILTIN_X10
#define EMPTY_BUILTIN_X100	EMPTY_BUILTIN_X50, EMPTY_BUILTIN_X50

builtin_t pr_builtins[] = {
NULL,
PF_makevectors,		// #1 void(entity e) makevectors;
PF_setorigin,		// #2 void(entity e, vector o) setorigin;
PF_setmodel,		// #3 void(entity e, string m) setmodel;
PF_setsize,			// #4 void(entity e, vector min, vector max) setsize;
NULL,
PF_break,			// #6 void() break;
PF_random,			// #7 float() random;
PF_sound,			// #8 void(entity e, float chan, string samp) sound;
PF_normalize,		// #9 vector(vector v) normalize;
PF_error,			// #10 void(string e) error;
PF_objerror,		// #11 void(string e) objerror;
PF_vlen,			// #12 float(vector v) vlen;
PF_vectoyaw,		// #13 float(vector v) vectoyaw;
PF_Spawn,			// #14 entity() spawn;
PF_Remove,			// #15 void(entity e) remove;
PF_traceline,		// #16 float(vector v1, vector v2, float tryents) traceline;
PF_checkclient,		// #17 entity() clientlist;
PF_Find,			// #18 entity(entity start, .string fld, string match) find;
PF_precache_sound,	// #19 void(string s) precache_sound;
PF_precache_model,	// #20 void(string s) precache_model;
PF_stuffcmd,		// #21 void(entity client, string s)stuffcmd;
PF_findradius,		// #22 entity(vector org, float rad) findradius;
PF_bprint,			// #23 void(string s) bprint3;
PF_sprint,			// #24 void(entity client, string s) sprint;
PF_dprint,			// #25 void(string s) dprint;
PF_ftos,			// #26 void(string s) ftos;
PF_vtos,			// #27 void(string s) vtos;
PF_coredump,		// #28 void() coredump
PF_traceon,			// #29 void() traceon
PF_traceoff,		// #30 void() traceoff
PF_eprint,			// #31 void(entity e) debug print an entire entity
PF_walkmove,		// #32 float(float yaw, float dist) walkmove
NULL,				// #33 float(float yaw, float dist) walkmove
PF_droptofloor,		// #34 float() droptofloor
PF_lightstyle,		// #35 void(float style, string value) lightstyle
PF_rint,			// #36 float(float v) rint
PF_floor,			// #37 float(float v) floor
PF_ceil,			// #38 float(float v) ceil
NULL,
PF_checkbottom,		// #40 float(entity e) checkbottom
PF_pointcontents,	// #41 float(vector v) pointcontents
NULL,
PF_fabs,			// #43 float(float f) fabs
PF_aim,				// #44 vector(entity e, float speed) aim
PF_cvar,			// #45 float(string s) cvar
PF_localcmd,		// #46 void(string s) localcmd
PF_nextent,			// #47 entity(entity e) nextent
NULL,
PF_changeyaw,		// #49 void() ChangeYaw
NULL,
PF_vectoangles,		// #51 vector(vector v) vectoangles
PF_WriteByte,		// #52 void(float to, float f) WriteByte
PF_WriteChar,		// #53 void(float to, float f) WriteChar
PF_WriteShort,		// #54 void(float to, float f) WriteShort
PF_WriteLong,		// #55 void(float to, float f) WriteLong
PF_WriteCoord,		// #56 void(float to, float f) WriteCoord
PF_WriteAngle,		// #57 void(float to, float f) WriteAngle
PF_WriteString,		// #58 void(float to, string s) WriteString
PF_WriteEntity,		// #59 void(float to, entity e) WriteEntity
PF_sin,				// #60 float(float f) sin (DP_QC_SINCOSSQRTPOW)
PF_cos,				// #61 float(float f) cos (DP_QC_SINCOSSQRTPOW)
PF_sqrt,			// #62 float(float f) sqrt (DP_QC_SINCOSSQRTPOW)
NULL,
NULL,
PF_etos,			// #65 string(entity ent) etos (DP_QC_ETOS)
NULL,
SV_MoveToGoal,		// #67 void(float step) movetogoal
PF_precache_file,	// #68 string(string s) precache_file
PF_makestatic,		// #69 void(entity e) makestatic
PF_changelevel,		// #70 void(string s) changelevel
NULL,
PF_cvar_set,		// #72 void(string var, string val) cvar_set
PF_centerprint,		// #73 void(entity client, strings) centerprint
PF_ambientsound,	// #74 void(vector pos, string samp, float vol, float atten) ambientsound
PF_precache_model,	// #75 string(string s) precache_model2
PF_precache_sound,	// #76 string(string s) precache_sound2
PF_precache_file,	// #77 string(string s) precache_file2
PF_setspawnparms,	// #78 void(entity e) setspawnparms
PF_logfrag,			// #79 logfrag (string killer, string killee)
PF_infokey,			// #80 string(entity e, string key) infokey
PF_stof,			// #81 float(string s) stof
PF_multicast,		// #82 void(vector where, float set) multicast
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
PF_randomvec,		// #91 vector() randomvec (DP_QC_RANDOMVEC)
NULL,
NULL,
PF_min,				// #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
PF_max,				// #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
PF_bound,			// #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
PF_pow,				// #97 float(float f, float f) pow (DP_QC_SINCOSSQRTPOW)
PF_FindFloat,		// #98 entity(entity start, float fld, float match) findfloat (DP_QC_FINDFLOAT)
PF_checkextension,	// #99 float(string s) checkextension (the basis of the extension system)
EMPTY_BUILTIN_X10,
NULL,				// #110 float(string filename, float mode) fopen (FRIK_FILE)
NULL,				// #111 void(float fhandle) fclose (FRIK_FILE)
NULL,				// #112 string(float fhandle) fgets (FRIK_FILE)
NULL,				// #113 void(float fhandle, string s) fputs (FRIK_FILE)
PF_strlen,			// #114 float(string s) strlen (FRIK_FILE)
PF_strcat,			// #115 string(string s1, string s2) strcat (FRIK_FILE)
PF_substring,		// #116 string(string s, float start, float length) substring (FRIK_FILE)
PF_stov,			// #117 vector(string s) stov (FRIK_FILE)
NULL,				// #118 string(string s) strzone (FRIK_FILE)
NULL,				// #119 string(string s) strunzone (FRIK_FILE)
EMPTY_BUILTIN_X10,
EMPTY_BUILTIN_X20,
EMPTY_BUILTIN_X50,
EMPTY_BUILTIN_X100,
EMPTY_BUILTIN_X100,
PF_copyentity,		// #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
NULL,
PF_findchain,		// #402 entity(string field, string match) findchain (DP_QC_FINDCHAIN)
PF_findchainfloat,	// #403 entity(float fld, float match) findchainfloat (DP_QC_FINDCHAINFLOAT)
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
EMPTY_BUILTIN_X10,
EMPTY_BUILTIN_X10,
NULL,
NULL,
PF_vectorvectors,	// #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
NULL,
PF_clientcommand,	// #440 void(entity e, string s) clientcommand (KRIMZON_SV_PARSECLIENTCOMMAND)
PF_tokenize,		// #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
PF_argv,			// #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND
};

int pr_numbuiltins = sizeof(pr_builtins) / sizeof(pr_builtins[0]);
