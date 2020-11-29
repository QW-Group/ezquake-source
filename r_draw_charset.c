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
#include "wad.h"
#include "stats_grid.h"
#include "utils.h"
#include "sbar.h"
#include "common_draw.h"
#include "fonts.h"
#include "r_texture.h"
#include "r_draw.h"
#include "r_trace.h"

static void OnChange_gl_consolefont(cvar_t *, char *, qbool *);
void Draw_InitFont(void);

cvar_t gl_alphafont    = { "gl_alphafont", "1" };
cvar_t gl_consolefont  = { "gl_consolefont", "povo5", 0, OnChange_gl_consolefont };
cvar_t scr_coloredText = { "scr_coloredText", "1" };
cvar_t gl_charsets_min = { "gl_charsets_min", "1" };

static byte *draw_chars; // 8*8 graphic characters

charset_t char_textures[MAX_CHARSETS];
int char_mapping[256]; // need to expand in future...

static byte cache_currentColor[4];
static qbool nextCharacterIsCrosshair = false;

static const byte color_white[4] = { 255, 255, 255, 255 };

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

	tex = R_LoadTexture(va("pic:%s", name), 256, 256, buf, flags, 1);
	if (R_TextureReferenceIsValid(tex)) {
		for (i = 0; i < 256; ++i) {
			charset->glyphs[i].texnum = tex;
			charset->glyphs[i].width = 128 / 16;
			charset->glyphs[i].height = 128 / 16;
			charset->glyphs[i].sl = 2 * (i & 0x0F) * (1.0f / 32);
			charset->glyphs[i].sh = charset->glyphs[i].sl + 8.0f / 256;
			charset->glyphs[i].tl = 2 * (i / 0x10) * (1.0f / 32);
			charset->glyphs[i].th = charset->glyphs[i].tl + 8.0f / 256;
		}

		charset->master = tex;
		charset->custom_scale_x = charset->custom_scale_y = 1;

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

	if (num >= MAX_CHARSETS) {
		return 0;
	}

	R_TraceEnterFunctionRegion;
	memset(&char_textures[num], 0, sizeof(char_textures[num]));

	COM_StripExtension(name, basename, sizeof(basename));
	snprintf(texture, sizeof(texture), "textures/charsets/%s-%s", basename, locale);
	snprintf(id, sizeof(id), "pic:charset-%s", locale);
	snprintf(lmp, sizeof(lmp), "conchars-%s", locale);

	// try first 24 bit, then 8 bit
	char_mapping[num] = 0;
	if (R_LoadCharsetImage(texture, id, flags, &char_textures[num])) {
		char_mapping[num] = num;
	}
	else if (Load_LMP_Charset(lmp, flags, &char_textures[num])) {
		char_mapping[num] = num;
	}
	R_TraceLeaveFunctionRegion;

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
		loaded = R_LoadCharsetImage(va("textures/charsets/%s", name), "pic:charset", flags, &char_textures[0]);
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
		for (i = 1; i < MAX_USER_CHARSETS; ++i) {
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
static float Draw_CharacterBaseW(float x, float y, wchar num, float scale, qbool apply_overall_alpha, qbool bigchar, qbool gl_statechange, qbool proportional)
{
	int original_x = x;

	if (FontAlterCharCoordsWide(&x, &y, num, bigchar, scale, proportional)) {
		return R_Draw_CharacterBase(x, y, num, scale, apply_overall_alpha, bigchar, gl_statechange, proportional);
	}
	return x - original_x;
}

static void Draw_ResetCharGLState(void)
{
	R_Draw_ResetCharGLState();
}

void Draw_SCharacter(float x, float y, int num, float scale)
{
	Draw_CharacterBaseW(x, y, char2wc(num), scale, true, false, true, false);
	Draw_ResetCharGLState();
}

float Draw_SCharacterP(float x, float y, int num, float scale, qbool proportional)
{
	float new_x = Draw_CharacterBaseW(x, y, char2wc(num), scale, true, false, true, proportional);
	Draw_ResetCharGLState();
	return new_x;
}

float Draw_CharacterWSP(float x, float y, wchar num, float scale, qbool proportional)
{
	float new_x = Draw_CharacterBaseW(x, y, num, scale, true, false, true, proportional);
	Draw_ResetCharGLState();
	return new_x;
}

void Draw_CharacterW(float x, float y, wchar num)
{
	Draw_CharacterBaseW(x, y, num, 1, true, false, true, false);
	Draw_ResetCharGLState();
}

void Draw_Character(float x, float y, int num)
{
	Draw_CharacterBaseW(x, y, char2wc(num), 1, true, false, true, false);
	Draw_ResetCharGLState();
}

void Draw_SetColor(byte *rgba)
{
	R_Draw_SetColor(rgba);
}

static float Draw_StringBase(float x, float y, const char *text, clrinfo_t *color, int color_count, int red, float scale, float alpha, qbool bigchar, int char_gap, qbool proportional, float max_x)
{
	byte rgba[4];
	qbool color_is_white = true;
	int i, r, g, b;
	int curr_char;
	int color_index = 0;
	color_t last_color = COLOR_WHITE;
	float original_x = x;
	float char_width = 0;

	// Nothing to draw.
	if (!*text) {
		return 0;
	}

	// Make sure we set the color from scratch so that the 
	// overall opacity is applied properly.
	rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255;
	if (scr_coloredText.integer && color_count > 0) {
		COLOR_TO_RGBA(color[0].c, rgba);
	}

	// Draw the string.
	R_Draw_StringBase_StartString(x, y, scale);
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
				rgba[3] = 255 * alpha;
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
		char_width = Draw_CharacterBaseW(x, y, curr_char, scale, false, bigchar, false, proportional);

		if (max_x && x + char_width > max_x) {
			R_UndoLastCharacter();
			break;
		}

		x += char_width;
	}
	Draw_ResetCharGLState();
	return ceil(x - original_x);
}

void Draw_BigString(float x, float y, const char *text, clrinfo_t *color, int color_count, float scale, float alpha, int char_gap)
{
	Draw_StringBase(x, y, text, color, color_count, false, scale, alpha, true, char_gap, false, 0);
}

float Draw_SColoredAlphaString(float x, float y, const char *text, clrinfo_t *color, int color_count, int red, float scale, float alpha, qbool proportional)
{
	return Draw_StringBase(x, y, text, color, color_count, red, scale, alpha, false, 0, proportional, 0);
}

void Draw_SColoredStringAligned(int x, int y, const char *text, clrinfo_t* color, int color_count, float scale, float alpha, qbool proportional, text_alignment_t align, float max_x)
{
	int initial_position, final_position;
	float width;

	initial_position = Draw_ImagePosition();
	width = Draw_StringBase(x, y, text, color, color_count, false, scale, alpha, false, 0, proportional, max_x);
	final_position = Draw_ImagePosition();

	if (align == text_align_center) {
		Draw_AdjustImages(initial_position, final_position, (max_x - x - width) / 2);
	}
	else if (align == text_align_right) {
		Draw_AdjustImages(initial_position, final_position, max_x - x - width);
	}
}

void Draw_SStringAligned(int x, int y, const char *text, float scale, float alpha, qbool proportional, text_alignment_t align, float max_x)
{
	int initial_position, final_position;
	float width;

	initial_position = Draw_ImagePosition();
	width = Draw_StringBase(x, y, text, NULL, 0, false, scale, alpha, false, 0, proportional, max_x);
	final_position = Draw_ImagePosition();

	if (align == text_align_center) {
		Draw_AdjustImages(initial_position, final_position, (max_x - x - width) / 2);
	}
	else if (align == text_align_right) {
		Draw_AdjustImages(initial_position, final_position, max_x - x - width);
	}
}

float Draw_SString(float x, float y, const char *text, float scale, qbool proportional)
{
	return Draw_StringBase(x, y, text, NULL, 0, false, scale, 1, false, 0, proportional, 0);
}

float Draw_SStringAlpha(int x, int y, const char* text, float scale, float alpha, qbool proportional)
{
	return Draw_StringBase(x, y, text, NULL, 0, false, scale, alpha, false, 0, proportional, 0);
}

void Draw_SAlt_String(float x, float y, const char *text, float scale, qbool proportional)
{
	Draw_StringBase(x, y, text, NULL, 0, true, scale, 1, false, 0, proportional, 0);
}

int Draw_ConsoleString(float x, float y, const wchar *text, clrinfo_t *color, int text_length, int red, float scale, qbool proportional)
{
	const qbool bigchar = false;
	byte rgba[4];
	qbool color_is_white = true;
	int i, r, g, b;
	int curr_char;
	int color_index = 0;
	color_t last_color = COLOR_WHITE;
	int color_count = color ? text_length : 0;

	// Nothing to draw.
	if (!*text) {
		return 0;
	}

	// Make sure we set the color from scratch so that the 
	// overall opacity is applied properly.
	rgba[0] = rgba[1] = rgba[2] = rgba[3] = 255;
	if (scr_coloredText.integer && color_count > 0) {
		COLOR_TO_RGBA(color[color_index].c, rgba);
	}

	// Draw the string.
	R_Draw_StringBase_StartString(x, y, scale);
	for (i = 0; text[i] != 0 && (text_length <= 0 || i < text_length); i++) {
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

						rgba[3] = 255;
						Draw_SetColor(rgba);

						i += 4;
						continue;
					}
				}
				else if ((text_length == 0 || i + 1 < text_length) && text[i + 1] == 'r') {
					if (!color_is_white) {
						rgba[0] = rgba[1] = rgba[2] = 255;
						rgba[3] = 255;
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
		x += Draw_CharacterBaseW(x, y, curr_char, scale, false, bigchar, false, proportional);
	}
	Draw_ResetCharGLState();

	return i;
}

void Draw_ColoredString3(float x, float y, const char *text, clrinfo_t *color, int color_count, int red)
{
	Draw_StringBase(x, y, text, color, color_count, red, 1, 1, false, 0, false, 0);
}

void Draw_ColoredString(float x, float y, const char *text, int red, qbool proportional)
{
	Draw_StringBase(x, y, text, NULL, 0, red, 1, 1, false, 0, proportional, 0);
}

void Draw_SColoredStringBasic(float x, float y, const char *text, int red, float scale, qbool proportional)
{
	Draw_StringBase(x, y, text, NULL, 0, red, scale, 1, false, 0, proportional, 0);
}

// FIXME: Replace with Draw_ColoredString()
void Draw_Alt_String(float x, float y, const char *text, float scale, qbool proportional)
{
	Draw_StringBase(x, y, text, NULL, 0, true, scale, 1, false, 0, proportional, 0);
}

// TODO: proportional
void Draw_AlphaString(float x, float y, const char *text, float alpha)
{
	Draw_StringBase(x, y, text, NULL, 0, false, 1, alpha, false, 0, false, 0);
}

void Draw_String(float x, float y, const char *text)
{
	Draw_StringBase(x, y, text, NULL, 0, false, 1, 1, false, 0, false, 0);
}

float Draw_StringLengthW(const wchar *text, int length, float scale, qbool proportional)
{
	int i;
	float x = 0;

	for (i = 0; text[i] && (length == -1 || i < length); i++) {
		x += FontCharacterWidthWide(text[i], scale, proportional);
	}

	return x;
}

float Draw_StringLength(const char *text, int length, float scale, qbool proportional)
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
			x += FontCharacterWidth(text[i], scale, proportional);
		}

		return x;
	}
}

float Draw_StringLengthColors(const char *text, int length, float scale, qbool proportional)
{
	return Draw_StringLengthColorsByTerminator(text, length, scale, proportional, 0);
}

float Draw_StringLengthColorsByTerminator(const char *text, int length, float scale, qbool proportional, char terminator)
{
	if (!proportional) {
		if (length < 0) {
			length = strlen_color_by_terminator(text, terminator);
		}
		return length * scale * 8;
	}
	else {
		int i;
		float x = 0;

		for (i = 0; text[i] && text[i] != terminator && (length == -1 || i < length); i++) {
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

			x += FontCharacterWidthWide(text[i], scale, proportional);
		}

		return x;
	}
}

int Draw_CharacterFit(const char* text, float width, float scale, qbool proportional)
{
	int fit = 0;

	if (!proportional) {
		return width / (8 * scale);
	}

	while (*text) {
		float next_char_width = FontCharacterWidthWide(*text, scale, proportional);
		if (next_char_width > width) {
			break;
		}

		width -= next_char_width;
		++fit;
		++text;
	}

	return fit;
}

// Called during initialisation, before R_Texture_Init
void Draw_Charset_Init(void)
{
	if (!host_initialized) {
		Cmd_AddCommand("loadcharset", Draw_LoadCharset_f);
#ifdef EZ_FREETYPE_SUPPORT
		FontInitialise();
#endif

		Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
		Cvar_Register(&gl_consolefont);
		Cvar_Register(&gl_alphafont);
		Cvar_Register(&gl_charsets_min);
		Cvar_Register(&scr_coloredText);
		Cvar_ResetCurrentGroup();
	}

	draw_chars = NULL;
}

// Called during initialisation, _after_ R_Texture_Init, wad file loaded etc
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

	if (!R_TextureReferenceIsValid(char_textures[0].glyphs[0].texnum)) {
		Cvar_Set(&gl_consolefont, "original");
	}

	if (!R_TextureReferenceIsValid(char_textures[0].glyphs[0].texnum)) {
		Sys_Error("Draw_InitCharset: Couldn't load charset");
	}

	Draw_InitFont();
}

void Draw_SetCrosshairTextMode(qbool enabled)
{
	nextCharacterIsCrosshair = enabled;
}

static void Draw_TextCacheInit(float x, float y, float scale)
{
	memcpy(cache_currentColor, color_white, sizeof(cache_currentColor));
}

static void Draw_TextCacheSetColor(const byte* color)
{
	memcpy(cache_currentColor, color, sizeof(cache_currentColor));
}

static float Draw_TextCacheAddCharacter(float x, float y, wchar ch, float scale, qbool proportional)
{
	int new_charset = (ch >> 8) & 0xFF;
	charset_t* texture = &char_textures[0];
	mpic_t* pic;

#ifdef EZ_FREETYPE_SUPPORT
	if (proportional) {
		extern charset_t proportional_fonts[MAX_CHARSETS];

		proportional &= R_TextureReferenceIsValid(proportional_fonts[new_charset].glyphs[ch & 0xFF].texnum);
		if (proportional) {
			texture = &proportional_fonts[new_charset];
		}
	}
#else
	proportional = false;
#endif

	if (!proportional) {
		int slot = new_charset;

		if (slot) {
			if (slot < 0xE0 && !char_mapping[slot]) {
				slot = 0;
				ch = '?';
			}
		}

		texture = &char_textures[slot];
	}

	pic = &texture->glyphs[ch & 0xFF];
	if (!R_TextureReferenceIsValid(pic->texnum)) {
		texture = &char_textures[0];
		pic = &texture->glyphs['*'];
	}
	R_TraceAPI("R_DrawChar(%d %c = %d[%d], pic[%d/%s] %0.3f %0.3f, %0.3f", ch, (ch < 256 ? ch : '?'), new_charset, ch & 0xFF, pic->texnum.index, R_TextureIdentifier(pic->texnum), texture->custom_scale_x, texture->custom_scale_y, scale);
	R_TraceAPI(
		"-> %f,%f %f,%f [%d,%d %d,%d]",
		pic->sl, pic->tl, pic->sh - pic->sl, pic->th - pic->tl,
		(int)(pic->sl * R_TextureWidth(pic->texnum)),
		(int)(pic->tl * R_TextureHeight(pic->texnum)),
		(int)((pic->sh - pic->sl) * R_TextureWidth(pic->texnum)),
		(int)((pic->th - pic->tl) * R_TextureHeight(pic->texnum))
	);
	R_DrawImage(x, y, texture->custom_scale_x * scale * 8, texture->custom_scale_y * scale * 8, pic->sl, pic->tl, pic->sh - pic->sl, pic->th - pic->tl, cache_currentColor, false, pic->texnum, true, nextCharacterIsCrosshair);
	return FontCharacterWidthWide(ch, scale, proportional);
}

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
float R_Draw_CharacterBase(float x, float y, wchar num, float scale, qbool apply_overall_alpha, qbool bigchar, qbool gl_statechange, qbool proportional)
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

			return 64 * scale;
		}

		// TODO : Force players to have mcharset.png or fallback to overscaling normal font? :s
	}

	return Draw_TextCacheAddCharacter(x, y, num, scale, proportional);
}

void R_Draw_ResetCharGLState(void)
{
	Draw_TextCacheSetColor(color_white);
}

void R_Draw_SetColor(byte* rgba)
{
	extern cvar_t scr_coloredText;

	if (scr_coloredText.integer) {
		Draw_TextCacheSetColor(rgba);
	}
}

void R_Draw_StringBase_StartString(float x, float y, float scale)
{
	Draw_TextCacheInit(x, y, scale);
}
