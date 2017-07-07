
#ifndef GL_SKY_HEADER
#define GL_SKY_HEADER

void EmitSkyVert(vec3_t v, qbool newPoly);
qbool R_DetermineSkyLimits(qbool *ignore_z);
void ClearSky(void);
void MakeSkyVec(float s, float t, int axis);
void MakeSkyVec2(float s, float t, int axis, vec3_t v);

extern float speedscale, speedscale2;
extern msurface_t *skychain;
extern msurface_t **skychain_tail;
extern float skymins[2][6], skymaxs[2][6];
extern int solidskytexture, alphaskytexture;

#endif
