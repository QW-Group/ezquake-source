/*
Copyright (C) 1996-2003 Id Software, Inc., A Nourai

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
#include "gl_model.h"
#include "glm_texture_arrays.h"
#include "r_sprite3d.h"
#include "r_texture.h"
#include "r_chaticons.h"
#include "r_state.h"
#include "r_matrix.h"

// Chat icons
typedef byte col_t[4]; // FIXME: why 4?

typedef struct ci_player_s {
	vec3_t		org;
	col_t		color;
	float		rotangle;
	float		size;
	byte		texindex;
	int			flags;
	float       distance;

	player_info_t *player;
} ci_player_t;

typedef enum {
	citex_chat,
	citex_afk,
	citex_chat_afk,
	num_citextures,
} ci_tex_t;

#define	MAX_CITEX_COMPONENTS		8
typedef struct ci_texture_s {
	texture_ref  texnum;
	texture_ref  tex_array;
	int          tex_index;
	int          components;
	float        coords[MAX_CITEX_COMPONENTS][4];
	float        originalCoords[MAX_CITEX_COMPONENTS][4];
} ci_texture_t;

#define TEXTURE_DETAILS(x) (R_UseModernOpenGL() ? x.tex_array : x.texnum),(x.tex_index)

extern cvar_t r_chaticons_alpha;

/**************************************** chat icon *****************************/
// qqshka: code is a mixture of autoid and particle engine

static ci_player_t ci_clients[MAX_CLIENTS];
static int ci_count;
static ci_texture_t ci_textures[num_citextures];
static qbool ci_initialized = false;

#define FONT_SIZE (256.0)

#define ADD_CICON_TEXTURE(_ptex, _texnum, _texindex, _components, _s1, _t1, _s2, _t2)	\
	do {																					\
		ci_textures[_ptex].texnum = _texnum;												\
		ci_textures[_ptex].components = _components;										\
		ci_textures[_ptex].coords[_texindex][0] = (_s1 + 1) / FONT_SIZE;					\
		ci_textures[_ptex].coords[_texindex][1] = (_t1 + 1) / FONT_SIZE;					\
		ci_textures[_ptex].coords[_texindex][2] = (_s2 - 1) / FONT_SIZE;					\
		ci_textures[_ptex].coords[_texindex][3] = (_t2 - 1) / FONT_SIZE;					\
		memcpy(ci_textures[_ptex].originalCoords, ci_textures[_ptex].coords, sizeof(ci_textures[_ptex].originalCoords)); \
	} while(0);

// FIXME: Almost duplicate of QMB_LoadTextureSubImage, extracting sprites from an atlas
//        This version works on different enumeration, and doesn't double-up for pre-multiplied alpha
static void R_LoadChatIconTextureSubImage(ci_tex_t tex, const char* id, const byte* pixels, byte* temp_buffer, int full_width, int full_height, int texIndex, int components, int sub_x, int sub_y, int sub_x2, int sub_y2)
{
	const int mode = TEX_ALPHA | TEX_COMPLAIN | TEX_NOSCALE;// | TEX_MIPMAP;
	texture_ref tex_ref;
	int y, x;
	int width = sub_x2 - sub_x;
	int height = sub_y2 - sub_y;

	for (y = 0; y < height; ++y) {
		const byte* source = pixels + ((sub_y + y) * full_width + sub_x) * 4;
		byte* target = temp_buffer + y * width * 4;

		for (x = 0; x < width; ++x, source += 4, target += 4) {
			target[0] = (byte)(((int)source[0] * (int)source[3]) / 255.0);
			target[1] = (byte)(((int)source[1] * (int)source[3]) / 255.0);
			target[2] = (byte)(((int)source[2] * (int)source[3]) / 255.0);
		}
	}

	tex_ref = R_LoadTexture(id, width, height, temp_buffer, mode, 4);

	ADD_CICON_TEXTURE(tex, tex_ref, 0, 1, 0, 0, FONT_SIZE, FONT_SIZE);
}

static void R_DrawChatIconBillboard(sprite3d_batch_id batch, ci_texture_t* _ptex, ci_player_t* _p, vec3_t _coord[4])
{
	float coordinates[4][4];
	r_sprite3d_vert_t* vert;
	int i;

	for (i = 0; i < 4; ++i) {
		VectorScale(_coord[i], _p->size, coordinates[i]);
		if (_p->rotangle) {
			R_RotateVector(coordinates[i], _p->rotangle, vpn[0], vpn[1], vpn[2]);
		}
		VectorAdd(coordinates[i], _p->org, coordinates[i]);
	}

	vert = R_Sprite3DAddEntry(batch, 4);
	if (vert) {
		R_Sprite3DSetVert(vert++, coordinates[0][0], coordinates[0][1], coordinates[0][2], _ptex->coords[_p->texindex][0], _ptex->coords[_p->texindex][3], _p->color, _ptex->tex_index);
		R_Sprite3DSetVert(vert++, coordinates[1][0], coordinates[1][1], coordinates[1][2], _ptex->coords[_p->texindex][0], _ptex->coords[_p->texindex][1], _p->color, _ptex->tex_index);
		R_Sprite3DSetVert(vert++, coordinates[3][0], coordinates[3][1], coordinates[3][2], _ptex->coords[_p->texindex][2], _ptex->coords[_p->texindex][3], _p->color, _ptex->tex_index);
		R_Sprite3DSetVert(vert++, coordinates[2][0], coordinates[2][1], coordinates[2][2], _ptex->coords[_p->texindex][2], _ptex->coords[_p->texindex][1], _p->color, _ptex->tex_index);
	}
}

static int CmpCI_Order(const void *p1, const void *p2)
{
	const ci_player_t	*a1 = (ci_player_t *)p1;
	const ci_player_t	*a2 = (ci_player_t *)p2;
	int l1 = a1->distance;
	int l2 = a2->distance;

	if (l1 > l2)
		return -1;
	if (l1 < l2)
		return  1;

	return 0;
}

void R_SetupChatIcons(void)
{
	int j, tracknum = -1;
	player_state_t *state;
	player_info_t *info;
	ci_player_t *id;
	centity_t *cent;

	ci_count = 0;

	if (!bound(0, r_chaticons_alpha.value, 1)) {
		return;
	}

	if (cls.state != ca_active || !cl.validsequence || cl.intermission) {
		return;
	}

	if (cl.spectator) {
		tracknum = Cam_TrackNum();
	}

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;
	cent = &cl_entities[1];

	for (j = 0; j < MAX_CLIENTS; j++, info++, state++, cent++) {
		vec3_t diff;
		float fade = 1;

		if (state->messagenum != cl.parsecount || j == cl.playernum || j == tracknum || info->spectator) {
			continue;
		}

		if (!info->chatflag) {
			continue; // user not chatting, so ignore
		}

		id = &ci_clients[ci_count];
		id->texindex = 0;
		id->player = info;

		id->org[0] = cent->lerp_origin[0];
		id->org[1] = cent->lerp_origin[1];
		id->org[2] = cent->lerp_origin[2] + 33; // move balloon up a bit
		VectorSubtract(id->org, r_refdef.vieworg, diff);
		id->distance = VectorLength(diff);

		if ((!cls.mvdplayback || Cam_TrackNum() >= 0) && cl.racing) {
			if (id->distance < KTX_RACING_PLAYER_MIN_DISTANCE) {
				continue; // too close, hide indicators
			}
			fade = min(id->distance, KTX_RACING_PLAYER_MAX_DISTANCE) / KTX_RACING_PLAYER_ALPHA_SCALE;
		}

		id->size = 8; // scale baloon
		id->rotangle = 5 * sin(2 * r_refdef2.time); // may be set to 0, if u dislike rolling
		id->color[0] = id->color[1] = id->color[2] = id->color[3] = 255 * bound(0, r_chaticons_alpha.value, 1) * fade; // pre-multiplied alpha

		id->flags = info->chatflag & (CIF_CHAT | CIF_AFK); // get known flags
		id->flags = (id->flags ? id->flags : CIF_CHAT); // use chat as default if we got some unknown "chat" value

		ci_count++;
	}

	if (ci_count) {
		// sort icons so we draw most far to you first
		qsort((void *)ci_clients, ci_count, sizeof(ci_clients[0]), CmpCI_Order);
	}
}

void R_InitChatIcons(void)
{
	int texmode = TEX_ALPHA | TEX_COMPLAIN | TEX_NOSCALE | TEX_MIPMAP;

	ci_initialized = false;

	{
		int real_width, real_height;
		byte* original;
		byte* temp_buffer;

		original = R_LoadImagePixels("textures/chaticons", 0, 0, texmode, &real_width, &real_height);
		if (!original) {
			return;
		}

		temp_buffer = Q_malloc(real_width * real_height * 4);
		R_LoadChatIconTextureSubImage(citex_chat, "ci:chat", original, temp_buffer, real_width, real_height, 0, 1, 0, 0, real_width / 4, real_height / 4);
		R_LoadChatIconTextureSubImage(citex_afk, "ci:afk", original, temp_buffer, real_width, real_height, 0, 1, real_width / 4, 0, real_width / 2, real_height / 4);
		R_LoadChatIconTextureSubImage(citex_chat_afk, "ci:chat-afk", original, temp_buffer, real_width, real_height, 0, 1, 0, 0, real_width / 2, real_height / 4);
		Q_free(temp_buffer);
		Q_free(original);
	}

	ci_initialized = true;
}

void R_DrawChatIcons(void)
{
	int	i, flags;
	vec3_t billboard[4], billboard2[4], vright_tmp;
	ci_player_t *p;

	if (!ci_initialized) {
		return;
	}

	if (!bound(0, r_chaticons_alpha.value, 1) || ci_count < 1) {
		return;
	}

	VectorAdd(vup, vright, billboard[2]);
	VectorSubtract(vright, vup, billboard[3]);
	VectorNegate(billboard[2], billboard[0]);
	VectorNegate(billboard[3], billboard[1]);

	VectorScale(vright, 2, vright_tmp);
	VectorAdd(vup, vright_tmp, billboard2[2]);
	VectorSubtract(vright_tmp, vup, billboard2[3]);
	VectorNegate(billboard2[2], billboard2[0]);
	VectorNegate(billboard2[3], billboard2[1]);

	R_Sprite3DInitialiseBatch(SPRITE3D_CHATICON_AFK_CHAT, r_state_chaticon, TEXTURE_DETAILS(ci_textures[citex_chat_afk]), r_primitive_triangle_strip);
	R_Sprite3DInitialiseBatch(SPRITE3D_CHATICON_AFK, r_state_chaticon, TEXTURE_DETAILS(ci_textures[citex_afk]), r_primitive_triangle_strip);
	R_Sprite3DInitialiseBatch(SPRITE3D_CHATICON_CHAT, r_state_chaticon, TEXTURE_DETAILS(ci_textures[citex_chat]), r_primitive_triangle_strip);

	for (i = 0; i < ci_count; i++) {
		p = &ci_clients[i];
		flags = p->flags;

		if ((flags & CIF_CHAT) && (flags & CIF_AFK)) {
			R_DrawChatIconBillboard(SPRITE3D_CHATICON_AFK_CHAT, &ci_textures[citex_chat_afk], p, billboard2);
		}
		else if (flags & CIF_CHAT) {
			R_DrawChatIconBillboard(SPRITE3D_CHATICON_CHAT, &ci_textures[citex_chat], p, billboard);
		}
		else if (flags & CIF_AFK) {
			R_DrawChatIconBillboard(SPRITE3D_CHATICON_AFK, &ci_textures[citex_afk], p, billboard);
		}
	}
}

void R_ImportChatIconTextureArrayReferences(texture_flag_t* texture_flags)
{
	ci_tex_t tex;
	int i;

	for (tex = 0; tex < num_citextures; ++tex) {
		if (R_TextureReferenceIsValid(ci_textures[tex].texnum)) {
			texture_array_ref_t* array_ref = &texture_flags[ci_textures[tex].texnum.index].array_ref[TEXTURETYPES_SPRITES];

			ci_textures[tex].tex_array = array_ref->ref;
			ci_textures[tex].tex_index = array_ref->index;
			for (i = 0; i < ci_textures[tex].components; ++i) {
				ci_textures[tex].coords[i][0] *= array_ref->scale_s;
				ci_textures[tex].coords[i][2] *= array_ref->scale_s;
				ci_textures[tex].coords[i][1] *= array_ref->scale_t;
				ci_textures[tex].coords[i][3] *= array_ref->scale_t;
			}
		}
	}
}

void R_FlagChatIconTexturesForArray(texture_flag_t* texture_flags)
{
	ci_tex_t tex;

	for (tex = 0; tex < num_citextures; ++tex) {
		if (R_TextureReferenceIsValid(ci_textures[tex].texnum)) {
			texture_flags[ci_textures[tex].texnum.index].flags |= (1 << TEXTURETYPES_SPRITES);
			memcpy(ci_textures[tex].coords, ci_textures[tex].originalCoords, sizeof(ci_textures[tex].coords));
		}
	}
}
