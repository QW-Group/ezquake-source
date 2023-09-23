/*
Copyright (C) 2001-2002       A Nourai

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: gl_texture.c,v 1.44 2007-10-05 19:06:24 johnnycz Exp $
*/

#include "quakedef.h"
#include "r_local.h"
#include "r_texture.h"
#include "r_texture_internal.h"
#include "tr_types.h"
#include "gl_texture.h"
#include "r_renderer.h"

static void OnChange_gl_max_size(cvar_t *var, char *string, qbool *cancel);
static void OnChange_gl_texturemode(cvar_t *var, char *string, qbool *cancel);
static void OnChange_gl_miptexLevel(cvar_t *var, char *string, qbool *cancel);
static void OnChange_gl_anisotropy(cvar_t *var, char *string, qbool *cancel);

cvar_t gl_lerpimages = { "gl_lerpimages", "1", CVAR_RELOAD_GFX };
static cvar_t gl_externalTextures_world = { "gl_externalTextures_world", "1", CVAR_RELOAD_GFX };
static cvar_t gl_externalTextures_bmodels = { "gl_externalTextures_bmodels", "1", CVAR_RELOAD_GFX };
cvar_t gl_wicked_luma_level = { "gl_luma_level", "1", CVAR_RELOAD_GFX };

static int anisotropy_tap = 1; //  1 - is off

cvar_t gl_max_size = { "gl_max_size", "2048", CVAR_RELOAD_GFX, OnChange_gl_max_size };
cvar_t gl_picmip = { "gl_picmip", "0", CVAR_RELOAD_GFX };
cvar_t gl_miptexLevel = { "gl_miptexLevel", "0", CVAR_RELOAD_GFX, OnChange_gl_miptexLevel };
cvar_t gl_texturemode = { "gl_texturemode", "GL_LINEAR_MIPMAP_LINEAR", 0, OnChange_gl_texturemode };
cvar_t gl_texturemode_viewmodels = { "gl_texturemode_viewmodels", "GL_LINEAR", 0, OnChange_gl_texturemode };
cvar_t gl_anisotropy = { "gl_anisotropy","16", 0, OnChange_gl_anisotropy };
cvar_t gl_scaleModelTextures = { "gl_scaleModelTextures", "0", CVAR_RELOAD_GFX };
cvar_t gl_scaleModelSimpleTextures = { "gl_scaleModelSimpleTextures", "0", CVAR_RELOAD_GFX };
cvar_t gl_scaleTurbTextures = { "gl_scaleTurbTextures", "1", CVAR_RELOAD_GFX };
cvar_t gl_scaleAlphaTextures = { "gl_scaleAlphaTextures", "0", CVAR_RELOAD_GFX };
cvar_t gl_scaleskytextures = { "gl_scaleskytextures", "0", CVAR_RELOAD_GFX };
cvar_t gl_no24bit = { "gl_no24bit", "0", CVAR_RELOAD_GFX };

static void OnChange_gl_max_size(cvar_t *var, char *string, qbool *cancel)
{
	int i;
	float newvalue = Q_atof(string);

	if (newvalue > glConfig.gl_max_size_default) {
		Com_Printf("Your hardware doesn't support texture sizes bigger than %dx%d\n", glConfig.gl_max_size_default, glConfig.gl_max_size_default);
		*cancel = true;
		return;
	}

	Q_ROUND_POWER2(newvalue, i);

	if (i != newvalue) {
		Com_Printf("Valid values for %s are powers of 2 only\n", var->name);
		*cancel = true;
		return;
	}
}

void R_TextureAnisotropyChanged(texture_ref tex, qbool mipmap, qbool viewmodel)
{
	if (mipmap || viewmodel) {
		renderer.TextureSetAnisotropy(tex, anisotropy_tap);
	}
}

static void OnChange_gl_anisotropy(cvar_t *var, char *string, qbool *cancel)
{
	int newvalue = Q_atoi(string);

	anisotropy_tap = max(1, newvalue); // 0 is bad, 1 is off, 2 and higher are valid modes

	R_TextureModeForEach(R_TextureAnisotropyChanged);
}

static void OnChange_gl_miptexLevel(cvar_t *var, char *string, qbool *cancel)
{
	float newval = Q_atof(string);

	if (newval != 0 && newval != 1 && newval != 2 && newval != 3) {
		Com_Printf("Valid values for %s are 0,1,2,3 only\n", var->name);
		*cancel = true;
	}
}

typedef struct {
	char *name;
	texture_minification_id	minimize;
	texture_magnification_id maximize;
} glmode_t;

static glmode_t modes[] = {
	{ "GL_NEAREST", texture_minification_nearest, texture_magnification_nearest },
	{ "GL_LINEAR", texture_minification_linear, texture_magnification_linear },
	{ "GL_NEAREST_MIPMAP_NEAREST", texture_minification_nearest_mipmap_nearest, texture_magnification_nearest },
	{ "GL_LINEAR_MIPMAP_NEAREST", texture_minification_linear_mipmap_nearest, texture_magnification_linear },
	{ "GL_NEAREST_MIPMAP_LINEAR", texture_minification_nearest_mipmap_linear, texture_magnification_nearest },
	{ "GL_LINEAR_MIPMAP_LINEAR", texture_minification_linear_mipmap_linear, texture_magnification_linear }
};

static texture_minification_id gl_filter_min = texture_minification_linear_mipmap_nearest;
static texture_magnification_id gl_filter_max = texture_magnification_linear;
static texture_minification_id gl_filter_viewmodel_min = texture_minification_linear;
static texture_magnification_id gl_filter_viewmodel_max = texture_magnification_linear;
static const texture_minification_id gl_filter_min_2d = texture_minification_linear;
static const texture_magnification_id gl_filter_max_2d = texture_magnification_linear;   // no longer controlled by cvar

void R_TextureModeChanged(texture_ref tex, qbool mipmap, qbool viewmodel)
{
	if (viewmodel) {
		renderer.TextureSetFiltering(tex, gl_filter_viewmodel_min, gl_filter_viewmodel_max);
	}
	else if (mipmap) {
		renderer.TextureSetFiltering(tex, gl_filter_min, gl_filter_max);
	}
	else {
		renderer.TextureSetFiltering(tex, gl_filter_min_2d, gl_filter_max_2d);
	}
}

static void OnChange_gl_texturemode(cvar_t *var, char *string, qbool *cancel)
{
	int i;

	for (i = 0; i < (sizeof(modes) / sizeof(glmode_t)); i++) {
		if (!strcasecmp(modes[i].name, string)) {
			break;
		}
	}

	if (i == (sizeof(modes) / sizeof(glmode_t))) {
		Com_Printf("bad filter name: %s\n", string);
		*cancel = true;
		return;
	}

	if (var == &gl_texturemode) {
		gl_filter_min = modes[i].minimize;
		gl_filter_max = modes[i].maximize;
	}
	else if (var == &gl_texturemode_viewmodels) {
		gl_filter_viewmodel_min = modes[i].minimize;
		gl_filter_viewmodel_max = modes[i].maximize;
	}
	else {
		Sys_Error("OnChange_gl_texturemode: unexpected cvar!");
		return;
	}

	R_TextureModeForEach(R_TextureModeChanged);
}

qbool R_ExternalTexturesEnabled(qbool worldmodel)
{
	return !gl_no24bit.integer && (worldmodel ? gl_externalTextures_world.integer : gl_externalTextures_bmodels.integer);
}

void R_TextureRegisterCvars(void)
{
	int i;
	cvar_t* cv;

	if (!host_initialized) {
		Cvar_SetCurrentGroup(CVAR_GROUP_TEXTURES);
		Cvar_Register(&gl_max_size);
		Cvar_Register(&gl_scaleModelTextures);
		Cvar_Register(&gl_scaleModelSimpleTextures);
		Cvar_Register(&gl_scaleTurbTextures);
		Cvar_Register(&gl_scaleskytextures);
		Cvar_Register(&gl_scaleAlphaTextures);
		Cvar_Register(&gl_miptexLevel);
		Cvar_Register(&gl_picmip);
		Cvar_Register(&gl_lerpimages);
		Cvar_Register(&gl_texturemode);
		Cvar_Register(&gl_texturemode_viewmodels);
		Cvar_Register(&gl_anisotropy);
		Cvar_Register(&gl_externalTextures_world);
		Cvar_Register(&gl_externalTextures_bmodels);

		Cvar_Register(&gl_no24bit);
		Cvar_Register(&gl_wicked_luma_level);
		Cvar_ResetCurrentGroup();
	}

	// This way user can specify gl_max_size in his cfg.
	i = (cv = Cvar_Find(gl_max_size.name)) ? cv->integer : 0;
	Cvar_SetDefaultAndValue(&gl_max_size, glConfig.gl_max_size_default, i ? i : glConfig.gl_max_size_default);
}
