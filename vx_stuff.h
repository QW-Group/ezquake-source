/*
Copyright (C) 2011 VultureIIC

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// VultureIIC
#ifndef __VX_STUFF__H__
#define __VX_STUFF__H__

#include "hud.h"

// For coronas
typedef enum
{
	C_FLASH,			// Explosion type - Basically just fades out
	C_SMALLFLASH,		// Muzzleflash
	C_BLUEFLASH,		// Blob expl
	C_FREE,				// Unused
	C_LIGHTNING,		// Used on lightning beams
	C_SMALLLIGHTNING,	// Used on lightning beams
	C_FIRE,				// Used in torches
	C_ROCKETLIGHT,		// A rockets glow...
	C_GREENLIGHT,		// Detpack
	C_REDLIGHT,			// Detpack about to blow
	C_BLUESPARK,		// Gibbed building spark
	C_GUNFLASH,			// Shotgun hit effect
	C_EXPLODE,			// Animated explosion
	C_WHITELIGHT,		// Gunshot #2 effect
	C_WIZLIGHT,			// CREATURE TRACER LIGHTS
	C_KNIGHTLIGHT,
	C_VORELIGHT,
} coronatype_t;

void NewCorona(coronatype_t type, vec3_t origin);
void R_DrawCoronas(void);
void InitCoronas(void);
void InitVXStuff(void);
void NewStaticLightCorona(coronatype_t type, vec3_t origin, entity_t *serialhint);

typedef enum {
	CORONATEX_STANDARD,
	CORONATEX_GUNFLASH,
	CORONATEX_EXPLOSIONFLASH1,
	CORONATEX_EXPLOSIONFLASH2,
	CORONATEX_EXPLOSIONFLASH3,
	CORONATEX_EXPLOSIONFLASH4,
	CORONATEX_EXPLOSIONFLASH5,
	CORONATEX_EXPLOSIONFLASH6,
	CORONATEX_EXPLOSIONFLASH7,

	CORONATEX_COUNT
} corona_texture_id;

typedef struct corona_texture_s {
	texture_ref texnum;

	texture_ref array_tex;
	int         array_index;
	float       array_scale_s;
	float       array_scale_t;
} corona_texture_t;

extern corona_texture_t corona_textures[CORONATEX_COUNT];


float CL_TraceLine(vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
void WeatherEffect(void);
void SparkGen(vec3_t org, byte col[3], float count, float size, float life);
extern cvar_t tei_lavafire;
extern cvar_t tei_slime;

extern cvar_t amf_coronas;
extern cvar_t amf_coronas_tele;
extern cvar_t amf_weather_rain;
extern cvar_t amf_weather_rain_fast;
extern cvar_t amf_nailtrail;
extern cvar_t amf_hidenails;
extern cvar_t amf_extratrails;
extern cvar_t amf_detpacklights;
extern cvar_t amf_buildingsparks;
extern cvar_t amf_lightning;
extern cvar_t amf_lightning_color;
extern cvar_t amf_lightning_size;
extern cvar_t amf_lightning_sparks;
extern cvar_t amf_lightning_sparks_size;
extern cvar_t amf_waterripple;
extern cvar_t cl_camera_tpp;
extern cvar_t amf_camera_chase_dist;
extern cvar_t amf_camera_chase_height;
extern cvar_t amf_camera_death;
extern cvar_t amf_stat_loss;
extern cvar_t amf_part_gunshot;
extern cvar_t amf_part_spikes;
extern cvar_t amf_part_explosion;
extern cvar_t amf_part_blobexplosion;
extern cvar_t amf_part_teleport;
extern cvar_t amf_part_blood;
extern cvar_t amf_part_traillen;
extern cvar_t amf_underwater_trails;
extern cvar_t amf_part_sparks;
extern cvar_t amf_hiderockets;
extern cvar_t amf_nailtrail_plasma;
extern cvar_t amf_part_muzzleflash;
extern cvar_t amf_inferno_trail;
extern cvar_t amf_inferno_speed;
extern cvar_t amf_part_2dshockwaves;
extern cvar_t amf_part_shockwaves;
extern cvar_t amf_part_trailtime;
extern cvar_t amf_part_gunshot_type;
extern cvar_t amf_part_spikes_type;
extern cvar_t amf_part_blood_color;
extern cvar_t amf_part_blood_type;
extern cvar_t amf_part_gibtrails;
extern cvar_t amf_lighting_vertex;
extern cvar_t amf_lighting_colour;
extern cvar_t amf_part_fire;
extern cvar_t amf_part_firecolor;
extern cvar_t amf_tracker_frags;
extern cvar_t amf_tracker_flags;
extern cvar_t amf_tracker_streaks;
extern cvar_t amf_cutf_tesla_effect;
extern cvar_t amf_nailtrail_water;
extern cvar_t amf_part_deatheffect;
extern cvar_t amf_part_fasttrails;
extern cvar_t amf_part_traildetail;
extern cvar_t amf_part_trailwidth;
extern cvar_t amf_part_trailtype;

void SCR_DrawAMFstats(void);
int ParticleCount, ParticleCountHigh, CoronaCount, CoronaCountHigh;

void ParticleAlphaTrail(vec3_t start, vec3_t end, vec3_t *trail_origin, float size, float life);
void ParticleNailTrail(vec3_t start, vec3_t end, centity_t* client_ent, float size, float life);

typedef enum
{
	C_NORMAL, //Normal camera
	C_CHASECAM, //Chase cam is on
	C_EXTERNAL, //Looking at the player for whatever reason
} cameramode_t;

extern cameramode_t cameratype;
void CameraUpdate(qbool dead);
void VXGunshot(vec3_t org, float count);
void VXTeleport(vec3_t org);
void VXBlobExplosion(vec3_t org);
void VXExplosion(vec3_t org);
void VXBlood(vec3_t org, float count);
void FuelRodGunTrail(vec3_t start, vec3_t end, vec3_t angle, vec3_t *trail_origin);
void FireballTrail(vec3_t start, vec3_t end, vec3_t *trail_origin, byte col[3], float size, float life);
void FireballTrailWave(vec3_t start, vec3_t end, vec3_t *trail_origin, byte col[3], float size, float life, vec3_t angle);

void DrawMuzzleflash(vec3_t start, vec3_t angle, vec3_t vel);
void VXNailhit(vec3_t org, float count);
void FuelRodExplosion(vec3_t org);
void Init_VLights(void);
void ParticleFire(vec3_t org);
void VX_TeslaCharge(vec3_t org);

void VX_LightningBeam(vec3_t start, vec3_t end);
void VX_DeathEffect(vec3_t org);
void VX_GibEffect(vec3_t org);
void VX_DetpackExplosion(vec3_t org);
void VX_LightningTrail(vec3_t start, vec3_t end);

void Amf_Reset_DamageStats(void);
void Draw_AMFStatLoss(int stat, hud_t* hud);

int VX_OwnFragTextLen(float scale, qbool proportional);
double VX_OwnFragTime(void);
const char * VX_OwnFragText(void);
qbool VX_TrackerIsEnemy(int player);

#endif // __VX_STUFF__H__
