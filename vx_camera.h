//vx_camera.h for QWAMF
//VultureIIC

void Start_DeathCam(void);
void Update_DeathCam(void);
void End_DeathCam(void);
extern cvar_t r_deathcam;
extern cvar_t chase_back;
extern cvar_t chase_up;
extern cvar_t chase_active;
extern cvar_t r_externcam;
extern qbool chasecam_allowed;

extern int externcam;
extern vec3_t death_org;


//Screen shaking
extern vec3_t shakemag[1];
extern cvar_t r_screenshaking;

void ApplyRadialForce(vec3_t org, float power);
void Update_ScreenShake (void);
void Reset_ScreenShake (void);
void Init_ScreenShake (void);
