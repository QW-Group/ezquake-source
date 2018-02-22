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

// Alias model (.mdl) rendering, classic (immediate mode) GL only
// Most code taken from gl_rmain.c

#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"
#include "vx_stuff.h"
#include "vx_vertexlights.h"
#include "utils.h"
#include "qsound.h"
#include "hud.h"
#include "hud_common.h"
#include "gl_bloom.h"
#include "rulesets.h"
#include "teamplay.h"
#include "gl_aliasmodel.h"
#include "crc.h"

static void GLC_DrawAliasOutlineFrame(model_t* model, int pose1, int pose2);
static void GLC_DrawAliasShadow(aliashdr_t *paliashdr, int posenum, vec3_t shadevector, vec3_t lightspot);

// Which pose to use if shadow to be drawn
static int lastposenum;

extern float r_avertexnormals[NUMVERTEXNORMALS][3];

extern cvar_t    r_lerpframes;
extern cvar_t    gl_outline;
extern cvar_t    gl_outline_width;

extern float     r_framelerp;
extern float     r_lerpdistance;
extern qbool     full_light;
extern vec3_t    lightcolor;
extern float     apitch;
extern float     ayaw;

void GLC_DrawAliasFrame(model_t* model, int pose1, int pose2, qbool mtex, qbool scrolldir, texture_ref texture, texture_ref fb_texture, qbool outline, int effects)
{
	int *order, count;
	vec3_t interpolated_verts;
	float l, lerpfrac;
	trivertx_t *verts1, *verts2;
	//VULT COLOURED MODEL LIGHTS
	int i;
	vec3_t lc;
	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(model);

	GLC_StateBeginDrawAliasFrame(texture, fb_texture, mtex, r_modelalpha, custom_model);

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;

	verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	for ( ; ; ) {
		count = *order++;
		if (!count) {
			break;
		}

		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else {
			glBegin(GL_TRIANGLE_STRIP);
		}

		do {
			float color[4];

			// texture coordinates come from the draw list
			if (mtex) {
				qglMultiTexCoord2f(GL_TEXTURE0, ((float *)order)[0], ((float *)order)[1]);
				qglMultiTexCoord2f(GL_TEXTURE1, ((float *)order)[0], ((float *)order)[1]);
			}
			else {
				glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
			}

			order += 2;

			if ((currententity->renderfx & RF_LIMITLERP)) {
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;
			}

			// VULT VERTEX LIGHTING
			if (amf_lighting_vertex.value && !full_light) {
				l = VLight_LerpLight(verts1->lightnormalindex, verts2->lightnormalindex, lerpfrac, apitch, ayaw);
			}
			else {
				l = FloatInterpolate(shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]) / 127.0;
				l = (l * shadelight + ambientlight) / 256.0;
			}
			l = min(l, 1);

			//VULT COLOURED MODEL LIGHTS
			if (amf_lighting_colour.value && !full_light) {
				for (i = 0;i < 3;i++) {
					lc[i] = lightcolor[i] / 256 + l;
				}

				if (r_modelcolor[0] < 0) {
					// normal color
					VectorCopy(lc, color);
				}
				else {
					color[0] = r_modelcolor[0] * lc[0];
					color[1] = r_modelcolor[1] * lc[1];
					color[2] = r_modelcolor[2] * lc[2];
				}
			}
			else if (custom_model == NULL) {
				if (r_modelcolor[0] < 0) {
					color[0] = color[1] = color[2] = l;
				}
				else {
					color[0] = r_modelcolor[0] * l;
					color[1] = r_modelcolor[1] * l;
					color[2] = r_modelcolor[2] * l;
				}
			}
			else {
				color[0] = custom_model->color_cvar.color[0] / 255.0f;
				color[1] = custom_model->color_cvar.color[1] / 255.0f;
				color[2] = custom_model->color_cvar.color[2] / 255.0f;
			}

			color[0] *= r_modelalpha;
			color[1] *= r_modelalpha;
			color[2] *= r_modelalpha;
			color[3] = r_modelalpha;

			GL_Color4fv(color);

			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv(interpolated_verts);

			verts1++;
			verts2++;
		} while (--count);

		glEnd();
	}

	GLC_StateEndDrawAliasFrame();

	if (outline) {
		GLC_DrawAliasOutlineFrame(model, pose1, pose2);
	}
}

static void GLC_DrawAliasOutlineFrame(model_t* model, int pose1, int pose2)
{
	int *order, count;
	vec3_t interpolated_verts;
	float lerpfrac;
	trivertx_t* verts1;
	trivertx_t* verts2;
	aliashdr_t* paliashdr = (aliashdr_t*) Mod_Extradata(model);

	GL_StateBeginAliasOutlineFrame();

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;

	verts2 = verts1 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);

	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	for (;;) {
		count = *order++;

		if (!count) {
			break;
		}

		if (count < 0) {
			count = -count;
			glBegin(GL_TRIANGLE_FAN);
		}
		else {
			glBegin(GL_TRIANGLE_STRIP);
		}

		do {
			order += 2;

			if ((currententity->renderfx & RF_LIMITLERP))
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;

			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv(interpolated_verts);

			verts1++;
			verts2++;
		} while (--count);

		glEnd();
	}

	GL_StateEndAliasOutlineFrame();
}

void GLC_DrawPowerupShell(
	model_t* model, int effects, int layer_no,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame
)
{
	float base_level;
	float effect_level;
	int pose1 = R_AliasFramePose(oldframe);
	int pose2 = R_AliasFramePose(frame);
	qbool scrolldir = (layer_no == 1);
	trivertx_t* verts1;
	trivertx_t* verts2;
	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(model);
	float r_shellcolor[3];

	if (!GL_TextureReferenceIsValid(shelltexture)) {
		return;
	}

	base_level = bound(0, (layer_no == 0 ? gl_powerupshells_base1level.value : gl_powerupshells_base2level.value), 1);
	effect_level = bound(0, (layer_no == 0 ? gl_powerupshells_effect1level.value : gl_powerupshells_effect2level.value), 1);

	r_shellcolor[0] = base_level + ((effects & EF_RED) ? effect_level : 0);
	r_shellcolor[1] = base_level + ((effects & EF_GREEN) ? effect_level : 0);
	r_shellcolor[2] = base_level + ((effects & EF_BLUE) ? effect_level : 0);

	lastposenum = (r_framelerp >= 0.5) ? pose2 : pose1;
	verts1 = verts2 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	{
		int *order, count;
		float scroll[2];
		float v[3];
		float shell_size = bound(0, gl_powerupshells_size.value, 20);

		if (scrolldir) {
			scroll[0] = cos(cl.time * -0.5); // FIXME: cl.time ????
			scroll[1] = sin(cl.time * -0.5);
		}
		else {
			scroll[0] = cos(cl.time * 1.5);
			scroll[1] = sin(cl.time * 1.1);
		}

		// get the vertex count and primitive type
		order = (int *)((byte *)paliashdr + paliashdr->commands);
		for (;;) {
			GLenum drawMode = GL_TRIANGLE_STRIP;

			count = *order++;
			if (!count) {
				break;
			}

			if (count < 0) {
				count = -count;
				drawMode = GL_TRIANGLE_FAN;
			}

			// alpha so we can see colour underneath still
			GL_Color4f(r_shellcolor[0] * bound(0, gl_powerupshells.value, 1), r_shellcolor[1] * bound(0, gl_powerupshells.value, 1), r_shellcolor[2] * bound(0, gl_powerupshells.value, 1), bound(0, gl_powerupshells.value, 1));

			glBegin(drawMode);
			do {
				glTexCoord2f(((float *)order)[0] * 2.0f + scroll[0], ((float *)order)[1] * 2.0f + scroll[1]);

				order += 2;

				v[0] = r_avertexnormals[verts1->lightnormalindex][0] * shell_size + verts1->v[0];
				v[1] = r_avertexnormals[verts1->lightnormalindex][1] * shell_size + verts1->v[1];
				v[2] = r_avertexnormals[verts1->lightnormalindex][2] * shell_size + verts1->v[2];
				v[0] += r_framelerp * (r_avertexnormals[verts2->lightnormalindex][0] * shell_size + verts2->v[0] - v[0]);
				v[1] += r_framelerp * (r_avertexnormals[verts2->lightnormalindex][1] * shell_size + verts2->v[1] - v[1]);
				v[2] += r_framelerp * (r_avertexnormals[verts2->lightnormalindex][2] * shell_size + verts2->v[2] - v[2]);

				glVertex3f(v[0], v[1], v[2]);

				verts1++;
				verts2++;
			} while (--count);
			glEnd();
		}
	}
}

void GLC_UnderwaterCaustics(entity_t* ent, model_t* clmodel, maliasframedesc_t* oldframe, maliasframedesc_t* frame, aliashdr_t* paliashdr)
{
	// Underwater caustics on alias models of QRACK -->
#define GL_RGB_SCALE 0x8573

	if ((gl_caustics.value) && (GL_TextureReferenceIsValid(underwatertexture) && gl_mtexable && R_PointIsUnderwater(ent->origin))) {
		GLC_StateBeginUnderwaterCaustics();

		R_SetupAliasFrame(clmodel, oldframe, frame, true, false, false, underwatertexture, null_texture_reference, 0, 0);

		GLC_StateEndUnderwaterCaustics();
	}
	// <-- Underwater caustics on alias models of QRACK
}

void GLC_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr, vec3_t shadevector, vec3_t lightspot)
{
	float theta;
	float oldMatrix[16];
	static float shadescale = 0;

	if (!shadescale) {
		shadescale = 1 / sqrt(2);
	}
	theta = -ent->angles[1] / 180 * M_PI;

	VectorSet(shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

	GL_PushMatrix(GL_MODELVIEW, oldMatrix);
	glTranslatef(ent->origin[0], ent->origin[1], ent->origin[2]);
	glRotatef(ent->angles[1], 0, 0, 1);

	GLC_StateBeginAliasModelShadow();
	GLC_DrawAliasShadow(paliashdr, lastposenum, shadevector, lightspot);
	GLC_StateEndAliasModelShadow();

	GL_PopMatrix(GL_MODELVIEW, oldMatrix);
}

static void GLC_DrawAliasShadow(aliashdr_t *paliashdr, int posenum, vec3_t shadevector, vec3_t lightspot)
{
	int *order, count;
	vec3_t point;
	float lheight = currententity->origin[2] - lightspot[2], height = 1 - lheight;
	trivertx_t *verts;

	verts = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts += posenum * paliashdr->poseverts;
	order = (int *) ((byte *) paliashdr + paliashdr->commands);

	while ((count = *order++)) {
		// get the vertex count and primitive type
		if (count < 0) {
			count = -count;
			glBegin (GL_TRIANGLE_FAN);
		} else {
			glBegin (GL_TRIANGLE_STRIP);
		}

		do {
			//no texture for shadows
			order += 2;

			// normals and vertexes come from the frame list
			point[0] = verts->v[0] * paliashdr->scale[0] + paliashdr->scale_origin[0];
			point[1] = verts->v[1] * paliashdr->scale[1] + paliashdr->scale_origin[1];
			point[2] = verts->v[2] * paliashdr->scale[2] + paliashdr->scale_origin[2];

			point[0] -= shadevector[0] * (point[2] +lheight);
			point[1] -= shadevector[1] * (point[2] + lheight);
			point[2] = height;
			//height -= 0.001;
			glVertex3fv (point);

			verts++;
		} while (--count);

		glEnd ();
	}	
}
