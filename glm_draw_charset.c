
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "fonts.h"

extern mpic_t char_textures[MAX_CHARSETS];
extern int char_range[MAX_CHARSETS];

static GLubyte cache_currentColor[4];
static qbool nextCharacterIsCrosshair = false;

void Draw_SetCrosshairTextMode(qbool enabled)
{
	nextCharacterIsCrosshair = enabled;
}

static void Draw_TextCacheInit(float x, float y, float scale)
{
	memcpy(cache_currentColor, color_white, sizeof(cache_currentColor));
}

static void Draw_TextCacheSetColor(GLubyte* color)
{
	memcpy(cache_currentColor, color, sizeof(cache_currentColor));
}

static int Draw_TextCacheAddCharacter(float x, float y, wchar ch, float scale, qbool proportional)
{
	int new_charset = (ch & 0xFF00) >> 8;
	mpic_t* texture = &char_textures[0];

	if (proportional) {
		if (new_charset != 0) {
			// TODO: add support for these
			ch = '?';
			proportional = false;
		}
		else {
			extern mpic_t font_texture;

			texture = &font_texture;

			proportional = GL_TextureReferenceIsValid(texture->texnum);
		}
	}

	if (!proportional) {
		int slot = 0;

		if ((ch & 0xFF00) != 0) {
			slot = ((ch >> 8) & 0xFF);
			if (!char_range[slot]) {
				slot = 0;
				ch = '?';
			}
		}

		texture = &char_textures[slot];
	}

	ch &= 0xFF;	// Only use the first byte.

	{
		float char_height = (texture->th - texture->tl) * CHARSET_CHAR_HEIGHT;
		float char_width = (texture->sh - texture->sl) * CHARSET_CHAR_WIDTH;
		float frow = texture->tl + (ch >> 4) * char_height;	// row = num * (16 chars per row)
		float fcol = texture->sl + (ch & 0x0F) * char_width;

		float s = fcol + 0.5f * (texture->th - texture->tl) / GL_TextureWidth(texture->texnum);
		float t = frow + 0.5f * (texture->th - texture->tl) / GL_TextureHeight(texture->texnum);
		float tex_width = char_width - 0.5f / GL_TextureWidth(texture->texnum);
		float tex_height = char_height - 0.5f / GL_TextureHeight(texture->texnum);

		GLM_DrawImage(x, y, scale * 8, scale * 8 * 2, s, t, tex_width, tex_height, cache_currentColor, false, texture->texnum, true, nextCharacterIsCrosshair);
	}

	return FontCharacterWidth(ch, proportional);
}

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// color				= Color!
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
int GLM_Draw_CharacterBase(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange, qbool proportional)
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

void GLM_Draw_ResetCharGLState(void)
{
	//GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	//GL_TextureEnvMode(GL_REPLACE);
	Draw_TextCacheSetColor(color_white);
}

void GLM_Draw_SetColor(byte* rgba)
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
