
#ifndef GL_SKY_HEADER
#define GL_SKY_HEADER

qbool R_DetermineSkyLimits(qbool *ignore_z);
void Sky_MakeSkyVec2(float s, float t, int axis, vec3_t v);
qbool R_LoadSkyboxTextures(const char* skyname);
qbool GLC_LoadSkyboxTextures(const char* skyname);
qbool GLM_LoadSkyboxTextures(const char* skyname);
qbool Sky_LoadSkyboxTextures(const char* skyname);

void GLM_DrawSky(void);
void GLC_DrawSkyChain(void);
void GLC_DrawSky(void);

extern msurface_t *skychain;
extern msurface_t **skychain_tail;
extern float skymins[2][6], skymaxs[2][6];
extern texture_ref solidskytexture, alphaskytexture;
extern cvar_t r_skyname;

#define MAX_SKYBOXTEXTURES 6
extern texture_ref skyboxtextures[MAX_SKYBOXTEXTURES];

void R_ClearSkyTextures(void);
void R_LoadSky_f(void);
extern qbool r_skyboxloaded;

#endif
