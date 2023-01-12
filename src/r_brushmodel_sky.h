
#ifndef EZQUAKE_R_BRUSHMODEL_SKY_HEADER
#define EZQUAKE_R_BRUSHMODEL_SKY_HEADER

void GLC_SkyDrawChainedSurfaces(void);

extern msurface_t *skychain;
extern msurface_t **skychain_tail;
extern texture_ref solidskytexture, alphaskytexture;
extern cvar_t r_skyname;

#define MAX_SKYBOXTEXTURES 6
extern texture_ref skyboxtextures[MAX_SKYBOXTEXTURES];

void R_ClearSkyTextures(void);
void R_LoadSky_f(void);
extern qbool r_skyboxloaded;

#endif // EZQUAKE_R_BRUSHMODEL_SKY_HEADER
