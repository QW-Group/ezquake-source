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

	if (!GL_ShadersSupported()) {
		// FIXME: Work on getting rid of these
		GL_PolygonMode(GL_FILL);
		GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_DISABLED);
		GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}

	LEAVE_STATE;
}

void GLC_StateBeginAliasPowerupShell(void)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
	}
	GL_DisableFog();

	GL_AlphaBlendFlags(GL_BLEND_ENABLED);
	GLC_InitTextureUnits1(shelltexture, GL_MODULATE);
	GL_BlendFunc(GL_ONE, GL_ONE);

	LEAVE_STATE;
}

void GLC_StateEndAliasPowerupShell(void)
{
}

void GLC_StateBeginMD3Draw(float alpha)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
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

	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	GL_Disable(GL_LINE_SMOOTH);
	GL_DisableFog();

	GL_AlphaBlendFlags(alpha < 1 ? GL_BLEND_ENABLED : GL_BLEND_DISABLED);

	if (custom_model) {
		if (custom_model->fullbright_cvar.integer || !GL_TextureReferenceIsValid(texture)) {
			GLC_DisableAllTexturing();
		}
		else {
			GLC_InitTextureUnits1(texture, GL_MODULATE);
		}
	}
	else {
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

	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GL_Disable(GL_LINE_SMOOTH);
	}
	GL_DisableFog();

	GLC_DisableAllTexturing();
	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_Color4f(0, 0, 0, 0.5);

	LEAVE_STATE;
}

void GLC_StateEndAliasModelShadow(void)
{
}

void GLC_StateBeginDrawViewModel(float alpha)
{
	ENTER_STATE;

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	GL_Disable(GL_LINE_SMOOTH);
	GLC_InitTextureUnitsNoBind1(GL_REPLACE);
	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	GL_DisableFog();

	// hack the depth range to prevent view model from poking into walls
	GL_DepthRange(gldepthmin, gldepthmin + 0.3 * (gldepthmax - gldepthmin));
	if (gl_mtexable && alpha < 1) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
		GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	}
	else {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	}

	LEAVE_STATE;
}

void GLC_StateEndDrawViewModel(void)
{
	GL_DepthRange(gldepthmin, gldepthmax);
	GL_PolygonMode(GL_FILL);
}

void GL_StateBeginDrawBrushModel(entity_t* e, qbool polygonOffset)
{
	GL_EnterTracedRegion("GL_StateBeginDrawBrushModel", true);

	GLC_PauseMatrixUpdate();
	GL_Translate(GL_MODELVIEW, e->origin[0],  e->origin[1],  e->origin[2]);
	GL_Rotate(GL_MODELVIEW, e->angles[1], 0, 0, 1);
	GL_Rotate(GL_MODELVIEW, e->angles[0], 0, 1, 0);
	GL_Rotate(GL_MODELVIEW, e->angles[2], 1, 0, 0);
	GLC_LoadMatrix(GL_MODELVIEW);
	GLC_ResumeMatrixUpdate();

	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);

	if (GL_ShadersSupported()) {
		GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | (e->alpha ? GL_BLEND_ENABLED : GL_BLEND_DISABLED));
	}
	else {
		GL_Disable(GL_LINE_SMOOTH);
		GL_DisableFog();

		if (e->alpha) {
			GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_ENABLED);
			GL_Color4f(e->alpha, e->alpha, e->alpha, e->alpha);
		}
		else {
			GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
			GL_Color3ubv(color_white);
		}
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

	GL_EnterTracedRegion(__FUNCTION__, true);

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
	GLC_ResumeMatrixUpdate();
	GLC_LoadMatrix(GL_MODELVIEW);

	GL_AlphaBlendFlags(GL_ALPHATEST_DISABLED | GL_BLEND_DISABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	if (!GL_ShadersSupported()) {
		GLC_InitTextureUnitsNoBind1(GL_REPLACE);
		GL_Color3ubv(color_white);
		GL_Disable(GL_LINE_SMOOTH);
		GLC_EnsureTMUEnabled(GL_TEXTURE0);
	}
	GL_EnableFog();

	LEAVE_STATE;
}

void GL_StateEndDrawAliasModel(void)
{
}

void GLC_StateBeginSimpleItem(texture_ref simpletexture)
{
	GL_PolygonOffset(POLYGONOFFSET_DISABLED);
	GL_CullFace(GL_FRONT);
	GL_PolygonMode(GL_FILL);
	GL_Color3ubv(color_white);
	GL_Disable(GL_LINE_SMOOTH);
	GLC_EnsureTMUEnabled(GL_TEXTURE0);
	GL_DisableFog();
	GL_AlphaBlendFlags(GL_ALPHATEST_ENABLED | GL_BLEND_ENABLED);
	GL_BlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
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
	GL_CullFace(GL_FRONT);
}
