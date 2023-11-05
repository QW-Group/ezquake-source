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

    $Id: teamplay.c,v 1.96 2007-10-23 08:51:02 himan Exp $
*/

#include <time.h>
#include <string.h>
#include <limits.h>
#include "quakedef.h"
#include "ignore.h"
#include "gl_model.h"
#include "teamplay.h"
#include "rulesets.h"
#include "pmove.h"
#include "stats_grid.h"
#include "utils.h"
#include "qsound.h"
#include "tp_msgs.h"

void OnChangeSkinForcing(cvar_t *var, char *string, qbool *cancel);
void OnChangeColorForcing(cvar_t *var, char *string, qbool *cancel);
void OnChangeSkinAndColorForcing(cvar_t *var, char *string, qbool *cancel);

cvar_t cl_parseSay = {"cl_parseSay", "1"};
cvar_t cl_parseFunChars = {"cl_parseFunChars", "1"};
cvar_t cl_nofake = {"cl_nofake", "2"};
cvar_t tp_loadlocs = {"tp_loadlocs", "1"};
cvar_t tp_pointpriorities = {"tp_pointpriorities", "0"}; // FIXME: buggy
cvar_t tp_tooktimeout = {"tp_tooktimeout", "15"};
cvar_t tp_pointtimeout = {"tp_pointtimeout", "15"};
static cvar_t tp_poweruptextstyle = { "tp_poweruptextstyle", "0" };

cvar_t  cl_teamtopcolor = {"teamtopcolor", "-1", 0, OnChangeColorForcing};
cvar_t  cl_teambottomcolor = {"teambottomcolor", "-1", 0, OnChangeColorForcing};
cvar_t  cl_enemytopcolor = {"enemytopcolor", "-1", 0, OnChangeColorForcing};
cvar_t  cl_enemybottomcolor = {"enemybottomcolor", "-1", 0, OnChangeColorForcing};
cvar_t  cl_teamskin = {"teamskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_enemyskin = {"enemyskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_teamquadskin = {"teamquadskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_enemyquadskin = {"enemyquadskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_teampentskin = {"teampentskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_enemypentskin = {"enemypentskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_teambothskin = {"teambothskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_enemybothskin = {"enemybothskin", "", 0, OnChangeSkinForcing};
cvar_t  cl_teamlock = {"teamlock", "0", 0, OnChangeSkinAndColorForcing};

cvar_t	tp_name_axe = {"tp_name_axe", "axe"};
cvar_t	tp_name_sg = {"tp_name_sg", "sg"};
cvar_t	tp_name_ssg = {"tp_name_ssg", "ssg"};
cvar_t	tp_name_ng = {"tp_name_ng", "ng"};
cvar_t	tp_name_sng = {"tp_name_sng", "sng"};
cvar_t	tp_name_gl = {"tp_name_gl", "gl"};
cvar_t	tp_name_rl = {"tp_name_rl", "{&cf13rl&cfff}"};
cvar_t	tp_name_lg = {"tp_name_lg", "{&c2aalg&cfff}"};
cvar_t	tp_name_rlg = {"tp_name_rlg", "{&cf13rl&cfff}{&c2aag&cfff}"};
cvar_t	tp_name_armortype_ra = {"tp_name_armortype_ra", "{&cf00r&cfff}"};
cvar_t	tp_name_armortype_ya = {"tp_name_armortype_ya", "{&cff0y&cfff}"};
cvar_t	tp_name_armortype_ga = {"tp_name_armortype_ga", "{&c0b0g&cfff}"};
cvar_t	tp_name_ra = {"tp_name_ra", "{&cf00ra&cfff}"};
cvar_t	tp_name_ya = {"tp_name_ya", "{&cff0ya&cfff}"};
cvar_t	tp_name_ga = {"tp_name_ga", "{&c0b0ga&cfff}"};
cvar_t	tp_name_quad = {"tp_name_quad", "{&c05fquad&cfff}"};
cvar_t	tp_name_pent = {"tp_name_pent", "{&cf00pent&cfff}"};
cvar_t	tp_name_ring = {"tp_name_ring", "{&cff0ring&cfff}"};
cvar_t	tp_name_suit = {"tp_name_suit", "suit"};
cvar_t	tp_name_shells = {"tp_name_shells", "shells"};
cvar_t	tp_name_nails = {"tp_name_nails", "nails"};
cvar_t	tp_name_rockets = {"tp_name_rockets", "rox"};
cvar_t	tp_name_cells = {"tp_name_cells", "cells"};
cvar_t	tp_name_mh = {"tp_name_mh", "{&c0a0mega&cfff}"};
cvar_t	tp_name_health = {"tp_name_health", "health"};
cvar_t	tp_name_armor = {"tp_name_armor", "armor"};
cvar_t	tp_name_weapon = {"tp_name_weapon", "weapon"};
cvar_t	tp_name_backpack = {"tp_name_backpack", "{&cf2apack&cfff}"};
cvar_t	tp_name_flag = {"tp_name_flag", "flag"};
cvar_t	tp_name_sentry = {"tp_name_sentry", "sentry gun"};
cvar_t	tp_name_disp = {"tp_name_disp", "dispenser"};
cvar_t	tp_name_filter = {"tp_name_filter", ""};
cvar_t	tp_name_rune1 = {"tp_name_rune1", "{&c0f0resistance&cfff}"};
cvar_t	tp_name_rune2 = {"tp_name_rune2", "{&cf00strength&cfff}"};
cvar_t	tp_name_rune3 = {"tp_name_rune3", "{&cff0haste&cfff}"};
cvar_t	tp_name_rune4 = {"tp_name_rune4", "{&c0ffregeneration&cfff}"};
cvar_t	tp_name_teammate = {"tp_name_teammate", ""};
cvar_t	tp_name_enemy = {"tp_name_enemy", "{&cf00enemy&cfff}"};
cvar_t	tp_name_eyes = {"tp_name_eyes", "{&cff0eyes&cfff}"};
cvar_t	tp_name_quaded = {"tp_name_quaded", "{&c05fquaded&cfff}"};
cvar_t	tp_name_pented = {"tp_name_pented", "{&cf00pented&cfff}"};
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
cvar_t	tp_name_status_white = {"tp_name_status_white", "$W"};

cvar_t	tp_need_ra = {"tp_need_ra", "120"};
cvar_t	tp_need_ya = {"tp_need_ya", "80"};
cvar_t	tp_need_ga = {"tp_need_ga", "60"};
cvar_t	tp_need_health = {"tp_need_health", "50"};
cvar_t	tp_need_weapon = {"tp_need_weapon", "87"};
cvar_t	tp_need_rl = {"tp_need_rl", "1"};
cvar_t	tp_need_rockets = {"tp_need_rockets", "5"};
cvar_t	tp_need_cells = {"tp_need_cells", "13"};
cvar_t	tp_need_nails = {"tp_need_nails", "0"}; // not so important, so let's not have it spam msg need
cvar_t	tp_need_shells = {"tp_need_shells", "0"}; // not so important, so let's not have it spam msg need

static qbool suppress;

char *skinforcing_team = "";

void TP_FindModelNumbers (void);
void TP_FindPoint (void);

static void CountNearbyPlayers(qbool dead);
char *Macro_LastTookOrPointed (void);
char *Macro_LastTookOrPointed2 (void);
void R_TranslatePlayerSkin (int playernum);

#define	POINT_TYPE_ITEM			1
#define POINT_TYPE_POWERUP		2
#define POINT_TYPE_TEAMMATE		3
#define	POINT_TYPE_ENEMY		4

#define TP_MACRO_ALIGNMENT_LEFT     0
#define TP_MACRO_ALIGNMENT_RIGHT    1
#define TP_MACRO_ALIGNMENT_CENTERED 2

tvars_t vars;

char lastip[64]; // FIXME: remove it

/*********************************** MACROS ***********************************/

#define MAX_MACRO_VALUE	256
static char	macro_buf[MAX_MACRO_VALUE] = "";

char *Macro_Lastip_f (void)
{
	snprintf (macro_buf, sizeof(macro_buf), "%s", lastip);
	return macro_buf;
}

char *Macro_Quote_f (void)
{
	return "\"";
}

char *Macro_Latency (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", Q_rint(cls.latency * 1000));
	return macro_buf;
}

char *Macro_Health (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_HEALTH]);
	return macro_buf;
}

char *Macro_Armor (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_ARMOR]);
	return macro_buf;
}

char *Macro_Shells (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_SHELLS]);
	return macro_buf;
}

char *Macro_Nails (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_NAILS]);
	return macro_buf;
}

char *Macro_Rockets (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_ROCKETS]);
	return macro_buf;
}

char *Macro_Cells (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_CELLS]);
	return macro_buf;
}

char *Macro_Ammo (void)
{
	snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_AMMO]);
	return macro_buf;
}

char *Macro_Weapon (void)
{
	return TP_ItemName(cl.stats[STAT_ACTIVEWEAPON]);
}

char *Macro_WeaponAndAmmo (void)
{
	char buf[MAX_MACRO_VALUE];
	snprintf (buf, sizeof(buf), "%s:%s", Macro_Weapon(), Macro_Ammo());
	strlcpy (macro_buf, buf, sizeof(macro_buf));
	return macro_buf;
}

char *Macro_WeaponNum (void)
{
	extern cvar_t cl_weaponpreselect;
	int best;

	if (cl_weaponpreselect.integer && (best = IN_BestWeapon(true))) {
		char buf[4];
		snprintf(buf, sizeof(buf), "%d", best);
		strlcpy(macro_buf, buf, sizeof(macro_buf));
		return macro_buf;
	}
	else {
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
}

int BestWeapon (void)
{
	return BestWeaponFromStatItems (cl.stats[STAT_ITEMS]);
}

int BestWeaponFromStatItems (int stat)
{
	int i;
	char *t[] = {tp_weapon_order.string, "78654321", NULL}, **s;

	for (s = t; *s; s++) {
		for (i = 0 ; i < strlen(*s) ; i++) {
			switch ((*s)[i]) {
					case '1': if (stat & IT_AXE) return IT_AXE; break;
					case '2': if (stat & IT_SHOTGUN) return IT_SHOTGUN; break;
					case '3': if (stat & IT_SUPER_SHOTGUN) return IT_SUPER_SHOTGUN; break;
					case '4': if (stat & IT_NAILGUN) return IT_NAILGUN; break;
					case '5': if (stat & IT_SUPER_NAILGUN) return IT_SUPER_NAILGUN; break;
					case '6': if (stat & IT_GRENADE_LAUNCHER) return IT_GRENADE_LAUNCHER; break;
					case '7': if (stat & IT_ROCKET_LAUNCHER) return IT_ROCKET_LAUNCHER; break;
					case '8': if (stat & IT_LIGHTNING) return IT_LIGHTNING; break;
			}
		}
	}
	return 0;
}

char *Macro_BestWeapon (void)
{
	return TP_ItemName(BestWeapon());
}

char *TP_ItemName(int item_flag)
{
	switch (item_flag) {
		case IT_SHOTGUN: return tp_name_sg.string;
		case IT_SUPER_SHOTGUN: return tp_name_ssg.string;
		case IT_NAILGUN: return tp_name_ng.string;
		case IT_SUPER_NAILGUN: return tp_name_sng.string;
		case IT_GRENADE_LAUNCHER: return tp_name_gl.string;
		case IT_ROCKET_LAUNCHER: return tp_name_rl.string;
		case IT_LIGHTNING: return tp_name_lg.string;
		case IT_SUPER_LIGHTNING: return "slg";
		case IT_SHELLS: return tp_name_shells.string;
		case IT_NAILS: return tp_name_nails.string;
		case IT_ROCKETS: return tp_name_rockets.string;
		case IT_CELLS: return tp_name_cells.string;
		case IT_AXE: return tp_name_axe.string;
		case IT_ARMOR1: return tp_name_ga.string;
		case IT_ARMOR2: return tp_name_ya.string;
		case IT_ARMOR3: return tp_name_ra.string;
		case IT_SUPERHEALTH: return tp_name_mh.string;
		case IT_KEY1: return "key1";
		case IT_KEY2: return "key2";
		case IT_INVISIBILITY: return tp_name_ring.string;
		case IT_INVULNERABILITY: return tp_name_pent.string;
		case IT_SUIT: return tp_name_suit.string;
		case IT_QUAD: return tp_name_quad.string;
		case IT_SIGIL1: return tp_name_rune1.string;
		case IT_SIGIL2: return tp_name_rune2.string;
		case IT_SIGIL3: return tp_name_rune3.string;
		case IT_SIGIL4: return tp_name_rune4.string;
		default: return tp_name_none.string;
	}
}

char *Macro_BestAmmo (void)
{
	switch (BestWeapon()) {
			case IT_SHOTGUN: case IT_SUPER_SHOTGUN:
			snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_SHELLS]);
			return macro_buf;

			case IT_NAILGUN: case IT_SUPER_NAILGUN:
			snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_NAILS]);
			return macro_buf;

			case IT_GRENADE_LAUNCHER: case IT_ROCKET_LAUNCHER:
			snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_ROCKETS]);
			return macro_buf;

			case IT_LIGHTNING:
			snprintf(macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_CELLS]);
			return macro_buf;

			default: return "0";
	}
}

// needed for %b parsing
char *Macro_BestWeaponAndAmmo (void)
{
	char buf[MAX_MACRO_VALUE];

	snprintf (buf, sizeof(buf), "%s:%s", Macro_BestWeapon(), Macro_BestAmmo());

	strlcpy (macro_buf, buf, sizeof(macro_buf));
	return macro_buf;
}

char *Macro_Colored_Armor_f (void)
{
    snprintf(macro_buf, sizeof(macro_buf), "%s", TP_MSG_Colored_Armor());
    return macro_buf;
}

char *Macro_Colored_Powerups_f (void)
{
	snprintf (macro_buf, sizeof(macro_buf), "%s", TP_MSG_Colored_Powerup());
	return macro_buf;
}

char *Macro_Colored_Short_Powerups_f (void) // same as above, but displays "qrp" instead of "quad ring pent"
{
	snprintf (macro_buf, sizeof(macro_buf), "%s", TP_MSG_Colored_Short_Powerups());
	return macro_buf;
}

char* Macro_Teamplay_Powerups_f(void)
{
	if (tp_poweruptextstyle.integer) {
		return Macro_Colored_Short_Powerups_f();
	}
	return Macro_Colored_Powerups_f();
}

char *Macro_ArmorType (void)
{
	if (cl.stats[STAT_ITEMS] & IT_ARMOR1)
		return tp_name_armortype_ga.string;
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR2)
		return tp_name_armortype_ya.string;
	else if (cl.stats[STAT_ITEMS] & IT_ARMOR3)
		return tp_name_armortype_ra.string;
	else
		return tp_name_none.string;
}

char *Macro_Powerups (void)
{
	int effects;

	macro_buf[0] = 0;

	if (cl.stats[STAT_ITEMS] & IT_QUAD)
		strlcpy(macro_buf, tp_name_quad.string, sizeof(macro_buf));

	if (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_pent.string, sizeof(macro_buf));
	}

	if (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_ring.string, sizeof(macro_buf));
	}

	effects = cl.frames[cl.parsecount & UPDATE_MASK].playerstate[cl.playernum].effects;
	if ( (effects & (EF_FLAG1|EF_FLAG2)) /* CTF */ ||
	        (cl.teamfortress && cl.stats[STAT_ITEMS] & (IT_KEY1|IT_KEY2)) /* TF */ ) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_flag.string, sizeof(macro_buf));
	}

	if (!macro_buf[0])
		strlcpy(macro_buf, tp_name_none.string, sizeof(macro_buf));

	return macro_buf;
}

char *Macro_Location (void)
{
	strlcpy(vars.lastreportedloc, TP_LocationName(cl.simorg), sizeof(vars.lastreportedloc));

	return vars.lastreportedloc;
}

char *Macro_LastDeath (void)
{
	return vars.deathtrigger_time ? vars.lastdeathloc : tp_name_someplace.string;
}

char *Macro_Last_Location (void)
{
	if (vars.deathtrigger_time && cls.realtime - vars.deathtrigger_time <= 5)
		strlcpy(vars.lastreportedloc, vars.lastdeathloc, sizeof(vars.lastreportedloc));
	else
		strlcpy(vars.lastreportedloc, TP_LocationName(cl.simorg), sizeof(vars.lastreportedloc));
	return vars.lastreportedloc;
}

char *Macro_LastReportedLoc(void)
{
	if (!vars.lastreportedloc[0])
		return tp_name_someplace.string;
	return vars.lastreportedloc;
}

char *Macro_Rune (void)
{
	if (cl.stats[STAT_ITEMS] & IT_SIGIL1)
		return tp_name_rune1.string;
	else if (cl.stats[STAT_ITEMS] & IT_SIGIL2)
		return tp_name_rune2.string;
	else if (cl.stats[STAT_ITEMS] & IT_SIGIL3)
		return tp_name_rune3.string;
	else if (cl.stats[STAT_ITEMS] & IT_SIGIL4)
		return tp_name_rune4.string;
	else
		return "";
}

char *Macro_Time (void)
{
	time_t t;
	struct tm *ptm;

	time (&t);
	if (!(ptm = localtime (&t)))
		return "#bad date#";
	strftime (macro_buf, sizeof(macro_buf) - 1, "%H:%M", ptm);
	return macro_buf;
}

char *Macro_Date (void)
{
	time_t t;
	struct tm *ptm;

	time (&t);
	if (!(ptm = localtime (&t)))
		return "#bad date#";
	strftime (macro_buf, sizeof(macro_buf) - 1, "%d.%m.%y", ptm);
	return macro_buf;
}

char *Macro_DateIso(void) {
	time_t t;
	struct tm *ptm;

	time (&t);
	if (!(ptm = localtime (&t)))
		return "#bad date#";
	strftime (macro_buf, sizeof(macro_buf) - 1, "%Y-%m-%d_%H-%M-%S", ptm);
	return macro_buf;
}

char* Macro_TimeStamp(void)
{
	time_t t;
	struct tm* ptm;

	time(&t);
	if (!(ptm = localtime(&t)))
		return "_baddate_";

	strftime(macro_buf, sizeof(macro_buf) - 1, "%Y%m%d-%H%M", ptm);
	return macro_buf;
}

// returns the last item picked up
char *Macro_Took (void)
{
	if (TOOK_EMPTY())
		strlcpy (macro_buf, tp_name_nothing.string, sizeof(macro_buf));
	else
		strlcpy (macro_buf, vars.tookname, sizeof(macro_buf));
	return macro_buf;
}

// returns location of the last item picked up
char *Macro_TookLoc (void)
{
	if (TOOK_EMPTY())
		strlcpy (macro_buf, tp_name_someplace.string, sizeof(macro_buf));
	else
		strlcpy (macro_buf, vars.tookloc, sizeof(macro_buf));
	return macro_buf;
}

// %i macro - last item picked up in "name at location" style
char *Macro_TookAtLoc (void)
{
	if (TOOK_EMPTY())
		strlcpy (macro_buf, tp_name_nothing.string, sizeof(macro_buf));
	else {
		strlcpy (macro_buf, va("%s %s %s", vars.tookname, tp_name_at.string, vars.tookloc), sizeof(macro_buf));
	}
	return macro_buf;
}

// pointing calculations are CPU expensive, so the results are cached
// in vars.pointname & vars.pointloc
char *Macro_PointName (void)
{
	if (flashed) // there should be more smart way to do it
		return tp_name_nothing.string;

	TP_FindPoint ();
	return vars.pointname;
}

char *Macro_PointLocation (void)
{
	if (flashed) // there should be more smart way to do it
		return tp_name_nothing.string;

	TP_FindPoint ();
	return vars.pointloc[0] ? vars.pointloc : Macro_Location();
}

char *Macro_LastPointAtLoc (void)
{
	if (!vars.pointtime || cls.realtime - vars.pointtime > tp_pointtimeout.value)
		strlcpy (macro_buf, tp_name_nothing.string, sizeof(macro_buf));
	else
		snprintf (macro_buf, sizeof(macro_buf), "%s %s %s", vars.pointname, tp_name_at.string, vars.pointloc[0] ? vars.pointloc : Macro_Location());
	return macro_buf;
}

char *Macro_PointNameAtLocation (void)
{
	if (flashed) // there should be more smart way to do it
		return tp_name_nothing.string;

	TP_FindPoint();
	return Macro_LastPointAtLoc();
}

char *Macro_Weapons (void)
{
	macro_buf[0] = 0;

	if (cl.stats[STAT_ITEMS] & IT_LIGHTNING)
		strlcpy(macro_buf, tp_name_lg.string, sizeof(macro_buf));

	if (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_rl.string, sizeof(macro_buf));
	}
	if (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_gl.string, sizeof(macro_buf));
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_sng.string, sizeof(macro_buf));
	}
	if (cl.stats[STAT_ITEMS] & IT_NAILGUN) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_ng.string, sizeof(macro_buf));
	}
	if (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_ssg.string, sizeof(macro_buf));
	}
	if (cl.stats[STAT_ITEMS] & IT_SHOTGUN) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_sg.string, sizeof(macro_buf));
	}
	if (cl.stats[STAT_ITEMS] & IT_AXE) {
		if (macro_buf[0])
			strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat(macro_buf, tp_name_axe.string, sizeof(macro_buf));
	}

	if (!macro_buf[0])
		strlcpy(macro_buf, tp_name_none.string, sizeof(macro_buf));

	return macro_buf;
}

static char *Skin_To_TFSkin (char *myskin) // These four TF classes don't have their full names as the skin (i.e. they have tf_snipe instead of tf_sniper)
{
	if (!cl.teamfortress || cl.spectator || strncasecmp(myskin, "tf_", 3)) {
		strlcpy(macro_buf, myskin, sizeof(macro_buf));
	} else {
		if (!strcasecmp(myskin, "tf_demo"))
			strlcpy(macro_buf, "demoman", sizeof(macro_buf));
		else if (!strcasecmp(myskin, "tf_eng"))
			strlcpy(macro_buf, "engineer", sizeof(macro_buf));
		else if (!strcasecmp(myskin, "tf_snipe"))
			strlcpy(macro_buf, "sniper", sizeof(macro_buf));
		else if (!strcasecmp(myskin, "tf_sold"))
			strlcpy(macro_buf, "soldier", sizeof(macro_buf));
		else
			strlcpy(macro_buf, myskin + 3, sizeof(macro_buf));
	}
	return macro_buf;
}

char *Macro_TF_Skin (void)
{
	return Skin_To_TFSkin(Info_ValueForKey(cl.players[cl.playernum].userinfo, "skin"));
}

char *Macro_LastDrop (void)
{
	if (vars.lastdrop_time)
		return vars.lastdroploc;
	else
		return tp_name_someplace.string;
}

char *Macro_GameDir(void)
{
	snprintf(macro_buf, sizeof (macro_buf), "%s", cls.gamedirfile);
	return macro_buf;
}

char *Macro_LastTrigger_Match(void)
{
	return vars.lasttrigger_match;
}

char *Macro_LastDropTime (void)
{
	if (vars.lastdrop_time)
		snprintf (macro_buf, sizeof (macro_buf), "%d", (int) (cls.realtime - vars.lastdrop_time));
	else
		snprintf (macro_buf, sizeof (macro_buf), "%d", -1);
	return macro_buf;
}

// fixme: rewrite this function into two separate functions
// first will just set vars.needflags value (put into TP_GetNeed function below)
// second will read it's value and produce appropriate $need macro string
char *Macro_Need (void)
{
	int i, weapon;
	char *needammo = NULL;

	macro_buf[0] = 0;
	vars.needflags = 0;

	// check armor
	if (((cl.stats[STAT_ITEMS] & IT_ARMOR1) && cl.stats[STAT_ARMOR] < tp_need_ga.value)
		|| ((cl.stats[STAT_ITEMS] & IT_ARMOR2) && cl.stats[STAT_ARMOR] < tp_need_ya.value)
		|| ((cl.stats[STAT_ITEMS] & IT_ARMOR3) && cl.stats[STAT_ARMOR] < tp_need_ra.value)
		|| (!(cl.stats[STAT_ITEMS] & (IT_ARMOR1|IT_ARMOR2|IT_ARMOR3))
		&& (tp_need_ga.value || tp_need_ya.value || tp_need_ra.value))
	   )
	{
		strlcpy (macro_buf, tp_name_armor.string, sizeof(macro_buf));
		vars.needflags |= it_armor;
	}

	// check health
	if (tp_need_health.value && cl.stats[STAT_HEALTH] < tp_need_health.value) {
		if (macro_buf[0])
			strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat (macro_buf, tp_name_health.string, sizeof(macro_buf));
		vars.needflags |= it_health;
	}

	if (cl.teamfortress) {
		if (cl.stats[STAT_ROCKETS] < tp_need_rockets.value)	{
			if (macro_buf[0])
				strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat (macro_buf, tp_name_rockets.string, sizeof(macro_buf));
			vars.needflags |= it_rockets;
		}
		if (cl.stats[STAT_SHELLS] < tp_need_shells.value) {
			if (macro_buf[0])
				strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat (macro_buf, tp_name_shells.string, sizeof(macro_buf));
			vars.needflags |= it_shells;
		}
		if (cl.stats[STAT_NAILS] < tp_need_nails.value)	{
			if (macro_buf[0])
				strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat (macro_buf, tp_name_nails.string, sizeof(macro_buf));
			vars.needflags |= it_nails;
		}
		if (cl.stats[STAT_CELLS] < tp_need_cells.value)	{
			if (macro_buf[0])
				strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat (macro_buf, tp_name_cells.string, sizeof(macro_buf));
			vars.needflags |= it_cells;
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
			strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
		strlcat (macro_buf, tp_name_weapon.string, sizeof(macro_buf));
		vars.needflags |= it_weapons;
	} else {
		if (tp_need_rl.value && !(cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)) {
			if (macro_buf[0])
				strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat (macro_buf, tp_name_rl.string, sizeof(macro_buf));
			vars.needflags |= it_rl;
		}

		switch (weapon) {
				case 2: case 3: if (cl.stats[STAT_SHELLS] < tp_need_shells.value) {
					needammo = tp_name_shells.string;
					vars.needflags |= it_shells;
				}
				break;
				case 4: case 5: if (cl.stats[STAT_NAILS] < tp_need_nails.value) {
					needammo = tp_name_nails.string;
					vars.needflags |= it_nails;
				}
				break;
				case 6: case 7: if (cl.stats[STAT_ROCKETS] < tp_need_rockets.value) {
					needammo = tp_name_rockets .string;
					vars.needflags |= it_rockets;
				} break;
				case 8: if (cl.stats[STAT_CELLS] < tp_need_cells.value) {
					needammo = tp_name_cells.string;
					vars.needflags |= it_cells;
				} break;
		}

		if (needammo) {
			if (macro_buf[0])
				strlcat (macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat (macro_buf, needammo, sizeof(macro_buf));
			vars.needflags |= it_ammo;
		}
	}

done:
	if (!macro_buf[0])
		strlcpy (macro_buf, tp_name_nothing.string, sizeof(macro_buf));

	return macro_buf;
}

void TP_GetNeed(void)
{
	Macro_Need();
}

char *Macro_Point_LED(void)
{
	TP_FindPoint();

	if (vars.pointtype == POINT_TYPE_ENEMY)
		return tp_name_status_red.string;
	else if (vars.pointtype == POINT_TYPE_TEAMMATE)
		return tp_name_status_green.string;
	else if (vars.pointtype == POINT_TYPE_POWERUP)
		return tp_name_status_yellow.string;
	else // POINT_TYPE_ITEM
		return tp_name_status_blue.string;
}

static int Macro_TeamSort(const void* lhs_, const void* rhs_)
{
	const char* lhs = *(const char**)lhs_;
	const char* rhs = *(const char**)rhs_;

	return Q_strcmp2(lhs, rhs);
}

static const char* Macro_TeamPick(int team_number, const char* default_teamname)
{
	int i, j;
	int team_count = 0;
	const char* teamnames[MAX_CLIENTS];


	for (i = 0; i < sizeof(cl.players) / sizeof(cl.players[0]); ++i) {
		if (cl.players[i].name[0] && !cl.players[i].spectator) {
			const char* team = (cl.teamplay ? cl.players[i].team : cl.players[i].name);

			for (j = 0; j < team_count; ++j) {
				if (!strcmp(team, teamnames[j])) {
					break;
				}
			}

			if (j >= team_count && team_count < sizeof(teamnames) / sizeof(teamnames[0])) {
				teamnames[team_count++] = team;
			}
		}
	}

	if (team_count > team_number) {
		qsort((void*)teamnames, team_count, sizeof(teamnames[0]), Macro_TeamSort);

		return teamnames[team_number];
	}
	else {
		return default_teamname;
	}
}

char *Macro_Team1(void)
{
	static char buffer[MAX_MACRO_VALUE];

	strlcpy(buffer, Macro_TeamPick(0, "team1"), sizeof(buffer));

	return buffer;
}

char* Macro_Team2(void)
{
	static char buffer[MAX_MACRO_VALUE];

	strlcpy(buffer, Macro_TeamPick(1, "team2"), sizeof(buffer));

	return buffer;
}

char *Macro_MyStatus_LED(void)
{
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
		snprintf(macro_buf, sizeof(macro_buf), "%s", tp_name_status_green.string);
	else if (count <= 1)
		snprintf(macro_buf, sizeof(macro_buf), "%s", tp_name_status_yellow.string);
	else
		snprintf(macro_buf, sizeof(macro_buf), "%s", tp_name_status_red.string);

	return macro_buf;
}

char *Macro_EnemyStatus_LED(void)
{
	CountNearbyPlayers(false);
	if (vars.numenemies == 0)
		snprintf(macro_buf, sizeof(macro_buf), "\xffl%s\xff", tp_name_status_green.string);
	else if (vars.numenemies <= vars.numfriendlies)
		snprintf(macro_buf, sizeof(macro_buf), "\xffl%s\xff", tp_name_status_yellow.string);
	else
		snprintf(macro_buf, sizeof(macro_buf), "\xffl%s\xff", tp_name_status_red.string);

	suppress = true;
	return macro_buf;
}


#define TP_PENT 1
#define TP_QUAD 2
#define TP_RING 4

char *Macro_LastSeenPowerup(void)
{
	if (!vars.enemy_powerups_time || cls.realtime - vars.enemy_powerups_time > 5) {
		strlcpy(macro_buf, tp_name_quad.string, sizeof(macro_buf));
	} else {
		macro_buf[0] = 0;
		if (vars.enemy_powerups & TP_QUAD)
			strlcat(macro_buf, tp_name_quad.string, sizeof(macro_buf));
		if (vars.enemy_powerups & TP_PENT) {
			if (macro_buf[0])
				strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat(macro_buf, tp_name_pent.string, sizeof(macro_buf));
		}
		if (vars.enemy_powerups & TP_RING) {
			if (macro_buf[0])
				strlcat(macro_buf, tp_name_separator.string, sizeof(macro_buf));
			strlcat(macro_buf, tp_name_ring.string, sizeof(macro_buf));
		}
	}
	return macro_buf;
}

qbool TP_SuppressMessage (wchar *buf)
{
	size_t len;
	wchar *s;

	if ((len = qwcslen (buf)) < 4)
		return false;

	s = buf + len - 4;

	if (s[0] == 0x7F && s[1] == '!' && s[3] == '\n') {
		*s++ = '\n';
		*s++ = 0;
		return (!cls.demoplayback && !cl.spectator && *s - 'A' == cl.playernum);
	}

	return false;
}

// things like content '%e' macro get hidden in here causing you yourself cannot see
// how many enemies are around you, the number get replaced with a 'x' char
// and then printed on screen as a message
void TP_PrintHiddenMessage(char *buf, int nodisplay)
{
	qbool team, hide = false;
	char dest[4096], msg[4096], *s, *d, c, *name;
	int length, offset, flags;
	extern cvar_t con_sound_mm2_file, con_sound_mm2_volume, cl_fakename, cl_fakename_suffix;

	if (!buf || !(length = strlen(buf)))
		return;

	team = !strcasecmp("say_team", Cmd_Argv(0));

	if (length >= 2 && buf[0] == '\"' && buf[length - 1] == '\"') {
		memmove(buf, buf + 1, length - 2);
		buf[length - 2] = 0;
	}

	s = buf;
	d = dest;

	while ((c = *s++) && (c != '\x7f')) {
		if (c == '\xff') {
			if ((hide = !hide)) {
				*d++ = (*s == 'z') ? 'x' : (char)139;
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

	// Player is ignoring themselves
	if (cl.players[cl.playernum].ignored) {
		return;
	}

	name = Info_ValueForKey (cl.players[cl.playernum].userinfo, "name");
	if (strlen(name) >= 32)
		name[31] = 0;

	if (team)
    {
        if (cl_fakename.string[0])
        {
        	char c_fn[1024], c_fna[1024];
        	strlcpy (c_fn, cl_fakename.string, sizeof(c_fn));
        	strlcpy (c_fna, cl_fakename_suffix.string, sizeof(c_fna));

			snprintf(msg, sizeof(msg), "%s\n", TP_ParseFunChars(strcat(strcat(c_fn, c_fna), dest) , true));
        }
        else
        {
            snprintf(msg, sizeof(msg), "(%s): %s\n", name, TP_ParseFunChars(dest, true));
        }
    }
	else
	{
		snprintf(msg, sizeof(msg), "%s: %s\n", name, TP_ParseFunChars(dest, true));
	}

	flags = TP_CategorizeMessage (msg, &offset);

	if (flags == msgtype_team && !TP_FilterMessage(str2wcs(msg) + offset))
		return;

	if (con_sound_mm2_volume.value > 0 && nodisplay == 0) {
		S_LocalSoundWithVol(con_sound_mm2_file.string, con_sound_mm2_volume.value);
	}

	if (cl_nofake.value == 1 || (cl_nofake.value == 2 && flags != msgtype_team)) {
		for (s = msg; *s; s++)
			if (*s == 0x0D || (*s == 0x0A && s[1]))
				*s = ' ';
	}

	if (nodisplay == 0) {
		Com_Printf(wcs2str(TP_ParseWhiteText (str2wcs(msg), team, offset)));
	}

}

static void CountNearbyPlayers(qbool dead)
{
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

char *Macro_CountNearbyEnemyPlayers (void)
{
	const char* override_text = Ruleset_BlockPlayerCountMacros();

	if (override_text) {
		strlcpy(macro_buf, "\xff", sizeof(macro_buf));
		strlcat(macro_buf, override_text, sizeof(macro_buf));
		strlcat(macro_buf, "\xff", sizeof(macro_buf));
	}
	else {
		CountNearbyPlayers(false);
		snprintf(macro_buf, sizeof(macro_buf), "\xffz%d\xff", vars.numenemies);
		suppress = true;
	}
	return macro_buf;
}

char *Macro_Count_Last_NearbyEnemyPlayers (void)
{
	const char* override_text = Ruleset_BlockPlayerCountMacros();

	if (override_text) {
		strlcpy(macro_buf, override_text, sizeof(macro_buf));
	}
	else {
		if (vars.deathtrigger_time && cls.realtime - vars.deathtrigger_time <= 5) {
			snprintf(macro_buf, sizeof(macro_buf), "\xffz%d\xff", vars.last_numenemies);
		}
		else {
			CountNearbyPlayers(false);
			snprintf(macro_buf, sizeof(macro_buf), "\xffz%d\xff", vars.numenemies);
		}
		suppress = true;
	}
	return macro_buf;
}

char *Macro_CountNearbyFriendlyPlayers (void)
{
	const char* override_text = Ruleset_BlockPlayerCountMacros();

	if (override_text) {
		strlcpy(macro_buf, override_text, sizeof(macro_buf));
	}
	else {
		CountNearbyPlayers(false);
		snprintf(macro_buf, sizeof(macro_buf), "\xffz%d\xff", vars.numfriendlies);
		suppress = true;
	}
	return macro_buf;
}

char* Macro_Count_Last_NearbyFriendlyPlayers (void)
{
	const char* override_text = Ruleset_BlockPlayerCountMacros();

	if (override_text) {
		strlcpy(macro_buf, override_text, sizeof(macro_buf));
	}
	else {
		if (vars.deathtrigger_time && cls.realtime - vars.deathtrigger_time <= 5) {
			snprintf(macro_buf, sizeof(macro_buf), "\xffz%d\xff", vars.last_numfriendlies);
		}
		else {
			CountNearbyPlayers(false);
			snprintf(macro_buf, sizeof(macro_buf), "\xffz%d\xff", vars.numfriendlies);
		}
		suppress = true;
	}
	return macro_buf;
}

// Note: longer macro names like "armortype" must be defined
// _before_ the shorter ones like "armor" to be parsed properly
void TP_AddMacros(void)
{
	int teamplay = (int)Rulesets_RestrictTriggers();

	Cmd_AddMacro(macro_lastip, Macro_Lastip_f);
	Cmd_AddMacro(macro_qt, Macro_Quote_f);
	Cmd_AddMacro(macro_latency, Macro_Latency);
	Cmd_AddMacro(macro_ping, Macro_Latency);
	Cmd_AddMacro(macro_timestamp, Macro_TimeStamp);
	Cmd_AddMacro(macro_time, Macro_Time);
	Cmd_AddMacro(macro_date, Macro_Date);
	Cmd_AddMacro(macro_dateiso, Macro_DateIso);

	Cmd_AddMacroEx(macro_health, Macro_Health, teamplay);
	Cmd_AddMacroEx(macro_armortype, Macro_ArmorType, teamplay);
	Cmd_AddMacroEx(macro_armor, Macro_Armor, teamplay);
	Cmd_AddMacroEx(macro_colored_armor, Macro_Colored_Armor_f, teamplay);
	Cmd_AddMacroEx(macro_colored_powerups, Macro_Colored_Powerups_f, teamplay);
	Cmd_AddMacroEx(macro_colored_short_powerups, Macro_Colored_Short_Powerups_f, teamplay);
	Cmd_AddMacroEx(macro_tp_powerups, Macro_Teamplay_Powerups_f, teamplay);

	Cmd_AddMacroEx(macro_shells, Macro_Shells, teamplay);
	Cmd_AddMacroEx(macro_nails, Macro_Nails, teamplay);
	Cmd_AddMacroEx(macro_rockets, Macro_Rockets, teamplay);
	Cmd_AddMacroEx(macro_cells, Macro_Cells, teamplay);

	Cmd_AddMacro(macro_weaponnum, Macro_WeaponNum);
	Cmd_AddMacroEx(macro_weapons, Macro_Weapons, teamplay);
	Cmd_AddMacro(macro_weapon, Macro_Weapon);

	Cmd_AddMacroEx(macro_ammo, Macro_Ammo, teamplay);

	Cmd_AddMacroEx(macro_bestweapon, Macro_BestWeapon, teamplay);
	Cmd_AddMacroEx(macro_bestammo, Macro_BestAmmo, teamplay);

	Cmd_AddMacroEx(macro_powerups, Macro_Powerups, teamplay);

	Cmd_AddMacroEx(macro_location, Macro_Location, teamplay);
	Cmd_AddMacroEx(macro_deathloc, Macro_LastDeath, teamplay);

	Cmd_AddMacroEx(macro_tookatloc, Macro_TookAtLoc, teamplay);
	Cmd_AddMacroEx(macro_tookloc, Macro_TookLoc, teamplay);
	Cmd_AddMacroEx(macro_took, Macro_Took, teamplay);

	Cmd_AddMacroEx(macro_pointatloc, Macro_PointNameAtLocation, teamplay);
	Cmd_AddMacroEx(macro_pointloc, Macro_PointLocation, teamplay);
	Cmd_AddMacroEx(macro_point, Macro_PointName, teamplay);

	Cmd_AddMacroEx(macro_need, Macro_Need, teamplay);

	Cmd_AddMacroEx(macro_droploc, Macro_LastDrop, teamplay);
	Cmd_AddMacroEx(macro_droptime, Macro_LastDropTime, teamplay);

	Cmd_AddMacro(macro_tf_skin, Macro_TF_Skin);
	Cmd_AddMacro(macro_gamedir, Macro_GameDir);

	Cmd_AddMacro(macro_triggermatch, Macro_LastTrigger_Match);
	Cmd_AddMacroEx(macro_ledpoint, Macro_Point_LED, teamplay);
	Cmd_AddMacroEx(macro_ledstatus, Macro_MyStatus_LED, teamplay);

	Cmd_AddMacroEx(macro_lastloc, Macro_Last_Location, teamplay);
	Cmd_AddMacroEx(macro_lastpowerup, Macro_LastSeenPowerup, teamplay);

	Cmd_AddMacro(macro_team1, Macro_Team1);
	Cmd_AddMacro(macro_team2, Macro_Team2);
}

/********************** MACRO/FUNCHAR/WHITE TEXT PARSING **********************/

wchar *TP_ParseWhiteText (const wchar *s, qbool team, int offset)
{
	static wchar	buf[4096];
	wchar *out, *p1;
	const wchar* p;
	extern cvar_t	cl_parseWhiteText;
	qbool	parsewhite;

	parsewhite = cl_parseWhiteText.value == 1 || (cl_parseWhiteText.value == 2 && team);

	buf[0] = 0;
	out = buf;

	for (p = s; *p; p++) {
		if  (parsewhite && *p == '{' && p-s >= offset) {
			if ((p1 = qwcschr (p + 1, '}'))) {
				memcpy (out, p + 1, (p1 - p - 1)*sizeof(wchar));
				out += p1 - p - 1;
				p = p1;
				continue;
			}
		}
		if (*p != 10 && *p != 13 && !(p==s && (*p==1 || *p==2)) && *p <= 0x7F)
			*out++ = *p | 128;	// convert to red
		else
			*out++ = *p;
	}
	*out = 0;
	return buf;
}

static int TP_MacroStringLength(char* output, const char* text, int buffer_length, int printable_length)
{
	extern cvar_t cl_parseWhiteText;
	qbool in_colour = false;
	qbool in_braces = false;
	int printed_length = 0;
	int buffered_length = 0;
	const char* s = 0;

	memset(output, 0, buffer_length);
	buffer_length -= 1;
	for (s = text; *s && printed_length < printable_length && buffered_length < buffer_length - (in_braces ? 1 : 0) - (in_colour ? 2 : 0); ++s) {
		if (*s == '&') {
			if (s[1] == 'c' && s[2] && s[3] && s[4]) {
				if (HexToInt(s[2]) >= 0 && HexToInt(s[3]) >= 0 && HexToInt(s[4]) >= 0) {
					if (buffer_length - buffered_length <= 5)
						break;
					memcpy(output + buffered_length, s, 5);
					buffered_length += 5;
					in_colour = HexToInt(s[2]) != 15 || HexToInt(s[3]) != 15 || HexToInt(s[4]) != 15;	// &cFFF used instead of &r...
					s += 4;
					continue; 
				}
			}
			else if (s[1] == 'r') {
				if (buffer_length - buffered_length <= 2)
					break;
				output[buffered_length++] = '&';
				output[buffered_length++] = 'r';
				++s;
				in_colour = false;
				continue; 
			}
		}
		else if (cl_parseWhiteText.value) {
			// We don't really know if the other clients have this set, presume same settings on all machines
			if ((s[0] == '{' && s[1] != '{') || (s[0] == '}' && s[1] != '}')) {
				output[buffered_length++] = s[0];
				in_braces = (s[0] == '{');
				continue;
			}
		}

		output[buffered_length++] = s[0];
		++printed_length;
	}

	if (in_colour && buffered_length < buffer_length - 2) {
		output[buffered_length++] = '&';
		output[buffered_length++] = 'r';
	}
	if (in_braces && buffered_length < buffer_length - 1) {
		output[buffered_length++] = '}';
	}
	output[buffer_length - 1] = 0;

	return printed_length;
}

char* TP_AlignMacroText(char* text, int fixed_width, int alignment)
{
	static char output[MAX_MACRO_STRING];
	static char content[MAX_MACRO_STRING];
	int string_length = 0;
	int spaces = 0;

	if (fixed_width == 0)
		return text;

	string_length = TP_MacroStringLength(content, text, MAX_MACRO_STRING, fixed_width);
	spaces = max(fixed_width - string_length, 0);

	memset(output, 0, sizeof(output));
	switch (alignment)
	{
	case TP_MACRO_ALIGNMENT_RIGHT:
		snprintf(output, MAX_MACRO_STRING - 1, "%*s%s", spaces, "", content);
		break;
	case TP_MACRO_ALIGNMENT_CENTERED:
		snprintf(output, MAX_MACRO_STRING - 1, "%*s%s%*s", spaces / 2, "", content, (spaces + 1) / 2, "");
		break;
	case TP_MACRO_ALIGNMENT_LEFT:
	default:
		snprintf(output, MAX_MACRO_STRING - 1, "%s%*s", content, spaces, "");
		break;
	}

	return output;
}

void TP_SetDefaultMacroFormat(char* cvar_ext, int* fixed_width, int* alignment)
{
	char cvar_name[128] = { 0 };
	cvar_t* width_cvar; 
	cvar_t* alignment_cvar;

	snprintf(cvar_name, sizeof(cvar_name) - 1, "tp_length_%s", cvar_ext);
	width_cvar = Cvar_Find(cvar_name);

	*fixed_width = 0;
	*alignment = TP_MACRO_ALIGNMENT_LEFT;

	if (width_cvar) {
		*fixed_width = max(0, min(width_cvar->integer, 40));

		snprintf(cvar_name, sizeof(cvar_name) - 1, "tp_align_%s", cvar_ext);
		alignment_cvar = Cvar_Find(cvar_name);
		if (alignment_cvar && tolower(alignment_cvar->string[0]) == 'r')
			*alignment = TP_MACRO_ALIGNMENT_RIGHT;
		else if (alignment_cvar && tolower(alignment_cvar->string[0]) == 'c')
			*alignment = TP_MACRO_ALIGNMENT_CENTERED;
	}
}

static void TP_SetDefaultMacroCharFormat(qbool extended, char character, int* fixed_width, int* alignment)
{
	char cvar_ext[128] = { 0 };

	snprintf(cvar_ext, sizeof(cvar_ext) - 1, "%s%s%c", extended ? "ext_" : "", isupper(character) ? "caps_" : "", character);

	TP_SetDefaultMacroFormat(cvar_ext, fixed_width, alignment);
}

qbool TP_ReadMacroFormat(char* s, int* fixed_width, int* alignment, char** new_s)
{
	*new_s = s;
	*fixed_width = 0;
	*alignment = TP_MACRO_ALIGNMENT_LEFT;

	if (s[0] && s[1] == '<') {
		s += 2;

		while (s[0] && s[0] != '>') {
			if (s[0] >= '0' && s[0] <= '9') {
				*fixed_width = (*fixed_width) * 10 + (s[0] - '0');
			}
			else if ((s[0] == 'l' || s[0] == 'L') && s[1] == '>') {
				*alignment = TP_MACRO_ALIGNMENT_LEFT;
			}
			else if ((s[0] == 'r' || s[0] == 'R') && s[1] == '>') {
				*alignment = TP_MACRO_ALIGNMENT_RIGHT;
			}
			else if ((s[0] == 'c' || s[0] == 'C') && s[1] == '>') {
				*alignment = TP_MACRO_ALIGNMENT_CENTERED;
			}
			else {
				break;
			}
			++s;
		}

		// not valid string
		if (s[0] != '>') {
			*fixed_width = *alignment = 0;
			return false;
		}

		*new_s = s;
		return true;
	}

	return false;
}

//Parses %a-like expressions
char *TP_ParseMacroString (char *s)
{
	static char	buf[MAX_MACRO_STRING];
	int i = 0;
	int pN, pn;
	char *macro_string;

	int r = 0;

	player_state_t *state;
	player_info_t *info;
	static int lastframecount = -1;

	if (!cl_parseSay.value)
		return s;

	suppress = false;
	pn = pN = 0;

	while (*s && i < MAX_MACRO_STRING - 1) {
		qbool is_macro_indicator = (*s == '%');
		int fixed_width = 0;
		int alignment = 0;
		qbool explicit_format = false;

		// check for %<size[alignment]>
		explicit_format = is_macro_indicator && TP_ReadMacroFormat(s, &fixed_width, &alignment, &s);

		// check %[P], etc
		if (is_macro_indicator && s[1]=='[' && s[2] && s[3]==']') {
			static char mbuf[MAX_MACRO_VALUE];
			char cvar_lookup_char = s[2];

			switch (s[2]) {
				case 'a':
					macro_string = Macro_ArmorType();
					if (!strcmp(macro_string, tp_name_none.string))
						macro_string = "a";
					if (cl.stats[STAT_ARMOR] < 30)
						snprintf (mbuf, sizeof(mbuf), "\x10%s:%i\x11", macro_string, cl.stats[STAT_ARMOR]);
					else
						snprintf (mbuf, sizeof(mbuf), "%s:%i", macro_string, cl.stats[STAT_ARMOR]);
					macro_string = mbuf;
					break;

				case 'h':
					if (cl.stats[STAT_HEALTH] >= 50)
						snprintf (macro_buf, sizeof(macro_buf), "%i", cl.stats[STAT_HEALTH]);
					else
						snprintf (macro_buf, sizeof(macro_buf), "\x10%i\x11", cl.stats[STAT_HEALTH]);
					macro_string = macro_buf;
					break;

				case 'p':
				case 'P':
					macro_string = Macro_Powerups();
					if (strcmp(macro_string, tp_name_none.string))
						snprintf (mbuf, sizeof(mbuf), "\x10%s\x11", macro_string);
					else
						mbuf[0] = 0;
					macro_string = mbuf;
					break;

				default:
					buf[i++] = *s++;
					continue;
			}

			if (!explicit_format)
				TP_SetDefaultMacroCharFormat(true, cvar_lookup_char, &fixed_width, &alignment);

			if (fixed_width)
				macro_string = TP_AlignMacroText(macro_string, fixed_width, alignment);

			if (i + strlen(macro_string) >= MAX_MACRO_STRING - 1)
				Sys_Error("TP_ParseMacroString: macro string length > MAX_MACRO_STRING)");
			strlcpy (&buf[i], macro_string, MAX_MACRO_STRING - i);
			i += strlen(macro_string);
			s += 4;	// skip %[<char>]
			continue;
		}

		// check %a, etc
		if (is_macro_indicator) {
			char cvar_lookup_char = s[1];

			switch (s[1]) {
				//case '\x7f': macro_string = ""; break;// skip cause we use this to hide mesgs
				//case '\xff': macro_string = ""; break;
				case 'n':   pn = 1; macro_string = ""; break;
				case 'N':   pN = 1; macro_string = ""; break;
				case 'a':	macro_string = Macro_Armor(); break;
				case 'A':	macro_string = Macro_ArmorType(); break;
				case 'b':	macro_string = Macro_BestWeaponAndAmmo(); break;
				case 'c':	macro_string = Macro_Cells(); break;
				case 'd':	macro_string = Macro_LastDeath(); cvar_lookup_char = 'l'; break;
				case 'h':	macro_string = Macro_Health(); break;
				case 'i':	macro_string = Macro_TookAtLoc(); cvar_lookup_char = 'l'; break;
				case 'j':	macro_string = Macro_LastPointAtLoc(); cvar_lookup_char = 'l'; break;
				case 'k':	macro_string = Macro_LastTookOrPointed(); break;
				case 'l':	macro_string = Macro_Location(); cvar_lookup_char = 'l'; break;
				case 'L':	macro_string = Macro_Last_Location(); cvar_lookup_char = 'l'; break;
				case 'm':	macro_string = Macro_LastTookOrPointed(); break;

				case 'o':	macro_string = Macro_CountNearbyFriendlyPlayers(); break;
				case 'e':	macro_string = Macro_CountNearbyEnemyPlayers(); break;
				case 'O':	macro_string = Macro_Count_Last_NearbyFriendlyPlayers(); break;
				case 'E':	macro_string = Macro_Count_Last_NearbyEnemyPlayers(); break;

				case 'P':
				case 'p':	macro_string = Macro_Powerups(); cvar_lookup_char = 'p'; break;
				case 'q':	macro_string = Macro_LastSeenPowerup(); break;
				case 'r':	macro_string = Macro_LastReportedLoc(); cvar_lookup_char = 'l'; break;
				case 'R':	macro_string = Macro_Rune(); break;
				case 's':	macro_string = Macro_EnemyStatus_LED(); break;
				case 'S':	macro_string = Macro_TF_Skin(); break;
				case 't':	macro_string = Macro_PointNameAtLocation(); break;
				case 'u':	macro_string = Macro_Need(); break;
				case 'w':	macro_string = Macro_WeaponAndAmmo(); break;
				case 'x':	macro_string = Macro_PointName(); break;
				case 'X':	macro_string = Macro_Took(); cvar_lookup_char = 'x'; break;
				case 'y':	macro_string = Macro_PointLocation(); cvar_lookup_char = 'l'; break;
				case 'Y':	macro_string = Macro_TookLoc(); cvar_lookup_char = 'l'; break;
				case '%': 
					++s;	// deliberate fall-through, skip this % and print the next
				default:
					buf[i++] = '%';
					++s;
					continue;
			}

			if (!explicit_format)
				TP_SetDefaultMacroCharFormat(false, cvar_lookup_char, &fixed_width, &alignment);

			if (fixed_width)
				macro_string = TP_AlignMacroText(macro_string, fixed_width, alignment);

			if (i + strlen(macro_string) >= MAX_MACRO_STRING - 1)
				Sys_Error("TP_ParseMacroString: macro string length > MAX_MACRO_STRING)");
			strlcpy (&buf[i], macro_string, MAX_MACRO_STRING - i);
			i += strlen(macro_string);
			s += 2;	// skip % and letter
			continue;
		}
		buf[i++] = *s++;
	}
	buf[i] = 0;

	i = strlen(buf);

	if (pN) {
		buf[i++] = 0x7f;
		buf[i++] = '!';
		buf[i++] = 'A' + cl.playernum;
	}
	if (pn) {

		if (!pN)
			buf[i++] = 0x7f;

		if (cls.framecount != lastframecount) {

			lastframecount = cls.framecount;

			if (!(!cl.oldparsecount || !cl.parsecount || cls.state < ca_active)) {

				state = cl.frames[cl.oldparsecount & UPDATE_MASK].playerstate;
				info = cl.players;

				for (r = 0; r < MAX_CLIENTS; r++, info++, state++) {
					if (r != cl.playernum && state->messagenum == cl.oldparsecount && !info->spectator && !ISDEAD(state->frame)) {
						if (cl.teamplay && !strcmp(info->team, TP_PlayerTeam()))
							buf[i++] = 'A' + r;
					}
				}

			}

		}
	}

	if (suppress) {
		qbool quotes = false;

		TP_PrintHiddenMessage(buf,pN);

		i = strlen(buf);

		if (i > 0 && buf[i - 1] == '\"') {
			buf[i - 1] = 0;
			quotes = true;
			i--;
		}

		if (!pN) {
			if (!pn) {
				buf[i++] = 0x7f;
			}
			buf[i++] = '!';
			buf[i++] = 'A' + cl.playernum;
		}

		if (quotes)
			buf[i++] = '\"';
		buf[i] = 0;
	}


	return buf;
}

//Doesn't check for overflows, so strlen(s) should be < MAX_MACRO_STRING
char *TP_ParseFunChars (const char *s, qbool chat)
{
	static char	 buf[MAX_MACRO_STRING];
	char		*out = buf;
	int			 c;

	if (!cl_parseFunChars.value) {
		strlcpy(buf, s, sizeof(buf));
		return buf;
	}

	while (*s) {
		if (*s == '$' && s[1] == 'x') {
			int i;
			// check for $x10, $x8a, etc
			c = tolower((int)(unsigned char)s[2]);
			if ( isdigit(c) )
				i = (c - (int)'0') << 4;
			else if ( isxdigit(c) )
				i = (c - (int)'a' + 10) << 4;
			else goto skip;
			c = tolower((int)(unsigned char)s[3]);
			if ( isdigit(c) )
				i += (c - (int)'0');
			else if ( isxdigit(c) )
				i += (c - (int)'a' + 10);
			else goto skip;
			if (!i)
				i = (int)' ';
			*out++ = (char)i;
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
					case 'G': c = 0x86; break; // green led
					case 'R': c = 0x87; break; // red led
					case 'Y': c = 0x88; break; // yellow led
					case 'B': c = 0x89; break; // blue led
					case 'W': c = 0x84; break; // white led
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
			if ( isdigit((int)(unsigned char)s[1]) )
				c = s[1] - (int)'0' + 0x12;
			if (c) {
				*out++ = (char)c;
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
qbool TP_TeamLockSpecified(void)
{
	// 1 => return 1st team (doesn't matter which, just locks colours)
	// non-blank string => use that teamname as the client's team
	return cl.spectator && (cl_teamlock.integer != 0 || (cl_teamlock.string[0] && strcmp(cl_teamlock.string, "0")));
}

char *TP_SkinForcingTeam(void)
{
	int tracknum;

	// FIXME: teams with names 0 & 1 will clash with this - teamlock & the team name should be diff cvars
	if (!cl.spectator)
	{
		// Normal player.
		return cl.players[cl.playernum].team;
	}
	else if (cl_teamlock.string[0] == '1' && cl_teamlock.string[1] == '\0') {
		extern const char* HUD_FirstTeam(void);
		int i;

		if (cls.mvdplayback && HUD_FirstTeam()[0]) {
			return (char*)HUD_FirstTeam();
		}

		for (i = 0; i < MAX_CLIENTS; i++) {
			if (cl.players[i].name[0] && !cl.players[i].spectator && cl.players[i].team[0]) {
				return cl.players[i].team;
			}
		}
	}
	else if (!(cl_teamlock.string[0] == '0' && cl_teamlock.string[1] == '\0')) {
		// anything that isn't "0" to disable
		return cl_teamlock.string;
	}
	else if ((tracknum = Cam_TrackNum()) != -1)
	{
		// Spectating and tracking someone (not free flying).
		return cl.players[tracknum].team;
	}

	return "";
}

static qbool need_skin_refresh;
void TP_UpdateSkins(void)
{
	int slot;

	if (!need_skin_refresh)
	{
		return;
	}

	for (slot = 0; slot < MAX_CLIENTS; slot++)
	{
		if (cl.players[slot].skin_refresh)
		{
			CL_NewTranslation(slot);
			cl.players[slot].skin_refresh = false;
		}
	}

	need_skin_refresh = false;
}

// Returns true if a change in player/team needs skins to be reloaded
qbool TP_NeedRefreshSkins(void)
{
	extern cvar_t r_enemyskincolor, r_teamskincolor;

	if (cl.teamfortress) {
		return false;
	}

	if ((cl_enemyskin.string[0] || cl_teamskin.string[0] || cl_enemypentskin.string[0] || cl_teampentskin.string[0] ||
	     cl_enemyquadskin.string[0] || cl_teamquadskin.string[0] || cl_enemybothskin.string[0] || cl_teambothskin.string[0])
	     && !(cl.fpd & FPD_NO_FORCE_SKIN))
		return true;

	if ((cl_teamtopcolor.value >= 0 || cl_enemytopcolor.value >= 0) && !(cl.fpd & FPD_NO_FORCE_COLOR))
		return true;

	if ((r_enemyskincolor.string[0] || r_teamskincolor.string[0])) {
		return true;
	}

	return false;
}

void TP_RefreshSkin(int slot)
{
	if (cls.state < ca_connected || slot < 0 || slot >= MAX_CLIENTS || !cl.players[slot].name[0] || cl.players[slot].spectator)
		return;

	// Multiview
	// Never allow a skin refresh in multiview, since it
	// results in players getting the wrong color when
	// force colors is used (Team/enemycolor).
	// TODO: Any better solution for this?
	/*if(cls.mvdplayback && cl_multiview.value)
	{
		return;
	}*/

	cl.players[slot].skin_refresh = true;
	need_skin_refresh = true;
}

void TP_RefreshSkins(void)
{
	int i;

	for (i = 0; i < MAX_CLIENTS; i++)
		TP_RefreshSkin(i);
}

void OnChangeSkinAndColorForcing(cvar_t *var, char *string, qbool *cancel)
{
	OnChangeColorForcing(var, string, cancel);
	OnChangeSkinForcing(var, string, cancel);
	return;
}

void OnChangeColorForcing(cvar_t *var, char *string, qbool *cancel)
{
	TP_RefreshSkins();
	return;
}

void TP_ColorForcing (cvar_t *topcolor, cvar_t *bottomcolor)
{
	int	top, bottom;

	if (Cmd_Argc() == 1) {
		if (topcolor->integer == -1 && bottomcolor->integer == -1)
			Com_Printf ("\"%s\" is \"off\"\n", Cmd_Argv(0));
		else
			Com_Printf ("\"%s\" is \"%i %i\"\n", Cmd_Argv(0), topcolor->integer, bottomcolor->integer);
		return;
	}

	if (!strcasecmp(Cmd_Argv(1), "off") || !strcasecmp(Cmd_Argv(1), "")) {
		Cvar_SetValue(topcolor, -1);
		Cvar_SetValue(bottomcolor, -1);
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

	Cvar_SetValue(topcolor, top);
	Cvar_SetValue(bottomcolor, bottom);

	TP_RefreshSkins();
}

void TP_TeamColor_f(void)
{
	TP_ColorForcing(&cl_teamtopcolor, &cl_teambottomcolor);
}

void TP_EnemyColor_f(void)
{
	TP_ColorForcing(&cl_enemytopcolor, &cl_enemybottomcolor);
}

/************************* BASIC MATCH INFO FUNCTIONS *************************/

char *TP_PlayerName (void)
{
	static char myname[MAX_INFO_STRING];

	strlcpy (myname, Info_ValueForKey(cl.players[cl.playernum].userinfo, "name"), MAX_INFO_STRING);
	return myname;
}

char *TP_PlayerTeam (void)
{
	static char myteam[MAX_INFO_STRING];

	strlcpy (myteam, cl.players[cl.playernum].team, MAX_INFO_STRING);
	return myteam;
}

int	TP_CountPlayers (void)
{
	int	i, count = 0;

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator)
			count++;
	}
	return count;
}

// tells the playernum of the player in the current point of view
int TP_CurrentTrackNum(void)
{
	if (!cl.spectator) return cl.playernum;
	else return Cam_TrackNum();
}

// returns true if the player in the current POV is from given team
qbool TP_ThisPOV_IsHisTeam(const char* team)
{
	int pn = TP_CurrentTrackNum();
	
	if (pn < 0 || pn >= MAX_CLIENTS)
		return false;
	else
		return !strcmp(team, cl.players[pn].team);
}

// returns the team of the player in the current POV
// if no player tracked in the POV, returns NULL pointer
static char* TP_ThisPOV_Team(void) {
	int n = TP_CurrentTrackNum();
	if (n >= 0 && n < MAX_CLIENTS)
		return cl.players[n].team;
	else
		return NULL;
}

int TP_PlayersNumber(int userid, const char* team)
{
	qbool t1 = TP_ThisPOV_IsHisTeam(team); // is the looked up one our teammate?
	qbool t2;
	char *pt = TP_ThisPOV_Team();
	int pc = 0, i;
	player_info_t* cp;

	for (i = 0; i < MAX_CLIENTS; i++) {
		cp = &cl.players[i];
		if (!cp->name[0] || cp->spectator) continue;
		if (pt)
			t2 = !strcmp(cp->team, pt);	// is the current one our teammate?
		else
			t2 = false;

		if ((t1 && t2) || (!t1 && !t2)) {
			pc++;
		}
		if (cp->userid == userid)
			return pc;
	}

	return 0;
}

char *TP_MapName(void)
{
	return host_mapname.string;
}

char *MT_GetSkyGroupName(char *mapname, qbool *system);

char *TP_GetSkyGroupName(char *mapname, qbool *system)
{
	return MT_GetSkyGroupName(mapname, system);
}

char *MT_GetMapGroupName(char *mapname, qbool *system);

char *TP_GetMapGroupName(char *mapname, qbool *system)
{
	return MT_GetMapGroupName(mapname, system);
}

/****************************** PUBLIC FUNCTIONS ******************************/

void TP_NewMap (void)
{
	memset (&vars, 0, sizeof(vars));
	TP_FindModelNumbers ();

	TP_LocFiles_NewMap();
	TP_ExecTrigger ("f_newmap");
	if (cl.teamfortress) {
		V_TF_ClearGrenadeEffects();
	}
	Ignore_ResetFloodList();
}

int TP_CategorizeMessage (const char *s, int *offset)
{
	int i, msglen, len, flags, tracknum;
	player_info_t	*player;
	char *name, *team=NULL;

	tracknum = -1;
	if (cl.spectator && (tracknum = Cam_TrackNum()) != -1)
		team = cl.players[tracknum].team;
	else if (!cl.spectator)
		team = cl.players[cl.playernum].team;

	flags = msgtype_unknown;
	*offset = 0;
	if (!(msglen = strlen(s)))
		return msgtype_unknown;

	for (i = 0, player = cl.players; i < MAX_CLIENTS; i++, player++)	{
		if (!player->name[0])
			continue;
		name = Info_ValueForKey (player->userinfo, "name");
		len = strlen(name);
		len = min (len, 31);
		// check messagemode1
		if (len + 2 <= msglen && s[len] == ':' && s[len + 1] == ' ' && !strncmp(name, s, len))	{
			if (player->spectator)
				flags |= msgtype_spec;
			else
				flags |= msgtype_normal;
			*offset = len + 2;
		}
		// check messagemode2
		else if (s[0] == '(' && len + 4 <= msglen && !strncmp(s + len + 1, "): ", 3) && !strncmp(name, s + 1, len)

		         && (!cl.spectator || tracknum != -1)
		        ) {
			// no team messages in teamplay 0, except for our own
			if (i == cl.playernum || ( cl.teamplay && !strcmp(team, player->team)) )
				flags |= msgtype_team;
			*offset = len + 4;
		}
		//check spec mm2
		else if (cl.spectator && !strncmp(s, "[SPEC] ", 7) && player->spectator &&
		         len + 9 <= msglen && s[len + 7] == ':' && s[len + 8] == ' ' && !strncmp(name, s + 7, len)) {
			flags |= msgtype_specteam;
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
                   "teammate", "enemy", "eyes", "sentry", "disp", "quaded", "pented", \
				   "rune1", "rune2", "rune3", "rune4", "resistance", "strength", "haste", "regeneration"};

#define default_pkflags ((unsigned int) (it_powerups|it_armor|it_weapons|it_mh|it_pack| \
				it_rockets|it_cells|it_pack|it_flag|it_runes))

 // tp_took
#define default_tookflags ((unsigned int) (it_powerups|it_armor|it_weapons|it_pack| \
				it_rockets|it_cells|it_mh|it_flag|it_runes))

/*
powerups flag runes players suit armor sentry  mh disp rl lg pack gl sng rockets cells nails
Notice this list takes into account ctf/tf as well. Dm players don't worry about ctf/tf items.

 below are defaults for tp_point (what comes up in point. also see tp_pointpriorities to prioritize this list) First items have highest priority (powerups in this case)
*/
// tp_point
#define default_pointflags ((unsigned int) (it_powerups|it_players|it_armor|it_weapons|it_mh|it_pack| \
				it_rockets|it_cells|it_sentry|it_disp|it_flag|it_runes))

unsigned int pkflags = default_pkflags;
unsigned int tookflags = default_tookflags;
unsigned int pointflags = default_pointflags;
byte pointpriorities[NUM_ITEMFLAGS];

static void DumpFlagCommand(FILE *f, char *name, unsigned int flags, unsigned int default_flags)
{
	int i;
	unsigned int all_flags = UINT_MAX;

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
	if ((flags & it_runes) == it_runes) {
		fprintf(f, "runes ");
		flags &= ~it_runes;
	}
	for (i = 0; i < NUM_ITEMFLAGS; i++) {
		if (flags & (1 << i))
			fprintf (f, "%s ", pknames[i]);
	}
	fprintf(f, "\n");
}

void DumpFlagCommands(FILE *f)
{
	DumpFlagCommand(f, "tp_pickup   ", pkflags, default_pkflags);
	DumpFlagCommand(f, "tp_took     ", tookflags, default_tookflags);
	DumpFlagCommand(f, "tp_point    ", pointflags, default_pointflags);
}

static void FlagCommand(unsigned int* flags, unsigned int defaultflags)
{
	int i, j, c, offset = 0;
	unsigned int flag;
	char* p, str[255] = { 0 };
	qbool removeflag = false;

	c = Cmd_Argc();
	if (c == 1) {
		qbool notfirst = false;
		if (!*flags)
			strlcpy(str, "none", sizeof(str));

		if (tp_pointpriorities.integer) {
			int p;
			for (p = 0; p < NUM_ITEMFLAGS; ++p) {
				for (i = 0; i < NUM_ITEMFLAGS; i++) {
					if (pointpriorities[i] == p && (*flags & (1 << i))) {
						if (notfirst) {
							Com_Printf(" ");
						}

						notfirst = true;
						Com_Printf("%s", pknames[i]);
					}
				}
			}
		}
		else {
			for (i = 0; i < NUM_ITEMFLAGS; i++) {
				if (*flags & (1 << i)) {
					if (notfirst) {
						Com_Printf(" ");
					}

					notfirst = true;
					Com_Printf("%s", pknames[i]);
				}
			}
		}
		Com_Printf("\n");
		return;
	}

	if (c == 2 && !strcasecmp(Cmd_Argv(1), "none")) {
		*flags = 0;
		memset(pointpriorities, 0, sizeof(pointpriorities));
		return;
	}

	if (*Cmd_Argv(1) != '+' && *Cmd_Argv(1) != '-') {
		*flags = 0;
		memset(pointpriorities, 0, sizeof(pointpriorities));
	}
	else if (*Cmd_Argv(1) == '+') {
		for (i = 0; i < NUM_ITEMFLAGS; ++i) {
			if (*flags & (1 << i)) {
				++offset;
			}
		}
	}

	for (i = 1; i < c; i++) {
		p = Cmd_Argv(i);
		if (*p == '+') {
			removeflag = false;
			p++;
		}
		else if (*p == '-') {
			removeflag = true;
			p++;
		}

		flag = 0;
		for (j = 0; j < NUM_ITEMFLAGS; j++) {
			if (!strcasecmp(p, pknames[j])) {
				flag = 1 << j;
				break;
			}
		}

		if (!flag) {
			if (!strcasecmp(p, "armor"))
				flag = it_armor;
			else if (!strcasecmp(p, "weapons"))
				flag = it_weapons;
			else if (!strcasecmp(p, "powerups"))
				flag = it_powerups;
			else if (!strcasecmp(p, "ammo"))
				flag = it_ammo;
			else if (!strcasecmp(p, "players"))
				flag = it_players;
			else if (!strcasecmp(p, "default"))
				flag = defaultflags;
			else if (!strcasecmp(p, "runes"))
				flag = it_runes;
			else if (!strcasecmp(p, "all"))
				flag = UINT_MAX; //(1 << NUM_ITEMFLAGS); //-1;
		}

		if (flags != &pointflags) {
			flag &= ~(it_sentry | it_disp | it_players);
		}

		if (removeflag) {
			*flags &= ~flag;

			for (j = 1; j <= NUM_ITEMFLAGS; j++) {
				if (flag & (1 << (j - 1))) {
					pointpriorities[j - 1] = 0;
				}
			}
		}
		else if (flag) {
			*flags |= flag;

			for (j = 1; j <= NUM_ITEMFLAGS; j++) {
				if (flag & (1 << (j - 1))) {
					pointpriorities[j - 1] = i + offset;
				}
			}
		}
	}
}

void TP_Took_f (void)
{
	FlagCommand (&tookflags, default_tookflags);
}

void TP_Pickup_f (void)
{
	FlagCommand (&pkflags, default_pkflags);
}

void TP_Point_f (void)
{
	FlagCommand (&pointflags, default_pointflags);
}

typedef struct
{
	unsigned int		itemflag;
	cvar_t	*cvar;
	char	*modelname;
	vec3_t	offset;		// offset of model graphics center
	float	radius;		// model graphics radius
	unsigned int		flags;		// TODO: "NOPICKUP" (disp), "TEAMENEMY" (flag, disp)
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
						{	it_flag,	&tp_name_flag,	"progs/stag.mdl",
						  {0, 0, 20},	18,
						},
						{	it_flag,	&tp_name_flag,	"progs/basrkey.bsp",
						  {0, 0, 20},	18,
						},
						{	it_flag,	&tp_name_flag,	"progs/basbkey.bsp",
						  {0, 0, 20},	18,
						},
						{	it_flag,	&tp_name_flag,	"progs/ff_flag.mdl",
						  {0, 0, 20},	18,
						},
						{	it_flag,	&tp_name_flag,	"progs/harbflag.mdl",
						  {0, 0, 20},	18,
						},
						{	it_flag,	&tp_name_flag,	"progs/princess.mdl",
						  {0, 0, 20},	18,
						},
						{	it_flag,	&tp_name_flag,	"progs/flag.mdl",
                          {0, 0, 14},	25,
                        },
                        {	it_rune1,	&tp_name_rune1,	"progs/end1.mdl",
                          {0, 0, 20},	18,
                        },
                        {	it_rune2,	&tp_name_rune2,	"progs/end2.mdl",
                          {0, 0, 20},	18,
                        },
                        {	it_rune3,	&tp_name_rune3,	"progs/end3.mdl",
                          {0, 0, 20},	18,
                        },
                        {	(unsigned int) it_rune4,	&tp_name_rune4,	"progs/end4.mdl",
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

void TP_FindModelNumbers (void)
{
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
static int FindNearestItem (int flags, item_t **pitem)
{
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
		strlcpy(vars.nearestitemloc, TP_LocationName(bestent->origin), sizeof(vars.nearestitemloc));
	else
		vars.nearestitemloc[0] = 0;

	if (bestent && (*pitem)->itemflag == it_armor)
		return bestent->skinnum + 1;	// 1 = green, 2 = yellow, 3 = red

	return bestent ? bestent->modelindex : 0;
}

char *Macro_LastTookOrPointed (void)
{
	if (vars.tooktime && vars.tooktime > vars.pointtime && cls.realtime - vars.tooktime < 5)
		return Macro_TookAtLoc();
	else if (vars.pointtime && vars.tooktime <= vars.pointtime && cls.realtime - vars.pointtime < 5)
		return Macro_LastPointAtLoc();

	snprintf(macro_buf, sizeof(macro_buf), "%s %s %s", tp_name_nothing.string, tp_name_at.string, tp_name_someplace.string);
	return macro_buf;
}

static qbool CheckTrigger (void)
{
	int	i, count;
	player_info_t *player;
	char *myteam;
	extern cvar_t tp_forceTriggers;

	if (cl.spectator || Rulesets_RestrictTriggers())
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

static void ExecTookTrigger (char *s, int flag, vec3_t org)
{
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
    vars.tookflag = flag;
	strlcpy (vars.tookname, s, sizeof (vars.tookname));
	strlcpy (vars.tookloc, TP_LocationName(org), sizeof(vars.tookloc));

	if ((tookflags_dmm & flag) && CheckTrigger())
		TP_ExecTrigger ("f_took");
}

void TP_ParsePlayerInfo(player_state_t *oldstate, player_state_t *state, player_info_t *info)
{
	if (TP_NeedRefreshSkins()) {
		if ((state->effects & (EF_BLUE|EF_RED) ) != (oldstate->effects & (EF_BLUE|EF_RED)))
			TP_RefreshSkin(info - cl.players);
	}

	if (!cl.spectator && cl.teamplay && strcmp(info->team, TP_PlayerTeam())) {
		qbool eyes;

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
			strlcpy (vars.lastdroploc, Macro_Location(), sizeof (vars.lastdroploc));
		}
	}
}

static qbool TP_DetectWeaponPickup(void)
{
	if (vars.items & ~vars.olditems & IT_LIGHTNING)
		ExecTookTrigger(tp_name_lg.string, it_lg, cl.simorg);
	else if (vars.items & ~vars.olditems & IT_ROCKET_LAUNCHER)
		ExecTookTrigger(tp_name_rl.string, it_rl, cl.simorg);
	else if (vars.items & ~vars.olditems & IT_GRENADE_LAUNCHER)
		ExecTookTrigger(tp_name_gl.string, it_gl, cl.simorg);
	else if (vars.items & ~vars.olditems & IT_SUPER_NAILGUN)
		ExecTookTrigger(tp_name_sng.string, it_sng, cl.simorg);
	else if (vars.items & ~vars.olditems & IT_NAILGUN)
		ExecTookTrigger(tp_name_ng.string, it_ng, cl.simorg);
	else if (vars.items & ~vars.olditems & IT_SUPER_SHOTGUN)
		ExecTookTrigger(tp_name_ssg.string, it_ssg, cl.simorg);
	else
		return false;
	return true;
}

void TP_CheckPickupSound (char *s, vec3_t org)
{
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
			if (! TP_DetectWeaponPickup())
				cl.last_weapon_pickup = cls.framecount;
		} else {
			cl.last_weapon_pickup = cls.framecount;
		}
		return;
	}

	// armor
	if (!strcmp(s, "items/armor1.wav"))	{
		qbool armor_updated;
		int armortype;

		armor_updated = (vars.stat_framecounts[STAT_ARMOR] == cls.framecount);
		armortype = FindNearestItem (it_armor, &item);
		if (armortype == 1 || (!armortype && armor_updated && cl.stats[STAT_ARMOR] == 100))
			ExecTookTrigger (tp_name_ga.string, it_ga, org);
		else if (armortype == 2 || (!armortype && armor_updated && cl.stats[STAT_ARMOR] == 150))
			ExecTookTrigger (tp_name_ya.string, it_ya, org);
		else if (armortype == 3 || (!armortype && armor_updated && cl.stats[STAT_ARMOR] == 200))
			ExecTookTrigger (tp_name_ra.string, it_ra, org);
		else 
			cl.last_armor_pickup = cls.framecount;
		return;
	}

	// backpack, ammo or runes
	if (!strcmp (s, "weapons/lock4.wav")) {
		if (FindNearestItem(it_ammo | it_pack | it_runes, &item))
			ExecTookTrigger(item->cvar->string, item->itemflag, org);
		else
			cl.last_ammo_pickup = cls.framecount;
		return;
	}
}

qbool TP_IsItemVisible(item_vis_t *visitem)
{
	vec3_t end, v;
	trace_t trace;

	if (visitem->dist <= visitem->radius)
		return true;

	VectorNegate (visitem->dir, v);
	VectorNormalizeFast (v);
	VectorMA (visitem->entorg, visitem->radius, v, end);
	trace = PM_TraceLine (visitem->vieworg, end);
	if ((int)trace.fraction == 1)
		return true;

	VectorMA (visitem->entorg, visitem->radius, visitem->right, end);
	VectorSubtract (visitem->vieworg, end, v);
	VectorNormalizeFast (v);
	VectorMA (end, visitem->radius, v, end);
	trace = PM_TraceLine (visitem->vieworg, end);
	if ((int)trace.fraction == 1)
		return true;

	VectorMA(visitem->entorg, -visitem->radius, visitem->right, end);
	VectorSubtract(visitem->vieworg, end, v);
	VectorNormalizeFast(v);
	VectorMA(end, visitem->radius, v, end);
	trace = PM_TraceLine(visitem->vieworg, end);
	if ((int)trace.fraction == 1)
		return true;

	VectorMA(visitem->entorg, visitem->radius, visitem->up, end);
	VectorSubtract(visitem->vieworg, end, v);
	VectorNormalizeFast(v);
	VectorMA (end, visitem->radius, v, end);
	trace = PM_TraceLine(visitem->vieworg, end);
	if ((int)trace.fraction == 1)
		return true;

	// use half the radius, otherwise it's possible to see through floor in some places
	VectorMA(visitem->entorg, -visitem->radius / 2, visitem->up, end);
	VectorSubtract(visitem->vieworg, end, v);
	VectorNormalizeFast(v);
	VectorMA(end, visitem->radius, v, end);
	trace = PM_TraceLine(visitem->vieworg, end);
	if ((int)trace.fraction == 1)
		return true;

	return false;
}

static float TP_RankPoint(item_vis_t *visitem)
{
	vec3_t v2, v3;
	float miss;

	if (visitem->dist < 10)
		return -1;

	VectorScale (visitem->forward, visitem->dist, v2);
	VectorSubtract (v2, visitem->dir, v3);
	miss = VectorLength (v3);
	if (miss > 300)
		return -1;
	if (miss > visitem->dist * (tp_pointpriorities.value ? 0.55 : 1.7)) // for prioritized point
		return -1;		// over 60 degrees off

	if (tp_pointpriorities.value)
		return 1;
	if (visitem->dist < 3000.0 / 8.0)
		return miss * (visitem->dist * 8.0 * 0.0002f + 0.3f);
	else return miss;
}

void TP_FindPoint (void)
{
	packet_entities_t *pak;
	entity_state_t *ent;
	int	i, j, tempflags;
	unsigned int pointflags_dmm;
	float best = -1, rank;
	entity_state_t *bestent = NULL;
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
					case 0: if (!(pointflags_dmm & it_ga)) continue; break;
					case 1: if (!(pointflags_dmm & it_ya)) continue; break;
					case 2: if (!(pointflags_dmm & it_ra)) continue; break;
			}
		}

		VectorAdd (ent->origin, item->offset, visitem.entorg);
		VectorSubtract (visitem.entorg, visitem.vieworg, visitem.dir);
		visitem.dist = DotProduct (visitem.dir, visitem.forward);
		visitem.radius = ent->effects & (EF_BLUE|EF_RED|EF_DIMLIGHT|EF_BRIGHTLIGHT) ? 200 : item->radius;

		if ((rank = TP_RankPoint(&visitem)) < 0)
			continue;

		if (tp_pointpriorities.value && rank != -1) {
			tempflags = item->itemflag;
			for (j = 1; j < NUM_ITEMFLAGS; j++)
				if (!(tempflags & 1))
					tempflags >>= 1;
				else
					break;

			/* FIXME: Added to prevent potential array out of bounds.
			 * Look into if its practically possible ... Anyway it wont
			 * crash now in case the loop above runs out
			 */
			if (j == NUM_ITEMFLAGS) {
				j--;
			}
			rank = pointpriorities[j];
		}

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
		    ( state->modelindex == cl_modelindices[mi_player] && ISDEAD(state->frame) ) ||
		    ( state->modelindex == cl_modelindices[mi_h_player] )
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
			qbool teammate, eyes = false;

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
		qbool teammate, eyes;
		char *name, buf[256] = {0};
        int flag = 0;

		eyes = beststate->modelindex && cl.model_precache[beststate->modelindex] &&
		       cl.model_precache[beststate->modelindex]->modhint == MOD_EYES;
		if (cl.teamfortress) {
			teammate = !strcmp(Utils_TF_ColorToTeam(bestinfo->real_bottomcolor), TP_PlayerTeam());

			if (eyes)
            {
				name = tp_name_eyes.string;		//duck on 2night2.bsp (TF map)
                flag = it_eyes;
            }
			else if (cl.spectator)
            {
				name = bestinfo->name;
                flag = it_players;
            }
			else if (teammate)
            {
				name = tp_name_teammate.string[0] ? tp_name_teammate.string : "teammate";
                flag = it_teammate;
            }
			else
            {
				name = tp_name_enemy.string;
                flag = it_enemy;
            }

			if (!eyes)
				name = va("%s%s%s", name, name[0] ? " " : "", Skin_To_TFSkin(Info_ValueForKey(bestinfo->userinfo, "skin")));
		} else {
			teammate = (cl.teamplay && !strcmp(bestinfo->team, TP_PlayerTeam()));

			if (eyes)
            {
				name = tp_name_eyes.string;
                flag = it_eyes;
            }
			else if (cl.spectator || (teammate && !tp_name_teammate.string[0]))
            {
				name = bestinfo->name;
                flag = it_teammate;
            }
			else
            {
				name = teammate ? tp_name_teammate.string : tp_name_enemy.string;
                flag = teammate ? it_teammate : it_enemy;
            }
		}
		if (beststate->effects & EF_BLUE)
        {
			strlcat (buf, tp_name_quaded.string, sizeof (buf) - strlen (buf));
            flag |= it_quaded;
        }
		if (beststate->effects & EF_RED)
        {
			strlcat (buf, va("%s%s", buf[0] ? " " : "", tp_name_pented.string), sizeof (buf) - strlen (buf));
            flag |= it_pented;
        }
		strlcat (buf, va("%s%s", buf[0] ? " " : "", name), sizeof (buf) - strlen (buf));
		strlcpy (vars.pointname, buf, sizeof (vars.pointname));
        vars.pointflag = flag;
		strlcpy (vars.pointloc, TP_LocationName(beststate->origin), sizeof(vars.pointloc));
		
		vars.pointtype = (teammate && !eyes) ? POINT_TYPE_TEAMMATE : POINT_TYPE_ENEMY;
	} else if (best >= 0) {
		char *p;

        vars.pointflag = bestitem->itemflag;

        if (!bestitem->cvar) {
			// armors are special
			switch (bestent->skinnum) {
					case 0: p = tp_name_ga.string; vars.pointflag = it_ga; break;
                    case 1: p = tp_name_ya.string; vars.pointflag = it_ya; break;
					default: p = tp_name_ra.string; vars.pointflag = it_ra;
			}
		} else {
			p = bestitem->cvar->string;
		}

		vars.pointtype = (bestitem->itemflag & (it_powerups|it_flag)) ? POINT_TYPE_POWERUP : POINT_TYPE_ITEM;
		strlcpy (vars.pointname, p, sizeof(vars.pointname));
		strlcpy (vars.pointloc, TP_LocationName(bestent->origin), sizeof(vars.pointloc));
	} else {
	nothing:
		strlcpy (vars.pointname, tp_name_nothing.string, sizeof(vars.pointname));
		vars.pointloc[0] = 0;
		vars.pointtype = POINT_TYPE_ITEM;
        vars.pointflag = 0;
	}
	vars.pointtime = cls.realtime;
}

void TP_ParseWeaponModel(model_t *model)
{
	static model_t *last_model = NULL;

	if (cl.teamfortress && (!cl.spectator || Cam_TrackNum() != -1)) {
		if (model && !last_model)
			TP_ExecTrigger ("f_reloadend");
		else if (!model && last_model)
			TP_ExecTrigger ("f_reloadstart");
	}
	last_model = model;
}

void TP_StatChanged (int stat, int value)
{
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
				strlcpy (vars.lastdeathloc, Macro_Location(), sizeof (vars.lastdeathloc));

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
				strlcpy (vars.lastdroploc, Macro_Location(), sizeof (vars.lastdroploc));
			}

			vars.olditems = vars.items;
			vars.items = value;

			// If we have received a sound previously, update 
			if (cl.last_weapon_pickup == cls.framecount)
				TP_DetectWeaponPickup();
			else if (cl.last_ammo_pickup == cls.framecount)
				ExecTookTrigger(tp_name_backpack.string, it_pack, cl.simorg);
			cl.last_weapon_pickup = 0;

			break;
		case STAT_ARMOR:
			if (cl.last_armor_pickup == cls.framecount)
			{
				if (value == 100)
					ExecTookTrigger(tp_name_ga.string, it_ga, cl.simorg);
				else if (value == 150)
					ExecTookTrigger(tp_name_ya.string, it_ya, cl.simorg);
				else if (value == 200)
					ExecTookTrigger(tp_name_ra.string, it_ra, cl.simorg);

				cl.last_armor_pickup = 0;
			}
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

/****************************** MESSAGE FILTERS ******************************/

#ifdef _WIN32
#define wcscasecmp(s1, s2)	_wcsicmp  ((s1),   (s2))
#endif

#define MAX_FILTER_LENGTH 4
char filter_strings[8][MAX_FILTER_LENGTH + 1];
int	num_filters = 0;

//returns false if the message shouldn't be printed.
//Matching filters are stripped from the message
qbool TP_FilterMessage (wchar *source)
{
	size_t i, j, maxlen, len;

	if (!num_filters)
		return true;

	len = qwcslen (source);
	if (len < 2 || source[len - 1] != '\n' || source[len - 2] == '#')
		return true;

	maxlen = MAX_FILTER_LENGTH + 1;
	for (i = len - 2 ; i != 0 && maxlen != 0 ; i--, maxlen--) {
		if (source[i] == ' ')
			return true;
		if (source[i] == '#')
			break;
	}
	if (!i || !maxlen)
		return true; // no filter at all

	source[len - 1] = 0; // so that strcmp works properly

	for (j = 0; j < num_filters; j++)
#ifdef _WIN32
		if (!wcscasecmp(source + i + 1, str2wcs(filter_strings[j]))) {
#else
		if (!strcasecmp(wcs2str(source + i + 1), filter_strings[j])) {
#endif
			// strip the filter from message
			if (i && source[i - 1] == ' ')	{
				// there's a space just before the filter, remove it
				// so that soundtriggers like ^blah #att work
				source[i - 1] = '\n';
				source[i] = 0;
			} else {
				source[i] = '\n';
				source[i + 1] = 0;
			}
			return true;
		}

	source[len - 1] = '\n';
	return false; // this message is not for us, don't print it
}

void TP_MsgFilter_f (void)
{
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

	if (c == 2 && (Cmd_Argv(1)[0] == 0 || !strcasecmp(Cmd_Argv(1), "clear"))) {
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
		strlcpy (filter_strings[num_filters], s + 1, sizeof(filter_strings[0]));
		num_filters++;
		if (num_filters >= 8)
			break;
	}
}

void TP_DumpMsgFilters(FILE *f)
{
	int i;

	fprintf(f, "filter       ");
	if (!num_filters)
		fprintf(f, "clear");

	for (i = 0; i < num_filters; i++)
		fprintf (f, "#%s ", filter_strings[i]);

	fprintf(f, "\n");
}

/************************************ INIT ************************************/
extern void TP_InitTriggers (void);
void TP_Init (void)
{
	TP_InitTriggers();
	TP_AddMacros();

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register (&cl_parseFunChars);
	Cvar_Register (&cl_parseSay);
	Cvar_Register (&cl_nofake);

	Cvar_SetCurrentGroup(CVAR_GROUP_SKIN);
	Cvar_Register (&cl_teamtopcolor);
	Cvar_Register (&cl_teambottomcolor);
	Cvar_Register (&cl_enemytopcolor);
	Cvar_Register (&cl_enemybottomcolor);
	Cvar_Register (&cl_enemybothskin);
	Cvar_Register (&cl_teambothskin);
	Cvar_Register (&cl_enemypentskin);
	Cvar_Register (&cl_teampentskin);
	Cvar_Register (&cl_enemyquadskin);
	Cvar_Register (&cl_teamquadskin);
	Cvar_Register (&cl_enemyskin);
	Cvar_Register (&cl_teamskin);
	Cvar_Register (&cl_teamlock);

	Cvar_SetCurrentGroup(CVAR_GROUP_COMMUNICATION);
	Cvar_Register(&tp_loadlocs);
	Cvar_Register(&tp_pointpriorities);
	Cvar_Register(&tp_weapon_order);
	Cvar_Register(&tp_tooktimeout);
	Cvar_Register(&tp_pointtimeout);
	Cvar_Register(&tp_poweruptextstyle);

	Cvar_SetCurrentGroup(CVAR_GROUP_ITEM_NAMES);
	Cvar_Register (&tp_name_separator);
	Cvar_Register (&tp_name_none);
	Cvar_Register (&tp_name_nothing);
	Cvar_Register (&tp_name_at);
	Cvar_Register (&tp_name_someplace);

	Cvar_Register (&tp_name_rune1);
	Cvar_Register (&tp_name_rune2);
	Cvar_Register (&tp_name_rune3);
	Cvar_Register (&tp_name_rune4);

	Cvar_Register (&tp_name_status_blue);
	Cvar_Register (&tp_name_status_red);
	Cvar_Register (&tp_name_status_yellow);
	Cvar_Register (&tp_name_status_green);
	Cvar_Register (&tp_name_status_white);

	Cvar_Register (&tp_name_pented);
	Cvar_Register (&tp_name_quaded);
	Cvar_Register (&tp_name_eyes);
	Cvar_Register (&tp_name_enemy);
	Cvar_Register (&tp_name_teammate);
	Cvar_Register (&tp_name_disp);
	Cvar_Register (&tp_name_sentry);
	Cvar_Register (&tp_name_filter);
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
	Cvar_Register (&tp_name_rlg);
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

	TP_LocFiles_Init();

	Cvar_ResetCurrentGroup();
	Cmd_AddCommand ("teamcolor", TP_TeamColor_f);
	Cmd_AddCommand ("enemycolor", TP_EnemyColor_f);

	Cmd_AddCommand ("tp_msgreport", TP_Msg_Report_f);
	Cmd_AddCommand ("tp_msgcoming", TP_Msg_Coming_f);
    Cmd_AddCommand ("tp_msglost", TP_Msg_Lost_f);
    Cmd_AddCommand ("tp_msgenemypwr", TP_Msg_EnemyPowerup_f);
	Cmd_AddCommand ("tp_msgquaddead", TP_Msg_QuadDead_f);
    Cmd_AddCommand ("tp_msgsafe", TP_Msg_Safe_f);
	Cmd_AddCommand ("tp_msgkillme", TP_Msg_KillMe_f);
    Cmd_AddCommand ("tp_msghelp", TP_Msg_Help_f);
	Cmd_AddCommand ("tp_msggetquad", TP_Msg_GetQuad_f);
	Cmd_AddCommand ("tp_msggetpent", TP_Msg_GetPent_f);
	Cmd_AddCommand ("tp_msgpoint", TP_Msg_Point_f);
	Cmd_AddCommand ("tp_msgtook", TP_Msg_Took_f);
	Cmd_AddCommand ("tp_msgtrick", TP_Msg_Trick_f);
	Cmd_AddCommand ("tp_msgreplace", TP_Msg_Replace_f);
	Cmd_AddCommand ("tp_msgneed", TP_Msg_Need_f);
	Cmd_AddCommand ("tp_msgyesok", TP_Msg_YesOk_f);
	Cmd_AddCommand ("tp_msgnocancel", TP_Msg_NoCancel_f);
	Cmd_AddCommand ("tp_msgutake", TP_Msg_YouTake_f);
	Cmd_AddCommand ("tp_msgitemsoon", TP_Msg_ItemSoon_f);
	Cmd_AddCommand ("tp_msgwaiting", TP_Msg_Waiting_f);
	Cmd_AddCommand ("tp_msgslipped", TP_Msg_Slipped_f);
	//TF messages
	Cmd_AddCommand ("tp_msgtfconced", TP_Msg_TFConced_f);
}

extern void TP_ShutdownTriggers(void);

void TP_Shutdown(void)
{
	TP_ShutdownTriggers();

	TP_LocFiles_Shutdown();
}
