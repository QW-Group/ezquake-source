/*
Copyright (C) 2011 ezQuake team

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
/**

  Inbuilt teamplay messages module

  made by johnnycz, Up2nOgoOd[ROCK]
  last edit:
  $Id: tp_msgs.c,v 1.6 2007-10-23 08:51:02 himan Exp $

*/

#include "quakedef.h"
#include "teamplay.h"

#define GLOBAL /* */
#define LOCAL static

#define NEED_WEAPON() (NEED(rl) || NEED(lg))
#define HAVE_FLAG() (cl.stats[STAT_ITEMS] & IT_FLAG)
#define HAVE_RL() (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
#define HAVE_LG() (cl.stats[STAT_ITEMS] & IT_LIGHTNING)
#define HAVE_GL() (cl.stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER)
#define HAVE_SNG() (cl.stats[STAT_ITEMS] & IT_SUPER_NAILGUN)
#define HAVE_SSG() (cl.stats[STAT_ITEMS] & IT_SUPER_SHOTGUN)
#define HOLD_GL() (cl.stats[STAT_ACTIVEWEAPON] == IT_GRENADE_LAUNCHER)
#define HOLD_RL() (cl.stats[STAT_ACTIVEWEAPON] == IT_ROCKET_LAUNCHER)
#define HOLD_LG() (cl.stats[STAT_ACTIVEWEAPON] == IT_LIGHTNING)
 
#define HAVE_POWERUP() (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
#define HAVE_QUAD() (cl.stats[STAT_ITEMS] & IT_QUAD) //quad
#define HAVE_PENT() (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) //pent
#define HAVE_RING() (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) //ring
#define HAVE_GA() (cl.stats[STAT_ITEMS] & IT_ARMOR1) //ga
#define HAVE_YA() (cl.stats[STAT_ITEMS] & IT_ARMOR2) //ya
#define HAVE_RA() (cl.stats[STAT_ITEMS] & IT_ARMOR3) //ra

#define HAVE_CELLS() (cl.stats[STAT_CELLS] > 0)
#define HAVE_ROCKETS() (cl.stats[STAT_ROCKETS] > 0)

// just some shortcuts:
#define INPOINTARMOR() (INPOINT(ra) || INPOINT(ya) || INPOINT(ga))
#define INPOINTWEAPON() (INPOINT(rl) || INPOINT(lg) || INPOINT(gl) || INPOINT(sng))
#define INPOINTPOWERUP() (INPOINT(quad) || INPOINT(pent) || INPOINT(ring))
#define INPOINTAMMO() (INPOINT(rockets) || INPOINT(cells) || INPOINT(nails))
 
#define TOOK(x) (!TOOK_EMPTY() && vars.tookflag == it_##x)
#define COLORED(c,str) "{&c" #c #str "&cfff}"

// it's important to call TP_FindPoint() before using these
// don't call TP_FindPoint if (flashed) is true
#define POINT_EMPTY() (!vars.pointflag)
#define INPOINT(thing) (!flashed && (vars.pointflag & it_##thing))

#define DEAD() (cl.stats[STAT_HEALTH] < 1)

// call TP_GetNeed() before using this!
#define NEED(x) (vars.needflags & it_##x)

typedef const char * MSGPART;
 
extern cvar_t cl_fakename;
// will return short version of player's nick for teamplay messages
LOCAL char *TP_ShortNick(void)
{
	static char buf[64];
 
	if (*(cl_fakename.string)) return "";
	else {
		if (*(Cvar_String("nick"))) { // isn't cl_fakename and name enough?
			snprintf(buf, sizeof(buf), "$\\%s%s", Cvar_String("nick"), Cvar_String("cl_fakename_suffix"));
		} else {
			snprintf(buf, sizeof(buf), "$\\%.3s%s", TP_PlayerName(), Cvar_String("cl_fakename_suffix"));
		}
		return buf;
	}
}      
 
// wrapper for snprintf & Cbuf_AddText that will add say_team nick part
LOCAL void TP_Send_TeamSay(char *format, ...)
{
    char tp_msg_head[256], tp_msg_body[256], tp_msg[512];
    va_list argptr;
 
    snprintf(tp_msg_head, sizeof(tp_msg_head), "say_team %s", TP_ShortNick());
 
	va_start (argptr, format);
    vsnprintf(tp_msg_body, sizeof(tp_msg_body), format, argptr);
	va_end (argptr);
 
    snprintf(tp_msg, sizeof(tp_msg), "%s%s\n", tp_msg_head, tp_msg_body);
 
    Cbuf_AddText(tp_msg);
}
 
 
 
///////////////////////////////////
////// Start teamplay scripts /////
///////////////////////////////////

GLOBAL void TP_Msg_Lost_f (void)
{
    MSGPART quad = "";
	MSGPART over = "";
	MSGPART dropped_or_lost = "";
	MSGPART location_enemy = " {&cf00[&cfff}{%d}{&cf00]&cfff} {%E}";
	extern cvar_t tp_name_quad;

	if (DEAD()) {
		if (HAVE_QUAD()) {
			quad = tp_name_quad.string;			
			over = " over ";
			location_enemy = "{&cf00[&cfff}{%d}{&cf00]&cfff} {%E}";
		}
		else
			dropped_or_lost = "{&cf00lost&cfff}";

		if (HOLD_RL() || HOLD_LG()) {
			dropped_or_lost = "{&cf00DROPPED} $weapon";
			location_enemy = " {&cf00[&cfff}{%d}{&cf00]&cfff} {%E}";
		}
	}
	else
		dropped_or_lost = "{&cf00lost&cfff}";

	TP_Send_TeamSay("%s%s%s%s", quad, over, dropped_or_lost, location_enemy);
}

GLOBAL void TP_Msg_Report_f (void)
{
	extern cvar_t tp_name_lg, tp_name_rl, tp_name_gl, tp_name_sng, tp_name_ssg;
	MSGPART powerup = "";
	MSGPART armor_health = "$colored_armor/%h";
	MSGPART location = "$[{%l}$]";
	MSGPART weapon = "";
	MSGPART rl = ""; // note we need by "rl" "lg" and "weapon" for the case that player has both
	MSGPART lg = "";
	MSGPART cells = "";
	MSGPART extra_cells = ""; //"extra" MSGPART needed to we can put these after %l
	MSGPART rockets = "";
	MSGPART extra_rockets = "";
	MSGPART ammo = "";
 
	if (DEAD()) {
		TP_Msg_Lost_f();
		return;
	}
	
	if (HAVE_POWERUP())
		powerup = "$tp_powerups ";
 
	if	(HAVE_RL() && HAVE_LG()) {
		rl = tp_name_rl.string;
		rockets = ":$rockets ";
		lg = tp_name_lg.string;
		cells = ":$cells ";
	}
	else if (HAVE_RL()) {
		rl = tp_name_rl.string;
		rockets = ":$rockets ";
	}
	else if (HAVE_LG()) {
		lg = tp_name_lg.string;
		cells = ":$cells ";
	}
	else if (HAVE_GL()) {
		weapon = tp_name_gl.string;
		ammo = ":$rockets ";
	}
	else if (HAVE_SNG()) {
		weapon = tp_name_sng.string;
		ammo = ":$nails ";
	}
	else if (HAVE_SSG()) {
		weapon = tp_name_ssg.string;
		ammo = ":$shells ";
	}
	
	// extra rockets and cells
	if (!HAVE_RL() && HAVE_ROCKETS())
		extra_rockets = " {&cf13r&cfff}:$rockets"; // see below comment
	if (!HAVE_LG() && HAVE_CELLS())
		extra_cells = " {&c2aac&cfff}:$cells"; //the "r" and "c" are hard-coded to have the same colors as tp_name_rl and tp_name_lg. Not sure if this is a good idea
											  //since the user can change those colors and then it won't match up
 	 
	TP_Send_TeamSay("%s%s %s%s%s%s%s%s%s%s%s", powerup, armor_health, weapon, ammo, rl, rockets, lg, cells, location, extra_rockets, extra_cells);
}

void TP_Msg_Need_f (void);
GLOBAL void TP_Msg_EnemyPowerup_f (void) // might as well add flag to this monster. need $point and $took to see red/blue flag!
{
	/*	This is the "go-to" function!". It contains all possible scenarios for any player, teammate or enemy, with any combination of powerup.
	!!! Please note this function is grouped into four parts: player with eyes (assumed enemy), enemy with powerup, you with powerup, teammate with powerup. !!! */
	extern cvar_t tp_name_quad, tp_name_pent, tp_name_quaded, tp_name_pented, tp_name_eyes, tp_name_enemy;
	MSGPART quad = "";
	MSGPART quaded = "";
	MSGPART pent = "";
	MSGPART pented = "";
	MSGPART eyes = "";
	MSGPART team = "";
	MSGPART enemy = "";
	MSGPART location = "at $[{%y}$]";
	MSGPART space = "";
	
	if (flashed) return;
    TP_FindPoint();
	
	/* Note we don't have && INPOINT(enemy) in the below if.
	This is because $point DOES NOT TELL YOU TEAMMATE/ENEMY if they have ring (because there is no way to know).
	Therefore, we don't assume enemy when we see eyes anymore because this confuses people into thinking it's ALWAYS enemy, so we just say "eyes at location" */		
	if (INPOINT(eyes)) {
		eyes = tp_name_eyes.string;

		if (INPOINT(quaded) && INPOINT(pented))	{ // Since we can't say "enemy (or) team eyes", instead we say "quaded pented eyes"
			quaded = tp_name_quaded.string;
			pented = tp_name_pented.string;
		}
		else if (INPOINT(quaded))
			quad = tp_name_quad.string;
		else if (INPOINT(pented))
			pent = tp_name_pent.string;
	}
	else if (INPOINT(enemy)) {
		enemy = tp_name_enemy.string; // %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored)
		space = " ";

		if (INPOINT(quaded) && INPOINT(pented))	{
			quad = tp_name_quad.string;
			pent = tp_name_pent.string;
		}
		else if (INPOINT(quaded))
			quad = tp_name_quad.string;
		else if (INPOINT(pented))
			pent = tp_name_pent.string;
		else {// Default to "enemy powerup", because that's the purpose of this function
			quad = "%q"; //we're hi-jacking "quad" here
			location = ""; //Report only "enemy quad", not "enemy quad %l" which is confusing
		}
	}
	else if (HAVE_POWERUP()) {
		if (DEAD()) { // if you are dead with powerup, then you dont technically have it.
			TP_Msg_Lost_f(); // this function will take care of it
			return;
		}
		
		TP_GetNeed();	
		if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells)) { // Note this doesn't include shells/nails. Not as broad as tp_msgneed.
			TP_Msg_Need_f();
			return;
		}
		else
			team = "{&c0b0team&cfff} $tp_powerups";
	}
	else if (INPOINT(teammate)) {
		team = "{&c0b0team&cfff} ";

		if (INPOINT(quaded) && INPOINT(pented))	{
			quad = tp_name_quad.string;
			pent = tp_name_pent.string;
		}
		else if (INPOINT(quaded))
			quad = tp_name_quad.string;
		else if (INPOINT(pented))
			pent = tp_name_pent.string;
		else { // Default to "enemy powerup", because that's the purpose of this function
			team = tp_name_enemy.string; // We're hi-jacking "team" to mean enemy here. %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored)
			quad = "%q"; // we're hi-jacking "quad" here
			location = ""; //Report only "enemy quad", not "enemy quad %l" which is confusing
		}
	}
	else { // Default to "enemy powerup", because that's the purpose of this function
		enemy = tp_name_enemy.string; // %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored)
		location = "%q"; //we're hi-jacking "location" here
	}

	TP_Send_TeamSay("%s%s%s%s%s%s%s%s %s", team, enemy, space, quad, pent, quaded, pented, eyes, location);
}


LOCAL void TP_Msg_GetPentQuad(qbool quad)
{
	extern cvar_t tp_name_quad, tp_name_pent;
	MSGPART get = "get";
	MSGPART powerup = "";

	TP_FindPoint();
 
	if (quad)
	{
		if (INPOINT(eyes) && INPOINT(quaded))
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		else if (HAVE_QUAD() || INPOINT(quaded)) {
			TP_Msg_EnemyPowerup_f(); // let tp_msgenemypwr handle it. Also works for the case that we have quad and are dead
			return;
		}
		else
			powerup = tp_name_quad.string;
	}
	else // get pent
	{
		if (INPOINT(eyes) && INPOINT(pented))
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		else if (HAVE_PENT() || INPOINT(pented)) { // if anyone has pent, as long as they dont have ring
			TP_Msg_EnemyPowerup_f(); // send to tp_enemypwr
			return;
		}			
		else
			powerup = tp_name_pent.string;
	}
 
	TP_Send_TeamSay("%s %s", get, powerup);
}
GLOBAL void TP_Msg_GetQuad_f (void) { TP_Msg_GetPentQuad(true); }
GLOBAL void TP_Msg_GetPent_f (void) { TP_Msg_GetPentQuad(false); }

GLOBAL void TP_Msg_QuadDead_f (void)
{
	extern cvar_t tp_name_quad;
    MSGPART quad = tp_name_quad.string;
	MSGPART dead = "dead/over";

	TP_FindPoint();
	if (HAVE_QUAD() && DEAD()) {
		TP_Msg_Lost_f(); // we use this function because it checks for dropped RL's, etc
		return;
	}
	else if (INPOINT(quaded)) { // This check is to make sure the button is not pressed accidentally.
		TP_Msg_EnemyPowerup_f(); // tp_enemypwr can handle this & all cases regarding players/powerups
		return;
	}

	TP_Send_TeamSay("%s %s", quad, dead);
}


GLOBAL void TP_Msg_Took_f (void)
{
	extern cvar_t tp_name_lg, tp_name_rl, tp_name_gl, tp_name_sng, tp_name_backpack, tp_name_cells, tp_name_rockets, tp_name_mh,
		tp_name_ra, tp_name_ya, tp_name_ga, tp_name_flag, tp_name_rune1, tp_name_rune2, tp_name_rune3, tp_name_rune4;
	MSGPART took = "";
	MSGPART at_location = " $[{%Y}$]"; // %Y is took location, remembers for 15 secs
	MSGPART powerup = "";
	MSGPART took_msg = "";
 
	if (TOOK_EMPTY())
		return;
	
	if (TOOK(quad) || TOOK(pent) || TOOK(ring)) {
		TP_GetNeed();
		
		if (DEAD())	{
			TP_Msg_QuadDead_f();
			return;
		}
		
		// Note that we check if you are holding powerup. This is because TOOK remembers for 15 seconds.
		// So a case could arise where you took quad then died less than 15 seconds later, and you'd be reporting "team need %u" (because $tp_powerups would be empty)
		else if ((NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells)) && HAVE_POWERUP())
			took = "{&c0b0team&cfff} $tp_powerups need %u";
		else if (HAVE_QUAD() || HAVE_RING()) // notice we can't send this check to tp_msgenemypwr, because if enemy with powerup is in your view, tp_enemypwr reports enemypwr first, but in this function you want to report TEAM powerup.
			took = "{&c0b0team&cfff} $tp_powerups";
		else { // In this case, you took quad or ring and died before 15 secs later. So just report what you need, nothing about powerups.
			took = "need %u"; //notice we don't say quad over, because it could be that you held ring. No way to distinguish
		}
	}
	else {
		if		(TOOK(rl))			took = tp_name_rl.string;
		else if (TOOK(lg))			took = tp_name_lg.string;
		else if (TOOK(gl))			took = tp_name_gl.string;
		else if (TOOK(sng))			took = tp_name_sng.string;
		else if (TOOK(pack))    	took = tp_name_backpack.string;
		else if (TOOK(cells))		took = tp_name_cells.string;
		else if (TOOK(rockets))		took = tp_name_rockets.string;
		else if (TOOK(mh))			took = tp_name_mh.string;
		else if (TOOK(ra))			took = tp_name_ra.string;
		else if (TOOK(ya))			took = tp_name_ya.string;
		else if (TOOK(ga))			took = tp_name_ga.string;
		else if (TOOK(flag))		took = tp_name_flag.string;
		else if (TOOK(rune1))		took = tp_name_rune1.string;
		else if (TOOK(rune2))		took = tp_name_rune2.string;
		else if (TOOK(rune3))		took = tp_name_rune3.string;
		else if (TOOK(rune4))		took = tp_name_rune4.string;
		else 						took = "{$took}"; // This should never happen
		
		took_msg = "took ";
		
		if (HAVE_POWERUP())
			powerup = "$tp_powerups ";
		else
			powerup = "";	
	}
	TP_Send_TeamSay("%s%s%s%s", powerup, took_msg, took, at_location);
}


GLOBAL void TP_Msg_Point_f (void)
{
	extern cvar_t tp_name_quad, tp_name_pent, tp_name_ring, tp_name_lg, tp_name_rl, tp_name_gl, tp_name_sng, tp_name_backpack,
		tp_name_ra, tp_name_ya, tp_name_ga, tp_name_cells, tp_name_rockets, tp_name_nails, tp_name_shells, tp_name_mh,
		tp_name_flag, tp_name_disp,	tp_name_sentry, tp_name_rune1, tp_name_rune2, tp_name_rune3, tp_name_rune4, tp_name_teammate, tp_name_enemy;
	MSGPART point = "";	
	MSGPART at_location = "at $[{%y}$] {%E}";
	MSGPART enemy = "";
	MSGPART powerup = "";
 
    if (flashed) // for teamfortres.
			return;

    TP_FindPoint();

	if (POINT_EMPTY())
		return;
	else if (INPOINT(eyes) || INPOINT(quaded) || INPOINT (pented)) {
		TP_Msg_EnemyPowerup_f(); // we use tp_enemypwr because it checks for all cases of player + powerup =)
		return;
	}

	// if you see something other than a player with powerup
	if (INPOINT(enemy))	{
		point = "{%E}";
		enemy = tp_name_enemy.string;
		at_location = " at $[{%y}$]"; // here we change at_location (remove {%E} at the end) because our point says how many enemies we have.
	}
	else if (INPOINT(teammate))	{
		if (tp_name_teammate.string[0]) // This will print the nick of your teammate if tp_name_teammate "" (not set). Else, it will print the value for tp_name_teammate
			point = tp_name_teammate.string;
		else
			point = va ("{&c0b0%s&cfff}", Macro_PointName());
	}
	else if (INPOINTPOWERUP() || INPOINTWEAPON() || INPOINTARMOR() || INPOINTAMMO() || INPOINT(pack) || INPOINT(mh) || INPOINT(flag) || INPOINT (disp) || INPOINT(sentry) || INPOINT(runes)) {
		if		(INPOINT(quad))		point = tp_name_quad.string;
		else if (INPOINT(pent))		point = tp_name_pent.string;
		else if (INPOINT(ring))		point = tp_name_ring.string;
		
		else if (INPOINT(rl))		point = tp_name_rl.string;
		else if (INPOINT(lg))		point = tp_name_lg.string;
		else if (INPOINT(gl))		point = tp_name_gl.string;
		else if (INPOINT(sng))		point = tp_name_sng.string;
		else if (INPOINT(pack))		point = tp_name_backpack.string;
		
		else if (INPOINT(ra))		point = tp_name_ra.string;
		else if (INPOINT(ya))		point = tp_name_ya.string;
		else if (INPOINT(ga))		point = tp_name_ga.string;
		
		else if (INPOINT(rockets)) 	point = tp_name_rockets.string;
		else if (INPOINT(cells)) 	point = tp_name_cells.string;
		else if (INPOINT(nails)) 	point = tp_name_nails.string;
		else if (INPOINT(shells)) 	point = tp_name_shells.string;
		
		else if (INPOINT(mh))		point = tp_name_mh.string;
		
		//TF. Note we can't tell whether they are enemy or team flag/disp/sent
		else if (INPOINT(flag))		point = tp_name_flag.string;
		else if (INPOINT(disp))		point = tp_name_disp.string;
		else if (INPOINT(sentry))	point = tp_name_sentry.string;
		
		//ctf, other
		else if (INPOINT(rune1))	point = tp_name_rune1.string;
		else if (INPOINT(rune2))	point = tp_name_rune2.string;
		else if (INPOINT(rune3))	point = tp_name_rune3.string;
		else if (INPOINT(rune4))	point = tp_name_rune4.string;
	}
	else point = "{$point}"; // this should never happen

	TP_Send_TeamSay("%s%s %s%s", powerup, point, enemy, at_location);
}


GLOBAL void TP_Msg_Need_f (void)
{
	MSGPART team_powerup = "";
    MSGPART need = "";
 
	if (DEAD())
		return;

	TP_GetNeed();

	if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells) || NEED(shells) || NEED(nails)) {
		need = "need %u $[{%l}$]";
		
		if (HAVE_POWERUP())
			team_powerup = "{&c0b0team&cfff} $tp_powerups ";
		
		if (need[0] == 0 && team_powerup[0] == 0)
			return;

		TP_Send_TeamSay("%s%s", team_powerup, need);
	}
}

GLOBAL void TP_Msg_Safe_f (void)
{
	extern cvar_t tp_name_rlg, tp_name_separator;
	MSGPART armor = "";
	MSGPART separator = "";
	MSGPART weapon = "";

	if (DEAD())
		return;

	TP_FindPoint(); // needed to make sure the area is, in fact, safe
	if (INPOINT(enemy) && !(INPOINT(quaded) || INPOINT(pented))) {
		return; //if you see an enemy without powerup, the place is still not 100% safe. But maybe you can handle him, so don't report anything yet.
	}
	if (INPOINT(enemy) && (INPOINT(quaded) || INPOINT(pented))) {
		TP_Msg_EnemyPowerup_f(); // if you see an enemy with a powerup, place is definitely not secure. report enemy powerup.
		return;
	}
	
	if ((HAVE_RA() || HAVE_YA() || HAVE_GA()) && (HAVE_RL() || HAVE_LG()))
		separator = tp_name_separator.string;
	if (HAVE_RA() || HAVE_YA() || HAVE_GA())
		armor = "$colored_armor";
	if (HAVE_RL() && HAVE_LG())
		weapon = tp_name_rlg.string;
	else if (HAVE_RL() || HAVE_LG())
		weapon = "$bestweapon";
	TP_Send_TeamSay("%s %s%s%s", "{&c0b0safe&cfff} {&c0b0[&cfff}{%l}{&c0b0]&cfff}", armor, separator, weapon);
}

GLOBAL void TP_Msg_KillMe_f (void)
{
	extern cvar_t tp_name_rl, tp_name_lg;
	MSGPART point = "";
	MSGPART kill_me = "{&cb1akill me [&cfff}{%l}{&cf2a]&cfff}";
	MSGPART weapon = "";
	MSGPART weapon_ammo = "";
	MSGPART extra_rockets = "";
	MSGPART extra_cells = "";

	if (DEAD())
		return;

	TP_FindPoint();
	if (INPOINT(teammate))
		point = va ("{&c0b0%s&cfff} ", Macro_PointName()); // Saying teammate kill me isn't much help. Only report if you can say e.g. Up2 Kill Me!

	if (HOLD_RL()) {
		weapon = tp_name_rl.string;
		weapon_ammo = ":$rockets ";
	}
	else if (HOLD_LG()) {
		weapon = tp_name_lg.string;
		weapon_ammo = ":$cells ";
	}

	if (!HOLD_RL() && HAVE_ROCKETS())
		extra_rockets = "{&cf13r&cfff}:$rockets "; // see below comment
	if (!HOLD_LG() && HAVE_CELLS())
		extra_cells = "{&c2aac&cfff}:$cells"; //the "r" and "c" are hard-coded to have the same colors as tp_name_rl and tp_name_lg. Not sure if this is a good idea
											  //since the user can change those colors and then it won't match up

	TP_Send_TeamSay("%s%s %s%s%s%s", point, kill_me, weapon, weapon_ammo, extra_rockets, extra_cells);
}

GLOBAL void TP_Msg_YouTake_f (void)
{
	MSGPART point = "";
	MSGPART take = "you take $[{%l}$]";

	TP_FindPoint();
	if (INPOINT(teammate)) {
			point = va ("{&c0b0%s&cfff} ", Macro_PointName()); // Saying teammate take isn't much help. Only report if you can say e.g. Up2 take!
			take = "take $[{%l}$]";
	}

	TP_Send_TeamSay("%s%s", point, take);
}

GLOBAL void TP_Msg_Help_f (void)
{
	TP_Send_TeamSay("%s%s", (HAVE_POWERUP() ? "$tp_powerups " : ""), "{&cff0help&cfff} {&cff0[&cfff}{%l}{&cff0]&cfff} {%e}"); // yellow help
}

// The following define allows us to make as many functions as we want and get the message "powerup message location"
#define TP_MSG_GENERIC(type) TP_Send_TeamSay("%s" type " $[{%%l}$]", (HAVE_POWERUP() ? "$tp_powerups " : ""))

GLOBAL void TP_Msg_YesOk_f (void)		{ TP_MSG_GENERIC("{yes/ok}"); }
GLOBAL void TP_Msg_NoCancel_f (void)	{ TP_MSG_GENERIC("{&cf00no/cancel&cfff}"); } //red no/cancel
GLOBAL void TP_Msg_ItemSoon_f (void)	{ TP_MSG_GENERIC("item soon"); }
GLOBAL void TP_Msg_Waiting_f (void)		{ TP_MSG_GENERIC("waiting"); }
GLOBAL void TP_Msg_Slipped_f (void)		{ TP_MSG_GENERIC("enemy slipped"); }
GLOBAL void TP_Msg_Replace_f (void)		{ TP_MSG_GENERIC("replace"); }
GLOBAL void TP_Msg_Trick_f (void)		{ TP_MSG_GENERIC("trick"); }
GLOBAL void TP_Msg_Coming_f (void)		{ TP_MSG_GENERIC("coming"); }

//TF binds
GLOBAL void TP_Msg_TFConced_f (void)
{
	extern cvar_t tp_name_filter;
	MSGPART conced = "";
	MSGPART filter = tp_name_filter.string;

	TP_FindPoint();
	if (INPOINT(enemy))
		conced = "$point conced at $[{%y}$]";
	else
		conced = "{Enemy conced}";

	TP_Send_TeamSay("%s %s", conced, filter);
}

///////////////////////////////////
////// End teamplay scripts ///////
///////////////////////////////////


GLOBAL const char *TP_MSG_Colored_Armor(void) // $colored_armor
{
	MSGPART armor = "";
 
	if (HAVE_GA())
		armor = COLORED(0b0,%a);
	else if (HAVE_YA())
		armor = COLORED(ff0,%a);
	else if (HAVE_RA())
		armor = COLORED(e00,%a);
	else
		armor = "0";
 
    return armor;
}

GLOBAL const char * TP_MSG_Colored_Powerup(void)
{
	extern cvar_t tp_name_pent, tp_name_quad, tp_name_ring, tp_name_separator;
    MSGPART pent = "";
	MSGPART quad = "";
	MSGPART ring = "";

	if (HAVE_QUAD() && HAVE_PENT() && HAVE_RING()) {
		pent = tp_name_pent.string;
		quad = tp_name_quad.string;
		ring = tp_name_ring.string;
	}
	else if (HAVE_QUAD() && HAVE_PENT()) {
		pent = tp_name_pent.string;
		quad = tp_name_quad.string;
	}
	else if (HAVE_QUAD() && HAVE_RING()) {
		quad = tp_name_quad.string;
		ring = tp_name_ring.string;
	}
	else if (HAVE_PENT() && HAVE_RING()) {
		pent = tp_name_pent.string;
		ring = tp_name_ring.string;
	}
	else if (HAVE_QUAD())
		quad = tp_name_quad.string;
	else if (HAVE_PENT())
		pent = tp_name_pent.string;
	else if (HAVE_RING())
		ring = tp_name_ring.string;

    return va("%s%s%s", quad, pent, ring);
}

#define tp_ib_name_q	    COLORED(03F,q)		// blue q for quad
#define tp_ib_name_p	    COLORED(e00,p)		// red p for pent
#define tp_ib_name_r	    COLORED(ff0,r)		// yellow r for ring

GLOBAL const char * TP_MSG_Colored_Short_Powerups(void) // this displays "qrp" instead of "quad ring pent", some people prefer this!
{
    MSGPART msg = "";

    if (HAVE_QUAD() && HAVE_RING() && HAVE_PENT())
		msg = tp_ib_name_q tp_ib_name_r tp_ib_name_p;
	else if (HAVE_QUAD() && HAVE_PENT())
		msg = tp_ib_name_q tp_ib_name_p;
	else if (HAVE_QUAD() && HAVE_RING())
		msg = tp_ib_name_q tp_ib_name_r;
	else if (HAVE_PENT() && HAVE_RING())
		msg = tp_ib_name_p tp_ib_name_r;
	else if (HAVE_QUAD())
		msg = tp_ib_name_q;
	else if (HAVE_PENT())
		msg = tp_ib_name_p;
	else if (HAVE_RING())
		msg = tp_ib_name_r;
	else
		msg = "";

    return msg;
}
