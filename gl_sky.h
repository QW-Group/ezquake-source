
#ifndef GL_SKY_HEADER
#define GL_SKY_HEADER

qbool R_DetermineSkyLimits(qbool *ignore_z);
void Sky_MakeSkyVec2(float s, float t, int axis, vec3_t v);
qbool GLC_LoadSkyboxTextures(char* skyname);
qbool GLM_LoadSkyboxTextures(char* skyname);
qbool Sky_LoadSkyboxTextures(const char* skyname);

extern msurface_t *skychain;
extern msurface_t **skychain_tail;
extern float skymins[2][6], skymaxs[2][6];
extern texture_ref solidskytexture, alphaskytexture;

#endif
