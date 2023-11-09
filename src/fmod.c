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
#include "rulesets.h"

typedef qbool (*pRulesetFilter)(const char* modelName);

typedef struct check_models_hashes_entry_s {
	const unsigned int hash[5];
	struct check_models_hashes_entry_s* next;
} check_models_hashes_entry_t;

// original models
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
static const int progs_stag_mdl_FMOD_TF[5]						= {0x6e970df0, 0xc91b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_basrkey_bsp_FMOD_TF[5]					= {0x6e971df0, 0xc11b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_basbkey_bsp_FMOD_TF[5]					= {0x6e972df0, 0xc21b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_w_s_key_mdl_FMOD_TF[5]					= {0x6e973df0, 0xc31b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_w_g_key_mdl_FMOD_TF[5]					= {0x6e974df0, 0xc41b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_b_g_key_mdl_FMOD_TF[5]					= {0x6e975df0, 0xc51b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_b_s_key_mdl_FMOD_TF[5]					= {0x6e976df0, 0xc61b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_ff_flag_mdl_FMOD_TF[5]					= {0x6e977df0, 0xc71b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_harbflag_mdl_FMOD_TF[5]					= {0x6e978df0, 0xc81b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
static const int progs_princess_mdl_FMOD_TF[5]					= {0x6e979df0, 0xc92b9eef, 0x84cb1a40, 0xcaee122e, 0x1b1d3d28};
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
static const int sound_items_r_item1_wav_FMOD_DM_FMOD_TF[5]     = {0xdde12941, 0xfbb3b804, 0x776cf5fa, 0xe81df8ea, 0xa1429f63};
static const int sound_items_r_item2_wav_FMOD_DM_FMOD_TF[5]     = {0xd9da4047, 0x5a7b191a, 0xc02d860d, 0x18f679de, 0xc47bd93a};
static const int sound_misc_water1_wav_FMOD_DM_FMOD_TF[5]		= {0x8625dbc7, 0xbaf30ae6, 0xfdb53965, 0x2d7eb9d6, 0xf9fd9304};
static const int sound_misc_water2_wav_FMOD_DM_FMOD_TF[5]		= {0xccda75ec, 0xfb5cd780, 0xe2d73b5b, 0x9f3560ad, 0x4e116b85};

static const int sound_misc_menu1_wav_FMOD_DM_FMOD_TF[5]		= {0x2e19b817, 0x0c0e1c5f, 0xd7a0ecf8, 0xb278c27e, 0xb0e1923c};
static const int sound_misc_menu2_wav_FMOD_DM_FMOD_TF[5]		= {0xd6719ab1, 0x0a407e2b, 0xb03a053b, 0x2a4bc2cb, 0x61cd7ff3};
static const int sound_misc_menu3_wav_FMOD_DM_FMOD_TF[5]		= {0xae120b9a, 0x3a217d2e, 0xc34f0990, 0x2d4332ed, 0x97c18f76};
static const int sound_misc_talk_wav_FMOD_DM_FMOD_TF[5]			= {0x5b15321c, 0x1af2d826, 0xb0228e9f, 0x49c06f56, 0xa8355e8b};
static const int sound_misc_basekey_wav_FMOD_DM_FMOD_TF[5]		= {0x926134ce, 0x22806dd3, 0x1952624a, 0x8f43f7e9, 0xa6fdfe64};
static const int sound_doors_runeuse_wav_FMOD_DM_FMOD_TF[5]		= {0xf41d6d59, 0xe99b93e8, 0x15edfd25, 0x668a499c, 0x1ac62572};

static const int gfx_colormap_lmp_FMOD_DM_FMOD_TF[5]			= {0xa93b3795, 0x17016397, 0x0a761d38, 0x4866e67d, 0x3c2a2a75};
static const int gfx_palette_lmp_FMOD_DM_FMOD_TF[5]			= {0xa6a2e242, 0xbad0f7da, 0xe263351f, 0x6d51f8ad, 0x49a45d4a};

// debug
static check_models_hashes_entry_t mdlhash_debug_armor = { {0x8d264ffc, 0xfabb1e7c, 0xc81128bc, 0x21633d32, 0xfa0b594f}, NULL };
static check_models_hashes_entry_t mdlhash_debug_backpack = { {0xe7c1f382, 0x0c3dc22e, 0xd31a04c0, 0x7251ed52, 0x45baf223}, NULL };
//static check_models_hashes_entry_t mdlhash_debug_flame = { {0xcfd8e629, 0x143a35a7, 0x4781ad82, 0x4254f247, 0xe07cd565}, NULL };
static check_models_hashes_entry_t mdlhash_debug_g_light = { {0x9d78b478, 0xfb44c849, 0xc56b6d9d, 0xab737839, 0x1fcfd848}, NULL };
static check_models_hashes_entry_t mdlhash_debug_g_nail = { {0xc6619f5d, 0x55511a85, 0x6de0cc63, 0xef61171a, 0x36b1b173}, NULL };
static check_models_hashes_entry_t mdlhash_debug_g_nail2 = { {0x9cd58369, 0xe54c8527, 0xcea234b4, 0x472bd93e, 0x7dfa8655}, NULL };
static check_models_hashes_entry_t mdlhash_debug_g_rock = { {0x334e9253, 0xa5a5a252, 0x68b9a856, 0x5e226647, 0xe4baeec7}, NULL };
static check_models_hashes_entry_t mdlhash_debug_g_rock2 = { {0xe24e10f9, 0x0f53bc41, 0x6043ee2b, 0x3d577eec, 0xbc75124c}, NULL };
static check_models_hashes_entry_t mdlhash_debug_g_shot = { {0x7bbba828, 0x99438f98, 0x975e3747, 0x6cbc8a2b, 0xd3a64db7}, NULL };
static check_models_hashes_entry_t mdlhash_debug_gib1 = { {0x69ee06fd, 0x8bac8483, 0xf9c5a43e, 0xd7513722, 0x55d5a4ff}, NULL };
static check_models_hashes_entry_t mdlhash_debug_gib2 = { {0x3251a1df, 0x50e68208, 0xeff0f797, 0x89894e71, 0x65505be3}, NULL };
static check_models_hashes_entry_t mdlhash_debug_gib3 = { {0x798f0b46, 0xe55c7250, 0x882ff3f5, 0x75495a80, 0x19a3ed99}, NULL };
static check_models_hashes_entry_t mdlhash_debug_grenade = { {0xdaf5db12, 0x41d4fc02, 0x764dd35a, 0xa4490888, 0xd5d26cea}, NULL };
//static check_models_hashes_entry_t mdlhash_debug_h_player = { {0x22ee657b, 0x31ac0cb9, 0x8ce678cc, 0xb174b570, 0x1a102fd9}, NULL };
static check_models_hashes_entry_t mdlhash_debug_invisibl = { {0xe5acae60, 0x8b2fe8fd, 0xb8ef8e78, 0x8d236ae4, 0xc3db0be3}, NULL };
static check_models_hashes_entry_t mdlhash_debug_invulner = { {0x57b00a3e, 0x009afa6d, 0xa4c2c8cb, 0xa7b0eccc, 0xa9e57049}, NULL };
static check_models_hashes_entry_t mdlhash_debug_missile = { {0x2ae7a078, 0xc39393d4, 0x73576788, 0x242699d2, 0x8f190bfd}, NULL };
static check_models_hashes_entry_t mdlhash_debug_quaddama = { {0x9010a156, 0x1b63addb, 0xbc9bd9e3, 0xff8d6e4e, 0xcecd1260}, NULL };
static check_models_hashes_entry_t mdlhash_debug_s_spike = { {0xb159e4cc, 0xbc5dccf0, 0x656e93ab, 0x3e72dd24, 0x10446fc6}, NULL };
static check_models_hashes_entry_t mdlhash_debug_spike = { {0xe78bb944, 0x92a753e4, 0x435c226b, 0x4021a65e, 0xef388c6b}, NULL };
/*
static check_models_hashes_entry_t mdlhash_debug_v_axe = { {0x77c9f8d4, 0x66d8b9e8, 0xaa75b3ce, 0xe6325d71, 0x70f57eea}, NULL };
static check_models_hashes_entry_t mdlhash_debug_v_light = { {0xf1ac7d77, 0x9ff2321d, 0x4ad3b6f0, 0x4372d11e, 0x46765fa7}, NULL };
static check_models_hashes_entry_t mdlhash_debug_v_nail = { {0x2322c673, 0xd5ec1e41, 0xe762d27e, 0x89c6ae72, 0x9006bd90}, NULL };
static check_models_hashes_entry_t mdlhash_debug_v_nail2 = { {0x636a9adb, 0xaeca483a, 0x284f0999, 0x3198b142, 0xd174e963}, NULL };
static check_models_hashes_entry_t mdlhash_debug_v_rock = { {0xf13c7070, 0xecc5c390, 0x22412e14, 0x51b13f01, 0x3c803dee}, NULL };
static check_models_hashes_entry_t mdlhash_debug_v_rock2 = { {0xda85f801, 0xa8bd2523, 0xa338f574, 0xa0466a00, 0xc5dc3964}, NULL };
static check_models_hashes_entry_t mdlhash_debug_v_shot = { {0x67e9aa5b, 0x294674a8, 0x553aea03, 0x13165b3c, 0x161e851e}, NULL };
static check_models_hashes_entry_t mdlhash_debug_v_shot2 = { {0x309594be, 0xe11e461a, 0x92cfcfdd, 0xe9b78788, 0x650daa89}, NULL };
*/

// Primevil deathmatch pack
static check_models_hashes_entry_t mdlhash_pdp_g_nail = { {0x86dd1964, 0xebe66f85, 0xf27e5b4c, 0x3230aeda, 0x10c13860}, NULL };
static check_models_hashes_entry_t mdlhash_pdp_g_rock = { {0x10e990fe, 0x3b401ed2, 0x9a9d71ad, 0x9085f059, 0x7710088f}, NULL };
static check_models_hashes_entry_t mdlhash_pdp_g_rock2 = { {0x3c4280d9, 0xd8a43a56, 0xeff931eb, 0xb76310af, 0xb28c39ad}, NULL };
static check_models_hashes_entry_t mdlhash_pdp_g_shot = { {0x3a9f015f, 0xd817a29e, 0xdc7a87fa, 0xc6e31628, 0xaa991119}, NULL };

// ruohis
static check_models_hashes_entry_t mdlhash_ruohis_armor = { {0x7772c52a, 0xdee37c13, 0xbbbc8523, 0x348525ba, 0x6e1bd649}, NULL };
//static check_models_hashes_entry_t mdlhash_ruohis_backpack = { {0x1a57bc37, 0xabe69424, 0xaa3b6b52, 0x64af310b, 0x177b9c20}, NULL };

static check_models_hashes_entry_t mdlhash_ruohis_g_light = { {0xfe2d50b8, 0x6ad56eb4, 0xe579fceb, 0x43e46f6d, 0x82f616da}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_g_nail = { {0xbe7da37e, 0xb531272b, 0x0c7de89a, 0x26f387bd, 0x20dc31ee}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_g_nail2 = { {0x38df15ca, 0xe2db4350, 0x5c065841, 0x67ec548c, 0x3db3f0b2}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_g_rock = { {0xd2e3e29a, 0x1e3a2acf, 0xa2f21d53, 0x60452add, 0xd8732afa}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_g_rock2 = { {0x54350c58, 0x6a09fd88, 0xae214b80, 0xc71d71de, 0x109d0fe6}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_g_shot = { {0xc74cafd8, 0xbc3af902, 0xbb52b588, 0x6f6fca30, 0x5b2ab554}, NULL };

static check_models_hashes_entry_t mdlhash_ruohis_gib1 = { {0x05ff57f8, 0x1073a096, 0xc7e5d055, 0x9c6c040b, 0x96d2eb8a}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_gib2 = { {0xdf23d687, 0x51ea9047, 0xbfdb3497, 0xed7b63db, 0x131a1bfb}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_gib3 = { {0xff5df01e, 0x7606952f, 0xdd36c784, 0xa0d72d33, 0x066afa33}, NULL };

static check_models_hashes_entry_t mdlhash_ruohis_invisibl = { {0xb7507e7e, 0x84dc7019, 0x8f5f1c39, 0xc0df2979, 0xe86b93dd}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_invulner = { {0xddcca118, 0x9e143d41, 0xa2a8dc57, 0x82744eb7, 0x09af301b}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_quaddama = { {0xdb93cf6c, 0x7006a0b8, 0x90b28c56, 0xf97afba7, 0x423699cb}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_suit = { {0xbf4de4f3, 0xe60927c1, 0xf4824f00, 0x21feef56, 0xac00a897}, NULL };

static check_models_hashes_entry_t mdlhash_ruohis_grenade = { {0xece5571e, 0xa35d61e8, 0xe2b6df49, 0xf35372d9, 0x7ac78910}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_missile = { {0x7e844aca, 0xb1b07ef9, 0x3d8994d8, 0xe6b4d14e, 0x56c49858}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_spike = { {0xd8225980, 0x0299e9f7, 0x6732fd66, 0x54555264, 0x67bda403}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_s_spike = { {0x40bf84c5, 0xcab85a9c, 0x498baa24, 0xbe07d9c6, 0x6b666756}, NULL };

static check_models_hashes_entry_t mdlhash_ruohis_end1 = { {0x51ff9322, 0x8b31b83d, 0x8988bebf, 0x0917dce7, 0xd9578e04}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_end2 = { {0x2ea71e4b, 0x0ef6cf22, 0x86d53ba5, 0x1145433d, 0xe6712054}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_end3 = { {0x04c3eda0, 0x2b4f2534, 0x127abbca, 0x755c97cf, 0x135b6597}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_end4 = { {0x4d5a54c1, 0x1fa8aaba, 0x600a5b2b, 0x7508ff3f, 0xc7af03de}, NULL };

static check_models_hashes_entry_t mdlhash_ruohis_b_bh100 = { {0x527fd5b6, 0xd490707f, 0xabd69622, 0xf3d18a69, 0xe9bc0eb9}, NULL };
static check_models_hashes_entry_t mdlhash_ruohis_b_bh100_other = { {0xb42135c1, 0x6816429c, 0x21e3ce3e, 0xfab9f8fc, 0x09387ae1}, NULL };

// plagues
static check_models_hashes_entry_t mdlhash_plaguespak_armor = { {0x5647e7b2, 0x52465746, 0xc10320fa, 0x1e92c90c, 0x27577c72}, NULL };
//static check_models_hashes_entry_t mdlhash_plaguespak_b_g_key = { {0x3f9779fe, 0x5ec019b7, 0xa25fe622, 0x6e217fa6, 0xe84cee45}, NULL };
//static check_models_hashes_entry_t mdlhash_plaguespak_b_s_key = { {0x370e1810, 0xb9bc0a5b, 0x4bb3fb54, 0x1573b0f5, 0x351207ab}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_g_light = { {0x6231b892, 0x1f39d616, 0x1442715d, 0xfbfc2826, 0xc866d8d6}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_g_nail = { {0xcb2e31c6, 0x1f1064ab, 0x5e0be681, 0x5e658642, 0xd9411ef4}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_g_nail2 = { {0x7d3fe401, 0x738a4344, 0x4ab62439, 0xba7313eb, 0x6a6f8536}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_g_rock = { {0xe31fc7b0, 0x35200618, 0x8ba6973c, 0x1296c555, 0xb2541bde}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_g_rock2 = { {0x674eb3e9, 0xde325927, 0xdabd4337, 0xc97a755c, 0x97f4f1f9}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_g_shot = { {0xd3486358, 0x4a3a3d37, 0x0efc43e4, 0x55a42f89, 0xf8078519}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_grenade = { {0xe3a1b910, 0x28c4e36a, 0x1af69321, 0x9d19df99, 0x5bcebfcb}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_missile = { {0xe047b3ec, 0xad03d2e2, 0x2a146207, 0x99e1f2df, 0xfb229f42}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_s_spike = { {0xe7c61b11, 0x703a7f30, 0x0051a5da, 0xb84a5bd1, 0xe23645ac}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_spike = { {0x28f1cb95, 0xafb8ed91, 0x6a8300ff, 0xeb29c03f, 0x28a2bbcb}, NULL };
/*
static check_models_hashes_entry_t mdlhash_plaguespak_v_light = { {0x3c9ca1d7, 0x77303f95, 0x0ad05a82, 0x64a83757, 0x8257cf75}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_v_nail = { {0x71876b6d, 0x934d5f0a, 0x322763f9, 0x924fb71c, 0x5027e3b4}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_v_nail2 = { {0x0b154635, 0x5a49f995, 0x9671641a, 0x2dfb1212, 0xe2631d44}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_v_rock = { {0xbac51d54, 0x884906a3, 0xa7ebec69, 0xe994be29, 0xdd326508}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_v_rock2 = { {0x170a3ecf, 0x5cd131ab, 0xfcecd4fb, 0x62151b89, 0x3bc663b7}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_v_shot = { {0x93dc8f24, 0x6739d838, 0x996b00cc, 0x85c2d98f, 0x430b8e30}, NULL };
static check_models_hashes_entry_t mdlhash_plaguespak_v_shot2 = { {0x58e68b33, 0x1127186f, 0xd1bdae79, 0x66fd9ca3, 0x82421f9b}, NULL };
*/

// unknown
// these come with nQuake and are told to be "harmless"
static check_models_hashes_entry_t mdlhash_unknown_end2 = { {0xe3a1b910, 0x28c4e36a, 0x1af69321, 0x9d19df99, 0x5bcebfcb }, NULL };
static check_models_hashes_entry_t mdlhash_unknown_end3 = { {0xe3a1b910, 0x28c4e36a, 0x1af69321, 0x9d19df99, 0x5bcebfcb }, NULL };
static check_models_hashes_entry_t mdlhash_unknown_end4 = { {0xe3a1b910, 0x28c4e36a, 0x1af69321, 0x9d19df99, 0x5bcebfcb }, NULL };

// grenade converted from CPMA
//static check_models_hashes_entry_t mdlhash_cpma_grenade = { {0xe89d9866, 0xf52324c9, 0x005d0507, 0x083c8cb4, 0xc1918636 }, NULL };
// megaheath converted from Generations Arena
static check_models_hashes_entry_t mdlhash_generations_b_bh100 = { {0xc6b455ff, 0xad7cb845, 0xa7356729, 0x35375fcf, 0x10adb479}, NULL };

// megahealth alternatives
// this one is said to be used in the US
static check_models_hashes_entry_t sound_items_r_item2_wav_us = {{0xf62a00d2, 0x5fceaaca, 0x25f91692, 0xf7602cb7, 0x230da525}, NULL};
// this is just 11kHz version of the US one, but resampled to 11 kHz
// this one might be used widely in russia
static check_models_hashes_entry_t sound_items_r_item2_wav_ru = {{0x83a8e646, 0xbc4cc313, 0xa07aa96e, 0x9d81eada, 0xf619bd10}, NULL};

// Mindgrid Audio sound files
static check_models_hashes_entry_t sound_buttons_mindgrid_airbut1 = { {0xd5fbbce0, 0x2083e431, 0xaa11c54d, 0x089ce053, 0xdf03ce11}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_armor1 = { {0x1b47daed, 0x5a60cd6a, 0x83940499, 0x65eb651b, 0x4441f6cf}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_damage = { {0x3da087a3, 0x1799d4c7, 0x8ddabf31, 0x42e2de6f, 0x5bfdabec}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_damage2 = { {0xd0eb779c, 0x0bf1d83c, 0x120e1c7e, 0x50fa449b, 0x2cd4655b}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_damage3 = { {0x2e0c3b71, 0xdd52cfc9, 0x8e5fe1e5, 0x0ac48fd9, 0xa2d8b970}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_inv1 = { {0x787708ba, 0x63f52c51, 0x55835932, 0x7db46d86, 0xccc830cc}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_inv2 = { {0xb2a3bc8c, 0x7eb56b5f, 0x675e4d67, 0x24054040, 0x3b2ecd0b}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_inv3 = { {0xa1945433, 0x7f3e2c52, 0x9144528a, 0x02891cf0, 0xd95eb255}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_itembk2 = { {0x41dd6dad, 0x45a8c24b, 0xef56b75e, 0x5ab3915a, 0x2901a70f}, NULL };
static check_models_hashes_entry_t sound_misc_mindgrid_outwater = { {0x6be98515, 0xfeab0126, 0x80edc811, 0xe7cf7012, 0x1def4980}, NULL };
static check_models_hashes_entry_t sound_weapons_mindgrid_pkup = { {0xda261fbc, 0x12f47c68, 0x9e91e794, 0xce0a434d, 0xc4e35e77}, NULL };
static check_models_hashes_entry_t sound_player_mindgrid_plyrjmp8 = { {0x5a9e8263, 0xb9b3f06e, 0x8a047de1, 0x72c18bec, 0x547df977}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_protect = { {0x5948333f, 0xa28e882a, 0xb4b82964, 0xe10679c5, 0x191f8cb1}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_protect2 = { {0x29e977b0, 0x0d9378fe, 0xb4489037, 0x02d83d62, 0xce0b718d}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_protect3 = { {0x7619314c, 0x07b6901c, 0x000601e2, 0xc358367e, 0x999f90b1}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_r_item1 = { {0x0f5754d4, 0xa8b84c5f, 0x0001903a, 0xd3ab289c, 0xa93bc848}, NULL };
static check_models_hashes_entry_t sound_items_mindgrid_r_item2 = { {0xd2d0de39, 0x2bb78d33, 0x09ef3081, 0xbe90bacf, 0xbf71ecce}, NULL };
static check_models_hashes_entry_t sound_misc_mindgrid_water1 = { {0x838a0318, 0x0805e53e, 0x6ebf4065, 0xaa5fb9a0, 0xe1cad807}, NULL };
static check_models_hashes_entry_t sound_misc_mindgrid_water2 = { {0xfb0a6b17, 0x065fbc33, 0x5fab7cc5, 0x110c01aa, 0x278b612e}, NULL };

// CapnBub's new player model, see http://www.quakeworld.nu/forum/topic/6073/new-quake-models/page/1
static check_models_hashes_entry_t mdlhash_player_mdl_CapNBubs_FMOD_DM = { {0xC0223459, 0x42B97D8D, 0xC52FF472, 0xF3EE0710, 0x41E21132}, NULL };

// gpl_maps.pk3 include a small wav file to allow map to load
static check_models_hashes_entry_t sound_gpl_maps_silence_wav = { {0xc99871d4, 0xc60e0fef, 0x14e64bf9, 0xbaf43934, 0x5376df18}, NULL };

// rerelease
static check_models_hashes_entry_t sound_buttons_rerelease_airbut1 = { {0x81911593, 0x3e85656b, 0x7a9475d0, 0x87de29a0, 0x157d3ef9}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_armor1 = { {0x1bde8fc5, 0xc979543f, 0x8fc7f57b, 0xe2c1367b, 0x62cac72f}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_damage = { {0x4dde0192, 0x1791d662, 0xa33168eb, 0xb98d083f, 0x15ba0ded}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_damage2 = { {0x6c9b6f7c, 0x85e48943, 0x803ddafd, 0x8249af31, 0x9967d89c}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_damage3 = { {0x18a390ad, 0x0c47d3e9, 0x19db8c3b, 0x1426eda5, 0xa2feb55a}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_health1 = { {0x3ef1d863, 0x683899cb, 0xd9191607, 0x02353423, 0x0737419c}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_inv1 = { {0x24c5cf56, 0xb3569f7a, 0xb21bb96a, 0x95638065, 0x2ea0d036}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_inv2 = { {0x810f09ea, 0x34d8c482, 0xcc9561b1, 0xffeba6d0, 0xc6610c57}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_inv3 = { {0x037ed5b5, 0x27ab500b, 0xcee3abb1, 0xd882645a, 0x48e02c87}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_protect = { {0x7f894d4b, 0xe4a5c499, 0xd06df01a, 0xb1e57bc8, 0xb2d552aa}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_protect2 = { {0xa8a31b8a, 0xf96967b1, 0xc394287f, 0x3a5bf203, 0xe80d111b}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_protect3 = { {0xd8a88cdf, 0x394d1b5b, 0x28fb397b, 0x4120f605, 0x3e5dbefa}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_r_item1 = { {0xc850a036, 0x86199e27, 0x8550ac67, 0x7fc29886, 0x1d80678a}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_r_item2 = { {0xb0e2cb5d, 0xcc12db33, 0x5d599343, 0x2835ac62, 0x76fa4846}, NULL };
static check_models_hashes_entry_t sound_items_rerelease_itembk2 = { {0xf3937197, 0x71d317b7, 0x7827aee9, 0x68ab6f3a, 0x5cb0e9c7}, NULL };
static check_models_hashes_entry_t sound_player_rerelease_land = { {0x349bc931, 0x8621a826, 0xca259e9d, 0x5eb296d1, 0xe0f4ac70}, NULL };
static check_models_hashes_entry_t sound_player_rerelease_land2 = { {0x8ac31277, 0xdfc8eedf, 0xf6f322ca, 0x37188a1f, 0xe3f7a688}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_outwater = { {0x13930c6c, 0xaa57abbf, 0x5356d6ea, 0x1a2e6d40, 0x6a818b4c}, NULL };
static check_models_hashes_entry_t sound_weapons_rerelease_pkup = { {0x6a0c9888, 0x2d68fa77, 0x5e448d95, 0x2a95666c, 0x842ee9a9}, NULL };
static check_models_hashes_entry_t sound_player_rerelease_plyrjmp8 = { {0xd33205fa, 0x2ec2aa75, 0x074e991b, 0x2f32c63e, 0xafbbbb6a}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_water1 = { {0xa694280d, 0xd6b28324, 0x9e2c2def, 0x007d86f7, 0xbb583438}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_water2 = { {0xd974d2db, 0x7c609ba1, 0x9ffefe98, 0x376e5f5d, 0x8cc0cbb9}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_menu1 = { {0x0849e8f9, 0x5828515c, 0x55d6b38c, 0xf7299ac3, 0x0655378e}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_menu2 = { {0xe028d3c0, 0x710031e1, 0xd83221ad, 0xd28205c7, 0x51c10a10}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_menu3 = { {0x2205061f, 0x00f4c078, 0xfa26934b, 0x15bacdfd, 0x98d90a7b}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_talk = { {0xf28522df, 0x7ea37652, 0xc4605d1f, 0xce10e1cf, 0x2d1fc5fc}, NULL };
static check_models_hashes_entry_t sound_misc_rerelease_basekey = { {0x6871924c, 0xbcc9e6dc, 0x6cea01c2, 0x367f979f, 0xb16619ae}, NULL };
static check_models_hashes_entry_t sound_doors_rerelease_runeuse = { {0x00ffef66, 0x4ed15aae, 0x8e67b5ef, 0x563f0c0f, 0x70b11c41}, NULL };

#define MAX_CHECK_MODELS 256
#define	FMOD_DM 1
#define FMOD_TF 2

typedef struct check_models_s {
	char		name[MAX_QPATH];
	qbool		modified;
	qbool		checked;
	int			flags;
	const void*	hash;
	check_models_hashes_entry_t*
				hash_alt;
} check_models_t;

static check_models_t check_models[MAX_CHECK_MODELS];
static int check_models_num = 0;
static float fmod_warn_time = 0.0f;

static qbool FMod_IsModelModified (const char *name, const int flags, const byte *buf, const size_t len)
{
	int i;
	unsigned char hash[DIGEST_SIZE];
	SHA1_CTX context;
	qbool found = false;

	SHA1Init (&context);
	SHA1Update (&context, (unsigned char *) buf, len);
	SHA1Final (hash, &context);

	for (i = 0; i < check_models_num; i++) {
		if (strcmp(name, check_models[i].name) == 0) {
			if (check_models[i].flags == flags) {
				found = true;
				break;
			}
		}
	}

	if (found) {
		if (memcmp(check_models[i].hash, hash, DIGEST_SIZE) == 0) {
			// original model
			return false;
		}
		else {
			check_models_hashes_entry_t *cur = check_models[i].hash_alt;
			while (cur) {
				if (memcmp(cur->hash, hash, DIGEST_SIZE) == 0 && Rulesets_AllowAlternateModel(name)) {
					// alternative legal model
					return false;
				}
				cur = cur->next;
			}
		}
	}

	// no match found
	return true;
}

static int FMod_AddModel (const char *name, const qbool flags, const void *hash)
{
	if (check_models_num >= MAX_CHECK_MODELS)
		return -1;

	strlcpy (check_models[check_models_num].name, name, sizeof (check_models[check_models_num].name));	
	check_models[check_models_num].checked = check_models[check_models_num].modified = false;
	check_models[check_models_num].flags = flags;
	check_models[check_models_num].hash = hash;
	check_models[check_models_num].hash_alt = NULL;
	return check_models_num++;
}

/// adds alternative models' hashes
static void FMod_AddModelAlt (int cm_num, check_models_hashes_entry_t *hash_new)
{
	check_models_hashes_entry_t* curr_head;
	if (cm_num < 0) return;

	curr_head = check_models[cm_num].hash_alt;
	check_models[cm_num].hash_alt = hash_new;
	hash_new->next = curr_head;
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
	int lastid;

	Cmd_AddCommand("f_modified", FMod_Response);

	memset (check_models, 0, sizeof (check_models));

	lastid = FMod_AddModel ("progs/armor.mdl",			FMOD_DM | FMOD_TF,	progs_armor_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_armor);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_armor);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_armor);

	lastid = FMod_AddModel ("progs/backpack.mdl",		FMOD_DM | FMOD_TF,	progs_backpack_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_backpack);

	FMod_AddModel ("progs/bolt2.mdl",			FMOD_DM | FMOD_TF,	progs_bolt2_mdl_FMOD_DM_FMOD_TF);
	lastid = FMod_AddModel ("progs/end1.mdl",			FMOD_DM | FMOD_TF,	progs_end1_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_end1);
	lastid = FMod_AddModel ("progs/end2.mdl",			FMOD_DM | FMOD_TF,	progs_end2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_unknown_end2);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_end2);
	lastid = FMod_AddModel ("progs/end3.mdl",			FMOD_DM | FMOD_TF,	progs_end3_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_unknown_end3);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_end3);
	lastid = FMod_AddModel ("progs/end4.mdl",			FMOD_DM | FMOD_TF,	progs_end4_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_unknown_end4);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_end4);
	FMod_AddModel ("progs/eyes.mdl",			FMOD_DM | FMOD_TF,	progs_eyes_mdl_FMOD_DM_FMOD_TF);
	lastid = FMod_AddModel ("progs/g_light.mdl",			FMOD_DM | FMOD_TF,	progs_g_light_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_g_light);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_g_light);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_g_light);

	lastid = FMod_AddModel ("progs/g_nail.mdl",			FMOD_DM | FMOD_TF,	progs_g_nail_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_g_nail);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_g_nail);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_g_nail);
	FMod_AddModelAlt(lastid, &mdlhash_pdp_g_nail);

	lastid = FMod_AddModel ("progs/g_nail2.mdl",			FMOD_DM | FMOD_TF,	progs_g_nail2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_g_nail2);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_g_nail2);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_g_nail2);

	lastid = FMod_AddModel ("progs/g_rock.mdl",			FMOD_DM | FMOD_TF,	progs_g_rock_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_g_rock);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_g_rock);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_g_rock);
	FMod_AddModelAlt(lastid, &mdlhash_pdp_g_rock);

	lastid = FMod_AddModel ("progs/g_rock2.mdl",			FMOD_DM | FMOD_TF,	progs_g_rock2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_g_rock2);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_g_rock2);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_g_rock2);
	FMod_AddModelAlt(lastid, &mdlhash_pdp_g_rock2);

	lastid = FMod_AddModel ("progs/g_shot.mdl",			FMOD_DM | FMOD_TF,	progs_g_shot_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_g_shot);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_g_shot);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_g_shot);
	FMod_AddModelAlt(lastid, &mdlhash_pdp_g_shot);

	lastid = FMod_AddModel ("progs/gib1.mdl",			FMOD_DM | FMOD_TF,	progs_gib1_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_gib1);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_gib1);

	lastid = FMod_AddModel ("progs/gib2.mdl",			FMOD_DM | FMOD_TF,	progs_gib2_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_gib2);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_gib2);

	lastid = FMod_AddModel ("progs/gib3.mdl",			FMOD_DM | FMOD_TF,	progs_gib3_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_gib3);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_gib3);

	lastid = FMod_AddModel ("progs/grenade.mdl",			FMOD_DM | FMOD_TF,	progs_grenade_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_grenade);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_grenade);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_grenade);
	//FMod_AddModelAlt(lastid, &mdlhash_cpma_grenade);

	lastid = FMod_AddModel ("progs/invisibl.mdl",		FMOD_DM | FMOD_TF,	progs_invisibl_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_invisibl);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_invisibl);

	lastid = FMod_AddModel ("progs/invulner.mdl",		FMOD_DM | FMOD_TF,	progs_invulner_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_invulner);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_invulner);

	lastid = FMod_AddModel ("progs/missile.mdl",			FMOD_DM | FMOD_TF,	progs_missile_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_missile);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_missile);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_missile);

	lastid = FMod_AddModel ("progs/quaddama.mdl",		FMOD_DM | FMOD_TF,	progs_quaddama_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_quaddama);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_quaddama);

	lastid = FMod_AddModel ("progs/s_spike.mdl",			FMOD_DM | FMOD_TF,	progs_s_spike_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_s_spike);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_s_spike);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_s_spike);

	lastid = FMod_AddModel ("progs/spike.mdl",			FMOD_DM | FMOD_TF,	progs_spike_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_debug_spike);
	FMod_AddModelAlt(lastid, &mdlhash_plaguespak_spike);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_spike);

	lastid = FMod_AddModel ("progs/suit.mdl",			FMOD_DM | FMOD_TF,	progs_suit_mdl_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_suit);

	lastid = FMod_AddModel ("progs/player.mdl",			FMOD_DM,			progs_player_mdl_FMOD_DM);
	FMod_AddModelAlt (lastid, &mdlhash_player_mdl_CapNBubs_FMOD_DM);
	FMod_AddModel ("progs/player.mdl",			FMOD_TF,			progs_player_mdl_FMOD_TF);

	FMod_AddModel ("progs/tf_flag.mdl",			FMOD_TF,			progs_tf_flag_mdl_FMOD_TF);
	FMod_AddModel("progs/stag.mdl",				FMOD_TF,			progs_stag_mdl_FMOD_TF);
	FMod_AddModel("progs/basrkey.bsp",			FMOD_TF,			progs_basrkey_bsp_FMOD_TF);
	FMod_AddModel("progs/basbkey.bsp",			FMOD_TF,			progs_basbkey_bsp_FMOD_TF);
	FMod_AddModel("progs/w_s_key.mdl",			FMOD_TF,			progs_w_s_key_mdl_FMOD_TF);
	FMod_AddModel("progs/w_g_key.mdl",			FMOD_TF,			progs_w_g_key_mdl_FMOD_TF);
	FMod_AddModel("progs/b_g_key.mdl",			FMOD_TF,			progs_b_g_key_mdl_FMOD_TF);
	FMod_AddModel("progs/b_s_key.mdl",			FMOD_TF,			progs_b_s_key_mdl_FMOD_TF);
	FMod_AddModel("progs/ff_flag.mdl",			FMOD_TF,			progs_ff_flag_mdl_FMOD_TF);
	FMod_AddModel("progs/harbflag.mdl",			FMOD_TF,			progs_harbflag_mdl_FMOD_TF);
	FMod_AddModel("progs/princess.mdl",			FMOD_TF,			progs_princess_mdl_FMOD_TF);
	FMod_AddModel ("progs/turrbase.mdl",		FMOD_TF,			progs_turrbase_mdl_FMOD_TF);
	FMod_AddModel ("progs/turrgun.mdl",			FMOD_TF,			progs_turrgun_mdl_FMOD_TF);
	FMod_AddModel ("progs/disp.mdl",			FMOD_TF,			progs_disp_mdl_FMOD_TF);
	FMod_AddModel ("progs/tf_stan.mdl",			FMOD_TF,			progs_tf_stan_mdl_FMOD_TF);
	FMod_AddModel ("progs/hgren2.mdl",			FMOD_TF,			progs_hgren2_mdl_FMOD_TF); // gren1
	FMod_AddModel ("progs/grenade2.mdl",		FMOD_TF,			progs_grenade2_mdl_FMOD_TF); // gren2
	FMod_AddModel ("progs/detpack.mdl",			FMOD_TF,			progs_detpack_mdl_FMOD_TF); // detpack

	FMod_AddModel ("progs/s_bubble.spr",		FMOD_DM | FMOD_TF,	progs_s_bubble_spr_FMOD_DM_FMOD_TF);
	FMod_AddModel ("progs/s_explod.spr",		FMOD_DM | FMOD_TF,	progs_s_explod_spr_FMOD_DM_FMOD_TF);

	lastid = FMod_AddModel ("maps/b_bh100.bsp",			FMOD_DM | FMOD_TF,	maps_b_bh100_bsp_FMOD_DM_FMOD_TF);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_b_bh100);
	FMod_AddModelAlt(lastid, &mdlhash_ruohis_b_bh100_other);
	FMod_AddModelAlt(lastid, &mdlhash_generations_b_bh100);

	//Wav files
	lastid = FMod_AddModel ("sound/buttons/airbut1.wav",	FMOD_DM | FMOD_TF,	sound_buttons_airbut1_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_buttons_mindgrid_airbut1);
		FMod_AddModelAlt(lastid, &sound_buttons_rerelease_airbut1);
	lastid = FMod_AddModel ("sound/items/armor1.wav",	FMOD_DM | FMOD_TF,	sound_items_armor1_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_armor1);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_armor1);
	lastid = FMod_AddModel ("sound/items/damage.wav",	FMOD_DM | FMOD_TF,	sound_items_damage_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_damage);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_damage);
	lastid = FMod_AddModel ("sound/items/damage2.wav",	FMOD_DM | FMOD_TF,	sound_items_damage2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_damage2);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_damage2);
	lastid = FMod_AddModel ("sound/items/damage3.wav",	FMOD_DM | FMOD_TF,	sound_items_damage3_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_damage3);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_damage3);
	lastid = FMod_AddModel ("sound/items/health1.wav",	FMOD_DM | FMOD_TF,	sound_items_health1_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_health1);
	lastid = FMod_AddModel ("sound/items/inv1.wav",		FMOD_DM | FMOD_TF,	sound_items_inv1_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_inv1);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_inv1);
	lastid = FMod_AddModel ("sound/items/inv2.wav",		FMOD_DM | FMOD_TF,	sound_items_inv2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_inv2);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_inv2);
	lastid = FMod_AddModel ("sound/items/inv3.wav",		FMOD_DM | FMOD_TF,	sound_items_inv3_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_inv3);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_inv3);
	lastid = FMod_AddModel ("sound/items/itembk2.wav",	FMOD_DM | FMOD_TF,	sound_items_itembk2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_itembk2);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_itembk2);
	lastid = FMod_AddModel ("sound/player/land.wav",		FMOD_DM | FMOD_TF,	sound_player_land_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_player_rerelease_land);
	lastid = FMod_AddModel ("sound/player/land2.wav",	FMOD_DM | FMOD_TF,	sound_player_land2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_player_rerelease_land2);
	lastid = FMod_AddModel ("sound/misc/outwater.wav",	FMOD_DM | FMOD_TF,	sound_misc_outwater_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_misc_mindgrid_outwater);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_outwater);
	lastid = FMod_AddModel ("sound/weapons/pkup.wav",	FMOD_DM | FMOD_TF,	sound_weapons_pkup_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_weapons_mindgrid_pkup);
		FMod_AddModelAlt(lastid, &sound_weapons_rerelease_pkup);
	lastid = FMod_AddModel ("sound/player/plyrjmp8.wav",	FMOD_DM | FMOD_TF,	sound_player_plyrjmp8_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_player_mindgrid_plyrjmp8);
		FMod_AddModelAlt(lastid, &sound_player_rerelease_plyrjmp8);
	lastid = FMod_AddModel ("sound/items/protect.wav",	FMOD_DM | FMOD_TF,	sound_items_protect_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_protect);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_protect);
	lastid = FMod_AddModel ("sound/items/protect2.wav",	FMOD_DM | FMOD_TF,	sound_items_protect2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_protect2);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_protect2);
	lastid = FMod_AddModel ("sound/items/protect3.wav",	FMOD_DM | FMOD_TF,	sound_items_protect3_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_protect3);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_protect3);
	lastid = FMod_AddModel ("sound/items/r_item1.wav",	FMOD_DM | FMOD_TF,	sound_items_r_item1_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_r_item1);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_r_item1);

	lastid = FMod_AddModel ("sound/items/r_item2.wav",	FMOD_DM | FMOD_TF,	sound_items_r_item2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_items_r_item2_wav_us);
		FMod_AddModelAlt(lastid, &sound_items_r_item2_wav_ru);
		FMod_AddModelAlt(lastid, &sound_items_mindgrid_r_item2);
		FMod_AddModelAlt(lastid, &sound_items_rerelease_r_item2);

	lastid = FMod_AddModel ("sound/misc/water1.wav",		FMOD_DM | FMOD_TF,	sound_misc_water1_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_misc_mindgrid_water1);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_water1);
	lastid = FMod_AddModel ("sound/misc/water2.wav",		FMOD_DM | FMOD_TF,	sound_misc_water2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_misc_mindgrid_water2);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_water2);

	lastid = FMod_AddModel("sound/misc/menu1.wav", FMOD_DM | FMOD_TF, sound_misc_menu1_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_menu1);
	lastid = FMod_AddModel("sound/misc/menu2.wav", FMOD_DM | FMOD_TF, sound_misc_menu2_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_menu2);
	lastid = FMod_AddModel("sound/misc/menu3.wav", FMOD_DM | FMOD_TF, sound_misc_menu3_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_menu3);
	lastid = FMod_AddModel("sound/misc/talk.wav", FMOD_DM | FMOD_TF, sound_misc_talk_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_talk);
	lastid = FMod_AddModel("sound/misc/basekey.wav", FMOD_DM | FMOD_TF, sound_misc_basekey_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt (lastid, &sound_gpl_maps_silence_wav);
		FMod_AddModelAlt(lastid, &sound_misc_rerelease_basekey);
	lastid = FMod_AddModel("sound/doors/runeuse.wav", FMOD_DM | FMOD_TF, sound_doors_runeuse_wav_FMOD_DM_FMOD_TF);
		FMod_AddModelAlt(lastid, &sound_doors_rerelease_runeuse);

	FMod_AddModel ("gfx/colormap.lmp",			FMOD_DM | FMOD_TF,	gfx_colormap_lmp_FMOD_DM_FMOD_TF);
	FMod_AddModel ("gfx/palette.lmp",			FMOD_DM | FMOD_TF,	gfx_palette_lmp_FMOD_DM_FMOD_TF);
}

char *FMod_Response_Text(void)
{
	static char buf[512];
	int i, count;
	qbool relevent;

	strlcpy(buf, "modified:", sizeof(buf));

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
	
	return buf;
}

void FMod_Response (void)
{
	Cbuf_AddText (va ("%s %s\n", cls.state == ca_disconnected ? "echo" : "say", FMod_Response_Text()));
}
