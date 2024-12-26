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
// gl_sky.c -- sky polygons (was previously in gl_warp.c)

#include "quakedef.h"
#include "gl_model.h"
#include "teamplay.h"
#include "r_brushmodel_sky.h"
#include "r_texture.h"
#include "r_local.h"
#include "r_trace.h"
#include "r_renderer.h"
#include "tr_types.h"

static qbool Sky_LoadSkyboxTextures(const char* skyname);

texture_ref solidskytexture, alphaskytexture;
texture_ref skyboxtextures[MAX_SKYBOXTEXTURES];
texture_ref skybox_cubeMap;

qbool r_skyboxloaded;

extern cvar_t r_skywind;

static float skywind_dist;
static float skywind_yaw;
static float skywind_pitch;
static float skywind_period;

static char active_skyname[MAX_QPATH];

#define SKYWIND_CFG			"_wind.cfg"

static void Skywind_Load_f(void);
static void Skywind_Clear(void);

//A sky texture is 256 * 128, with the right side being a masked overlay
void R_InitSky (texture_t *mt) {
	int i, j, p, r, g, b;
	byte *src;
	unsigned trans[128 * 128], transpix, *rgba;
	int flags = TEX_MIPMAP | (gl_scaleskytextures.integer ? 0 : TEX_NOSCALE);

	src = (byte *) mt + mt->offsets[0];

	// make an average value for the back to avoid a fringe on the top level
	r = g = b = 0;
	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j + 128];
			rgba = &d_8to24table[p];
			trans[(i * 128) + j] = *rgba;
			r += ((byte *) rgba)[0];
			g += ((byte *) rgba)[1];
			b += ((byte *) rgba)[2];
		}
	}

	((byte *) &transpix)[0] = r / (128 * 128);
	((byte *) &transpix)[1] = g / (128 * 128);
	((byte *) &transpix)[2] = b / (128 * 128);
	((byte *) &transpix)[3] = 0;

	solidskytexture = R_LoadTexture ("***solidskytexture***", 128, 128, (byte *)trans, flags, 4);

	for (i = 0; i < 128; i++) {
		for (j = 0; j < 128; j++) {
			p = src[i * 256 + j];
			trans[(i * 128) + j] = p ? d_8to24table[p] : transpix;
		}
	}

	alphaskytexture = R_LoadTexture ("***alphaskytexture***", 128, 128, (byte *)trans, TEX_ALPHA | flags, 4);
}

int R_SetSky(char *skyname)
{
	char *groupname;

	memset(active_skyname, 0, sizeof(active_skyname));
	r_skyboxloaded = false;
	Skywind_Clear();

	// set skyname to groupname if any
	skyname	= (groupname = TP_GetSkyGroupName(TP_MapName(), NULL)) ? groupname : skyname;

	if (!skyname || !skyname[0] || strchr(skyname, '.'))
	{
		// Empty name or contain dot(dot causing troubles with skipping part of the name as file extenson),
		// so do nothing.
		return 1;
	}

	if (!Sky_LoadSkyboxTextures(skyname)) {
		return 1;
	}

	// everything was OK
	r_skyboxloaded = true;

	strlcpy(active_skyname, skyname, sizeof(active_skyname));

	if (r_skywind.integer) {
		Skywind_Load_f();
	}

	return 0;
}

void OnChange_r_skyname (cvar_t *v, char *skyname, qbool* cancel)
{
	if (!skyname[0]) {
		memset(active_skyname, 0, sizeof(active_skyname));
		r_skyboxloaded = false;
		return;
	}

	*cancel = R_SetSky(skyname);
}

static void R_LoadSky_f(void)
{
	switch (Cmd_Argc()) {
	case 1:
		if (r_skyboxloaded) {
			Com_Printf("Current skybox is \"%s\"\n", r_skyname.string);
		}
		else {
			Com_Printf("No skybox has been set\n");
		}
		break;
	case 2:
		if (!strcasecmp(Cmd_Argv(1), "none")) {
			Cvar_Set(&r_skyname, "");
		}
		else {
			Cvar_Set(&r_skyname, Cmd_Argv(1));
		}
		break;
	default:
		Com_Printf("Usage: %s <skybox>\n", Cmd_Argv(0));
		break;
	}
}

/*
==============
R_DrawSky

Draw either the classic cloudy quake sky or a skybox
==============
*/
void R_DrawSky (void)
{
	if (!skychain) {
		return;
	}

	R_TraceEnterNamedRegion("R_DrawSky");
	renderer.DrawSky();
	R_TraceLeaveNamedRegion();

	skychain = NULL;
	skychain_tail = &skychain;
}

static qbool R_LoadSkyTexturePixels(r_cubemap_direction_id dir, const char* skyname, byte** data, int* width, int* height)
{
	static const char *skybox_ext[r_cubemap_direction_count] = { "rt", "bk", "lf", "ft", "up", "dn" };
	static const char* search_paths[][2] = { { "env/", "" }, { "gfx/env/", "" }, { "env/", "_" }, { "gfx/env/", "_" } };
	int j, flags = TEX_NOCOMPRESS | TEX_MIPMAP | (gl_scaleskytextures.integer ? 0 : TEX_NOSCALE);

	if (dir < 0 || dir >= r_cubemap_direction_count) {
		return false;
	}

	for (j = 0; j < sizeof(search_paths) / sizeof(search_paths[0]); ++j) {
		char path[MAX_PATH];

		strlcpy(path, search_paths[j][0], sizeof(path));
		strlcat(path, skyname, sizeof(path));
		strlcat(path, search_paths[j][1], sizeof(path));
		strlcat(path, skybox_ext[dir], sizeof(path));

		*data = R_LoadImagePixels(path, 0, 0, flags, width, height);
		if (*data) {
			return *data != NULL;
		}
	}

	return false;
}

static qbool Sky_LoadSkyboxTextures(const char* skyname)
{
	static int skydirection[] = { 4, 1, 5, 0, 2, 3 };

	r_cubemap_direction_id i;
	byte* data;
	int fixed_size = 0;
	int width, height;

	R_DeleteTexture(&skybox_cubeMap);
	for (i = 0; i < MAX_SKYBOXTEXTURES; i++) {
		R_DeleteTexture(&skyboxtextures[i]);
	}

	for (i = 0; i < MAX_SKYBOXTEXTURES; i++) {
		if (R_LoadSkyTexturePixels(i, skyname, &data, &width, &height)) {
			if (R_UseCubeMapForSkyBox()) {
				int size = (i != 0 ? fixed_size : min(width, height));

				R_TextureRescaleOverlay(&data, &width, &height, size, size);

				fixed_size = size;

				if (i == 0) {
					skybox_cubeMap = R_CreateCubeMap("***skybox***", size, size, TEX_NOCOMPRESS | TEX_MIPMAP | TEX_NOSCALE | TEX_ALPHA);
					if (!R_TextureReferenceIsValid(skybox_cubeMap)) {
						Q_free(data);
						Com_Printf("Couldn't load skybox \"%s\"\n", skyname);
						return false;
					}
				}

				renderer.TextureLoadCubemapFace(skybox_cubeMap, skydirection[i], data, size, size);
			}
			else {
				char id[16];

				snprintf(id, sizeof(id) - 1, "***skybox%d***", i);

				skyboxtextures[i] = R_LoadTexture(id, width, height, data, TEX_NOCOMPRESS | TEX_MIPMAP | TEX_NOSCALE, 4);
			}

			// we should free data from R_LoadImagePixels()
			Q_free(data);
		}
		else {
			int j;

			Com_Printf("Couldn't load skybox \"%s\"\n", skyname);
			if (R_UseCubeMapForSkyBox()) {
				R_DeleteCubeMap(&skybox_cubeMap);
			}
			else {
				for (j = 0; j < i; ++j) {
					R_DeleteTexture(&skyboxtextures[j]);
				}
			}

			return false;
		}
	}

	if (R_TextureReferenceIsValid(skybox_cubeMap)) {
		renderer.TextureMipmapGenerate(skybox_cubeMap);
	}

	return true;
}

void R_ClearSkyTextures(void)
{
	R_TextureReferenceInvalidate(solidskytexture);
	R_TextureReferenceInvalidate(alphaskytexture);
}

static void Skywind_Clear(void)
{
	if (!r_skyboxloaded)
		return;
	skywind_dist = 0.f;
	skywind_yaw = 45.f;
	skywind_pitch = 0.f;
	skywind_period = 30.f;
}

static void Skywind_Load_f(void)
{
	char relname[MAX_QPATH];
	char *buf;
	const char *data;

	if (!r_skyboxloaded)
	{
		Con_Printf ("No skybox loaded\n");
		return;
	}

	snprintf(relname, sizeof(relname), "gfx/env/%s" SKYWIND_CFG, active_skyname);
	buf = (char *)FS_LoadTempFile(relname, NULL);
	if (!buf)
	{
		Con_DPrintf ("Sky wind config not found '%s'.\n", relname);
		return;
	}

	data = COM_Parse(buf);
	if (!data)
	{
		return;
	}

	if (strcmp(com_token, "skywind") != 0)
	{
		Con_Printf("Skywind_Load_f: first token must be 'skywind'.\n");
		return;
	}

	Skywind_Clear();

	if ((data = COM_Parse(data)) != NULL)
	{
		skywind_dist = bound(-2.0f, atof(com_token), 2.0f);
	}

	if ((data = COM_Parse(data)) != NULL)
	{
		skywind_yaw = fmodf(atof(com_token), 360.0f);
	}

	if ((data = COM_Parse(data)) != NULL)
	{
		skywind_period = atof(com_token);
	}

	if ((data = COM_Parse(data)) != NULL)
	{
		skywind_pitch = fmodf(atof(com_token) + 90.0f, 180.0f) - 90.0f;
	}
}

static void Skywind_Save_f (void)
{
	char relname[MAX_QPATH];
	char path[MAX_OSPATH];
	FILE *f;

	if (!r_skyboxloaded)
	{
		Con_Printf("No skybox loaded\n");
		return;
	}

	snprintf(relname, sizeof(relname), "gfx/env/%s" SKYWIND_CFG, active_skyname);
	snprintf(path, sizeof(path), "%s/%s", com_gamedir, relname);
	f = fopen(path, "wt");
	if (!f)
	{
		Con_Printf("Couldn't write '%s'.\n", relname);
		return;
	}

	fprintf(f,
			"// distance yaw period pitch\n"
			"skywind %g %g %g %g\n",
			skywind_dist,
			skywind_yaw,
			skywind_period,
			skywind_pitch
	);

	fclose(f);

	Con_Printf("Wrote %s\n", relname);
}

static void Skywind_LookDir_f (void)
{
	if (cls.state != ca_active)
	{
		return;
	}

	if (!r_skyboxloaded)
	{
		Con_Printf("No skybox loaded\n");
		return;
	}

	// invert view direction so that clouds move towards the player, not away from them
	skywind_yaw = fmodf(cl.viewangles[YAW] + 180.0f, 360.0f);
	skywind_pitch = -cl.viewangles[PITCH];

	// first argument, if present, overrides the loop duration (default: 30 seconds)
	if (Cmd_Argc() >= 2)
	{
		skywind_period = atof(Cmd_Argv(1));
	}
	else if (!skywind_period)
	{
		skywind_period = 30.f;
	}

	// second argument, if present, overrides the amplitude of the movement (default: 1.0)
	if (Cmd_Argc() >= 3)
	{
		skywind_dist = bound(-2.0f, atof(Cmd_Argv(2)), 2.0f);
	}
	else if (!skywind_dist)
	{
		skywind_dist = 1.0f;
	}
}

static void Skywind_Rotate_f(void)
{
	if (cls.state != ca_active)
	{
		return;
	}

	if (!r_skyboxloaded)
	{
		Con_Printf("No skybox loaded\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf(
				"usage:\n"
				"   %s <yawdelta> [pitchdelta]\n",
				Cmd_Argv (0)
		);
		return;
	}

	skywind_yaw = fmodf(skywind_yaw + atof(Cmd_Argv(1)), 360.0f);
	if (Cmd_Argc() >= 3)
	{
		skywind_pitch = fmodf(skywind_pitch + atof(Cmd_Argv(2)) + 90.0f, 180.0f) - 90.0f;
	}
}

static void Skywind_f (void)
{
	if (cls.state != ca_active)
	{
		return;
	}

	if (!r_skyboxloaded)
	{
		Con_Printf("No skybox loaded\n");
		return;
	}

	if (Cmd_Argc() < 2)
	{
		Con_Printf (
				"usage:\n"
				"   %s [distance] [yaw] [period] [pitch]\n"
				"current values:\n"
				"   \"distance\" is \"%g\"\n"
				"   \"yaw\"      is \"%g\"\n"
				"   \"period\"   is \"%g\"\n"
				"   \"pitch\"    is \"%g\"\n",
				Cmd_Argv(0),
				skywind_dist,
				skywind_yaw,
				skywind_period,
				skywind_pitch
		);
		return;
	}

	skywind_dist = bound(-2.0f, atof(Cmd_Argv (1)), 2.0f);
	if (Cmd_Argc () >= 3)
	{
		skywind_yaw = fmodf(atof(Cmd_Argv(2)), 360.0f);
	}
	if (Cmd_Argc () >= 4)
	{
		skywind_period = atof(Cmd_Argv(3));
	}
	if (Cmd_Argc () >= 5)
	{
		skywind_pitch = fmodf(atof(Cmd_Argv(4)) + 90.0f, 180.0f) - 90.0f;
	}
}

qbool Skywind_Active(void)
{
	return r_skyboxloaded && skywind_dist > 0.0f;
}

qbool Skywind_GetDirectionAndPhase(float wind_dir[3], float *wind_phase)
{
	float yaw, pitch, sy, sp, cy, cp, dist, period;
	double phase;

	if (!Skywind_Active())
	{
		return false;
	}

	yaw = DEG2RAD(skywind_yaw);
	pitch = DEG2RAD(skywind_pitch);
	sy = sinf(yaw);
	sp = sinf(pitch);
	cy = cosf(yaw);
	cp = cosf(pitch);
	dist = bound(-2.f, skywind_dist, 2.f);
	period = skywind_period / r_skywind.value;
	phase = period ? cl.time * 0.5 / period : 0.5;

	phase -= floor(phase) + 0.5; // [-0.5, 0.5)
	wind_dir[0] = dist * cp * sy;
	wind_dir[1] = dist * sp;
	wind_dir[2] = -dist * cp * cy;
	*wind_phase = (float) phase;

	return true;
}

void R_SkyRegisterCvars(void)
{
	Cmd_AddCommand("loadsky", R_LoadSky_f);
	Cmd_AddCommand("skywind",Skywind_f);
	Cmd_AddCommand("skywind_save",Skywind_Save_f);
	Cmd_AddCommand("skywind_load",Skywind_Load_f);
	Cmd_AddCommand("skywind_lookdir",Skywind_LookDir_f);
	Cmd_AddCommand("skywind_rotate",Skywind_Rotate_f);
}