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

// gl_state_resets_entities.c
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "gl_aliasmodel.h"
#include "r_matrix.h"
#include "r_state.h"
#include "r_vao.h"

#include "gl_aliasmodel.h" // for shelltexture only

#define ALIASMODEL_NOTEXTURE_OPAQUE            0
#define ALIASMODEL_NOTEXTURE_TRANSPARENT       1
#define ALIASMODEL_SINGLETEXTURE_OPAQUE        2
#define ALIASMODEL_SINGLETEXTURE_TRANSPARENT   3
#define ALIASMODEL_MULTITEXTURE_OPAQUE         4
#define ALIASMODEL_MULTITEXTURE_TRANSPARENT    5
#define WEAPONMODEL_SINGLETEXTURE_OPAQUE       6
#define WEAPONMODEL_SINGLETEXTURE_TRANSPARENT  7
#define WEAPONMODEL_MULTITEXTURE_OPAQUE        8
#define WEAPONMODEL_MULTITEXTURE_TRANSPARENT   9

static rendering_state_t powerupShellState;
static rendering_state_t aliasModelState[10];
static rendering_state_t aliasModelShadowState;
static rendering_state_t aliasModelOutlineState;
static rendering_state_t brushModelOpaqueState;
static rendering_state_t brushModelOpaqueOffsetState;
static rendering_state_t brushModelTranslucentState;
static rendering_state_t brushModelTranslucentOffsetState;
static rendering_state_t aliasModelBatchState;
static rendering_state_t aliasModelTranslucentBatchState;

void R_InitialiseEntityStates(void)
{
	extern cvar_t gl_outline_width;
	int i;

	R_InitRenderingState(&powerupShellState, true, "powerupShellState", vao_aliasmodel);
	powerupShellState.polygonOffset.option = r_polygonoffset_disabled;
	powerupShellState.cullface.enabled = true;
	powerupShellState.cullface.mode = r_cullface_front;
	powerupShellState.polygonMode = r_polygonmode_fill;
	powerupShellState.line.smooth = false;
	powerupShellState.fog.enabled = false;
	powerupShellState.alphaTesting.enabled = false;
	powerupShellState.blendingEnabled = true;
	powerupShellState.blendFunc = r_blendfunc_additive_blending;
	powerupShellState.textureUnits[0].enabled = true;
	powerupShellState.textureUnits[0].mode = r_texunit_mode_modulate;

	R_InitRenderingState(&aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE], true, "opaqueAliasModelNoTexture", vao_aliasmodel);
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].blendFunc = r_blendfunc_premultiplied_alpha;
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].blendingEnabled = aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].alphaTesting.enabled = false;
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].polygonOffset.option = r_polygonoffset_disabled;
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].cullface.enabled = true;
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].cullface.mode = r_cullface_front;
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].polygonMode = r_polygonmode_fill;
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].line.smooth = false;
	aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE].fog.enabled = true;

	R_CopyRenderingState(&aliasModelState[ALIASMODEL_SINGLETEXTURE_OPAQUE], &aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE], "opaqueAliasModelSingleTex");
	aliasModelState[ALIASMODEL_SINGLETEXTURE_OPAQUE].textureUnits[0].enabled = true;
	aliasModelState[ALIASMODEL_SINGLETEXTURE_OPAQUE].textureUnits[0].mode = r_texunit_mode_modulate;

	R_CopyRenderingState(&aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE], &aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE], "opaqueAliasModelMultiTex");
	aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE].textureUnits[0].enabled = true;
	aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE].textureUnits[0].mode = r_texunit_mode_modulate;
	aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE].textureUnits[1].enabled = true;
	aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE].textureUnits[1].mode = r_texunit_mode_decal;

	R_CopyRenderingState(&aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE + 1], &aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE], "transparentAliasModelNoTex");
	R_CopyRenderingState(&aliasModelState[ALIASMODEL_SINGLETEXTURE_OPAQUE + 1], &aliasModelState[ALIASMODEL_SINGLETEXTURE_OPAQUE], "transparentAliasModelSingleTex");
	R_CopyRenderingState(&aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE + 1], &aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE], "transparentAliasModelMultiTex");

	for (i = ALIASMODEL_NOTEXTURE_OPAQUE; i <= ALIASMODEL_MULTITEXTURE_OPAQUE; i += 2) {
		aliasModelState[i + 1].blendingEnabled = true;
		aliasModelState[i + 1].blendFunc = r_blendfunc_premultiplied_alpha;
	}

	R_CopyRenderingState(&aliasModelState[WEAPONMODEL_SINGLETEXTURE_OPAQUE], &aliasModelState[ALIASMODEL_SINGLETEXTURE_OPAQUE], "weaponModelSingleOpaque");
	R_CopyRenderingState(&aliasModelState[WEAPONMODEL_SINGLETEXTURE_TRANSPARENT], &aliasModelState[ALIASMODEL_SINGLETEXTURE_TRANSPARENT], "weaponModelSingleTransparent");
	R_CopyRenderingState(&aliasModelState[WEAPONMODEL_MULTITEXTURE_OPAQUE], &aliasModelState[ALIASMODEL_MULTITEXTURE_OPAQUE], "weaponModelMultiOpaque");
	R_CopyRenderingState(&aliasModelState[WEAPONMODEL_MULTITEXTURE_TRANSPARENT], &aliasModelState[ALIASMODEL_MULTITEXTURE_TRANSPARENT], "weaponModelMultiOpaque");
	for (i = WEAPONMODEL_SINGLETEXTURE_OPAQUE; i <= WEAPONMODEL_MULTITEXTURE_TRANSPARENT; ++i) {
		aliasModelState[i].depth.farRange = 0.3f;
	}

	R_InitRenderingState(&aliasModelShadowState, true, "aliasModelShadowState", vao_aliasmodel);
	aliasModelShadowState.polygonOffset.option = r_polygonoffset_disabled;
	aliasModelShadowState.cullface.enabled = true;
	aliasModelShadowState.cullface.mode = r_cullface_front;
	aliasModelShadowState.polygonMode = r_polygonmode_fill;
	aliasModelShadowState.line.smooth = false;
	aliasModelShadowState.fog.enabled = false;
	aliasModelShadowState.alphaTesting.enabled = false;
	aliasModelShadowState.blendingEnabled = true;
	aliasModelShadowState.blendFunc = r_blendfunc_premultiplied_alpha;
	aliasModelShadowState.color[0] = aliasModelShadowState.color[1] = aliasModelShadowState.color[2] = 0;
	aliasModelShadowState.color[3] = 0.5f;

	R_InitRenderingState(&aliasModelOutlineState, true, "aliasModelOutlineState", vao_aliasmodel);
	aliasModelOutlineState.alphaTesting.enabled = false;
	aliasModelOutlineState.blendingEnabled = false;
	aliasModelOutlineState.fog.enabled = false;
	aliasModelOutlineState.polygonOffset.option = r_polygonoffset_outlines;
	aliasModelOutlineState.cullface.enabled = true;
	aliasModelOutlineState.cullface.mode = r_cullface_back;
	aliasModelOutlineState.polygonMode = r_polygonmode_line;
	// limit outline width, since even width == 3 can be considered as cheat.
	aliasModelOutlineState.line.width = bound(0.1, gl_outline_width.value, 3.0);
	aliasModelOutlineState.line.flexible_width = true;
	aliasModelOutlineState.line.smooth = true;
	aliasModelOutlineState.color[0] = aliasModelOutlineState.color[1] = aliasModelOutlineState.color[2] = 0;

	R_InitRenderingState(&brushModelOpaqueState, true, "brushModelOpaqueState", vao_brushmodel);
	brushModelOpaqueState.cullface.mode = r_cullface_front;
	brushModelOpaqueState.cullface.enabled = true;
	brushModelOpaqueState.polygonMode = r_polygonmode_fill;
	brushModelOpaqueState.alphaTesting.enabled = false;
	brushModelOpaqueState.blendingEnabled = false;
	brushModelOpaqueState.line.smooth = false;
	brushModelOpaqueState.fog.enabled = false;
	brushModelOpaqueState.polygonOffset.option = r_polygonoffset_disabled;

	R_CopyRenderingState(&brushModelOpaqueOffsetState, &brushModelOpaqueState, "brushModelOpaqueOffsetState");
	brushModelOpaqueOffsetState.polygonOffset.option = r_polygonoffset_standard;

	R_CopyRenderingState(&brushModelTranslucentState, &brushModelOpaqueState, "brushModelTranslucentState");
	brushModelTranslucentState.blendingEnabled = true;
	brushModelTranslucentState.blendFunc = r_blendfunc_premultiplied_alpha;

	R_CopyRenderingState(&brushModelTranslucentOffsetState, &brushModelTranslucentState, "brushModelTranslucentOffsetState");
	brushModelTranslucentOffsetState.polygonOffset.option = r_polygonoffset_standard;

	R_InitRenderingState(&aliasModelBatchState, true, "aliasModelBatchState", vao_aliasmodel);
	aliasModelBatchState.cullface.mode = r_cullface_front;
	aliasModelBatchState.cullface.enabled = true;
	aliasModelBatchState.depth.mask_enabled = true;
	aliasModelBatchState.depth.test_enabled = true;
	aliasModelBatchState.blendingEnabled = false;

	R_CopyRenderingState(&aliasModelTranslucentBatchState, &aliasModelBatchState, "aliasModelTranslucentBatchState");
	aliasModelTranslucentBatchState.blendFunc = r_blendfunc_premultiplied_alpha;
	aliasModelTranslucentBatchState.blendingEnabled = true;
}

void GLC_StateBeginAliasPowerupShell(void)
{
	R_ApplyRenderingState(&powerupShellState);
	R_TextureUnitBind(0, shelltexture);
}

void GLC_StateBeginMD3Draw(float alpha, qbool textured)
{
	if (textured) {
		if (alpha < 1) {
			R_ApplyRenderingState(&aliasModelState[ALIASMODEL_SINGLETEXTURE_TRANSPARENT]);
		}
		else {
			R_ApplyRenderingState(&aliasModelState[ALIASMODEL_SINGLETEXTURE_OPAQUE]);
		}
	}
	else {
		if (alpha < 1) {
			R_ApplyRenderingState(&aliasModelState[ALIASMODEL_NOTEXTURE_TRANSPARENT]);
		}
		else {
			R_ApplyRenderingState(&aliasModelState[ALIASMODEL_NOTEXTURE_OPAQUE]);
		}
	}
}

void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, qbool alpha_blend, struct custom_model_color_s* custom_model, qbool weapon_model)
{
	int index;

	ENTER_STATE;

	if (!weapon_model && (!GL_TextureReferenceIsValid(texture) || (custom_model && custom_model->fullbright_cvar.integer))) {
		index = ALIASMODEL_NOTEXTURE_OPAQUE;
	}
	else if (custom_model == NULL && GL_TextureReferenceIsValid(fb_texture) && mtex) {
		index = weapon_model ? WEAPONMODEL_MULTITEXTURE_OPAQUE : ALIASMODEL_MULTITEXTURE_OPAQUE;
		R_TextureUnitBind(0, texture);
		R_TextureUnitBind(1, fb_texture);
	}
	else {
		index = weapon_model ? WEAPONMODEL_SINGLETEXTURE_OPAQUE : ALIASMODEL_SINGLETEXTURE_OPAQUE;
		R_TextureUnitBind(0, texture);
	}

	if (alpha_blend) {
		++index;
	}
	R_ApplyRenderingState(&aliasModelState[index]);

	LEAVE_STATE;
}

void GLC_StateBeginAliasModelShadow(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&aliasModelShadowState);

	LEAVE_STATE;
}

void GL_StateBeginDrawBrushModel(entity_t* e, qbool polygonOffset)
{
	static rendering_state_t* brushModelStates[] = {
		&brushModelOpaqueState,
		&brushModelOpaqueOffsetState,
		&brushModelTranslucentState,
		&brushModelTranslucentOffsetState,
	};

	GL_EnterTracedRegion("GL_StateBeginDrawBrushModel", true);

	GLC_PauseMatrixUpdate();
	GL_TranslateModelview(e->origin[0],  e->origin[1],  e->origin[2]);
	GL_RotateModelview(e->angles[1], 0, 0, 1);
	GL_RotateModelview(e->angles[0], 0, 1, 0);
	GL_RotateModelview(e->angles[2], 1, 0, 0);
	GLC_LoadMatrix(GL_MODELVIEW);
	GLC_ResumeMatrixUpdate();

	R_ApplyRenderingState(brushModelStates[(e->alpha ? 2 : 0) + (polygonOffset ? 1 : 0)]);

	if (GL_UseImmediateMode()) {
		if (e->alpha) {
			R_CustomColor(e->alpha, e->alpha, e->alpha, e->alpha);
		}
	}

	LEAVE_STATE;
}

void GL_StateBeginDrawAliasModel(entity_t* ent, aliashdr_t* paliashdr)
{
	extern cvar_t r_viewmodelsize;

	GL_EnterTracedRegion(__FUNCTION__, true);

	GLC_PauseMatrixUpdate();
	R_RotateForEntity(ent);
	if (ent->model->modhint == MOD_EYES) {
		GL_TranslateModelview(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		GL_ScaleModelview(paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else if (ent->renderfx & RF_WEAPONMODEL) {
		float scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;

		GL_TranslateModelview(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_ScaleModelview(paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	}
	else {
		GL_TranslateModelview(paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_ScaleModelview(paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}
	GLC_ResumeMatrixUpdate();
	GLC_LoadMatrix(GL_MODELVIEW);

	LEAVE_STATE;
}

void GLC_StateBeginAliasOutlineFrame(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&aliasModelOutlineState);

	LEAVE_STATE;
}

void GLM_StateBeginAliasOutlineBatch(void)
{
	R_ApplyRenderingState(&aliasModelOutlineState);
}

void GLM_StateBeginAliasModelBatch(qbool translucent)
{
	if (translucent) {
		R_ApplyRenderingState(&aliasModelTranslucentBatchState);
	}
	else {
		R_ApplyRenderingState(&aliasModelBatchState);
	}
}
