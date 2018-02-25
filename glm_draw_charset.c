
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

extern mpic_t char_textures[MAX_CHARSETS];
extern int char_range[MAX_CHARSETS];

static GLubyte cache_currentColor[4];

void Draw_TextCacheInit(float x, float y, float scale)
{
	memcpy(cache_currentColor, color_white, sizeof(cache_currentColor));
}

static void Draw_TextCacheSetColor(GLubyte* color)
{
	memcpy(cache_currentColor, color, sizeof(cache_currentColor));
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

	{
		float char_height = (char_textures[slot].th - char_textures[slot].tl) * CHARSET_CHAR_HEIGHT;
		float char_width = (char_textures[slot].sh - char_textures[slot].sl) * CHARSET_CHAR_WIDTH;
		float frow = char_textures[slot].tl + (ch >> 4) * char_height;	// row = num * (16 chars per row)
		float fcol = char_textures[slot].sl + (ch & 0x0F) * char_width;

		float s = fcol + 0.5f * (char_textures[slot].th - char_textures[slot].tl) / GL_TextureWidth(char_textures[slot].texnum);
		float t = frow + 0.5f * (char_textures[slot].th - char_textures[slot].tl) / GL_TextureHeight(char_textures[slot].texnum);
		float tex_width = char_width - 0.5f / GL_TextureWidth(char_textures[slot].texnum);
		float tex_height = char_height - 0.5f / GL_TextureHeight(char_textures[slot].texnum);

		GLM_DrawImage(x, y, scale * 8, scale * 8 * 2, s, t, tex_width, tex_height, cache_currentColor, false, char_textures[new_charset].texnum, true);
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
