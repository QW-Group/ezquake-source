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

#ifdef RENDERER_OPTION_CLASSIC_OPENGL

#include "quakedef.h"
#include "gl_local.h"
#include "glc_state.h"
#include "glc_vao.h"
#include "r_trace.h"
#include "r_aliasmodel.h"
#include "r_matrix.h"
#include "r_renderer.h"
#include "r_program.h"

extern texture_ref solidskytexture, alphaskytexture;

void GLC_StateBeginFastTurbPoly(byte color[4])
{
	float wateralpha = r_refdef2.wateralpha;
	wateralpha = bound(0, wateralpha, 1);

	R_TraceEnterFunctionRegion;

	// START shaman FIX /gl_turbalpha + /r_fastturb {
	R_CustomColor(color[0] * wateralpha / 255.0f, color[1] * wateralpha / 255.0f, color[2] * wateralpha / 255.0f, wateralpha);
	// END shaman FIX /gl_turbalpha + /r_fastturb {

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginBlendLightmaps(qbool use_buffers)
{
	R_ApplyRenderingState(r_state_world_blend_lightmaps);
}

void GLC_StateBeginCausticsPolys(void)
{
	R_ApplyRenderingState(r_state_world_caustics);
}

// Alias models
void GLC_StateBeginUnderwaterAliasModelCaustics(texture_ref base_texture, texture_ref caustics_texture)
{
	R_ApplyRenderingState(r_state_aliasmodel_caustics);
	renderer.TextureUnitBind(0, base_texture);
	GLC_BeginCausticsTextureMatrix();
	renderer.TextureUnitBind(1, caustics_texture);
}

void GLC_StateEndUnderwaterAliasModelCaustics(void)
{
	GLC_EndCausticsTextureMatrix();
}

void GLC_StateBeginWaterSurfaces(void)
{
	extern cvar_t r_fastturb;
	float wateralpha = r_refdef2.wateralpha;

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

	R_TraceEnterFunctionRegion;

	//bind the world texture
	renderer.TextureUnitBind(0, t->gl_texturenum);
	if (gl_mtexable) {
		renderer.TextureUnitBind(1, GLC_LightmapTexture(s->lightmaptexturenum));
	}

	R_TraceLeaveFunctionRegion;
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
	extern texture_ref detailtexture;

	R_ApplyRenderingState(r_state_world_details);
	renderer.TextureUnitBind(0, detailtexture);
}

void GLC_StateBeginDrawMapOutline(void)
{
	R_ApplyRenderingState(r_state_world_outline);
}

void GLC_StateBeginAliasPowerupShell(qbool weapon)
{
	extern texture_ref shelltexture;
	extern cvar_t r_viewmodelsize;

	R_ApplyRenderingState(weapon ? r_state_weaponmodel_powerupshell : r_state_aliasmodel_powerupshell);
	renderer.TextureUnitBind(0, shelltexture);

	if (weapon) {
		R_ScaleModelview(0.5 + bound(0, r_viewmodelsize.value, 1) / 2, 1, 1);
	}
}

void GLC_StateBeginMD3Draw(float alpha, qbool textured, qbool weapon, qbool additive_pass)
{
	qbool transparent = (alpha < 1);

	if (additive_pass) {
		R_ApplyRenderingState(weapon ? r_state_weaponmodel_singletexture_additive : textured ? r_state_aliasmodel_singletexture_additive : r_state_aliasmodel_notexture_additive);
	}
	else if (weapon) {
		R_ApplyRenderingState(transparent ? r_state_weaponmodel_singletexture_transparent : r_state_weaponmodel_singletexture_opaque);
	}
	else if (textured) {
		R_ApplyRenderingState(transparent ? r_state_aliasmodel_singletexture_transparent : r_state_aliasmodel_singletexture_opaque);
	}
	else {
		R_ApplyRenderingState(transparent ? r_state_aliasmodel_notexture_transparent : r_state_aliasmodel_notexture_opaque);
	}
}

void GLC_StateBeginDrawAliasFrameProgram(texture_ref texture, texture_ref fb_texture, int render_effects, struct custom_model_color_s* custom_model, float ent_alpha, qbool additive_pass)
{
	qbool weapon_model = render_effects & RF_WEAPONMODEL;
	qbool alpha_blend = (render_effects & RF_ALPHABLEND) || ent_alpha < 1;
	qbool no_texture = !weapon_model && (!R_TextureReferenceIsValid(texture) || (custom_model && custom_model->fullbright_cvar.integer));
	qbool multi_texture = custom_model == NULL && R_TextureReferenceIsValid(fb_texture);

	R_TraceEnterFunctionRegion;

	if (no_texture) {
		R_ApplyRenderingState(additive_pass ? r_state_aliasmodel_notexture_additive : alpha_blend ? r_state_aliasmodel_notexture_transparent : r_state_aliasmodel_notexture_opaque);
	}
	else if (multi_texture) {
		// meag: no additive_pass here yet as .mdl has multitexture, .md3 has additive surfaces
		if (weapon_model) {
			R_ApplyRenderingState(alpha_blend ? r_state_weaponmodel_multitexture_transparent : r_state_weaponmodel_multitexture_opaque);
		}
		else {
			R_ApplyRenderingState(alpha_blend ? r_state_aliasmodel_multitexture_transparent : r_state_aliasmodel_multitexture_opaque);
		}
		renderer.TextureUnitBind(0, texture);
		if (render_effects & RF_CAUSTICS) {
			GLC_BeginCausticsTextureMatrix();
		}
		renderer.TextureUnitBind(1, fb_texture);
	}
	else {
		if (weapon_model) {
			R_ApplyRenderingState(additive_pass ? r_state_weaponmodel_singletexture_additive : alpha_blend ? r_state_weaponmodel_singletexture_transparent : r_state_weaponmodel_singletexture_opaque);
		}
		else {
			R_ApplyRenderingState(additive_pass ? r_state_aliasmodel_singletexture_additive : alpha_blend ? r_state_aliasmodel_singletexture_transparent : r_state_aliasmodel_singletexture_opaque);
		}
		renderer.TextureUnitBind(0, texture);
	}

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginDrawAliasFrame(texture_ref texture, texture_ref fb_texture, qbool mtex, qbool alpha_blend, struct custom_model_color_s* custom_model, qbool weapon_model)
{
	R_TraceEnterFunctionRegion;

	if (!weapon_model && (!R_TextureReferenceIsValid(texture) || (custom_model && (custom_model->fullbright_cvar.integer || custom_model->disable_texturing)))) {
		R_ApplyRenderingState(alpha_blend ? r_state_aliasmodel_notexture_transparent : r_state_aliasmodel_notexture_opaque);
	}
	else if (custom_model == NULL && R_TextureReferenceIsValid(fb_texture) && mtex) {
		R_ApplyRenderingState(weapon_model ? (alpha_blend ? r_state_weaponmodel_multitexture_transparent : r_state_weaponmodel_multitexture_opaque) : (alpha_blend ? r_state_aliasmodel_multitexture_transparent : r_state_aliasmodel_multitexture_opaque));
		renderer.TextureUnitBind(0, texture);
		renderer.TextureUnitBind(1, fb_texture);
	}
	else {
		R_ApplyRenderingState(weapon_model ? (alpha_blend ? r_state_weaponmodel_singletexture_transparent : r_state_weaponmodel_singletexture_opaque) : (alpha_blend ? r_state_aliasmodel_singletexture_transparent : r_state_aliasmodel_singletexture_opaque));
		renderer.TextureUnitBind(0, texture);
	}

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginAliasModelShadow(void)
{
	R_ApplyRenderingState(r_state_aliasmodel_shadows);
}

#ifdef WITH_RENDERING_TRACE
static int glcVertsPerPrimitive = 0;
static int glcBaseVertsPerPrimitive = 0;
static int glcVertsSent = 0;
static const char* glcPrimitiveName = "?";
#endif

void GLC_Begin(GLenum primitive)
{
#ifdef WITH_RENDERING_TRACE
	glcVertsSent = 0;
	glcVertsPerPrimitive = 0;
	glcBaseVertsPerPrimitive = 0;
	glcPrimitiveName = "?";

	GL_ProcessErrors("glBegin");
	switch (primitive) {
		case GL_QUADS:
			glcVertsPerPrimitive = 4;
			glcBaseVertsPerPrimitive = 0;
			glcPrimitiveName = "GL_QUADS";
			break;
		case GL_POLYGON:
			glcVertsPerPrimitive = 0;
			glcBaseVertsPerPrimitive = 0;
			glcPrimitiveName = "GL_POLYGON";
			break;
		case GL_TRIANGLE_FAN:
			glcVertsPerPrimitive = 1;
			glcBaseVertsPerPrimitive = 2;
			glcPrimitiveName = "GL_TRIANGLE_FAN";
			break;
		case GL_TRIANGLE_STRIP:
			glcVertsPerPrimitive = 1;
			glcBaseVertsPerPrimitive = 2;
			glcPrimitiveName = "GL_TRIANGLE_STRIP";
			break;
		case GL_LINE_LOOP:
			glcVertsPerPrimitive = 1;
			glcBaseVertsPerPrimitive = 1;
			glcPrimitiveName = "GL_LINE_LOOP";
			break;
		case GL_LINES:
			glcVertsPerPrimitive = 2;
			glcBaseVertsPerPrimitive = 0;
			glcPrimitiveName = "GL_LINES";
			break;
	}
#endif

	++frameStats.draw_calls;
	glBegin(primitive);
	R_TraceLogAPICall("glBegin(%s...)", glcPrimitiveName);
}

#undef glEnd

void GLC_End(void)
{
#ifdef WITH_RENDERING_TRACE
	int primitives;
	const char* count_name = "vertices";
#endif

	glEnd();

#ifdef WITH_RENDERING_TRACE
	primitives = max(0, glcVertsSent - glcBaseVertsPerPrimitive);
	if (glcVertsPerPrimitive) {
		primitives = glcVertsSent / glcVertsPerPrimitive;
		count_name = "primitives";
	}
	R_TraceLogAPICall("glEnd(%s: %d %s)", glcPrimitiveName, primitives, count_name);
	GL_ProcessErrors("glEnd");
#endif
}

void GLC_Vertex2f(GLfloat x, GLfloat y)
{
	glVertex2f(x, y);
#ifdef WITH_RENDERING_TRACE
	++glcVertsSent;
#endif
}

void GLC_Vertex2fv(const GLfloat* v)
{
	glVertex2fv(v);
#ifdef WITH_RENDERING_TRACE
	++glcVertsSent;
#endif
}

void GLC_Vertex3f(GLfloat x, GLfloat y, GLfloat z)
{
	glVertex3f(x, y, z);
#ifdef WITH_RENDERING_TRACE
	++glcVertsSent;
#endif
}

void GLC_Vertex3fv(const GLfloat* v)
{
	glVertex3fv(v);
#ifdef WITH_RENDERING_TRACE
	++glcVertsSent;
#endif
}

void GLC_InitialiseSkyStates(void)
{
	rendering_state_t* state;

	state = R_InitRenderingState(r_state_sky_fast, true, "fastSkyState", vao_brushmodel);
	state->depth.test_enabled = false;

	state = R_InitRenderingState(r_state_skybox, true, "glcSkyBox", vao_brushmodel);
	R_GLC_TextureUnitSet(state, 0, true, r_texunit_mode_replace);

	state = R_InitRenderingState(r_state_sky_fast_fogged, true, "fastSkyStateFogged", vao_brushmodel);
	state->depth.test_enabled = false;
	state->fog.enabled = true;

	state = R_CopyRenderingState(r_state_sky_fast_bmodel, r_state_sky_fast, "fastSkyState_bmodel");
	state->depth.test_enabled = true;

	state = R_CopyRenderingState(r_state_sky_fast_fogged_bmodel, r_state_sky_fast_fogged, "fastSkyStateFogged_bmodel");
	state->depth.test_enabled = true;

	state = R_InitRenderingState(r_state_skydome_zbuffer_pass, true, "skyDomeZPassState", vao_brushmodel);
	state->depth.test_enabled = true;
	state->blendingEnabled = true;
	state->fog.enabled = false;
	state->colorMask[0] = state->colorMask[1] = state->colorMask[2] = state->colorMask[3] = false;
	state->blendFunc = r_blendfunc_src_zero_dest_one;

	state = R_InitRenderingState(r_state_skydome_zbuffer_pass_fogged, true, "skyDomeZPassFoggedState", vao_brushmodel);
	state->depth.test_enabled = true;
	state->blendingEnabled = true;
	state->fog.enabled = true;
	state->blendFunc = r_blendfunc_src_one_dest_zero;

	state = R_InitRenderingState(r_state_skydome_background_pass, true, "skyDomeFirstPassState", vao_none);
	state->depth.test_enabled = false;
	state->blendingEnabled = false;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_CopyRenderingState(r_state_skydome_background_pass_bmodel, r_state_skydome_background_pass, "skyDomeFirstPassState_bmodel");
	state->depth.test_enabled = true;

	state = R_InitRenderingState(r_state_skydome_cloud_pass, true, "skyDomeCloudPassState", vao_none);
	state->depth.test_enabled = false;
	state->blendingEnabled = true;
	state->blendFunc = r_blendfunc_premultiplied_alpha;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;

	state = R_CopyRenderingState(r_state_skydome_cloud_pass_bmodel, r_state_skydome_cloud_pass, "skyDomeCloudPassState_bmodel");
	state->depth.test_enabled = true;

	// Used when rendering the skydome/cloud background, prior to z-pass
	state = R_InitRenderingState(r_state_skydome_single_pass, true, "skyDomeSinglePassState", vao_brushmodel);
	state->depth.test_enabled = false;
	state->blendingEnabled = false;
	state->textureUnits[0].enabled = true;
	state->textureUnits[0].mode = r_texunit_mode_replace;
	state->textureUnits[1].enabled = true;
	state->textureUnits[1].mode = r_texunit_mode_decal;

	// Used when rendering the polys directly (like r_fastsky) but texturing
	state = R_CopyRenderingState(r_state_skydome_single_pass_program, r_state_skydome_single_pass, "skyDomeSinglePass(program)");
	state->depth.test_enabled = true;

	// Deliberately not using r_state_skydome_single_pass_program just so we can tie it to vbo etc in future
	state = R_CopyRenderingState(r_state_skydome_single_pass_bmodel, r_state_skydome_single_pass, "skyDomeSinglePass(bmodel)");
	state->depth.test_enabled = true;
}

void GLC_StateBeginFastSky(qbool world)
{
	extern cvar_t gl_fogsky, gl_fogenable, r_skycolor;

	R_TraceEnterFunctionRegion;

	if (gl_fogsky.integer && gl_fogenable.integer) {
		R_ApplyRenderingState(world ? r_state_sky_fast_fogged : r_state_sky_fast_fogged_bmodel);
	}
	else {
		R_ApplyRenderingState(world ? r_state_sky_fast : r_state_sky_fast_bmodel);
	}
	R_CustomColor(r_skycolor.color[0] / 255.0f, r_skycolor.color[1] / 255.0f, r_skycolor.color[2] / 255.0f, 1.0f);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSkyZBufferPass(void)
{
	extern cvar_t gl_fogsky, gl_fogenable, r_skycolor, gl_fogred, gl_foggreen, gl_fogblue;

	R_TraceEnterFunctionRegion;

	if (gl_fogenable.integer && gl_fogsky.integer) {
		R_ApplyRenderingState(r_state_skydome_zbuffer_pass_fogged);
		R_CustomColor(gl_fogred.value, gl_foggreen.value, gl_fogblue.value, 1);
	}
	else {
		R_ApplyRenderingState(r_state_skydome_zbuffer_pass);
	}

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureSkyDome(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_background_pass);
	renderer.TextureUnitBind(0, solidskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureSkyDomeCloudPass(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_cloud_pass);
	renderer.TextureUnitBind(0, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginMultiTextureSkyDome(qbool use_program)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(use_program ? r_state_skydome_single_pass_program : r_state_skydome_single_pass);
	renderer.TextureUnitBind(0, solidskytexture);
	renderer.TextureUnitBind(1, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginMultiTextureSkyChain(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_single_pass_bmodel);
	renderer.TextureUnitBind(0, solidskytexture);
	renderer.TextureUnitBind(1, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureSkyPass(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_background_pass_bmodel);
	renderer.TextureUnitBind(0, solidskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginSingleTextureCloudPass(void)
{
	R_TraceEnterFunctionRegion;

	R_ApplyRenderingState(r_state_skydome_cloud_pass_bmodel);
	renderer.TextureUnitBind(0, alphaskytexture);

	R_TraceLeaveFunctionRegion;
}

void GLC_StateBeginBrightenScreen(void)
{
	R_ApplyRenderingState(r_state_brighten_screen);
}

void GLC_StateBeginDrawAlphaPieSliceRGB(float thickness)
{
	// Same as lineState
	R_ApplyRenderingState(r_state_line);
	if (thickness > 0.0) {
		R_CustomLineWidth(thickness);
	}
}

void GLC_StateBeginBloomDraw(texture_ref texture)
{
	R_ApplyRenderingState(r_state_postprocess_bloom_draweffect);
	R_CustomColor(r_bloom_alpha.value, r_bloom_alpha.value, r_bloom_alpha.value, 1.0f);
	renderer.TextureUnitBind(0, texture);
}

void GLC_StateBeginImageDraw(qbool is_text)
{
	extern cvar_t gl_alphafont;

	if (is_text && !gl_alphafont.integer) {
		R_ApplyRenderingState(r_state_hud_images_alphatested_glc);
	}
	else {
		R_ApplyRenderingState(r_state_hud_images_glc);
	}
}

void GLC_StateBeginImageDrawNonGLSL(qbool is_text)
{
	extern cvar_t gl_alphafont;

	if (is_text && !gl_alphafont.integer) {
		R_ApplyRenderingState(r_state_hud_images_alphatested_glc_non_glsl);
	}
	else {
		R_ApplyRenderingState(r_state_hud_images_glc_non_glsl);
	}
}

void GLC_StateBeginAliasOutlineFrame(qbool weaponmodel)
{
	R_ApplyRenderingState(weaponmodel ? r_state_weaponmodel_outline : r_state_aliasmodel_outline);
	R_GLC_DisableColorPointer();
	R_CustomColor(0, 0, 0, 1);
}

#endif // #ifdef RENDERER_OPTION_CLASSIC_OPENGL
