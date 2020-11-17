/*
Copyright (C) 2011 VULTUREIIC

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "quakedef.h"
#include "gl_model.h"
#include "hud.h"
#include "hud_common.h"
#include "vx_stuff.h"
#include "sbar.h"
#include "fonts.h"
#include "r_texture.h"
#include "qmb_particles.h"

corona_texture_t corona_textures[CORONATEX_COUNT];

cvar_t		tei_lavafire = { "gl_surface_lava", "0" };
cvar_t		tei_slime = { "gl_surface_slime", "0" };

cvar_t		amf_coronas = { "gl_coronas", "0" }; // 1
cvar_t		amf_coronas_tele = { "gl_coronas_tele", "0" };
cvar_t		amf_weather_rain = { "gl_weather_rain", "0" };
cvar_t		amf_weather_rain_fast = { "gl_weather_rain_fast", "0" };
cvar_t		amf_extratrails = { "gl_extratrails", "0" }; // 1
cvar_t		amf_detpacklights = { "gl_detpacklights", "0" }; // 1
cvar_t		amf_buildingsparks = { "gl_buildingsparks", "0" }; // 1
cvar_t		amf_cutf_tesla_effect = { "gl_cutf_tesla_effect", "0" }; // 1
cvar_t		amf_underwater_trails = { "gl_turb_trails", "0" }; // 1

cvar_t		amf_hidenails = { "cl_hidenails", "0" };
cvar_t		amf_hiderockets = { "cl_hiderockets", "0" };

cvar_t		amf_stat_loss = { "r_damagestats", "0" };
cvar_t		amf_lightning = { "gl_lightning", "0" }; // 1

cvar_t		amf_lightning_size = { "gl_lightning_size", "3" };
cvar_t		amf_lightning_sparks = { "gl_lightning_sparks", "0" }; // 0.4
cvar_t		amf_lightning_sparks_size = { "gl_lightning_sparks_size", "300", CVAR_RULESET_MAX | CVAR_RULESET_MIN, NULL, 300, 300, 1 };
cvar_t		amf_lightning_color = { "gl_lightning_color", "120 140 255", CVAR_COLOR };
cvar_t		amf_lighting_vertex = { "gl_lighting_vertex", "0" }; // 1
cvar_t		amf_lighting_colour = { "gl_lighting_color", "0" }; // 1

cvar_t		amf_inferno_trail = { "gl_inferno_trail", "2" };
cvar_t		amf_inferno_speed = { "gl_inferno_speed", "1000" };

cvar_t		cl_camera_tpp = { "cl_camera_tpp", "0" };
cvar_t		amf_camera_chase_dist = { "cl_camera_tpp_distance", "-56" };
cvar_t		amf_camera_chase_height = { "cl_camera_tpp_height", "24" };
cvar_t		amf_camera_death = { "cl_camera_death", "0" }; // 1
cvar_t		amf_nailtrail_water = { "gl_nailtrail_turb", "0" };
cvar_t		amf_nailtrail_plasma = { "gl_nailtrail_plasma", "0" };
cvar_t		amf_nailtrail = { "gl_nailtrail", "0" }; // 1

cvar_t		amf_part_gunshot = { "gl_particle_gunshots", "0" };
cvar_t		amf_part_gunshot_type = { "gl_particle_gunshots_type", "1" };
cvar_t		amf_part_spikes = { "gl_particle_spikes", "0" }; // 0.1
cvar_t		amf_part_spikes_type = { "gl_particle_spikes_type", "1" };
cvar_t		amf_part_shockwaves = { "gl_particle_shockwaves", "0" }; // 1
cvar_t		amf_part_2dshockwaves = { "gl_particle_shockwaves_flat", "0" };
cvar_t		amf_part_blood = { "gl_particle_blood", "0" }; // 1
cvar_t		amf_part_blood_color = { "gl_particle_blood_color", "1" };
cvar_t		amf_part_blood_type = { "gl_particle_blood_type", "1" };
cvar_t		amf_part_explosion = { "gl_particle_explosions", "0" };
cvar_t		amf_part_blobexplosion = { "gl_particle_blobs", "0" }; // 0.1
cvar_t		amf_part_gibtrails = { "gl_particle_gibtrails", "0" }; // 1
cvar_t		amf_part_muzzleflash = { "gl_particle_muzzleflash", "0" };
cvar_t		amf_part_deatheffect = { "gl_particle_deatheffect", "0" }; // 1
cvar_t		amf_part_teleport = { "gl_particle_telesplash", "0" };
cvar_t		amf_part_fire = { "gl_particle_fire", "0" }; // 1
cvar_t		amf_part_firecolor = { "gl_particle_firecolor", "" }; // 1
cvar_t		amf_part_sparks = { "gl_particle_sparks", "0" }; // 1
cvar_t		amf_part_traillen = { "gl_particle_trail_length", "1" };
cvar_t		amf_part_trailtime = { "gl_particle_trail_time",   "1" };
cvar_t		amf_part_trailwidth = { "gl_particle_trail_width",  "3" };
cvar_t		amf_part_traildetail = { "gl_particle_trail_detail", "1" };
cvar_t		amf_part_trailtype = { "gl_particle_trail_type",   "1" };

static int vxdamagepov;
static int vxdamagecount;
static int vxdamagecount_oldhealth;
static int vxdamagecount_time;
static int vxdamagecountarmour;
static int vxdamagecountarmour_oldhealth;
static int vxdamagecountarmour_time;

void Amf_Reset_DamageStats(void)
{
	vxdamagepov = cl.spectator ? Cam_TrackNum() : -1;
	vxdamagecount = 0;
	vxdamagecount_time = 0;
	vxdamagecount_oldhealth = 0;
	vxdamagecountarmour = 0;
	vxdamagecountarmour_time = 0;
	vxdamagecountarmour_oldhealth = 0;
}

char *TP_ConvertToWhiteText(char *s)
{
	static char	buf[4096];
	char	*out, *p;
	buf[0] = 0;
	out = buf;

	for (p = s; *p; p++) {
		*out++ = *p - (*p & 128);
	}
	*out = 0;
	return buf;
}

///////////////////////////////////////
//Model Checking
typedef struct
{
	char *number;
	char *name;
	char *description;
	qbool notify;
} checkmodel_t;

checkmodel_t tp_checkmodels[] =
{
	{"6967", "Eyes Model", "", false},
	{"8986", "Player Model with External Skins", "Possible external fullbright skins.", true},
	{"20572", "Dox TF PAK", ".", false},
	{"37516", "Unknown non-cheating model", "I was told it wasn't a cheat model by Kay", false},
	{"13845", "TF 2.8 Standard", "", false},
	{"33168", "Q1 Standard", "", false},
	{"10560", "Ugly Quake Guy", "", false},
	{"44621", "Women's TF PAK", "", false},
	{"64351", "Razer PAK", "Glowing/Spiked plus other cheats.", true},
	{"17542", "Glow13 and Razer PAK", "Glowing/Spiked plus other cheats.", true},
	{"43672", "Modified/Original Glow", "This model is a cheat.", true},
	{"52774", "Thugguns PAK", "Spiked Player Model.", true},
	{"54273", "YourRom3ro's PAK", "Spiked Player Model.", true},
	{"47919", "DMPAK", "Spiked/Fullbright Player Model and loud water", true},
	{"60647", "Navy Seals PAK", "No Information (Considered cheat by TF Anti-Cheat", true},
	{"36870", "Clan Llama Force", "Fullbright Player Model", true},
	{"58759", "Clan LF / Acid Angel", "Fullbright Player Model plus other cheats", true},
	{"36870", "WBMod", "Hacked/Glowing Models", true},
	{"64524", "Fullbright", "Fullbright Player Model missing the head", true}
};

#define NUMCHECKMODELS (sizeof(tp_checkmodels) / sizeof(tp_checkmodels[0]))

checkmodel_t *TP_GetModels(char *s)
{
	unsigned int i;
	char *f = TP_ConvertToWhiteText(s);
	checkmodel_t *model = tp_checkmodels;

	for (i = 0, model = tp_checkmodels; i < NUMCHECKMODELS; i++, model++) {
		if (strstr(f, model->number)) {
			return model;
		}
	}

	return NULL;
}

////////////////////////////////
/// test stuff
void Draw_AlphaWindow(int x1, int y1, int x2, int y2, int col, float alpha)
{
	int dist;

	dist = x2 - x1;
	Draw_Fill(x1, y1, dist + 1, 1, 2);      //Border - Top
	Draw_Fill(x1, y2, dist, 1, 1);          //Border - Bottom
	dist = y2 - y1;
	Draw_Fill(x1, y1, 1, dist, 2);          //Border - Left
	Draw_Fill(x2, y1, 1, dist + 1, 1);      //Border - Right

	Draw_AlphaFill(x1, y1, x2 - x1, y2 - y1, col, alpha);
}

void Draw_AMFStatLoss(int stat, hud_t* hud)
{
	static int *vxdmgcnt, *vxdmgcnt_t, *vxdmgcnt_o;
	static int x;
	static cvar_t* scale[2] = { NULL, NULL };
	static cvar_t* style[2];
	static cvar_t* digits[2];
	static cvar_t* align[2];
	static cvar_t* duration[2];
	static cvar_t* proportional[2];
	int index = (stat == STAT_HEALTH ? 0 : 1);
	float effect_duration;

	if (cl.spectator && Cam_TrackNum() != vxdamagepov) {
		Amf_Reset_DamageStats();
	}

	if (stat == STAT_HEALTH) {
		vxdmgcnt = &vxdamagecount;
		vxdmgcnt_t = &vxdamagecount_time;
		vxdmgcnt_o = &vxdamagecount_oldhealth;
		x = 136;
	}
	else {
		vxdmgcnt = &vxdamagecountarmour;
		vxdmgcnt_t = &vxdamagecountarmour_time;
		vxdmgcnt_o = &vxdamagecountarmour_oldhealth;
		x = 24;
	}

	if (scale[index] == NULL && hud) {
		// first time called
		scale[index] = HUD_FindVar(hud, "scale");
		style[index] = HUD_FindVar(hud, "style");
		digits[index] = HUD_FindVar(hud, "digits");
		align[index] = HUD_FindVar(hud, "align");
		duration[index] = HUD_FindVar(hud, "duration");
		proportional[index] = HUD_FindVar(hud, "proportional");
	}
	effect_duration = hud && duration[index] ? duration[index]->value : amf_stat_loss.value;

	//VULT STAT LOSS
	//Pretty self explanitory, I just thought it would be a nice feature to go with my "what the hell is going on?" theme
	//and obscure even more of the screen
	if (cl.stats[stat] < (*vxdmgcnt_o - 1)) {
		if (*vxdmgcnt_t > cl.time) {
			//add to damage
			*vxdmgcnt = *vxdmgcnt + (*vxdmgcnt_o - cl.stats[stat]);
		}
		else {
			*vxdmgcnt = *vxdmgcnt_o - cl.stats[stat];
		}
		*vxdmgcnt_t = cl.time + 2 * effect_duration;
	}
	*vxdmgcnt_o = cl.stats[stat];

	if (*vxdmgcnt_t > cl.time) {
		float alpha = min(1, (*vxdmgcnt_t - cl.time));
		float old_alpha = Draw_MultiplyOverallAlpha(alpha);
		if (hud) {
			SCR_HUD_DrawNum(hud, abs(*vxdmgcnt), 1, scale[index]->value, style[index]->value, digits[index]->integer, align[index]->string, proportional[index]->integer);
		}
		else {
			Sbar_DrawNum(x, -24, abs(*vxdmgcnt), 3, (*vxdmgcnt) > 0);
		}
		Draw_SetOverallAlpha(old_alpha);
	}
}

void InitVXStuff(void)
{
	int flags = TEX_COMPLAIN | TEX_MIPMAP | TEX_ALPHA | TEX_NOSCALE;
	int i;

	if (!qmb_initialized) {
		// FIXME: hm, in case of vid_restart, what we must do if before vid_restart qmb_initialized was true?
		return;
	}

	for (i = 0; i < CORONATEX_COUNT; ++i) {
		corona_textures[i].array_scale_s = 1;
		corona_textures[i].array_scale_t = 1;
		corona_textures[i].array_index = 0;
		R_TextureReferenceInvalidate(corona_textures[i].array_tex);
		R_TextureReferenceInvalidate(corona_textures[i].texnum);
	}

	corona_textures[CORONATEX_STANDARD].texnum = R_LoadTextureImage("textures/flash", NULL, 0, 0, flags);
	corona_textures[CORONATEX_GUNFLASH].texnum = R_LoadTextureImage("textures/gunflash", NULL, 0, 0, flags);
	corona_textures[CORONATEX_EXPLOSIONFLASH1].texnum = R_LoadTextureImage("textures/explosionflash1", NULL, 0, 0, flags);
	corona_textures[CORONATEX_EXPLOSIONFLASH2].texnum = R_LoadTextureImage("textures/explosionflash2", NULL, 0, 0, flags);
	corona_textures[CORONATEX_EXPLOSIONFLASH3].texnum = R_LoadTextureImage("textures/explosionflash3", NULL, 0, 0, flags);
	corona_textures[CORONATEX_EXPLOSIONFLASH4].texnum = R_LoadTextureImage("textures/explosionflash4", NULL, 0, 0, flags);
	corona_textures[CORONATEX_EXPLOSIONFLASH5].texnum = R_LoadTextureImage("textures/explosionflash5", NULL, 0, 0, flags);
	corona_textures[CORONATEX_EXPLOSIONFLASH6].texnum = R_LoadTextureImage("textures/explosionflash6", NULL, 0, 0, flags);
	corona_textures[CORONATEX_EXPLOSIONFLASH7].texnum = R_LoadTextureImage("textures/explosionflash7", NULL, 0, 0, flags);

	InitCoronas(); // safe re-init

	Init_VLights(); // safe re-init imo

	Cvar_SetCurrentGroup(CVAR_GROUP_SBAR);
	Cvar_Register(&amf_stat_loss);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_SPECTATOR);
	Cvar_Register(&cl_camera_tpp);
	Cvar_Register(&amf_camera_chase_dist);
	Cvar_Register(&amf_camera_chase_height);
	Cvar_Register(&amf_camera_death);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_EYECANDY);
	Cvar_Register(&amf_hiderockets);
	Cvar_Register(&amf_hidenails);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_LIGHTING);
	Cvar_Register(&amf_coronas);
	Cvar_Register(&amf_coronas_tele);
	Cvar_Register(&amf_lighting_vertex);
	Cvar_Register(&amf_lighting_colour);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_PARTICLES);
	Cvar_Register(&amf_part_blood);
	Cvar_Register(&amf_part_blood_color);
	Cvar_Register(&amf_part_blood_type);
	Cvar_Register(&amf_part_muzzleflash);
	Cvar_Register(&amf_part_fire);
	Cvar_Register(&amf_part_firecolor);
	Cvar_Register(&amf_part_deatheffect);
	Cvar_Register(&amf_part_gunshot);
	Cvar_Register(&amf_part_gunshot_type);
	Cvar_Register(&amf_part_spikes);
	Cvar_Register(&amf_part_spikes_type);
	Cvar_Register(&amf_part_explosion);
	Cvar_Register(&amf_part_blobexplosion);
	Cvar_Register(&amf_part_teleport);
	Cvar_Register(&amf_part_sparks);
	Cvar_Register(&amf_part_2dshockwaves);
	Cvar_Register(&amf_part_shockwaves);
	Cvar_Register(&amf_part_gibtrails);
	Cvar_Register(&amf_part_traillen);
	Cvar_Register(&amf_part_trailtime);
	Cvar_Register(&amf_part_traildetail);
	Cvar_Register(&amf_part_trailwidth);
	Cvar_Register(&amf_part_trailtype);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_TURB);
	Cvar_Register(&tei_lavafire);
	Cvar_Register(&tei_slime);
	Cvar_Register(&amf_weather_rain);
	Cvar_Register(&amf_weather_rain_fast);
	Cvar_Register(&amf_underwater_trails);
	Cvar_ResetCurrentGroup();

	Cvar_SetCurrentGroup(CVAR_GROUP_OPENGL);
	Cvar_Register(&amf_nailtrail);
	Cvar_Register(&amf_nailtrail_plasma);
	Cvar_Register(&amf_nailtrail_water);
	Cvar_Register(&amf_extratrails);
	Cvar_Register(&amf_detpacklights);
	Cvar_Register(&amf_buildingsparks);
	Cvar_Register(&amf_lightning);
	Cvar_Register(&amf_lightning_color);
	Cvar_Register(&amf_lightning_size);
	Cvar_Register(&amf_lightning_sparks);
	Cvar_Register(&amf_lightning_sparks_size);
	Cvar_Register(&amf_inferno_trail);
	Cvar_Register(&amf_inferno_speed);
	Cvar_Register(&amf_cutf_tesla_effect);
	Cvar_ResetCurrentGroup();
}


