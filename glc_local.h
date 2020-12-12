
#ifndef EZQUAKE_GLC_LOCAL_HEADER
#define EZQUAKE_GLC_LOCAL_HEADER

void GLC_FreeAliasPoseBuffer(void);
void GLC_Shutdown(qbool restarting);

void GLC_PreRenderView(void);
void GLC_SetupGL(void);
void GLC_StateBeginAliasOutlineFrame(qbool weaponmodel);
void GLC_StateBeginBrightenScreen(void);
void GLC_StateBeginFastSky(qbool world);
void GLC_StateBeginSkyZBufferPass(void);
void GLC_StateBeginSingleTextureSkyDome(void);
void GLC_StateBeginSingleTextureSkyDomeCloudPass(void);
void GLC_StateBeginMultiTextureSkyDome(qbool use_program);
void GLC_StateBeginMultiTextureSkyChain(void);
void GLC_StateBeginSingleTextureSkyPass(void);
void GLC_StateBeginSingleTextureCloudPass(void);
void GLC_StateBeginRenderFullbrights(void);
void GLC_StateBeginRenderLumas(void);
void GLC_StateBeginEmitDetailPolys(void);
void GLC_StateBeginDrawMapOutline(void);
void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, qbool alpha_blend, struct custom_model_color_s* custom_model, qbool weapon_model);
void GLC_StateBeginDrawAliasFrameProgram(texture_ref texture, texture_ref fb_texture, int render_effects, struct custom_model_color_s* custom_model, float ent_alpha, qbool additive_pass);
void GLC_StateBeginAliasModelShadow(void);
void GLC_StateBeginFastTurbPoly(byte color[4]);
void GLC_StateBeginBlendLightmaps(qbool use_buffers);
void GLC_StateBeginCausticsPolys(void);
void GLC_StateBeginUnderwaterAliasModelCaustics(texture_ref base_texture, texture_ref caustics_texture);
void GLC_StateEndUnderwaterAliasModelCaustics(void);
void GLC_StateBeginBloomDraw(texture_ref texture);
void GLC_StateBeginImageDraw(qbool is_text);
void GLC_StateBeginImageDrawNonGLSL(qbool is_text);
void GLC_StateBeginDrawAlphaPieSliceRGB(float thickness);

void GLC_Begin(GLenum primitive);
void GLC_End(void);
void GLC_Vertex2f(GLfloat x, GLfloat y);
void GLC_Vertex2fv(const GLfloat* v);
void GLC_Vertex3f(GLfloat x, GLfloat y, GLfloat z);
void GLC_Vertex3fv(const GLfloat* v);

unsigned int GLC_DrawIndexedPoly(glpoly_t* p, unsigned int* modelIndexes, unsigned int modelIndexMaximum, unsigned int index_count);

#endif
