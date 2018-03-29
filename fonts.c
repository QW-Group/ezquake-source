
// fonts.c
#ifdef EZ_FREETYPE_SUPPORT

#include <ft2build.h>
#include FT_FREETYPE_H
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

typedef struct glyphinfo_s {
	float offsets[2];
	int sizes[2];
	float advance[2];
	qbool loaded;
} glyphinfo_t;

static glyphinfo_t glyphs[4096];

#define MAX_FONT_TEXTURE_SIZE 1024
static mpic_t* font_textures[4];

static void FontLoadBitmap(int ch, FT_Face face, int base_font_width, int base_font_height, byte* image_buffer, byte base_color[4])
{
	byte* font_buffer;
	int x, y;

	glyphs[ch].loaded = true;
	glyphs[ch].advance[0] = (face->glyph->advance.x / 64.0f) / base_font_width;
	glyphs[ch].advance[1] = (face->glyph->advance.y / 64.0f) / (base_font_height / 2);
	glyphs[ch].offsets[0] = face->glyph->bitmap_left;
	glyphs[ch].offsets[1] = face->glyph->bitmap_top;
	glyphs[ch].sizes[0] = face->glyph->bitmap.width;
	glyphs[ch].sizes[1] = face->glyph->bitmap.rows;

	font_buffer = face->glyph->bitmap.buffer;
	for (y = 0; y < face->glyph->bitmap.rows && y < base_font_height; ++y, font_buffer += face->glyph->bitmap.pitch) {
		for (x = 0; x < face->glyph->bitmap.width && x < base_font_width; ++x) {
			int base = (x + y * base_font_width) * 4;
			image_buffer[base] = (base_color[0] / 255.0f) * font_buffer[x];
			image_buffer[base + 1] = (base_color[1] / 255.0f) * font_buffer[x];
			image_buffer[base + 2] = (base_color[2] / 255.0f) * font_buffer[x];
			image_buffer[base + 3] = font_buffer[x] ? 255 : 0;
		}
	}
}

void FontCreate(const char* path)
{
	FT_Library library;
	FT_Error error;
	FT_Face face;
	int ch;
	byte* temp_buffer;
	byte* full_buffer;

	int original_width = GL_TextureWidth(char_textures[0].texnum);
	int original_height = GL_TextureHeight(char_textures[0].texnum);
	int original_left = original_width * char_textures[0].sl;
	int original_top = original_height * char_textures[0].tl;
	int texture_width = original_width * (char_textures[0].sh - char_textures[0].sl);
	int texture_height = original_height * (char_textures[0].th - char_textures[0].tl);
	int base_font_width = texture_width / 16;
	int base_font_height = texture_height / 16;
	int baseline_offset = 0;
	byte color_brown[4] = { 100, 64, 24, 255 };
	byte color_numbers[4] = { 227, 224, 130, 255 };

	memset(glyphs, 0, sizeof(glyphs));

	error = FT_Init_FreeType(&library);
	if (error) {
		Con_Printf("Error during freetype initialisation\n");
		return;
	}

	error = FT_New_Face(library, path, 0, &face);
	if (error == FT_Err_Unknown_File_Format) {
		Con_Printf("Font file found, but format is unsupported\n");
		return;
	}
	else if (error) {
		Con_Printf("Font file could not be opened\n");
		return;
	}

	FT_Set_Pixel_Sizes(
		face,
		base_font_width,
		base_font_height / 2
	);

	temp_buffer = full_buffer = Q_malloc(4 * base_font_width * base_font_height * 256);
	for (ch = 18; ch < 128; ++ch, temp_buffer += 4 * base_font_width * base_font_height) {
		FT_UInt glyph_index;
		int offset128 = 4 * base_font_width * base_font_height * 128;

		if (ch >= 28 && ch < 32) {
			continue;
		}

		if (ch < 32) {
			glyph_index = FT_Load_Char(face, '0' + (ch - 18), FT_LOAD_RENDER);
		}
		else {
			glyph_index = FT_Load_Char(face, ch, FT_LOAD_RENDER);
		}

		if (glyph_index) {
			continue;
		}

		error = FT_Render_Glyph(face->glyph, FT_RENDER_MODE_NORMAL);
		if (error) {
			continue;
		}

		if (ch < 32) {
			FontLoadBitmap(ch, face, base_font_width, base_font_height, temp_buffer, color_numbers);
			FontLoadBitmap(ch + 128, face, base_font_width, base_font_height, temp_buffer + offset128, color_numbers);
		}
		else {
			FontLoadBitmap(ch, face, base_font_width, base_font_height, temp_buffer, color_white);
			FontLoadBitmap(ch + 128, face, base_font_width, base_font_height, temp_buffer + offset128, color_brown);
		}
	}

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

		baseline_offset = base_font_height / 2 - 1 - max_beneath;
	}

	// Update charset image
	temp_buffer = full_buffer;
	for (ch = 18; ch < 256; ++ch, temp_buffer += 4 * base_font_width * base_font_height) {
		int xbase = (ch % 16) * base_font_width;
		int ybase = (ch / 16) * base_font_height;
		int yoffset = max(0, baseline_offset - glyphs[ch].offsets[1]);

		if (!glyphs[ch].loaded) {
			continue;
		}

		if (yoffset) {
			memmove(temp_buffer + yoffset * base_font_width * 4, temp_buffer, (base_font_height - yoffset) * base_font_width * 4);
			memset(temp_buffer, 0, yoffset * base_font_width * 4);
		}

		glyphs[ch].offsets[0] /= base_font_width;

		GL_TexSubImage2D(
			GL_TEXTURE0, char_textures[0].texnum, 0,
			original_left + xbase,
			original_top + ybase,
			base_font_width,
			base_font_height,
			GL_RGBA, GL_UNSIGNED_BYTE, temp_buffer
		);
	}
	Q_free(full_buffer);
}

qbool FontAlterCharCoordsWide(int* x, int* y, wchar ch, qbool bigchar, float scale)
{
	int char_size = (bigchar ? 64 : 8);

	// Totally off screen.
	if (*y <= (-char_size * scale)) {
		return false;
	}

	// Space.
	if (ch == 32) {
		return false;
	}

	if (ch <= sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		*x += glyphs[ch].offsets[0] * char_size * scale;
	}

	return true;
}

void FontAdvanceCharCoordsWide(int* x, int* y, wchar ch, qbool bigchar, float scale, int char_gap)
{
	if (bigchar) {
		*x += 64 * scale + char_gap;
	}
	else if (ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		*x += ceil(8 * (glyphs[ch].advance[0] - glyphs[ch].offsets[0])) * scale + char_gap;
	}
	else {
		*x += 8 * scale + char_gap;
	}
}

float FontCharacterWidthWide(wchar ch)
{
	if (ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		return ceil(8 * glyphs[ch].advance[0]);
	}
	else {
		return 8;
	}
}

qbool FontAlterCharCoords(int* x, int* y, char ch, qbool bigchar, float scale)
{
	int char_size = (bigchar ? 64 : 8);

	// Totally off screen.
	if (*y <= (-char_size * scale)) {
		return false;
	}

	// Space.
	if (ch == 32) {
		return false;
	}

	if (ch <= sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		*x += glyphs[ch].offsets[0] * char_size * scale;
	}

	return true;
}

void FontAdvanceCharCoords(int* x, int* y, char ch, qbool bigchar, float scale, int char_gap)
{
	if (bigchar) {
		*x += 64 * scale + char_gap;
	}
	else if (ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		*x += ceil(8 * (glyphs[ch].advance[0] - glyphs[ch].offsets[0])) * scale + char_gap;
	}
	else {
		*x += 8 * scale + char_gap;
	}
}

float FontCharacterWidth(char ch)
{
	if (ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		return ceil(8 * glyphs[ch].advance[0]);
	}
	else {
		return 8;
	}
}

#endif // EZ_FREETYPE_SUPPORT
