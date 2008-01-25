/*
Copyright (C) 2001-2002 A Nourai

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

	$Id: fmod.c,v 1.12 2007-09-14 13:29:28 disconn3ct Exp $
*/

#include "quakedef.h"
#include "fmod.h"
#include "sha1.h"

static const int progs_armor_mdl_FMOD_DM_FMOD_TF[5]				= {0x18d1b8ef, 0xfe43d973, 0xbb104313, 0xef6a9090, 0x2b048696};
static const int progs_backpack_mdl_FMOD_DM_FMOD_TF[5]			= {0x80cedaeb, 0x18f7d282, 0xa911e7c2, 0xffe609aa, 0x050a6005};
static const int progs_bolt2_mdl_FMOD_DM_FMOD_TF[5]				= {0x14fba8de, 0x99237ae4, 0xc8806043, 0xebeea454, 0x024a99a9};
static const int progs_end1_mdl_FMOD_DM_FMOD_TF[5]				= {0xd1fed19f, 0x5d67c632, 0x1d72a0e6, 0x14ab39d7, 0x4bf435c4};
static const int progs_end2_mdl_FMOD_DM_FMOD_TF[5]				= {0x98eee620, 0x107acd79, 0x8331620d, 0x2a9a1848, 0x64d29e1a};
static const int progs_end3_mdl_FMOD_DM_FMOD_TF[5]				= {0x7f1e336d, 0xe04f4bb0, 0x4ac25a1a, 0x2caedde7, 0x09973319};
static const int progs_end4_mdl_FMOD_DM_FMOD_TF[5]				= {0xa4b572aa, 0xdd00c38d, 0x130f8da2, 0x797f0e55, 0xcca52b76};
static const int progs_eyes_mdl_FMOD_DM_FMOD_TF[5]				= {0x276f25a8, 0xc6b8f782, 0x3ef75d52, 0x911d163e, 0x5d79e557};
static const int progs_g_light_mdl_FMOD_DM_FMOD_TF[5]			= {0xcb9b9366, 0x1fd74d91, 0x9a6a82f6, 0x5d6b2e3e, 0xc458f5ac};
static const int progs_g_nail_mdl_FMOD_DM_FMOD_TF[5]			= {0x7b209f71, 0xa77cd00f, 0x91f24953, 0x932f264b, 0x350e7440};
static const int progs_g_nail2_mdl_FMOD_DM_FMOD_TF[5]			= {0x97cb4499, 0x88e3e11a, 0xecec6eca, 0x883f6c8f, 0xac7ecf0c};
static const int progs_g_rock_mdl_FMOD_DM_FMOD_TF[5]			= {0x9017dd83, 0x150d954c, 0x9b5f4544, 0x10843b72, 0x4697068e};
static const int progs_g_rock2_mdl_FMOD_DM_FMOD_TF[5]			= {0x5a47ec20, 0xd0211cdc, 0xd6b8af60, 0xaf813eab, 0xba330b5b};
static const int progs_g_shot_mdl_FMOD_DM_FMOD_TF[5]			= {0x35a735e1, 0xc1ee6090, 0x9f8940b5, 0x6cdefd1c, 0x7eec1d67};
static const int progs_gib1_mdl_FMOD_DM_FMOD_TF[5]				= {0x99dc9ea4, 0x6e9bf74a, 0x25710a1e, 0x701fc77b, 0x09777092};
static const int progs_gib2_mdl_FMOD_DM_FMOD_TF[5]				= {0x5ac3e29b, 0xd66358d9, 0x1044d27a, 0xb3da48ad, 0x5f1e9fbb};
static const int progs_gib3_mdl_FMOD_DM_FMOD_TF[5]				= {0xc6558e61, 0x13ea4f63, 0x20c9da45, 0x0640212e, 0x7b98f350};
static const int progs_grenade_mdl_FMOD_DM_FMOD_TF[5]			= {0x60dfffb8, 0xfc871f0c, 0xd9f3c325, 0x61aadcaf, 0x0ec37cbf};
static const int progs_invisibl_mdl_FMOD_DM_FMOD_TF[5]			= {0x23e2f389, 0x8479657f, 0x437e0d25, 0xee100bae, 0xbad6a775};
static const int progs_invulner_mdl_FMOD_DM_FMOD_TF[5]			= {0x987ee175, 0xfd0d4f35, 0x06d2641c, 0x725c0dc8, 0x871f537a};
static const int progs_missile_mdl_FMOD_DM_FMOD_TF[5]			= {0x9adfeee8, 0x185872c1, 0xb3bb36f8, 0x996e29ab, 0xd46ab2a9};
static const int progs_quaddama_mdl_FMOD_DM_FMOD_TF[5]			= {0x2760f663, 0x32dc8405, 0x057563df, 0x9614c3a7, 0x0125949b};
static const int progs_s_spike_mdl_FMOD_DM_FMOD_TF[5]			= {0xf308f8e5, 0xcdc242e2, 0x4f71b01f, 0xafb9880a, 0x52198e9f};
static const int progs_spike_mdl_FMOD_DM_FMOD_TF[5]				= {0xebd9adaf, 0xfb3b2f28, 0x67cc2c34, 0x926ec21a, 0x09e1a233};
static const int progs_suit_mdl_FMOD_DM_FMOD_TF[5]				= {0xb7dcb9dd, 0xed8da03b, 0x416efc5e, 0x8ee38d5a, 0x4063bf25};
static const int progs_player_mdl_FMOD_DM[5]					= {0x95ca0ab4, 0x021be62e, 0x6655e9a5, 0xd4a7ef1c, 0xb484582f};
static const int progs_player_mdl_FMOD_TF[5]					= {0x8fcab658, 0x027a97ef, 0xa96eee3d, 0xc1e44f46, 0x18a533c9};
static const int progs_tf_flag_mdl_FMOD_TF[5]					= {0x6e969df0, 0xc91b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_turrbase_mdl_FMOD_TF[5]					= {0x0cda73d5, 0x822703cb, 0x9e6cbe6b, 0x94fb95f7, 0xc7c04bed};
static const int progs_turrgun_mdl_FMOD_TF[5]					= {0x314b126f, 0x7d8d7cb5, 0x8df185c8, 0x1f3662f7, 0xa272957b};
static const int progs_disp_mdl_FMOD_TF[5]						= {0x5596fa4a, 0xd94e2457, 0x140f84df, 0x5859dda8, 0xe96752fc};
static const int progs_tf_stan_mdl_FMOD_TF[5]					= {0x1b732a1b, 0x763069c2, 0xe779e270, 0x252b9daa, 0xddf7db2a};
static const int progs_hgren2_mdl_FMOD_TF[5]					= {0x1dc3715f, 0x58c53c0f, 0xdb1504fc, 0xab354bdf, 0x6aacee97};
static const int progs_grenade2_mdl_FMOD_TF[5]					= {0x4b116cef, 0xebc6f074, 0x2acdea73, 0x876a8716, 0xe5e7409d};
static const int progs_detpack_mdl_FMOD_TF[5]					= {0xaf73d4e1, 0x0c98dc38, 0x6d3983c0, 0xd7a70033, 0x616a675e};
static const int progs_s_bubble_spr_FMOD_DM_FMOD_TF[5]			= {0x3dbefaf2, 0x73b38654, 0x70910774, 0xdfe285f3, 0x4db5d90e};
static const int progs_s_explod_spr_FMOD_DM_FMOD_TF[5]			= {0x295a7b06, 0xacdf1f88, 0x50a20894, 0x5c75905f, 0xb9d95d0b};
static const int maps_b_bh100_bsp_FMOD_DM_FMOD_TF[5]			= {0x65e7e302, 0x94a85b4d, 0x8092e674, 0xf700e5f0, 0xde667fcc};
static const int sound_buttons_airbut1_wav_FMOD_DM_FMOD_TF[5]	= {0x47d5141e, 0xc925e8eb, 0x26e5583c, 0xc8dfd021, 0x226792ef};
static const int sound_items_armor1_wav_FMOD_DM_FMOD_TF[5]		= {0x44488db0, 0xef0b0a1d, 0x3acda8b4, 0x3d87b467, 0xe4dd4fcc};
static const int sound_items_damage_wav_FMOD_DM_FMOD_TF[5]		= {0x197778ac, 0x92a252c5, 0x90c30256, 0x7ba6f264, 0x56ab654f};
static const int sound_items_damage2_wav_FMOD_DM_FMOD_TF[5]		= {0x719a9b4d, 0x1c76b218, 0xfcbf979f, 0x5ea5e6fa, 0x68afda0e};
static const int sound_items_damage3_wav_FMOD_DM_FMOD_TF[5]		= {0xfa07de6c, 0x6e306d39, 0x7d18f3ed, 0x90272558, 0x63d01e7a};
static const int sound_items_health1_wav_FMOD_DM_FMOD_TF[5]		= {0xebdd17b2, 0xa1289b80, 0x7810e3f2, 0xdb8d01bf, 0x0b495a96};
static const int sound_items_inv1_wav_FMOD_DM_FMOD_TF[5]		= {0xa01240b8, 0xb588d430, 0xc62406e0, 0x39e59dfd, 0xad5a4b98};
static const int sound_items_inv2_wav_FMOD_DM_FMOD_TF[5]		= {0x9ab6025d, 0x2d9d24a2, 0xc127b17c, 0x019e908a, 0xa521f7d3};
static const int sound_items_inv3_wav_FMOD_DM_FMOD_TF[5]		= {0xfb785777, 0x9c622826, 0x6121d05c, 0xf42d5661, 0xa34a5429};
static const int sound_items_itembk2_wav_FMOD_DM_FMOD_TF[5]		= {0x178c519b, 0x3b030527, 0x759aaed2, 0xf7dca7d7, 0xf01a1e36};
static const int sound_player_land_wav_FMOD_DM_FMOD_TF[5]		= {0xb8bb5e41, 0xf087ad8e, 0x13323cd5, 0x2e2d2052, 0x338e9e38};
static const int sound_player_land2_wav_FMOD_DM_FMOD_TF[5]		= {0x6c13b55c, 0xc46c1589, 0xeeabac42, 0x087cd24e, 0xd9552386};
static const int sound_misc_outwater_wav_FMOD_DM_FMOD_TF[5]		= {0x9cb6fc36, 0x9c20e9ba, 0x595f8418, 0x50e76d9f, 0xa7503dfd};
static const int sound_weapons_pkup_wav_FMOD_DM_FMOD_TF[5]		= {0x05a43523, 0x09bbab60, 0x77ce67a0, 0xb52fe23d, 0xf2715701};
static const int sound_player_plyrjmp8_wav_FMOD_DM_FMOD_TF[5]	= {0x75e2e946, 0xc9ad54f2, 0x6cd47b03, 0xeef0c9d4, 0x5d3386da};
static const int sound_items_protect_wav_FMOD_DM_FMOD_TF[5]		= {0x38ff1351, 0x7d6799c7, 0x7577256d, 0xbf233192, 0xb6223baa};
static const int sound_items_protect2_wav_FMOD_DM_FMOD_TF[5]	= {0x588dd159, 0xae489f2c, 0x22f7118e, 0x487eeb4c, 0x3c54c997};
static const int sound_items_protect3_wav_FMOD_DM_FMOD_TF[5]	= {0x869a1143, 0xc9e6a404, 0xcbd5d90e, 0xa1b57e4c, 0xae77dd20};
static const int sound_misc_water1_wav_FMOD_DM_FMOD_TF[5]		= {0x8625dbc7, 0xbaf30ae6, 0xfdb53965, 0x2d7eb9d6, 0xf9fd9304};
static const int sound_misc_water2_wav_FMOD_DM_FMOD_TF[5]		= {0xccda75ec, 0xfb5cd780, 0xe2d73b5b, 0x9f3560ad, 0x4e116b85};
static const int gfx_colormap_lmp_FMOD_DM_FMOD_TF[5]			= {0xa93b3795, 0x17016397, 0x0a761d38, 0x4866e67d, 0x3c2a2a75};
static const int gfx_palette_lmp_FMOD_DM_FMOD_TF[5]				= {0xa6a2e242, 0xbad0f7da, 0xe263351f, 0x6d51f8ad, 0x49a45d4a};

#define MAX_CHECK_MODELS 128
#define	FMOD_DM 1
#define FMOD_TF 2

typedef struct check_models_s {
	char		name[MAX_QPATH];
	qbool		modified;
	qbool		checked;
	int			flags;
	const void*	hash;
} check_models_t;

static check_models_t check_models[MAX_CHECK_MODELS];
static int check_models_num = 0;
static float fmod_warn_time = 0.0f;

static qbool FMod_IsModelModified (const char *name, const int flags, const byte *buf, const size_t len)
{
	int i;
	unsigned char hash[DIGEST_SIZE];
	SHA1_CTX context;


	SHA1Init (&context);
	SHA1Update (&context, (unsigned char *) buf, len);
	SHA1Final (hash, &context);

	for (i = 0; i < check_models_num; i++)
		if (!memcmp (name, check_models[i].name, min (strlen (name), strlen (check_models[i].name))))
			if (check_models[i].flags == flags)
				break;

	return (!memcmp (check_models[i].hash, hash, DIGEST_SIZE)) ? false : true;
}

static void FMod_AddModel (const char *name, const qbool flags, const void *hash)
{
	if (check_models_num >= MAX_CHECK_MODELS)
		return;

	strlcpy (check_models[check_models_num].name, name, sizeof (check_models[check_models_num].name));	
	check_models[check_models_num].checked = check_models[check_models_num].modified = false;
	check_models[check_models_num].flags = flags;
	check_models[check_models_num].hash = hash;
	check_models_num++;
}

void FMod_CheckModel (const char *name, const void *buf, const size_t len)
{
	int i;
	qbool modified, relevent;

	for (i = 0; i < MAX_CHECK_MODELS && i < check_models_num; i++) {
		relevent = (cl.teamfortress && (check_models[i].flags & FMOD_TF)) ||
			(!cl.teamfortress && (check_models[i].flags & FMOD_DM));

		if (relevent && !strcasecmp (name, check_models[i].name))
			break;
	}

	if (i >= check_models_num)
		return;

	modified = FMod_IsModelModified (name, check_models[i].flags, buf, len);

	if (check_models[i].checked && !check_models[i].modified && modified) {
		if (!fmod_warn_time || (float) cls.realtime - fmod_warn_time >= 3.0f) {
			Cbuf_AddText("say warning: models changed !!  Use f_modified again\n");
			fmod_warn_time = (float) cls.realtime;
		}
	}

	check_models[i].modified = modified;
	check_models[i].checked = true;
}

void FMod_Init (void)
{
	Cmd_AddCommand("f_modified", FMod_Response);

	memset (check_models, 0, sizeof (check_models));

	FMod_AddModel ("progs/armor.mdl",			FMOD_DM | FMOD_TF,	progs_armor_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/backpack.mdl",		FMOD_DM | FMOD_TF,	progs_backpack_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/bolt2.mdl",			FMOD_DM | FMOD_TF,	progs_bolt2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/end1.mdl",			FMOD_DM | FMOD_TF,	progs_end1_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/end2.mdl",			FMOD_DM | FMOD_TF,	progs_end2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/end3.mdl",			FMOD_DM | FMOD_TF,	progs_end3_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/end4.mdl",			FMOD_DM | FMOD_TF,	progs_end4_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/eyes.mdl",			FMOD_DM | FMOD_TF,	progs_eyes_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/g_light.mdl",			FMOD_DM | FMOD_TF,	progs_g_light_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/g_nail.mdl",			FMOD_DM | FMOD_TF,	progs_g_nail_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/g_nail2.mdl",			FMOD_DM | FMOD_TF,	progs_g_nail2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/g_rock.mdl",			FMOD_DM | FMOD_TF,	progs_g_rock_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/g_rock2.mdl",			FMOD_DM | FMOD_TF,	progs_g_rock2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/g_shot.mdl",			FMOD_DM | FMOD_TF,	progs_g_shot_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/gib1.mdl",			FMOD_DM | FMOD_TF,	progs_gib1_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/gib2.mdl",			FMOD_DM | FMOD_TF,	progs_gib2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/gib3.mdl",			FMOD_DM | FMOD_TF,	progs_gib3_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/grenade.mdl",			FMOD_DM | FMOD_TF,	progs_grenade_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/invisibl.mdl",		FMOD_DM | FMOD_TF,	progs_invisibl_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/invulner.mdl",		FMOD_DM | FMOD_TF,	progs_invulner_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/missile.mdl",			FMOD_DM | FMOD_TF,	progs_missile_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/quaddama.mdl",		FMOD_DM | FMOD_TF,	progs_quaddama_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/s_spike.mdl",			FMOD_DM | FMOD_TF,	progs_s_spike_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/spike.mdl",			FMOD_DM | FMOD_TF,	progs_spike_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/suit.mdl",			FMOD_DM | FMOD_TF,	progs_suit_mdl_FMOD_DM_FMOD_TF);

	FMod_AddModel ("progs/player.mdl",			FMOD_DM,			progs_player_mdl_FMOD_DM);
	FMod_AddModel ("progs/player.mdl",			FMOD_TF,			progs_player_mdl_FMOD_TF);

	FMod_AddModel ("progs/tf_flag.mdl",			FMOD_TF,			progs_tf_flag_mdl_FMOD_TF);
	FMod_AddModel ("progs/turrbase.mdl",		FMOD_TF,			progs_turrbase_mdl_FMOD_TF);
	FMod_AddModel ("progs/turrgun.mdl",			FMOD_TF,			progs_turrgun_mdl_FMOD_TF);
	FMod_AddModel ("progs/disp.mdl",			FMOD_TF,			progs_disp_mdl_FMOD_TF);
	FMod_AddModel ("progs/tf_stan.mdl",			FMOD_TF,			progs_tf_stan_mdl_FMOD_TF);
	FMod_AddModel ("progs/hgren2.mdl",			FMOD_TF,			progs_hgren2_mdl_FMOD_TF); // gren1
	FMod_AddModel ("progs/grenade2.mdl",		FMOD_TF,			progs_grenade2_mdl_FMOD_TF); // gren2
	FMod_AddModel ("progs/detpack.mdl",			FMOD_TF,			progs_detpack_mdl_FMOD_TF); // detpack

	FMod_AddModel ("progs/s_bubble.spr",		FMOD_DM | FMOD_TF,	progs_s_bubble_spr_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/s_explod.spr",		FMOD_DM | FMOD_TF,	progs_s_explod_spr_FMOD_DM_FMOD_TF);

	FMod_AddModel ("maps/b_bh100.bsp",			FMOD_DM | FMOD_TF,	maps_b_bh100_bsp_FMOD_DM_FMOD_TF);

	FMod_AddModel ("sound/buttons/airbut1.wav",	FMOD_DM | FMOD_TF,	sound_buttons_airbut1_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/armor1.wav",	FMOD_DM | FMOD_TF,	sound_items_armor1_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/damage.wav",	FMOD_DM | FMOD_TF,	sound_items_damage_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/damage2.wav",	FMOD_DM | FMOD_TF,	sound_items_damage2_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/damage3.wav",	FMOD_DM | FMOD_TF,	sound_items_damage3_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/health1.wav",	FMOD_DM | FMOD_TF,	sound_items_health1_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/inv1.wav",		FMOD_DM | FMOD_TF,	sound_items_inv1_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/inv2.wav",		FMOD_DM | FMOD_TF,	sound_items_inv2_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/inv3.wav",		FMOD_DM | FMOD_TF,	sound_items_inv3_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/itembk2.wav",	FMOD_DM | FMOD_TF,	sound_items_itembk2_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/player/land.wav",		FMOD_DM | FMOD_TF,	sound_player_land_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/player/land2.wav",	FMOD_DM | FMOD_TF,	sound_player_land2_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/misc/outwater.wav",	FMOD_DM | FMOD_TF,	sound_misc_outwater_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/weapons/pkup.wav",	FMOD_DM | FMOD_TF,	sound_weapons_pkup_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/player/plyrjmp8.wav",	FMOD_DM | FMOD_TF,	sound_player_plyrjmp8_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/protect.wav",	FMOD_DM | FMOD_TF,	sound_items_protect_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/protect2.wav",	FMOD_DM | FMOD_TF,	sound_items_protect2_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/items/protect3.wav",	FMOD_DM | FMOD_TF,	sound_items_protect3_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/misc/water1.wav",		FMOD_DM | FMOD_TF,	sound_misc_water1_wav_FMOD_DM_FMOD_TF);
	FMod_AddModel ("sound/misc/water2.wav",		FMOD_DM | FMOD_TF,	sound_misc_water2_wav_FMOD_DM_FMOD_TF);

	FMod_AddModel ("gfx/colormap.lmp",			FMOD_DM | FMOD_TF,	gfx_colormap_lmp_FMOD_DM_FMOD_TF);
	FMod_AddModel ("gfx/palette.lmp",			FMOD_DM | FMOD_TF,	gfx_palette_lmp_FMOD_DM_FMOD_TF);
}

void FMod_Response (void)
{
	int i, count;
	char buf[512] = {'m', 'o', 'd', 'i', 'f', 'i', 'e', 'd', ':', '\0'};
	qbool relevent;


	for (i = count = 0; i < check_models_num; i++) {
		relevent = (cl.teamfortress && (check_models[i].flags & FMOD_TF)) || 
			(!cl.teamfortress && (check_models[i].flags & FMOD_DM));

		if (check_models[i].checked && check_models[i].modified && relevent ) {
			if (strlen (buf) < 240) {
				strlcat (buf, va(" %s", COM_SkipPath (check_models[i].name)), sizeof (buf));
				count++;
			} else {
				strlcat (buf, " & more...", sizeof (buf));
				break;
			}
		}
	}

	if (!count)
		strlcpy (buf, "all models ok", sizeof (buf));

	Cbuf_AddText (va ("%s %s\n", cls.state == ca_disconnected ? "echo" : "say", buf));
}
