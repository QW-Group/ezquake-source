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

*/

#include "quakedef.h"
#include "gl_local.h"
#include "version.h"
#include "sbar.h"

#include "image.h"
#include "utils.h"

extern cvar_t crosshair, cl_crossx, cl_crossy, crosshaircolor, crosshairsize;
extern cvar_t scr_coloredText;

cvar_t	scr_conalpha		= {"scr_conalpha", "0.8"};
cvar_t	scr_menualpha		= {"scr_menualpha", "0.7"};


qboolean OnChange_gl_crosshairimage(cvar_t *, char *);
cvar_t	gl_crosshairimage   = {"crosshairimage", "", 0, OnChange_gl_crosshairimage};

qboolean OnChange_gl_consolefont (cvar_t *, char *);
cvar_t	gl_consolefont		= {"gl_consolefont", "original", 0, OnChange_gl_consolefont};

cvar_t	gl_crosshairalpha	= {"crosshairalpha", "1"};


qboolean OnChange_gl_smoothfont (cvar_t *var, char *string);
cvar_t gl_smoothfont = {"gl_smoothfont", "0", 0, OnChange_gl_smoothfont};

byte			*draw_chars;						// 8*8 graphic characters
mpic_t			*draw_disc;
static mpic_t	*draw_backtile;

static int		translate_texture;
static int		char_texture;

static mpic_t	conback;


#define		NUMCROSSHAIRS 6
int			crosshairtextures[NUMCROSSHAIRS];
int			crosshairtexture_txt;
mpic_t		crosshairpic;

static byte customcrosshairdata[64];

#define CROSSHAIR_NONE	0
#define CROSSHAIR_TXT	1
#define CROSSHAIR_IMAGE	2
static int customcrosshair_loaded = CROSSHAIR_NONE;


static byte crosshairdata[NUMCROSSHAIRS][64] = {
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,

	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,
	0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
	0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff,
	0xff, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff,
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe,

	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 
	0xff, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xff, 0xff, 
	0xfe, 0xfe, 0xff, 0xfe, 0xff, 0xfe, 0xfe, 0xff, 
	0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xfe, 0xff, 
	0xfe, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xfe, 0xff, 
	0xff, 0xfe, 0xff, 0xff, 0xff, 0xfe, 0xff, 0xff, 
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
	
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 
	0xfe, 0xff, 0xff, 0xfe, 0xff, 0xff, 0xfe, 0xff, 
	0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xfe, 0xfe, 0xfe, 0xff, 0xff, 0xff, 
	0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
};


qboolean OnChange_gl_smoothfont (cvar_t *var, char *string) {
	float newval;

	newval = Q_atof (string);
	if (!newval == !gl_smoothfont.value || !char_texture)
		return false;

	GL_Bind(char_texture);

	if (newval)	{
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	} else {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	return false;
}


qboolean OnChange_gl_crosshairimage(cvar_t *v, char *s) {
	mpic_t *pic;

	if (!s[0]) {
		customcrosshair_loaded &= ~CROSSHAIR_IMAGE;
		return false;
	}
	if (!(pic = GL_LoadPicImage(va("crosshairs/%s", s), "crosshair", 0, 0, TEX_ALPHA))) {
		customcrosshair_loaded &= ~CROSSHAIR_IMAGE;
		Com_Printf("Couldn't load image %s\n", s);
		return false;
	}
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	crosshairpic = *pic;
	customcrosshair_loaded |= CROSSHAIR_IMAGE;
	return false;
}

void customCrosshair_Init(void) {
	FILE *f;
	int i = 0, c;

	customcrosshair_loaded = CROSSHAIR_NONE;

	if (FS_FOpenFile("crosshairs/crosshair.txt", &f) == -1)
		return;

	while (i < 64) {
		c = fgetc(f);
		if (c == EOF) {
			Com_Printf("Invalid format in crosshair.txt (Need 64 X's and O's)\n");	
			fclose(f);
			return;
		}
		if (isspace(c))
			continue;
		if (tolower(c) != 'x' && tolower(c) != 'o') {
			Com_Printf("Invalid format in crosshair.txt (Only X's and O's and whitespace permitted)\n");
			fclose(f);
			return;
		}		
		customcrosshairdata[i++] = (c == 'x' || c  == 'X') ? 0xfe : 0xff;
	}
	fclose(f);
	crosshairtexture_txt = GL_LoadTexture ("", 8, 8, customcrosshairdata, TEX_ALPHA, 1);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	customcrosshair_loaded |= CROSSHAIR_TXT;
}


/*
=============================================================================
  scrap allocation

  Allocate all the little status bar objects into a single texture
  to crutch up stupid hardware / drivers
=============================================================================
*/

// some cards have low quality of alpha pics, so load the pics
// without transparent pixels into a different scrap block.
// scrap 0 is solid pics, 1 is transparent
#define	MAX_SCRAPS		2
#define	BLOCK_WIDTH		256
#define	BLOCK_HEIGHT	256

int			scrap_allocated[MAX_SCRAPS][BLOCK_WIDTH];
byte		scrap_texels[MAX_SCRAPS][BLOCK_WIDTH*BLOCK_HEIGHT*4];
int			scrap_dirty = 0;	// bit mask
int			scrap_texnum;

// returns false if allocation failed
qboolean Scrap_AllocBlock (int scrapnum, int w, int h, int *x, int *y) {
	int i, j, best, best2;

	best = BLOCK_HEIGHT;
	
	for (i = 0; i < BLOCK_WIDTH - w; i++) {
		best2 = 0;
		
		for (j = 0; j < w; j++) {
			if (scrap_allocated[scrapnum][i + j] >= best)
				break;
			if (scrap_allocated[scrapnum][i + j] > best2)
				best2 = scrap_allocated[scrapnum][i + j];
		}
		if (j == w) {	// this is a valid spot
			*x = i;
			*y = best = best2;
		}
	}
	
	if (best + h > BLOCK_HEIGHT)
		return false;
	
	for (i = 0; i < w; i++)
		scrap_allocated[scrapnum][*x + i] = best + h;

	scrap_dirty |= (1 << scrapnum);

	return true;
}

int	scrap_uploads;

void Scrap_Upload (void) {
	int i;

	scrap_uploads++;
	for (i = 0; i < 2 ; i++) {
		if (!(scrap_dirty & (1 << i)))
			continue;
		scrap_dirty &= ~(1 << i);
		GL_Bind(scrap_texnum + i);
		GL_Upload8 (scrap_texels[i], BLOCK_WIDTH, BLOCK_HEIGHT, TEX_ALPHA);
	}
}

//=============================================================================
/* Support Routines */

typedef struct cachepic_s {
	char		name[MAX_QPATH];
	mpic_t		pic;
} cachepic_t;

#define	MAX_CACHED_PICS		128
cachepic_t	cachepics[MAX_CACHED_PICS];
int			numcachepics;

byte	menuplyr_pixels[4096];

int		pic_texels, pic_count;

mpic_t *Draw_CacheWadPic (char *name) {
	qpic_t	*p;
	mpic_t	*pic, *pic_24bit;

	p = W_GetLumpName (name);
	pic = (mpic_t *)p;

	
	if (
		(pic_24bit = GL_LoadPicImage(va("textures/wad/%s", name), name, 0, 0, TEX_ALPHA)) ||
		(pic_24bit = GL_LoadPicImage(va("gfx/%s", name), name, 0, 0, TEX_ALPHA))
	) {
		memcpy(&pic->texnum, &pic_24bit->texnum, sizeof(mpic_t) - 8);
		return pic;
	}

	// load little ones into the scrap
	if (p->width < 64 && p->height < 64) {
		int x, y, i, j, k, texnum;

		texnum = memchr(p->data, 255, p->width*p->height) != NULL;
		if (!Scrap_AllocBlock (texnum, p->width, p->height, &x, &y)) {
			GL_LoadPicTexture (name, pic, p->data);
			return pic;
		}
		k = 0;
		for (i = 0; i < p->height; i++)
			for (j  = 0; j < p->width; j++, k++)
				scrap_texels[texnum][(y + i) * BLOCK_WIDTH + x + j] = p->data[k];
		texnum += scrap_texnum;
		pic->texnum = texnum;
		pic->sl = (x + 0.01) / (float) BLOCK_WIDTH;
		pic->sh = (x + p->width - 0.01) / (float) BLOCK_WIDTH;
		pic->tl = (y + 0.01) / (float) BLOCK_WIDTH;
		pic->th = (y + p->height - 0.01) / (float) BLOCK_WIDTH;

		pic_count++;
		pic_texels += p->width * p->height;
	} else {
		GL_LoadPicTexture (name, pic, p->data);
	}

	return pic;
}

mpic_t *Draw_CachePic (char *path) {
	cachepic_t *pic;
	int i;
	qpic_t *dat;
	mpic_t *pic_24bit;

	for (pic = cachepics, i = 0; i < numcachepics; pic++, i++)
		if (!strcmp (path, pic->name))
			return &pic->pic;

	if (numcachepics == MAX_CACHED_PICS)
		Sys_Error ("numcachepics == MAX_CACHED_PICS");
	numcachepics++;
	Q_strncpyz (pic->name, path, sizeof(pic->name));

	// load the pic from disk
	if (!(dat = (qpic_t *)FS_LoadTempFile (path)))
		Sys_Error ("Draw_CachePic: failed to load %s", path);
	SwapPic (dat);

	// HACK HACK HACK --- we need to keep the bytes for
	// the translatable player picture just for the menu
	// configuration dialog
	if (!strcmp (path, "gfx/menuplyr.lmp"))
		memcpy (menuplyr_pixels, dat->data, dat->width*dat->height);

	pic->pic.width = dat->width;
	pic->pic.height = dat->height;

	
	if (pic_24bit = GL_LoadPicImage(path, NULL, 0, 0, TEX_ALPHA))
		memcpy(&pic->pic.texnum, &pic_24bit->texnum, sizeof(mpic_t) - 8);
	else
		GL_LoadPicTexture (path, &pic->pic, dat->data);

	return &pic->pic;
}

void Draw_InitConback (void) {
	qpic_t *cb;
	int start;
	mpic_t *pic_24bit;

	start = Hunk_LowMark ();

	if (!(cb = (qpic_t *) FS_LoadHunkFile ("gfx/conback.lmp")))
		Sys_Error ("Couldn't load gfx/conback.lmp");
	SwapPic (cb);

	if (cb->width != 320 || cb->height != 200)
		Sys_Error ("Draw_InitConback: conback.lmp size is not 320x200");


	if ((pic_24bit = GL_LoadPicImage("gfx/conback", "conback", 0, 0, 0))) {
		memcpy(&conback.texnum, &pic_24bit->texnum, sizeof(mpic_t) - 8);
	} else {
		conback.width = cb->width;
		conback.height = cb->height;
		GL_LoadPicTexture ("conback", &conback, cb->data);
	}
	conback.width = vid.conwidth;
	conback.height = vid.conheight;
	// free loaded console
	Hunk_FreeToLowMark (start);
}

static int Draw_LoadCharset(char *name) {
	int texnum;

	if (!Q_strcasecmp(name, "original")) {
		int i;
		char buf[128 * 256], *src, *dest;

		memset (buf, 255, sizeof(buf));
		src = draw_chars;
		dest = buf;
		for (i = 0; i < 16; i++) {
			memcpy (dest, src, 128 * 8);
			src += 128 * 8;
			dest += 128 * 8 * 2;
		}
		char_texture = GL_LoadTexture ("pic:charset", 128, 256, buf, TEX_ALPHA, 1);
		goto done;
	}

	if ((texnum = GL_LoadCharsetImage (va("textures/charsets/%s", name), "pic:charset"))) {
		char_texture = texnum;
		goto done;
	}

	Com_Printf ("Couldn't load charset \"%s\"\n", name);
	return 1;

done:
	if (!gl_smoothfont.value) {
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	return 0;
}

qboolean OnChange_gl_consolefont(cvar_t *var, char *string) {
	return Draw_LoadCharset(string);
}

void Draw_LoadCharset_f (void) {
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

void Draw_InitCharset(void) {
	int i;

	draw_chars = W_GetLumpName ("conchars");
	for (i = 0; i < 256 * 64; i++) {
		if (draw_chars[i] == 0)
			draw_chars[i] = 255;
	}

	Draw_LoadCharset(gl_consolefont.string);

	if (!char_texture)
		Cvar_Set(&gl_consolefont, "original");

	if (!char_texture)	
		Sys_Error("Draw_InitCharset: Couldn't load charset");
}

void Draw_Init (void) {
	int i;

	Cmd_AddCommand("loadcharset", Draw_LoadCharset_f);	

	Cvar_SetCurrentGroup(CVAR_GROUP_CONSOLE);
	Cvar_Register (&scr_conalpha);
	Cvar_Register (&gl_smoothfont);
	Cvar_Register (&gl_consolefont);	

	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&scr_menualpha);

	Cvar_SetCurrentGroup(CVAR_GROUP_CROSSHAIR);
	Cvar_Register (&gl_crosshairimage);	
	Cvar_Register (&gl_crosshairalpha);

	Cvar_ResetCurrentGroup();

	GL_Texture_Init();

	// load the console background and the charset by hand, because we need to write the version
	// string into the background before turning it into a texture
	Draw_InitCharset ();
	Draw_InitConback ();

	// save a texture slot for translated picture
	translate_texture = texture_extension_number++;

	// save slots for scraps
	scrap_texnum = texture_extension_number;
	texture_extension_number += MAX_SCRAPS;

	// Load the crosshair pics
	for (i = 0; i < NUMCROSSHAIRS; i++) {
		crosshairtextures[i] = GL_LoadTexture ("", 8, 8, crosshairdata[i], TEX_ALPHA, 1);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameterf (GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}
	customCrosshair_Init();		
	// get the other pics we need
	draw_disc = Draw_CacheWadPic ("disc");
	draw_backtile = Draw_CacheWadPic ("backtile");
}

__inline static void Draw_CharPoly(int x, int y, int num) {
	float frow, fcol;

	frow = (num >> 4) * 0.0625;
	fcol = (num & 15) * 0.0625;

	glTexCoord2f (fcol, frow);
	glVertex2f (x, y);
	glTexCoord2f (fcol + 0.0625, frow);
	glVertex2f (x + 8, y);
	glTexCoord2f (fcol + 0.0625, frow + 0.03125);
	glVertex2f (x + 8, y + 8);
	glTexCoord2f (fcol, frow + 0.03125);
	glVertex2f (x, y + 8);
}

//Draws one 8*8 graphics character with 0 being transparent.
//It can be clipped to the top of the screen to allow the console to be smoothly scrolled off.
void Draw_Character (int x, int y, int num) {

	if (y <= -8)
		return;			// totally off screen

	if (num == 32)
		return;		// space

	num &= 255;


	GL_Bind (char_texture);

	glBegin (GL_QUADS);
	Draw_CharPoly(x, y, num);
	glEnd ();
}

void Draw_String (int x, int y, char *str) {
	int num;

	if (y <= -8)
		return;			// totally off screen

	if (!*str)
		return;

	GL_Bind (char_texture);

	glBegin (GL_QUADS);

	while (*str) {	// stop rendering when out of characters		
		if ((num = *str++) != 32)	// skip spaces
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();
}

void Draw_Alt_String (int x, int y, char *str) {
	int num;

	if (y <= -8)
		return;			// totally off screen

	if (!*str)
		return;

	GL_Bind (char_texture);

	glBegin (GL_QUADS);

	while (*str) {// stop rendering when out of characters
		if ((num = *str++ | 128) != (32 | 128))	// skip spaces
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();
}


static int HexToInt(char c) {
	if (isdigit(c))
		return c - '0';
	else if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	else if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	else
		return -1;
}

void Draw_ColoredString (int x, int y, char *text, int red) {
	int r, g, b, num;
	qboolean white = true;

	if (y <= -8)
		return;			// totally off screen

	if (!*text)
		return;

	GL_Bind (char_texture);

	if (scr_coloredText.value)
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

	glBegin (GL_QUADS);

	for ( ; *text; text++) {
		
		if (*text == '&') {
			if (text[1] == 'c' && text[2] && text[3] && text[4])	{
				r = HexToInt(text[2]);
				g = HexToInt(text[3]);
				b = HexToInt(text[4]);
				if (r >= 0 && g >= 0 && b >= 0) {
					if (scr_coloredText.value) {
						glColor3f(r / 16.0, g / 16.0, b / 16.0);
						white = false;
					}
					text += 5;
				}
            } else if (text[1] == 'r')	{
				if (!white) {
					glColor3ubv(color_white);
					white = true;
				}
				text += 2;
			}
		}

		num = *text & 255;
		if (!scr_coloredText.value && red)
			num |= 128;

		if (num != 32 && num != (32 | 128))
			Draw_CharPoly(x, y, num);

		x += 8;
	}

	glEnd ();

	if (!white)
		glColor3ubv(color_white);

	if (scr_coloredText.value)
		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
}


void Draw_Crosshair (void) {
	float x, y, ofs1, ofs2, sh, th, sl, tl;
	byte *col;
	extern vrect_t scr_vrect;

	if ((crosshair.value >= 2 && crosshair.value <= NUMCROSSHAIRS + 1) || 
		((customcrosshair_loaded & CROSSHAIR_TXT) && crosshair.value == 1) ||
		(customcrosshair_loaded & CROSSHAIR_IMAGE)
	) {
		x = scr_vrect.x + scr_vrect.width / 2 + cl_crossx.value; 
		y = scr_vrect.y + scr_vrect.height / 2 + cl_crossy.value;


		if (!gl_crosshairalpha.value)
			return;


		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);


		col = StringToRGB(crosshaircolor.string);



		if (gl_crosshairalpha.value) {
            glDisable(GL_ALPHA_TEST);
            glEnable (GL_BLEND);
			col[3] = bound(0, gl_crosshairalpha.value, 1) * 255;
			glColor4ubv (col);
		} else {
			glColor3ubv (col);
		}


		if (customcrosshair_loaded & CROSSHAIR_IMAGE) {
			GL_Bind (crosshairpic.texnum);
			ofs1 = 4 - 4.0 / crosshairpic.width;
			ofs2 = 4 + 4.0 / crosshairpic.width;
			sh = crosshairpic.sh;
			sl = crosshairpic.sl;
			th = crosshairpic.th;
			tl = crosshairpic.tl;
		} else {
			GL_Bind ((crosshair.value >= 2) ? crosshairtextures[(int) crosshair.value - 2] : crosshairtexture_txt);	
			ofs1 = 3.5;
			ofs2 = 4.5;
			tl = sl = 0;
			sh = th = 1;
		}
		ofs1 *= (vid.width / 320) * bound(0, crosshairsize.value, 20);
		ofs2 *= (vid.width / 320) * bound(0, crosshairsize.value, 20);

		glBegin (GL_QUADS);
		glTexCoord2f (sl, tl);
		glVertex2f (x - ofs1, y - ofs1);
		glTexCoord2f (sh, tl);
		glVertex2f (x + ofs2, y - ofs1);
		glTexCoord2f (sh, th);
		glVertex2f (x + ofs2, y + ofs2);
		glTexCoord2f (sl, th);
		glVertex2f (x - ofs1, y + ofs2);
		glEnd ();


		if (gl_crosshairalpha.value) {
			glDisable(GL_BLEND);
			glEnable (GL_ALPHA_TEST);
		}


		glTexEnvf (GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
		glColor3ubv (color_white);
	} else if (crosshair.value) {
		Draw_Character (scr_vrect.x + scr_vrect.width / 2 - 4 + cl_crossx.value, scr_vrect.y + scr_vrect.height / 2 - 4 + cl_crossy.value, '+');
	}
}

void Draw_TextBox (int x, int y, int width, int lines) {
	mpic_t *p;
	int cx, cy, n;

	// draw left side
	cx = x;
	cy = y;
	p = Draw_CachePic ("gfx/box_tl.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_ml.lmp");
	for (n = 0; n < lines; n++) {
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_bl.lmp");
	Draw_TransPic (cx, cy+8, p);

	// draw middle
	cx += 8;
	while (width > 0) {
		cy = y;
		p = Draw_CachePic ("gfx/box_tm.lmp");
		Draw_TransPic (cx, cy, p);
		p = Draw_CachePic ("gfx/box_mm.lmp");
		for (n = 0; n < lines; n++) {
			cy += 8;
			if (n == 1)
				p = Draw_CachePic ("gfx/box_mm2.lmp");
			Draw_TransPic (cx, cy, p);
		}
		p = Draw_CachePic ("gfx/box_bm.lmp");
		Draw_TransPic (cx, cy+8, p);
		width -= 2;
		cx += 16;
	}

	// draw right side
	cy = y;
	p = Draw_CachePic ("gfx/box_tr.lmp");
	Draw_TransPic (cx, cy, p);
	p = Draw_CachePic ("gfx/box_mr.lmp");
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		Draw_TransPic (cx, cy, p);
	}
	p = Draw_CachePic ("gfx/box_br.lmp");
	Draw_TransPic (cx, cy+8, p);
}

void Draw_Pic (int x, int y, mpic_t *pic) {
	if (scrap_dirty)
		Scrap_Upload ();
	GL_Bind (pic->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (pic->sl, pic->tl);
	glVertex2f (x, y);
	glTexCoord2f (pic->sh, pic->tl);
	glVertex2f (x + pic->width, y);
	glTexCoord2f (pic->sh, pic->th);
	glVertex2f (x + pic->width, y + pic->height);
	glTexCoord2f (pic->sl, pic->th);
	glVertex2f (x, y + pic->height);
	glEnd ();
}

void Draw_AlphaPic (int x, int y, mpic_t *pic, float alpha) {
	if (scrap_dirty)
		Scrap_Upload ();
	glDisable(GL_ALPHA_TEST);
	glEnable (GL_BLEND);
//	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glCullFace(GL_FRONT);
	glColor4f (1, 1, 1, alpha);
	GL_Bind (pic->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (pic->sl, pic->tl);
	glVertex2f (x, y);
	glTexCoord2f (pic->sh, pic->tl);
	glVertex2f (x + pic->width, y);
	glTexCoord2f (pic->sh, pic->th);
	glVertex2f (x + pic->width, y + pic->height);
	glTexCoord2f (pic->sl, pic->th);
	glVertex2f (x, y + pic->height);
	glEnd ();
	glColor3ubv (color_white);
	glEnable(GL_ALPHA_TEST);
	glDisable (GL_BLEND);
}

void Draw_SubPic(int x, int y, mpic_t *pic, int srcx, int srcy, int width, int height) {
	float newsl, newtl, newsh, newth, oldglwidth, oldglheight;

	if (scrap_dirty)
		Scrap_Upload ();

	oldglwidth = pic->sh - pic->sl;
	oldglheight = pic->th - pic->tl;

	newsl = pic->sl + (srcx * oldglwidth) / pic->width;
	newsh = newsl + (width * oldglwidth) / pic->width;

	newtl = pic->tl + (srcy * oldglheight) / pic->height;
	newth = newtl + (height * oldglheight) / pic->height;

	GL_Bind (pic->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (newsl, newtl);
	glVertex2f (x, y);
	glTexCoord2f (newsh, newtl);
	glVertex2f (x + width, y);
	glTexCoord2f (newsh, newth);
	glVertex2f (x + width, y + height);
	glTexCoord2f (newsl, newth);
	glVertex2f (x, y + height);
	glEnd ();
}

void Draw_TransPic (int x, int y, mpic_t *pic) {
	if (x < 0 || (unsigned) (x + pic->width) > vid.width || y < 0 || (unsigned) (y + pic->height) > vid.height)
		Sys_Error ("Draw_TransPic: bad coordinates");
		
	Draw_Pic (x, y, pic);
}

//Only used for the player color selection menu
void Draw_TransPicTranslate (int x, int y, mpic_t *pic, byte *translation) {
	int v, u, c, p;
	unsigned trans[64 * 64], *dest;
	byte *src;

	GL_Bind (translate_texture);

	c = pic->width * pic->height;

	dest = trans;
	for (v = 0; v < 64; v++, dest += 64) {
		src = &menuplyr_pixels[ ((v * pic->height) >> 6) *pic->width];
		for (u = 0; u < 64; u++) {
			p = src[(u * pic->width) >> 6];
			dest[u] = (p == 255) ? p : d_8to24table[translation[p]];
		}
	}

	glTexImage2D (GL_TEXTURE_2D, 0, gl_alpha_format, 64, 64, 0, GL_RGBA, GL_UNSIGNED_BYTE, trans);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glBegin (GL_QUADS);
	glTexCoord2f (0, 0);
	glVertex2f (x, y);
	glTexCoord2f (1, 0);
	glVertex2f (x + pic->width, y);
	glTexCoord2f (1, 1);
	glVertex2f (x + pic->width, y + pic->height);
	glTexCoord2f (0, 1);
	glVertex2f (x, y + pic->height);
	glEnd ();
}

void Draw_ConsoleBackground (int lines) {
	char ver[80];

	if (SCR_NEED_CONSOLE_BACKGROUND) {
		Draw_Pic(0, lines - vid.height, &conback);
	} else {
		if (scr_conalpha.value)
			Draw_AlphaPic (0, lines - vid.height, &conback, bound (0, scr_conalpha.value, 1));
	}

	sprintf (ver, "FuhQuake %s", FUH_VERSION);
	Draw_Alt_String (vid.conwidth - strlen(ver) * 8 - 8, lines - 10, ver);
}

//This repeats a 64 * 64 tile graphic to fill the screen around a sized down refresh window.
void Draw_TileClear (int x, int y, int w, int h) {
	GL_Bind (draw_backtile->texnum);
	glBegin (GL_QUADS);
	glTexCoord2f (x / 64.0, y / 64.0);
	glVertex2f (x, y);
	glTexCoord2f ((x + w) / 64.0, y / 64.0);
	glVertex2f (x + w, y);
	glTexCoord2f ((x + w) / 64.0, (y + h) / 64.0);
	glVertex2f (x + w, y + h);
	glTexCoord2f (x / 64.0, (y + h) / 64.0 );
	glVertex2f (x, y + h);
	glEnd ();
}

void Draw_AlphaFill (int x, int y, int w, int h, int c, float alpha) {
	alpha = bound(0, alpha, 1);

	if (!alpha)
		return;

	glDisable (GL_TEXTURE_2D);
	if (alpha < 1) {
		glEnable (GL_BLEND);
		glDisable(GL_ALPHA_TEST);
		glColor4f (host_basepal[c * 3] / 255.0,  host_basepal[c * 3 + 1] / 255.0, host_basepal[c * 3 + 2] / 255.0, alpha);
	} else {
		glColor3f (host_basepal[c * 3] / 255.0, host_basepal[c * 3 + 1] / 255.0, host_basepal[c * 3 + 2]  /255.0);
	}

	glBegin (GL_QUADS);
	glVertex2f (x, y);
	glVertex2f (x + w, y);
	glVertex2f (x + w, y + h);
	glVertex2f (x, y + h);
	glEnd ();

	glEnable (GL_TEXTURE_2D);
	if (alpha < 1) {
		glEnable(GL_ALPHA_TEST);
		glDisable (GL_BLEND);
	}
	glColor3ubv (color_white);
}

void Draw_Fill (int x, int y, int w, int h, int c) {
	Draw_AlphaFill(x, y, w, h, c, 1);
}

//=============================================================================

void Draw_FadeScreen (void) {
	float alpha;

	alpha = bound(0, scr_menualpha.value, 1);
	if (!alpha)
		return;

	if (alpha < 1) {
		glDisable (GL_ALPHA_TEST);
		glEnable (GL_BLEND);
		glColor4f (0, 0, 0, alpha);
	} else {
		glColor3f (0, 0, 0);
	}
	glDisable (GL_TEXTURE_2D);

	glBegin (GL_QUADS);
	glVertex2f (0, 0);
	glVertex2f (vid.width, 0);
	glVertex2f (vid.width, vid.height);
	glVertex2f (0, vid.height);
	glEnd ();

	if (alpha < 1) {
		glDisable (GL_BLEND);
		glEnable (GL_ALPHA_TEST);
	}
	glColor3ubv (color_white);
	glEnable (GL_TEXTURE_2D);

	Sbar_Changed();
}

//=============================================================================

//Draws the little blue disc in the corner of the screen. 
//Call before beginning any disc IO.
void Draw_BeginDisc (void) {
	if (!draw_disc)
		return;
	glDrawBuffer  (GL_FRONT);
	Draw_Pic (vid.width - 24, 0, draw_disc);
	glDrawBuffer  (GL_BACK);
}

//Erases the disc icon.
//Call after completing any disc IO
void Draw_EndDisc (void) {}

void GL_Set2D (void) {
	glViewport (glx, gly, glwidth, glheight);

	glMatrixMode(GL_PROJECTION);
    glLoadIdentity ();
	glOrtho (0, vid.width, vid.height, 0, -99999, 99999);

	glMatrixMode(GL_MODELVIEW);
    glLoadIdentity ();

	glDisable (GL_DEPTH_TEST);
	glDisable (GL_CULL_FACE);
	glDisable (GL_BLEND);
	glEnable (GL_ALPHA_TEST);

	glColor3ubv (color_white);
}
