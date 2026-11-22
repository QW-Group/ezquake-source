/*
Copyright (C) 1996-1997 Id Software, Inc.
Copyright (C) 2016      Spike

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

// Scripted particle effects, FTE's r_part configs. Uses vec3_t and qbool,
// so include it after common.h.

#ifndef EZQUAKE_R_PART_SCRIPT_H
#define EZQUAKE_R_PART_SCRIPT_H

struct model_s;

// Effect type indices are >= 0; 0 is a valid effect.
#define P_INVALID (-1)

// Handle to a per-owner trail state, 0 for none. It carries the state's
// generation, so a key whose state was recycled no longer matches. Owners may
// be zeroed or copied freely; a copied key shares the state.
typedef unsigned int trailkey_t;

// model_t emitflags
#define PSCRIPT_EMIT_REPLACE  1 // the effect replaces the model, which is not drawn
#define PSCRIPT_EMIT_FORWARDS 2 // emit along the entity's forward vector instead of down

// Effects the engine spawns by fixed name. Some resolve through a fallback
// name; see PScript_EffectType.
typedef enum {
	PSCRIPT_TE_SPIKE,
	PSCRIPT_TE_SUPERSPIKE,
	PSCRIPT_TE_BULLET,
	PSCRIPT_TE_SUPERBULLET,
	PSCRIPT_TE_GUNSHOT,
	PSCRIPT_TE_QWGUNSHOT,
	PSCRIPT_TE_BLOOD,
	PSCRIPT_TE_QWBLOOD,
	PSCRIPT_TE_LIGHTNINGBLOOD,
	PSCRIPT_TE_EXPLOSION,
	PSCRIPT_TE_TAREXPLOSION,
	PSCRIPT_TE_WIZSPIKE,
	PSCRIPT_TE_KNIGHTSPIKE,
	PSCRIPT_TE_LAVASPLASH,
	PSCRIPT_TE_TELEPORT,
	PSCRIPT_TE_RAILTRAIL,
	PSCRIPT_TE_MUZZLEFLASH,
	PSCRIPT_TE_EXPLOSION2,
	PSCRIPT_TE_GUNSHOTQUAD,
	PSCRIPT_TE_SPIKEQUAD,
	PSCRIPT_TE_SUPERSPIKEQUAD,
	PSCRIPT_TE_EXPLOSIONQUAD,
	PSCRIPT_TE_SPARK,
	PSCRIPT_TE_SMALLFLASH,
	PSCRIPT_TE_FLAMEJET,
	PSCRIPT_TE_PLASMABURN,
	PSCRIPT_TE_TEI_G3,
	PSCRIPT_TE_TEI_SMOKE,
	PSCRIPT_TE_TEI_BIGEXPLOSION,
	PSCRIPT_TE_TEI_PLASMAHIT,
	PSCRIPT_TR_ROCKET,
	PSCRIPT_TR_GRENADE,
	PSCRIPT_TR_BLOOD,
	PSCRIPT_TR_SLIGHTBLOOD,
	PSCRIPT_TR_WIZSPIKE,
	PSCRIPT_TR_KNIGHTSPIKE,
	PSCRIPT_TR_VORESPIKE,
	PSCRIPT_TE_LIGHTNING1,
	PSCRIPT_TE_LIGHTNING1_END,
	PSCRIPT_TE_LIGHTNING2,
	PSCRIPT_TE_LIGHTNING2_END,
	PSCRIPT_TE_LIGHTNING3,
	PSCRIPT_TE_LIGHTNING3_END,
	PSCRIPT_TE_BEAM,
	PSCRIPT_TE_BEAM_END,
	PSCRIPT_EFFECT_COUNT
} pscript_effect_t;

// Seconds simulated in the current host frame, 0 while paused or rewinding.
extern float pscript_frametime;
// Increases once per host frame and is never 0, so a zeroed stamp is stale.
extern int pscript_hostframe;
// Increases on every reload of the particle sets and is never 0 after the
// first; callers that cache effect type numbers by name look them up again
// when it changes.
extern int pscript_refreshcount;

void PScript_Init(void);
void PScript_Shutdown(void);
void PScript_NewMap(void);
void PScript_ClearParticles(void);
void PScript_BeginFrame(void);
void PScript_DrawParticles(void);
void PScript_EndFrame(void);
qbool PScript_Active(void);
qbool PScript_DynamicActive(void);
qbool PScript_InlineModel(const struct model_s *mod);
qbool PScript_EmitterDue(int *stamp);

// The handle may be cached; rulesets are applied when the effect is run.
int PScript_FindParticleType(const char *name);
int PScript_EffectType(pscript_effect_t effect);
qbool PScript_ExplosionSprite(void);

// For events and moving entities, so they fail under rulesets that restrict
// particles, as does PScript_ParticleTrail.
qbool PScript_RunParticleEffectState(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk);
// For effects on the world, static entities and the map's inline brush
// models, which run under every ruleset.
qbool PScript_RunWorldEffect(vec3_t org, vec3_t dir, float count, int typenum, trailkey_t *tk);
qbool PScript_RunParticleEffectTypeString(vec3_t org, vec3_t dir, float count, const char *name);
qbool PScript_RunParticleEffectTinted(vec3_t org, vec3_t dir, float count, int typenum, int colorindex, int colorrand);
qbool PScript_RunParticleEffect(vec3_t org, vec3_t dir, int color, int count);
qbool PScript_RunParticleWeather(vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, const char *efname);
qbool PScript_RunParticleCube(int typenum, vec3_t minb, vec3_t maxb, vec3_t dir, float count, int colour, qbool gravity);
qbool PScript_ParticleTrail(vec3_t start, vec3_t end, int typenum, float timeinterval, int dlkey, trailkey_t *tk);
void PScript_DelinkTrailstate(trailkey_t *tk);
void PScript_MatchTrailstate(trailkey_t *tk, int *tktype, int typenum);

void PScript_UpdateModelEffects(struct model_s *mod);

void PScript_ClearTextureEffects(void);
void PScript_SetTextureEffect(const char *texture, const char *effect);
void PScript_ClearSurfaceParticles(struct model_s *mod);

#endif // EZQUAKE_R_PART_SCRIPT_H
