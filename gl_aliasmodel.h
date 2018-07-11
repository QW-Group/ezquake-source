
#ifndef GL_ALIASMODEL_HEADER
#define GL_ALIASMODEL_HEADER

#include "r_buffers.h"

typedef struct custom_model_color_s {
	cvar_t color_cvar;
	cvar_t fullbright_cvar;
	cvar_t* amf_cvar;
	int model_hint;
} custom_model_color_t;

extern float     r_modelcolor[3];
extern float     r_modelalpha;
extern custom_model_color_t* custom_model;
extern byte      *shadedots;
extern float     shadelight;
extern float     ambientlight;

void R_AliasSetupLighting(entity_t *ent);
void GLC_DrawAliasFrame(
	entity_t* ent,
	model_t* model, int pose1, int pose2,
	qbool mtex, qbool scrolldir,
	texture_ref texture, texture_ref fb_texture,
	qbool outline, int effects, qbool alpha_blend
);
void GLM_DrawAliasFrame(
	entity_t* ent,
	model_t* model, int pose1, int pose2,
	texture_ref texture, texture_ref fb_texture,
	qbool outline, int effects, int render_effects
);

void GLM_DrawAliasModelFrame(
	entity_t* ent, model_t* model, int poseVertIndex, int poseVertIndex2, int vertsPerPose,
	texture_ref texture, texture_ref fb_texture, qbool outline, int effects, int render_effects
);

void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t* pskintype);

extern cvar_t gl_powerupshells_base1level, gl_powerupshells_base2level;
extern cvar_t gl_powerupshells_effect1level, gl_powerupshells_effect2level;
extern cvar_t gl_fb_models, r_shadows, r_fullbrightSkins, r_drawentities;

void R_SetupAliasFrame(
	entity_t* ent,
	model_t* model,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame,
	qbool mtex, qbool scrolldir, qbool outline,
	texture_ref texture, texture_ref fb_texture,
	int effects, int render_effects
);

#define ALIAS_BASE_SIZE_RATIO		(1.0 / 11.0)
// normalizing factor so player model works out to about
//  1 pixel per triangle
#define	MAX_LBM_HEIGHT		480

// FIXME: Get rid
extern qbool gl_mtexable;

extern texture_ref shelltexture;

void GLC_StateBeginMD3Draw(float alpha, qbool textured);
void GL_StateBeginDrawAliasModel(entity_t* e, aliashdr_t* paliashdr);
void GLC_StateBeginUnderwaterCaustics(void);
void GLC_StateEndUnderwaterCaustics(void);
void GLC_UnderwaterCaustics(entity_t* ent, model_t* clmodel, maliasframedesc_t* oldframe, maliasframedesc_t* frame, aliashdr_t* paliashdr);

void GLM_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr);
void GLC_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr);

// gl_mesh.c
void GL_MakeAliasModelDisplayLists(model_t *m, aliashdr_t *hdr);
void GL_AliasModelAddToVBO(model_t* mod, aliashdr_t* hdr, vbo_model_vert_t* aliasModelBuffer, int position);
void GL_MD3ModelAddToVBO(model_t* mod, vbo_model_vert_t* aliasModelBuffer, int position);
void GL_CreateAliasModelVBO(buffer_ref instance_vbo);

void GLC_StateBeginAliasPowerupShell(void);
void GLC_DrawPowerupShell(model_t* model, int effects, maliasframedesc_t *oldframe, maliasframedesc_t *frame);
void GLC_AllocateAliasPoseBuffer(void);

void R_DrawAliasModel(entity_t *ent);
void GLC_DrawAliasPowerupShell(entity_t *ent);

qbool R_FilterEntity(entity_t* ent);
qbool R_CullAliasModel(entity_t* ent, maliasframedesc_t* oldframe, maliasframedesc_t* frame);

#endif
