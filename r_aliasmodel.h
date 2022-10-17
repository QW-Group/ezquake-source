
#ifndef EZQUAKE_R_ALIASMODEL_HEADER
#define EZQUAKE_R_ALIASMODEL_HEADER

#include "r_buffers.h"

extern byte      *shadedots;

void R_AliasSetupLighting(entity_t *ent);

void GLM_DrawAliasModelFrame(
	entity_t* ent, model_t* model, int poseVertIndex, int poseVertIndex2, int vertsPerPose,
	texture_ref texture, qbool outline, int effects, int render_effects, float lerp_fraction
);

void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t* pskintype);

extern cvar_t gl_powerupshells_base1level, gl_powerupshells_base2level;
extern cvar_t gl_powerupshells_effect1level, gl_powerupshells_effect2level;
extern cvar_t gl_fb_models, r_shadows, r_fullbrightSkins, r_drawentities;

void R_SetupAliasFrame(
	entity_t* ent,
	model_t* model,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame,
	qbool outline, texture_ref texture, texture_ref fb_texture,
	int effects, int render_effects
);

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
// normalizing factor so player model works out to about
//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

// FIXME: Get rid
extern qbool gl_mtexable;

extern texture_ref shelltexture;

void GLC_StateBeginMD3Draw(float alpha, qbool textured, qbool weapon, qbool additive_pass);
void R_StateBeginDrawAliasModel(const entity_t* e, aliashdr_t* paliashdr);

// gl_mesh.c
void R_AliasModelPopulateVBO(model_t* mod, vbo_model_vert_t* aliasModelBuffer, int position);
void R_CreateAliasModelVBO(void);

void GLC_StateBeginAliasPowerupShell(qbool weapon);
void GLC_AllocateAliasPoseBuffer(void);

void GLC_BeginCausticsTextureMatrix(void);
void GLC_EndCausticsTextureMatrix(void);

void R_DrawAliasModel(entity_t *ent, qbool outline);

qbool R_FilterEntity(entity_t* ent);
qbool R_CullAliasModel(entity_t* ent, maliasframedesc_t* oldframe, maliasframedesc_t* frame);

void R_AliasModelPrepare(entity_t* ent, int framecount, int* frame1, int* frame2, float* lerpfrac, qbool* outline);
int R_AliasFramePose(const maliasframedesc_t* frame);
maliasframedesc_t* R_AliasModelFindFrame(aliashdr_t* hdr, const char* framename, int framenumber);
void R_AliasModelColor(const entity_t* ent, float* color, qbool* invalidate_texture);

// Previous constant was 135, pre-scaling... this is in world coordinates now,
//   so picked a new constant that stopped the nailgun moving 
#define ALIASMODEL_MAX_LERP_DISTANCE (6.3)

void R_SetSkinForPlayerEntity(entity_t* ent, texture_ref* texture, texture_ref* fb_texture, byte** color32bit);
void R_AliasModelDeterminePoses(const maliasframedesc_t* oldframe, const maliasframedesc_t* frame, int* prevpose, int* nextpose, float* lerpfrac);

// FIXME: not really GL_, probably Vulkan too?
void GL_PrepareAliasModel(model_t* m, aliashdr_t* hdr);

#endif // EZQUAKE_R
