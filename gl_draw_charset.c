/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: gl_draw.c,v 1.104 2007-10-18 05:28:23 dkure Exp $
*/

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "wad.h"
#include "stats_grid.h"
#include "utils.h"
#include "sbar.h"
#include "common_draw.h"
#ifndef __APPLE__
#include "tr_types.h"
#endif

extern float overall_alpha;

void GLM_DrawImage(float x, float y, float width, float height, int texture_unit, float tex_s, float tex_t, float tex_width, float tex_height, byte* color, qbool alpha);

static void Apply_OnChange_gl_smoothfont(int value);

void OnChange_gl_consolefont (cvar_t *, char *, qbool *);
static cvar_t gl_alphafont            = {"gl_alphafont", "1"};
void OnChange_gl_smoothfont (cvar_t *var, char *string, qbool *cancel);

cvar_t gl_consolefont  = {"gl_consolefont", "povo5", 0, OnChange_gl_consolefont};
cvar_t gl_smoothfont   = {"gl_smoothfont", "1", 0, OnChange_gl_smoothfont};
cvar_t scr_coloredText = {"scr_coloredText", "1"};

byte *draw_chars; // 8*8 graphic characters

int char_textures[MAX_CHARSETS];
int char_range[MAX_CHARSETS];

/*
* Load_LMP_Charset
*/
static int Load_LMP_Charset (char *name, int flags)
{
	int i;
	byte	buf[128*256];
	byte	*data;
	byte	*src, *dest;
	int texnum;
	int filesize;

	// We expect an .lmp to be in QPIC format, but it's ok if it's just raw data.
	if (!strcasecmp(name, "charset"))
	{
		// work around for original charset
		data = draw_chars;
		filesize = 128*128;
	}
	else
	{
		data = FS_LoadTempFile (va("gfx/%s.lmp", name), &filesize);	
	}

	if (!data)
		return 0;

	if (filesize == 128*128)
	{
		// Raw data.
	}
	else if (filesize == 128*128 + 8)
	{
		qpic_t *p = (qpic_t *)data;
		SwapPic (p);
		if (p->width != 128 || p->height != 128)
			return 0;
		data += 8;
	}
	else
	{
		return 0;
	}

	for (i=0 ; i < (256 * 64) ; i++)
	{
		if (data[i] == 0)
			data[i] = 255;	// Proper transparent color.
	}

	// Convert the 128*128 conchars texture to 128*256 leaving
	// empty space between rows so that chars don't stumble on
	// each other because of texture smoothing.
	// This hack costs us 64K of GL texture memory
	memset (buf, 255, sizeof(buf));
	src = data;
	dest = buf;

	for (i = 0 ; i < 16 ; i++)
	{
		memcpy (dest, src, 128*8);
		src += 128*8;
		dest += 128*8*2;
	}

	texnum = GL_LoadTexture (va("pic:%s", name), 128, 256, buf, flags, 1);
	return texnum;
}

/*
* Load_Locale_Charset
*/
static int Load_Locale_Charset (const char *name, const char *locale, unsigned int num, int range, int flags)
{
	char texture[1024], id[256], lmp[256], basename[MAX_QPATH];

	if (num >= MAX_CHARSETS)
		return 0;

	char_range[num] = 0;

	COM_StripExtension(name, basename, sizeof(basename));
	snprintf(texture, sizeof(texture), "textures/charsets/%s-%s", basename, locale);
	snprintf(id, sizeof(id), "pic:charset-%s", locale);
	snprintf(lmp, sizeof(lmp), "conchars-%s", locale);

	// try first 24 bit
	char_textures[num] = GL_LoadCharsetImage (texture, id, flags);
	// then 8 bit
	if (!char_textures[num])
		char_textures[num] = Load_LMP_Charset (lmp, flags);

	char_range[num] = char_textures[num] ? range : 0;

	return char_textures[num];
}

static int Draw_LoadCharset(const char *name)
{
	int flags = TEX_ALPHA | TEX_NOCOMPRESS | TEX_NOSCALE | TEX_NO_TEXTUREMODE;
	int texnum;
	int i = 0;
	qbool loaded = false;

	//
	// NOTE: we trying to not change char_textures[0] if we can't load charset.
	//		This way user still have some charset and can fix issue.
	//

	if (!strcasecmp(name, "original"))
	{
		if ((texnum = Load_LMP_Charset("charset", flags)))
		{
			char_textures[0] = texnum;
			loaded = true;
		}
	}
	else if ((texnum = GL_LoadCharsetImage(va("textures/charsets/%s", name), "pic:charset", flags)))
	{
		char_textures[0] = texnum;
		loaded = true;
	}

	if (!loaded)
	{
		Com_Printf ("Couldn't load charset \"%s\"\n", name);
		return 1;
	}

	// Load alternate charsets if available
	for (i = 1; i < MAX_CHARSETS; ++i)
	{
		char charsetName[10];
		int textureNumber;

		snprintf(charsetName, sizeof(charsetName), "%03d", i);

		textureNumber = Load_Locale_Charset(name, charsetName, i, i << 8, flags);

		// 2.2 only supported -cyr suffix
		if (i == 4 && !textureNumber)
			Load_Locale_Charset(name, "cyr", 4, 0x0400, flags);
	}

	// apply gl_smoothfont
	Apply_OnChange_gl_smoothfont(gl_smoothfont.integer);

	return 0;
}

void OnChange_gl_consolefont(cvar_t *var, char *string, qbool *cancel)
{
	*cancel = Draw_LoadCharset(string);
}

void Draw_LoadCharset_f (void)
{
	switch (Cmd_Argc())
	{
	case 1:
		Com_Printf("Current charset is \"%s\"\n", gl_consolefont.string);
		break;
	case 2:
		Cvar_Set(&gl_consolefont, Cmd_Argv(1));
		break;
	default:
		Com_Printf("Usage: %s <charset>\n", Cmd_Argv(0));
	}
}

void Draw_InitCharset(void)
{
	int i;

	memset(char_textures, 0, sizeof(char_textures));
	memset(char_range,    0, sizeof(char_range));

	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++)
	{
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;
	}

	Draw_LoadCharset(gl_consolefont.string);

	if (!char_textures[0])
		Cvar_Set(&gl_consolefont, "original");

	if (!char_textures[0])
		Sys_Error("Draw_InitCharset: Couldn't load charset");
}

// call it when gl_smoothfont changed or after we load charset
static void Apply_OnChange_gl_smoothfont(int value)
{
	int i;

	if (!char_textures[0])
		return;

	for (i = 0; i < MAX_CHARSETS; i++)
	{
		if (!char_textures[i])
			break;

		GL_Bind(char_textures[i]);
		if (value)
		{
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else
		{
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}
}

void OnChange_gl_smoothfont (cvar_t *var, char *string, qbool *cancel)
{
	Apply_OnChange_gl_smoothfont( Q_atoi(string) );
}

void Draw_Charset_Init(void)
{
	Cmd_AddCommand("loadcharset", Draw_LoadCharset_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&gl_smoothfont);
	Cvar_Register(&gl_consolefont);
	Cvar_Register(&gl_alphafont);
	Cvar_Register(&scr_coloredText);
	Cvar_ResetCurrentGroup();

	draw_chars = NULL;
}

qbool R_CharAvailable (wchar num)
{
	if (num == (num & 0xff)) {
		return true;
	}

	return (char_range[(num >> 8) & 0xff]);
}

#define CHARSET_CHARS_PER_ROW	16
#define CHARSET_WIDTH			1.0
#define CHARSET_HEIGHT			1.0
#define CHARSET_CHAR_WIDTH		(CHARSET_WIDTH / CHARSET_CHARS_PER_ROW)
#define CHARSET_CHAR_HEIGHT		(CHARSET_HEIGHT / CHARSET_CHARS_PER_ROW)

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
		glVertexAttribIPointer(0, 1, GL_INT, sizeof(gl_text_cache_t), (void*) 0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(gl_text_cache_t), (void*) (sizeof(int)));
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, sizeof(gl_text_cache_t), (void*) (sizeof(int) + sizeof(float) * 2));
		glVertexAttribPointer(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(gl_text_cache_t), (void*) (sizeof(int) + sizeof(float) * 3));
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
	}
}

void Draw_TextCacheFlush(void)
{
	extern cvar_t scr_coloredText;

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

				glActiveTexture(GL_TEXTURE0);
				GL_Bind(char_textures[cached_charset]);

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

	if (cache_pos >= GLM_STRING_CACHE || (cache_pos && cached_charset != new_charset)) {
		Draw_TextCacheFlush();
	}

	cache[cache_pos].character = ch & 0xFF;
	cache[cache_pos].x = x;
	cache[cache_pos].y = y;
	cache[cache_pos].scale = scale * 8;
	memcpy(&cache[cache_pos].color, cache_currentColor, sizeof(cache[cache_pos].color));
	cached_charset = new_charset;
	++cache_pos;
}

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// color				= Color!
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
void Draw_CharacterBase (int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange)
{
	float frow, fcol;
	int slot;
	int char_size = (bigchar ? 64 : 8);

	// Totally off screen.
	if (y <= (-char_size * scale))
		return;

	// Space.
	if (num == 32)
		return;

	if (GL_ShadersSupported()) {
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
		return;
	}

	// Only apply overall opacity if it's not fully opague.
	apply_overall_alpha = (apply_overall_alpha && (overall_alpha < 1.0));

	// Only change the GL state if we're told to. We keep track of the need for GL state changes
	// in the string drawing function instead. For character drawing functions we do this every time.
	// (For instance, only change color in a string when the actual color changes, instead of doing
	// it on each character always).
	if (gl_statechange) {
		// Turn on alpha transparency.
		if ((gl_alphafont.value || apply_overall_alpha)) {
			GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
		}
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);

		if (scr_coloredText.integer) {
			GL_TextureEnvMode(GL_MODULATE);
		}
		else {
			GL_TextureEnvMode(GL_REPLACE);
		}

		// Set the overall alpha.
		glColor4ub(color[0], color[1], color[2], color[3] * overall_alpha);
	}

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

	// Is this is a wchar, find a charset that has the char in it.
	slot = 0;
	if ((num & 0xFF00) != 0) {
		slot = ((num >> 8) & 0xFF);
		if (!char_range[slot]) {
			slot = 0;
			num = '?';
		}
	}

	num &= 0xFF;	// Only use the first byte.

					// Find the texture coordinates for the character.
	frow = (num >> 4) * CHARSET_CHAR_HEIGHT;	// row = num * (16 chars per row)
	fcol = (num & 0x0F) * CHARSET_CHAR_WIDTH;

	if (GL_ShadersSupported()) {
		glActiveTexture(GL_TEXTURE0);
		GL_Bind(char_textures[slot]);

		GLM_DrawImage(x, y, scale * 8, scale * 8 * 2, 0, fcol, frow, CHARSET_CHAR_WIDTH, CHARSET_CHAR_HEIGHT, color, false);
	}
	else {
		GL_Bind(char_textures[slot]);

		// Draw the character polygon.
		glBegin(GL_QUADS);
		{
			float scale8 = scale * 8;
			float scale8_2 = scale8 * 2;

			// Top left.
			glTexCoord2f(fcol, frow);
			glVertex2f(x, y);

			// Top right.
			glTexCoord2f(fcol + CHARSET_CHAR_WIDTH, frow);
			glVertex2f(x + scale8, y);

			// Bottom right.
			glTexCoord2f(fcol + CHARSET_CHAR_WIDTH, frow + CHARSET_CHAR_WIDTH);
			glVertex2f(x + scale8, y + scale8_2);

			// Bottom left.
			glTexCoord2f(fcol, frow + CHARSET_CHAR_WIDTH);
			glVertex2f(x, y + scale8_2);
		}
		glEnd();
	}
}

static void Draw_ResetCharGLState(void)
{
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_TextureEnvMode(GL_REPLACE);
	glColor4ubv(color_white);
	Draw_TextCacheSetColor(color_white);
}

void Draw_BigCharacter(int x, int y, char c, color_t color, float scale, float alpha)
{
	byte rgba[4];
	COLOR_TO_RGBA(color, rgba);
	Draw_CharacterBase(x, y, char2wc(c), scale, true, rgba, true, true);
	Draw_ResetCharGLState();
}

// Only ever called by the tracker
void Draw_SColoredCharacterW (int x, int y, wchar num, color_t color, float scale)
{
	byte rgba[4];
	COLOR_TO_RGBA(color, rgba);
	Draw_CharacterBase(x, y, num, scale, true, rgba, false, true);
	Draw_ResetCharGLState();
}

void Draw_SCharacter (int x, int y, int num, float scale)
{
	Draw_CharacterBase(x, y, char2wc(num), scale, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_SCharacterW (int x, int y, wchar num, float scale)
{
	Draw_CharacterBase(x, y, num, scale, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_CharacterW (int x, int y, wchar num)
{
	Draw_CharacterBase(x, y, num, 1, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_Character (int x, int y, int num)
{
	Draw_CharacterBase(x, y, char2wc(num), 1, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_SetColor(byte *rgba, float alpha)
{
	if (scr_coloredText.integer) {
		glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3] * alpha * overall_alpha);
		Draw_TextCacheSetColor(rgba);
	}
}

static void Draw_StringBase(int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale, float alpha, qbool bigchar, int char_gap)
{
	byte rgba[4];
	qbool color_is_white = true;
	int i, r, g, b;
	int curr_char;
	int color_index = 0;
	color_t last_color = COLOR_WHITE;

	// Nothing to draw.
	if (!*text)
		return;

	// Turn on alpha transparency.
	if (gl_alphafont.value || (overall_alpha < 1.0)) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	}
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);

	// Make sure we set the color from scratch so that the 
	// overall opacity is applied properly.
	memcpy(rgba, color_white, sizeof(byte) * 4);
	if (scr_coloredText.integer) {
		if (color_count > 0) {
			COLOR_TO_RGBA(color[color_index].c, rgba);
		}

		GL_TextureEnvMode(GL_MODULATE);
	}
	else {
		GL_TextureEnvMode(GL_REPLACE);
	}

	// Draw the string.
	if (GL_ShadersSupported()) {
		Draw_TextCacheInit(x, y, scale);
	}
	for (i = 0; text[i]; i++) {
		// If we didn't get a color array, check for color codes in the text instead.
		if (!color) {
			if (text[i] == '&') {
				if (text[i + 1] == 'c' && text[i + 2] && text[i + 3] && text[i + 4]) {
					r = HexToInt(text[i + 2]);
					g = HexToInt(text[i + 3]);
					b = HexToInt(text[i + 4]);

					if (r >= 0 && g >= 0 && b >= 0) {
						if (scr_coloredText.value) {
							rgba[0] = (r * 16);
							rgba[1] = (g * 16);
							rgba[2] = (b * 16);
							rgba[3] = 255;
							color_is_white = false;
						}

						color_count++; // Keep track on how many colors we're using.

						Draw_SetColor(rgba, alpha);

						i += 4;
						continue;
					}
				}
				else if (text[i + 1] == 'r') {
					if (!color_is_white) {
						memcpy(rgba, color_white, sizeof(byte) * 4);
						color_is_white = true;
						Draw_SetColor(rgba, alpha);
					}

					i++;
					continue;
				}
			}
		}
		else if (scr_coloredText.value && (color_index < color_count) && (i == color[color_index].i)) {
			// Change color if the color array tells us this index should have a new color.

			// Set the new color if it's not the same as the last.
			if (color[color_index].c != last_color) {
				last_color = color[color_index].c;
				COLOR_TO_RGBA(color[color_index].c, rgba);
				rgba[3] = 255;
				Draw_SetColor(rgba, alpha);
			}

			color_index++; // Goto next color.
		}
		curr_char = text[i];

		// Do not convert the character to red if we're applying color to the text.
		if (red && color_count <= 0) {
			curr_char |= 128;
		}

		// Draw the character but don't apply overall opacity, we've already done that
		// And don't update the glstate, we've done that also!
		Draw_CharacterBase(x, y, curr_char, scale, false, rgba, bigchar, false);

		x += ((bigchar ? 64 : 8) * scale) + char_gap;
	}
	Draw_ResetCharGLState();
}

void Draw_BigString (int x, int y, const char *text, clrinfo_t *color, int color_count, float scale, float alpha, int char_gap)
{
	Draw_StringBase(x, y, str2wcs(text), color, color_count, false, scale, alpha, true, char_gap);
}

void Draw_SColoredAlphaString (int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale, float alpha)
{
	Draw_StringBase(x, y, text, color, color_count, red, scale, alpha, false, 0);
}

void Draw_SColoredString (int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale)
{
	Draw_StringBase(x, y, text, color, color_count, red, scale, 1, false, 0);
}

void Draw_SString (int x, int y, const char *text, float scale)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, scale, 1, false, 0);
}

void Draw_SAlt_String (int x, int y, const char *text, float scale)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, true, scale, 1, false, 0);
}

// Only ever called by console, hence scale 1
void Draw_ColoredString3W(int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red)
{
	Draw_StringBase(x, y, text, color, color_count, red, 1, 1, false, 0);
}

void Draw_ColoredString3(int x, int y, const char *text, clrinfo_t *color, int color_count, int red)
{
	Draw_StringBase(x, y, str2wcs(text), color, color_count, red, 1, 1, false, 0);
}

void Draw_ColoredString(int x, int y, const char *text, int red)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, red, 1, 1, false, 0);
}

void Draw_SColoredStringBasic(int x, int y, const char *text, int red, float scale)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, red, scale, 1, false, 0);
}

void Draw_Alt_String(int x, int y, const char *text)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, true, 1, 1, false, 0);
}

void Draw_AlphaString(int x, int y, const char *text, float alpha)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, 1, alpha, false, 0);
}

void Draw_StringW (int x, int y, const wchar *text)
{
	Draw_StringBase(x, y, text, NULL, 0, false, 1, 1, false, 0);
}

void Draw_String (int x, int y, const char *text)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, 1, 1, false, 0);
}
