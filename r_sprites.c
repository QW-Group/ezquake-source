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
// gl_sprites.c 
// taken from gl_model.c -- model loading and caching

// models are the only shared resource between a client and server running on the same machine.

#include "quakedef.h"
#include "gl_model.h"
#include "r_texture.h"

extern model_t* loadmodel;
extern char     loadname[32];   // for hunk tags

//=============================================================================
static texture_ref Mod_LoadExternalSpriteSkin(char *identifier, int framenum)
{
	char loadpath[64];
	int texmode;
	texture_ref texnum;

	texmode = TEX_MIPMAP | TEX_ALPHA | TEX_PREMUL_ALPHA;
	if (!gl_scaleModelTextures.value && !loadmodel->isworldmodel) {
		texmode |= TEX_NOSCALE;
	}

	snprintf (loadpath, sizeof(loadpath), "textures/sprites/%s", identifier);
	texnum = R_LoadTextureImage (loadpath, identifier, 0, 0, texmode);

	if (!R_TextureReferenceIsValid(texnum)) {
		snprintf (loadpath, sizeof(loadpath), "textures/%s", identifier);
		texnum = R_LoadTextureImage (loadpath, identifier, 0, 0, texmode);
	}

	return texnum;
}

void *Mod_LoadSpriteFrame (void * pin, mspriteframe_t **ppframe, int framenum)
{
	dspriteframe_t *pinframe;
	mspriteframe_t *pspriteframe;
	char basename[64], identifier[64];
	int width, height, size, origin[2], texmode;
	texture_ref texnum;

	texmode = TEX_MIPMAP | TEX_ALPHA;
	if (!gl_scaleModelTextures.value && !loadmodel->isworldmodel) {
		texmode |= TEX_NOSCALE;
	}

	COM_StripExtension(COM_SkipPath(loadmodel->name), basename, sizeof(basename));

	pinframe = (dspriteframe_t *) pin;

	width = LittleLong (pinframe->width);
	height = LittleLong (pinframe->height);
	size = width * height;

	pspriteframe = (mspriteframe_t *) Hunk_AllocName (sizeof (mspriteframe_t),loadname);

	memset (pspriteframe, 0, sizeof (mspriteframe_t));

	*ppframe = pspriteframe;

	pspriteframe->width = width;
	pspriteframe->height = height;
	origin[0] = LittleLong (pinframe->origin[0]);
	origin[1] = LittleLong (pinframe->origin[1]);

	pspriteframe->up = origin[1];
	pspriteframe->down = origin[1] - height;
	pspriteframe->left = origin[0];
	pspriteframe->right = width + origin[0];

	snprintf (identifier, sizeof(identifier), "sprites/%s_%i", basename, framenum);
	texnum = Mod_LoadExternalSpriteSkin(identifier, framenum);
	if (!R_TextureReferenceIsValid(texnum) && width > 0 && height > 0) {
		texnum = R_LoadTexture(identifier, width, height, (byte*)(pinframe + 1), texmode, 1);
	}

	pspriteframe->gl_texturenum = texnum;
	pspriteframe->gl_scalingS = 1.0f;
	pspriteframe->gl_scalingT = 1.0f;

	return (void *) ((byte *) pinframe + sizeof (dspriteframe_t) + size);
}

void *Mod_LoadSpriteGroup (void * pin, mspriteframe_t **ppframe, int framenum) {
	dspritegroup_t *pingroup;
	mspritegroup_t *pspritegroup;
	int i, numframes;
	dspriteinterval_t *pin_intervals;
	float *poutintervals;
	void *ptemp;

	pingroup = (dspritegroup_t *) pin;

	numframes = LittleLong (pingroup->numframes);

	if (numframes < 1) {
		Host_Error("Mod_LoadSpriteGroup: numframes < 1");
	}

	pspritegroup = (mspritegroup_t *) Hunk_AllocName (sizeof (mspritegroup_t) +
		(numframes - 1) * sizeof (pspritegroup->frames[0]), loadname);

	pspritegroup->numframes = numframes;

	*ppframe = (mspriteframe_t *) pspritegroup;

	pin_intervals = (dspriteinterval_t *) (pingroup + 1);

	poutintervals = (float *) Hunk_AllocName (numframes * sizeof (float), loadname);

	pspritegroup->intervals = poutintervals;

	for (i = 0; i < numframes; i++) {
		*poutintervals = LittleFloat (pin_intervals->interval);
		if (*poutintervals <= 0.0) {
			Host_Error("Mod_LoadSpriteGroup: interval <= 0");
		}

		poutintervals++;
		pin_intervals++;
	}

	ptemp = (void *) pin_intervals;
	for (i = 0; i < numframes; i++) {
		ptemp = Mod_LoadSpriteFrame(ptemp, &pspritegroup->frames[i], framenum * 100 + i);
	}

	return ptemp;
}

void Mod_LoadSpriteModel (model_t *mod, void *buffer)
{
	int i, j, version, numframes, size, start, offset, sztmp;
	dsprite_t *pin;
	msprite_t *psprite;
	dspriteframetype_t *pframetype;

	msprite2_t *psprite2;


	// remember point
	start = Hunk_LowMark ();


	pin = (dsprite_t *)buffer;

	version = LittleLong (pin->version);

	if (version != SPRITE_VERSION) {
		Host_Error ("Mod_LoadSpriteModel: %s has wrong version number (%i should be %i)", mod->name, version, SPRITE_VERSION);
		return;
	}

	numframes = LittleLong (pin->numframes);

	size = sizeof (msprite_t) +	(numframes - 1) * sizeof (psprite->frames);

	psprite = (msprite_t *) Hunk_AllocName (size, loadname);

	psprite->type = LittleLong (pin->type);
	psprite->maxwidth = LittleLong (pin->width);
	psprite->maxheight = LittleLong (pin->height);
	psprite->beamlength = LittleFloat (pin->beamlength);
	mod->synctype = LittleLong (pin->synctype);
	psprite->numframes = numframes;

	mod->mins[0] = mod->mins[1] = -psprite->maxwidth/2;
	mod->maxs[0] = mod->maxs[1] = psprite->maxwidth/2;
	mod->mins[2] = -psprite->maxheight/2;
	mod->maxs[2] = psprite->maxheight/2;

	// load the frames
	if (numframes < 1 || numframes > MAX_SPRITE_FRAMES)
		Host_Error ("Mod_LoadSpriteModel: Invalid # of frames: %d\n", numframes);

	mod->numframes = numframes;

	pframetype = (dspriteframetype_t *)(pin + 1);

	size = sizeof(msprite2_t);

	for (i = 0; i < numframes; i++) {
		spriteframetype_t	frametype;

		frametype = LittleLong (pframetype->type);
		psprite->frames[i].type = frametype;

		if (frametype == SPR_SINGLE) {
			pframetype = (dspriteframetype_t *)	Mod_LoadSpriteFrame (pframetype + 1, &psprite->frames[i].frameptr, i);
			size += (int)sizeof(mspriteframe_t);
		} else {
			pframetype = (dspriteframetype_t *)	Mod_LoadSpriteGroup (pframetype + 1, &psprite->frames[i].frameptr, i);
			size += (((mspritegroup_t*)(psprite->frames[i].frameptr))->numframes) * (int)sizeof(mspriteframe2_t);
		}
	}

	psprite2 = Q_malloc(size); // !!!!!!! Q_free

	psprite2->type		= psprite->type;
	psprite2->maxwidth	= psprite->maxwidth;
	psprite2->maxheight	= psprite->maxheight;
	psprite2->numframes	= psprite->numframes;

	for (i = 0, offset = sizeof(msprite2_t); i < numframes; i++) {
		psprite2->frames[i].type   = psprite->frames[i].type;
		psprite2->frames[i].offset = offset;

		if (psprite2->frames[i].type == SPR_SINGLE) {
			psprite2->frames[i].numframes = 1; // always one

			sztmp = (int)sizeof(mspriteframe_t);
			if (offset + sztmp > size)
				Sys_Error("Mod_LoadSpriteModel: internal error");

			memcpy((byte*)(psprite2) + offset, psprite->frames[i].frameptr, sztmp);
			offset += sztmp;
		}
		else {
			psprite2->frames[i].numframes = ((mspritegroup_t*)(psprite->frames[i].frameptr))->numframes;

			for (j = 0; j < psprite2->frames[i].numframes; j++) {
				mspriteframe2_t spr;

				sztmp = (int)sizeof(mspriteframe2_t);
				if (offset + sztmp > size)
					Sys_Error("Mod_LoadSpriteModel: internal error");

				memset(&spr, 0, sizeof(spr));
				spr.frame    = *(((mspritegroup_t*)(psprite->frames[i].frameptr))->frames[j]); // copy whole struct
				spr.interval =   ((mspritegroup_t*)(psprite->frames[i].frameptr))->intervals[j];
				memcpy((byte*)(psprite2) + offset, &spr, sztmp);
				offset += sztmp;
			}
		}
	}

	mod->type = mod_sprite;

	// move the complete, relocatable model to the cache
	mod->cached_data = Q_malloc(size);
	if (mod->cached_data) {
		memcpy(mod->cached_data, psprite2, size);
	}

	Q_free(psprite2);
	Hunk_FreeToLowMark(start);
}

mspriteframe_t *R_GetSpriteFrame(entity_t *e, msprite2_t *psprite)
{
	mspriteframe_t  *pspriteframe;
	mspriteframe2_t *pspriteframe2;
	int i, numframes, frame, offset;
	float fullinterval, targettime, time;

	frame = e->frame;

	if (frame >= psprite->numframes || frame < 0) {
		Com_DPrintf ("R_GetSpriteFrame: no such frame %d (model %s)\n", frame, e->model->name);
		return NULL;
	}

	offset    = psprite->frames[frame].offset;
	numframes = psprite->frames[frame].numframes;

	if (offset < (int)sizeof(msprite2_t) || numframes < 1) {
		Com_Printf ("R_GetSpriteFrame: wrong sprite\n");
		return NULL;
	}

	if (psprite->frames[frame].type == SPR_SINGLE) {
		pspriteframe  = (mspriteframe_t* )((byte*)psprite + offset);
	}
	else {
		pspriteframe2 = (mspriteframe2_t*)((byte*)psprite + offset);

		fullinterval = pspriteframe2[numframes-1].interval;

		time = r_refdef2.time;

		// when loading in Mod_LoadSpriteGroup, we guaranteed all interval values
		// are positive, so we don't have to worry about division by 0
		targettime = time - ((int) (time / fullinterval)) * fullinterval;

		for (i = 0; i < (numframes - 1); i++) {
			if (pspriteframe2[i].interval > targettime) {
				break;
			}
		}

		pspriteframe = &pspriteframe2[i].frame;
	}

	return pspriteframe;
}
