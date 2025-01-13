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
// r_surf.c: surface-related refresh code

#include "quakedef.h"
#include "gl_model.h"
#include "rulesets.h"
#include "utils.h"
#include "r_performance.h"
#include "r_local.h"
#include "r_texture.h"
#include "r_lighting.h"
#include "r_brushmodel.h"
#include "r_brushmodel_sky.h"
#include "r_lightmaps.h"
#include "r_trace.h"
#include "r_renderer.h"

// Move to API
void GLC_ClearTextureChains(void);

// gl_refrag.c
void R_StoreEfrags(efrag_t **ppefrag);

static void R_TurbSurfacesEmitParticleEffects(msurface_t* s);

msurface_t	*skychain = NULL;
msurface_t	**skychain_tail = &skychain;

msurface_t	*waterchain = NULL;

msurface_t	*alphachain = NULL;
msurface_t	**alphachain_tail = &alphachain;

typedef void(*chain_surf_func)(msurface_t** chain_head, msurface_t* surf);

void chain_surfaces_simple(msurface_t** chain_head, msurface_t* surf)
{
	surf->texturechain = *chain_head;
	*chain_head = surf;
}

void chain_surfaces_simple_drawflat(msurface_t** chain_head, msurface_t* surf)
{
	surf->drawflatchain = *chain_head;
	*chain_head = surf;
}

void chain_surfaces_by_lightmap(msurface_t** chain_head, msurface_t* surf)
{
	msurface_t* current = *chain_head;
	qbool alphaSurface = surf->flags & SURF_DRAWALPHA;

	while (current) {
		if ((alphaSurface && !(current->flags & SURF_DRAWALPHA)) || (!alphaSurface && surf->lightmaptexturenum > current->lightmaptexturenum)) {
			chain_head = &(current->texturechain);
			current = *chain_head;
			continue;
		}

		break;
	}

	surf->texturechain = current;
	*chain_head = surf;
}

#define CHAIN_SURF_F2B(surf, chain_tail)		\
	{											\
		*(chain_tail) = (surf);					\
		(chain_tail) = &(surf)->texturechain;	\
		(surf)->texturechain = NULL;			\
	}

#define CHAIN_SURF_B2F(surf, chain) 			\
	{											\
		(surf)->texturechain = (chain);			\
		(chain) = (surf);						\
	}

#define CHAIN_RESET(chain)			\
{								\
	chain = NULL;				\
	chain##_tail = &chain;		\
}

//Returns the proper texture for a given time and base texture
texture_t *R_TextureAnimation(entity_t* ent, texture_t *base)
{
	int relative, count;

	if (ent && ent->frame && base->alternate_anims) {
		base = base->alternate_anims;
	}

	if (!base->anim_total) {
		return base;
	}

	relative = (int) (r_refdef2.time * 10) % base->anim_total;

	count = 0;
	while (base->anim_min > relative || base->anim_max <= relative) {
		base = base->anim_next;
		if (!base) {
			Host_Error("R_TextureAnimation: broken cycle");
		}
		if (++count > 100) {
			Host_Error("R_TextureAnimation: infinite cycle");
		}
	}

	return base;
}

void R_BrushModelClearTextureChains(model_t *clmodel)
{
	int i;
	texture_t *texture;

#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		GLC_ClearTextureChains();
	}
#endif

	clmodel->texturechains_have_lumas = false;
	for (i = 0; i < clmodel->numtextures; i++) {
		if ((texture = clmodel->textures[i])) {
			texture->texturechain = NULL;
			texture->texturechain_tail = &texture->texturechain;
		}
	}
	clmodel->drawflat_chain = NULL;
	clmodel->drawflat_todo = false;
	clmodel->alphapass_todo = false;
	clmodel->first_texture_chained = clmodel->numtextures;
	clmodel->last_texture_chained = -1;

	r_notexture_mip->texturechain = NULL;
	r_notexture_mip->texturechain_tail = &r_notexture_mip->texturechain;

	CHAIN_RESET(skychain);
	if (clmodel == cl.worldmodel) {
		waterchain = NULL;
	}
	CHAIN_RESET(alphachain);
}

void OnChange_r_drawflat (cvar_t *var, char *value, qbool *cancel) {
	char *p;
	qbool progress = false;


	p = Info_ValueForKey (cl.serverinfo, "status");
	progress = (strstr (p, "left")) ? true : false;

	if (cls.state >= ca_connected && progress && !r_refdef2.allow_cheats && !cl.spectator) {
		Com_Printf ("%s changes are not allowed during the match.\n", var->name);
		*cancel = true;
		return;
	}
}

void R_RecursiveWorldNode(mnode_t *node, int clipflags)
{
	extern cvar_t r_fastturb, r_fastsky;
	model_t* clmodel = cl.worldmodel;

	int c, side, clipped;
	mplane_t *plane, *clipplane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	float dot;

	if (node->contents == CONTENTS_SOLID || node->visframe != r_visframecount) {
		return;
	}
	for (c = 0, clipplane = frustum; c < 4; c++, clipplane++) {
		if (!(clipflags & (1 << c))) {
			continue;	// don't need to clip against it
		}

		clipped = BOX_ON_PLANE_SIDE(node->minmaxs, node->minmaxs + 3, clipplane);
		if (clipped == 2) {
			return;
		}
		else if (clipped == 1) {
			clipflags &= ~(1 << c);	// node is entirely on screen
		}
	}

	// if a leaf node, draw stuff
	if (node->contents < 0) {
		pleaf = (mleaf_t *)node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

		// deal with model fragments in this leaf
		if (pleaf->efrags) {
			R_StoreEfrags(&pleaf->efrags);
		}

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;

	dot = PlaneDiff(modelorg, plane);
	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode(node->children[side], clipflags);

	// draw stuff
	c = node->numsurfaces;

	if (c) {
		qbool turbSurface;
		qbool alphaSurface;

		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < -BACKFACE_EPSILON) {
			side = SURF_PLANEBACK;
		}
		else if (dot > BACKFACE_EPSILON) {
			side = 0;
		}

		for (; c; c--, surf++) {
			if (surf->visframe != r_framecount) {
				continue;
			}

			if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK)) {
				continue;		// wrong side
			}

			// add surf to the right chain
			cl.worldmodel->first_texture_chained = min(cl.worldmodel->first_texture_chained, surf->texinfo->miptex);
			cl.worldmodel->last_texture_chained = max(cl.worldmodel->last_texture_chained, surf->texinfo->miptex);

			turbSurface = (surf->flags & SURF_DRAWTURB);
			alphaSurface = (surf->flags & SURF_DRAWALPHA);
			if (surf->flags & SURF_DRAWSKY) {
				if (r_fastsky.integer || R_UseModernOpenGL()) {
					chain_surfaces_simple_drawflat(&cl.worldmodel->drawflat_chain, surf);
					cl.worldmodel->drawflat_todo = true;
				}
				else if (!r_fastsky.integer) {
					chain_surfaces_simple(&skychain, surf);
				}
			}
			else if (turbSurface) {
				if (r_fastturb.integer && r_refdef2.wateralpha == 1) {
					chain_surfaces_simple_drawflat(&cl.worldmodel->drawflat_chain, surf);
					cl.worldmodel->drawflat_todo = true;
				}
				else if (r_refdef2.solidTexTurb && R_UseModernOpenGL()) {
					chain_surfaces_simple(&surf->texinfo->texture->texturechain, surf);
				}
				else {
					chain_surfaces_simple(&waterchain, surf);
				}
				R_TurbSurfacesEmitParticleEffects(surf);
			}
			else {
				if (!alphaSurface && r_refdef2.drawFlatFloors && (surf->flags & SURF_DRAWFLAT_FLOOR)) {
					if (R_UseImmediateOpenGL()) {
						R_AddDrawflatChainSurface(surf, true);
					}
					else {
						chain_surfaces_simple_drawflat(&cl.worldmodel->drawflat_chain, surf);
					}
					cl.worldmodel->drawflat_todo = true;
				}
				else if (!alphaSurface && r_refdef2.drawFlatWalls && !(surf->flags & SURF_DRAWFLAT_FLOOR)) {
					if (R_UseImmediateOpenGL()) {
						R_AddDrawflatChainSurface(surf, false);
					}
					else {
						chain_surfaces_simple_drawflat(&cl.worldmodel->drawflat_chain, surf);
					}
					cl.worldmodel->drawflat_todo = true;
				}
				else {
					clmodel->texturechains_have_lumas |= R_TextureAnimation(NULL, surf->texinfo->texture)->isLumaTexture;
					clmodel->alphapass_todo |= alphaSurface;

					if (R_UseImmediateOpenGL()) {
						if (alphaSurface) {
							CHAIN_SURF_B2F(surf, surf->texinfo->texture->texturechain);
						}
						else {
							chain_surfaces_by_lightmap(&surf->texinfo->texture->texturechain, surf);
						}
					}
					else {
						chain_surfaces_simple(&surf->texinfo->texture->texturechain, surf);
					}
				}
			}
		}
	}
	// recurse down the back side
	R_RecursiveWorldNode(node->children[!side], clipflags);
}

void R_CreateWorldTextureChains(void)
{
	extern cvar_t r_drawworld;

	if (cl.worldmodel && (!cls.timedemo || r_drawworld.integer)) {
		R_BrushModelClearTextureChains(cl.worldmodel);

		VectorCopy(r_refdef.vieworg, modelorg);

		//set up texture chains for the world
		R_RecursiveWorldNode(cl.worldmodel->nodes, 15);

		// these are rendered later now, so we can process earlier
		R_RenderDlights();

		R_RenderAllDynamicLightmaps(cl.worldmodel);
	}
}

void R_DrawWorld(void)
{
	R_TraceEnterNamedRegion("R_DrawWorld");
	VectorCopy(r_refdef.vieworg, modelorg);

	//draw the world sky
	R_DrawSky();

	renderer.DrawWorld();

	if (r_refdef2.wateralpha == 1) {
		renderer.DrawWaterSurfaces();
	}
	R_TraceLeaveNamedRegion();
}

void R_MarkLeaves(void)
{
	byte *vis;
	mnode_t *node;
	int i;
	byte solid[MAX_MAP_LEAFS / 8];
	extern cvar_t r_novis;

	if (!r_novis.value && r_oldviewleaf == r_viewleaf && r_oldviewleaf2 == r_viewleaf2) {
		// watervis hack
		return;
	}

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value) {
		vis = solid;
		memset(solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	}
	else {
		vis = Mod_LeafPVS(r_viewleaf, cl.worldmodel);

		if (r_viewleaf2) {
			int			i, count;
			unsigned	*src, *dest;

			// merge visibility data for two leafs
			count = (cl.worldmodel->numleafs + 7) >> 3;
			memcpy(solid, vis, count);
			src = (unsigned *)Mod_LeafPVS(r_viewleaf2, cl.worldmodel);
			dest = (unsigned *)solid;
			count = (count + 3) >> 2;
			for (i = 0; i < count; i++)
				*dest++ |= *src++;
			vis = solid;
		}
	}

	for (i = 0; i < cl.worldmodel->numleafs; i++) {
		if (vis[i >> 3] & (1 << (i & 7))) {
			node = (mnode_t *)&cl.worldmodel->leafs[i + 1];
			do {
				if (node->visframe == r_visframecount)
					break;
				node->visframe = r_visframecount;
				node = node->parent;
			} while (node);
		}
	}
}

static void R_TurbSurfacesEmitParticleEffects(msurface_t* s)
{
#define ESHADER(eshadername)  extern void eshadername (vec3_t org)
	extern void EmitParticleEffect(msurface_t *fa, void (*fun)(vec3_t nv));
	extern cvar_t tei_lavafire, tei_slime;

	ESHADER(FuelRodExplosion);//green mushroom explosion
	ESHADER(ParticleFire);//torch fire
	ESHADER(ParticleFirePool);//lavapool alike fire
	ESHADER(VX_DeathEffect);//big white spark explosion
	ESHADER(VX_GibEffect);//huge red blood cloud
	ESHADER(VX_DetpackExplosion);//cool huge explosion
	ESHADER(VX_Implosion);//TODO
	ESHADER(VX_TeslaCharge);
	ESHADER(ParticleSlime);
	ESHADER(ParticleSlimeHarcore);
	ESHADER(ParticleBloodPool);
	ESHADER(ParticleSlimeBubbles); //HyperNewbie particles init
	ESHADER(ParticleSlimeGlow);
	ESHADER(ParticleSmallerFirePool);
	ESHADER(ParticleLavaSmokePool);

	if (!tei_lavafire.integer && !tei_slime.integer) {
		return;
	}

	//Tei "eshaders".
	if (s->texinfo && s->texinfo->texture && s->texinfo->texture->name[0]) {
		switch (s->texinfo->texture->name[1]) {
			//Lava
		case 'l':
		case 'L':
		{
			switch (tei_lavafire.integer) {
			case 1:
				//Tei lavafire, normal
				EmitParticleEffect(s, ParticleFire);
				break;
			case 2:
				//Tei lavafire HARDCORE
				EmitParticleEffect(s, ParticleFirePool);
				//Tei redblood smoke
				EmitParticleEffect(s, ParticleBloodPool);
				break;
			case 3:
				//HyperNewbie's smokefire
				EmitParticleEffect(s, ParticleSmallerFirePool);
				EmitParticleEffect(s, ParticleLavaSmokePool);
				break;
			case 4:
				EmitParticleEffect(s, ParticleSmallerFirePool);
				EmitParticleEffect(s, ParticleLavaSmokePool);
				EmitParticleEffect(s, ParticleLavaSmokePool);
				EmitParticleEffect(s, ParticleLavaSmokePool);
				break;
			}
			break;
		}
		case 't':
			//Teleport
			//TODO: a cool implosion subtel fx
			//		EmitParticleEffect(s,VX_Implosion);
			break;
		case 's':
		{
			switch (tei_slime.integer) {
			case 1:
				EmitParticleEffect(s, ParticleSlime);
				break;
			case 2:
				EmitParticleEffect(s, ParticleSlimeHarcore);
				break;
			case 3:
				if (!(rand() % 40)) {
					EmitParticleEffect(s, ParticleSlimeGlow);
				}
				if (!(rand() % 40)) {
					EmitParticleEffect(s, ParticleSlimeBubbles);
				}
				break;
			case 4:
				if (!(rand() % 10)) {
					EmitParticleEffect(s, ParticleSlimeGlow);
				}
				if (!(rand() % 10)) {
					EmitParticleEffect(s, ParticleSlimeBubbles);
				}
				break;
			}
			break;
		}
		case 'w':
			//	EmitParticleEffect(s,VX_TeslaCharge);
			break;
		default:
			break;
		}
	}
}

qbool R_DrawWorldOutlines(void)
{
	extern cvar_t gl_outline;

	return (gl_outline.integer & 2) && !RuleSets_DisallowModelOutline(NULL);
}
