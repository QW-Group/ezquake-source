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

$Id: gl_model.c,v 1.41 2007-10-07 08:06:33 tonik Exp $
*/
// gl_model.c  -- model loading and caching

// models are the only shared resource between a client and server running on the same machine.

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "rulesets.h"

void GLC_DrawFlat(model_t *model)
{
	extern glpoly_t* caustics_polys;

	msurface_t *s;
	int waterline, i, k;
	float *v;
	vec3_t n;
	byte w[3], f[3];
	qbool draw_caustics = underwatertexture && gl_caustics.value;

	memcpy(w, r_wallcolor.color, 3);
	memcpy(f, r_floorcolor.color, 3);

	GL_DisableMultitexture();

	// START shaman BUG /fog not working with /r_drawflat {
	if (gl_fogenable.value) {
		glEnable(GL_FOG);
	}
	// } END shaman BUG /fog not working with /r_drawflat

	glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_BLEND);

	GL_SelectTexture(GL_TEXTURE0);

	for (i = 0; i < model->numtextures; i++) {
		if (!model->textures[i] || (!model->textures[i]->texturechain[0] && !model->textures[i]->texturechain[1])) {
			continue;
		}

		for (waterline = 0; waterline < 2; waterline++) {
			if (!(s = model->textures[i]->texturechain[waterline]))
				continue;

			for (; s; s = s->texturechain) {
				GLC_SetTextureLightmap(s->lightmaptexturenum);

				v = s->polys->verts[0];
				VectorCopy(s->plane->normal, n);
				VectorNormalize(n);

				// r_drawflat 1 == All solid colors
				// r_drawflat 2 == Solid floor/ceiling only
				// r_drawflat 3 == Solid walls only

				if (n[2] < -0.5 || n[2] > 0.5) // floor or ceiling
				{
					if (r_drawflat.integer == 2 || r_drawflat.integer == 1) {
						glColor3ubv(f);
					}
					else {
						continue;
					}
				}
				else										// walls
				{
					if (r_drawflat.integer == 3 || r_drawflat.integer == 1) {
						glColor3ubv(w);
					}
					else {
						continue;
					}
				}

				glBegin(GL_POLYGON);
				for (k = 0; k < s->polys->numverts; k++, v += VERTEXSIZE) {
					glTexCoord2f(v[5], v[6]);
					glVertex3fv(v);
				}
				glEnd();

				// START shaman FIX /r_drawflat + /gl_caustics {
				if (waterline && draw_caustics) {
					s->polys->caustics_chain = caustics_polys;
					caustics_polys = s->polys;
				}
				// } END shaman FIX /r_drawflat + /gl_caustics
			}
		}
	}

	if (gl_fogenable.value) {
		glDisable(GL_FOG);
	}

	glColor3f(1.0f, 1.0f, 1.0f);
	// START shaman FIX /r_drawflat + /gl_caustics {
	EmitCausticsPolys();
	// } END shaman FIX /r_drawflat + /gl_caustics
}

void GLC_DrawBrushModel(entity_t* e, model_t* clmodel)
{
	extern cvar_t gl_outline;

	if (r_drawflat.value != 0 && clmodel->isworldmodel) {
		if (r_drawflat.integer == 1) {
			GLC_DrawFlat(clmodel);
		}
		else {
			DrawTextureChains(clmodel, (TruePointContents(e->origin)));//R00k added contents point for underwater bmodels
			GLC_DrawFlat(clmodel);
		}
	}
	else {
		DrawTextureChains(clmodel, (TruePointContents(e->origin)));//R00k added contents point for underwater bmodels
	}

	if ((gl_outline.integer & 2) && clmodel->isworldmodel && !RuleSets_DisallowModelOutline(NULL)) {
		R_DrawMapOutline(clmodel);
	}
}

void GLC_DrawWorld(void)
{
	extern msurface_t* alphachain;
	extern cvar_t gl_outline;

	if (r_drawflat.value) {
		if (r_drawflat.integer == 1) {
			GLC_DrawFlat(cl.worldmodel);
		}
		else {
			DrawTextureChains(cl.worldmodel, 0);
			GLC_DrawFlat(cl.worldmodel);
		}
	}
	else {
		DrawTextureChains(cl.worldmodel, 0);
	}

	if ((gl_outline.integer & 2) && !RuleSets_DisallowModelOutline(NULL)) {
		R_DrawMapOutline(cl.worldmodel);
	}

	//draw the world alpha textures
	R_DrawAlphaChain(alphachain);
}
