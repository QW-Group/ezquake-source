/*
	Copyright (C) 2000-2003       Anton Gavrilov, A Nourai

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
#include "common.h"
#include "sound.h"
#include "pmove.h"
#include "teamplay.h"
#include "utils.h"
#include <time.h>
#include <string.h>


#include "ignore.h"
#include "rulesets.h"

qboolean OnChangeSkinForcing(cvar_t *var, char *string);	

cvar_t	cl_parseSay = {"cl_parseSay", "1"};
cvar_t	cl_parseFunChars = {"cl_parseFunChars", "1"};
cvar_t	tp_triggers = {"tp_triggers", "1"};
cvar_t	tp_msgtriggers = {"tp_msgtriggers", "1"};
cvar_t	tp_forceTriggers = {"tp_forceTriggers", "0"};
cvar_t	cl_nofake = {"cl_nofake", "0"};
cvar_t	tp_loadlocs = {"tp_loadlocs", "1"};


cvar_t	cl_teamskin = {"teamskin", "", 0, OnChangeSkinForcing};
cvar_t	cl_enemyskin = {"enemyskin", "", 0, OnChangeSkinForcing};
cvar_t	cl_teamquadskin = {"teamquadskin", "", 0, OnChangeSkinForcing};
cvar_t	cl_enemyquadskin = {"enemyquadskin", "", 0, OnChangeSkinForcing};
cvar_t	cl_teampentskin = {"teampentskin", "", 0, OnChangeSkinForcing};
cvar_t	cl_enemypentskin = {"enemypentskin", "", 0, OnChangeSkinForcing};
cvar_t	cl_teambothskin = {"teambothskin", "", 0, OnChangeSkinForcing};
cvar_t	cl_enemybothskin = {"enemybothskin", "", 0, OnChangeSkinForcing};


cvar_t  tp_soundtrigger = {"tp_soundtrigger", "~"};

cvar_t	tp_name_axe = {"tp_name_axe", "axe"};
cvar_t	tp_name_sg = {"tp_name_sg", "sg"};
cvar_t	tp_name_ssg = {"tp_name_ssg", "ssg"};
cvar_t	tp_name_ng = {"tp_name_ng", "ng"};
cvar_t	tp_name_sng = {"tp_name_sng", "sng"};
cvar_t	tp_name_gl = {"tp_name_gl", "gl"};
cvar_t	tp_name_rl = {"tp_name_rl", "rl"};
cvar_t	tp_name_lg = {"tp_name_lg", "lg"};
cvar_t	tp_name_armortype_ra = {"tp_name_armortype_ra", "r"};
cvar_t	tp_name_armortype_ya = {"tp_name_armortype_ya", "y"};
cvar_t	tp_name_armortype_ga = {"tp_name_armortype_ga", "g"};
cvar_t	tp_name_ra = {"tp_name_ra", "ra"};
cvar_t	tp_name_ya = {"tp_name_ya", "ya"};
cvar_t	tp_name_ga = {"tp_name_ga", "ga"};
cvar_t	tp_name_quad = {"tp_name_quad", "quad"};
cvar_t	tp_name_pent = {"tp_name_pent", "pent"};
cvar_t	tp_name_ring = {"tp_name_ring", "ring"};
cvar_t	tp_name_suit = {"tp_name_suit", "suit"};
cvar_t	tp_name_shells = {"tp_name_shells", "shells"};
cvar_t	tp_name_nails = {"tp_name_nails", "nails"};
cvar_t	tp_name_rockets = {"tp_name_rockets", "rockets"};
cvar_t	tp_name_cells = {"tp_name_cells", "cells"};
cvar_t	tp_name_mh = {"tp_name_mh", "mega"};
cvar_t	tp_name_health = {"tp_name_health", "health"};
cvar_t	tp_name_armor = {"tp_name_armor", "armor"};
cvar_t	tp_name_weapon = {"tp_name_weapon", "weapon"};
cvar_t	tp_name_backpack = {"tp_name_backpack", "pack"};
cvar_t	tp_name_flag = {"tp_name_flag", "flag"};
cvar_t	tp_name_sentry = {"tp_name_sentry", "sentry gun"};
cvar_t	tp_name_disp = {"tp_name_disp", "dispenser"};
cvar_t	tp_name_rune_1 = {"tp_name_rune_1", "resistance rune"};
cvar_t	tp_name_rune_2 = {"tp_name_rune_2", "strength rune"};
cvar_t	tp_name_rune_3 = {"tp_name_rune_3", "haste rune"};
cvar_t	tp_name_rune_4 = {"tp_name_rune_4", "regeneration rune"};
cvar_t	tp_name_teammate = {"tp_name_teammate", ""};
cvar_t	tp_name_enemy = {"tp_name_enemy", "enemy"};
cvar_t	tp_name_eyes = {"tp_name_eyes", "eyes"};
cvar_t	tp_name_quaded = {"tp_name_quaded", "quaded"};
cvar_t	tp_name_pented = {"tp_name_pented", "pented"};
cvar_t	tp_name_nothing = {"tp_name_nothing", "nothing"};
cvar_t	tp_name_someplace = {"tp_name_someplace", "someplace"};
cvar_t	tp_name_at = {"tp_name_at", "at"};
cvar_t	tp_name_none = {"tp_name_none", ""};				
cvar_t	tp_name_separator = {"tp_name_separator", "/"};		
cvar_t	tp_weapon_order = {"tp_weapon_order", "78654321"};	

cvar_t	tp_name_status_green = {"tp_name_status_green", "$G"};
cvar_t	tp_name_status_yellow = {"tp_name_status_yellow", "$Y"};
cvar_t	tp_name_status_red = {"tp_name_status_red", "$R"};
cvar_t	tp_name_status_blue = {"tp_name_status_blue", "$B"};

cvar_t	tp_need_ra = {"tp_need_ra", "50"};
cvar_t	tp_need_ya = {"tp_need_ya", "50"};
cvar_t	tp_need_ga = {"tp_need_ga", "50"};
cvar_t	tp_need_health = {"tp_need_health", "50"};
cvar_t	tp_need_weapon = {"tp_need_weapon", "35687"};
cvar_t	tp_need_rl = {"tp_need_rl", "1"};
cvar_t	tp_need_rockets = {"tp_need_rockets", "5"};
cvar_t	tp_need_cells = {"tp_need_cells", "20"};
cvar_t	tp_need_nails = {"tp_need_nails", "40"};
cvar_t	tp_need_shells = {"tp_need_shells", "10"};

static qboolean suppress;		

char *skinforcing_team = "";	

void TP_FindModelNumbers (void);
void TP_FindPoint (void);
char *TP_LocationName (vec3_t location);
static void CountNearbyPlayers(qboolean dead);	
char *Macro_LastTookOrPointed (void);
char *Macro_LastTookOrPointed2 (void);

#define	POINT_TYPE_ITEM			1
#define POINT_TYPE_POWERUP		2
#define POINT_TYPE_TEAMMATE		3
#define	POINT_TYPE_ENEMY		4

#define	TP_TOOK_EXPIRE_TIME		15
#define	TP_POINT_EXPIRE_TIME	TP_TOOK_EXPIRE_TIME

#define MAX_LOC_NAME			64

// this structure is cleared after entering a new map
typedef struct tvars_s {
	int		health;
	int		items;
	int		olditems;
	int		activeweapon;
	int		stat_framecounts[MAX_CL_STATS];
	double	deathtrigger_time;
	float	f_skins_reply_time;
	float	f_version_reply_time;
	char	lastdeathloc[MAX_LOC_NAME];
	char	tookname[32];
	char	tookloc[MAX_LOC_NAME];
	double	tooktime;
	double	pointtime;					// cls.realtime for which pointitem & pointloc are valid
	char	pointname[32];
	char	pointloc[MAX_LOC_NAME];
	int		pointtype;
	char	nearestitemloc[MAX_LOC_NAME];
	char	lastreportedloc[MAX_LOC_NAME];
	double	lastdrop_time;				
	char	lastdroploc[MAX_LOC_NAME];	
	char	lasttrigger_match[256];	

	int		numenemies;
	int		numfriendlies;
	int		last_numenemies;
	int		last_numfriendlies;

    int enemy_powerups;
    double enemy_powerups_time;

} tvars_t;

tvars_t vars;

/********************************** TRIGGERS **********************************/

typedef struct f_trigger_s {
	char *name;
	int restricted;
	qboolean teamplay;
} f_trigger_t;

f_trigger_t f_triggers[] = {
	{"f_newmap", 0, false},
	{"f_spawn", 0, false},
	{"f_mapend", 0, false},
	{"f_reloadstart", 0, false},
	{"f_reloadend", 0, false},

	{"f_weaponchange", 1, false},

	{"f_took", 2, true},
	{"f_respawn", 1, true},
	{"f_death", 1, true},
	{"f_flagdeath", 1, true},
};

#define num_f_triggers	(sizeof(f_triggers) / sizeof(f_triggers[0]))

void TP_ExecTrigger (char *trigger) {
	int i, j, numteammates;
	cmd_alias_t *alias;

	if (!tp_triggers.value || cls.demoplayback || cl.spectator)
		return;

	for (i = 0; i < num_f_triggers; i++) {
		if (!strcmp(f_triggers[i].name, trigger))
			break;
	}
	if (i == num_f_triggers)
		Sys_Error("Unknown f_trigger \"%s\"", trigger);

	if (f_triggers[i].teamplay && !tp_forceTriggers.value) {
		if (!cl.teamplay)
			return;

		numteammates = 0;
		for (j = 0; j < MAX_CLIENTS; j++) {
			if (cl.players[j].name[0] && !cl.players[j].spectator && j != cl.playernum) {
				if (!strcmp(cl.players[j].team, cl.players[cl.playernum].team))
					numteammates++;
			}
		}

		if (!numteammates)
			return;
	}

	if ((alias = Cmd_FindAlias(trigger))) {
		if (f_triggers[i].restricted && Rulesets_RestrictTriggers()) {
			if (f_triggers[i].restricted < 2)
				Cbuf_AddTextEx (&cbuf_nocomms, va("%s\n", alias->value));
		} else {
			Cbuf_AddTextEx (&cbuf_main, va("%s\n", alias->value));
		}
	}
}

/*********************************** MACROS ***********************************/

#define MAX_MACRO_VALUE	256
static char	macro_buf[MAX_MACRO_VALUE] = "";

char *Macro_Quote_f (void) {
	return "\"";
}

char *Macro_Latency (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", Q_rint(cls.latency * 1000));
	return macro_buf;
}

char *Macro_Health (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_HEALTH]);
	return macro_buf;
}

char *Macro_Armor (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_ARMOR]);
	return macro_buf;
}

char *Macro_Shells (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_SHELLS]);
	return macro_buf;
}

char *Macro_Nails (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_NAILS]);
	return macro_buf;
}

char *Macro_Rockets (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_ROCKETS]);
	return macro_buf;
}

char *Macro_Cells (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_CELLS]);
	return macro_buf;
}

char *Macro_Ammo (void) {
	Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_AMMO]);
	return macro_buf;
}

static char *Weapon_NumToString(int num) {
	switch (num) {
	case IT_AXE: return tp_name_axe.string;
	case IT_SHOTGUN: return tp_name_sg.string;
	case IT_SUPER_SHOTGUN: return tp_name_ssg.string;
	case IT_NAILGUN: return tp_name_ng.string;
	case IT_SUPER_NAILGUN: return tp_name_sng.string;
	case IT_GRENADE_LAUNCHER: return tp_name_gl.string;
	case IT_ROCKET_LAUNCHER: return tp_name_rl.string;
	case IT_LIGHTNING: return tp_name_lg.string;
	default: return tp_name_none.string;		
	}
}

char *Macro_Weapon (void) {
	return Weapon_NumToString(cl.stats[STAT_ACTIVEWEAPON]);
}

char *Macro_WeaponAndAmmo (void) {
	char buf[MAX_MACRO_VALUE];
	Q_snprintfz (buf, sizeof(buf), "%s:%s", Macro_Weapon(), Macro_Ammo());
	Q_strncpyz (macro_buf, buf, sizeof(macro_buf));
	return macro_buf;
}

char *Macro_WeaponNum (void) {
	switch (cl.stats[STAT_ACTIVEWEAPON]) {

	case IT_AXE: return "1";
	case IT_SHOTGUN: return "2";
	case IT_SUPER_SHOTGUN: return "3";
	case IT_NAILGUN: return "4";
	case IT_SUPER_NAILGUN: return "5";
	case IT_GRENADE_LAUNCHER: return "6";
	case IT_ROCKET_LAUNCHER: return "7";
	case IT_LIGHTNING: return "8";
	default:
		return "0";
	}
}

static int BestWeapon (void) {
	int i;
	char *t[] = {tp_weapon_order.string, "78654321", NULL}, **s;	

	for (s = t; *s; s++) {
		for (i = 0 ; i < strlen(*s) ; i++) {
			switch ((*s)[i]) {
				case '1': if (cl.stats[STAT_ITEMS] & IT_AXE) return IT_AXE; break;
				case '2': if (cl.stats[STAT_ITEMS] & IT_SHOTGUN) return IT_SHOTGUN; break;
				case '3': if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return IT_SUPER_SHOTGUN; break;
				case '4': if (cl.stats[STAT_ITEMS] & IT_NAILGUN) return IT_NAILGUN; break;
				case '5': if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return IT_SUPER_NAILGUN; break;
				case '6': if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return IT_GRENADE_LAUNCHER; break;
				case '7': if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return IT_ROCKET_LAUNCHER; break;
				case '8': if (cl.stats[STAT_ITEMS] & IT_LIGHTNING) return IT_LIGHTNING; break;
			}
		}
	}
	return 0;
}

char *Macro_BestWeapon (void) {
	switch (BestWeapon()) {
	case IT_AXE: return tp_name_axe.string;
	case IT_SHOTGUN: return tp_name_sg.string;
	case IT_SUPER_SHOTGUN: return tp_name_ssg.string;
	case IT_NAILGUN: return tp_name_ng.string;
	case IT_SUPER_NAILGUN: return tp_name_sng.string;
	case IT_GRENADE_LAUNCHER: return tp_name_gl.string;
	case IT_ROCKET_LAUNCHER: return tp_name_rl.string;
	case IT_LIGHTNING: return tp_name_lg.string;
	default: return tp_name_none.string;	
	}
}

char *Macro_BestAmmo (void) {
	switch (BestWeapon()) {
	case IT_SHOTGUN: case IT_SUPER_SHOTGUN:
		Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_SHELLS]);
		return macro_buf;

	case IT_NAILGUN: case IT_SUPER_NAILGUN:
		Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_NAILS]);
		return macro_buf;

	case IT_GRENADE_LAUNCHER: case IT_ROCKET_LAUNCHER:
		Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_ROCKETS]);
		return macro_buf;

	case IT_LIGHTNING:
		Q_snprintfz(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_CELLS]);
		return macro_buf;

	default: return "0";
	}
}

// needed for %b parsing
char *Macro_BestWeaponAndAmmo (void) {
	char buf[MAX_MACRO_VALUE];

	Q_snprintfz (buf, sizeof(buf), "%s:%s", Macro_BestWeapon(), Macro_BestAmmo());
	Q_strncpyz (macro_buf, buf, sizeof(buf));
	return macro_buf;
}

char *Macro_ArmorType (void) {
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
		return tp_name_armortype_ga.string;
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
		return tp_name_armortype_ya.string;
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
		return tp_name_armortype_ra.string;
	else
		return tp_name_none.string;
}


#define Q_strncatz(dest, src)	\
	do {	\
		strncat(dest, src, sizeof(dest) - strlen(dest) - 1);	\
		dest[sizeof(dest) - 1] = 0;	\
	} while (0)

char *Macro_Powerups (void) {
	int effects;

	macro_buf[0] = 0;

	if (cl.stats[STAT_ITEMS] & IT_QUAD)
		Q_strncpyz(macro_buf, tp_name_quad.string, sizeof(macro_buf));

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_pent.string);
	}

	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_ring.string);
	}

	effects = cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum].effects;
	if ( (effects & (EF_FLAG1|EF_FLAG2)) /* CTF */ ||		
		(cl.teamfortress && cl.stats[STAT_ITEMS] & (IT_KEY1|IT_KEY2)) /* TF */ ) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_flag.string);
	}

	if (!macro_buf[0])			
		Q_strncpyz(macro_buf, tp_name_none.string, sizeof(macro_buf));

	return macro_buf;
}

char *Macro_Location (void) {
	Q_strncpyz(vars.lastreportedloc, TP_LocationName (cl.simorg), sizeof(vars.lastreportedloc));
	return vars.lastreportedloc;
}

char *Macro_LastDeath (void) {
	return vars.deathtrigger_time ? vars.lastdeathloc : tp_name_someplace.string;
}

char *Macro_Last_Location (void) {
	if (vars.deathtrigger_time && cls.realtime - vars.deathtrigger_time <= 5)
		Q_strncpyz(vars.lastreportedloc, vars.lastdeathloc, sizeof(vars.lastreportedloc));
	else		
		Q_strncpyz(vars.lastreportedloc, TP_LocationName (cl.simorg), sizeof(vars.lastreportedloc));
	return vars.lastreportedloc;
}

char *Macro_LastReportedLoc(void) {
	if (!vars.lastreportedloc[0])
		return tp_name_someplace.string;
	return vars.lastreportedloc;
}

char *Macro_Time (void) {
	time_t t;
	struct tm *ptm;

	time (&t);
	if (!(ptm = localtime (&t)))
		return "#bad date#";
	strftime (macro_buf, sizeof(macro_buf) - 1, "%H:%M", ptm);
	return macro_buf;
}

char *Macro_Date (void) {
	time_t t;
	struct tm *ptm;

	time (&t);
	if (!(ptm = localtime (&t)))
		return "#bad date#";
	strftime (macro_buf, sizeof(macro_buf) - 1, "%d.%m.%y", ptm);
	return macro_buf;
}

// returns the last item picked up
char *Macro_Took (void) {
	if (!vars.tooktime || cls.realtime > vars.tooktime + TP_TOOK_EXPIRE_TIME)
		Q_strncpyz (macro_buf, tp_name_nothing.string, sizeof(macro_buf));
	else
		Q_strncpyz (macro_buf, vars.tookname, sizeof(macro_buf));
	return macro_buf;
}

// returns location of the last item picked up
char *Macro_TookLoc (void) {
	if (!vars.tooktime || cls.realtime > vars.tooktime + TP_TOOK_EXPIRE_TIME)
		Q_strncpyz (macro_buf, tp_name_someplace.string, sizeof(macro_buf));
	else
		Q_strncpyz (macro_buf, vars.tookloc, sizeof(macro_buf));
	return macro_buf;
}

// %i macro - last item picked up in "name at location" style
char *Macro_TookAtLoc (void) {
	if (!vars.tooktime || cls.realtime > vars.tooktime + TP_TOOK_EXPIRE_TIME)
		Q_strncpyz (macro_buf, tp_name_nothing.string, sizeof(macro_buf));
	else {
		Q_strncpyz (macro_buf, va("%s %s %s", vars.tookname, tp_name_at.string, vars.tookloc), sizeof(macro_buf));
	}
	return macro_buf;
}

// pointing calculations are CPU expensive, so the results are cached
// in vars.pointname & vars.pointloc
char *Macro_PointName (void) {
	TP_FindPoint ();
	return vars.pointname;
}

char *Macro_PointLocation (void) {
	TP_FindPoint ();
	return vars.pointloc[0] ? vars.pointloc : Macro_Location();
}

char *Macro_LastPointAtLoc (void) {
	if (!vars.pointtime || cls.realtime - vars.pointtime > TP_POINT_EXPIRE_TIME)
		Q_strncpyz (macro_buf, tp_name_nothing.string, sizeof(macro_buf));
	else
		Q_snprintfz (macro_buf, sizeof(macro_buf), "%s %s %s", vars.pointname, tp_name_at.string, vars.pointloc[0] ? vars.pointloc : Macro_Location());		
	return macro_buf;
}

char *Macro_PointNameAtLocation(void) {
	TP_FindPoint();
	return Macro_LastPointAtLoc();
}

char *Macro_Weapons (void) {	
	macro_buf[0] = 0;

	if (cl.stats[STAT_ITEMS] & IT_LIGHTNING)
		Q_strncpyz(macro_buf, tp_name_lg.string, sizeof(macro_buf));

	if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_rl.string);
	}
	if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_gl.string);
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_sng.string);
	}
	if (cl.stats[STAT_ITEMS] & IT_NAILGUN) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_ng.string);
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_ssg.string);
	}
	if (cl.stats[STAT_ITEMS] & IT_SHOTGUN) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_sg.string);
	}
	if (cl.stats[STAT_ITEMS] & IT_AXE) {
		if (macro_buf[0])
			Q_strncatz(macro_buf, tp_name_separator.string);
		Q_strncatz(macro_buf, tp_name_axe.string);
	}

	if (!macro_buf[0])	
		Q_strncpyz(macro_buf, tp_name_none.string, sizeof(macro_buf));

	return macro_buf;
}

static char *Skin_To_TFSkin (char *myskin) {
	if (!cl.teamfortress || cl.spectator || Q_strncasecmp(myskin, "tf_", 3)) {
		Q_strncpyz(macro_buf, myskin, sizeof(macro_buf));
	} else {
		if (!Q_strcasecmp(myskin, "tf_demo"))
			Q_strncpyz(macro_buf, "demoman", sizeof(macro_buf));
		else if (!Q_strcasecmp(myskin, "tf_eng"))
			Q_strncpyz(macro_buf, "engineer", sizeof(macro_buf));
		else if (!Q_strcasecmp(myskin, "tf_snipe"))
			Q_strncpyz(macro_buf, "sniper", sizeof(macro_buf));
		else if (!Q_strcasecmp(myskin, "tf_sold"))
			Q_strncpyz(macro_buf, "soldier", sizeof(macro_buf));
		else
			Q_strncpyz(macro_buf, myskin + 3, sizeof(macro_buf));
	}
	return macro_buf;
}

char *Macro_TF_Skin (void) {
	return Skin_To_TFSkin(Info_ValueForKey(cl.players[cl.playernum].userinfo, "skin"));
}

char *Macro_LastDrop (void)	{			
	if (vars.lastdrop_time)
		return vars.lastdroploc;
	else
		return tp_name_someplace.string;
}

char *Macro_LastTrigger_Match(void) {	
	return vars.lasttrigger_match;
}

char *Macro_LastDropTime (void)	{		
	if (vars.lastdrop_time)
		Q_snprintfz (macro_buf, 32, "%d", (int) (cls.realtime - vars.lastdrop_time));
	else
		Q_snprintfz (macro_buf, 32, "%d", -1);
	return macro_buf;
}

char *Macro_Need (void) {
	int i, weapon;
	char *needammo = NULL;

	macro_buf[0] = 0;

	// check armor
	if (   ((cl.stats[STAT_ITEMS] & IT_ARMOR1) && cl.stats[STAT_ARMOR] < tp_need_ga.value)
		|| ((cl.stats[STAT_ITEMS] & IT_ARMOR2) && cl.stats[STAT_ARMOR] < tp_need_ya.value)
		|| ((cl.stats[STAT_ITEMS] & IT_ARMOR3) && cl.stats[STAT_ARMOR] < tp_need_ra.value)
		|| (!(cl.stats[STAT_ITEMS] & (IT_ARMOR1|IT_ARMOR2|IT_ARMOR3))
			&& (tp_need_ga.value || tp_need_ya.value || tp_need_ra.value)))
		Q_strncpyz (macro_buf, tp_name_armor.string, sizeof(macro_buf));

	// check health
	if (tp_need_health.value && cl.stats[STAT_HEALTH] < tp_need_health.value) {
		if (macro_buf[0])
			Q_strncatz (macro_buf, tp_name_separator.string);
		Q_strncatz (macro_buf, tp_name_health.string);
	}

	if (cl.teamfortress) {					
		if (cl.stats[STAT_ROCKETS] < tp_need_rockets.value)	{
			if (macro_buf[0])
				Q_strncatz (macro_buf, tp_name_separator.string);
			Q_strncatz (macro_buf, tp_name_rockets.string);
		}
		if (cl.stats[STAT_SHELLS] < tp_need_shells.value) {
			if (macro_buf[0])
				Q_strncatz (macro_buf, tp_name_separator.string);
			Q_strncatz (macro_buf, tp_name_shells.string);
		}
		if (cl.stats[STAT_NAILS] < tp_need_nails.value)	{
			if (macro_buf[0])
				Q_strncatz (macro_buf, tp_name_separator.string);
			Q_strncatz (macro_buf, tp_name_nails.string);
		}
		if (cl.stats[STAT_CELLS] < tp_need_cells.value)	{
			if (macro_buf[0])
				Q_strncatz (macro_buf, tp_name_separator.string);
			Q_strncatz (macro_buf, tp_name_cells.string);
		}
		goto done;
	}
	// check weapon
	weapon = 0;
	for (i = strlen(tp_need_weapon.string) - 1 ; i >= 0 ; i--) {
		switch (tp_need_weapon.string[i]) {
			case '2': if (cl.stats[STAT_ITEMS] & IT_SHOTGUN) weapon = 2; break;
			case '3': if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) weapon = 3; break;
			case '4': if (cl.stats[STAT_ITEMS] & IT_NAILGUN) weapon = 4; break;
			case '5': if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN) weapon = 5; break;
			case '6': if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) weapon = 6; break;
			case '7': if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) weapon = 7; break;
			case '8': if (cl.stats[STAT_ITEMS] & IT_LIGHTNING) weapon = 8; break;
		}
		if (weapon)
			break;
	}

	if (!weapon) {
		if (macro_buf[0])
			Q_strncatz (macro_buf, tp_name_separator.string);
		Q_strncatz (macro_buf, tp_name_weapon.string);
	} else {
		if (tp_need_rl.value && !(cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)) {
			if (macro_buf[0])
				Q_strncatz (macro_buf, tp_name_separator.string);
			Q_strncatz (macro_buf, tp_name_rl.string);
		}

		switch (weapon) {
			case 2: case 3: if (cl.stats[STAT_SHELLS] < tp_need_shells.value) 
								needammo = tp_name_shells.string;
							break;
			case 4: case 5: if (cl.stats[STAT_NAILS] < tp_need_nails.value) 
								needammo = tp_name_nails.string;
							break;
			case 6: case 7: if (cl.stats[STAT_ROCKETS] < tp_need_rockets.value) 
								needammo = tp_name_rockets .string;
							break;
			case 8: if (cl.stats[STAT_CELLS] < tp_need_cells.value) 
								needammo = tp_name_cells.string;
							break;
		}

		if (needammo) {
			if (macro_buf[0])
				Q_strncatz (macro_buf, tp_name_separator.string);
			Q_strncatz (macro_buf, needammo);
		}
	}

done:
	if (!macro_buf[0])
		Q_strncpyz (macro_buf, tp_name_nothing.string, sizeof(macro_buf));

	return macro_buf;
}

char *Macro_Point_LED(void) {
	TP_FindPoint();

	if (vars.pointtype == POINT_TYPE_ENEMY)
		return tp_name_status_red.string;
	else if (vars.pointtype == POINT_TYPE_TEAMMATE)
		return tp_name_status_green.string;
	else if (vars.pointtype == POINT_TYPE_POWERUP)
		return tp_name_status_yellow.string;
	else	// POINT_TYPE_ITEM
		return tp_name_status_blue.string;

	return macro_buf;
}



char *Macro_MyStatus_LED(void) {
	int count;
	float save_need_rl;
	char *s, *save_separator;
	static char separator[] = {'/', '\0'};

	save_need_rl = tp_need_rl.value;
	save_separator = tp_name_separator.string;
	tp_need_rl.value = 0;
	tp_name_separator.string = separator;
	s = Macro_Need();
	tp_need_rl.value = save_need_rl;
	tp_name_separator.string = save_separator;

	if (!strcmp(s, tp_name_nothing.string)) {
		count = 0;
	} else  {
		for (count = 1; *s; s++)
			if (*s == separator[0])
				count++;
	}	

	if (count == 0)
		Q_snprintfz(macro_buf, sizeof(macro_buf), "%s", tp_name_status_green.string);
	else if (count <= 1)
		Q_snprintfz(macro_buf, sizeof(macro_buf), "%s", tp_name_status_yellow.string);
	else
		Q_snprintfz(macro_buf, sizeof(macro_buf), "%s", tp_name_status_red.string);	

	return macro_buf;
}

char *Macro_EnemyStatus_LED(void) {
	CountNearbyPlayers(false);
	if (vars.numenemies == 0)
		Q_snprintfz(macro_buf, sizeof(macro_buf), "\xffl%s\xff", tp_name_status_green.string);
	else if (vars.numenemies <= vars.numfriendlies)
		Q_snprintfz(macro_buf, sizeof(macro_buf), "\xffl%s\xff", tp_name_status_yellow.string);
	else
		Q_snprintfz(macro_buf, sizeof(macro_buf), "\xffl%s\xff", tp_name_status_red.string);

	suppress = true;
	return macro_buf;
}


#define TP_PENT 1
#define TP_QUAD 2
#define TP_RING 4

char *Macro_LastSeenPowerup(void) {
	if (!vars.enemy_powerups_time || cls.realtime - vars.enemy_powerups_time > 5) {
		Q_strncpyz(macro_buf, tp_name_quad.string, sizeof(macro_buf));
	} else {
		macro_buf[0] = 0;
		if (vars.enemy_powerups & TP_QUAD)
			Q_strncatz(macro_buf, tp_name_quad.string);
		if (vars.enemy_powerups & TP_PENT) {
			if (macro_buf[0])  
				Q_strncatz(macro_buf, tp_name_separator.string);
			Q_strncatz(macro_buf, tp_name_pent.string);
		}
		if (vars.enemy_powerups & TP_RING) {
			if (macro_buf[0])  
				Q_strncatz(macro_buf, tp_name_separator.string);
			Q_strncatz(macro_buf, tp_name_ring.string);
		}
	}
	return macro_buf;
}


qboolean TP_SuppressMessage(char *buf) {
	char *s;

	for (s = buf; *s && *s != 0x7f; s++)
		;

	if (*s == 0x7f && *(s + 1) == '!')	{
		*s++ = '\n';
		*s++ = 0;

		return (!cls.demoplayback && !cl.spectator && *s - 'A' == cl.playernum);
	}
	return false;
}

void TP_PrintHiddenMessage(char *buf) {
    qboolean hide = false;
    char dest[4096], msg[4096], *s, *d, c, *name;
	int length, offset, flags;
	extern cvar_t cl_chatsound;

	if (!buf || !(length = strlen(buf)))
		return;

	if (length >= 2 && buf[0] == '\"' && buf[length - 1] == '\"') {
		memmove(buf, buf + 1, length - 2);
		buf[length - 2] = 0;
	}

	s = buf;
	d = dest;

    while ((c = *s++)) {
        if (c == '\xff') {
			if ((hide = !hide)) {
				*d++ = (*s == 'z') ? 'x' : 139;	
				s++;
				memmove(s - 2, s, strlen(s) + 1);
				s -= 2;
			} else {
				memmove(s - 1, s, strlen(s) + 1);
				s -= 1;
			}
		} else if (!hide) {
			*d++ = c;
		}
	}
	*d = 0;

	if (cls.demoplayback)
		return;

	name = Info_ValueForKey (cl.players[cl.playernum].userinfo, "name");
	if (strlen(name) >= 32)	
		name[31] = 0;

	if (!strcmp("say", Cmd_Argv(0)))
		Q_snprintfz(msg, sizeof(msg), "%s: %s\n", name, TP_ParseFunChars(dest, true));
	else
		Q_snprintfz(msg, sizeof(msg), "(%s): %s\n", name, TP_ParseFunChars(dest, true));

	flags = TP_CategorizeMessage (msg, &offset);

	if (flags == 2 && !TP_FilterMessage(msg + offset))
		return;

	if (cl_chatsound.value)
		S_LocalSound ("misc/talk.wav");

	if (cl_nofake.value == 1 || (cl_nofake.value == 2 && flags != 2)) {
		for (s = msg; *s; s++)
			if (*s == 0x0D || (*s == 0x0A && s[1]))
				*s = ' ';
	}

	Com_Printf(TP_ParseWhiteText (msg, false, offset));
}

#define ISDEAD(i) ( (i) >= 41 && (i) <= 102 )

static void CountNearbyPlayers(qboolean dead) {
	int i;
	player_state_t *state;
	player_info_t *info;
	static int lastframecount = -1;

	if (cls.framecount == lastframecount)
		return;
	lastframecount = cls.framecount;

	vars.numenemies = vars.numfriendlies = 0;

	if (!cl.spectator && !dead)
		vars.numfriendlies++;

	if (!cl.oldparsecount || !cl.parsecount || cls.state < ca_active)
		return;

	state = cl.frames[cl.oldparsecount & UPDATE_MASK].playerstate;
	info = cl.players;
	for (i = 0; i < MAX_CLIENTS; i++, info++, state++) {			
		if (i != cl.playernum && state->messagenum == cl.oldparsecount && !info->spectator && !ISDEAD(state->frame)) {
			if (cl.teamplay && !strcmp(info->team, TP_PlayerTeam()))
				vars.numfriendlies++;
			else
				vars.numenemies++;
		}
	}
}


char *Macro_CountNearbyEnemyPlayers (void) {
	CountNearbyPlayers(false);
	sprintf(macro_buf, "\xffz%d\xff", vars.numenemies);
	suppress = true;
	return macro_buf;
}


char *Macro_Count_Last_NearbyEnemyPlayers (void) {
	if (vars.deathtrigger_time && cls.realtime - vars.deathtrigger_time <= 5) {
		sprintf(macro_buf, "\xffz%d\xff", vars.last_numenemies);
	} else {
		CountNearbyPlayers(false);
		sprintf(macro_buf, "\xffz%d\xff", vars.numenemies);
	}
	suppress = true;
	return macro_buf;
}


char *Macro_CountNearbyFriendlyPlayers (void) {
	CountNearbyPlayers(false);
	sprintf(macro_buf, "\xffz%d\xff", vars.numfriendlies);
	suppress = true;
	return macro_buf;
}


char *Macro_Count_Last_NearbyFriendlyPlayers (void) {
	if (vars.deathtrigger_time && cls.realtime - vars.deathtrigger_time <= 5) {
		sprintf(macro_buf, "\xffz%d\xff", vars.last_numfriendlies);
	} else {
		CountNearbyPlayers(false);
		sprintf(macro_buf, "\xffz%d\xff", vars.numfriendlies);
	}
	suppress = true;
	return macro_buf;
}

// Note: longer macro names like "armortype" must be defined
// _before_ the shorter ones like "armor" to be parsed properly
void TP_AddMacros(void) {
	Cmd_AddMacro("qt", Macro_Quote_f);

	Cmd_AddMacro("latency", Macro_Latency);
	Cmd_AddMacro("health", Macro_Health);
	Cmd_AddMacro("armortype", Macro_ArmorType);
	Cmd_AddMacro("armor", Macro_Armor);

	Cmd_AddMacro("shells", Macro_Shells);
	Cmd_AddMacro("nails", Macro_Nails);
	Cmd_AddMacro("rockets", Macro_Rockets);
	Cmd_AddMacro("cells", Macro_Cells);

	Cmd_AddMacro("weaponnum", Macro_WeaponNum);
	Cmd_AddMacro("weapons", Macro_Weapons);						
	Cmd_AddMacro("weapon", Macro_Weapon);

	Cmd_AddMacro("ammo", Macro_Ammo);

	Cmd_AddMacro("bestweapon", Macro_BestWeapon);
	Cmd_AddMacro("bestammo", Macro_BestAmmo);

	Cmd_AddMacro("powerups", Macro_Powerups);

	Cmd_AddMacro("location", Macro_Location);
	Cmd_AddMacro("deathloc", Macro_LastDeath);

	Cmd_AddMacro("time", Macro_Time);
	Cmd_AddMacro("date", Macro_Date);

	Cmd_AddMacro("tookatloc", Macro_TookAtLoc);
	Cmd_AddMacro("tookloc", Macro_TookLoc);
	Cmd_AddMacro("took", Macro_Took);

	Cmd_AddMacro("pointatloc", Macro_PointNameAtLocation);
	Cmd_AddMacro("pointloc", Macro_PointLocation);
	Cmd_AddMacro("point", Macro_PointName);

	Cmd_AddMacro("need", Macro_Need);

	Cmd_AddMacro("droploc", Macro_LastDrop);					
	Cmd_AddMacro("droptime", Macro_LastDropTime);				

	Cmd_AddMacro("tf_skin", Macro_TF_Skin);						

	Cmd_AddMacro("triggermatch", Macro_LastTrigger_Match);		


	Cmd_AddMacro("ledpoint", Macro_Point_LED);
	Cmd_AddMacro("ledstatus", Macro_MyStatus_LED);

};

/********************** MACRO/FUNCHAR/WHITE TEXT PARSING **********************/

#define MAX_MACRO_STRING 2048

char *TP_ParseWhiteText(char *s, qboolean team, int offset) {
	static char	buf[4096];	
	char *out, *p, *p1;
	extern cvar_t	cl_parseWhiteText;
	qboolean	parsewhite;

	parsewhite = cl_parseWhiteText.value == 1 || (cl_parseWhiteText.value == 2 && team);

	buf[0] = 0;
	out = buf;

	for (p = s; *p; p++) {
		if  (parsewhite && *p == '{' && p-s >= offset) {
			if ((p1 = strchr (p + 1, '}'))) {
				memcpy (out, p + 1, p1 - p - 1);
				out += p1 - p - 1;
				p = p1;
				continue;
			}
		}
		if (*p != 10 && *p != 13 && !(p==s && (*p==1 || *p==2)))
			*out++ = *p | 128;	// convert to red
		else
			*out++ = *p;
	}
	*out = 0;
	return buf;
}

//Parses %a-like expressions
char *TP_ParseMacroString (char *s) {
	static char	buf[MAX_MACRO_STRING];
	int i = 0;
	char *macro_string;

	if (!cl_parseSay.value)
		return s;

	suppress = false;		

	while (*s && i < MAX_MACRO_STRING - 1) {
		// check %[P], etc
		if (*s == '%' && s[1]=='[' && s[2] && s[3]==']') {
			static char mbuf[MAX_MACRO_VALUE];

			switch (s[2]) {
			case 'a':
				macro_string = Macro_ArmorType();
				if (!strcmp(macro_string, tp_name_none.string))
					macro_string = "a";
				if (cl.stats[STAT_ARMOR] < 30)
					Q_snprintfz (mbuf, sizeof(mbuf), "\x10%s:%i\x11", macro_string, cl.stats[STAT_ARMOR]);
				else
					Q_snprintfz (mbuf, sizeof(mbuf), "%s:%i", macro_string, cl.stats[STAT_ARMOR]);
				macro_string = mbuf;
				break;

			case 'h':
				if (cl.stats[STAT_HEALTH] >= 50)
					Q_snprintfz (macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_HEALTH]);
				else
					Q_snprintfz (macro_buf, sizeof(macro_buf), "\x10%i\x11", cl.stats[STAT_HEALTH]);
				macro_string = macro_buf;
				break;

			case 'p':
			case 'P':
				macro_string = Macro_Powerups();
				if (strcmp(macro_string, tp_name_none.string))
					Q_snprintfz (mbuf, sizeof(mbuf), "\x10%s\x11", macro_string);
				else
					mbuf[0] = 0;
				macro_string = mbuf;
				break;

			default:
				buf[i++] = *s++;
				continue;
			}
			if (i + strlen(macro_string) >= MAX_MACRO_STRING - 1)
				Sys_Error("TP_ParseMacroString: macro string length > MAX_MACRO_STRING)");
			Q_strncpyz (&buf[i], macro_string, MAX_MACRO_STRING - i);
			i += strlen(macro_string);
			s += 4;	// skip %[<char>]
			continue;
		}

		// check %a, etc
		if (*s == '%') {
			switch (s[1]) {
				case 'a':	macro_string = Macro_Armor(); break;
				case 'A':	macro_string = Macro_ArmorType(); break;
				case 'b':	macro_string = Macro_BestWeaponAndAmmo(); break;
				case 'c':	macro_string = Macro_Cells(); break;
				case 'd':	macro_string = Macro_LastDeath(); break;
				case 'h':	macro_string = Macro_Health(); break;
				case 'i':	macro_string = Macro_TookAtLoc(); break;
				case 'j':	macro_string = Macro_LastPointAtLoc(); break;
				case 'k':	macro_string = Macro_LastTookOrPointed(); break;
				case 'l':	macro_string = Macro_Location(); break;
				case 'L':	macro_string = Macro_Last_Location(); break;
				case 'm':	macro_string = Macro_LastTookOrPointed(); break;
		
				case 'o':	macro_string = Macro_CountNearbyFriendlyPlayers(); break;
				case 'e':	macro_string = Macro_CountNearbyEnemyPlayers(); break;
				case 'O':	macro_string = Macro_Count_Last_NearbyFriendlyPlayers(); break;
				case 'E':	macro_string = Macro_Count_Last_NearbyEnemyPlayers(); break;
		
				case 'P': 
				case 'p':	macro_string = Macro_Powerups(); break;
				case 'q':	macro_string = Macro_LastSeenPowerup(); break;	
				case 'r':	macro_string = Macro_LastReportedLoc(); break;
				case 's':	macro_string = Macro_EnemyStatus_LED(); break;	
				case 'S':	macro_string = Macro_TF_Skin(); break;			
				case 't':	macro_string = Macro_PointNameAtLocation(); break;
				case 'u':	macro_string = Macro_Need(); break;
				case 'w':	macro_string = Macro_WeaponAndAmmo(); break;
				case 'x':	macro_string = Macro_PointName(); break;
				case 'X':	macro_string = Macro_Took(); break;
				case 'y':	macro_string = Macro_PointLocation(); break;
				case 'Y':	macro_string = Macro_TookLoc(); break;
				default:
					buf[i++] = *s++;
					continue;
			}
			if (i + strlen(macro_string) >= MAX_MACRO_STRING - 1)
				Sys_Error("TP_ParseMacroString: macro string length > MAX_MACRO_STRING)");
			Q_strncpyz (&buf[i], macro_string, MAX_MACRO_STRING - i);
			i += strlen(macro_string);
			s += 2;	// skip % and letter
			continue;
		}
		buf[i++] = *s++;
	}
	buf[i] = 0;

	
	if (suppress) {
		qboolean quotes = false;

		TP_PrintHiddenMessage(buf);
		i = strlen(buf);
		if (i > 0 && buf[i - 1] == '\"') {
			buf[i - 1] = 0;
			quotes = true;
			i--;
		}
		buf[i++] = 0x7f;
		buf[i++] = '!';
        buf[i++]= 'A' + cl.playernum;
		if (quotes)
			buf[i++] = '\"';
		buf[i] = 0;
	}
	

	return buf;
}

//Doesn't check for overflows, so strlen(s) should be < MAX_MACRO_STRING
char *TP_ParseFunChars (char *s, qboolean chat) {
	static char	buf[MAX_MACRO_STRING];
	char *out, c;

	if (!cl_parseFunChars.value)
		return s;

	for (out = buf; *s && out - buf < MAX_MACRO_STRING; ) {
		if (*s == '$' && s[1] == 'x') {
			int i;
			// check for $x10, $x8a, etc
			c = tolower(s[2]);
			if (c >= '0' && c <= '9')
				i = (c - '0') << 4;
			else if (c >= 'a' && c <= 'f')
				i = (c - 'a' + 10) << 4;
			else goto skip;
			c = tolower(s[3]);
			if (c >= '0' && c <= '9')
				i += (c - '0');
			else if (c >= 'a' && c <= 'f')
				i += (c - 'a' + 10);
			else goto skip;
			if (!i)
				i = ' ';
			*out++ = i;
			s += 4;
			continue;
		}
		if (*s == '$' && s[1]) {
			c = 0;
			switch (s[1]) {
				case '\\': c = 0x0D; break;
				case ':': c = 0x0A; break;
				case '[': c = 0x10; break;
				case ']': c = 0x11; break;
				case 'G': c = 0x86; break;
				case 'R': c = 0x87; break;
				case 'Y': c = 0x88; break;
				case 'B': c = 0x89; break;
				case '(': c = 0x80; break;
				case '=': c = 0x81; break;
				case ')': c = 0x82; break;
				case 'a': c = 0x83; break;
				case '<': c = 0x1d; break;
				case '-': c = 0x1e; break;
				case '>': c = 0x1f; break;
				case ',': c = 0x1c; break;
				case '.': c = 0x9c; break;
				case 'b': c = 0x8b; break;
				case 'c':
				case 'd': c = 0x8d; break;
				case '$': c = '$'; break;
				case '^': c = '^'; break;
			}
			if (s[1] >= '0' && s[1] <= '9')
				c = s[1] - '0' + 0x12;
			if (c) {
				*out++ = c;
				s += 2;
				continue;
			}
		}
		if (!chat && *s == '^' && s[1] && s[1] != ' ') {
			*out++ = s[1] | 128;
			s += 2;
			continue;
		}
skip:
		*out++ = *s++;
	}
	*out = 0;

	return buf;
}

/************************* SKIN FORCING & REFRESHING *************************/



char *Skin_FindName (player_info_t *sc);

static qboolean need_skin_refresh;
void TP_UpdateSkins(void) {
	int slot;

	if (!need_skin_refresh)
		return;

	need_skin_refresh = false;

	for (slot = 0; slot < MAX_CLIENTS; slot++) {
		if (cl.players[slot].skin_refresh) {
			CL_NewTranslation(slot);
			cl.players[slot].skin_refresh = false;
		}
	}
}

qboolean TP_NeedRefreshSkins(void) {
	if (cl.teamfortress)
		return false;

	if ((cl_enemyskin.string[0] || cl_teamskin.string[0] || cl_enemypentskin.string[0] || cl_teampentskin.string[0] ||
	 cl_enemyquadskin.string[0] || cl_teamquadskin.string[0] || cl_enemybothskin.string[0] || cl_teambothskin.string[0])
	 && !(cl.fpd & FPD_NO_FORCE_SKIN))
		return true;

	if ((cl_teamtopcolor >= 0 || cl_enemytopcolor >= 0) && !(cl.fpd & FPD_NO_FORCE_COLOR))
		return true;

	return false;
}

void TP_RefreshSkin(int slot) {
	if (cls.state < ca_connected || slot < 0 || slot >= MAX_CLIENTS || !cl.players[slot].name[0] || cl.players[slot].spectator)
		return;

	
	
	cl.players[slot].skin_refresh = true;
	need_skin_refresh = true;
}

void TP_RefreshSkins(void) {
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
		TP_RefreshSkin(i);
}

qboolean OnChangeSkinForcing(cvar_t *var, char *string) {
	extern cvar_t noskins;

	if (cl.teamfortress || (cl.fpd & FPD_NO_FORCE_SKIN))
		return false;

	if (cls.state == ca_active) {
		float oldskins;

		Cvar_Set(var, string);
		oldskins = noskins.value;
		noskins.value = 2;
		con_suppress = true;
		Skin_Skins_f();		
		con_suppress = false;
		noskins.value = oldskins;
		return true;
	}
	return false;
}

int cl_teamtopcolor = -1, cl_teambottomcolor, cl_enemytopcolor = -1, cl_enemybottomcolor;

void TP_ColorForcing (int *topcolor, int *bottomcolor) {
	int	top, bottom;

	if (Cmd_Argc() == 1) {
		if (*topcolor < 0)
			Com_Printf ("\"%s\" is \"off\"\n", Cmd_Argv(0));
		else
			Com_Printf ("\"%s\" is \"%i %i\"\n", Cmd_Argv(0), *topcolor, *bottomcolor);
		return;
	}

	if (!Q_strcasecmp(Cmd_Argv(1), "off")) {
		*topcolor = -1;
		TP_RefreshSkins();
		return;
	}

	if (Cmd_Argc() == 2) {
		top = bottom = atoi(Cmd_Argv(1));
	} else {
		top = atoi(Cmd_Argv(1));
		bottom = atoi(Cmd_Argv(2));
	}

	top &= 15;
	top = min(13, top);
	bottom &= 15;
	bottom = min(13, bottom);

	*topcolor = top;
	*bottomcolor = bottom;

	TP_RefreshSkins();
}

void TP_TeamColor_f(void) {
	TP_ColorForcing(&cl_teamtopcolor, &cl_teambottomcolor);
}

void TP_EnemyColor_f(void) {
	TP_ColorForcing(&cl_enemytopcolor, &cl_enemybottomcolor);
}

/********************************* .LOC FILES *********************************/

typedef struct locdata_s {
	vec3_t coord;
	char *name;
	struct locdata_s *next;
} locdata_t;

static locdata_t	*locdata = NULL;

static void TP_ClearLocs(void) {
	locdata_t *node, *temp;

	for (node = locdata; node; node = temp) {
		free(node->name);
		temp = node->next;
		free(node);
	}

	locdata = NULL;
}

static void TP_AddLocNode(vec3_t coord, char *name) {
	locdata_t *newnode, *node;

	newnode = Q_Malloc(sizeof(locdata_t));
	newnode->name = strdup(name);
	newnode->next = NULL;
	memcpy(newnode->coord, coord, sizeof(vec3_t));

	if (!locdata) {
		locdata = newnode;
		return;
	}

	for (node = locdata; node->next; node = node->next)
		;

	node->next = newnode;
}

#define SKIPBLANKS(ptr) while (*ptr == ' ' || *ptr == '\t' || *ptr == '\r') ptr++
#define SKIPTOEOL(ptr) {while (*ptr != '\n' && *ptr != 0) ptr++; if (*ptr == '\n') ptr++;}

qboolean TP_LoadLocFile (char *path, qboolean quiet) {
	char *buf, *p, locname[MAX_OSPATH] = {0}, location[MAX_LOC_NAME];
	int i, n, sign, line, nameindex, mark, overflow, loc_numentries;
	vec3_t coord;

	if (!*path)
		return false;

	strcpy (locname, "locs/");
	if (strlen(path) + strlen(locname) + 2 + 4 > MAX_OSPATH) {
		Com_Printf ("TP_LoadLocFile: path name > MAX_OSPATH\n");
		return false;
	}
	strncat (locname, path, sizeof(locname) - strlen(locname) - 1);
	COM_DefaultExtension(locname, ".loc");

	mark = Hunk_LowMark ();
	if (!(buf = (char *) FS_LoadHunkFile (locname))) {
		if (!quiet)
			Com_Printf ("Could not load %s\n", locname);
		return false;
	}

	TP_ClearLocs();
	loc_numentries = 0;

	// Parse the whole file now
	p = buf;
	line = 1;

	while (1) {
		SKIPBLANKS(p);

		if (!*p) {
			goto _endoffile;
		} else if (*p == '\n') {
			p++;
			goto _endofline;
		} else if (*p == '/' && p[1] == '/') {
			SKIPTOEOL(p);
			goto _endofline;
		}

		// parse three ints
		for (i = 0; i < 3; i++)	{
			n = 0;
			sign = 1;
			while (1) {
				switch (*p++) {
				case ' ': case '\t':
					goto _next;
				case '-':
					if (n) {
						Com_Printf ("Locfile error (line %d): unexpected '-'\n", line);
						SKIPTOEOL(p);
						goto _endofline;
					}
					sign = -1;
					break;
				case '0': case '1': case '2': case '3': case '4':
				case '5': case '6': case '7': case '8': case '9':
					n = n * 10 + (p[-1] - '0');
					break;
				default:	// including eol or eof
					Com_Printf ("Locfile error (line %d): couldn't parse coords\n", line);
					SKIPTOEOL(p);
					goto _endofline;
				}
			}
_next:
			n *= sign;
			coord[i] = n / 8.0;

			SKIPBLANKS(p);
		}

		// parse location name
		overflow = nameindex = 0;
		while (1) {
			switch (*p)	{
			case '\r':
				p++;
				break;
			case '\n':
			case '\0':
				location[nameindex] = 0;
				TP_AddLocNode(coord, location);
				loc_numentries++;
				if (*p == '\n')
					p++;
				goto _endofline;
			default:
				if (nameindex < MAX_LOC_NAME - 1) {
					location[nameindex++] = *p;
				} else if (!overflow) {
					overflow = 1;
					Com_Printf ("Locfile warning (line %d): truncating loc name to %d chars\n", line, MAX_LOC_NAME - 1);
				}
				p++;
			}
		}
_endofline:
		line++;
	}
_endoffile:

	Hunk_FreeToLowMark (mark);

	if (loc_numentries) {
		if (!quiet)
			Com_Printf ("Loaded locfile \"%s\" (%i loc points)\n", COM_SkipPath(locname), loc_numentries);
	} else {
		TP_ClearLocs();
		if (!quiet)
			Com_Printf("Locfile \"%s\" was empty\n", COM_SkipPath(locname));
	}

	return true;
}

void TP_LoadLocFile_f (void) {
	if (Cmd_Argc() != 2) {
		Com_Printf ("loadloc <filename> : load a loc file\n");
		return;
	}
	TP_LoadLocFile (Cmd_Argv(1), false);
}

typedef struct locmacro_s {
	char *macro;
	char *val;
} locmacro_t;

static locmacro_t locmacros[] = {
	{"ssg", "ssg"},
	{"ng", "ng"},
	{"sng", "sng"},
	{"gl", "gl"},
	{"rl", "rl"},
	{"lg", "lg"},
	{"separator", "-"},
	{"ga", "ga"},
	{"ya", "ya"},
	{"ra", "ra"},
	{"quad", "quad"},
	{"pent", "pent"},
	{"ring", "ring"},
	{"suit", "suit"},
};

#define NUM_LOCMACROS	(sizeof(locmacros) / sizeof(locmacros[0]))

char *TP_LocationName (vec3_t location) {
	char *in, *out;
	int i;
	float dist, mindist;
	vec3_t vec;
	static locdata_t *node, *best;
	static qboolean recursive;
	static char	buf[1024], newbuf[MAX_LOC_NAME];

	if (!locdata || cls.state != ca_active)
		return tp_name_someplace.string;

	if (recursive)
		return "";

	best = NULL;

	for (node = locdata; node; node = node->next) {
		VectorSubtract (location, node->coord, vec);
		dist = vec[0] * vec[0] + vec[1] * vec[1] + vec[2] * vec[2];
		if (!best || dist < mindist) {
			best = node;
			mindist = dist;
		}
	}

	recursive = true;
	Cmd_ExpandString (best->name, buf);
	recursive = false; 

	
	newbuf[0] = 0;
	out = newbuf;
	in = buf;
	while (*in && out - newbuf < sizeof(newbuf) - 1) {
		if (!Q_strncasecmp(in, "$loc_name_", 10)) {
			in += 10;
			for (i = 0; i < NUM_LOCMACROS; i++) {
				if (!Q_strncasecmp(in, locmacros[i].macro, strlen(locmacros[i].macro))) {
					if (out - newbuf >= sizeof(newbuf) - strlen(locmacros[i].val) - 1)
						goto done_locmacros;
					strcpy(out, locmacros[i].val);
					out += strlen(locmacros[i].val);
					in += strlen(locmacros[i].macro);
					break;
				}
			}
			if (i == NUM_LOCMACROS) {
				if (out - newbuf >= sizeof(newbuf) - 10 - 1)
					goto done_locmacros;
				strcpy(out, "$loc_name_");
				out += 10;
			}
		} else {
			*out++ = *in++;
		}
	}
done_locmacros:
	*out = 0;

	return newbuf; 
}

/****************************** MESSAGE TRIGGERS ******************************/

typedef struct msg_trigger_s {
	char	name[32];
	char	string[64];
	int		level;
	struct msg_trigger_s *next;
} msg_trigger_t;

static msg_trigger_t *msg_triggers;

void TP_ResetAllTriggers(void) {
	msg_trigger_t *temp;

	while (msg_triggers) {
		temp = msg_triggers->next;
		Z_Free(msg_triggers);
		msg_triggers = temp;
	}
}

void TP_DumpTriggers(FILE *f) {
	msg_trigger_t *t;

	for (t = msg_triggers; t; t = t->next) {
		if (t->level == PRINT_HIGH)
			fprintf(f, "msg_trigger  %s \"%s\"\n", t->name, t->string);
		else
			fprintf(f, "msg_trigger  %s \"%s\" -l %c\n", t->name, t->string, t->level == 4 ? 't' : '0' + t->level);
	}
}

msg_trigger_t *TP_FindTrigger (char *name) {
	msg_trigger_t *t;

	for (t = msg_triggers; t; t = t->next)
		if (!strcmp(t->name, name))
			return t;

	return NULL;
}

void TP_MsgTrigger_f (void) {
	int c;
	char *name;
	msg_trigger_t *trig;

	c = Cmd_Argc();

	if (c > 5) {
		Com_Printf ("msg_trigger <trigger name> \"string\" [-l <level>]\n");
		return;
	}

	if (c == 1) {
		if (!msg_triggers)
			Com_Printf ("no triggers defined\n");
		else
		for (trig=msg_triggers; trig; trig=trig->next)
			Com_Printf ("%s : \"%s\"\n", trig->name, trig->string);
		return;
	}

	name = Cmd_Argv(1);
	if (strlen(name) > 31) {
		Com_Printf ("trigger name too long\n");
		return;
	}

	if (c == 2) {
		trig = TP_FindTrigger (name);
		if (trig)
			Com_Printf ("%s: \"%s\"\n", trig->name, trig->string);
		else
			Com_Printf ("trigger \"%s\" not found\n", name);
		return;
	}

	if (c >= 3) {
		if (strlen(Cmd_Argv(2)) > 63) {
			Com_Printf ("trigger string too long\n");
			return;
		}

		if (!(trig = TP_FindTrigger (name))) {
			// allocate new trigger
			trig = Z_Malloc (sizeof(msg_trigger_t));
			trig->next = msg_triggers;
			msg_triggers = trig;
			strcpy (trig->name, name);
			trig->level = PRINT_HIGH;
		}

		Q_strncpyz (trig->string, Cmd_Argv(2), sizeof(trig->string));
		if (c == 5 && !Q_strcasecmp (Cmd_Argv(3), "-l")) {
			if (!strcmp(Cmd_Argv(4), "t")) {
				trig->level = 4;
			} else {
				trig->level = Q_atoi (Cmd_Argv(4));
				if ((unsigned) trig->level > PRINT_CHAT)
					trig->level = PRINT_HIGH;
			}
		}
	}
}

static qboolean TP_IsFlagMessage(char *message) {
	if	(	strstr(message, " has your key!") ||
			strstr(message, " has taken your Key") ||
			strstr(message, " has your flag") ||
			strstr(message, " took your flag!") ||
			strstr(message, " ÔÏÏË ÙÏÕÒ flag!") ||
			strstr(message, " çïô òåä§ó æìáç®") || strstr(message, " çïô âìõå§ó æìáç®") ||
			strstr(message, " took the blue flag") || strstr(message, " took the red flag") ||
			strstr(message, " Has the Red Flag") || strstr(message, " Has the Blue Flag")
		)
		return true;

	return false;
}

void TP_SearchForMsgTriggers (char *s, int level) {
	msg_trigger_t	*t;
	char *string;

	if (!tp_msgtriggers.value || cls.demoplayback)
		return;

	for (t = msg_triggers; t; t = t->next) {
		if ((t->level == level || (t->level == 3 && level == 4)) && t->string[0] && strstr(s, t->string)) {
			if	(	level == PRINT_CHAT && (
					strstr (s, "fuh_version") || strstr (s, "f_version") || strstr (s, "f_system") ||
					strstr (s, "f_server") || strstr (s, "f_speed") || strstr (s, "f_modified"))
				)
				continue; 	// don't let llamas fake proxy replies

			if (cl.teamfortress && level == PRINT_HIGH && TP_IsFlagMessage(s)) 
				continue;

			if ((string = Cmd_AliasString (t->name))) {
				Q_strncpyz(vars.lasttrigger_match, s, sizeof(vars.lasttrigger_match));	
				Cbuf_AddTextEx (&cbuf_safe, string);
			} else {
				Com_Printf ("trigger \"%s\" has no matching alias\n", t->name);
			}
		}
	}
}

/************************* BASIC MATCH INFO FUNCTIONS *************************/

char *TP_PlayerName(void) {
	static char	myname[MAX_INFO_STRING];

	strcpy (myname, Info_ValueForKey(cl.players[cl.playernum].userinfo, "name"));
	return myname;
}

char *TP_PlayerTeam(void) {
	static char	myteam[MAX_INFO_STRING];

	strcpy (myteam, cl.players[cl.playernum].team);
	return myteam;
}

int	TP_CountPlayers(void) {
	int	i, count = 0;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator)
			count++;
	}
	return count;
}

char *TP_MapName(void) {
	return mapname.string;
}

char *MT_GetMapGroupName(char *mapname, qboolean *system);

char *TP_GetMapGroupName(char *mapname, qboolean *system) {
	return MT_GetMapGroupName(mapname, system);
}

/****************************** PUBLIC FUNCTIONS ******************************/

void TP_NewMap (void) {
	static char last_map[MAX_QPATH] = {0};
	char *groupname, *mapname;
	qboolean system;

	memset (&vars, 0, sizeof(vars));
	TP_FindModelNumbers ();

	mapname = TP_MapName();
	if (strcmp(mapname, last_map)) {	// map name has changed
		TP_ClearLocs();					// clear loc file
		if (tp_loadlocs.value && cl.deathmatch && !cls.demoplayback) {
			if (!TP_LoadLocFile (va("%s.loc", mapname), true)) {
				if ((groupname = TP_GetMapGroupName(mapname, &system)) && !system)
					TP_LoadLocFile (va("%s.loc", groupname), true);
			}
			Q_strncpyz (last_map, mapname, sizeof(last_map));
		} else {
			last_map[0] = 0;
		}
	}
	TP_ExecTrigger ("f_newmap");
	Ignore_ResetFloodList();
}

/*
returns a combination of these values:
0 -- unknown (probably generated by the server)
1 -- normal
2 -- team message
4 -- spectator
8 -- spec team message
Note that sometimes we can't be sure who really sent the message,  e.g. when there's a 
player "unnamed" in your team and "(unnamed)" in the enemy team. The result will be 3 (1+2)
*/
int TP_CategorizeMessage (char *s, int *offset) {
	int i, msglen, len, flags, tracknum;
	player_info_t	*player;
	char *name, *team;

	
	tracknum = -1;
	if (cl.spectator && (tracknum = Cam_TrackNum()) != -1)
		team = cl.players[tracknum].team;
	else if (!cl.spectator)
		team = cl.players[cl.playernum].team;

	flags = 0;
	*offset = 0;
	if (!(msglen = strlen(s)))
		return 0;

	for (i = 0, player = cl.players; i < MAX_CLIENTS; i++, player++)	{
		if (!player->name[0])
			continue;
		name = Info_ValueForKey (player->userinfo, "name");
		len = bound(0, strlen(name), 31);	
		// check messagemode1
		if (len + 2 <= msglen && s[len] == ':' && s[len + 1] == ' ' && !strncmp(name, s, len))	{
			if (player->spectator)
				flags |= 4;
			else
				flags |= 1;
			*offset = len + 2;
		}
		// check messagemode2
		else if (s[0] == '(' && len + 4 <= msglen && !strncmp(s + len + 1, "): ", 3) && !strncmp(name, s + 1, len)
		 
		 && (!cl.spectator || tracknum != -1)
		) {
			// no team messages in teamplay 0, except for our own
			if (i == cl.playernum || ( cl.teamplay && !strcmp(team, player->team)) )
				flags |= 2;
			*offset = len + 4;
		}
		//check spec mm2
		else if (cl.spectator && !strncmp(s, "[SPEC] ", 7) && player->spectator && 
		 len + 9 <= msglen && s[len + 7] == ':' && s[len + 8] == ' ' && !strncmp(name, s + 7, len)) {
			flags |= 8;
			*offset = len + 9;
		}
	}
	return flags;
}

/****************************** POINTING & TOOK ******************************/

// symbolic names used in tp_took, tp_pickup, tp_point commands
char *pknames[] = {"quad", "pent", "ring", "suit", "ra", "ya",	"ga",
"mh", "health", "lg", "rl", "gl", "sng", "ng", "ssg", "pack",
"cells", "rockets", "nails", "shells", "flag", 
"teammate", "enemy", "eyes", "sentry", "disp", "runes"};

#define it_quad		(1 << 0)
#define it_pent		(1 << 1)
#define it_ring		(1 << 2)
#define it_suit		(1 << 3)
#define it_ra		(1 << 4)
#define it_ya		(1 << 5)
#define it_ga		(1 << 6)
#define it_mh		(1 << 7)
#define it_health	(1 << 8)
#define it_lg		(1 << 9)
#define it_rl		(1 << 10)
#define it_gl		(1 << 11)
#define it_sng		(1 << 12)
#define it_ng		(1 << 13)
#define it_ssg		(1 << 14)
#define it_pack		(1 << 15)
#define it_cells	(1 << 16)
#define it_rockets	(1 << 17)
#define it_nails	(1 << 18)
#define it_shells	(1 << 19)
#define it_flag		(1 << 20)
#define it_teammate	(1 << 21)
#define it_enemy	(1 << 22)
#define it_eyes		(1 << 23)
#define it_sentry   (1 << 24)
#define it_disp		(1 << 25)
#define it_runes	(1 << 26)
#define NUM_ITEMFLAGS 27

#define it_powerups	(it_quad|it_pent|it_ring)
#define it_weapons	(it_lg|it_rl|it_gl|it_sng|it_ng|it_ssg)
#define it_armor	(it_ra|it_ya|it_ga)
#define it_ammo		(it_cells|it_rockets|it_nails|it_shells)
#define it_players	(it_teammate|it_enemy|it_eyes)

#define default_pkflags (it_powerups|it_suit|it_armor|it_weapons|it_mh| \
				it_rockets|it_pack|it_flag)

#define default_tookflags (it_powerups|it_ra|it_ya|it_lg|it_rl|it_mh|it_flag)

#define default_pointflags (it_powerups|it_suit|it_armor|it_mh| \
				it_lg|it_rl|it_gl|it_sng|it_rockets|it_pack|it_flag|it_players)

int pkflags = default_pkflags;
int tookflags = default_tookflags;
int pointflags = default_pointflags;

static void DumpFlagCommand(FILE *f, char *name, int flags, int default_flags) {
	int i, all_flags = (1 << NUM_ITEMFLAGS) - 1;

	fprintf(f, "%s ", name);

	if (flags == 0) {
		fprintf(f, "none\n");
		return;
	}
	if (flags == all_flags) {
		fprintf(f, "all\n");
		return;
	} else if (flags == default_flags) {
		fprintf(f, "default\n");
		return;
	}

	if ((flags & it_powerups) == it_powerups) {
		fprintf(f, "powerups ");
		flags &= ~it_powerups;
	}
	if ((flags & it_weapons) == it_weapons) {
		fprintf(f, "weapons ");
		flags &= ~it_weapons;
	}
	if ((flags & it_armor) == it_armor) {
		fprintf(f, "armor ");
		flags &= ~it_armor;
	}
	if ((flags & it_ammo) == it_ammo) {
		fprintf(f, "ammo ");
		flags &= ~it_ammo;
	}
	if ((flags & it_players) == it_players) {
		fprintf(f, "players ");
		flags &= ~it_players;
	}
	for (i = 0; i < NUM_ITEMFLAGS; i++) {
		if (flags & (1 << i))
			fprintf (f, "%s ", pknames[i]);
	}
	fprintf(f, "\n");
}

void DumpFlagCommands(FILE *f) {
	DumpFlagCommand(f, "tp_pickup   ", pkflags, default_pkflags);
	DumpFlagCommand(f, "tp_took     ", tookflags, default_tookflags);
	DumpFlagCommand(f, "tp_point    ", pointflags, default_pointflags);
}

static void FlagCommand (int *flags, int defaultflags) {
	int i, j, c, flag;
	char *p, str[255] = {0};
	qboolean removeflag = false;

	c = Cmd_Argc ();
	if (c == 1)	{
		if (!*flags)
			strcpy (str, "none");
		for (i = 0 ; i < NUM_ITEMFLAGS ; i++)
			if (*flags & (1 << i)) {
				if (*str)
					strncat (str, " ", sizeof(str) - strlen(str) - 1);
				strncat (str, pknames[i], sizeof(str) - strlen(str) - 1);
			}
		Com_Printf ("%s\n", str);
		return;
	}

	if (c == 2 && !Q_strcasecmp(Cmd_Argv(1), "none")) {
		*flags = 0;
		return;
	}

	if (*Cmd_Argv(1) != '+' && *Cmd_Argv(1) != '-')
		*flags = 0;

	for (i = 1; i < c; i++) {
		p = Cmd_Argv (i);
		if (*p == '+') {
			removeflag = false;
			p++;
		} else if (*p == '-') {
			removeflag = true;
			p++;
		}

		flag = 0;
		for (j=0 ; j<NUM_ITEMFLAGS ; j++) {
			if (!Q_strncasecmp (p, pknames[j], 3)) {
				flag = 1<<j;
				break;
			}
		}

		if (!flag) {
			if (!Q_strcasecmp (p, "armor"))
				flag = it_armor;
			else if (!Q_strcasecmp (p, "weapons"))
				flag = it_weapons;
			else if (!Q_strcasecmp (p, "powerups"))
				flag = it_powerups;
			else if (!Q_strcasecmp (p, "ammo"))
				flag = it_ammo;
			else if (!Q_strcasecmp (p, "players"))
				flag = it_players;
			else if (!Q_strcasecmp (p, "default"))
				flag = defaultflags;
			else if (!Q_strcasecmp (p, "all"))
				flag = (1<<NUM_ITEMFLAGS)-1;
		}

		
		if (flags != &pointflags)
			flag &= ~(it_sentry|it_disp|it_players);

		if (removeflag)
			*flags &= ~flag;
		else
			*flags |= flag;
	}
}

void TP_Took_f (void) {
	FlagCommand (&tookflags, default_tookflags);
}

void TP_Pickup_f (void) {
	FlagCommand (&pkflags, default_pkflags);
}

void TP_Point_f (void) {
	FlagCommand (&pointflags, default_pointflags);
}

typedef struct {
	int		itemflag;
	cvar_t	*cvar;
	char	*modelname;
	vec3_t	offset;		// offset of model graphics center
	float	radius;		// model graphics radius
	int		flags;		// TODO: "NOPICKUP" (disp), "TEAMENEMY" (flag, disp)
} item_t;

item_t	tp_items[] = {
	{	it_quad,	&tp_name_quad,	"progs/quaddama.mdl",
		{0, 0, 24},	25,
	},
	{	it_pent,	&tp_name_pent,	"progs/invulner.mdl",
		{0, 0, 22},	25,
	},
	{	it_ring,	&tp_name_ring,	"progs/invisibl.mdl",
		{0, 0, 16},	12,
	},
	{	it_suit,	&tp_name_suit,	"progs/suit.mdl",
		{0, 0, 24}, 20,
	},
	{	it_lg,		&tp_name_lg,	"progs/g_light.mdl",
		{0, 0, 30},	20,
	},
	{	it_rl,		&tp_name_rl,	"progs/g_rock2.mdl",
		{0, 0, 30},	20,
	},
	{	it_gl,		&tp_name_gl,	"progs/g_rock.mdl",
		{0, 0, 30},	20,
	},
	{	it_sng,		&tp_name_sng,	"progs/g_nail2.mdl",
		{0, 0, 30},	20,
	},
	{	it_ng,		&tp_name_ng,	"progs/g_nail.mdl",
		{0, 0, 30},	20,
	},
	{	it_ssg,		&tp_name_ssg,	"progs/g_shot.mdl",
		{0, 0, 30},	20,
	},
	{	it_cells,	&tp_name_cells,	"maps/b_batt0.bsp",
		{16, 16, 24},	18,
	},
	{	it_cells,	&tp_name_cells,	"maps/b_batt1.bsp",
		{16, 16, 24},	18,
	},
	{	it_rockets,	&tp_name_rockets,"maps/b_rock0.bsp",
		{8, 8, 20},	18,
	},
	{	it_rockets,	&tp_name_rockets,"maps/b_rock1.bsp",
		{16, 8, 20},	18,
	},
	{	it_nails,	&tp_name_nails,	"maps/b_nail0.bsp",
		{16, 16, 10},	18,
	},
	{	it_nails,	&tp_name_nails,	"maps/b_nail1.bsp",
		{16, 16, 10},	18,
	},
	{	it_shells,	&tp_name_shells,"maps/b_shell0.bsp",
		{16, 16, 10},	18,
	},
	{	it_shells,	&tp_name_shells,"maps/b_shell1.bsp",
		{16, 16, 10},	18,
	},
	{	it_health,	&tp_name_health,"maps/b_bh10.bsp",
		{16, 16, 8},	18,
	},
	{	it_health,	&tp_name_health,"maps/b_bh25.bsp",
		{16, 16, 8},	18,
	},
	{	it_mh,		&tp_name_mh,	"maps/b_bh100.bsp",
		{16, 16, 14},	20,
	},
	{	it_pack,	&tp_name_backpack, "progs/backpack.mdl",
		{0, 0, 18},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/tf_flag.mdl",
		{0, 0, 14},	25,
	},
	{	it_flag,	&tp_name_flag,	"progs/tf_stan.mdl",
		{0, 0, 45},	40,
	},
	{	it_flag,	&tp_name_flag,	"progs/w_g_key.mdl",
		{0, 0, 20},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/w_s_key.mdl",
		{0, 0, 20},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/m_g_key.mdl",
		{0, 0, 20},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/m_s_key.mdl",
		{0, 0, 20},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/b_s_key.mdl",
		{0, 0, 20},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/b_g_key.mdl",
		{0, 0, 20},	18,
	},
	{	it_flag,	&tp_name_flag,	"progs/flag.mdl",
		{0, 0, 14},	25,
	},
	{	it_runes,	&tp_name_rune_1,	"progs/end1.mdl",
		{0, 0, 20},	18,
	},
	{	it_runes,	&tp_name_rune_2,	"progs/end2.mdl",
		{0, 0, 20},	18,
	},
	{	it_runes,	&tp_name_rune_3,	"progs/end3.mdl",
		{0, 0, 20},	18,
	},
	{	it_runes,	&tp_name_rune_4,	"progs/end4.mdl",
		{0, 0, 20},	18,
	},
	{	it_ra|it_ya|it_ga, NULL,	"progs/armor.mdl",
		{0, 0, 24},	22,
	},
	{	it_sentry, &tp_name_sentry, "progs/turrgun.mdl",
		{0, 0, 23},	25,
	},
	{	it_disp, &tp_name_disp,	"progs/disp.mdl",
		{0, 0, 24},	25,
	}
};

#define NUMITEMS (sizeof(tp_items) / sizeof(tp_items[0]))

item_t *model2item[MAX_MODELS];

void TP_FindModelNumbers (void) {
	int i, j;
	char *s;
	item_t *item;

	for (i = 0 ; i < MAX_MODELS ; i++) {
		model2item[i] = NULL;
		s = cl.model_name[i];
		if (!s)
			continue;
		for (j = 0, item = tp_items ; j < NUMITEMS ; j++, item++)
			if (!strcmp(s, item->modelname))
				model2item[i] = item;
	}
}

// on success, result is non-zero
// on failure, result is zero
// for armors, returns skinnum+1 on success
static int FindNearestItem (int flags, item_t **pitem) {
	frame_t *frame;
	packet_entities_t *pak;
	entity_state_t *ent, *bestent = NULL;
	int	i;
	float bestdist, dist;
	vec3_t org, v;
	item_t *item;

	VectorCopy (cl.frames[cl.validsequence & UPDATE_MASK].playerstate[cl.playernum].origin, org);

	// look in previous frame
	frame = &cl.frames[cl.oldvalidsequence&UPDATE_MASK];
	pak = &frame->packet_entities;
	bestdist = 100 * 100;
	*pitem = NULL;
	for (i = 0, ent = pak->entities; i < pak->num_entities; i++, ent++) {
		item = model2item[ent->modelindex];
		if (!item || !(item->itemflag & flags))
			continue;

		VectorSubtract (ent->origin, org, v);
		VectorAdd (v, item->offset, v);
		dist = DotProduct (v, v);

		if (dist <= bestdist) {
			bestdist = dist;
			bestent = ent;
			*pitem = item;
		}
	}

	if (bestent)
		Q_strncpyz(vars.nearestitemloc, TP_LocationName(bestent->origin), sizeof(vars.nearestitemloc));
	else
		vars.nearestitemloc[0] = 0;

	if (bestent && (*pitem)->itemflag == it_armor)
		return bestent->skinnum + 1;	// 1 = green, 2 = yellow, 3 = red

	return bestent ? bestent->modelindex : 0;
}

char *Macro_LastTookOrPointed (void) {
	if (vars.tooktime && vars.tooktime > vars.pointtime && cls.realtime - vars.tooktime < 5)
		return Macro_TookAtLoc();
	else if (vars.pointtime && vars.tooktime <= vars.pointtime && cls.realtime - vars.pointtime < 5)
		return Macro_LastPointAtLoc();

	Q_snprintfz(macro_buf, sizeof(macro_buf), "%s %s %s", tp_name_nothing.string, tp_name_at.string, tp_name_someplace.string);
    return macro_buf;
}

static qboolean CheckTrigger (void) {
	int	i, count;
	player_info_t *player;
	char *myteam;

	if (cl.spectator || !Rulesets_AllowTriggers())
		return false;

	if (tp_forceTriggers.value)
		return true;

	if (!cl.teamplay)
		return false;

	count = 0;
	myteam = cl.players[cl.playernum].team;
	for (i = 0, player= cl.players; i < MAX_CLIENTS; i++, player++) {
		if (player->name[0] && !player->spectator && i != cl.playernum && !strcmp(player->team, myteam))
			count++;
	}

	return count;
}

static void ExecTookTrigger (char *s, int flag, vec3_t org) {
	int pkflags_dmm, tookflags_dmm;

	pkflags_dmm = pkflags;
	tookflags_dmm = tookflags;
	
	if (!cl.teamfortress && cl.deathmatch >= 1 && cl.deathmatch <= 4) {
		if (cl.deathmatch == 4) {
			pkflags_dmm &= ~(it_ammo|it_weapons);
			tookflags_dmm &= ~(it_ammo|it_weapons);
		}
	}
	if (!((pkflags_dmm|tookflags_dmm) & flag))
		return;

	vars.tooktime = cls.realtime;
	strncpy (vars.tookname, s, sizeof(vars.tookname)-1);
	strncpy (vars.tookloc, TP_LocationName (org), sizeof(vars.tookloc)-1);

	if ((tookflags_dmm & flag) && CheckTrigger())
		TP_ExecTrigger ("f_took");
}

void TP_ParsePlayerInfo(player_state_t *oldstate, player_state_t *state, player_info_t *info) {
	if (TP_NeedRefreshSkins()) {
		if ((state->effects & (EF_BLUE|EF_RED) ) != (oldstate->effects & (EF_BLUE|EF_RED)))
			TP_RefreshSkin(info - cl.players);
	}

	if (!cl.spectator && cl.teamplay && strcmp(info->team, TP_PlayerTeam())) {
		qboolean eyes;

		eyes = state->modelindex && cl.model_precache[state->modelindex] && cl.model_precache[state->modelindex]->modhint == MOD_EYES;

        if (state->effects & (EF_BLUE | EF_RED) || eyes) {
            vars.enemy_powerups = 0;
            vars.enemy_powerups_time = cls.realtime;

            if (state->effects & EF_BLUE)
                vars.enemy_powerups |= TP_QUAD;
            if (state->effects & EF_RED)
                vars.enemy_powerups |= TP_PENT;
            if (eyes)
                vars.enemy_powerups |= TP_RING;
        }
    }
	if (!cl.spectator && !cl.teamfortress && info - cl.players == cl.playernum) {
		if ((state->effects & (EF_FLAG1|EF_FLAG2)) && !(oldstate->effects & (EF_FLAG1|EF_FLAG2))) {
			ExecTookTrigger (tp_name_flag.string, it_flag, cl.frames[cl.validsequence & UPDATE_MASK].playerstate[cl.playernum].origin);
		} else if (!(state->effects & (EF_FLAG1|EF_FLAG2)) && (oldstate->effects & (EF_FLAG1|EF_FLAG2))) {
			vars.lastdrop_time = cls.realtime;
			strcpy (vars.lastdroploc, Macro_Location());
		}
	}
}

void TP_CheckPickupSound (char *s, vec3_t org) {
	item_t *item;

	if (cl.spectator)
		return;

	if (!strcmp(s, "items/damage.wav"))
		ExecTookTrigger (tp_name_quad.string, it_quad, org);
	else if (!strcmp(s, "items/protect.wav"))
		ExecTookTrigger (tp_name_pent.string, it_pent, org);
	else if (!strcmp(s, "items/inv1.wav"))
		ExecTookTrigger (tp_name_ring.string, it_ring, org);
	else if (!strcmp(s, "items/suit.wav"))
		ExecTookTrigger (tp_name_suit.string, it_suit, org);
	else if (!strcmp(s, "items/health1.wav") || !strcmp(s, "items/r_item1.wav"))
		ExecTookTrigger (tp_name_health.string, it_health, org);
	else if (!strcmp(s, "items/r_item2.wav"))
		ExecTookTrigger (tp_name_mh.string, it_mh, org);
	else
		goto more;
	return;

more:
	if (!cl.validsequence || !cl.oldvalidsequence)
		return;

	// weapons
	if (!strcmp(s, "weapons/pkup.wav"))	{
		if (FindNearestItem (it_weapons, &item)) {
			ExecTookTrigger (item->cvar->string, item->itemflag, org);
		} else if (vars.stat_framecounts[STAT_ITEMS] == cls.framecount) {
			if (vars.items & ~vars.olditems & IT_LIGHTNING) 
				ExecTookTrigger (tp_name_lg.string, it_lg, cl.simorg);
			else if (vars.items & ~vars.olditems & IT_ROCKET_LAUNCHER) 
				ExecTookTrigger (tp_name_rl.string, it_rl, cl.simorg);
			else if (vars.items & ~vars.olditems & IT_GRENADE_LAUNCHER) 
				ExecTookTrigger (tp_name_gl.string, it_gl, cl.simorg);
			else if (vars.items & ~vars.olditems & IT_SUPER_NAILGUN) 
				ExecTookTrigger (tp_name_sng.string, it_sng, cl.simorg);
			else if (vars.items & ~vars.olditems & IT_NAILGUN) 
				ExecTookTrigger (tp_name_ng.string, it_ng, cl.simorg);
			else if (vars.items & ~vars.olditems & IT_SUPER_SHOTGUN)
				ExecTookTrigger (tp_name_ssg.string, it_ssg, cl.simorg);
		}
		return;
	}

	// armor
	if (!strcmp(s, "items/armor1.wav"))	{
		qboolean armor_updated;
		int armortype;

		armor_updated = (vars.stat_framecounts[STAT_ARMOR] == cls.framecount);
		armortype = FindNearestItem (it_armor, &item);
		if (armortype == 1 || (!armortype && armor_updated && cl.stats[STAT_ARMOR] == 100))
			ExecTookTrigger (tp_name_ga.string, it_ga, org);
		else if (armortype == 2 || (!armortype && armor_updated && cl.stats[STAT_ARMOR] == 150))
			ExecTookTrigger (tp_name_ya.string, it_ya, org);
		else if (armortype == 3 || (!armortype && armor_updated && cl.stats[STAT_ARMOR] == 200))
			ExecTookTrigger (tp_name_ra.string, it_ra, org);
		return;
	}

	// backpack, ammo or runes
	if (!strcmp (s, "weapons/lock4.wav")) {
		if (!FindNearestItem (it_ammo|it_pack|it_runes, &item))
			return;
		ExecTookTrigger (item->cvar->string, item->itemflag, org);
	}
}

qboolean TP_IsItemVisible(item_vis_t *visitem) {
	vec3_t end, v;
	pmtrace_t trace;

	if (visitem->dist <= visitem->radius)
		return true;

	VectorNegate (visitem->dir, v);
	VectorNormalizeFast (v);
	VectorMA (visitem->entorg, visitem->radius, v, end);
	trace = PM_TraceLine (visitem->vieworg, end);
	if (trace.fraction == 1)
		return true;

	VectorMA (visitem->entorg, visitem->radius, visitem->right, end);
	VectorSubtract (visitem->vieworg, end, v);
	VectorNormalizeFast (v);
	VectorMA (end, visitem->radius, v, end);
	trace = PM_TraceLine (visitem->vieworg, end);
	if (trace.fraction == 1)
		return true;

	VectorMA(visitem->entorg, -visitem->radius, visitem->right, end);
	VectorSubtract(visitem->vieworg, end, v);
	VectorNormalizeFast(v);
	VectorMA(end, visitem->radius, v, end);
	trace = PM_TraceLine(visitem->vieworg, end);
	if (trace.fraction == 1)
		return true;

	VectorMA(visitem->entorg, visitem->radius, visitem->up, end);
	VectorSubtract(visitem->vieworg, end, v);
	VectorNormalizeFast(v);
	VectorMA (end, visitem->radius, v, end);
	trace = PM_TraceLine(visitem->vieworg, end);
	if (trace.fraction == 1)
		return true;

	// use half the radius, otherwise it's possible to see through floor in some places
	VectorMA(visitem->entorg, -visitem->radius / 2, visitem->up, end);
	VectorSubtract(visitem->vieworg, end, v);
	VectorNormalizeFast(v);
	VectorMA(end, visitem->radius, v, end);
	trace = PM_TraceLine(visitem->vieworg, end);
	if (trace.fraction == 1)
		return true;

	return false;
}

static float TP_RankPoint(item_vis_t *visitem) {
	vec3_t v2, v3;
	float miss;

	if (visitem->dist < 10)
		return -1;

	VectorScale (visitem->forward, visitem->dist, v2);
	VectorSubtract (v2, visitem->dir, v3);
	miss = VectorLength (v3);
	if (miss > 300)
		return -1;
	if (miss > visitem->dist * 1.7)
		return -1;		// over 60 degrees off

	return (visitem->dist < 3000.0 / 8.0) ? miss * (visitem->dist * 8.0 * 0.0002f + 0.3f) : miss;
}

void TP_FindPoint (void) {
	packet_entities_t *pak;
	entity_state_t *ent;
	int	i, j, pointflags_dmm;
	float best = -1, rank;
	entity_state_t *bestent;
	vec3_t ang;
	item_t *item, *bestitem = NULL;
	player_state_t *state, *beststate = NULL;
	player_info_t *info, *bestinfo = NULL;
	item_vis_t visitem;
	extern cvar_t v_viewheight;

	if (vars.pointtime == cls.realtime)
		return;

	if (!cl.validsequence)
		goto nothing;

	ang[0] = cl.viewangles[0]; ang[1] = cl.viewangles[1]; ang[2] = 0;
	AngleVectors (ang, visitem.forward, visitem.right, visitem.up);
	VectorCopy (cl.simorg, visitem.vieworg);
	visitem.vieworg[2] += 22 + (v_viewheight.value ? bound (-7, v_viewheight.value, 4) : 0);

	pointflags_dmm = pointflags;
	if (!cl.teamfortress && cl.deathmatch >= 1 && cl.deathmatch <= 4) {
		if (cl.deathmatch == 4)
			pointflags_dmm &= ~it_ammo;
		if (cl.deathmatch != 1)
			pointflags_dmm &= ~it_weapons;
	}

	pak = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;
	for (i = 0,ent = pak->entities; i < pak->num_entities; i++, ent++) {
		item = model2item[ent->modelindex];
		if (!item || !(item->itemflag & pointflags_dmm))
			continue;
		// special check for armors
		if (item->itemflag == (it_ra|it_ya|it_ga)) {
			switch (ent->skinnum) {
				case 0: if (!(pointflags_dmm & it_ga)) continue;
				case 1: if (!(pointflags_dmm & it_ya)) continue;
				default: if (!(pointflags_dmm & it_ra)) continue;
			}
		}

		VectorAdd (ent->origin, item->offset, visitem.entorg);
		VectorSubtract (visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct (visitem.dir, visitem.forward);
		visitem.radius = ent->effects & (EF_BLUE|EF_RED|EF_DIMLIGHT|EF_BRIGHTLIGHT) ? 200 : item->radius;

		if ((rank = TP_RankPoint(&visitem)) < 0)
			continue;

		// check if we can actually see the object
		if ((rank < best || best < 0) && TP_IsItemVisible(&visitem)) {
			best = rank;
			bestent = ent;
			bestitem = item;
		}
	}

	state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate;
	info = cl.players;
	for (j = 0; j < MAX_CLIENTS; j++, info++, state++) {			
		if (state->messagenum != cl.parsecount || j == cl.playernum || info->spectator)
			continue;

		if (
			state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame) ||
			state->modelindex == cl_modelindices[mi_h_player]
		)
			continue;

		VectorCopy (state->origin, visitem.entorg);
		visitem.entorg[2] += 30;
		VectorSubtract (visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct (visitem.dir, visitem.forward);
		visitem.radius = (state->effects & (EF_BLUE|EF_RED|EF_DIMLIGHT|EF_BRIGHTLIGHT) ) ? 200 : 27;

		if ((rank = TP_RankPoint(&visitem)) < 0)
			continue;

		// check if we can actually see the object
		if ((rank < best || best < 0) && TP_IsItemVisible(&visitem)) {
			qboolean teammate, eyes = false;

			eyes = state->modelindex && cl.model_precache[state->modelindex] && cl.model_precache[state->modelindex]->modhint == MOD_EYES;
			teammate = !!(cl.teamplay && !strcmp(info->team, TP_PlayerTeam()));

			if (eyes && !(pointflags_dmm & it_eyes))
				continue;
			else if (teammate && !(pointflags_dmm & it_teammate))
				continue;
			else if (!(pointflags_dmm & it_enemy))
				continue;

			best = rank;
			bestinfo = info;
			beststate = state;
		}
	}

	if (best >= 0 && bestinfo) {
		qboolean teammate, eyes;
		char *name, buf[256] = {0};

		eyes = beststate->modelindex && cl.model_precache[beststate->modelindex] && cl.model_precache[beststate->modelindex]->modhint == MOD_EYES;
		if (cl.teamfortress) {
			teammate = !strcmp(Utils_TF_ColorToTeam(bestinfo->real_bottomcolor), TP_PlayerTeam());

			if (eyes)
				name = tp_name_eyes.string;		//duck on 2night2
			else if (cl.spectator)
				name = bestinfo->name;
			else if (teammate)
				name = tp_name_teammate.string[0] ? tp_name_teammate.string : "teammate";
			else
				name = tp_name_enemy.string;

			if (!eyes)
				name = va("%s%s%s", name, name[0] ? " " : "", Skin_To_TFSkin(Info_ValueForKey(bestinfo->userinfo, "skin")));
		} else {
			teammate = !!(cl.teamplay && !strcmp(bestinfo->team, TP_PlayerTeam()));

			if (eyes)
				name = tp_name_eyes.string;
			else if (cl.spectator || (teammate && !tp_name_teammate.string[0]))
				name = bestinfo->name;
			else
				name = teammate ? tp_name_teammate.string : tp_name_enemy.string;
		}
		if (beststate->effects & EF_BLUE)
			Q_strncatz(buf, tp_name_quaded.string);
		if (beststate->effects & EF_RED)
			Q_strncatz(buf, va("%s%s", buf[0] ? " " : "", tp_name_pented.string));
		Q_strncatz(buf, va("%s%s", buf[0] ? " " : "", name));
		Q_strncpyz (vars.pointname, buf, sizeof(vars.pointname));
		Q_strncpyz (vars.pointloc, TP_LocationName (beststate->origin), sizeof(vars.pointloc));

		vars.pointtype = (teammate && !eyes) ? POINT_TYPE_TEAMMATE : POINT_TYPE_ENEMY;
	} else if (best >= 0) {
		char *p;

		if (!bestitem->cvar) {
			// armors are special
			switch (bestent->skinnum) {
				case 0: p = tp_name_ga.string; break;
				case 1: p = tp_name_ya.string; break;
				default: p = tp_name_ra.string;
			}
		} else {
			p = bestitem->cvar->string;
		}

		vars.pointtype = (bestitem->itemflag & (it_powerups|it_flag)) ? POINT_TYPE_POWERUP : POINT_TYPE_ITEM;
		Q_strncpyz (vars.pointname, p, sizeof(vars.pointname));
		Q_strncpyz (vars.pointloc, TP_LocationName (bestent->origin), sizeof(vars.pointloc));
	}
	else {
nothing:
		Q_strncpyz (vars.pointname, tp_name_nothing.string, sizeof(vars.pointname));
		vars.pointloc[0] = 0;
		vars.pointtype = POINT_TYPE_ITEM;
	}
	vars.pointtime = cls.realtime;
}

void TP_ParseWeaponModel(model_t *model) {
	static model_t *last_model = NULL;

	if (cl.teamfortress && (!cl.spectator || Cam_TrackNum() != -1)) {
		if (model && !last_model)
			TP_ExecTrigger ("f_reloadend");
		else if (!model && last_model)
			TP_ExecTrigger ("f_reloadstart");
	}
	last_model = model;
}

void TP_StatChanged (int stat, int value) {
	int effects;

	effects = cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum].effects;

	switch (stat) {
	case STAT_HEALTH:
		if (value > 0) {
			if (vars.health <= 0) {
				extern cshift_t	cshift_empty;
				if (cl.teamfortress)
					memset (&cshift_empty, 0, sizeof(cshift_empty));
				if (CheckTrigger())
					TP_ExecTrigger ("f_respawn");
			}
			vars.health = value;
			return;
		}
		if (vars.health > 0) {		// We have just died
			vars.deathtrigger_time = cls.realtime;
			strcpy (vars.lastdeathloc, Macro_Location());
		
			CountNearbyPlayers(true);
			vars.last_numenemies = vars.numenemies;
			vars.last_numfriendlies = vars.numfriendlies;
		
			if (CheckTrigger()) {
				if (cl.teamfortress && (cl.stats[STAT_ITEMS] & (IT_KEY1|IT_KEY2)))
					TP_ExecTrigger ("f_flagdeath");
				else if (effects & (EF_FLAG1|EF_FLAG2))
					TP_ExecTrigger ("f_flagdeath");
				else
					TP_ExecTrigger ("f_death");
			}
		}
		vars.health = value;
		break;
	case STAT_ITEMS:
		if (value & ~vars.items & (IT_KEY1|IT_KEY2)) {
			if (cl.teamfortress && !cl.spectator)
				ExecTookTrigger (tp_name_flag.string, it_flag, cl.frames[cl.validsequence & UPDATE_MASK].playerstate[cl.playernum].origin);
		}
		if (!cl.spectator && cl.teamfortress && ~value & vars.items & (IT_KEY1|IT_KEY2)) {
			vars.lastdrop_time = cls.realtime;
			strcpy (vars.lastdroploc, Macro_Location());
		}
		vars.olditems = vars.items;
		vars.items = value;
		break;
	case STAT_ACTIVEWEAPON:
		if (cl.stats[STAT_ACTIVEWEAPON] != vars.activeweapon) {
			TP_ExecTrigger ("f_weaponchange");
			vars.activeweapon = cl.stats[STAT_ACTIVEWEAPON];
		}
		break;
	}
	vars.stat_framecounts[stat] = cls.framecount;
}

/******************************* SOUND TRIGGERS *******************************/

//Find and execute sound triggers. A sound trigger must be terminated by either a CR or LF.
//Returns true if a sound was found and played
qboolean TP_CheckSoundTrigger (char *str) {
	int i, j, start, length;
	char soundname[MAX_OSPATH];
	FILE *f;

	if (!tp_soundtrigger.string[0])
		return false;

	for (i = strlen(str) - 1; i; i--)	{
		if (str[i] != 0x0A && str[i] != 0x0D)
			continue;

		for (j = i - 1; j >= 0; j--) {
			// quick check for chars that cannot be used
			// as sound triggers but might be part of a file name
			if (isalnum(str[j]))
				continue;	// file name or chat

			if (strchr(tp_soundtrigger.string, str[j]))	{
				// this might be a sound trigger

				start = j + 1;
				length = i - start;

				if (!length)
					break;
				if (length >= MAX_QPATH)
					break;

				Q_strncpyz (soundname, str + start, length + 1);
				if (strstr(soundname, ".."))
					break;	// no thank you

				// clean up the message
				strcpy (str + j, str + i);

				if (!snd_initialized)
					return false;

				COM_DefaultExtension (soundname, ".wav");

				// make sure we have it on disk (FIXME)
				FS_FOpenFile (va("sound/%s", soundname), &f);
				if (!f)
					return false;
				fclose (f);

				// now play the sound
				S_LocalSound (soundname);
				return true;
			}
			if (str[j] == '\\')
				str[j] = '/';
			if (str[j] <= ' ' || strchr("\"&'*,:;<>?\\|\x7f", str[j]))
				break;	// we don't allow these in a file name
		}
	}

	return false;
}

/****************************** MESSAGE FILTERS ******************************/

#define MAX_FILTER_LENGTH 4
char filter_strings[8][MAX_FILTER_LENGTH + 1];
int	num_filters = 0;

//returns false if the message shouldn't be printed.
//Matching filters are stripped from the message
qboolean TP_FilterMessage (char *s) {
	int i, j, len, maxlen;

	if (!num_filters)
		return true;

	len = strlen (s);
	if (len < 2 || s[len - 1] != '\n' || s[len - 2] == '#')
		return true;

	maxlen = MAX_FILTER_LENGTH + 1;
	for (i = len - 2 ; i >= 0 && maxlen > 0 ; i--, maxlen--) {
		if (s[i] == ' ')
			return true;
		if (s[i] == '#')
			break;
	}
	if (i < 0 || !maxlen)
		return true;	// no filter at all

	s[len - 1] = 0;	// so that strcmp works properly

	for (j = 0; j < num_filters; j++)
		if (!Q_strcasecmp(s + i + 1, filter_strings[j])) {
			// strip the filter from message
			if (i && s[i - 1] == ' ')	{	
				// there's a space just before the filter, remove it
				// so that soundtriggers like ^blah #att work
				s[i - 1] = '\n';
				s[i] = 0;
			} else {
				s[i] = '\n';
				s[i + 1] = 0;
			}
			return true;
		}

	s[len - 1] = '\n';
	return false;	// this message is not for us, don't print it
}

void TP_MsgFilter_f (void) {
	int c, i;
	char *s;

	if ((c = Cmd_Argc()) == 1) {
		if (!num_filters) {
			Com_Printf ("No filters defined\n");
			return;
		}
		for (i = 0; i < num_filters; i++)
			Com_Printf ("%s#%s", i ? " " : "", filter_strings[i]);
		Com_Printf ("\n");
		return;
	}

	if (c == 2 && (Cmd_Argv(1)[0] == 0 || !Q_strcasecmp(Cmd_Argv(1), "clear"))) {
		num_filters = 0;
		return;
	}

	num_filters = 0;
	for (i = 1; i < c; i++) {
		s = Cmd_Argv(i);
		if (*s != '#') {
			Com_Printf ("A filter must start with \"#\"\n");
			return;
		}
		if (strchr(s + 1, ' ')) {
			Com_Printf ("A filter may not contain spaces\n");
			return;
		}
		Q_strncpyz (filter_strings[num_filters], s + 1, sizeof(filter_strings[0]));
		num_filters++;
		if (num_filters >= 8)
			break;
	}
}

void TP_DumpMsgFilters(FILE *f) {
	int i;
	
	fprintf(f, "filter       ");
	if (!num_filters)
		fprintf(f, "clear");

	for (i = 0; i < num_filters; i++)
		fprintf (f, "#%s ", filter_strings[i]);

	fprintf(f, "\n");
}

/************************************ INIT ************************************/

void TP_Init (void) {
	TP_AddMacros();

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register (&cl_parseFunChars);
	Cvar_Register (&cl_parseSay);
	Cvar_Register (&cl_nofake);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&cl_enemybothskin);
	Cvar_Register (&cl_teambothskin);
	Cvar_Register (&cl_enemypentskin);
	Cvar_Register (&cl_teampentskin);
	Cvar_Register (&cl_enemyquadskin);
	Cvar_Register (&cl_teamquadskin);
	Cvar_Register (&cl_enemyskin);
	Cvar_Register (&cl_teamskin);

	Cvar_SetCurrentGroup(CVAR_GROUP_COMMUNICATION);
	Cvar_Register (&tp_triggers);
	Cvar_Register (&tp_msgtriggers);
	Cvar_Register (&tp_forceTriggers);
	Cvar_Register (&tp_loadlocs);
	Cvar_Register (&tp_soundtrigger);
	Cvar_Register (&tp_weapon_order);		

	Cvar_SetCurrentGroup(CVAR_GROUP_ITEM_NAMES);
	Cvar_Register (&tp_name_separator);		
	Cvar_Register (&tp_name_none);			
	Cvar_Register (&tp_name_nothing);
	Cvar_Register (&tp_name_at);
	Cvar_Register (&tp_name_someplace);

	Cvar_Register(&tp_name_status_blue);
	Cvar_Register(&tp_name_status_red);
	Cvar_Register(&tp_name_status_yellow);
	Cvar_Register(&tp_name_status_green);

	Cvar_Register (&tp_name_pented);
	Cvar_Register (&tp_name_quaded);
	Cvar_Register (&tp_name_eyes);
	Cvar_Register (&tp_name_enemy);
	Cvar_Register (&tp_name_teammate);
	Cvar_Register (&tp_name_disp);
	Cvar_Register (&tp_name_sentry);
	Cvar_Register (&tp_name_flag);
	Cvar_Register (&tp_name_backpack);
	Cvar_Register (&tp_name_weapon);
	Cvar_Register (&tp_name_armor);
	Cvar_Register (&tp_name_health);
	Cvar_Register (&tp_name_mh);
	Cvar_Register (&tp_name_suit);
	Cvar_Register (&tp_name_ring);
	Cvar_Register (&tp_name_pent);
	Cvar_Register (&tp_name_quad);
	Cvar_Register (&tp_name_cells);
	Cvar_Register (&tp_name_rockets);
	Cvar_Register (&tp_name_nails);
	Cvar_Register (&tp_name_shells);
	Cvar_Register (&tp_name_armortype_ra);
	Cvar_Register (&tp_name_armortype_ya);
	Cvar_Register (&tp_name_armortype_ga);
	Cvar_Register (&tp_name_ra);
	Cvar_Register (&tp_name_ya);
	Cvar_Register (&tp_name_ga);
	Cvar_Register (&tp_name_lg);
	Cvar_Register (&tp_name_rl);
	Cvar_Register (&tp_name_gl);
	Cvar_Register (&tp_name_sng);
	Cvar_Register (&tp_name_ng);
	Cvar_Register (&tp_name_ssg);
	Cvar_Register (&tp_name_sg);
	Cvar_Register (&tp_name_axe);

	Cvar_SetCurrentGroup(CVAR_GROUP_ITEM_NEED);
	Cvar_Register (&tp_need_health);
	Cvar_Register (&tp_need_cells);
	Cvar_Register (&tp_need_rockets);
	Cvar_Register (&tp_need_nails);
	Cvar_Register (&tp_need_shells);
	Cvar_Register (&tp_need_ra);
	Cvar_Register (&tp_need_ya);
	Cvar_Register (&tp_need_ga);
	Cvar_Register (&tp_need_weapon);
	Cvar_Register (&tp_need_rl);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("loadloc", TP_LoadLocFile_f);
	Cmd_AddCommand ("filter", TP_MsgFilter_f);
	Cmd_AddCommand ("msg_trigger", TP_MsgTrigger_f);
	Cmd_AddCommand ("teamcolor", TP_TeamColor_f);
	Cmd_AddCommand ("enemycolor", TP_EnemyColor_f);
	Cmd_AddCommand ("tp_took", TP_Took_f);
	Cmd_AddCommand ("tp_pickup", TP_Pickup_f);
	Cmd_AddCommand ("tp_point", TP_Point_f);
}
