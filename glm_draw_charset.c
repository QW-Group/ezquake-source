
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

extern cvar_t gl_alphafont;
extern mpic_t char_textures[MAX_CHARSETS];
extern int char_range[MAX_CHARSETS];

// Modern: just cache as the string is printed, dump out as one.  Still pretty terrible
#define GLM_STRING_CACHE 4096
typedef struct gl_text_cache_s {
	int character;
	float x, y;
	float scale;
	GLubyte color[4];
} gl_text_cache_t;

static gl_text_cache_t cache[GLM_STRING_CACHE];
static GLubyte cache_currentColor[4];
static int cache_pos;
static qbool cached_bigchar;
static int cached_charset;
static float cache_original_x;
static float cache_x;
static float cache_y;
static float cache_scale;

static glm_program_t textStringProgram;
static GLint textString_modelViewMatrix;
static GLint textString_projectionMatrix;
static GLint textString_materialTex;
static GLint textString_bigFont;
static GLint textString_textureBase;
static GLint textString_fontSize;
static GLuint textStringVBO;
static GLuint textStringVAO;

void Draw_TextCacheInit(float x, float y, float scale)
{
	cache_original_x = x;
	cache_x = x;
	cache_y = y;

	if (cache_pos && (scale != cache_scale || memcmp(cache_currentColor, color_white, sizeof(cache_currentColor)))) {
		Draw_TextCacheFlush();
	}

	cache_scale = scale;
	memcpy(cache_currentColor, color_white, sizeof(cache_currentColor));
}

static GLuint Draw_CreateTextStringVAO(void)
{
	if (!textStringVBO) {
		glGenBuffers(1, &textStringVBO);
		glBindBufferExt(GL_ARRAY_BUFFER, textStringVBO);
		glBufferDataExt(GL_ARRAY_BUFFER, sizeof(cache), cache, GL_STATIC_DRAW);
	}

	if (!textStringVAO) {
		glGenVertexArrays(1, &textStringVAO);
		glBindVertexArray(textStringVAO);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		glEnableVertexAttribArray(2);
		glEnableVertexAttribArray(3);
		glBindBufferExt(GL_ARRAY_BUFFER, textStringVBO);
		glVertexAttribIPointer(0, 1, GL_INT, sizeof(gl_text_cache_t), (void*)0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(gl_text_cache_t), (void*)(sizeof(int)));
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(gl_text_cache_t), (void*)(sizeof(int) + sizeof(float) * 2));
		glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_text_cache_t), (void*)(sizeof(int) + sizeof(float) * 3));
	}

	return textStringVAO;
}

static void Draw_CreateTextStringProgram(void)
{
	if (!textStringProgram.program) {
		GL_VGFDeclare(text_string)

		GLM_CreateVGFProgram("StringDraw", GL_VGFParams(text_string), &textStringProgram);

		textString_modelViewMatrix = glGetUniformLocation(textStringProgram.program, "modelViewMatrix");
		textString_projectionMatrix = glGetUniformLocation(textStringProgram.program, "projectionMatrix");
		textString_materialTex = glGetUniformLocation(textStringProgram.program, "materialTex");
		textString_bigFont = glGetUniformLocation(textStringProgram.program, "bigFont");
		textString_fontSize = glGetUniformLocation(textStringProgram.program, "fontSize");
		textString_textureBase = glGetUniformLocation(textStringProgram.program, "textureBase");
	}
}

void Draw_TextCacheFlush(void)
{
	extern cvar_t scr_coloredText;

	if (!GL_ShadersSupported()) {
		return;
	}

	if (cache_pos) {
		// Draw text
		if (!textStringProgram.program) {
			Draw_CreateTextStringProgram();
		}

		if (textStringProgram.program) {
			GLuint vao = Draw_CreateTextStringVAO();

			{
				float modelViewMatrix[16];
				float projectionMatrix[16];
				float font_width = (char_textures[cached_charset].sh - char_textures[cached_charset].sl) / 16.0;
				float font_height = (char_textures[cached_charset].th - char_textures[cached_charset].tl) / 16.0;

				GLM_GetMatrix(GL_MODELVIEW, modelViewMatrix);
				GLM_GetMatrix(GL_PROJECTION, projectionMatrix);

				// Update VBO
				glBindBufferExt(GL_ARRAY_BUFFER, textStringVBO);
				glBufferDataExt(GL_ARRAY_BUFFER, sizeof(cache[0]) * cache_pos, cache, GL_STATIC_DRAW);

				if ((gl_alphafont.value/* || apply_overall_alpha*/)) {
					GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
				}
				GL_AlphaBlendFlags(GL_BLEND_ENABLED);

				if (scr_coloredText.integer) {
					GL_TextureEnvMode(GL_MODULATE);
				}
				else {
					GL_TextureEnvMode(GL_REPLACE);
				}

				// Call the program to draw the text string
				GL_UseProgram(textStringProgram.program);
				glUniformMatrix4fv(textString_modelViewMatrix, 1, GL_FALSE, modelViewMatrix);
				glUniformMatrix4fv(textString_projectionMatrix, 1, GL_FALSE, projectionMatrix);
				glUniform1i(textString_materialTex, 0);
				glUniform1i(textString_bigFont, cached_bigchar);
				glUniform2f(textString_fontSize, font_width, font_height);
				glUniform2f(textString_textureBase, char_textures[cached_charset].sl, char_textures[cached_charset].tl);

				glActiveTexture(GL_TEXTURE0);
				GL_Bind(char_textures[cached_charset].texnum);

				glBindVertexArray(vao);
				glDrawArrays(GL_POINTS, 0, cache_pos);
			}
		}
	}
	cache_pos = 0;
}

static void Draw_TextCacheSetColor(GLubyte* color)
{
	memcpy(cache_currentColor, color, sizeof(cache_currentColor));
}

static void Draw_TextCacheAdd(wchar ch, qbool bigchar)
{
	if (cache_pos >= GLM_STRING_CACHE) {
		Draw_TextCacheFlush();
	}

	cached_bigchar = bigchar;
	cache[cache_pos].character = ch & 0xFF;
	cache[cache_pos].x = cache_x;
	cache[cache_pos].y = cache_y;
	cache[cache_pos].scale = cache_scale * 8;
	memcpy(&cache[cache_pos].color, cache_currentColor, sizeof(cache[cache_pos].color));
	if (ch == 13) {
		cache_x = cache_original_x;
		cache_y += 8 * cache_scale;
	}
	else {
		cache_x += 8 * cache_scale;
	}
	++cache_pos;
}

static void Draw_TextCacheAddCharacter(float x, float y, wchar ch, float scale)
{
	int new_charset = (ch & 0xFF00) >> 8;
	int slot = 0;

	if ((ch & 0xFF00) != 0) {
		slot = ((ch >> 8) & 0xFF);
		if (!char_range[slot]) {
			slot = 0;
			ch = '?';
		}
	}

	ch &= 0xFF;	// Only use the first byte.

	/*if (cache_pos >= GLM_STRING_CACHE || (cache_pos && cached_charset != new_charset)) {
		Draw_TextCacheFlush();
	}

	cache[cache_pos].character = ch & 0xFF;
	cache[cache_pos].x = x;
	cache[cache_pos].y = y;
	cache[cache_pos].scale = scale * 8;
	memcpy(&cache[cache_pos].color, cache_currentColor, sizeof(cache[cache_pos].color));
	cached_charset = new_charset;
	++cache_pos;*/

	{
		float char_height = (char_textures[slot].th - char_textures[slot].tl) * CHARSET_CHAR_HEIGHT;
		float char_width = (char_textures[slot].sh - char_textures[slot].sl) * CHARSET_CHAR_WIDTH;
		float frow = char_textures[slot].tl + (ch >> 4) * char_height;	// row = num * (16 chars per row)
		float fcol = char_textures[slot].sl + (ch & 0x0F) * char_width;

		GLM_DrawImage(x, y, scale * 8, scale * 8 * 2, 0, fcol, frow, char_width, char_height, cache_currentColor, false, char_textures[cached_charset].texnum, true);
	}
}

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// color				= Color!
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
void GLM_Draw_CharacterBase(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange)
{
	int char_size = (bigchar ? 64 : 8);

	if (bigchar) {
		mpic_t *p = Draw_CachePicSafe(MCHARSET_PATH, false, true);

		if (p) {
			int sx = 0;
			int sy = 0;
			int char_width = (p->width / 8);
			int char_height = (p->height / 8);
			char c = (char)(num & 0xFF);

			Draw_GetBigfontSourceCoords(c, char_width, char_height, &sx, &sy);

			if (sx >= 0) {
				// Don't apply alpha here, since we already applied it above.
				Draw_SAlphaSubPic(x, y, p, sx, sy, char_width, char_height, (((float)char_size / char_width) * scale), 1);
			}

			return;
		}

		// TODO : Force players to have mcharset.png or fallback to overscaling normal font? :s
	}

	Draw_TextCacheAddCharacter(x, y, num, scale);
}

void GLM_Draw_ResetCharGLState(void)
{
	//GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	//GL_TextureEnvMode(GL_REPLACE);
	Draw_TextCacheSetColor(color_white);
}

void GLM_Draw_SetColor(byte* rgba, float alpha)
{
	extern cvar_t scr_coloredText;

	if (scr_coloredText.integer) {
		Draw_TextCacheSetColor(rgba);
	}
}

void GLM_Draw_StringBase_StartString(int x, int y, float scale)
{
	Draw_TextCacheInit(x, y, scale);
}
