/*
Copyright (C) 2017-2018 ezQuake team

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
#include "glm_draw.h"
#include "r_draw.h"
#include "r_local.h"
#include "tr_types.h"
#include "r_matrix.h"
#include "glsl/constants.glsl"

#define MAX_2D_ELEMENTS 4096

typedef struct draw_hud_element_s {
	r_image_type_t type;
	texture_ref texture;
	int index;
	draw_layer_t layer;
} draw_hud_element_t;

typedef struct hud_api_s {
	struct {
		void(*Prepare)(void);
		void(*Draw)(texture_ref texture, int start, int end);
	} types[imagetype_count];
	void(*OnComplete)(void);
	//void(*DrawAccelBar)(int x, int y, int length, int charsize, int pos);

	draw_hud_element_t elements[MAX_2D_ELEMENTS];
	int count;
} hud_api_t;

static hud_api_t hud;
#define HUD_LAYER_STACK_MAX 8
static draw_layer_t hud_current_layer = draw_layer_base;
static draw_layer_t hud_layer_stack[HUD_LAYER_STACK_MAX];
static int hud_layer_stack_depth = 0;
static draw_layer_t hud_text_layer = draw_layer_overlay;
static draw_layer_t hud_text_layer_stack[HUD_LAYER_STACK_MAX];
static int hud_text_layer_stack_depth = 0;

void Draw_SetLayer(draw_layer_t layer)
{
	hud_current_layer = bound(draw_layer_base, layer, draw_layer_top);
}

draw_layer_t Draw_GetLayer(void)
{
	return hud_current_layer;
}

void Draw_PushLayer(draw_layer_t layer)
{
	if (hud_layer_stack_depth < HUD_LAYER_STACK_MAX) {
		hud_layer_stack[hud_layer_stack_depth++] = hud_current_layer;
	}
	hud_current_layer = bound(draw_layer_base, layer, draw_layer_top);
}

void Draw_PopLayer(void)
{
	if (hud_layer_stack_depth > 0) {
		hud_current_layer = hud_layer_stack[--hud_layer_stack_depth];
	}
	else {
		hud_current_layer = draw_layer_base;
	}
}

void Draw_SetTextLayer(draw_layer_t layer)
{
	hud_text_layer = bound(draw_layer_base, layer, draw_layer_top);
}

draw_layer_t Draw_GetTextLayer(void)
{
	return hud_text_layer;
}

void Draw_PushTextLayer(draw_layer_t layer)
{
	if (hud_text_layer_stack_depth < HUD_LAYER_STACK_MAX) {
		hud_text_layer_stack[hud_text_layer_stack_depth++] = hud_text_layer;
	}
	hud_text_layer = bound(draw_layer_base, layer, draw_layer_top);
}

void Draw_PopTextLayer(void)
{
	if (hud_text_layer_stack_depth > 0) {
		hud_text_layer = hud_text_layer_stack[--hud_text_layer_stack_depth];
	}
	else {
		hud_text_layer = draw_layer_overlay;
	}
}

#define HudSetFunctionPointers(prefix) \
{ \
	extern void prefix ## _HudDrawCircles(texture_ref texture, int start, int end); \
	extern void prefix ## _HudDrawLines(texture_ref texture, int start, int end); \
	extern void prefix ## _HudDrawPolygons(texture_ref texture, int start, int end); \
	extern void prefix ## _HudDrawImages(texture_ref texture, int start, int length); \
	extern void prefix ## _HudPrepareCircles(void); \
	extern void prefix ## _HudPrepareImages(void); \
	extern void prefix ## _HudDrawComplete(void); \
\
	hud.types[imagetype_image].Draw = prefix ## _HudDrawImages; \
	hud.types[imagetype_image].Prepare = prefix ## _HudPrepareImages; \
	hud.types[imagetype_circle].Draw = prefix ## _HudDrawCircles; \
	hud.types[imagetype_circle].Prepare = prefix ## _HudPrepareCircles; \
	hud.types[imagetype_line].Draw = prefix ## _HudDrawLines; \
	hud.types[imagetype_line].Prepare = NULL; \
	hud.types[imagetype_polygon].Draw = prefix ## _HudDrawPolygons; \
	hud.types[imagetype_polygon].Prepare = NULL; \
	hud.OnComplete = prefix ## _HudDrawComplete; \
}

void R_Hud_Initialise(void)
{
#ifdef RENDERER_OPTION_MODERN_OPENGL
	if (R_UseModernOpenGL()) {
		HudSetFunctionPointers(GLM);
	}
#endif
#ifdef RENDERER_OPTION_CLASSIC_OPENGL
	if (R_UseImmediateOpenGL()) {
		HudSetFunctionPointers(GLC);
	}
#endif
#ifdef RENDERER_OPTION_VULKAN
	if (R_UseVulkan()) {
		HudSetFunctionPointers(VK);
	}
#endif
}

static void R_PrepareImageDraw(void)
{
	hud.types[imagetype_image].Prepare();
	hud.types[imagetype_circle].Prepare();

	R_IdentityModelView();
	R_IdentityProjectionView();
}

static void R_DrawElementRange(texture_ref texture, r_image_type_t type, int start_elem, int end_elem)
{
	if (type == imagetype_image) {
		hud.types[imagetype_image].Draw(texture, hud.elements[start_elem].index, hud.elements[end_elem].index);
	}
	else {
		hud.types[type].Draw(texture, hud.elements[start_elem].index, hud.elements[end_elem].index);
	}
}

static void R_FlushImageDrawLayerInternal(draw_layer_t layer, qbool finalize)
{
	int start = -1;
	texture_ref currentTexture = null_texture_reference;
	r_image_type_t type = imagetype_image;
	int i;

	if (!hud.count || !glConfig.initialized) {
		if (finalize) {
			R_EmptyImageQueue();
			hud.OnComplete();
		}
		return;
	}

	R_PrepareImageDraw();

	for (i = 0; i < hud.count; ++i) {
		if (hud.elements[i].layer != layer) {
			if (start >= 0) {
				R_DrawElementRange(currentTexture, type, start, i - 1);
				start = -1;
				currentTexture = null_texture_reference;
			}
			continue;
		}

		if (start < 0) {
			start = i;
			type = hud.elements[i].type;
			currentTexture = hud.elements[i].texture;
			continue;
		}

		{
			qbool texture_changed = (R_TextureReferenceIsValid(currentTexture) &&
				R_TextureReferenceIsValid(hud.elements[i].texture) &&
				!R_TextureReferenceEqual(currentTexture, hud.elements[i].texture));

			if (hud.elements[i].type != type || texture_changed) {
				R_DrawElementRange(currentTexture, type, start, i - 1);
				start = i;
				type = hud.elements[i].type;
				currentTexture = hud.elements[i].texture;
			}
			else if (R_TextureReferenceIsValid(hud.elements[i].texture)) {
				currentTexture = hud.elements[i].texture;
			}
		}
	}

	if (start >= 0) {
		R_DrawElementRange(currentTexture, type, start, hud.count - 1);
	}

	if (finalize) {
		R_EmptyImageQueue();
		hud.OnComplete();
	}
}

void R_FlushImageDrawLayer(draw_layer_t layer, qbool finalize)
{
	R_FlushImageDrawLayerInternal(layer, finalize);
}

void R_DrawRectangle(float x, float y, float width, float height, byte* color);

void R_DrawAlphaRectangleRGB(int x, int y, int w, int h, float thickness, qbool fill, byte* bytecolor)
{
	if (fill) {
		R_DrawRectangle(x, y, w, h, bytecolor);
	}
	else {
		R_DrawRectangle(x, y, w, thickness, bytecolor);
		R_DrawRectangle(x, y + thickness, thickness, h - thickness, bytecolor);
		R_DrawRectangle(x + w - thickness, y + thickness, thickness, h - thickness, bytecolor);
		R_DrawRectangle(x, y + h - thickness, w, thickness, bytecolor);
	}
}

void R_Draw_SAlphaSubPic2(float x, float y, mpic_t *pic, int src_width, int src_height, float newsl, float newtl, float newsh, float newth, float scale_x, float scale_y, float alpha)
{
	byte color[] = { 255, 255, 255, alpha * 255 };

	R_DrawImage(x, y, scale_x * src_width, scale_y * src_height, newsl, newtl, newsh - newsl, newth - newtl, color, false, pic->texnum, false, false);
}

void R_Draw_FadeScreen(float alpha)
{
	Draw_AlphaRectangleRGB(0, 0, vid.width, vid.height, 0.0f, true, RGBA_TO_COLOR(0, 0, 0, (alpha < 1 ? alpha * 255 : 255)));
}

void R_EmptyImageQueue(void)
{
	hud.count = imageData.imageCount = circleData.circleCount = lineData.lineCount = polygonData.polygonCount = 0;
	hud_current_layer = draw_layer_base;
	hud_layer_stack_depth = 0;
	hud_text_layer = draw_layer_overlay;
	hud_text_layer_stack_depth = 0;
}

void R_FlushImageDraw(void)
{
	R_PrepareImageDraw();

	if (hud.count > 0 && glConfig.initialized) {
		texture_ref currentTexture = null_texture_reference;
		r_image_type_t type = imagetype_image;
		int start = 0;
		int i;

		for (i = 0; i < hud.count; ++i) {
			qbool texture_changed = (R_TextureReferenceIsValid(currentTexture) &&
				R_TextureReferenceIsValid(hud.elements[i].texture) &&
				!R_TextureReferenceEqual(currentTexture, hud.elements[i].texture));

			if (i && (hud.elements[i].type != type || texture_changed)) {
				R_DrawElementRange(currentTexture, type, start, i - 1);
				start = i;
			}

			if (R_TextureReferenceIsValid(hud.elements[i].texture)) {
				currentTexture = hud.elements[i].texture;
			}
			type = hud.elements[i].type;
		}

		R_DrawElementRange(currentTexture, type, start, hud.count - 1);
	}

	R_EmptyImageQueue();
	hud.OnComplete();
}

qbool R_LogCustomImageType(r_image_type_t type, int index)
{
	if (hud.count >= MAX_2D_ELEMENTS) {
		return false;
	}

	hud.elements[hud.count].type = type;
	hud.elements[hud.count].index = index;
	hud.elements[hud.count].texture = null_texture_reference;
	hud.elements[hud.count].layer = hud_current_layer;
	hud.count++;
	return true;
}

qbool R_LogCustomImageTypeWithTexture(r_image_type_t type, int index, texture_ref texture)
{
	if (hud.count >= MAX_2D_ELEMENTS) {
		return false;
	}

	hud.elements[hud.count].type = type;
	hud.elements[hud.count].index = index;
	hud.elements[hud.count].texture = texture;
	hud.elements[hud.count].layer = hud_current_layer;
	hud.count++;
	return true;
}

void R_HudUndoLastElement(void)
{
	if (hud.count) {
		--hud.count;
	}
}
