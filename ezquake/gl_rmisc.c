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
// gl_rmisc.c

#include "quakedef.h"
#include "gl_local.h"

#include "rulesets.h"


void R_InitOtherTextures (void) {
	static const int flags = TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN;

	underwatertexture = GL_LoadTextureImage ("textures/water_caustic", NULL, 0, 0,  flags );	
	detailtexture = GL_LoadTextureImage("textures/detail", NULL, 256, 256, flags);	
}

void R_InitTextures (void) {
	int x,y, m;
	byte *dest;

	// create a simple checkerboard texture for the default
	r_notexture_mip = Hunk_AllocName (sizeof(texture_t) + 16 * 16 + 8 * 8+4 * 4 + 2 * 2, "notexture");
	
	strcpy(r_notexture_mip->name, "notexture");
	r_notexture_mip->width = r_notexture_mip->height = 16;
	r_notexture_mip->offsets[0] = sizeof(texture_t);
	r_notexture_mip->offsets[1] = r_notexture_mip->offsets[0] + 16 * 16;
	r_notexture_mip->offsets[2] = r_notexture_mip->offsets[1] + 8 * 8;
	r_notexture_mip->offsets[3] = r_notexture_mip->offsets[2] + 4 * 4;
	
	for (m = 0; m < 4; m++) {
		dest = (byte *) r_notexture_mip + r_notexture_mip->offsets[m];
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

char *Skin_FindName (player_info_t *sc) ;
//Translates a skin texture by the per-player color lookup
void R_TranslatePlayerSkin (int playernum) {
	byte translate[256], *inrow, *original;
	char s[512];
	int	top, bottom, i, j, scaled_width, scaled_height, inwidth, inheight, tinwidth, tinheight;
	unsigned translate32[256], pixels[512 * 256], *out, frac, fracstep;
	player_info_t *player;
	extern byte player_8bit_texels[320 * 200];
	extern cvar_t gl_scaleModelTextures;
	extern int gl_max_size_default;

	player = &cl.players[playernum];
	if (!player->name[0])
		return;

	Q_strncpyz(s, Skin_FindName(player), sizeof(s));
	COM_StripExtension(s, s);

	if (player->skin && Q_strcasecmp(s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor == player->topcolor && player->_bottomcolor == player->bottomcolor && player->skin)
		return;

	GL_DisableMultitexture();

	player->_topcolor = player->topcolor;
	player->_bottomcolor = player->bottomcolor;

	top = bound(0, player->topcolor, 13) * 16;
	bottom = bound(0, player->bottomcolor, 13) * 16;

	for (i = 0; i < 256; i++)
		translate[i] = i;

	for (i = 0; i < 16; i++) {
		if (top < 128)	// the artists made some backwards ranges.  sigh.
			translate[TOP_RANGE + i] = top + i;
		else
			translate[TOP_RANGE + i] = top + 15 - i;

		if (bottom < 128)
			translate[BOTTOM_RANGE + i] = bottom + i;
		else
			translate[BOTTOM_RANGE + i] = bottom + 15 - i;
	}

	// locate the original skin pixels
	// real model width
	tinwidth = 296;
	tinheight = 194;

	if (!player->skin)
		Skin_Find(player);
	if ((original = Skin_Cache(player->skin)) != NULL) {
		//skin data width
		inwidth = 320;
		inheight = 200;
	} else {
		original = player_8bit_texels;
		inwidth = 296;
		inheight = 194;
	}

	GL_Bind(playertextures + playernum);

	scaled_width = gl_scaleModelTextures.value ? min(gl_max_size.value, 512) : min(gl_max_size_default, 512);
	scaled_height = gl_scaleModelTextures.value ? min(gl_max_size.value, 256) : min(gl_max_size_default, 256);

	// allow users to crunch sizes down even more if they want
	scaled_width >>= (int) gl_playermip.value;
	scaled_height >>= (int) gl_playermip.value;
	if (scaled_width < 1)
		scaled_width = 1;
	if (scaled_height < 1)
		scaled_height = 1;

	for	(i = 0; i < 256; i++)
		translate32[i] = d_8to24table[translate[i]];

	out	= pixels;
	memset(pixels, 0, sizeof(pixels));
	fracstep = tinwidth * 0x10000 / scaled_width;
	for	(i = 0; i < scaled_height; i++, out += scaled_width) {
		inrow =	original + inwidth*(i * tinheight / scaled_height);
		frac = fracstep	>> 1;
		for	(j = 0; j < scaled_width; j += 4) {
			out[j] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 1] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 2] = translate32[inrow[frac >> 16]];
			frac += fracstep;
			out[j + 3] = translate32[inrow[frac >> 16]];
			frac += fracstep;
		}
	}

	glTexImage2D (GL_TEXTURE_2D, 0,	3, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	playerfbtextures[playernum] = 0;
	if (Img_HasFullbrights ((byte *) original, inwidth * inheight)) {
		playerfbtextures[playernum] = playertextures + playernum + MAX_CLIENTS;

		GL_Bind(playerfbtextures[playernum]);

		out = pixels;
		memset(pixels, 0, sizeof(pixels));
		fracstep = tinwidth * 0x10000 / scaled_width;

		// make all non-fullbright colors transparent
		for (i = 0; i < scaled_height; i++, out += scaled_width) {
			inrow = original + inwidth * (i * tinheight / scaled_height);
			frac = fracstep >> 1;
			for (j = 0; j < scaled_width; j += 4) {
				if (inrow[frac >> 16] < 224)
					out[j] = translate32[inrow[frac >> 16]] & 0x00FFFFFF; // transparent
				else
					out[j] = translate32[inrow[frac >> 16]]; // fullbright
				frac += fracstep;
				if (inrow[frac >> 16] < 224)
					out[j + 1] = translate32[inrow[frac >> 16]] & 0x00FFFFFF;
				else
					out[j + 1] = translate32[inrow[frac >> 16]];
				frac += fracstep;
				if (inrow[frac >> 16] < 224)
					out[j + 2] = translate32[inrow[frac >> 16]] & 0x00FFFFFF;
				else
					out[j + 2] = translate32[inrow[frac >> 16]];
				frac += fracstep;
				if (inrow[frac >> 16] < 224)
					out[j + 3] = translate32[inrow[frac >> 16]] & 0x00FFFFFF;
				else
					out[j + 3] = translate32[inrow[frac >> 16]];
				frac += fracstep;
			}
		}

		glTexImage2D (GL_TEXTURE_2D, 0, 4, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}


void R_PreMapLoad(char *name) {

	Cvar_ForceSet (&mapname, name);
	if (!dedicated)
		lightmode = gl_lightmode.value == 0 ? 0 : 2;
}

void R_NewMap (void) {
	int	i, waterline;

	for (i = 0; i < 256; i++)
		d_lightstylevalue[i] = 264;		// normal light value

	memset (&r_worldentity, 0, sizeof(r_worldentity));
	r_worldentity.model = cl.worldmodel;

	// clear out efrags in case the level hasn't been reloaded
	// FIXME: is this one short?
	for (i = 0; i < cl.worldmodel->numleafs; i++)
		cl.worldmodel->leafs[i].efrags = NULL;
		 	
	r_viewleaf = NULL;
	R_ClearParticles ();

	GL_BuildLightmaps ();

	// identify sky texture
	for (i = 0; i < cl.worldmodel->numtextures; i++) {
		if (!cl.worldmodel->textures[i])
			continue;
		for (waterline = 0; waterline < 2; waterline++) {
 			cl.worldmodel->textures[i]->texturechain[waterline] = NULL;
			cl.worldmodel->textures[i]->texturechain_tail[waterline] = &cl.worldmodel->textures[i]->texturechain[waterline];
		}
	}
}

void R_TimeRefresh_f (void) {
	int i;
	float start, stop, time;

	if (cls.state != ca_active)
		return;

	if (!Rulesets_AllowTimerefresh()) {
		Com_Printf("Timerefresh's disabled during match\n");
		return;
	}

	glDrawBuffer  (GL_FRONT);
	glFinish ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) {
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);

	glDrawBuffer  (GL_BACK);
	GL_EndRendering ();
}

void D_FlushCaches (void) {}
