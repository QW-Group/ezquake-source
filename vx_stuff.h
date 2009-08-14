// VultureIIC
#ifndef __VX_STUFF__H__
#define __VX_STUFF__H__

float TraceLineVX (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);

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

void NewCorona (coronatype_t type, vec3_t origin);
void R_DrawCoronas(void);
void InitCoronas(void);
void InitVXStuff(void);
void NewStaticLightCorona (coronatype_t type, vec3_t origin, entity_t *serialhint);

int	coronatexture;
int	gunflashtexture;
int	explosionflashtexture1;
int	explosionflashtexture2;
int	explosionflashtexture3;
int	explosionflashtexture4;
int	explosionflashtexture5;
int	explosionflashtexture6;
int	explosionflashtexture7;

float CL_TraceLine (vec3_t start, vec3_t end, vec3_t impact, vec3_t normal);
void WeatherEffect(void);
int QW_strncmp (char *s1, char *s2);
void SparkGen (vec3_t org, byte col[3], float count, float size, float life);
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
extern cvar_t amf_motiontrails;
extern cvar_t amf_motiontrails_wtf;
extern cvar_t amf_waterripple;
extern cvar_t amf_camera_chase;
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
int ParticleCount, ParticleCountHigh, CoronaCount, CoronaCountHigh, MotionBlurCount, MotionBlurCountHigh;
void CL_CreateBlurs (vec3_t start, vec3_t end, entity_t *ent);
void CL_UpdateBlurs (void);

void ParticleAlphaTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, float size, float life);

typedef enum 
{
	C_NORMAL, //Normal camera
	C_CHASECAM, //Chase cam is on
	C_EXTERNAL, //Looking at the player for whatever reason
} cameramode_t;

extern cameramode_t cameratype;
void CameraUpdate (qbool dead);
void VXGunshot (vec3_t org, float count);
void VXTeleport (vec3_t org);
void VXBlobExplosion (vec3_t org);
void VXExplosion (vec3_t org);
void VXBlood (vec3_t org, float count);
void AMFDEBUGTRAIL (vec3_t start, vec3_t end, float time);
void FuelRodGunTrail (vec3_t start, vec3_t end, vec3_t angle, vec3_t *trail_origin);
void FireballTrail (vec3_t start, vec3_t end, vec3_t *trail_origin, byte col[3], float size, float life);
void FireballTrailWave (vec3_t start, vec3_t end, vec3_t *trail_origin, byte col[3], float size, float life, vec3_t angle);

void DrawMuzzleflash (vec3_t start, vec3_t angle, vec3_t vel);
void VXNailhit (vec3_t org, float count);
void InfernoFire_f (void);
void CL_FakeExplosion (vec3_t pos);
void CL_FakeRocketLight(vec3_t org);
void FuelRodExplosion (vec3_t org);
void BurningExplosion (vec3_t org);
void Init_VLights(void);
void ParticleFire (vec3_t org) ;
void VX_TeslaCharge (vec3_t org) ;

void VX_BleedingTrail (vec3_t start, vec3_t end);
void VX_LightningBeam (vec3_t start, vec3_t end);
void CL_ClearBlurs(void);
void VX_DeathEffect (vec3_t org);
void VX_GibEffect (vec3_t org);
void VX_DetpackExplosion (vec3_t org);
void VX_LightningTrail (vec3_t start, vec3_t end);

void Amf_Reset_DamageStats (void);

int VX_OwnFragTextLen(void);
double VX_OwnFragTime(void);
const char * VX_OwnFragText(void);
qbool VX_TrackerIsEnemy(int player);

#endif // __VX_STUFF__H__
