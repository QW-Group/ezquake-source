
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

extern float overall_alpha;
extern mpic_t char_textures[MAX_CHARSETS];
extern int char_range[MAX_CHARSETS];

// x, y					= Pixel position of char.
// num					= The character to draw.
// scale				= The scale of the character.
// apply_overall_alpha	= Should the overall alpha for all drawing apply to this char?
// color				= Color!
// bigchar				= Draw this char using the big character charset.
// gl_statechange		= Change the gl state before drawing?
void GLC_Draw_CharacterBase(int x, int y, wchar num, float scale, qbool apply_overall_alpha, byte color[4], qbool bigchar, qbool gl_statechange)
{
	extern cvar_t gl_alphafont;
	extern cvar_t scr_coloredText;
	extern int char_range[MAX_CHARSETS];

	float frow, fcol;
	int slot;
	int char_size = (bigchar ? 64 : 8);
	float char_width;
	float char_height;

	// Only apply overall opacity if it's not fully opague.
	apply_overall_alpha = (apply_overall_alpha && (overall_alpha < 1.0));

	// Only change the GL state if we're told to. We keep track of the need for GL state changes
	// in the string drawing function instead. For character drawing functions we do this every time.
	// (For instance, only change color in a string when the actual color changes, instead of doing
	// it on each character always).
	if (gl_statechange) {
		GLC_StateBeginDrawCharacterBase(apply_overall_alpha, color);
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
	char_height = (char_textures[slot].th - char_textures[slot].tl) * CHARSET_CHAR_HEIGHT;
	char_width = (char_textures[slot].sh - char_textures[slot].sl) * CHARSET_CHAR_WIDTH;
	frow = char_textures[slot].tl + (num >> 4) * char_height;	// row = num * (16 chars per row)
	fcol = char_textures[slot].sl + (num & 0x0F) * char_width;

	GL_BindTextureUnit(GL_TEXTURE0, char_textures[slot].texnum);
	glEnable(GL_TEXTURE_2D);

	// Draw the character polygon.
	glBegin(GL_QUADS);
	{
		float scale8 = scale * 8;
		float scale8_2 = scale8 * 2;

		// Top left.
		glTexCoord2f(fcol, frow);
		glVertex2f(x, y);

		// Top right.
		glTexCoord2f(fcol + char_width, frow);
		glVertex2f(x + scale8, y);

		// Bottom right.
		glTexCoord2f(fcol + char_width, frow + char_height);
		glVertex2f(x + scale8, y + scale8_2);

		// Bottom left.
		glTexCoord2f(fcol, frow + char_height);
		glVertex2f(x, y + scale8_2);
	}
	glEnd();
}

void GLC_Draw_SetColor(byte* rgba, float alpha)
{
	extern cvar_t scr_coloredText;

	if (scr_coloredText.integer) {
		glColor4ub(rgba[0], rgba[1], rgba[2], rgba[3] * alpha * overall_alpha);
	}
}

void GLC_Draw_StringBase_StartString(int x, int y, float scale)
{
	GLC_StateBeginStringDraw();
}
