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

	$Id: gl_rmisc.c,v 1.27 2007-09-17 19:37:55 qqshka Exp $
*/
// gl_rmisc.c

#include "quakedef.h"
#include "vx_stuff.h"
#include "vx_tracker.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"
#ifndef  __APPLE__
#include "tr_types.h"
#endif
#include "gl_sky.h"
#include "r_texture.h"

static texture_ref GL_GenerateShellTexture(void)
{
	int x, y, d;
	byte data[32][32][4];

	for (y = 0;y < 32;y++)
	{
		for (x = 0;x < 32;x++)
		{
			d = (sin(x * M_PI / 8.0f) + cos(y * M_PI / 8.0f)) * 64 + 64;
			if (d < 0)
				d = 0;
			if (d > 255)
				d = 255;
			data[y][x][0] = data[y][x][1] = data[y][x][2] = d;
			data[y][x][3] = 255;
		}
	}

	return GL_LoadTexture("shelltexture", 32, 32, &data[0][0][0], TEX_MIPMAP, 4);
}

void R_InitOtherTextures(void)
{
	/*	static const int flags = TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN;

		underwatertexture = GL_LoadTextureImage ("textures/water_caustic", NULL, 0, 0,  flags );
		detailtexture = GL_LoadTextureImage("textures/detail", NULL, 256, 256, flags);
	*/
	unsigned char solidtexels[] = { 255, 255, 255, 255 };
	int flags = TEX_MIPMAP | TEX_ALPHA;

	underwatertexture = GL_LoadTextureImage("textures/water_caustic", NULL, 0, 0, flags | (gl_waterfog.value ? TEX_COMPLAIN : 0));
	detailtexture = GL_LoadTextureImage("textures/detail", NULL, 256, 256, flags | (gl_detail.value ? TEX_COMPLAIN : 0));

	shelltexture = GL_LoadTextureImage("textures/shellmap", NULL, 0, 0, flags | TEX_PREMUL_ALPHA | TEX_ZERO_ALPHA | (bound(0, gl_powerupshells.value, 1) ? TEX_COMPLAIN : 0));
	if (!GL_TextureReferenceIsValid(shelltexture)) {
		shelltexture = GL_GenerateShellTexture();
	}

	solidtexture = GL_LoadTexture("billboard:solid", 1, 1, solidtexels, TEX_ALPHA | TEX_NOSCALE, 4);
}

void R_InitTextures(void)
{
	int x, y, m;
	byte *dest;

	if (r_notexture_mip)
		return; // FIXME: may be do not Hunk_AllocName but made other stuff ???

	// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t *)Hunk_AllocName(sizeof(texture_t) + 16 * 16 + 8 * 8 + 4 * 4 + 2 * 2, "notexture");

	strlcpy(r_notexture_mip->name, "notexture", sizeof(r_notexture_mip->name));
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;

	for (m = 0; m < 4; m++) {
		dest = (byte *)r_notexture_mip + r_notexture_mip->offsets[m];
		for (y = 0; y < (16 >> m); y++) {
			for (x = 0; x < (16 >> m); x++) {
				if ((y < (8 >> m)) ^ (x < (8 >> m)))
					*dest++ = 0;
				else
					*dest++ = 0x0e;
			}
		}
	}
}

// Called after models have been downloaded but before they've been loaded into memory
void R_NewMapPreLoad(void)
{
	R_ClearSkyTextures();

	// Set this here so lightmaps & textures both use the same setting...
	lightmode = gl_lightmode.integer ? 2 : 0;
}

void R_NewMap(qbool vid_restart)
{
	int	i, waterline;

	extern int R_SetSky(char *skyname);
	extern void HUD_NewRadarMap(void); // hud_common.c

	R_SetSky (r_skyname.string);

	if (!vid_restart) {
		for (i = 0; i < 256; i++) {
			// normal light value
			d_lightstylevalue[i] = 264;
		}
    
		memset (&r_worldentity, 0, sizeof(r_worldentity));
		r_worldentity.model = cl.worldmodel;
    
		// clear out efrags in case the level hasn't been reloaded
		// FIXME: is this one short?
		for (i = 0; i < cl.worldmodel->numleafs; i++) {
			cl.worldmodel->leafs[i].efrags = NULL;
		}

		r_viewleaf = NULL;
		R_ClearParticles ();
	}
	else {
		Mod_ReloadModelsTextures(); // reload textures for brush models
#if defined(WITH_PNG)
		HUD_NewRadarMap();			// Need to reload the radar picture.
#endif
	}

	Mod_ReloadModels(vid_restart);

	if (cl.worldmodel) {
		GL_BuildLightmaps();
		if (GL_UseGLSL()) {
			GL_BuildCommonTextureArrays(vid_restart);
		}
		if (GL_BuffersSupported()) {
			GL_CreateModelVBOs(vid_restart);
		}
	}
	if (GL_UseGLSL()) {
		GLM_InitPrograms();
	}

	if (!vid_restart) {
		// identify sky texture
		for (i = 0; i < cl.worldmodel->numtextures; i++) {
			if (!cl.worldmodel->textures[i]) {
				continue;
			}
			for (waterline = 0; waterline < 2; waterline++) {
 	 			cl.worldmodel->textures[i]->texturechain[waterline] = NULL;
				cl.worldmodel->textures[i]->texturechain_tail[waterline] = &cl.worldmodel->textures[i]->texturechain[waterline];
			}
		}

		//VULT CORONAS
		InitCoronas();
		//VULT NAMES
		VX_TrackerClear();
		//VULT MOTION TRAILS
		CL_ClearBlurs();
	}
}

void R_TimeRefresh_f(void)
{
	int i;
	float start, stop, time;

	if (cls.state != ca_active)
		return;

	if (!Rulesets_AllowTimerefresh()) {
		Com_Printf("Timerefresh is disabled during match\n");
		return;
	}

#ifndef __APPLE__
	if (glConfig.hardwareType != GLHW_INTEL) {
		// Causes the console to flicker on Intel cards.
		glDrawBuffer(GL_FRONT);
	}
#endif
	
	glFinish ();

	start = Sys_DoubleTime();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_SetupFrame();
		R_RenderView();
	}

	glFinish ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);

#ifndef __APPLE__
	if (glConfig.hardwareType != GLHW_INTEL) {
		glDrawBuffer(GL_BACK);
	}
#endif

	GL_EndRendering ();
}

void D_FlushCaches (void) {}

