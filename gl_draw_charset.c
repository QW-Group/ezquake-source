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
#include "fonts.h"

static void OnChange_gl_consolefont(cvar_t *, char *, qbool *);

cvar_t gl_alphafont    = { "gl_alphafont", "1" };
cvar_t gl_consolefont  = { "gl_consolefont", "povo5", 0, OnChange_gl_consolefont };
cvar_t scr_coloredText = { "scr_coloredText", "1" };
cvar_t gl_charsets_min = { "gl_charsets_min", "1" };

static byte *draw_chars; // 8*8 graphic characters

charset_t char_textures[MAX_CHARSETS];
int char_mapping[256]; // need to expand in future...

/*
* Load_LMP_Charset
*/
static qbool Load_LMP_Charset(char *name, int flags, charset_t* charset)
{
	int i;
	byte	buf[256 * 256];
	byte	*data;
	byte	*src, *dest;
	int filesize;
	texture_ref tex;

	// We expect an .lmp to be in QPIC format, but it's ok if it's just raw data.
	if (!strcasecmp(name, "charset")) {
		// work around for original charset
		data = draw_chars;
		filesize = 128 * 128;
	}
	else {
		data = FS_LoadTempFile(va("gfx/%s.lmp", name), &filesize);
	}

	if (!data)
		return 0;

	if (filesize == 128 * 128) {
		// Raw data.
	}
	else if (filesize == 128 * 128 + 8) {
		qpic_t *p = (qpic_t *)data;
		SwapPic(p);
		if (p->width != 128 || p->height != 128)
			return 0;
		data += 8;
	}
	else {
		return 0;
	}

	for (i = 0; i < (256 * 64); i++) {
		if (data[i] == 0) {
			data[i] = 255;	// Proper transparent color.
		}
	}

	// Convert the 128*128 conchars texture to 256*256 leaving
	// empty space between rows so that chars don't stumble on
	// each other because of texture smoothing.
	memset(buf, 255, sizeof(buf));
	src = data;
	dest = buf;

	for (i = 0; i < 128; i++) {
		int j;

		if (i % 8 == 0 && i) {
			dest += 256 * 8;
		}
		for (j = 0; j < 16; ++j) {
			memcpy(dest, src, 8);
			src += 8;
			dest += 8 * 2;
		}
	}

	tex = GL_LoadTexture(va("pic:%s", name), 256, 256, buf, flags, 1);
	if (GL_TextureReferenceIsValid(tex)) {
		for (i = 0; i < 256; ++i) {
			charset->glyphs[i].texnum = tex;
			charset->glyphs[i].width = 128 / 16;
			charset->glyphs[i].height = 128 / 16;
			charset->glyphs[i].sl = 2 * (i & 0x0F) * (1.0f / 32);
			charset->glyphs[i].sh = charset->glyphs[i].sl + 8.0f / 256;
			charset->glyphs[i].tl = 2 * (i / 0x10) * (1.0f / 32);
			charset->glyphs[i].th = charset->glyphs[i].tl + 8.0f / 256;
		}

		return true;
	}
	return false;
}

/*
* Load_Locale_Charset
*/
static qbool Load_Locale_Charset(const char *name, const char *locale, unsigned int num, int flags)
{
	char texture[1024], id[256], lmp[256], basename[MAX_QPATH];
	int range = num << 8;

	if (num >= MAX_CHARSETS) {
		return 0;
	}

	memset(&char_textures[num], 0, sizeof(char_textures[num]));

	COM_StripExtension(name, basename, sizeof(basename));
	snprintf(texture, sizeof(texture), "textures/charsets/%s-%s", basename, locale);
	snprintf(id, sizeof(id), "pic:charset-%s", locale);
	snprintf(lmp, sizeof(lmp), "conchars-%s", locale);

	// try first 24 bit, then 8 bit
	char_mapping[num] = 0;
	if (GL_LoadCharsetImage(texture, id, flags, &char_textures[num])) {
		char_mapping[num] = num;
	}
	else if (Load_LMP_Charset(lmp, flags, &char_textures[num])) {
		char_mapping[num] = num;
	}

	return char_mapping[num];
}

static int Draw_LoadCharset(const char *name)
{
	int flags = TEX_ALPHA | TEX_NOCOMPRESS | TEX_NOSCALE | TEX_NO_TEXTUREMODE;
	int i = 0;
	qbool loaded = false;

	//
	// NOTE: we trying to not change char_textures[0] if we can't load charset.
	//		This way user still have some charset and can fix issue.
	//
	if (!strcasecmp(name, "original")) {
		loaded = Load_LMP_Charset("charset", flags, &char_textures[0]);
	}
	else {
		loaded = GL_LoadCharsetImage(va("textures/charsets/%s", name), "pic:charset", flags, &char_textures[0]);
	}

	if (!loaded) {
		Com_Printf("Couldn't load charset \"%s\"\n", name);
		return 1;
	}

	// Load alternate charsets if available
	if (gl_charsets_min.integer) {
		Load_Locale_Charset(name, "cyr", 4, flags);
	}
	else {
		for (i = 1; i < MAX_CHARSETS; ++i) {
			char charsetName[10];

			snprintf(charsetName, sizeof(charsetName), "%03d", i);

			loaded = Load_Locale_Charset(name, charsetName, i, flags);

			// 2.2 only supported -cyr suffix
			if (i == 4 && !loaded) {
				Load_Locale_Charset(name, "cyr", 4, flags);
			}
		}
	}

	CachePics_MarkAtlasDirty();

	return 0;
}

static void OnChange_gl_consolefont(cvar_t *var, char *string, qbool *cancel)
{
	*cancel = Draw_LoadCharset(string);
}

static void Draw_LoadCharset_f(void)
{
	switch (Cmd_Argc()) {
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

// Called when textencoding, if we can't display a font it can replace with alternative
qbool R_CharAvailable(wchar num)
{
	if (num == (num & 0xff)) {
		return true;
	}

	return (char_mapping[(num >> 8) & 0xff]);
}

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// color				= Color!
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
static int Draw_CharacterBaseW(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange, qbool proportional)
{
	int original_x = x;

	if (FontAlterCharCoords(&x, &y, num, bigchar, scale, proportional)) {
		return GLM_Draw_CharacterBase(x, y, num, scale, apply_overall_alpha, color, bigchar, gl_statechange, proportional);
	}
	return x - original_x;
}

static void Draw_ResetCharGLState(void)
{
	GLM_Draw_ResetCharGLState();
}

void Draw_SCharacter(int x, int y, int num, float scale)
{
	Draw_CharacterBaseW(x, y, char2wc(num), scale, true, color_white, false, true, false);
	Draw_ResetCharGLState();
}

int Draw_SCharacterP(int x, int y, int num, float scale, qbool proportional)
{
	int new_x = Draw_CharacterBaseW(x, y, char2wc(num), scale, true, color_white, false, true, proportional);
	Draw_ResetCharGLState();
	return new_x;
}

void Draw_SCharacterW(int x, int y, wchar num, float scale)
{
	Draw_CharacterBaseW(x, y, num, scale, true, color_white, false, true, false);
	Draw_ResetCharGLState();
}

void Draw_CharacterW(int x, int y, wchar num)
{
	Draw_CharacterBaseW(x, y, num, 1, true, color_white, false, true, false);
	Draw_ResetCharGLState();
}

void Draw_Character(int x, int y, int num)
{
	Draw_CharacterBaseW(x, y, char2wc(num), 1, true, color_white, false, true, false);
	Draw_ResetCharGLState();
}

void Draw_SetColor(byte *rgba)
{
	GLM_Draw_SetColor(rgba);
}

static int Draw_StringBase(int x, int y, const char *text, clrinfo_t *color, int color_count, int red, float scale, float alpha, qbool bigchar, int char_gap, qbool proportional)
{
	byte rgba[4];
	qbool color_is_white = true;
	int i, r, g, b;
	int curr_char;
	int color_index = 0;
	color_t last_color = COLOR_WHITE;
	int original_x = x;

	// Nothing to draw.
	if (!*text) {
		return 0;
	}

	// Make sure we set the color from scratch so that the 
	// overall opacity is applied properly.
	memcpy(rgba, color_white, sizeof(byte) * 4);
	if (scr_coloredText.integer && color_count > 0) {
		COLOR_TO_RGBA(color[color_index].c, rgba);
	}

	// Draw the string.
	GLM_Draw_StringBase_StartString(x, y, scale);
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

						rgba[3] = 255 * alpha;
						Draw_SetColor(rgba);

						i += 4;
						continue;
					}
				}
				else if (text[i + 1] == 'r') {
					if (!color_is_white) {
						rgba[0] = rgba[1] = rgba[2] = 255;
						rgba[3] = 255 * alpha;
						color_is_white = true;
						Draw_SetColor(rgba);
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
				Draw_SetColor(rgba);
			}

			color_index++; // Goto next color.
		}
		curr_char = (unsigned char)text[i];

		// Do not convert the character to red if we're applying color to the text.
		if (red && color_count <= 0) {
			curr_char |= 128;
		}

		// Draw the character but don't apply overall opacity, we've already done that
		// And don't update the glstate, we've done that also!
		x += Draw_CharacterBaseW(x, y, curr_char, scale, false, rgba, bigchar, false, proportional);
	}
	Draw_ResetCharGLState();
	return x - original_x;
}

int Draw_BigString(int x, int y, const char *text, clrinfo_t *color, int color_count, float scale, float alpha, int char_gap)
{
	return Draw_StringBase(x, y, text, color, color_count, false, scale, alpha, true, char_gap, false);
}

// TODO: proportional
int Draw_SColoredAlphaString(int x, int y, const char *text, clrinfo_t *color, int color_count, int red, float scale, float alpha)
{
	return Draw_StringBase(x, y, text, color, color_count, red, scale, alpha, false, 0, false);
}

int Draw_SString(int x, int y, const char *text, float scale, qbool proportional)
{
	return Draw_StringBase(x, y, text, NULL, 0, false, scale, 1, false, 0, proportional);
}

int Draw_SAlt_String(int x, int y, const char *text, float scale, qbool proportional)
{
	return Draw_StringBase(x, y, text, NULL, 0, true, scale, 1, false, 0, proportional);
}

int Draw_ConsoleString(int x, int y, const wchar *text, clrinfo_t *color, int text_length, int red, float scale, qbool proportional)
{
	float alpha = 1;
	qbool bigchar = false;
	int char_gap = 0;
	byte rgba[4];
	qbool color_is_white = true;
	int i, r, g, b;
	int curr_char;
	int color_index = 0;
	color_t last_color = COLOR_WHITE;
	int color_count = color ? text_length : 0;
	int original_x = x;

	// Nothing to draw.
	if (!*text) {
		return 0;
	}

	// Make sure we set the color from scratch so that the 
	// overall opacity is applied properly.
	memcpy(rgba, color_white, sizeof(byte) * 4);
	if (scr_coloredText.integer && color_count > 0) {
		COLOR_TO_RGBA(color[color_index].c, rgba);
	}

	// Draw the string.
	GLM_Draw_StringBase_StartString(x, y, scale);
	for (i = 0; (text_length ? i < text_length : text[i]); i++) {
		// If we didn't get a color array, check for color codes in the text instead.
		if (!color) {
			if (text[i] == '&') {
				if ((text_length == 0 || i + 4 < text_length) && text[i + 1] == 'c' && text[i + 2] && text[i + 3] && text[i + 4]) {
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

						rgba[3] = 255 * alpha;
						Draw_SetColor(rgba);

						i += 4;
						continue;
					}
				}
				else if ((text_length == 0 || i + 1 < text_length) && text[i + 1] == 'r') {
					if (!color_is_white) {
						rgba[0] = rgba[1] = rgba[2] = 255;
						rgba[3] = 255 * alpha;
						color_is_white = true;
						Draw_SetColor(rgba);
					}

					i++;
					continue;
				}
			}
		}
		else if (scr_coloredText.value) {
			// Change color if the color array tells us this index should have a new color.

			// Set the new color if it's not the same as the last.
			if (color[color_index].c != last_color) {
				last_color = color[color_index].c;
				COLOR_TO_RGBA(color[color_index].c, rgba);
				rgba[3] = 255;
				Draw_SetColor(rgba);
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
		x += Draw_CharacterBaseW(x, y, curr_char, scale, false, rgba, bigchar, false, proportional);
	}
	Draw_ResetCharGLState();

	return x - original_x;
}

// TODO: proportional
int Draw_ColoredString3(int x, int y, const char *text, clrinfo_t *color, int color_count, int red)
{
	return Draw_StringBase(x, y, text, color, color_count, red, 1, 1, false, 0, false);
}

int Draw_ColoredString(int x, int y, const char *text, int red, qbool proportional)
{
	return Draw_StringBase(x, y, text, NULL, 0, red, 1, 1, false, 0, proportional);
}

// TODO: proportional
int Draw_SColoredStringBasic(int x, int y, const char *text, int red, float scale)
{
	return Draw_StringBase(x, y, text, NULL, 0, red, scale, 1, false, 0, false);
}

// FIXME: Replace with Draw_ColoredString()
int Draw_Alt_String(int x, int y, const char *text, float scale, qbool proportional)
{
	return Draw_StringBase(x, y, text, NULL, 0, true, scale, 1, false, 0, proportional);
}

// TODO: proportional
int Draw_AlphaString(int x, int y, const char *text, float alpha)
{
	return Draw_StringBase(x, y, text, NULL, 0, false, 1, alpha, false, 0, false);
}

// TODO: proportional
int Draw_String(int x, int y, const char *text)
{
	return Draw_StringBase(x, y, text, NULL, 0, false, 1, 1, false, 0, false);
}

int Draw_StringLength(const char *text, int length, float scale, qbool proportional)
{
	if (!proportional) {
		if (length < 0) {
			length = strlen(text);
		}
		return length * scale * 8;
	}
	else {
		int i;
		float x = 0;

		for (i = 0; text[i] && (length == -1 || i < length); i++) {
			x += FontCharacterWidth(text[i], proportional) * scale;
		}

		return (int)(x + 0.5f);
	}
}

int Draw_StringLengthColors(const char *text, int length, float scale, qbool proportional)
{
	int i;
	int x = 0, y = 0;

	if (!proportional) {
		if (length < 0) {
			length = strlen_color(text);
		}
		return length * scale * 8;
	}
	else {
		for (i = 0; text[i] && (length == -1 || i < length); i++) {
			if (text[i] == '&') {
				if (text[i + 1] == 'c' && HexToInt(text[i + 2]) >= 0 && HexToInt(text[i + 3]) >= 0 && HexToInt(text[i + 4]) >= 0) {
					i += 4; // skip "&cRGB"
					continue;
				}
				else if (text[i + 1] == 'r') {
					i += 1; // skip "&r"
					continue;
				}
			}

			x += FontCharacterWidthWide(text[i]) * scale;
		}
	}

	return x;
}

int Draw_CharacterFit(const char* text, int width, float scale, qbool proportional)
{
	int fit = 0;

	if (!proportional) {
		return width / (8 * scale);
	}

	while (*text) {
		int next_char_width = FontCharacterWidthWide(*text) * scale;
		if (next_char_width > width) {
			break;
		}

		width -= next_char_width;
		++fit;
		++text;
	}

	return fit;
}

void Draw_LoadFont_f(void)
{
	if (Cmd_Argc() > 1) {
		FontCreate(0, Cmd_Argv(1));
	}
}

// Called during initialisation, before GL_Texture_Init
void Draw_Charset_Init(void)
{
	Cmd_AddCommand("loadcharset", Draw_LoadCharset_f);
	Cmd_AddCommand("loadfont", Draw_LoadFont_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&gl_consolefont);
	Cvar_Register(&gl_alphafont);
	Cvar_Register(&gl_charsets_min);
	Cvar_Register(&scr_coloredText);
	Cvar_ResetCurrentGroup();

	draw_chars = NULL;
}

// Called during initialisation, _after_ GL_Texture_Init, wad file loaded etc
void Draw_InitCharset(void)
{
	int i;

	memset(char_textures, 0, sizeof(char_textures));
	memset(char_mapping, 0, sizeof(char_mapping));

	draw_chars = W_GetLumpName("conchars");
	for (i = 0; i < 256 * 64; i++) {
		if (draw_chars[i] == 0) {
			draw_chars[i] = 255;
		}
	}

	Draw_LoadCharset(gl_consolefont.string);

	if (!GL_TextureReferenceIsValid(char_textures[0].glyphs[0].texnum)) {
		Cvar_Set(&gl_consolefont, "original");
	}

	if (!GL_TextureReferenceIsValid(char_textures[0].glyphs[0].texnum)) {
		Sys_Error("Draw_InitCharset: Couldn't load charset");
	}
}
