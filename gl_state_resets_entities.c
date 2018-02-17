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

void GL_StateBeginEntities(visentlist_t* vislist)
{
}

void GL_StateEndEntities(visentlist_t* vislist)
{
	ENTER_STATE;

	GL_PolygonMode(GL_FILL);
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	if (!GL_ShadersSupported()) {
		if (gl_affinemodels.value) {
			GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
		}
		if (gl_smoothmodels.value) {
			GL_ShadeModel(GL_FLAT);
		}
	}

	LEAVE_STATE;
}

void GLC_StateBeginAliasPowerupShell(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_DisableFog();

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GLC_InitTextureUnits1(shelltexture, GL_MODULATE);
	if (gl_powerupshells_style.integer) {
		GL_BlendFunc(GL_SRC_ALPHA, GL_ONE);
	}
	else {
		GL_BlendFunc(GL_ONE, GL_ONE);
	}

	LEAVE_STATE;
}

void GLC_StateEndAliasPowerupShell(void)
{
}

void GLC_StateBeginMD3Draw(float alpha)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_AlphaBlendFlags(alpha < 1 ? GL_BLEND_ENABLED : GL_BLEND_DISABLED);
	GL_EnableFog();
	GLC_InitTextureUnitsNoBind1(GL_MODULATE);

	LEAVE_STATE;
}

void GLC_StateEndMD3Draw(void)
{
}

void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, float alpha, struct custom_model_color_s* custom_model)
{
	ENTER_STATE;

	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	GL_Disable(GL_LINE_SMOOTH);
	GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	GL_DisableFog();

	GL_AlphaBlendFlags(alpha < 1 ? GL_BLEND_ENABLED : GL_BLEND_DISABLED);

	if (custom_model) {
		GL_Color4ub(custom_model->color_cvar.color[0], custom_model->color_cvar.color[1], custom_model->color_cvar.color[2], alpha * 255);
		if (custom_model->fullbright_cvar.integer || !GL_TextureReferenceIsValid(texture)) {
			GLC_DisableAllTexturing();
		}
		else {
			GLC_InitTextureUnits1(texture, GL_MODULATE);
		}
	}
	else {
		GL_Color3ubv(color_white);
		if (GL_TextureReferenceIsValid(texture) && GL_TextureReferenceIsValid(fb_texture) && mtex) {
			GLC_InitTextureUnits2(texture, GL_MODULATE, fb_texture, GL_DECAL);
		}
		else if (GL_TextureReferenceIsValid(texture)) {
			GLC_InitTextureUnits1(texture, GL_MODULATE);
		}
		else {
			GLC_DisableAllTexturing();
		}
	}

	LEAVE_STATE;
}

void GLC_StateEndDrawAliasFrame(void)
{
}

void GLC_StateBeginAliasModelShadow(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_DisableFog();

	GLC_DisableAllTexturing();
	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	glColor4f(0, 0, 0, 0.5);

	LEAVE_STATE;
}

void GLC_StateEndAliasModelShadow(void)
{
}

void GL_StateBeginDrawViewModel(float alpha)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_DisableFog();

	// hack the depth range to prevent view model from poking into walls
	GL_DepthRange(gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	if (gl_mtexable) {
		if (alpha < 1) {
			GL_AlphaBlendFlags(GL_BLEND_ENABLED);
			GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
		}
		else {
			GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		}
	}
	if (gl_affinemodels.value) {
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	}

	LEAVE_STATE;
}

void GL_StateEndDrawViewModel(void)
{
	GL_DepthRange(gldepthmin, gldepthmax);
	GL_PolygonMode(GL_FILL);
}

void GL_StateBeginDrawBrushModel(entity_t* e, qbool polygonOffset)
{
	GL_EnterTracedRegion(va("GL_StateBeginDrawBrushModel(%s)", e->model->name), true);

	GLC_PauseMatrixUpdate();
	GL_Translate(GL_MODELVIEW, e->origin[0],  e->origin[1],  e->origin[2]);
	GL_Rotate(GL_MODELVIEW, e->angles[1], 0, 0, 1);
	GL_Rotate(GL_MODELVIEW, e->angles[0], 0, 1, 0);
	GL_Rotate(GL_MODELVIEW, e->angles[2], 1, 0, 0);
	GLC_LoadMatrix(GL_MODELVIEW);
	GLC_ResumeMatrixUpdate();

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_DisableFog();

	if (e->alpha) {
		GL_AlphaBlendFlags(GL_BLEND_ENABLED);
		GLC_InitTextureUnitsNoBind1(GL_MODULATE);
		glColor4f(1, 1, 1, e->alpha);
	}
	else {
		GL_AlphaBlendFlags(GL_BLEND_DISABLED);
		GLC_InitTextureUnitsNoBind1(GL_REPLACE);
		GL_Color3ubv(color_white);
	}

	GL_PolygonOffset(polygonOffset ? POLYGONOFFSET_STANDARD : POLYGONOFFSET_DISABLED);

	LEAVE_STATE;
}

void GL_StateEndDrawBrushModel(void)
{
}

void GL_StateBeginDrawAliasModel(entity_t* ent, aliashdr_t* paliashdr)
{
	extern cvar_t r_viewmodelsize;

	GL_EnterTracedRegion(va("GL_StateBeginDrawAliasModel(%s)", ent->model->name), true);

	GLC_PauseMatrixUpdate();
	R_RotateForEntity(ent);
	if (ent->model->modhint == MOD_EYES) {
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2] - (22 + 8));
		// double size of eyes, since they are really hard to see in gl
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0] * 2, paliashdr->scale[1] * 2, paliashdr->scale[2] * 2);
	}
	else if (ent->renderfx & RF_WEAPONMODEL) {
		float scale = 0.5 + bound(0, r_viewmodelsize.value, 1) / 2;

		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0] * scale, paliashdr->scale[1], paliashdr->scale[2]);
	}
	else {
		GL_Translate(GL_MODELVIEW, paliashdr->scale_origin[0], paliashdr->scale_origin[1], paliashdr->scale_origin[2]);
		GL_Scale(GL_MODELVIEW, paliashdr->scale[0], paliashdr->scale[1], paliashdr->scale[2]);
	}
	GLC_LoadMatrix(GL_MODELVIEW);
	GLC_ResumeMatrixUpdate();

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_EnableFog();

	LEAVE_STATE;
}

void GL_StateEndDrawAliasModel(void)
{
}

void GLC_StateBeginSimpleItem(texture_ref simpletexture)
{
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_ShadeModel(GL_FLAT);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}
	GL_DisableFog();
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_ENABLED);
	GLC_InitTextureUnits1(simpletexture, GL_REPLACE);
}

void GLC_StateEndSimpleItem(void)
{
}

void GL_StateBeginAliasOutlineFrame(void)
{
	extern cvar_t gl_outline_width;

	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_ShadeModel(GL_FLAT);
	GL_DisableFog();

	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);
	GL_CullFace(GL_BACK);
	GL_PolygonMode(GL_LINE);

	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));

	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_black);
		GL_Enable(GL_LINE_SMOOTH);
		GLC_DisableAllTexturing();
		GL_Hint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
	}

	LEAVE_STATE;
}

void GL_StateEndAliasOutlineFrame(void)
{
}

void GLM_StateBeginAliasOutlineBatch(void)
{
	extern cvar_t gl_outline_width;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	GL_ShadeModel(GL_FLAT);
	GL_DisableFog();

	GL_PolygonOffset(POLYGONOFFSET_OUTLINES);
	GL_CullFace(GL_BACK);
	GL_PolygonMode(GL_LINE);

	// limit outline width, since even width == 3 can be considered as cheat.
	glLineWidth(bound(0.1, gl_outline_width.value, 3.0));
}

void GLM_StateEndAliasOutlineBatch(void)
{
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_PolygonMode(GL_FILL);
	GL_CullFace(GL_BACK);
}
