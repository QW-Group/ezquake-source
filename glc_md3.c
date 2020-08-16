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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "glc_local.h"
#include "r_aliasmodel_md3.h"
#include "vx_vertexlights.h" 
#include "r_matrix.h"
#include "r_state.h"
#include "r_aliasmodel.h"
#include "glc_local.h"
#include "r_renderer.h"
#include "r_program.h"
#include "tr_types.h"
#include "rulesets.h"

const float* GLC_PowerupShell_ScrollParams(void);
void GLC_SetPowerupShellColor(int layer_no, int effects);
void GLC_PowerupShellColor(int layer_no, int effects, float* color);
qbool GLC_AliasModelStandardCompileSpecific(int subprogram_index);
int GLC_AliasModelSubProgramIndex(qbool textured, qbool fullbright, qbool caustics, qbool muzzlehack);
qbool GLC_AliasModelShellCompile(void);

static void GLC_AliasModelLightPointMD3(float color[4], const entity_t* ent, ezMd3XyzNormal_t* vert1, ezMd3XyzNormal_t* vert2, float lerpfrac)
{
	float l;
	extern cvar_t amf_lighting_vertex, amf_lighting_colour;

	// VULT VERTEX LIGHTING
	if (amf_lighting_vertex.integer && !ent->full_light) {
		vec3_t lc;

		l = VLight_LerpLightByAngles(vert1->normal_lat, vert1->normal_lng, vert2->normal_lat, vert2->normal_lng, lerpfrac, ent->angles[0], ent->angles[1]);
		l *= (ent->shadelight + ent->ambientlight) / 256.0;
		l = min(l, 1);

		if (amf_lighting_colour.integer) {
			int i;
			for (i = 0; i < 3; i++) {
				lc[i] = l * ent->lightcolor[i] / 255;
				lc[i] = min(lc[i], 1);
			}
		}
		else {
			VectorSet(lc, l, l, l);
		}

		if (ent->r_modelcolor[0] < 0) {
			// normal color
			VectorCopy(lc, color);
		}
		else {
			color[0] = ent->r_modelcolor[0] * lc[0];
			color[1] = ent->r_modelcolor[1] * lc[1];
			color[2] = ent->r_modelcolor[2] * lc[2];
		}
	}
	else {
		float yaw_rad = ent->angles[YAW] * M_PI / 180.0;
		vec3_t angleVector = { cos(-yaw_rad), sin(yaw_rad), 1 };

		VectorNormalize(angleVector);
		l = FloatInterpolate(DotProduct(angleVector, vert1->normal), lerpfrac, DotProduct(angleVector, vert2->normal));
		l = (l * ent->shadelight + ent->ambientlight) / 256.0;
		l = min(l, 1);

		if (ent->custom_model == NULL) {
			if (ent->r_modelcolor[0] < 0) {
				color[0] = color[1] = color[2] = l;
			}
			else {
				color[0] = ent->r_modelcolor[0] * l;
				color[1] = ent->r_modelcolor[1] * l;
				color[2] = ent->r_modelcolor[2] * l;
			}
		}
		else {
			color[0] = ent->custom_model->color_cvar.color[0] / 255.0f;
			color[1] = ent->custom_model->color_cvar.color[1] / 255.0f;
			color[2] = ent->custom_model->color_cvar.color[2] / 255.0f;
		}
	}

	color[0] *= ent->r_modelalpha;
	color[1] *= ent->r_modelalpha;
	color[2] *= ent->r_modelalpha;
	color[3] = ent->r_modelalpha;
}

static void GLC_DrawMD3Frame(const entity_t* ent, md3Header_t* pheader, int frame1, int frame2, float lerpfrac, const surfinf_t* surface_info, qbool invalidate_texture, qbool outline)
{
	md3Surface_t *surf;
	int surfnum;
	ezMd3XyzNormal_t *verts, *v1, *v2;
	md3St_t *tc;
	unsigned int* tris;
	int numtris, i;
	const int distance = MD3_INTERP_MAXDIST / MD3_XYZ_SCALE;

	MD3_ForEachSurface(pheader, surf, surfnum) {
		// loop through the surfaces.
		int pose1 = frame1 * surf->numVerts;
		int pose2 = frame2 * surf->numVerts;

		if (R_TextureReferenceIsValid(surface_info[surfnum].texnum) && !invalidate_texture) {
			renderer.TextureUnitBind(0, surface_info[surfnum].texnum);
		}

		//skin texture coords.
		tc = MD3_SurfaceTextureCoords(surf);
		verts = MD3_SurfaceVertices(surf);

		tris = (unsigned int*)MD3_SurfaceTriangles(surf);
		numtris = surf->numTriangles * 3;

		GLC_Begin(GL_TRIANGLES);
		for (i = 0; i < numtris; i++) {
			float s, t;
			float vertexColor[4], interpolated_verts[3];

			v1 = verts + *tris + pose1;
			v2 = verts + *tris + pose2;

			s = tc[*tris].s;
			t = tc[*tris].t;

			lerpfrac = VectorL2Compare(v1->xyz, v2->xyz, distance) ? lerpfrac : 1;

			if (!outline) {
				GLC_AliasModelLightPointMD3(vertexColor, ent, v1, v2, lerpfrac);
				R_CustomColor(vertexColor[0], vertexColor[1], vertexColor[2], vertexColor[3]);
			}

			VectorInterpolate(v1->xyz, lerpfrac, v2->xyz, interpolated_verts);
			glTexCoord2f(s, t);
			GLC_Vertex3fv(interpolated_verts);

			tris++;
		}
		GLC_End();

		frameStats.classic.polycount[polyTypeAliasModel] += surf->numTriangles;
	}
}

static void GLC_DrawAlias3ModelProgram(entity_t* ent, int frame1, qbool invalidate_texture, float* vertexColor, float lerpfrac, qbool outline)
{
	model_t *mod = ent->model;
	md3model_t *mhead = (md3model_t *)Mod_Extradata(mod);
	md3Header_t *pheader = MD3_HeaderForModel(mhead);
	float angle_radians = -ent->angles[YAW] * M_PI / 180.0;
	vec3_t angle_vector = { cos(angle_radians), sin(angle_radians), 1 };
	int vertsPerFrame = mod->vertsInVBO / pheader->numFrames;
	int first_vert = mod->vbo_start + vertsPerFrame * frame1;
	int vert_index;
	surfinf_t* sinf = MD3_ExtraSurfaceInfoForModel(mhead);
	md3Surface_t* surf;
	int surfnum;

	if (ent->skinnum >= 0 && ent->skinnum < pheader->numSkins) {
		sinf += ent->skinnum * pheader->numSurfaces;
	}

	// Temporarily disable caustics
	R_ProgramUse(r_program_aliasmodel_std_glc);
	R_ProgramUniform3fv(r_program_uniform_aliasmodel_std_glc_angleVector, angle_vector);
	R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_shadelight, ent->shadelight / 256.0f);
	R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_ambientlight, ent->ambientlight / 256.0f);
	R_ProgramUniform1i(r_program_uniform_aliasmodel_std_glc_fsTextureEnabled, invalidate_texture ? 0 : 1);
	R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_fsMinLumaMix, 1.0f - (ent->full_light ? bound(0, gl_fb_models.integer, 1) : 0));
	R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_fsCausticEffects, 0 /*ent->renderfx & RF_CAUSTICS ? 1 : 0*/);
	R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_lerpFraction, lerpfrac);
	R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_time, cl.time);

	GLC_StateBeginDrawAliasFrameProgram(sinf->texnum, null_texture_reference, ent->renderfx, ent->custom_model, ent->r_modelalpha);
	R_CustomColor(vertexColor[0], vertexColor[1], vertexColor[2], vertexColor[3]);
	vert_index = first_vert;
	MD3_ForEachSurface(pheader, surf, surfnum) {
		if (R_TextureReferenceIsValid(sinf[surfnum].texnum) && !invalidate_texture) {
			renderer.TextureUnitBind(0, sinf[surfnum].texnum);
		}

		GL_DrawArrays(GL_TRIANGLES, vert_index, 3 * surf->numTriangles);
		vert_index += 3 * surf->numTriangles;
	}
	if (outline) {
		if (ent->renderfx & RF_CAUSTICS) {
			R_ProgramUniform1f(r_program_uniform_aliasmodel_std_glc_fsCausticEffects, 0);
		}
		GLC_StateBeginAliasOutlineFrame();
		vert_index = first_vert;
		MD3_ForEachSurface(pheader, surf, surfnum)
		{
			GL_DrawArrays(GL_TRIANGLES, vert_index, 3 * surf->numTriangles);
			vert_index += 3 * surf->numTriangles;
		}
	}
	R_ProgramUse(r_program_none);
}

static void GLC_DrawAlias3ModelImmediate(entity_t* ent, int frame1, int frame2, qbool invalidate_texture, float* vertexColor, float lerpfrac, qbool outline)
{
	model_t* mod = ent->model;
	md3model_t* mhead = (md3model_t *)Mod_Extradata(mod);
	md3Header_t* pheader = MD3_HeaderForModel(mhead);
	surfinf_t* sinf = MD3_ExtraSurfaceInfoForModel(mhead);

	// Immediate mode
	R_ProgramUse(r_program_none);
	GLC_StateBeginMD3Draw(ent->r_modelalpha, R_TextureReferenceIsValid(sinf->texnum) && !invalidate_texture, ent->renderfx & RF_WEAPONMODEL);
	GLC_DrawMD3Frame(ent, pheader, frame1, frame2, lerpfrac, sinf, invalidate_texture, false);

	if (outline) {
		GLC_StateBeginAliasOutlineFrame();
		GLC_DrawMD3Frame(ent, pheader, frame1, frame2, lerpfrac, sinf, true, true);
	}
}

/*
To draw, for each surface, run through the triangles, getting tex coords from s+t, 
*/
void GLC_DrawAlias3Model(entity_t *ent)
{
	extern cvar_t gl_fb_models, gl_program_aliasmodels;

	float lerpfrac;
	float vertexColor[4];

	model_t *mod = ent->model;
	md3model_t *mhead = (md3model_t *)Mod_Extradata(mod);
	md3Header_t *pheader = MD3_HeaderForModel(mhead);
	qbool invalidate_texture = false;

	int frame1, frame2;
	qbool outline;
	float oldMatrix[16];
	int subprogram;
	extern cvar_t r_lerpmuzzlehack;
	int render_effects = ent->renderfx;

	R_PushModelviewMatrix(oldMatrix);
	R_AliasModelPrepare(ent, pheader->numFrames, &frame1, &frame2, &lerpfrac, &outline);
	R_AliasModelColor(ent, vertexColor, &invalidate_texture);

	subprogram = GLC_AliasModelSubProgramIndex(!invalidate_texture, ent->full_light, gl_caustics.integer && (ent->renderfx & RF_CAUSTICS), r_lerpmuzzlehack.integer && (render_effects & RF_WEAPONMODEL));

	if (gl_program_aliasmodels.integer && buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_AliasModelStandardCompileSpecific(subprogram)) {
		GLC_DrawAlias3ModelProgram(ent, frame1, invalidate_texture, vertexColor, lerpfrac, outline);
	}
	else if (GL_Supported(R_SUPPORT_IMMEDIATEMODE)) {
		GLC_DrawAlias3ModelImmediate(ent, frame1, frame2, invalidate_texture, vertexColor, lerpfrac, outline);
	}
	R_PopModelviewMatrix(oldMatrix);
}

static void GLC_DrawAlias3ModelPowerupShellProgram(model_t * mod, md3Header_t * pheader, int frame1, entity_t * ent, float lerpfrac)
{
	int vertsPerFrame = mod->vertsInVBO / pheader->numFrames;
	int first_vert = mod->vbo_start + vertsPerFrame * frame1;
	float color1[4], color2[4];

	GLC_PowerupShellColor(0, ent->effects, color1);
	GLC_PowerupShellColor(1, ent->effects, color2);

	R_ProgramUse(r_program_aliasmodel_shell_glc);
	R_ProgramUniform4fv(r_program_uniform_aliasmodel_shell_glc_fsBaseColor1, color1);
	R_ProgramUniform4fv(r_program_uniform_aliasmodel_shell_glc_fsBaseColor2, color2);
	R_ProgramUniform4fv(r_program_uniform_aliasmodel_shell_glc_scroll, GLC_PowerupShell_ScrollParams());
	R_ProgramUniform1f(r_program_uniform_aliasmodel_shell_glc_lerpFraction, lerpfrac);
	GL_DrawArrays(GL_TRIANGLES, first_vert, vertsPerFrame);
	R_ProgramUse(r_program_none);
}

static void GLC_DrawAlias3ModelPowerupShellImmediate(model_t * mod, md3Header_t * pheader, int frame1, int frame2, entity_t * ent, float lerpfrac)
{
	float scroll[4];
	int layer_no;
	const int distance = MD3_INTERP_MAXDIST / MD3_XYZ_SCALE;

	scroll[0] = cos(cl.time * 1.5);
	scroll[1] = sin(cl.time * 1.1);
	scroll[2] = cos(cl.time * -0.5);
	scroll[3] = sin(cl.time * -0.5);

	R_ProgramUse(r_program_none);

	for (layer_no = 0; layer_no <= 1; ++layer_no) {
		md3Surface_t* surf;
		int surfnum;
		 
		MD3_ForEachSurface(pheader, surf, surfnum) {
			// loop through the surfaces.
			int pose1 = frame1 * surf->numVerts;
			int pose2 = frame2 * surf->numVerts;
			int numtris, i;
			unsigned int* tris;
			md3St_t* tc;
			ezMd3XyzNormal_t* verts;

			//skin texture coords.
			tc = MD3_SurfaceTextureCoords(surf);
			verts = MD3_SurfaceVertices(surf);

			tris = (unsigned int *)((char *)surf + surf->ofsTriangles);
			numtris = surf->numTriangles * 3;

			GLC_SetPowerupShellColor(layer_no, ent->effects);
			GLC_Begin(GL_TRIANGLES);
			for (i = 0; i < numtris; i++) {
				float s, t;
				vec3_t vec1pos, vec2pos, interpolated_verts;
				ezMd3XyzNormal_t *v1, *v2;

				v1 = verts + *tris + pose1;
				v2 = verts + *tris + pose2;

				s = tc[*tris].s * 2.0f + scroll[layer_no * 2];
				t = tc[*tris].t * 2.0f + scroll[layer_no * 2 + 1];

				lerpfrac = VectorL2Compare(v1->xyz, v2->xyz, distance) ? lerpfrac : 1;
				VectorAdd(v1->normal, v1->xyz, vec1pos);
				VectorAdd(v2->normal, v2->xyz, vec2pos);
				VectorInterpolate(vec1pos, lerpfrac, vec2pos, interpolated_verts);

				glTexCoord2f(s, t);
				GLC_Vertex3fv(interpolated_verts);

				tris++;
			}
			GLC_End();
		}
	}
}

/*
To draw, for each surface, run through the triangles, getting tex coords from s+t,
*/
void GLC_DrawAlias3ModelPowerupShell(entity_t *ent)
{
	extern cvar_t gl_fb_models, gl_program_aliasmodels;
	model_t *mod = ent->model;
	md3model_t *mhead = (md3model_t *)Mod_Extradata(mod);
	md3Header_t *pheader = MD3_HeaderForModel(mhead);
	float oldMatrix[16];
	int frame1, frame2;
	float lerpfrac;
	qbool outline;

	R_PushModelviewMatrix(oldMatrix);
	R_AliasModelPrepare(ent, pheader->numFrames, &frame1, &frame2, &lerpfrac, &outline);

	GLC_StateBeginAliasPowerupShell(ent->renderfx & RF_WEAPONMODEL);
	if (gl_program_aliasmodels.integer && buffers.supported && GL_Supported(R_SUPPORT_RENDERING_SHADERS) && GLC_AliasModelShellCompile()) {
		GLC_DrawAlias3ModelPowerupShellProgram(mod, pheader, frame1, ent, lerpfrac);
	}
	else if (GL_Supported(R_SUPPORT_IMMEDIATEMODE)) {
		GLC_DrawAlias3ModelPowerupShellImmediate(mod, pheader, frame1, frame2, ent, lerpfrac);
	}

	R_PopModelviewMatrix(oldMatrix);
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
