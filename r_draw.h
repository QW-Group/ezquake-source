
#ifndef EZQUAKE_R_DRAW_HEADER
#define EZQUAKE_R_DRAW_HEADER

void R_Draw_SAlphaSubPic2(int x, int y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha);
void R_Draw_AlphaPieSliceRGB(int x, int y, float radius, float startangle, float endangle, float thickness, qbool fill, color_t color);
void R_Draw_LineRGB(float thickness, byte* color, int x_start, int y_start, int x_end, int y_end);
void R_DrawImage(float x, float y, float width, float height, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha_test, texture_ref texnum, qbool isText, qbool isCrosshair);
void R_DrawAlphaRectangleRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor);
void R_Draw_FadeScreen(float alpha);
float R_Draw_CharacterBase(float x, float y, wchar num, float scale, qbool apply_overall_alpha, qbool bigchar, qbool gl_statechange, qbool proportional);
void R_Draw_ResetCharGLState(void);
void R_Draw_SetColor(byte* rgba);
void R_Draw_StringBase_StartString(int x, int y, float scale);
void R_Cache2DMatrix(void);
void R_UndoLastCharacter(void);
void R_Draw_Polygon(int x, int y, vec3_t *vertices, int num_vertices, color_t color);

#endif // EZQUAKE_R_DRAW_HEADER
