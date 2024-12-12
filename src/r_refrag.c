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
// gl_refrag.c

#include "quakedef.h"
#include "gl_model.h"
#include "r_local.h"

mnode_t	*r_pefragtopnode;

//===========================================================================

/*
===============================================================================
					ENTITY FRAGMENT FUNCTIONS
===============================================================================
*/

efrag_t		**lastlink;
vec3_t		r_emins, r_emaxs;
entity_t	*r_addent;

void R_Init_EFrags (void)
{
	cl.free_efrags = (efrag_t *) Hunk_AllocName (sizeof (efrag_t), "efrag");
	memset (cl.free_efrags, 0, sizeof (efrag_t));
}

void R_Next_Free_EFrag (void)
{
	// advance the linked list, growing it as needed
	if (cl.free_efrags->entnext == NULL)
	{
		cl.free_efrags->entnext = (efrag_t *) Hunk_AllocName (sizeof (efrag_t), "efrag");
		memset (cl.free_efrags->entnext, 0, sizeof (efrag_t));
	}
	cl.free_efrags = cl.free_efrags->entnext;
}

void R_SplitEntityOnNode (mnode_t *node) {
	efrag_t *ef;
	mplane_t *splitplane;
	mleaf_t *leaf;
	int sides;
	
	if (node->contents == CONTENTS_SOLID)
		return;

	// add an efrag if the node is a leaf

	if ( node->contents < 0) {
		if (!r_pefragtopnode)
			r_pefragtopnode = node;

		leaf = (mleaf_t *)node;

		R_Next_Free_EFrag ();

		ef = cl.free_efrags;
		ef->entity = r_addent;

		// add the entity link	
		*lastlink = ef;
		lastlink = &ef->entnext;
		ef->entnext = NULL;

		// set the leaf links
		ef->leaf = leaf;
		ef->leafnext = leaf->efrags;
		leaf->efrags = ef;

		return;
	}

	// NODE_MIXED

	splitplane = node->plane;
	sides = BOX_ON_PLANE_SIDE(r_emins, r_emaxs, splitplane);
	
	if (sides == 3) {
		// split on this plane
		// if this is the first splitter of this bmodel, remember it
		if (!r_pefragtopnode)
			r_pefragtopnode = node;
	}

	// recurse down the contacted sides
	if (sides & 1)
		R_SplitEntityOnNode (node->children[0]);

	if (sides & 2)
		R_SplitEntityOnNode (node->children[1]);
}

void R_AddEfrags (entity_t *ent) {
	model_t *entmodel;
	int i;

	if (!ent->model)
		return;

	r_addent = ent;

	lastlink = &ent->efrag;
	r_pefragtopnode = NULL;

	entmodel = ent->model;

	for (i = 0; i < 3; i++) {
		r_emins[i] = ent->origin[i] + entmodel->mins[i];
		r_emaxs[i] = ent->origin[i] + entmodel->maxs[i];
	}

	R_SplitEntityOnNode (cl.worldmodel->nodes);

	ent->topnode = r_pefragtopnode;
}

void R_StoreEfrags(efrag_t **ppefrag)
{
	entity_t *pent;
	model_t *model;
	efrag_t *pefrag;
	extern cvar_t r_drawflame;

	for (pefrag = *ppefrag; pefrag; pefrag = pefrag->leafnext) {
		pent = pefrag->entity;
		model = pent->model;

		if ((model->modhint == MOD_FLAME || model->modhint == MOD_FLAME2) && !r_drawflame.integer) {
			continue;
		}

		switch (model->type) {
			case mod_alias:
			case mod_alias3:
			case mod_brush:
			case mod_sprite:
				if (pent->visframe != r_framecount) {
					CL_AddEntity(pent);
					pent->visframe = r_framecount;	// mark that we've recorded this entity for this frame
					pent->oldframe = pent->frame;
				}
				break;

			default:
				Sys_Error("R_StoreEfrags: Bad entity type %d", model->type);
		}
	}
}
