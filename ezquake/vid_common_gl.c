/*
Copyright (C) 2002-2003 A Nourai

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

// vid_common_gl.c -- Common code for vid_wgl.c and vid_glx.c

#include "quakedef.h"
#include "gl_local.h"

#ifdef _WIN32
#define qglGetProcAddress wglGetProcAddress
#else
#define qglGetProcAddress glXGetProcAddressARB
#endif

const char *gl_vendor;
const char *gl_renderer;
const char *gl_version;
const char *gl_extensions;

qboolean gl_mtexable = false;
int gl_textureunits = 1;
lpMTexFUNC qglMultiTexCoord2f = NULL;
lpSelTexFUNC qglActiveTexture = NULL;

qboolean gl_combine = false;

qboolean gl_add_ext = false;

float gldepthmin, gldepthmax;
qboolean gl_allow_ztrick = true;

float vid_gamma = 1.0;
byte vid_gamma_table[256];

unsigned short d_8to16table[256];
unsigned d_8to24table[256];
unsigned d_8to24table2[256];

byte color_white[4] = {255, 255, 255, 0};
byte color_black[4] = {0, 0, 0, 0};

qboolean OnChange_gl_ext_texture_compression(cvar_t *, char *);

cvar_t	gl_strings = {"gl_strings", "", CVAR_ROM};
cvar_t	gl_ext_texture_compression = {"gl_ext_texture_compression", "0", 0, OnChange_gl_ext_texture_compression};

/************************************* EXTENSIONS *************************************/

qboolean CheckExtension (const char *extension) {
	const char *start;
	char *where, *terminator;

	if (!gl_extensions && !(gl_extensions = glGetString (GL_EXTENSIONS)))
		return false;

	
	if (!extension || *extension == 0 || strchr (extension, ' '))
		return false;

	for (start = gl_extensions; where = strstr(start, extension); start = terminator) {
		terminator = where + strlen (extension);
		if ((where == start || *(where - 1) == ' ') && (*terminator == 0 || *terminator == ' '))
			return true;
	}
	return false;
}

void CheckMultiTextureExtensions (void) {
	if (!COM_CheckParm("-nomtex") && CheckExtension("GL_ARB_multitexture")) {
		if (strstr(gl_renderer, "Savage"))
			return;
		qglMultiTexCoord2f = (void *) qglGetProcAddress("glMultiTexCoord2fARB");
		qglActiveTexture = (void *) qglGetProcAddress("glActiveTextureARB");
		if (!qglMultiTexCoord2f || !qglActiveTexture)
			return;
		Com_Printf ("Multitexture extensions found\n");
		gl_mtexable = true;
	}

	glGetIntegerv(GL_MAX_TEXTURE_UNITS_ARB, &gl_textureunits);
	gl_textureunits = min(gl_textureunits, 4);

	if (COM_CheckParm("-maxtmu2") || !strcmp(gl_vendor, "ATI Technologies Inc."))
		gl_textureunits = min(gl_textureunits, 2);

	if (gl_textureunits < 2)
		gl_mtexable = false;

	if (!gl_mtexable)
		gl_textureunits = 1;
	else
		Com_Printf("Enabled %i texture units on hardware\n", gl_textureunits);
}

void GL_CheckExtensions (void) {
	CheckMultiTextureExtensions ();

	gl_combine = CheckExtension("GL_ARB_texture_env_combine");
		gl_add_ext = CheckExtension("GL_ARB_texture_env_add");

	if (CheckExtension("GL_ARB_texture_compression")) {
		Com_Printf("Texture compression extensions found\n");
		Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
		Cvar_Register (&gl_ext_texture_compression);	
		Cvar_ResetCurrentGroup();
	}
}

qboolean OnChange_gl_ext_texture_compression(cvar_t *var, char *string) {
	float newval = Q_atof(string);

	if (!newval == !var->value)
		return false;

	
	gl_alpha_format = newval ? GL_COMPRESSED_RGBA_ARB : GL_RGBA;
	gl_solid_format = newval ? GL_COMPRESSED_RGB_ARB : GL_RGB;

	return false;
}

/************************************** GL INIT **************************************/

void GL_Init (void) {
	gl_vendor = glGetString (GL_VENDOR);
	Com_Printf ("GL_VENDOR: %s\n", gl_vendor);
	gl_renderer = glGetString (GL_RENDERER);
	Com_Printf ("GL_RENDERER: %s\n", gl_renderer);
	gl_version = glGetString (GL_VERSION);
	Com_Printf ("GL_VERSION: %s\n", gl_version);
	gl_extensions = glGetString (GL_EXTENSIONS);
	if (COM_CheckParm("-gl_ext"))
		Com_Printf ("GL_EXTENSIONS: %s\n", gl_extensions);

	Cvar_Register (&gl_strings);
	Cvar_ForceSet (&gl_strings, va("GL_VENDOR: %s\nGL_RENDERER: %s\n"
		"GL_VERSION: %s\nGL_EXTENSIONS: %s", gl_vendor, gl_renderer, gl_version, gl_extensions));

	glClearColor (1,0,0,0);
	glCullFace(GL_FRONT);
	glEnable(GL_TEXTURE_2D);

	glEnable(GL_ALPHA_TEST);
	glAlphaFunc(GL_GREATER, 0.666);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);
	glShadeModel (GL_FLAT);

	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);

	GL_CheckExtensions();
}

/************************************* VID GAMMA *************************************/

void Check_Gamma (unsigned char *pal) {
	float inf;
	unsigned char palette[768];
	int i;

	if ((i = COM_CheckParm("-gamma")) != 0 && i + 1 < com_argc)
		vid_gamma = bound (0.3, Q_atof(com_argv[i + 1]), 1);
	else
		vid_gamma = 1;

	Cvar_SetDefault (&v_gamma, vid_gamma);

	if (vid_gamma != 1){
		for (i = 0; i < 256; i++){
			inf = 255 * pow((i + 0.5) / 255.5, vid_gamma) + 0.5;
			if (inf > 255)
				inf = 255;
			vid_gamma_table[i] = inf;
		}
	} else {
		for (i = 0; i < 256; i++)
			vid_gamma_table[i] = i;
	}

	for (i = 0; i < 768; i++)
		palette[i] = vid_gamma_table[pal[i]];

	memcpy (pal, palette, sizeof(palette));
}

/************************************* HW GAMMA *************************************/

void VID_SetPalette (unsigned char *palette) {
	int i;
	byte *pal;
	unsigned r,g,b, v, *table;

	// 8 8 8 encoding
	pal = palette;
	table = d_8to24table;
	for (i = 0; i < 256; i++) {
		r = pal[0];
		g = pal[1];
		b = pal[2];
		pal += 3;

		v = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
		*table++ = v;
	}
	d_8to24table[255] = 0;	// 255 is transparent

	// Tonik: create a brighter palette for bmodel textures
	pal = palette;
	table = d_8to24table2;

	for (i = 0; i < 256; i++) {
		r = pal[0] * (2.0 / 1.5); if (r > 255) r = 255;
		g = pal[1] * (2.0 / 1.5); if (g > 255) g = 255;
		b = pal[2] * (2.0 / 1.5); if (b > 255) b = 255;
		pal += 3;
		*table++ = (255 << 24) + (r << 0) + (g << 8) + (b << 16);
	}
	d_8to24table2[255] = 0;	// 255 is transparent
}
