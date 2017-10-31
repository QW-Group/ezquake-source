/*
Copyright (C) 2011 fuh and ezQuake team

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

// Code to display MD3 models (classic GL/immediate-mode)

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_md3.h"
#include "vx_vertexlights.h" 

/*
To draw, for each surface, run through the triangles, getting tex coords from s+t, 
*/
void GLC_DrawAlias3Model(entity_t *ent)
{
	extern cvar_t cl_drawgun, r_viewmodelsize, r_lerpframes, gl_smoothmodels, gl_affinemodels, gl_fb_models;
	extern byte	*shadedots;
	extern byte	r_avertexnormal_dots[SHADEDOT_QUANT][NUMVERTEXNORMALS];
	extern float shadelight, ambientlight;
	extern qbool full_light;
	extern void R_AliasSetupLighting(entity_t *ent);

	float lerpfrac, scale;
	int distance = MD3_INTERP_MAXDIST / MD3_XYZ_SCALE;
	vec3_t interpolated_verts;

	md3model_t *mhead;
	md3Header_t *pheader;
	model_t *mod;
	int surfnum, numtris, i;
	md3Surface_t *surf;

	int frame1 = ent->oldframe, frame2 = ent->frame;
	md3XyzNormal_t *verts, *v1, *v2;

	surfinf_t *sinf;

	unsigned int	*tris;
	md3St_t *tc;

	//	float ang;

	float r_modelalpha;
	float oldMatrix[16];

	mod = ent->model;

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);
	R_RotateForEntity (ent);

	// 
	r_modelalpha = ((ent->renderfx & RF_WEAPONMODEL) && gl_mtexable) ? bound(0, cl_drawgun.value, 1) : 1;
	if (ent->alpha) {
		r_modelalpha = ent->alpha;
	}

	GLC_StateBeginMD3Draw(r_modelalpha);

	scale = (ent->renderfx & RF_WEAPONMODEL) ? bound(0.5, r_viewmodelsize.value, 1) : 1;
	// perform two scalling at once, one scalling for MD3_XYZ_SCALE, other for r_viewmodelsize
	GL_Scale(GL_MODELVIEW, scale * MD3_XYZ_SCALE, MD3_XYZ_SCALE, MD3_XYZ_SCALE);
	GL_Color4f(1, 1, 1, r_modelalpha);

	R_AliasSetupLighting(ent);
	shadedots = r_avertexnormal_dots[((int) (ent->angles[1] * (SHADEDOT_QUANT / 360.0))) & (SHADEDOT_QUANT - 1)];
	if (gl_fb_models.integer) {
		ambientlight = 999999;
	}

	mhead = (md3model_t *)Mod_Extradata (mod);
	sinf = (surfinf_t *)((char *)mhead + mhead->surfinf);
	pheader = (md3Header_t *)((char *)mhead + mhead->md3model);

	if (frame1 >= pheader->numFrames) {
		frame1 = pheader->numFrames - 1;
	}
	if (frame2 >= pheader->numFrames) {
		frame2 = pheader->numFrames - 1;
	}

	if (!r_lerpframes.value || ent->framelerp < 0 || ent->oldframe == ent->frame) {
		lerpfrac = 1.0;
	}
	else {
		lerpfrac = min(ent->framelerp, 1);
	}

	surf = (md3Surface_t *)((char *)pheader + pheader->ofsSurfaces);
	for (surfnum = 0; surfnum < pheader->numSurfaces; surfnum++) {
		// loop through the surfaces.
		int pose1 = frame1 * surf->numVerts;
		int pose2 = frame2 * surf->numVerts;

		//skin texture coords.
		tc = (md3St_t *)((char *)surf + surf->ofsSt);
		verts = (md3XyzNormal_t *)((char *)surf + surf->ofsXyzNormals);

		tris = (unsigned int *)((char *)surf + surf->ofsTriangles);
		numtris = surf->numTriangles * 3;

		if (GL_TextureReferenceIsValid(sinf->texnum)) {
			GLC_InitTextureUnits1(sinf->texnum, GL_REPLACE);
		}
		else {
			GLC_DisableAllTexturing();
		}

		glBegin (GL_TRIANGLES);
		for (i = 0; i < numtris; i++) {
			float s, t;
			float l;

			v1 = verts + *tris + pose1;
			v2 = verts + *tris + pose2;

			s = tc[*tris].s;
			t = tc[*tris].t;

			lerpfrac = VectorL2Compare(v1->xyz, v2->xyz, distance) ? lerpfrac : 1;

			l = FloatInterpolate(shadedots[v1->normal >> 8], lerpfrac, shadedots[v2->normal >> 8]);
			l = (l * shadelight + ambientlight) / 256;
			l = min(l, 1);

			glColor4f(l, l, l, r_modelalpha);

			VectorInterpolate(v1->xyz, lerpfrac, v2->xyz, interpolated_verts);
			glTexCoord2f(s, t);
			glVertex3fv(interpolated_verts);

			tris++;
		}
		glEnd();

		//NEXT!   Getting cocky!
		surf = (md3Surface_t *)((char *)surf + surf->ofsEnd);
	}

	GLC_StateEndMD3Draw();

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}
