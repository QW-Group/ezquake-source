
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

void GLM_DrawShellPoly(GLenum type, byte* color, float shellSize, unsigned int vao, int start, int vertices);
void R_AliasSetupLighting(entity_t *ent);
void R_DrawViewModel(void);
void R_DrawAliasModel(entity_t *ent);

#endif
