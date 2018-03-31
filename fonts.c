
// fonts.c
#ifdef EZ_FREETYPE_SUPPORT

#include <ft2build.h>
#include FT_FREETYPE_H
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

GLuint GL_TextureNameFromReference(texture_ref ref);

typedef struct glyphinfo_s {
	float offsets[2];
	int sizes[2];
	float advance[2];
	qbool loaded;
} glyphinfo_t;

static glyphinfo_t glyphs[4096];
static float max_glyph_width;
static float max_num_glyph_width;

#define FONT_TEXTURE_SIZE 1024
charset_t proportional_fonts[MAX_CHARSETS];

static void FontLoadBitmap(int ch, FT_Face face, int base_font_width, int base_font_height, byte* image_buffer, byte base_color[4])
{
	byte* font_buffer;
	int x, y;
	qbool outline = false; // Need to be smarter with font edges for this to be enabled

	glyphs[ch].loaded = true;
	glyphs[ch].advance[0] = (face->glyph->advance.x / 64.0f) / (base_font_width / 2);
	glyphs[ch].advance[1] = (face->glyph->advance.y / 64.0f) / (base_font_height / 2);
	glyphs[ch].offsets[0] = face->glyph->bitmap_left;
	glyphs[ch].offsets[1] = face->glyph->bitmap_top;
	glyphs[ch].sizes[0] = face->glyph->bitmap.width;
	glyphs[ch].sizes[1] = face->glyph->bitmap.rows;

	max_glyph_width = max(max_glyph_width, glyphs[ch].advance[0] * 8);
	if ((ch >= '0' && ch <= '9') || ch == '+' || ch == '-') {
		max_num_glyph_width = max(max_num_glyph_width, glyphs[ch].advance[0] * 8);
	}

	font_buffer = face->glyph->bitmap.buffer;
	for (y = 0; y < face->glyph->bitmap.rows && y < base_font_height / 2; ++y, font_buffer += face->glyph->bitmap.pitch) {
		for (x = 0; x < face->glyph->bitmap.width && x < base_font_width / 2; ++x) {
			int base_lineup = (x + (y - 1) * base_font_width) * 4;
			int base = (x + y * base_font_width) * 4;
			byte alpha = font_buffer[x];

			if (outline) {
				// If previous pixel was transparent, make it black
				if (x && alpha && font_buffer[x - 1] == 0) {
					image_buffer[base - 1] = 255;
				}

				if (y && alpha && image_buffer[base_lineup + 3] == 0) {
					image_buffer[base_lineup + 3] = 255;
				}
			}
			
			image_buffer[base + 0] = (base_color[0] / 255.0f) * alpha;
			image_buffer[base + 1] = (base_color[1] / 255.0f) * alpha;
			image_buffer[base + 2] = (base_color[2] / 255.0f) * alpha;

			// If previous pixel was solid, make this black
			if (outline) {
				if (x && !alpha && font_buffer[x - 1] != 0) {
					alpha = 255;
				}

				if (y && !alpha && image_buffer[base_lineup + 3] && (image_buffer[base_lineup] || image_buffer[base_lineup + 1] || image_buffer[base_lineup + 2])) {
					alpha = 255;
				}
			}
			image_buffer[base + 3] = alpha;// ? 255 : 0;
		}
	}
}

void FontCreate(int grouping, const char* path)
{
	FT_Library library;
	FT_Error error;
	FT_Face face;
	int ch;
	byte* temp_buffer;
	byte* full_buffer;
	byte color_brown[4] = { 100, 64, 24, 255 };
	byte color_numbers[4] = { 227, 224, 130, 255 };
	int original_width, original_height, original_left, original_top;
	int texture_width, texture_height;
	int base_font_width, base_font_height;
	int baseline_offset;
	charset_t* charset;

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

	charset = &proportional_fonts[grouping];
	if (GL_TextureReferenceIsValid(charset->master)) {
		original_width = GL_TextureWidth(charset->master);
		original_height = GL_TextureHeight(charset->master);
		original_left = 0;
		original_top = 0;
		texture_width = original_width;
		texture_height = original_height;
	}
	else {
		original_width = texture_width = FONT_TEXTURE_SIZE * 2;
		original_height = texture_height = FONT_TEXTURE_SIZE * 2;
		original_left = original_top = 0;
		GL_CreateTexturesWithIdentifier(GL_TEXTURE0, GL_TEXTURE_2D, 1, &charset->master, "font");
		GL_TexStorage2D(GL_TEXTURE0, charset->master, 1, GL_RGBA8, texture_width, texture_height);
		GL_TexParameterf(GL_TEXTURE0, charset->master, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		GL_TexParameterf(GL_TEXTURE0, charset->master, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
		GL_TexParameteri(GL_TEXTURE0, charset->master, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
		GL_TexParameteri(GL_TEXTURE0, charset->master, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	}
	base_font_width = texture_width / 16;
	base_font_height = texture_height / 16;
	baseline_offset = 0;

	memset(glyphs, 0, sizeof(glyphs));
	max_glyph_width = max_num_glyph_width = 0;

	FT_Set_Pixel_Sizes(
		face,
		base_font_width / 2,
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
	memset(charset->glyphs, 0, sizeof(charset->glyphs));
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

		charset->glyphs[ch].width = base_font_width / 2;
		charset->glyphs[ch].height = base_font_height / 2;
		charset->glyphs[ch].sl = (original_left + xbase) * 1.0f / texture_width;
		charset->glyphs[ch].tl = (original_top + ybase) * 1.0f / texture_height;
		charset->glyphs[ch].sh = charset->glyphs[ch].sl + 0.5f * base_font_width / texture_width;
		charset->glyphs[ch].th = charset->glyphs[ch].tl + 0.5f * base_font_height / texture_height;
		charset->glyphs[ch].texnum = charset->master;

		GL_TexSubImage2D(
			GL_TEXTURE0, charset->master, 0,
			original_left + xbase,
			original_top + ybase,
			base_font_width,
			base_font_height,
			GL_RGBA, GL_UNSIGNED_BYTE, temp_buffer
		);
	}
	Q_free(full_buffer);

	CachePics_MarkAtlasDirty();
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

qbool FontAlterCharCoords(int* x, int* y, char ch_, qbool bigchar, float scale, qbool proportional)
{
	int char_size = (bigchar ? 64 : 8);
	unsigned char ch = (unsigned char)ch_;

	// Totally off screen.
	if (*y <= (-char_size * scale)) {
		return false;
	}

	// Space.
	if (ch == 32) {
		return false;
	}

	if (proportional && !bigchar && ch <= sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		*x += glyphs[ch].offsets[0] * char_size * scale;
	}

	return true;
}

void FontAdvanceCharCoords(int* x, int* y, char ch_, qbool bigchar, float scale, int char_gap, qbool proportional)
{
	unsigned char ch = (unsigned char)ch_;

	if (bigchar) {
		*x += 64 * scale + char_gap;
	}
	else if (proportional && ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		*x += ceil(8 * (glyphs[ch].advance[0] - glyphs[ch].offsets[0])) * scale + char_gap;
	}
	else {
		*x += 8 * scale + char_gap;
	}
}

float FontCharacterWidth(char ch_, qbool proportional)
{
	unsigned char ch = (unsigned char)ch_;

	if (proportional && ch < sizeof(glyphs) / sizeof(glyphs[0]) && glyphs[ch].loaded) {
		return ceil(8 * glyphs[ch].advance[0]);
	}
	else {
		return 8;
	}
}

// Used for allocating space - if we just measure the string then other hud elements
//   might move around as content changes, which is probably not what is wanted
int FontFixedWidth(int max_length, qbool digits_only, qbool proportional)
{
	if (!proportional || !GL_TextureReferenceIsValid(proportional_fonts[0].master)) {
		return max_length * 8;
	}

	return (int)((digits_only ? max_num_glyph_width : max_glyph_width) * max_length + 0.5f);
}

#endif // EZ_FREETYPE_SUPPORT
