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
#include "rulesets.h"
#include "teamplay.h"
#include "gl_aliasmodel.h"
#include "crc.h"
#include "r_matrix.h"
#include "r_state.h"
#include "r_texture.h"
#include "r_vao.h"
#include "glc_vao.h"
#include "r_brushmodel.h" // R_PointIsUnderwater only
#include "r_buffers.h"

static void GLC_DrawAliasOutlineFrame(entity_t* ent, model_t* model, int pose1, int pose2);
static void GLC_DrawAliasShadow(entity_t* ent, aliashdr_t *paliashdr, int posenum, vec3_t shadevector);
static void GLC_DrawCachedAliasOutlineFrame(model_t* model, GLenum primitive, int verts);

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

// Temporary r_lerpframes fix, unless we go for shaders-in-classic...
//   After all models loaded, this needs to be allocated enough space to hold a complete pose of verts
//   We fill it in with the lerped positions, then render model from there
// Ugly, but don't think there's another way to do this in fixed pipeline
typedef struct glc_aliasmodel_vert_s {
	float position[3];
	float texture_coords[2];
	byte color[4];
} glc_aliasmodel_vert_t;
static glc_aliasmodel_vert_t* temp_aliasmodel_buffer;
static int temp_aliasmodel_buffer_size;

static buffer_ref aliasmodel_pose_vbo;

static void GLC_ConfigureAliasModelState(void)
{
	extern cvar_t gl_vbo_clientmemory;

	if (gl_vbo_clientmemory.integer) {
		R_GenVertexArray(vao_aliasmodel);
		GLC_VAOEnableVertexPointer(vao_aliasmodel, 3, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].position[0]);
		GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 0, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].texture_coords[0]);
		GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 1, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].texture_coords[0]);
		GLC_VAOEnableColorPointer(vao_aliasmodel, 4, GL_UNSIGNED_BYTE, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].color[0]);
	}
	else {
		R_GenVertexArray(vao_aliasmodel);
		GLC_VAOSetVertexBuffer(vao_aliasmodel, aliasmodel_pose_vbo);
		GLC_VAOEnableVertexPointer(vao_aliasmodel, 3, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, position));
		GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 0, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, texture_coords));
		GLC_VAOEnableTextureCoordPointer(vao_aliasmodel, 1, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, texture_coords));
		GLC_VAOEnableColorPointer(vao_aliasmodel, 4, GL_UNSIGNED_BYTE, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, color));
	}

	R_GenVertexArray(vao_aliasmodel_powerupshell);
	GLC_VAOEnableVertexPointer(vao_aliasmodel_powerupshell, 3, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), &temp_aliasmodel_buffer[0].position);
	GLC_VAOEnableTextureCoordPointer(vao_aliasmodel_powerupshell, 0, 2, GL_FLOAT, sizeof(temp_aliasmodel_buffer[0]), VBO_FIELDOFFSET(glc_aliasmodel_vert_t, texture_coords));
}

void GLC_AllocateAliasPoseBuffer(void)
{
	int max_verts = 0;
	int i;

	for (i = 1; i < MAX_MODELS; ++i) {
		model_t* mod = cl.model_precache[i];

		if (mod && mod->type == mod_alias) {
			aliashdr_t* hdr = (aliashdr_t *)Mod_Extradata(mod);

			if (hdr) {
				max_verts = max(max_verts, hdr->vertsPerPose);
			}
		}
	}

	for (i = 0; i < MAX_VWEP_MODELS; i++) {
		model_t* mod = cl.vw_model_precache[i];

		if (mod && mod->type == mod_alias) {
			aliashdr_t* hdr = (aliashdr_t *)Mod_Extradata(mod);

			if (hdr) {
				max_verts = max(max_verts, hdr->vertsPerPose);
			}
		}
	}

	if (temp_aliasmodel_buffer == NULL || temp_aliasmodel_buffer_size < max_verts) {
		Q_free(temp_aliasmodel_buffer);
		temp_aliasmodel_buffer = Q_malloc(sizeof(temp_aliasmodel_buffer[0]) * max_verts);
		temp_aliasmodel_buffer_size = max_verts;
	}

	if (GL_BufferReferenceIsValid(aliasmodel_pose_vbo)) {
		aliasmodel_pose_vbo = buffers.Resize(aliasmodel_pose_vbo, sizeof(temp_aliasmodel_buffer[0]) * max_verts, NULL);
	}
	else {
		aliasmodel_pose_vbo = buffers.Create(buffertype_vertex, "glc-alias-pose", sizeof(temp_aliasmodel_buffer[0]) * max_verts, NULL, bufferusage_reuse_per_frame);
	}

	GLC_ConfigureAliasModelState();
}

void GLC_FreeAliasPoseBuffer(void)
{
	Q_free(temp_aliasmodel_buffer);
	temp_aliasmodel_buffer_size = 0;
}

void GLC_DrawAliasFrame(entity_t* ent, model_t* model, int pose1, int pose2, qbool mtex, qbool scrolldir, texture_ref texture, texture_ref fb_texture, qbool outline, int effects, qbool alpha_blend)
{
	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(model);
	qbool cache = buffers.supported && temp_aliasmodel_buffer_size >= paliashdr->poseverts;
	int position = 0;
	GLenum primitive = GL_TRIANGLE_STRIP;

	int *order, count;
	vec3_t interpolated_verts;
	float l, lerpfrac;
	trivertx_t *verts1, *verts2;
	//VULT COLOURED MODEL LIGHTS
	int i;
	vec3_t lc;

	GLC_StateBeginDrawAliasFrame(texture, fb_texture, mtex, alpha_blend || r_modelalpha < 1, custom_model, ent->renderfx & RF_WEAPONMODEL);

	lerpfrac = r_framelerp;
	lastposenum = (lerpfrac >= 0.5) ? pose2 : pose1;

	verts2 = verts1 = (trivertx_t *) ((byte *) paliashdr + paliashdr->posedata);
	verts1 += pose1 * paliashdr->poseverts;
	verts2 += pose2 * paliashdr->poseverts;

	order = (int *)((byte *)paliashdr + paliashdr->commands);

	for (; ; ) {
		count = *order++;
		if (!count) {
			break;
		}

		if (count < 0) {
			primitive = GL_TRIANGLE_FAN;
			count = -count;
		}
		else {
			primitive = GL_TRIANGLE_STRIP;
		}

		if (!cache) {
			glBegin(primitive);
		}

		do {
			float color[4];

			// texture coordinates come from the draw list
			if (cache) {
				temp_aliasmodel_buffer[position].texture_coords[0] = ((float *)order)[0];
				temp_aliasmodel_buffer[position].texture_coords[1] = ((float *)order)[1];
			}
			else if (mtex) {
				qglMultiTexCoord2f(GL_TEXTURE0, ((float *)order)[0], ((float *)order)[1]);
				qglMultiTexCoord2f(GL_TEXTURE1, ((float *)order)[0], ((float *)order)[1]);
			}
			else {
				glTexCoord2f(((float *)order)[0], ((float *)order)[1]);
			}

			order += 2;

			if ((ent->renderfx & RF_LIMITLERP)) {
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;
			}
			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);

			// VULT VERTEX LIGHTING
			if (amf_lighting_vertex.integer && !full_light) {
				l = VLight_LerpLight(verts1->lightnormalindex, verts2->lightnormalindex, lerpfrac, ent->angles[0], ent->angles[1]);
			}
			else {
				l = FloatInterpolate(shadedots[verts1->lightnormalindex], lerpfrac, shadedots[verts2->lightnormalindex]) / 127.0;
				l = (l * shadelight + ambientlight) / 256.0;
			}
			l = min(l, 1);

			//VULT COLOURED MODEL LIGHTS
			if (amf_lighting_colour.integer && !full_light) {
				for (i = 0; i < 3; i++) {
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

			if (cache) {
				VectorCopy(interpolated_verts, temp_aliasmodel_buffer[position].position);
				temp_aliasmodel_buffer[position].color[0] = color[0] * 255;
				temp_aliasmodel_buffer[position].color[1] = color[1] * 255;
				temp_aliasmodel_buffer[position].color[2] = color[2] * 255;
				temp_aliasmodel_buffer[position].color[3] = color[3] * 255;

				++position;
			}
			else {
				R_CustomColor(color[0], color[1], color[2], color[3]);
				glVertex3fv(interpolated_verts);
			}

			verts1++;
			verts2++;
		} while (--count);

		if (cache) {
			buffers.Update(aliasmodel_pose_vbo, sizeof(temp_aliasmodel_buffer[0]) * position, temp_aliasmodel_buffer);
			GL_DrawArrays(primitive, 0, position);
		}
		else {
			glEnd();
		}
	}

	if (outline) {
		if (cache) {
			GLC_DrawCachedAliasOutlineFrame(model, primitive, position);
		}
		else {
			GLC_DrawAliasOutlineFrame(ent, model, pose1, pose2);
		}
	}
}

static void GLC_DrawCachedAliasOutlineFrame(model_t* model, GLenum primitive, int verts)
{
	GLC_StateBeginAliasOutlineFrame();

	GL_DrawArrays(primitive, 0, verts);
}

static void GLC_DrawAliasOutlineFrame(entity_t* ent, model_t* model, int pose1, int pose2)
{
	int *order, count;
	vec3_t interpolated_verts;
	float lerpfrac;
	trivertx_t* verts1;
	trivertx_t* verts2;
	aliashdr_t* paliashdr = (aliashdr_t*) Mod_Extradata(model);

	GLC_StateBeginAliasOutlineFrame();

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

			if ((ent->renderfx & RF_LIMITLERP))
				lerpfrac = VectorL2Compare(verts1->v, verts2->v, r_lerpdistance) ? r_framelerp : 1;

			VectorInterpolate(verts1->v, lerpfrac, verts2->v, interpolated_verts);
			glVertex3fv(interpolated_verts);

			verts1++;
			verts2++;
		} while (--count);

		glEnd();
	}
}

static void GLC_SetPowerupShellColor(int layer_no, int effects)
{
	// set color: alpha so we can see colour underneath still
	float r_shellcolor[3];
	float base_level;
	float effect_level;

	base_level = bound(0, (layer_no == 0 ? gl_powerupshells_base1level.value : gl_powerupshells_base2level.value), 1);
	effect_level = bound(0, (layer_no == 0 ? gl_powerupshells_effect1level.value : gl_powerupshells_effect2level.value), 1);
	r_shellcolor[0] = base_level + ((effects & EF_RED) ? effect_level : 0);
	r_shellcolor[1] = base_level + ((effects & EF_GREEN) ? effect_level : 0);
	r_shellcolor[2] = base_level + ((effects & EF_BLUE) ? effect_level : 0);
	R_CustomColor(r_shellcolor[0] * bound(0, gl_powerupshells.value, 1), r_shellcolor[1] * bound(0, gl_powerupshells.value, 1), r_shellcolor[2] * bound(0, gl_powerupshells.value, 1), bound(0, gl_powerupshells.value, 1));
}

void GLC_DrawPowerupShell(
	model_t* model, int effects,
	maliasframedesc_t *oldframe, maliasframedesc_t *frame
)
{
	int pose1 = R_AliasFramePose(oldframe);
	int pose2 = R_AliasFramePose(frame);
	trivertx_t* verts1;
	trivertx_t* verts2;
	aliashdr_t* paliashdr = (aliashdr_t*)Mod_Extradata(model);
	int layer_no;
	int *order, count;
	float scroll[4];
	float v[3];
	float shell_size = bound(0, gl_powerupshells_size.value, 20);
	qbool cache = buffers.supported && temp_aliasmodel_buffer_size >= paliashdr->poseverts;
	int position = 0;

	if (!GL_TextureReferenceIsValid(shelltexture)) {
		return;
	}

	lastposenum = (r_framelerp >= 0.5) ? pose2 : pose1;

	scroll[0] = cos(cl.time * 1.5);
	scroll[1] = sin(cl.time * 1.1);
	scroll[2] = cos(cl.time * -0.5);
	scroll[3] = sin(cl.time * -0.5);

	for (layer_no = 0; layer_no <= 1; ++layer_no) {
		GLC_SetPowerupShellColor(layer_no, effects);

		// get the vertex count and primitive type
		order = (int *)((byte *)paliashdr + paliashdr->commands);
		verts1 = verts2 = (trivertx_t *)((byte *)paliashdr + paliashdr->posedata);
		verts1 += pose1 * paliashdr->poseverts;
		verts2 += pose2 * paliashdr->poseverts;

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

			if (!cache) {
				glBegin(drawMode);
			}

			do {
				float s = ((float *)order)[0] * 2.0f + scroll[layer_no * 2];
				float t = ((float *)order)[1] * 2.0f + scroll[layer_no * 2 + 1];

				order += 2;

				v[0] = r_avertexnormals[verts1->lightnormalindex][0] * shell_size + verts1->v[0];
				v[1] = r_avertexnormals[verts1->lightnormalindex][1] * shell_size + verts1->v[1];
				v[2] = r_avertexnormals[verts1->lightnormalindex][2] * shell_size + verts1->v[2];
				v[0] += r_framelerp * (r_avertexnormals[verts2->lightnormalindex][0] * shell_size + verts2->v[0] - v[0]);
				v[1] += r_framelerp * (r_avertexnormals[verts2->lightnormalindex][1] * shell_size + verts2->v[1] - v[1]);
				v[2] += r_framelerp * (r_avertexnormals[verts2->lightnormalindex][2] * shell_size + verts2->v[2] - v[2]);

				if (cache) {
					temp_aliasmodel_buffer[position].texture_coords[0] = s;
					temp_aliasmodel_buffer[position].texture_coords[1] = t;
					VectorCopy(v, temp_aliasmodel_buffer[position].position);
					position++;
				}
				else {
					glTexCoord2f(s, t);
					glVertex3f(v[0], v[1], v[2]);
				}

				verts1++;
				verts2++;
			} while (--count);

			if (cache) {
				int i;

				R_BindVertexArray(vao_aliasmodel_powerupshell);

				GL_DrawArrays(drawMode, 0, position);

				// And quickly update texture-coordinates and run again...
				GLC_SetPowerupShellColor(1, effects);
				for (i = 0; i < position; ++i) {
					temp_aliasmodel_buffer[i].texture_coords[0] += scroll[2] - scroll[0];
					temp_aliasmodel_buffer[i].texture_coords[1] += scroll[3] - scroll[1];
				}
				GL_DrawArrays(drawMode, 0, position);
				++layer_no;
			}
			else {
				glEnd();
			}
		}
	}
}

void GLC_UnderwaterCaustics(entity_t* ent, model_t* clmodel, maliasframedesc_t* oldframe, maliasframedesc_t* frame, aliashdr_t* paliashdr)
{
	// Underwater caustics on alias models of QRACK -->
	if (gl_caustics.integer && (GL_TextureReferenceIsValid(underwatertexture) && gl_mtexable && R_PointIsUnderwater(ent->origin))) {
		GLC_StateBeginUnderwaterCaustics();

		R_SetupAliasFrame(ent, clmodel, oldframe, frame, true, false, false, underwatertexture, null_texture_reference, 0, 0);

		GLC_StateEndUnderwaterCaustics();
	}
	// <-- Underwater caustics on alias models of QRACK
}

void GLC_AliasModelShadow(entity_t* ent, aliashdr_t* paliashdr)
{
	float theta;
	float oldMatrix[16];
	static float shadescale = 0;
	vec3_t shadevector;

	if (!shadescale) {
		shadescale = 1 / sqrt(2);
	}
	theta = -ent->angles[1] / 180 * M_PI;

	VectorSet(shadevector, cos(theta) * shadescale, sin(theta) * shadescale, shadescale);

	GL_PushModelviewMatrix(oldMatrix);
	GL_TranslateModelview(ent->origin[0], ent->origin[1], ent->origin[2]);
	GL_RotateModelview(ent->angles[1], 0, 0, 1);

	GLC_StateBeginAliasModelShadow();
	GLC_DrawAliasShadow(ent, paliashdr, lastposenum, shadevector);

	GL_PopModelviewMatrix(oldMatrix);
}

static void GLC_DrawAliasShadow(entity_t* ent, aliashdr_t *paliashdr, int posenum, vec3_t shadevector)
{
	extern vec3_t lightspot;
	int *order, count;
	vec3_t point;
	float lheight = ent->origin[2] - lightspot[2], height = 1 - lheight;
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
