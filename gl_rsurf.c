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
#include "gl_local.h"
#include "rulesets.h"
#include "utils.h"

void GLM_DrawBrushModel(model_t* model);

msurface_t	*skychain = NULL;
msurface_t	**skychain_tail = &skychain;

msurface_t	*waterchain = NULL;

msurface_t	*alphachain = NULL;
msurface_t	**alphachain_tail = &alphachain;

void CHAIN_SURF_SORTED_BY_TEX(msurface_t** chain_head, msurface_t* surf)
{
	msurface_t* current = *chain_head;
	int surf_order = surf->texinfo->miptex * 1000 + surf->lightmaptexturenum;

	while (current) {
		int current_order = current->texinfo->miptex * 1000 + current->lightmaptexturenum;

		if (surf_order > current_order) {
			chain_head = &(current->texturechain);
			current = *chain_head;
			continue;
		}

		break;
	}

	surf->texturechain = current;
	*chain_head = surf;
}

void CHAIN_SURF_DRAWFLAT(msurface_t** chain_head, msurface_t* surf)
{
	msurface_t* current = *chain_head;
	int surf_order = (surf->flags & SURF_DRAWFLAT_FLOOR ? 1 : 0) + surf->lightmaptexturenum * 2;

	while (current) {
		int current_order = (current->flags & SURF_DRAWFLAT_FLOOR ? 1 : 0) + current->lightmaptexturenum * 2;

		if (surf_order > current_order) {
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

glpoly_t *caustics_polys = NULL;
glpoly_t *detail_polys = NULL;

extern cvar_t gl_textureless; //Qrack

// mark all surfaces so ALL light maps will reload in R_RenderDynamicLightmaps()
static void R_ForceReloadLightMaps(void)
{
	model_t	*m;
	int i, j;

	Com_DPrintf("forcing of reloading all light maps!\n");

	for (j = 1; j < MAX_MODELS; j++)
	{
		if (!(m = cl.model_precache[j]))
			break;

		if (m->name[0] == '*')
			continue;

		for (i = 0; i < m->numsurfaces; i++)
		{
			m->surfaces[i].cached_dlight = true; // kinda hack, so we force reload light map
		}
	}
}

qbool R_FullBrightAllowed(void)
{
	return r_fullbright.value && r_refdef2.allow_cheats;
}

void R_Check_R_FullBright(void)
{
	static qbool allowed;

	// not changed, nothing to do
	if( allowed == R_FullBrightAllowed() )
		return;

	// ok, it changed, lets update all our light maps...
	allowed = R_FullBrightAllowed();
	R_ForceReloadLightMaps();
}

//Returns the proper texture for a given time and base texture
texture_t *R_TextureAnimation (texture_t *base) {
	int relative, count;

	if (currententity->frame) {
		if (base->alternate_anims)
			base = base->alternate_anims;
	}

	if (!base->anim_total)
		return base;

	relative = (int) (r_refdef2.time * 10) % base->anim_total;

	count = 0;	
	while (base->anim_min > relative || base->anim_max <= relative) {
		base = base->anim_next;
		if (!base)
			Host_Error ("R_TextureAnimation: broken cycle");
		if (++count > 100)
			Host_Error ("R_TextureAnimation: infinite cycle");
	}

	return base;
}

void R_DrawWaterSurfaces(void)
{
	if (!waterchain) {
		return;
	}

	if (GL_ShadersSupported()) {
		GLM_DrawWaterSurfaces();
	}
	else {
		GLC_DrawWaterSurfaces();
	}

	waterchain = NULL;
}

static void R_ClearTextureChains(model_t *clmodel) {
	int i, waterline;
	texture_t *texture;

	GLC_ClearTextureChains();

	for (i = 0; i < clmodel->numtextures; i++) {
		for (waterline = 0; waterline < 2; waterline++) {
			if ((texture = clmodel->textures[i])) {
				texture->texturechain[waterline] = NULL;
				texture->texturechain_tail[waterline] = &texture->texturechain[waterline];
			}
		}
	}

	r_notexture_mip->texturechain[0] = NULL;
	r_notexture_mip->texturechain_tail[0] = &r_notexture_mip->texturechain[0];
	r_notexture_mip->texturechain[1] = NULL;
	r_notexture_mip->texturechain_tail[1] = &r_notexture_mip->texturechain[1];


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

void R_DrawBrushModel (entity_t *e) {
	int i, k, underwater;
	extern cvar_t gl_outline;
	unsigned int li;
	unsigned int lj;
	vec3_t mins, maxs;
	msurface_t *psurf;
	float dot;
	mplane_t *pplane;
	model_t *clmodel;
	qbool rotated;
	float oldMatrix[16];

	currententity = e;
	currenttexture = -1;

	clmodel = e->model;

	if (e->angles[0] || e->angles[1] || e->angles[2]) {
		rotated = true;
		if (R_CullSphere(e->origin, clmodel->radius)) {
			return;
		}
	}
	else {
		rotated = false;
		VectorAdd (e->origin, clmodel->mins, mins);
		VectorAdd (e->origin, clmodel->maxs, maxs);

		if (R_CullBox(mins, maxs)) {
			return;
		}
	}

	if (e->alpha) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GL_TextureEnvMode(GL_MODULATE);
		glColor4f (1, 1, 1, e->alpha);
	}
	else {
		glColor3f (1,1,1);
	}

	VectorSubtract (r_refdef.vieworg, e->origin, modelorg);
	if (rotated) {
		vec3_t	temp;
		vec3_t	forward, right, up;

		VectorCopy (modelorg, temp);
		AngleVectors (e->angles, forward, right, up);
		modelorg[0] = DotProduct (temp, forward);
		modelorg[1] = -DotProduct (temp, right);
		modelorg[2] = DotProduct (temp, up);
	}

	psurf = &clmodel->surfaces[clmodel->firstmodelsurface];

	// calculate dynamic lighting for bmodel if it's not an instanced model
	if (clmodel->firstmodelsurface) {
		for (li = 0; li < MAX_DLIGHTS/32; li++) {
			if (cl_dlight_active[li]) {
				for (lj = 0; lj < 32; lj++) {
					if ((cl_dlight_active[li]&(1 << lj)) && li*32 + lj < MAX_DLIGHTS) {
						k = li*32 + lj;

						if (!gl_flashblend.integer || (cl_dlights[k].bubble && gl_flashblend.integer != 2)) {
							R_MarkLights (&cl_dlights[k], 1 << k, clmodel->nodes + clmodel->firstnode);
						}
					}
				}
			}
		}
	}

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);

	GL_Translate(GL_MODELVIEW, e->origin[0],  e->origin[1],  e->origin[2]);
	GL_Rotate(GL_MODELVIEW, e->angles[1], 0, 0, 1);
	GL_Rotate(GL_MODELVIEW, e->angles[0], 0, 1, 0);
	GL_Rotate(GL_MODELVIEW, e->angles[2], 1, 0, 0);

	R_ClearTextureChains(clmodel);

	for (i = 0; i < clmodel->nummodelsurfaces; i++, psurf++) {
		// find which side of the node we are on
		pplane = psurf->plane;
		dot = PlaneDiff(modelorg, pplane);

		//draw the water surfaces now, and setup sky/normal chains
		if (	((psurf->flags & SURF_PLANEBACK) && (dot < -BACKFACE_EPSILON)) || 
				(!(psurf->flags & SURF_PLANEBACK) && (dot > BACKFACE_EPSILON)))
		{
			if (psurf->flags & SURF_DRAWSKY) {	
				CHAIN_SURF_B2F(psurf, skychain);
			} else if (psurf->flags & SURF_DRAWTURB) {	
				EmitWaterPolys (psurf);
			} else if (psurf->flags & SURF_DRAWALPHA) {
				
				CHAIN_SURF_B2F(psurf, alphachain);
			} else {
				
				underwater = (psurf->flags & SURF_UNDERWATER) ? 1 : 0;
				CHAIN_SURF_B2F(psurf, psurf->texinfo->texture->texturechain[underwater]);
			}
		}
	}

	// START shaman FIX for no simple textures on world brush models {
	//draw the textures chains for the model
	R_RenderAllDynamicLightmaps(clmodel);

	if (GL_ShadersSupported()) {
		GLM_DrawBrushModel(clmodel);

		// TODO: DrawSkyChain/DrawAlphaChain
	}
	else {
		GLC_DrawBrushModel(e, clmodel);

		R_DrawSkyChain();

		R_DrawAlphaChain(alphachain);
	}
	// } END shaman FIX for no simple textures on world brush models

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}


void R_RecursiveWorldNode (mnode_t *node, int clipflags) {
	int c, side, clipped, underwater;
	mplane_t *plane, *clipplane;
	msurface_t *surf, **mark;
	mleaf_t *pleaf;
	float dot;
	qbool drawFlatFloors = (r_drawflat.integer == 2 || r_drawflat.integer == 1);
	qbool drawFlatWalls = (r_drawflat.integer == 3 || r_drawflat.integer == 1);

	if (node->contents == CONTENTS_SOLID)
		return;		// solid
	if (node->visframe != r_visframecount)
		return;
	for (c = 0, clipplane = frustum; c < 4; c++, clipplane++) {
		if (!(clipflags & (1 << c)))
			continue;	// don't need to clip against it

		clipped = BOX_ON_PLANE_SIDE (node->minmaxs, node->minmaxs + 3, clipplane);
		if (clipped == 2)
			return;
		else if (clipped == 1)
			clipflags &= ~(1<<c);	// node is entirely on screen
	}

	// if a leaf node, draw stuff
	if (node->contents < 0)	{
		pleaf = (mleaf_t *) node;

		mark = pleaf->firstmarksurface;
		c = pleaf->nummarksurfaces;

		if (c) {
			do {
				(*mark)->visframe = r_framecount;
				mark++;
			} while (--c);
		}

	// deal with model fragments in this leaf
		if (pleaf->efrags)
			R_StoreEfrags (&pleaf->efrags);

		return;
	}

	// node is just a decision point, so go down the apropriate sides

	// find which side of the node we are on
	plane = node->plane;

	dot = PlaneDiff(modelorg, plane);
	side = (dot >= 0) ? 0 : 1;

	// recurse down the children, front side first
	R_RecursiveWorldNode (node->children[side], clipflags);

	// draw stuff
	c = node->numsurfaces;

	if (c)	{
		surf = cl.worldmodel->surfaces + node->firstsurface;

		if (dot < -BACKFACE_EPSILON)
			side = SURF_PLANEBACK;
		else if (dot > BACKFACE_EPSILON)
			side = 0;

		for ( ; c; c--, surf++) {
			if (surf->visframe != r_framecount)
				continue;

			if ((dot < 0) ^ !!(surf->flags & SURF_PLANEBACK))
				continue;		// wrong side

			// add surf to the right chain
			if (surf->flags & SURF_DRAWSKY) {
				CHAIN_SURF_SORTED_BY_TEX(&skychain, surf);
			}
			else if (surf->flags & SURF_DRAWTURB) {
				CHAIN_SURF_SORTED_BY_TEX(&waterchain, surf);
			}
			else if (surf->flags & SURF_DRAWALPHA) {
				CHAIN_SURF_B2F(surf, alphachain);
			}
			else {
				underwater = (underwatertexture && gl_caustics.value && (surf->flags & SURF_UNDERWATER)) ? 1 : 0;

				if (drawFlatFloors && (surf->flags & SURF_DRAWFLAT_FLOOR)) {
					CHAIN_SURF_DRAWFLAT(&cl.worldmodel->drawflat_chain[underwater], surf);
				}
				else if (drawFlatWalls && !(surf->flags & SURF_DRAWFLAT_FLOOR)) {
					CHAIN_SURF_DRAWFLAT(&cl.worldmodel->drawflat_chain[underwater], surf);
				}
				else {
					CHAIN_SURF_SORTED_BY_TEX(&surf->texinfo->texture->texturechain[underwater], surf);
				}
			}
		}
	}
	// recurse down the back side
	R_RecursiveWorldNode (node->children[!side], clipflags);
}

void R_CreateWorldTextureChains(void)
{
	if (cl.worldmodel) {
		R_ClearTextureChains(cl.worldmodel);

		VectorCopy(r_refdef.vieworg, modelorg);

		//set up texture chains for the world
		cl.worldmodel->drawflat_chain[0] = cl.worldmodel->drawflat_chain[1] = NULL;
		R_RecursiveWorldNode(cl.worldmodel->nodes, 15);

		R_RenderAllDynamicLightmaps(cl.worldmodel);
	}
}

void R_DrawWorld(void)
{
	entity_t ent;
	extern cvar_t gl_outline;

	memset(&ent, 0, sizeof(ent));
	ent.model = cl.worldmodel;

	VectorCopy(r_refdef.vieworg, modelorg);

	currententity = &ent;
	currenttexture = -1;

	//draw the world sky
	GL_EnterRegion("R_DrawSky");
	R_DrawSky();
	GL_LeaveRegion();

	GL_EnterRegion("Entities-1st");
	R_DrawEntitiesOnList(&cl_firstpassents);
	GL_LeaveRegion();

	if (GL_ShadersSupported()) {
		GL_EnterRegion("DrawWorld");
		GLM_DrawTexturedWorld(cl.worldmodel);
		GL_LeaveRegion();
	}
	else {
		GLC_DrawWorld();
	}
}

void R_MarkLeaves (void) {
	byte *vis;
	mnode_t *node;
	int i;
	byte solid[MAX_MAP_LEAFS/8];

	if (!r_novis.value && r_oldviewleaf == r_viewleaf
		&& r_oldviewleaf2 == r_viewleaf2)	// watervis hack
		return;

	r_visframecount++;
	r_oldviewleaf = r_viewleaf;

	if (r_novis.value) {
		vis = solid;
		memset (solid, 0xff, (cl.worldmodel->numleafs + 7) >> 3);
	} else {
		vis = Mod_LeafPVS (r_viewleaf, cl.worldmodel);

		if (r_viewleaf2) {
			int			i, count;
			unsigned	*src, *dest;

			// merge visibility data for two leafs
			count = (cl.worldmodel->numleafs + 7) >> 3;
			memcpy (solid, vis, count);
			src = (unsigned *) Mod_LeafPVS (r_viewleaf2, cl.worldmodel);
			dest = (unsigned *) solid;
			count = (count + 3) >> 2;
			for (i = 0; i < count; i++)
				*dest++ |= *src++;
			vis = solid;
		}
	}
		
	for (i = 0; i < cl.worldmodel->numleafs; i++)	{
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
