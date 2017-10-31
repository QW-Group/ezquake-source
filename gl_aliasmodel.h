
#ifndef GL_ALIASMODEL_HEADER
#define GL_ALIASMODEL_HEADER

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

//void GLM_DrawShellPoly(GLenum type, byte* color, float shellSize, unsigned int vao, int start, int vertices);
void R_AliasSetupLighting(entity_t *ent);

void GLC_DrawAliasFrame(model_t* model, int pose1, int pose2, qbool mtex, qbool scrolldir, texture_ref texture, texture_ref fb_texture, qbool outline);
void GLC_DrawAliasOutlineFrame(model_t* model, int pose1, int pose2);

void* Mod_LoadAllSkins(model_t* loadmodel, int numskins, daliasskintype_t* pskintype);

extern cvar_t gl_powerupshells_base1level, gl_powerupshells_base2level;
extern cvar_t gl_powerupshells_effect1level, gl_powerupshells_effect2level;

#endif
