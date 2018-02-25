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

static void Apply_OnChange_gl_smoothfont(int value);
static void OnChange_gl_consolefont(cvar_t *, char *, qbool *);
static void OnChange_gl_smoothfont(cvar_t *var, char *string, qbool *cancel);

cvar_t gl_alphafont    = { "gl_alphafont", "1" };
cvar_t gl_consolefont  = { "gl_consolefont", "povo5", 0, OnChange_gl_consolefont };
cvar_t gl_smoothfont   = { "gl_smoothfont", "1", 0, OnChange_gl_smoothfont };
cvar_t scr_coloredText = { "scr_coloredText", "1" };
cvar_t gl_charsets_min = { "gl_charsets_min", "1" };

static byte *draw_chars; // 8*8 graphic characters
mpic_t char_textures[MAX_CHARSETS];
int char_range[MAX_CHARSETS];

/*
* Load_LMP_Charset
*/
static qbool Load_LMP_Charset(char *name, int flags, mpic_t* pic)
{
	int i;
	byte	buf[128 * 256];
	byte	*data;
	byte	*src, *dest;
	int filesize;

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

	// Convert the 128*128 conchars texture to 128*256 leaving
	// empty space between rows so that chars don't stumble on
	// each other because of texture smoothing.
	// This hack costs us 64K of GL texture memory
	memset(buf, 255, sizeof(buf));
	src = data;
	dest = buf;

	for (i = 0; i < 16; i++) {
		memcpy(dest, src, 128 * 8);
		src += 128 * 8;
		dest += 128 * 8 * 2;
	}

	pic->texnum = GL_LoadTexture(va("pic:%s", name), 128, 256, buf, flags, 1);
	pic->width = 128;
	pic->height = 256;
	pic->sl = pic->tl = 0.0f;
	pic->sh = pic->th = 1.0f;
	return GL_TextureReferenceIsValid(pic->texnum);
}

/*
* Load_Locale_Charset
*/
static qbool Load_Locale_Charset(const char *name, const char *locale, unsigned int num, int range, int flags)
{
	char texture[1024], id[256], lmp[256], basename[MAX_QPATH];

	if (num >= MAX_CHARSETS) {
		return 0;
	}

	memset(&char_textures[num], 0, sizeof(char_textures[num]));

	COM_StripExtension(name, basename, sizeof(basename));
	snprintf(texture, sizeof(texture), "textures/charsets/%s-%s", basename, locale);
	snprintf(id, sizeof(id), "pic:charset-%s", locale);
	snprintf(lmp, sizeof(lmp), "conchars-%s", locale);

	// try first 24 bit, then 8 bit
	char_range[num] = 0;
	if (GL_LoadCharsetImage(texture, id, flags, &char_textures[num])) {
		char_range[num] = range;
	}
	else if (Load_LMP_Charset(lmp, flags, &char_textures[num])) {
		char_range[num] = range;
	}

	return GL_TextureReferenceIsValid(char_textures[num].texnum);
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
		Load_Locale_Charset(name, "cyr", 4, 0x0400, flags);
	}
	else {
		for (i = 1; i < MAX_CHARSETS; ++i) {
			char charsetName[10];

			snprintf(charsetName, sizeof(charsetName), "%03d", i);

			loaded = Load_Locale_Charset(name, charsetName, i, i << 8, flags);

			// 2.2 only supported -cyr suffix
			if (i == 4 && !loaded) {
				Load_Locale_Charset(name, "cyr", 4, 0x0400, flags);
			}
		}
	}

	// apply gl_smoothfont
	Apply_OnChange_gl_smoothfont(gl_smoothfont.integer);

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

// call it when gl_smoothfont changed or after we load charset
static void Apply_OnChange_gl_smoothfont(int value)
{
	int i;

	if (!GL_TextureReferenceIsValid(char_textures[0].texnum)) {
		return;
	}

	for (i = 0; i < MAX_CHARSETS; i++) {
		if (!GL_TextureReferenceIsValid(char_textures[i].texnum)) {
			break;
		}

		if (value) {
			GL_TexParameterf(GL_TEXTURE0, char_textures[i].texnum, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			GL_TexParameterf(GL_TEXTURE0, char_textures[i].texnum, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		}
		else {
			GL_TexParameterf(GL_TEXTURE0, char_textures[i].texnum, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
			GL_TexParameterf(GL_TEXTURE0, char_textures[i].texnum, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
		}
	}
}

static void OnChange_gl_smoothfont(cvar_t *var, char *string, qbool *cancel)
{
	Apply_OnChange_gl_smoothfont(Q_atoi(string));
}

// Called when textencoding, if we can't display a font it can replace with alternative
qbool R_CharAvailable(wchar num)
{
	if (num == (num & 0xff)) {
		return true;
	}

	return (char_range[(num >> 8) & 0xff]);
}

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// color				= Color!
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
void Draw_CharacterBase(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange)
{
	int char_size = (bigchar ? 64 : 8);

	// Totally off screen.
	if (y <= (-char_size * scale)) {
		return;
	}

	// Space.
	if (num == 32) {
		return;
	}

	GLM_Draw_CharacterBase(x, y, num, scale, apply_overall_alpha, color, bigchar, gl_statechange);
}

static void Draw_ResetCharGLState(void)
{
	GLM_Draw_ResetCharGLState();
}

void Draw_BigCharacter(int x, int y, char c, color_t color, float scale, float alpha)
{
	byte rgba[4];
	COLOR_TO_RGBA(color, rgba);
	Draw_CharacterBase(x, y, char2wc(c), scale, true, rgba, true, true);
	Draw_ResetCharGLState();
}

// Only ever called by the tracker
void Draw_SColoredCharacterW(int x, int y, wchar num, color_t color, float scale)
{
	byte rgba[4];
	COLOR_TO_RGBA(color, rgba);
	Draw_CharacterBase(x, y, num, scale, true, rgba, false, true);
	Draw_ResetCharGLState();
}

void Draw_SCharacter(int x, int y, int num, float scale)
{
	Draw_CharacterBase(x, y, char2wc(num), scale, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_SCharacterW(int x, int y, wchar num, float scale)
{
	Draw_CharacterBase(x, y, num, scale, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_CharacterW(int x, int y, wchar num)
{
	Draw_CharacterBase(x, y, num, 1, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_Character(int x, int y, int num)
{
	Draw_CharacterBase(x, y, char2wc(num), 1, true, color_white, false, true);
	Draw_ResetCharGLState();
}

void Draw_SetColor(byte *rgba, float alpha)
{
	GLM_Draw_SetColor(rgba, alpha);
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
	if (!*text) {
		return;
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

void Draw_BigString(int x, int y, const char *text, clrinfo_t *color, int color_count, float scale, float alpha, int char_gap)
{
	Draw_StringBase(x, y, str2wcs(text), color, color_count, false, scale, alpha, true, char_gap);
}

void Draw_SColoredAlphaString(int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale, float alpha)
{
	Draw_StringBase(x, y, text, color, color_count, red, scale, alpha, false, 0);
}

void Draw_SColoredString(int x, int y, const wchar *text, clrinfo_t *color, int color_count, int red, float scale)
{
	Draw_StringBase(x, y, text, color, color_count, red, scale, 1, false, 0);
}

void Draw_SString(int x, int y, const char *text, float scale)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, scale, 1, false, 0);
}

void Draw_SAlt_String(int x, int y, const char *text, float scale)
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

void Draw_StringW(int x, int y, const wchar *text)
{
	Draw_StringBase(x, y, text, NULL, 0, false, 1, 1, false, 0);
}

void Draw_String(int x, int y, const char *text)
{
	Draw_StringBase(x, y, str2wcs(text), NULL, 0, false, 1, 1, false, 0);
}

// Called during initialisation, before GL_Texture_Init
void Draw_Charset_Init(void)
{
	Cmd_AddCommand("loadcharset", Draw_LoadCharset_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register(&gl_smoothfont);
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
	memset(char_range, 0, sizeof(char_range));

	draw_chars = W_GetLumpName("conchars");
	for (i = 0; i < 256 * 64; i++) {
		if (draw_chars[i] == 0) {
			draw_chars[i] = 255;
		}
	}

	Draw_LoadCharset(gl_consolefont.string);

	if (!GL_TextureReferenceIsValid(char_textures[0].texnum)) {
		Cvar_Set(&gl_consolefont, "original");
	}

	if (!GL_TextureReferenceIsValid(char_textures[0].texnum)) {
		Sys_Error("Draw_InitCharset: Couldn't load charset");
	}
}

