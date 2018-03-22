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

static rendering_state_t powerupShellState;
static rendering_state_t aliasModelOpaqueState;
static rendering_state_t aliasModelTranslucentState;
static rendering_state_t aliasModelShadowState;
static rendering_state_t viewModelOpaqueState;
static rendering_state_t viewModelTranslucentState;
static rendering_state_t simpleItemState;
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

	R_InitRenderingState(&powerupShellState, true);
	powerupShellState.polygonOffset.option = r_polygonoffset_disabled;
	powerupShellState.cullface.enabled = true;
	powerupShellState.cullface.mode = r_cullface_front;
	powerupShellState.polygonMode = r_polygonmode_fill;
	powerupShellState.line.smooth = false;
	powerupShellState.fog.enabled = false;
	powerupShellState.alphaTestingEnabled = false;
	powerupShellState.blendingEnabled = true;
	powerupShellState.blendFunc = r_blendfunc_additive_blending;
	//GLC_InitTextureUnits1(shelltexture, GL_MODULATE);

	R_InitRenderingState(&aliasModelOpaqueState, true);
	aliasModelOpaqueState.blendFunc = r_blendfunc_premultiplied_alpha;
	aliasModelOpaqueState.blendingEnabled = aliasModelOpaqueState.alphaTestingEnabled = false;
	aliasModelOpaqueState.polygonOffset.option = r_polygonoffset_disabled;
	aliasModelOpaqueState.cullface.enabled = true;
	aliasModelOpaqueState.cullface.mode = r_cullface_front;
	aliasModelOpaqueState.polygonMode = r_polygonmode_fill;
	aliasModelOpaqueState.line.smooth = false;
	aliasModelOpaqueState.fog.enabled = true;
	//GLC_InitTextureUnitsNoBind1(GL_MODULATE);

	R_InitRenderingState(&aliasModelTranslucentState, true);
	memcpy(&aliasModelTranslucentState, &aliasModelOpaqueState, sizeof(aliasModelTranslucentState));
	aliasModelTranslucentState.blendingEnabled = true;

	R_InitRenderingState(&aliasModelShadowState, true);
	aliasModelShadowState.polygonOffset.option = r_polygonoffset_disabled;
	aliasModelShadowState.cullface.enabled = true;
	aliasModelShadowState.cullface.mode = r_cullface_front;
	aliasModelShadowState.polygonMode = r_polygonmode_fill;
	aliasModelShadowState.line.smooth = false;
	aliasModelShadowState.fog.enabled = false;
	aliasModelShadowState.alphaTestingEnabled = false;
	aliasModelShadowState.blendingEnabled = true;
	aliasModelShadowState.blendFunc = r_blendfunc_premultiplied_alpha;
	//GLC_DisableAllTexturing();

	R_InitRenderingState(&viewModelOpaqueState, true);
	viewModelOpaqueState.polygonOffset.option = r_polygonoffset_disabled;
	viewModelOpaqueState.cullface.mode = r_cullface_front;
	viewModelOpaqueState.cullface.enabled = true;
	viewModelOpaqueState.polygonMode = r_polygonmode_fill;
	viewModelOpaqueState.line.smooth = false;
	viewModelOpaqueState.fog.enabled = false;
	viewModelOpaqueState.depth.mask_enabled = true;
	viewModelOpaqueState.depth.test_enabled = true;
	viewModelOpaqueState.depth.nearRange = 0;   // gldepthmin
	viewModelOpaqueState.depth.farRange = 0.3; // gldepthmin + 0.3 * (gldepthmax - gldepthmin)
	viewModelOpaqueState.alphaTestingEnabled = false;
	viewModelOpaqueState.blendingEnabled = false;
	viewModelOpaqueState.blendFunc = r_blendfunc_premultiplied_alpha;

	memcpy(&viewModelTranslucentState, &viewModelOpaqueState, sizeof(viewModelTranslucentState));
	viewModelOpaqueState.blendingEnabled = true;

	R_InitRenderingState(&simpleItemState, true);
	simpleItemState.polygonOffset.option = r_polygonoffset_disabled;
	simpleItemState.cullface.enabled = true;
	simpleItemState.cullface.mode = r_cullface_front;
	simpleItemState.polygonMode = r_polygonmode_fill;
	simpleItemState.line.smooth = false;
	simpleItemState.fog.enabled = false;
	simpleItemState.alphaTestingEnabled = true;
	simpleItemState.blendingEnabled = true;
	simpleItemState.blendFunc = r_blendfunc_premultiplied_alpha;

	R_InitRenderingState(&aliasModelOutlineState, true);
	aliasModelOutlineState.alphaTestingEnabled = false;
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

	R_InitRenderingState(&brushModelOpaqueState, true);
	brushModelOpaqueState.cullface.mode = r_cullface_front;
	brushModelOpaqueState.cullface.enabled = true;
	brushModelOpaqueState.polygonMode = r_polygonmode_fill;
	brushModelOpaqueState.alphaTestingEnabled = false;
	brushModelOpaqueState.blendingEnabled = false;
	brushModelOpaqueState.line.smooth = false;
	brushModelOpaqueState.fog.enabled = false;
	brushModelOpaqueState.polygonOffset.option = r_polygonoffset_disabled;

	memcpy(&brushModelOpaqueOffsetState, &brushModelOpaqueState, sizeof(brushModelTranslucentState));
	brushModelOpaqueOffsetState.polygonOffset.option = r_polygonoffset_standard;

	memcpy(&brushModelTranslucentState, &brushModelOpaqueState, sizeof(brushModelTranslucentState));
	brushModelTranslucentState.blendingEnabled = true;
	brushModelTranslucentState.blendFunc = r_blendfunc_premultiplied_alpha;

	memcpy(&brushModelTranslucentOffsetState, &brushModelTranslucentState, sizeof(brushModelTranslucentState));
	brushModelTranslucentOffsetState.polygonOffset.option = r_polygonoffset_standard;

	R_InitRenderingState(&aliasModelBatchState, true);
	aliasModelBatchState.cullface.mode = r_cullface_front;
	aliasModelBatchState.cullface.enabled = true;
	aliasModelBatchState.depth.mask_enabled = true;
	aliasModelBatchState.depth.test_enabled = true;
	aliasModelBatchState.blendingEnabled = false;

	memcpy(&aliasModelTranslucentBatchState, &aliasModelBatchState, sizeof(aliasModelTranslucentBatchState));
	aliasModelTranslucentBatchState.blendFunc = r_blendfunc_premultiplied_alpha;
	aliasModelTranslucentBatchState.blendingEnabled = true;
}

void GL_StateBeginEntities(visentlist_t* vislist)
{
}

void GL_StateEndEntities(visentlist_t* vislist)
{
}

void GLC_StateBeginAliasPowerupShell(void)
{
	R_ApplyRenderingState(&powerupShellState);
	glColor3ubv(color_white);
}

void GLC_StateEndAliasPowerupShell(void)
{
}

void GLC_StateBeginMD3Draw(float alpha)
{
	if (alpha < 1) {
		R_ApplyRenderingState(&aliasModelTranslucentState);
	}
	else {
		R_ApplyRenderingState(&aliasModelOpaqueState);
	}
	glColor3ubv(color_white);
}

void GLC_StateEndMD3Draw(void)
{
}

void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, qbool alpha_blend, struct custom_model_color_s* custom_model)
{
	ENTER_STATE;

	if (alpha_blend) {
		R_ApplyRenderingState(&aliasModelTranslucentState);
	}
	else {
		R_ApplyRenderingState(&aliasModelOpaqueState);
	}
	glColor3ubv(color_white);

	if (!GL_TextureReferenceIsValid(texture)) {
		GLC_DisableAllTexturing();
	}
	else if (custom_model && custom_model->fullbright_cvar.integer) {
		GLC_DisableAllTexturing();
	}
	else if (custom_model) {
		GLC_InitTextureUnits1(texture, GL_MODULATE);
	}
	else if (GL_TextureReferenceIsValid(fb_texture) && mtex) {
		GLC_InitTextureUnits2(texture, GL_MODULATE, fb_texture, GL_DECAL);
	}
	else {
		GLC_InitTextureUnits1(texture, GL_MODULATE);
	}

	LEAVE_STATE;
}

void GLC_StateEndDrawAliasFrame(void)
{
}

void GLC_StateBeginAliasModelShadow(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&aliasModelShadowState);
	GLC_DisableAllTexturing();
	glColor4f(0, 0, 0, 0.5);

	LEAVE_STATE;
}

void GLC_StateEndAliasModelShadow(void)
{
}

void GLC_StateBeginDrawViewModel(float alpha)
{
	ENTER_STATE;

	// hack the depth range to prevent view model from poking into walls
	if (gl_mtexable && alpha < 1) {
		R_ApplyRenderingState(&viewModelTranslucentState);
	}
	else {
		R_ApplyRenderingState(&viewModelOpaqueState);
	}
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GLC_EnsureTMUEnabled(GL_TEXTURE0);

	LEAVE_STATE;
}

void GLC_StateEndDrawViewModel(void)
{
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
			glColor4f(e->alpha, e->alpha, e->alpha, e->alpha);
		}
		else {
			glColor3ubv(color_white);
		}
	}

	LEAVE_STATE;
}

void GL_StateEndDrawBrushModel(void)
{
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

void GL_StateEndDrawAliasModel(void)
{
}

void GLC_StateBeginSimpleItem(texture_ref simpletexture)
{
	R_ApplyRenderingState(&simpleItemState);

	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	GLC_InitTextureUnits1(simpletexture, GL_REPLACE);
	glColor3ubv(color_white);
}

void GLC_StateEndSimpleItem(void)
{
}

void GLC_StateBeginAliasOutlineFrame(void)
{
	ENTER_STATE;

	R_ApplyRenderingState(&aliasModelOutlineState);

	if (GL_UseImmediateMode()) {
		glColor3ubv(color_black);
		GLC_DisableAllTexturing();
	}

	LEAVE_STATE;
}

void GLC_StateEndAliasOutlineFrame(void)
{
}

void GLM_StateBeginAliasOutlineBatch(void)
{
	R_ApplyRenderingState(&aliasModelOutlineState);
}

void GLM_StateEndAliasOutlineBatch(void)
{
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
