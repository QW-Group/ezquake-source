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

#include "quakedef.h"
#include "gl_local.h"
#include "glc_state.h"
#include "glc_vao.h"
#include "r_trace.h"
#include "r_aliasmodel.h"

void GLC_StateBeginFastTurbPoly(byte color[4])
{
	float wateralpha = R_WaterAlpha();
	wateralpha = bound(0, wateralpha, 1);

	ENTER_STATE;

	// START shaman FIX /gl_turbalpha + /r_fastturb {
	R_CustomColor(color[0] * wateralpha / 255.0f, color[1] * wateralpha / 255.0f, color[2] * wateralpha / 255.0f, wateralpha);
	// END shaman FIX /gl_turbalpha + /r_fastturb {

	LEAVE_STATE;
}

void GLC_StateBeginBlendLightmaps(qbool use_buffers)
{
	GLC_EnsureVAOCreated(vao_brushmodel_lightmap_pass);
	R_ApplyRenderingState(r_state_world_blend_lightmaps);
}

void GLC_StateBeginCausticsPolys(void)
{
	GLC_EnsureVAOCreated(vao_brushmodel);
	R_ApplyRenderingState(r_state_world_caustics);
}

// Alias models
void GLC_StateBeginUnderwaterCaustics(void)
{
	R_ApplyRenderingState(r_state_aliasmodel_caustics);
}

void GLC_StateBeginWaterSurfaces(void)
{
	extern cvar_t r_fastturb;
	float wateralpha = R_WaterAlpha();

	if (r_fastturb.integer) {
		if (wateralpha < 1.0) {
			R_ApplyRenderingState(r_state_world_fast_translucent_water);
			R_CustomColor(wateralpha, wateralpha, wateralpha, wateralpha);
		}
		else {
			R_ApplyRenderingState(r_state_world_fast_opaque_water);
		}
	}
	else {
		if (wateralpha < 1.0) {
			R_ApplyRenderingState(r_state_world_translucent_water);
			R_CustomColor(wateralpha, wateralpha, wateralpha, wateralpha);
		}
		else {
			R_ApplyRenderingState(r_state_world_opaque_water);
		}
	}
}

void GLC_StateBeginAlphaChain(void)
{
	R_ApplyRenderingState(r_state_world_alpha_surfaces);
}

void GLC_StateBeginAlphaChainSurface(msurface_t* s)
{
	texture_t* t = s->texinfo->texture;

	ENTER_STATE;

	//bind the world texture
	R_TextureUnitBind(0, t->gl_texturenum);
	if (gl_mtexable) {
		R_TextureUnitBind(1, GLC_LightmapTexture(s->lightmaptexturenum));
	}

	LEAVE_STATE;
}

void GLC_StateBeginRenderFullbrights(void)
{
	R_ApplyRenderingState(r_state_world_fullbrights);
}

void GLC_StateBeginRenderLumas(void)
{
	R_ApplyRenderingState(r_state_world_lumas);
}

void GLC_StateBeginEmitDetailPolys(void)
{
	R_ApplyRenderingState(r_state_world_details);
}

void GLC_StateBeginDrawMapOutline(void)
{
	extern cvar_t gl_outline_width;

	R_ApplyRenderingState(r_state_world_outline);
	R_CustomLineWidth(bound(0.1, gl_outline_width.value, 3.0));
}

void GLC_StateBeginAliasPowerupShell(void)
{
	extern texture_ref shelltexture;

	R_ApplyRenderingState(r_state_aliasmodel_powerupshell);
	R_TextureUnitBind(0, shelltexture);
}

void GLC_StateBeginMD3Draw(float alpha, qbool textured)
{
	if (textured) {
		if (alpha < 1) {
			R_ApplyRenderingState(r_state_aliasmodel_singletexture_transparent);
		}
		else {
			R_ApplyRenderingState(r_state_aliasmodel_singletexture_opaque);
		}
	}
	else {
		if (alpha < 1) {
			R_ApplyRenderingState(r_state_aliasmodel_notexture_transparent);
		}
		else {
			R_ApplyRenderingState(r_state_aliasmodel_notexture_opaque);
		}
	}
}

void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, qbool alpha_blend, struct custom_model_color_s* custom_model, qbool weapon_model)
{
	r_state_id state;

	ENTER_STATE;

	if (!weapon_model && (!GL_TextureReferenceIsValid(texture) || (custom_model && custom_model->fullbright_cvar.integer))) {
		state = alpha_blend ? r_state_aliasmodel_notexture_transparent : r_state_aliasmodel_notexture_opaque;
	}
	else if (custom_model == NULL && GL_TextureReferenceIsValid(fb_texture) && mtex) {
		state = weapon_model ? (alpha_blend ? r_state_weaponmodel_multitexture_transparent : r_state_weaponmodel_multitexture_opaque) : (alpha_blend ? r_state_aliasmodel_multitexture_transparent : r_state_aliasmodel_multitexture_opaque);
		R_TextureUnitBind(0, texture);
		R_TextureUnitBind(1, fb_texture);
	}
	else {
		state = weapon_model ? (alpha_blend ? r_state_weaponmodel_singletexture_transparent : r_state_weaponmodel_singletexture_opaque) : (alpha_blend ? r_state_aliasmodel_singletexture_transparent : r_state_aliasmodel_singletexture_opaque);
		R_TextureUnitBind(0, texture);
	}

	R_ApplyRenderingState(state);

	LEAVE_STATE;
}

void GLC_StateBeginAliasModelShadow(void)
{
	R_ApplyRenderingState(r_state_aliasmodel_shadows);
}
