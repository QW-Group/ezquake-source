
#ifndef GL_ALIASMODEL_HEADER
#define GL_ALIASMODEL_HEADER

typedef struct custom_model_color_s {
	cvar_t color_cvar;
	cvar_t fullbright_cvar;
	cvar_t* amf_cvar;
	int model_hint;
} custom_model_color_t;

extern float     r_shellcolor[3];
extern float     r_modelcolor[3];
extern float     r_modelalpha;
extern custom_model_color_t* custom_model;
extern byte      *shadedots;
extern float     shadelight;
extern float     ambientlight;

//void GLM_DrawShellPoly(GLenum type, byte* color, float shellSize, unsigned int vao, int start, int vertices);
void R_AliasSetupLighting(entity_t *ent);
void R_DrawAliasModel(entity_t *ent, qbool shell_only);

void GLC_DrawAliasFrame(aliashdr_t *paliashdr, int pose1, int pose2, qbool mtex, qbool scrolldir, texture_ref texture, texture_ref fb_texture, GLenum textureEnvMode, qbool outline);
void GLC_DrawAliasOutlineFrame(aliashdr_t *paliashdr, int pose1, int pose2);

extern cvar_t gl_powerupshells_base1level, gl_powerupshells_base2level;
extern cvar_t gl_powerupshells_effect1level, gl_powerupshells_effect2level;

#endif
