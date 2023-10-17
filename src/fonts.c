/*
Copyright (C) 2018 ezQuake team

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
*/

// fonts.c
#ifdef EZ_FREETYPE_SUPPORT
#include <ft2build.h>
#include FT_FREETYPE_H
#include "pcre2.h"
#endif
#include "quakedef.h"
#include "gl_model.h"
#include "r_texture.h"
#include "fonts.h"
#include "r_renderer.h"
#include "rulesets.h"

#ifdef EZ_FREETYPE_SUPPORT
typedef struct glyphinfo_s {
	float offsets[2];
	int sizes[2];
	float advance[2];
	qbool loaded;
} glyphinfo_t;

#ifdef EZ_FREETYPE_SUPPORT_STATIC
#define ezFT_Init_FreeType   FT_Init_FreeType
#define ezFT_New_Face        FT_New_Face
#define ezFT_Done_FreeType   FT_Done_FreeType
#define ezFT_Set_Pixel_Sizes FT_Set_Pixel_Sizes
#define ezFT_Load_Char       FT_Load_Char
#define ezFT_Render_Glyph    FT_Render_Glyph
#define ezCloseFTDLL(x)
#else
#define ezCloseFTDLL(dll)    if (dll) { Sys_DLClose(dll); }
#endif

static glyphinfo_t glyphs[4096];
static float max_glyph_width;
static float max_num_glyph_width;

#define FONT_TEXTURE_SIZE 1024
charset_t proportional_fonts[MAX_CHARSETS];

int Font_Load(const char* path);
static void OnChange_font_facepath(cvar_t* cvar, char* newvalue, qbool* cancel);

static cvar_t font_facepath                   = { "font_facepath", "", 0, OnChange_font_facepath };
static cvar_t font_capitalize                 = { "font_capitalize", "0" };
static cvar_t font_gradient_normal_color1     = { "font_gradient_normal_color1", "255 255 255", CVAR_COLOR };
static cvar_t font_gradient_normal_color2     = { "font_gradient_normal_color2", "107 98 86", CVAR_COLOR };
static cvar_t font_gradient_normal_percent    = { "font_gradient_normal_percent", "0.2" };
static cvar_t font_gradient_alternate_color1  = { "font_gradient_alternate_color1", "175 120 52", CVAR_COLOR };
static cvar_t font_gradient_alternate_color2  = { "font_gradient_alternate_color2", "75 52 22", CVAR_COLOR };
static cvar_t font_gradient_alternate_percent = { "font_gradient_alternate_percent", "0.4" };
static cvar_t font_gradient_number_color1     = { "font_gradient_number_color1", "255 255 150", CVAR_COLOR };
static cvar_t font_gradient_number_color2     = { "font_gradient_number_color2", "218 132 7", CVAR_COLOR };
static cvar_t font_gradient_number_percent    = { "font_gradient_number_percent", "0.2" };
static cvar_t font_outline                    = { "font_outline_width", "2" };

typedef struct gradient_def_s {
	cvar_t* top_color;
	cvar_t* bottom_color;
	cvar_t* gradient_start;
} gradient_def_t;

static gradient_def_t standard_gradient = { &font_gradient_normal_color1, &font_gradient_normal_color2, &font_gradient_normal_percent };
static gradient_def_t brown_gradient    = { &font_gradient_alternate_color1, &font_gradient_alternate_color2, &font_gradient_alternate_percent };
static gradient_def_t numbers_gradient  = { &font_gradient_number_color1, &font_gradient_number_color2, &font_gradient_number_percent };

static void FontSetColor(byte* color, byte alpha, gradient_def_t* gradient, float relative_y)
{
	float base_color[3];
	float grad_start = bound(0, gradient->gradient_start->value, 1);

	if (relative_y <= grad_start) {
		VectorScale(gradient->top_color->color, 1.0f / 255, base_color);
	}
	else {
		float mix = (relative_y - grad_start) / (1.0f - grad_start);

		VectorScale(gradient->top_color->color, (1.0f - mix) / 255.0f, base_color);
		VectorMA(base_color, mix / 255.0f, gradient->bottom_color->color, base_color);
	}

	color[0] = min(1, base_color[0]) * alpha;
	color[1] = min(1, base_color[1]) * alpha;
	color[2] = min(1, base_color[2]) * alpha;
	color[3] = alpha;
}

// Very simple: boost the alpha of every pixel to strongest alpha nearby
// - small adjustment made for distance
// - not a great solution but works for the moment
// - can supply matrix to freetype, should use that instead
static void SimpleOutline(byte* image_buffer, int base_font_width, int base_font_height)
{
	int x, y;
	int xdiff, ydiff;
	byte* font_buffer = Q_malloc(base_font_width * base_font_height * 4);
	const int search_distance = bound(0, font_outline.integer, 4);

	memcpy(font_buffer, image_buffer, base_font_width * base_font_height * 4);
	for (x = 0; x < base_font_width; ++x) {
		for (y = 0; y < base_font_height; ++y) {
			int base = (x + y * base_font_width) * 4;
			int best_alpha = 0;

			if (font_buffer[base + 3] == 255) {
				continue;
			}

			for (xdiff = max(0, x - search_distance); xdiff <= min(x + search_distance, base_font_width - 1); ++xdiff) {
				for (ydiff = max(0, y - search_distance); ydiff <= min(y + search_distance, base_font_height - 1); ++ydiff) {
					float dist = abs(x - xdiff) + abs(y - ydiff);
					int this_alpha = font_buffer[(xdiff + ydiff * base_font_width) * 4 + 3] / (0.3 * (dist + 1));

					if (this_alpha >= best_alpha) {
						best_alpha = min(255, this_alpha);
					}
				}
			}

			image_buffer[base + 3] = best_alpha;
		}
	}

	Q_free(font_buffer);
}

static void FontLoadBitmap(int ch, FT_Face face, int base_font_width, int base_font_height, byte* image_buffer, gradient_def_t* gradient)
{
	byte* font_buffer;
	int x, y;
	int outline = bound(0, font_outline.integer, 4);

	glyphs[ch].loaded = true;
	glyphs[ch].advance[0] = ((face->glyph->metrics.horiAdvance / 64.0f) + (2 * outline)) / (base_font_width / 2);
	glyphs[ch].advance[1] = ((face->glyph->metrics.vertAdvance / 64.0f) + (2 * outline)) / (base_font_height / 2);
	glyphs[ch].offsets[0] = face->glyph->metrics.horiBearingX / 64.0f - (outline);
	glyphs[ch].offsets[1] = face->glyph->metrics.horiBearingY / 64.0f - (outline);
	glyphs[ch].sizes[0] = face->glyph->bitmap.width;
	glyphs[ch].sizes[1] = face->glyph->bitmap.rows;

	max_glyph_width = max(max_glyph_width, glyphs[ch].advance[0] * 8);
	if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-') {
		max_num_glyph_width = max(max_num_glyph_width, glyphs[ch].advance[0] * 8);
	}

	font_buffer = face->glyph->bitmap.buffer;
	for (y = 0; y < face->glyph->bitmap.rows && y + outline < base_font_height - 4; ++y, font_buffer += face->glyph->bitmap.pitch) {
		for (x = 0; x < face->glyph->bitmap.width && x + outline < base_font_width - 4; ++x) {
			int base = (x + outline + (y + outline) * base_font_width) * 4;
			byte alpha = font_buffer[x];

			FontSetColor(&image_buffer[base], alpha, gradient, y * 1.0f / face->glyph->bitmap.rows);
		}
	}

	if (outline) {
		SimpleOutline(image_buffer, base_font_width, base_font_height);
	}
}

static void FontClear(int grouping)
{
	charset_t* charset;

	memset(glyphs, 0, sizeof(glyphs));
	max_glyph_width = max_num_glyph_width = 0;

	charset = &proportional_fonts[grouping];
	memset(charset, 0, sizeof(*charset));
}

static qbool FontCreate(int grouping, const char* userpath)
{
	char path[MAX_OSPATH];
	FT_Library library;
	FT_Error error;
	FT_Face face;
	int ch;
	byte* temp_buffer;
	byte* full_buffer;
	int original_width, original_height, original_left, original_top;
	int texture_width, texture_height;
	int base_font_width, base_font_height;
	int baseline_offset;
	charset_t* charset;
	int outline_width = bound(0, font_outline.integer, 4);
#ifndef EZ_FREETYPE_SUPPORT_STATIC
	// freetyped.lib;
	DL_t dll = NULL;
	FT_Error(*ezFT_Init_FreeType)(FT_Library* library) = NULL;
	FT_Error(*ezFT_New_Face)(FT_Library library, const char* filepathname, FT_Long face_index, FT_Face* aface) = NULL;
	FT_Error(*ezFT_Done_FreeType)(FT_Library library) = NULL;
	FT_Error(*ezFT_Set_Pixel_Sizes)(FT_Face face, FT_UInt pixel_width, FT_UInt pixel_height) = NULL;
	FT_Error(*ezFT_Load_Char)(FT_Face face, FT_ULong char_code, FT_Int32 load_flags) = NULL;
	FT_Error(*ezFT_Render_Glyph)(FT_GlyphSlot slot, FT_Render_Mode render_mode) = NULL;

	if (Rulesets_RestrictPacket()) {
		Con_Printf("Cannot change font during match\n");
		return false;
	}

	dll = Sys_DLOpen("freetype.dll");
	if (dll) {
		ezFT_Init_FreeType = Sys_DLProc(dll, "FT_Init_FreeType");
		ezFT_New_Face = Sys_DLProc(dll, "FT_New_Face");
		ezFT_Done_FreeType = Sys_DLProc(dll, "FT_Done_FreeType");
		ezFT_Set_Pixel_Sizes = Sys_DLProc(dll, "FT_Set_Pixel_Sizes");
		ezFT_Load_Char = Sys_DLProc(dll, "FT_Load_Char");
		ezFT_Render_Glyph = Sys_DLProc(dll, "FT_Render_Glyph");
	}

	if (ezFT_Init_FreeType == NULL || ezFT_New_Face == NULL || ezFT_Done_FreeType == NULL ||
		ezFT_Set_Pixel_Sizes == NULL || ezFT_Load_Char == NULL || ezFT_Render_Glyph == NULL) {
		Con_Printf("Unable to load freetype library, proportional fonts not available\n");
		ezCloseFTDLL(dll);
		return false;
	}
#endif

	error = ezFT_Init_FreeType(&library);
	if (error) {
		Con_Printf("Error during freetype initialisation\n");
		ezCloseFTDLL(dll);
		return false;
	}

	strlcpy(path, Sys_FontsDirectory(), sizeof(path));
	strlcat(path, "/", sizeof(path));
	strlcat(path, userpath, sizeof(path));
	COM_DefaultExtension(path, ".ttf");

	error = ezFT_New_Face(library, path, 0, &face);
	if (error == FT_Err_Unknown_File_Format) {
		Con_Printf("Font file found, but format is unsupported\n");
		ezCloseFTDLL(dll);
		return false;
	}
	else if (error) {
		Con_Printf("Font file could not be opened\n");
		Con_Printf("> Attempted path: %s\n", path);
		ezCloseFTDLL(dll);
		return false;
	}

	charset = &proportional_fonts[grouping];
	if (R_TextureReferenceIsValid(charset->master)) {
		original_width = R_TextureWidth(charset->master);
		original_height = R_TextureHeight(charset->master);
		original_left = 0;
		original_top = 0;
		texture_width = original_width;
		texture_height = original_height;
	}
	else {
		original_width = texture_width = FONT_TEXTURE_SIZE * 2;
		original_height = texture_height = FONT_TEXTURE_SIZE * 2;
		original_left = original_top = 0;

		renderer.TextureCreate2D(&charset->master, texture_width, texture_height, "font", false);
	}
	base_font_width = texture_width / 16;
	base_font_height = texture_height / 16;
	baseline_offset = 0;

	memset(glyphs, 0, sizeof(glyphs));
	max_glyph_width = max_num_glyph_width = 0;

	ezFT_Set_Pixel_Sizes(
		face,
		base_font_width / 2 - 2 * outline_width,
		base_font_height / 2 - 2 * outline_width
	);

	temp_buffer = full_buffer = Q_malloc(4 * base_font_width * base_font_height * 256);
	for (ch = 18; ch < 128; ++ch, temp_buffer += 4 * base_font_width * base_font_height) {
		FT_UInt glyph_index;
		int offset128 = 4 * base_font_width * base_font_height * 128;
		int offsetCaps = 4 * base_font_width * base_font_height * ('a' - 'A');

		if (ch >= 28 && ch < 32) {
			continue;
		}

		if (font_capitalize.integer && ch >= 'a' && ch <= 'z') {
			continue;
		}

		if (ch < 32) {
			glyph_index = ezFT_Load_Char(face, '0' + (ch - 18), FT_LOAD_RENDER);
		}
		else {
			glyph_index = ezFT_Load_Char(face, ch, FT_LOAD_RENDER);
		}

		if (glyph_index) {
			continue;
		}

		error = ezFT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		if (error) {
			continue;
		}

		if (ch < 32) {
			FontLoadBitmap(ch, face, base_font_width, base_font_height, temp_buffer, &numbers_gradient);
			FontLoadBitmap(ch + 128, face, base_font_width, base_font_height, temp_buffer + offset128, &numbers_gradient);
		}
		else {
			FontLoadBitmap(ch, face, base_font_width, base_font_height, temp_buffer, &standard_gradient);
			FontLoadBitmap(ch + 128, face, base_font_width, base_font_height, temp_buffer + offset128, &brown_gradient);

			if (font_capitalize.integer && ch >= 'A' && ch <= 'Z') {
				FontLoadBitmap(ch + ('a' - 'A'), face, base_font_width, base_font_height, temp_buffer + offsetCaps, &standard_gradient);
				FontLoadBitmap(ch + ('a' - 'A') + 128, face, base_font_width, base_font_height, temp_buffer + offset128 + offsetCaps, &brown_gradient);
			}
		}
	}
	ezFT_Done_FreeType(library);
	ezCloseFTDLL(dll);

	// Work out where the baseline is...
	{
		int max_beneath = 0;
		int beneath_baseline;

		for (ch = 18; ch < 128; ++ch) {
			if (!glyphs[ch].loaded) {
				continue;
			}

			beneath_baseline = glyphs[ch].sizes[1] - glyphs[ch].offsets[1];
			max_beneath = max(max_beneath, beneath_baseline);
		}

		baseline_offset = base_font_height / 2 - 1 - max_beneath - outline_width;
	}

	// Update charset image
	temp_buffer = full_buffer;
	memset(charset->glyphs, 0, sizeof(charset->glyphs));
	for (ch = 18; ch < 256; ++ch, temp_buffer += 4 * base_font_width * base_font_height) {
		int xbase = (ch % 16) * base_font_width;
		int ybase = (ch / 16) * base_font_height;
		int yoffset = max(0, baseline_offset - glyphs[ch].offsets[1]);
		float width = max(base_font_width / 2, glyphs[ch].sizes[0]);
		float height = max(base_font_height / 2, glyphs[ch].sizes[1]);

		if (!glyphs[ch].loaded) {
			continue;
		}

		if (yoffset) {
			memmove(temp_buffer + yoffset * base_font_width * 4, temp_buffer, (base_font_height - yoffset) * base_font_width * 4);
			memset(temp_buffer, 0, yoffset * base_font_width * 4);
		}

		glyphs[ch].offsets[0] /= (base_font_width / 2);
		glyphs[ch].offsets[1] /= (base_font_height / 2);

		charset->glyphs[ch].width = width;
		charset->glyphs[ch].height = height;
		charset->glyphs[ch].sl = (original_left + xbase) * 1.0f / texture_width;
		charset->glyphs[ch].tl = (original_top + ybase) * 1.0f / texture_height;
		charset->glyphs[ch].sh = charset->glyphs[ch].sl + width * 1.0f / texture_width;
		charset->glyphs[ch].th = charset->glyphs[ch].tl + height * 1.0f / texture_height;
		charset->glyphs[ch].texnum = charset->master;

		renderer.TextureReplaceSubImageRGBA(charset->master, original_left + xbase, original_top + ybase, base_font_width, base_font_height, temp_buffer);
	}
	Q_free(full_buffer);
	charset->custom_scale_x = charset->custom_scale_y = 1;

	CachePics_MarkAtlasDirty();

	return true;
}

// Used for allocating space - if we just measure the string then other hud elements
//   might move around as content changes, which is probably not what is wanted
int FontFixedWidth(int max_length, float scale, qbool digits_only, qbool proportional)
{
	if (!proportional || !R_TextureReferenceIsValid(proportional_fonts[0].master)) {
		return max_length * 8 * scale;
	}

	return ceil((digits_only ? max_num_glyph_width : max_glyph_width) * max_length * scale);
}

static void OnChange_font_facepath(cvar_t* cvar, char* newvalue, qbool* cancel)
{
	if (newvalue && !newvalue[0]) {
		FontClear(0);
		*cancel = false;
	}
	else {
		*cancel = !FontCreate(0, Cmd_Argv(1));
	}
}

void Draw_LoadFont_f(void)
{
	switch (Cmd_Argc()) {
	case 1:
		if (font_facepath.string[0]) {
			Com_Printf("Current proportional font is \"%s\"\n", font_facepath.string);
			Com_Printf("To clear, use \"%s none\"\n", Cmd_Argv(0));
		}
		else {
			Com_Printf("No proportional font loaded\n");
		}
		break;
	case 2:
		if (!strcmp(Cmd_Argv(1), "none")) {
			Cvar_Set(&font_facepath, "");
		}
		else {
			Cvar_Set(&font_facepath, Cmd_Argv(1));
		}
		break;
	default:
		Com_Printf("Usage: \"%s <path>\" or \"%s none\"\n", Cmd_Argv(0), Cmd_Argv(0));
		break;
	}
}

// FIXME, should be in client but relies on Sys_listdir
#ifndef CLIENTONLY
void Draw_ListFonts_f(void)
{
	char path[MAX_OSPATH];
	dir_t dir;
	int i, printed;
	pcre2_code* regexp = NULL;
	pcre2_match_data *match_data = NULL;

	strlcpy(path, Sys_FontsDirectory(), sizeof(path));

	if (Cmd_Argc() > 1) {
		int error = 0;
		PCRE2_SIZE error_offset = 0;

		regexp = pcre2_compile((PCRE2_SPTR)Cmd_Argv(1), PCRE2_ZERO_TERMINATED, PCRE2_CASELESS, &error, &error_offset, NULL);
		if (!regexp) {
			PCRE2_UCHAR error_str[256];
			pcre2_get_error_message(error, error_str, sizeof(error_str));
			Con_Printf("Error in regular expression: %s\n", error_str);
			return;
		}
	}

	dir = Sys_listdir(path, ".ttf", SORT_BY_NAME);
	for (i = 0, printed = 0; i < dir.numfiles; ++i) {
		if (dir.files[i].isdir) {
			continue;
		}
		if (regexp) {
			match_data = pcre2_match_data_create_from_pattern(regexp, NULL);
			if (pcre2_match(regexp, (PCRE2_SPTR)dir.files[i].name, strlen(dir.files[i].name), 0, 0, match_data, NULL) <= 0) {
				pcre2_match_data_free(match_data);
				continue;
			}
			pcre2_match_data_free(match_data);
		}

		Con_Printf("  %s\n", dir.files[i].name);
		printed++;
	}

	if (regexp) {
		pcre2_code_free(regexp);
	}
	Con_Printf("Found %d/%d files in %s\n", printed, dir.numfiles, path);
}
#endif

void FontInitialise(void)
{
	Cmd_AddCommand("fontload", Draw_LoadFont_f);
#ifndef CLIENTONLY // FIXME
	Cmd_AddCommand("fontlist", Draw_ListFonts_f);
#endif

	Cvar_Register(&font_facepath);
	Cvar_Register(&font_capitalize);
	Cvar_Register(&font_gradient_normal_color1);
	Cvar_Register(&font_gradient_normal_color2);
	Cvar_Register(&font_gradient_normal_percent);
	Cvar_Register(&font_gradient_alternate_color1);
	Cvar_Register(&font_gradient_alternate_color2);
	Cvar_Register(&font_gradient_alternate_percent);
	Cvar_Register(&font_gradient_number_color1);
	Cvar_Register(&font_gradient_number_color2);
	Cvar_Register(&font_gradient_number_percent);
	Cvar_Register(&font_outline);
}

#endif // EZ_FREETYPE_SUPPORT

qbool FontAlterCharCoordsWide(float* x, float* y, wchar ch, qbool bigchar, float scale, qbool proportional)
{
	int char_size = (bigchar ? 64 : 8);

	// Totally off screen.
	if (*y <= (-char_size * scale)) {
		return false;
	}

	// Space (& 'red' version).
	if (ch == 32 || ch == (32 | 128)) {
		*x += FontCharacterWidthWide(ch, scale, proportional);
		return false;
	}

#ifdef EZ_FREETYPE_SUPPORT
	if (proportional && ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		*x += glyphs[ch].offsets[0] * char_size * scale;
	}
#endif

	return true;
}

float FontCharacterWidthWide(wchar ch, float scale, qbool proportional)
{
	int charset = ((ch >> 8) & 0xFF);
	float width = 8 * scale;

#ifdef EZ_FREETYPE_SUPPORT
	if (proportional && ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		return width * glyphs[ch].advance[0];
	}
#endif

	if (charset >= 0 && charset < sizeof(char_textures) / sizeof(char_textures[0]) && char_textures[charset].custom_scale_x != 0) {
		width *= char_textures[charset].custom_scale_x;
	}

	return width;
}

float FontCharacterWidth(char ch_, float scale, qbool proportional)
{
	int charset = 0;

#ifdef EZ_FREETYPE_SUPPORT
	int ch = (unsigned char)(ch_);

	if (proportional && ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		return 8 * glyphs[ch].advance[0] * scale;
	}
#endif

	if (charset < 0 || charset >= MAX_CHARSETS || char_textures[charset].custom_scale_x == 0) {
		return 8 * scale;
	}

	return 8 * char_textures[charset].custom_scale_x * scale;
}

void Draw_InitFont(void)
{
#ifdef EZ_FREETYPE_SUPPORT
	if (!font_facepath.string[0]) {
		FontClear(0);
	}
	else {
		FontCreate(0, font_facepath.string);
	}
#endif
}
