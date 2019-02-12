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
// world.h
#ifndef __WORLD_H__
#define __WORLD_H__

#define	MOVE_NORMAL	0
#define	MOVE_NOMONSTERS	1
#define	MOVE_MISSILE	2
// { sv_antilag related
#define MOVE_LAGGED		64	//trace touches current last-known-state, instead of actual ents (just affects players for now)
// }

typedef struct areanode_s
{
	int		axis;		// -1 = leaf node
	float	dist;
	struct areanode_s	*children[2];
	link_t	trigger_edicts;
	link_t	solid_edicts;
} areanode_t;

#define AREA_SOLID	0
#define AREA_TRIGGERS	1

#define	AREA_DEPTH	4
#define	AREA_NODES	32

extern	areanode_t	sv_areanodes[AREA_NODES];

void SV_ClearWorld (void);
// called after the world model has been loaded, before linking any entities

void SV_UnlinkEdict (edict_t *ent);
// call before removing an entity, and before trying to move one,
// so it doesn't clip against itself
// flags ent->v->modified

void SV_LinkEdict (edict_t *ent, qbool touch_triggers);
// Needs to be called any time an entity changes origin, mins, maxs, or solid
// flags ent->v->modified
// sets ent->v->absmin and ent->v->absmax
// if touchtriggers, calls prog functions for the intersected triggers

int SV_PointContents (vec3_t p);
// returns the CONTENTS_* value from the world at the given point.
// does not check any entities at all

edict_t	*SV_TestEntityPosition (edict_t *ent);

trace_t SV_Trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end, int type, edict_t *passedict);
// mins and maxs are relative

// if the entire move stays in a solid volume, trace.allsolid will be set

// if the starting point is in a solid, it will be allowed to move out
// to an open area

// nomonsters is used for line of sight or edge testing, where mosnters
// shouldn't be considered solid objects

// passedict is explicitly excluded from clipping checks (normally NULL)

int SV_AreaEdicts (vec3_t mins, vec3_t maxs, edict_t **edicts, int max_edicts, int area);

void SV_AntilagReset (edict_t *ent);

#endif /* !__WORLD_H__ */
