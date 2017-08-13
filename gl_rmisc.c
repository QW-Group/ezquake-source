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

static unsigned int model_vao = 0;

void GL_BuildCommonTextureArrays(void);

void R_InitOtherTextures (void) {
/*	static const int flags = TEX_MIPMAP | TEX_ALPHA | TEX_COMPLAIN;

	underwatertexture = GL_LoadTextureImage ("textures/water_caustic", NULL, 0, 0,  flags );	
	detailtexture = GL_LoadTextureImage("textures/detail", NULL, 256, 256, flags);	
*/
	int flags = TEX_MIPMAP | TEX_ALPHA;

	underwatertexture = GL_LoadTextureImage ("textures/water_caustic", NULL, 0, 0,  flags | (gl_waterfog.value ? TEX_COMPLAIN : 0));	
	detailtexture = GL_LoadTextureImage ("textures/detail", NULL, 256, 256, flags | (gl_detail.value ? TEX_COMPLAIN : 0));

	shelltexture = GL_LoadTextureImage ("textures/shellmap", NULL, 0, 0,  flags | (bound(0, gl_powerupshells.value, 1) ? TEX_COMPLAIN : 0));
}

void R_InitTextures (void) {
	int x,y, m;
	byte *dest;

	if (r_notexture_mip)
		return; // FIXME: may be do not Hunk_AllocName but made other stuff ???

	// create a simple checkerboard texture for the default
	r_notexture_mip = (texture_t *) Hunk_AllocName (sizeof(texture_t) + 16 * 16 + 8 * 8+4 * 4 + 2 * 2, "notexture");
	
	strlcpy(r_notexture_mip->name, "notexture", sizeof (r_notexture_mip->name));
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

//Translates a skin texture by the per-player color lookup
void R_TranslatePlayerSkin (int playernum) {
	byte translate[256], *inrow, *original;
	char s[512];
	int	top, bottom, i, j, scaled_width, scaled_height, inwidth, inheight, tinwidth, tinheight, glinternalfmt, glinternalfmt_alpha;
	unsigned translate32[256], *out, frac, fracstep;

	unsigned pixels[512 * 256];

	player_info_t *player;
	extern byte player_8bit_texels[256 * 256];
	extern cvar_t gl_scaleModelTextures;
	extern int gl_max_size_default;

	if(gl_gammacorrection.integer)
	{
		glinternalfmt = GL_SRGB8;
		glinternalfmt_alpha = GL_SRGB8_ALPHA8;
	}
	else
	{
		glinternalfmt = GL_RGB;
		glinternalfmt_alpha = GL_RGBA;
	}

	player = &cl.players[playernum];
	if (!player->name[0])
		return;

	strlcpy(s, Skin_FindName(player), sizeof(s));
	COM_StripExtension(s, s, sizeof(s));

	if (player->skin && strcasecmp(s, player->skin->name))
		player->skin = NULL;

	if (player->_topcolor == player->topcolor && player->_bottomcolor == player->bottomcolor && player->skin)
		return;

	GL_DisableMultitexture();

	player->_topcolor = player->topcolor;
	player->_bottomcolor = player->bottomcolor;

	if (!player->skin)
		Skin_Find(player);

	playerfbtextures[playernum] = 0; // no full bright texture by default

	if (player->skin->texnum && player->skin->bpp == 4) { // do not even bother call Skin_Cache(), we have texture num alredy
//		Com_Printf("    ###SHORT loaded skin %s %d\n", player->skin->name, player->skin->texnum);

		playernmtextures[playernum] = player->skin->texnum;
		return;
	}

	if ((original = Skin_Cache(player->skin, false)) != NULL) {
		switch (player->skin->bpp) {
		case 4: // 32 bit skin
//			Com_Printf("    ###FULL loaded skin %s %d\n", player->skin->name, player->skin->texnum);

			// FIXME: in ideal, GL_LoadTexture() must be issued in Skin_Cache(), but I fail with that, so move it here
			playernmtextures[playernum] = player->skin->texnum =
				GL_LoadTexture (player->skin->name, player->skin->width, player->skin->height, original, (gl_playermip.integer ? TEX_MIPMAP : 0) | TEX_NOSCALE, 4);

			return; // we done all we want

		case 1: // 8 bit skin
			break;

		default:
			Sys_Error("R_TranslatePlayerSkin: wrong bpp %d", player->skin->bpp);
		}

		//skin data width
		inwidth = 320;
		inheight = 200;
	} else {
		original = player_8bit_texels;
		inwidth = 296;
		inheight = 194;
	}

//	Com_Printf("    ###8 bit loaded skin %s %d\n", player->skin->name, player->skin->texnum);

	// locate the original skin pixels
	// real model width
	tinwidth = 296;
	tinheight = 194;

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

	GL_Bind(playernmtextures[playernum]);

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

	glTexImage2D (GL_TEXTURE_2D, 0, glinternalfmt, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
	GL_TextureEnvMode(GL_MODULATE);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	if (Img_HasFullbrights ((byte *) original, inwidth * inheight)) {
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
					out[j] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF); // transparent
				else
					out[j] = translate32[inrow[frac >> 16]]; // fullbright
				frac += fracstep;
				if (inrow[frac >> 16] < 224)
					out[j + 1] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF);
				else
					out[j + 1] = translate32[inrow[frac >> 16]];
				frac += fracstep;
				if (inrow[frac >> 16] < 224)
					out[j + 2] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF);
				else
					out[j + 2] = translate32[inrow[frac >> 16]];
				frac += fracstep;
				if (inrow[frac >> 16] < 224)
					out[j + 3] = translate32[inrow[frac >> 16]] & LittleLong(0x00FFFFFF);
				else
					out[j + 3] = translate32[inrow[frac >> 16]];
				frac += fracstep;
			}
		}

		glTexImage2D (GL_TEXTURE_2D, 0, glinternalfmt_alpha, scaled_width, scaled_height, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

		GL_TextureEnvMode(GL_MODULATE);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
}


void R_NewMap (qbool vid_restart) {
	int	i, waterline;

	extern int R_SetSky(char *skyname);
	extern void HUD_NewRadarMap(void); // hud_common.c

	R_SetSky (r_skyname.string);

	lightmode = gl_lightmode.value ? 2 : 0;

	if (!vid_restart) {
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
	}
	else {
		Mod_ReloadModelsTextures(); // reload textures for brush models
#if defined(WITH_PNG)
		HUD_NewRadarMap();			// Need to reload the radar picture.
#endif
	}

	GL_BuildLightmaps();
	GL_BuildCommonTextureArrays();

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

void R_TimeRefresh_f (void) {
	int i;
	float start, stop, time;

	if (cls.state != ca_active)
		return;

	if (!Rulesets_AllowTimerefresh()) {
		Com_Printf("Timerefresh's disabled during match\n");
		return;
	}

#ifndef __APPLE__
	if (glConfig.hardwareType != GLHW_INTEL)
	{
		// Causes the console to flicker on Intel cards.
		glDrawBuffer  (GL_FRONT);
	}
#endif
	
	glFinish ();

	start = Sys_DoubleTime ();
	for (i = 0; i < 128; i++) 
	{
		r_refdef.viewangles[1] = i * (360.0 / 128.0);
		R_RenderView ();
	}

	glFinish ();
	stop = Sys_DoubleTime ();
	time = stop-start;
	Com_Printf ("%f seconds (%f fps)\n", time, 128/time);

#ifndef __APPLE__
	if (glConfig.hardwareType != GLHW_INTEL)
	{
		glDrawBuffer  (GL_BACK);
	}
#endif

	GL_EndRendering ();
}

void D_FlushCaches (void) {}

typedef struct common_texture_s {
	int width;
	int height;
	int count;
	int any_size_count;
	GLuint gl_texturenum;
	int gl_width;
	int gl_height;

	int allocated;

	struct common_texture_s* next;
} common_texture_t;

static int count = 0;
void GL_RegisterCommonTextureSize(common_texture_t* list, GLint texture, qbool any_size)
{
	GLint width, height;

	if (!texture) {
		return;
	}

	glBindTexture(GL_TEXTURE_2D, texture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	while (list) {
		if (list->width == width && list->height == height) {
			list->count++;
			if (any_size) {
				list->any_size_count++;
			}
			break;
		}
		else if (!list->next) {
			list->next = Q_malloc(sizeof(common_texture_t));
			list->next->width = width;
			list->next->height = height;
		}

		list = list->next;
	}

	++count;
}

GLuint GL_CreateTextureArray(int width, int height, int depth)
{
	GLuint gl_texturenum;
	int max_miplevels = 0;
	int min_dimension = min(width, height);

	// 
	while (min_dimension > 0) {
		max_miplevels++;
		min_dimension /= 2;
	}

	glGenTextures(1, &gl_texturenum);
	glBindTexture(GL_TEXTURE_2D_ARRAY, gl_texturenum);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_NEAREST);
	glTexParameterf(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexStorage3D(GL_TEXTURE_2D_ARRAY, max_miplevels, GL_RGBA8, width, height, depth);

	Con_Printf("Creating texture array %u, %d x %d x %d\n", gl_texturenum, width, height, depth);

	return gl_texturenum;
}

void GL_AddTextureToArray(GLuint arrayTexture, int width, int height, int index)
{
	int level = 0;
	GLubyte* buffer;
	
	buffer = Q_malloc(width * height * 4 * sizeof(GLubyte));

	Con_Printf("Adding texture to array %u[%d]\n", arrayTexture, index);
	glBindTexture(GL_TEXTURE_2D_ARRAY, arrayTexture);
	for (level = 0; width && height; ++level, width /= 2, height /= 2) {
		glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
		glTexSubImage3D(GL_TEXTURE_2D_ARRAY, level, 0, 0, index, width, height, 1, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
	}

	Q_free(buffer);
}

common_texture_t* GL_FindTextureBySize(common_texture_t* list, int width, int height)
{
	while (list) {
		if (list->width == width && list->height == height) {
			return list;
		}

		list = list->next;
	}

	return NULL;
}

int GL_CopyToTextureArraySize(common_texture_t* list, GLuint stdTexture, qbool anySize, float* scaleS, float* scaleT)
{
	GLint width, height;
	common_texture_t* tex;

	if (!stdTexture) {
		*scaleS = *scaleT = 0;
		return -1;
	}

	glBindTexture(GL_TEXTURE_2D, stdTexture);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);

	if (anySize) {
		tex = GL_FindTextureBySize(list, 0, 0);
	}
	else {
		tex = GL_FindTextureBySize(list, width, height);
		if (!tex->gl_texturenum) {
			GL_CreateTextureArray(width, height, tex->count - tex->any_size_count);
			tex->gl_width = width;
			tex->gl_height = height;
		}
	}
	*scaleS = width * 1.0f / tex->gl_width;
	*scaleT = height * 1.0f / tex->gl_height;

	GL_AddTextureToArray(tex->gl_texturenum, width, height, tex->allocated);
	return tex->allocated++;
}

void GL_FreeTextureSizeList(common_texture_t* tex)
{
	common_texture_t* next = tex->next;
	while (next) {
		Q_free(tex);
		tex = next;
		next = tex->next;
	}
	Q_free(tex);
}

void GL_PrintTextureSizes(common_texture_t* list)
{
	common_texture_t* tex;

	for (tex = list; tex; tex = tex->next) {
		if (tex->count) {
			Con_Printf("%dx%d = %d (%d anysize)\n", tex->width, tex->height, tex->count, tex->any_size_count);
		}
	}
}

void GL_SortTextureSizes(common_texture_t** first)
{
	qbool changed = true;

	while (changed) {
		common_texture_t** link = first;

		changed = false;
		while (*link && (*link)->next) {
			common_texture_t* current = *link;
			common_texture_t* next = current->next;

			if (next && current->any_size_count < next->any_size_count) {
				Con_Printf("Swapping %dx%d with %dx%d\n", current->width, current->height, next->width, next->height);
				*link = next;
				current->next = next->next;
				next->next = current;
				changed = true;
				link = &next->next;
			}
			else {
				link = &current->next;
			}
		}
	}
}

static void GL_SetModelTextureArray(model_t* mod, GLuint array_num, float widthRatio, float heightRatio)
{
	mod->texture_arrays = Q_malloc(sizeof(GLuint));
	mod->texture_array_count = 1;
	mod->texture_arrays[0] = array_num;
	mod->texture_arrays_scale_s = Q_malloc(sizeof(GLuint));
	mod->texture_arrays_scale_t = Q_malloc(sizeof(GLuint));
	mod->texture_arrays_scale_s[0] = widthRatio;
	mod->texture_arrays_scale_t[0] = heightRatio;
}


void GL_BuildCommonTextureArrays(void)
{
	common_texture_t* common = Q_malloc(sizeof(common_texture_t));
	int required_vbo_length = 4;
	unsigned int model_vbo = 0;
	int i;

	glGenBuffers(1, &model_vbo);
	glGenVertexArrays(1, &model_vao);

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (!mod) {
			continue;
		}

		if (!mod->isworldmodel) {
			int j;

			if (mod->type == mod_alias) {
				aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);
				qbool any_size = mod->max_tex[0] <= 1.0 && mod->max_tex[1] <= 1.0 && mod->min_tex[0] >= 0 && mod->min_tex[1] >= 0;

				for (j = 0; j < paliashdr->numskins; ++j) {
					int anim;
					for (anim = 0; anim < 4; ++anim) {
						if (anim == 0 || paliashdr->gl_texturenum[j][anim] != paliashdr->gl_texturenum[j][anim - 1]) {
							GL_RegisterCommonTextureSize(common, paliashdr->gl_texturenum[j][anim], any_size);
						}
						if (anim == 0 || paliashdr->fb_texturenum[j][anim] != paliashdr->fb_texturenum[j][anim - 1]) {
							GL_RegisterCommonTextureSize(common, paliashdr->fb_texturenum[j][anim], any_size);
						}
					}
				}

				for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
					if (mod->simpletexture[j]) {
						GL_RegisterCommonTextureSize(common, mod->simpletexture[j], true);
					}
				}

				Con_Printf("Registered %s: %d\n", mod->name, count);
				required_vbo_length += mod->vertsInVBO;
			}
			else if (mod->type == mod_sprite) {
				msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);
				int count = 0;

				for (j = 0; j < psprite->numframes; ++j) {
					int offset    = psprite->frames[j].offset;
					int numframes = psprite->frames[j].numframes;

					if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
						continue;
					}

					GL_RegisterCommonTextureSize(common, ((mspriteframe_t* )((byte*)psprite + offset))->gl_texturenum, true);
					++count;
				}
				Con_Printf("Registered %s: %d\n", mod->name, count);
			}
			else if (mod->type == mod_brush && !mod->isworldmodel) {
				for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
					if (mod->simpletexture[j]) {
						GL_RegisterCommonTextureSize(common, mod->simpletexture[j], true);
						++count;
					}
				}
				Con_Printf("Registered %s: %d\n", mod->name, count);
			}
			else {
				//Con_Printf("***: type %d (%s)\n", mod->type, mod->name);
			}
		}
		else {
			//Con_Printf("%d: WorldModel\n", i);
		}
	}

	{
		// Find highest dimensions, stick everything in there for the moment unless texture is tiling
		// FIXME: this is a memory vs texture-switch tradeoff
		int maxWidth = 0;
		int maxHeight = 0;
		common_texture_t* tex;
		common_texture_t* commonTex = NULL;
		int anySizeCount = 0;
		float* new_vbo_buffer = Q_malloc(required_vbo_length * MODELVERTEXSIZE * sizeof(float));
		int new_vbo_position = 0;
		
		for (tex = common; tex; tex = tex->next) {
			if (tex->width == 0 && tex->height == 0) {
				commonTex = tex;
			}
			else {
				maxWidth = max(maxWidth, tex->width);
				maxHeight = max(maxHeight, tex->height);
			}
			anySizeCount += tex->any_size_count;
		}

		// Create non-specific array to fit everything that doesn't require tiling
		commonTex->gl_texturenum = GL_CreateTextureArray(maxWidth, maxHeight, anySizeCount);
		commonTex->gl_width = maxWidth;
		commonTex->gl_height = maxHeight;

		// VBO starts with simple-model/sprite vertices
		{
			float* vert;
			
			vert = new_vbo_buffer;
			VectorSet(vert, 0, -1, -1);
			vert[3] = 1;
			vert[4] = 1;

			vert = new_vbo_buffer + MODELVERTEXSIZE;
			VectorSet(vert, 0, -1, 1);
			vert[3] = 1;
			vert[4] = 0;

			vert = new_vbo_buffer + MODELVERTEXSIZE * 2;
			VectorSet(vert, 0, 1, 1);
			vert[3] = 0;
			vert[4] = 0;

			vert = new_vbo_buffer + MODELVERTEXSIZE * 3;
			VectorSet(vert, 0, 1, -1);
			vert[3] = 0;
			vert[4] = 1;

			new_vbo_position = 4;
		}

		// Go back through all models, importing textures into arrays and creating new VBO
		for (i = 1; i < MAX_MODELS; ++i) {
			model_t* mod = cl.model_precache[i];
			int count = 0;

			if (!mod) {
				continue;
			}

			if (!mod->isworldmodel) {
				int j;

				if (mod->type == mod_alias) {
					aliashdr_t* paliashdr = (aliashdr_t *)Mod_Extradata(mod);
					qbool any_size = mod->max_tex[0] <= 1.0 && mod->max_tex[1] <= 1.0 && mod->min_tex[0] >= 0 && mod->min_tex[1] >= 0;

					for (j = 0; j < paliashdr->numskins; ++j) {
						int anim;
						for (anim = 0; anim < 4; ++anim) {
							if (anim == 0 || paliashdr->gl_texturenum[j][anim] != paliashdr->gl_texturenum[j][anim - 1]) {
								paliashdr->gl_arrayindex[j][anim] = GL_CopyToTextureArraySize(common, paliashdr->gl_texturenum[j][anim], any_size, &paliashdr->gl_scalingS[j][anim], &paliashdr->gl_scalingT[j][anim]);
							}
							else {
								paliashdr->gl_arrayindex[j][anim] = paliashdr->gl_arrayindex[j][anim - 1];
								
							}

							if (anim == 0 || paliashdr->fb_texturenum[j][anim] != paliashdr->fb_texturenum[j][anim - 1]) {
								paliashdr->gl_fb_arrayindex[j][anim] = GL_CopyToTextureArraySize(common, paliashdr->fb_texturenum[j][anim], any_size, &paliashdr->gl_scalingS[j][anim], &paliashdr->gl_scalingT[j][anim]);
							}
							else {
								paliashdr->gl_fb_arrayindex[j][anim] = paliashdr->gl_fb_arrayindex[j][anim - 1];
							}
						}
					}

					for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
						if (mod->simpletexture[j]) {
							mod->simpletexture_indexes[j] = GL_CopyToTextureArraySize(common, mod->simpletexture[j], true, &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j]);
						}
					}

					Con_Printf("Added %s: %d\n", mod->name, commonTex->allocated);
					GL_SetModelTextureArray(mod, commonTex->gl_texturenum, commonTex->width * 1.0f / maxWidth, commonTex->height * 1.0f / maxHeight);

					// Copy VBO info to buffer (FIXME: Free the memory?  but is cached.  But CacheAlloc() fails... argh)
					memcpy(&new_vbo_buffer[new_vbo_position * MODELVERTEXSIZE], mod->temp_vbo_buffer, mod->vertsInVBO * MODELVERTEXSIZE * sizeof(float));
					//Q_free(mod->temp_vbo_buffer);

					mod->vao_simple = mod->vao = model_vao;
					mod->vbo = model_vbo;
					mod->vbo_start = new_vbo_position;

					paliashdr->vbo = model_vbo;
					paliashdr->vao = model_vao;
					paliashdr->vertsOffset = new_vbo_position;

					new_vbo_position += mod->vertsInVBO;
				}
				else if (mod->type == mod_sprite) {
					msprite2_t* psprite = (msprite2_t*)Mod_Extradata(mod);

					for (j = 0; j < psprite->numframes; ++j) {
						int offset    = psprite->frames[j].offset;
						int numframes = psprite->frames[j].numframes;
						mspriteframe_t* frame;

						if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
							continue;
						}

						frame = ((mspriteframe_t*)((byte*)psprite + offset));
						GL_CopyToTextureArraySize(common, frame->gl_texturenum, true, &frame->gl_scalingS, &frame->gl_scalingT);
					}

					Con_Printf("Added %s: %d\n", mod->name, commonTex->allocated);
					mod->vao_simple = model_vao;
					// FIXME
					GL_SetModelTextureArray(mod, commonTex->gl_texturenum, 0.5f, 0.5f);
					mod->vbo = model_vbo;
					mod->vbo_start = 0;
				}
				else if (mod->type == mod_brush) {
					for (j = 0; j < MAX_SIMPLE_TEXTURES; ++j) {
						if (mod->simpletexture[j]) {
							mod->simpletexture_indexes[j] = GL_CopyToTextureArraySize(common, mod->simpletexture[j], true, &mod->simpletexture_scalingS[j], &mod->simpletexture_scalingT[j]);
						}
					}

					Con_Printf("Added %s: %d\n", mod->name, commonTex->allocated);
					mod->vao_simple = model_vao;
					mod->vbo_start = 0;

					// FIXME
					GL_SetModelTextureArray(mod, commonTex->gl_texturenum, 0.25f, 0.25f);
				}
				else {
					//Con_Printf("***: type %d (%s)\n", mod->type, mod->name);
				}
			}
			else {
				//Con_Printf("%d: WorldModel\n", i);
			}
		}

		glBindBufferExt(GL_ARRAY_BUFFER, model_vbo);
		glBufferDataExt(GL_ARRAY_BUFFER, required_vbo_length * MODELVERTEXSIZE * sizeof(float), new_vbo_buffer, GL_STATIC_DRAW);

		glBindVertexArray(model_vao);
		glEnableVertexAttribArray(0);
		glEnableVertexAttribArray(1);
		//glEnableVertexAttribArray(2);
		//glEnableVertexAttribArray(3);
		glBindBufferExt(GL_ARRAY_BUFFER, model_vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)(sizeof(float) * 3));
		//glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)(sizeof(float) * 5));
		//glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(float) * MODELVERTEXSIZE, (void*)(sizeof(float) * 8));

		Q_free(new_vbo_buffer);
	}

	GL_FreeTextureSizeList(common);
}
