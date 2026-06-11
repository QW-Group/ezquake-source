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
// cl_tent.h -- client side temporary entities, antilag 1 fake projectiles

#ifndef __CL_TENT_H__
#define __CL_TENT_H__

void CL_CreateBeam(int type, int ent, vec3_t start, vec3_t end);

// client-side "fake" projectiles: spawned locally by weapon prediction
// and/or driven by server-sent simple projectile states
#if defined(MVD_PEXT1_WEAPONPREDICTION) || defined(MVD_PEXT1_SIMPLEPROJECTILE)
#define EZQ_FAKEPROJ
#endif

#ifdef EZQ_FAKEPROJ

#define MAX_FAKEPROJ 96

typedef struct fproj_s {
	int modelindex, dl_key;
	int type, effects, flags, partcount, index;
	float starttime, endtime, parttime;
	vec3_t start, vel, avel, angs, org, partorg;
	centity_t cent;
#ifdef MVD_PEXT1_SIMPLEPROJECTILE
	int entnum;
	int owner;
#endif
} fproj_t;

extern fproj_t cl_fakeprojectiles[MAX_FAKEPROJ];

void Fproj_Physics_Bounce(fproj_t *proj, float dt);
void CL_MatchFakeProjectile(centity_t *cent);
fproj_t *CL_AllocFakeProjectile(void);

#ifdef MVD_PEXT1_WEAPONPREDICTION
fproj_t *CL_CreateFakeNail(void);
fproj_t *CL_CreateFakeSuperNail(void);
fproj_t *CL_CreateFakeGrenade(void);
fproj_t *CL_CreateFakeRocket(void);
#endif

#ifdef MVD_PEXT1_SIMPLEPROJECTILE
// per-edict mirror of the server's simple projectile state
typedef struct cs_sprojectile_s {
	int active;

	int fproj_number;
	int owner;
	vec3_t origin;
	vec3_t velocity;
	vec3_t angles;
	int modelindex;
	float time_offset;
} cs_sprojectile_t;
#endif

#endif // EZQ_FAKEPROJ

#endif // __CL_TENT_H__
