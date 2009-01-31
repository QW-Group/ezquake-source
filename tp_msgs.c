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

/*
Should we enforce special colors this way below or we just should make
these values new default?
In my opinion with new defaults we should wait for different color syntax. - johnnycz
However this brings a problem - you always have to use these and cannot
use the %-macros nor the $-macros.
*/
#define tp_ib_name_rl	    COLORED(f0f,rl)	    // purple rl
#define tp_ib_name_lg	    COLORED(f0f,lg)	    // purple lg
#define tp_ib_name_gl	    COLORED(f0f,gl)	    // purple gl
#define tp_ib_name_sng	    COLORED(f0f,sng)	// purple sng
#define tp_ib_name_ga	    COLORED(0b0,ga)	    // green ga
#define tp_ib_name_ya	    COLORED(ff0,ya)	    // yellow ya
#define tp_ib_name_ra	    COLORED(e00,ra)	    // red ra
#define tp_ib_name_mh	    COLORED(0b0,mega)	// green mega
#define tp_ib_name_backpack	COLORED(F0F,pack)	// purple backpack
#define tp_ib_name_quad	    COLORED(03F,quad)	// blue quad
#define tp_ib_name_pent	    COLORED(e00,pent)	// red pent
#define tp_ib_name_ring	    COLORED(ff0,ring)	// yellow ring
#define tp_ib_name_q	    COLORED(03F,q)		// blue q for quad
#define tp_ib_name_p	    COLORED(e00,p)		// red p for pent
#define tp_ib_name_r	    COLORED(ff0,r)		// yellow r for ring
#define tp_ib_name_eyes	    COLORED(ff0,eyes)	// yellow eyes (remember, ring is when you see the ring, eyes is when someone has rings!)
#define tp_ib_name_flag	    COLORED(f50,flag)	// orange flag
#define tp_ib_name_enemy	COLORED(e00,enemy)	// red enemy
#define tp_ib_name_team	    COLORED(0b0,team)	// green "team" (with powerup)
#define tp_ib_name_teammate	COLORED(0b0,teammate)// green "teamate"
#define tp_ib_name_quaded	COLORED(03F,quaded)	// blue "quaded"
#define tp_ib_name_pented	COLORED(e00,pented)	// red "pented"
#define tp_ib_name_rlg      COLORED(f0f,rlg)    // purple rlg
 
typedef const char * MSGPART;
 
extern cvar_t cl_fakename;
// will return short version of player's nick for teamplay messages
LOCAL char *TP_ShortNick(void)
{
	static char buf[7];
 
	if (*(cl_fakename.string)) return "";
	else {
		snprintf(buf, sizeof(buf), "$\\%.3s:", TP_PlayerName());
		return buf;
	}
}      
 
// wrapper for snprintf & Cbuf_AddText that will add say_team nick part
LOCAL void TP_Send_TeamSay(char *format, ...)
{
    char tp_msg_head[256], tp_msg_body[256], tp_msg[512];
    va_list argptr;
 
    snprintf(tp_msg_head, sizeof(tp_msg_head), "say_team %s ", TP_ShortNick());
 
	va_start (argptr, format);
    vsnprintf(tp_msg_body, sizeof(tp_msg_body), format, argptr);
	va_end (argptr);
 
    snprintf(tp_msg, sizeof(tp_msg), "%s%s\n", tp_msg_head, tp_msg_body);
 
    Cbuf_AddText(tp_msg);
}
 
 
 
///////////////////////////////////
//////// Start teamplay scripts ///////
///////////////////////////////////

GLOBAL void TP_Msg_Lost_f (void)
{ // This function has no use in being an external (client bind), excep that people are probably used to having this. Notice that tp_report makes this function obsolete.
    MSGPART msg1 = "";
	MSGPART msg2 = "";
	if (DEAD()) // make sure you're dead when you press this bind, else it's useless. this check exists in case player has a bind tp_lost, b/c in tp_report we always check if dead.
	{
		if (HAVE_QUAD())
		{
		msg1 = tp_ib_name_quad " over ";
		}
 
		if (HOLD_RL() || HOLD_LG() || HOLD_GL()) // gl could be useful too
		{
			msg2 = "lost "  COLORED(f0f,$weapon) " $[{%d}$] %E";
		}
		else
		{
			msg2 = "lost $[{%d}$] %E";
		}
	}
	else // if currently alive, then report last death location
		msg1 = "lost $[{%d}$]";
		
    TP_Send_TeamSay("%s%s", msg1, msg2);
}


GLOBAL void TP_Msg_Report_f (void)
{
	MSGPART powerup = "";
	MSGPART armor_health = "";
	MSGPART location = "";
	MSGPART weap_ammo = "";
 
	if (DEAD()) // no matter what, if dead report lost (how could you be coming if you're dead?)
		{
		TP_Msg_Lost_f();
		return;
		}
	
	if (HAVE_POWERUP())
		powerup = "$colored_short_powerups";
	else
		powerup = "";
 
    if		(HAVE_RL() && HAVE_LG())	weap_ammo = tp_ib_name_rlg ":$rockets/$cells";
    else if (HAVE_RL())					weap_ammo = tp_ib_name_rl ":$rockets";
    else if (HAVE_LG())					weap_ammo = tp_ib_name_lg ":$cells";
	else if (HAVE_GL())					weap_ammo = tp_ib_name_gl ":$rockets";
    else								weap_ammo = "";
 
	armor_health = "$colored_armor/%h";
	location = "$[{%l}$]";
	 
	TP_Send_TeamSay("%s %s %s %s", powerup, armor_health, location, weap_ammo);
}


GLOBAL void TP_Msg_EnemyPowerup_f (void) // might as well add flag to this monster. need $point and $took to see red/blue flag!
{
	/*
	This is the "go-to" function!". It contains all possible scenarios for any player, teammate or enemy, with any combination of powerup.
	!!! Please note this function is grouped into four parts: player with eyes (assumed enemy), enemy with powerup, you with powerup, teammate with powerup. !!!
	*/
	MSGPART message = "";
	
	if (flashed) return;
    TP_FindPoint();
	
		/*
		Note we don't have && INPOINT(enemy) in the below if.
		This is because $point DOES NOT TELL YOU TEAMMATE/ENEMY if they have ring (because there is no way to know).
		Therefore, we don't assume enemy when we see eyes anymore because this confuses people into thinking it's ALWAYS enemy, so we just say "eyes at location"
		*/
		
	if (INPOINT(eyes))
	{
		if (INPOINT(quaded) && INPOINT(pented))
		{
			message = tp_ib_name_quaded " " tp_ib_name_pented " " tp_ib_name_eyes " at $[{%y}$]";
		}
		else if (INPOINT(quaded))
		{
			message = tp_ib_name_quaded " " tp_ib_name_eyes " at $[{%y}$]";
		}
		else if (INPOINT(pented))
		{
			message = tp_ib_name_pented " " tp_ib_name_eyes " at $[{%y}$]";
		}
		else
		{
			message = tp_ib_name_eyes " at $[{%y}$]";
		}
	}
	else if (INPOINT(enemy))
	{
		if (INPOINT(quaded) && INPOINT(pented))
		{
			message = tp_ib_name_quaded " " tp_ib_name_pented " " tp_ib_name_enemy " at $[{%y}$]";
		}
		else if (INPOINT(quaded))
		{
			message = tp_ib_name_quaded " " tp_ib_name_enemy " at $[{%y}$]";
		}
		else if (INPOINT(pented))
		{
			message = tp_ib_name_pented " " tp_ib_name_enemy " at $[{%y}$]";
		}
		else
		{
			message = tp_ib_name_enemy " %q"; // %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored)
		}
	}
	else if (HAVE_POWERUP())
	{
		TP_GetNeed();
		if (DEAD()) // if you are dead with powerup, then you dont technically have it.
		{
			TP_Msg_Lost_f(); // this function will take care of it
			return;
		}		
		else if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells)) // all things we care for
		{
			message = tp_ib_name_team " $colored_powerups need %u";
		}
		else
		{
			message = tp_ib_name_team " $colored_powerups";
		}
	}
	else if (INPOINT(teammate))
	{
		if (INPOINT(quaded) && INPOINT(pented))
		{
			message = tp_ib_name_team " " tp_ib_name_quad " " tp_ib_name_pent;
		}
		else if (INPOINT(quaded))
		{
			message = tp_ib_name_team " " tp_ib_name_quad;
		}
		else if (INPOINT(pented))
		{
			message = tp_ib_name_team " " tp_ib_name_pent;	
		}
		else
		{
			message = tp_ib_name_enemy " %q"; // %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored)
		}
	}
	else
	{
		message = tp_ib_name_enemy " %q"; // %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored)
	}

	TP_Send_TeamSay("%s", message);
}


LOCAL void TP_Msg_GetPentQuad(qbool quad)
{
	MSGPART get_powerup = "";
 
	if (quad)
	{
		if (DEAD() && HAVE_QUAD())
		{ // player messed up, he's dead with quad, so there's no quad to get!
			TP_Msg_Lost_f();
			return;
		}
		else if (INPOINT(eyes) && INPOINT(quaded))
		{
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		}
		else if (INPOINT(quaded))
		{
			TP_Msg_EnemyPowerup_f(); // let tp_msgenemypwr handle it...
			return;
		}
		else
		{
			get_powerup = "get " tp_ib_name_quad;
		}
	}
	else // get pent
	{
		if (INPOINT(eyes) && INPOINT(pented))
		{
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		}
		else if (HAVE_PENT() || INPOINT(pented)) // if anyone has pent, as long as they dont have ring
		{
			TP_Msg_EnemyPowerup_f(); // send to tp_enemypwr
			return;
		}			
		else
		{
			get_powerup = "get " tp_ib_name_pent;
		}
	}
 
	//$Y$Y get powerup(1)
	TP_Send_TeamSay("%s", get_powerup);
}
GLOBAL void TP_Msg_GetQuad_f (void) { TP_Msg_GetPentQuad(true); }
GLOBAL void TP_Msg_GetPent_f (void) { TP_Msg_GetPentQuad(false); }


GLOBAL void TP_Msg_QuadDead_f (void)
{
    MSGPART quad_dead = "";
 
	if (DEAD() && HAVE_QUAD()) // when you have quad and you die, before you spawn you're still glowing (in some mods/settings), meaning you have quad. this check is necessary!
	{
		TP_Msg_Lost_f();
		return;
	}
	else if (HAVE_QUAD() || INPOINT(quaded)) // If ANYONE has quad
	{ // This check is to make sure the button is not pressed accidentally. (how can you report quad dead if you see quad?)
		TP_Msg_EnemyPowerup_f(); // tp_enemypwr can handle this & all cases regarding players/powerups
		return;
	}
	else quad_dead = tp_ib_name_quad " dead/over";

	TP_Send_TeamSay("%s",quad_dead);
}


GLOBAL void TP_Msg_Took_f (void)
{
	MSGPART took = "";
	MSGPART at_location = "";
	MSGPART powerup = "";
	MSGPART took_msg = "";
 
	if (TOOK_EMPTY())
		return;
	else if (TOOK(quad) || TOOK(pent) || TOOK(ring))
	{
		TP_GetNeed();
		if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells))
		{
			took = tp_ib_name_team " $colored_powerups need %u";
		}
		else
		{ // notice we can't send this check to tp_msgenemypwr, that's because if enemy with powerup is in your view, tp_enemypwr reports enemypwr first, but in this function you want to report TEAM powerup.
			took = tp_ib_name_team " $colored_powerups";
		}
	}
	else
	{
		if	(TOOK(rl))									took = tp_ib_name_rl;
		else if (TOOK(lg))								took = tp_ib_name_lg;
		else if (TOOK(gl))								took = tp_ib_name_gl;
		else if (TOOK(sng))								took = tp_ib_name_sng;
		else if (TOOK(pack))    						took = tp_ib_name_backpack;
		else if (TOOK(rockets) || TOOK(cells))			took = "{$took}";
		else if (TOOK(mh))								took = tp_ib_name_mh;
		else if (TOOK(ra))								took = tp_ib_name_ra;
		else if (TOOK(ya))								took = tp_ib_name_ya;
		else if (TOOK(ga))								took = tp_ib_name_ga;
		else if (TOOK(flag))							took = tp_ib_name_flag;
		else if (TOOK(rune1))							took = "$tp_name_rune1";
		else if (TOOK(rune2))							took = "$tp_name_rune2";
		else if (TOOK(rune3))							took = "$tp_name_rune3";
		else if (TOOK(rune4))							took = "$tp_name_rune4";
		else 											took = "{$took}"; // This should never happen
		
		if (HAVE_POWERUP())
		{
			powerup = "$colored_short_powerups";
		}
		else
		{
			powerup = "";
		}
		
		at_location = "$[{%Y}$]"; // %Y is "forgotten" macro - location of item you took
		took_msg = " took ";
	}
	TP_Send_TeamSay("%s%s%s %s", powerup, took_msg, took, at_location);
}

extern cvar_t tp_name_teammate;
GLOBAL void TP_Msg_Point_f (void)
{
	MSGPART point = "";
	MSGPART at_location = "";
	MSGPART powerup = "";
 
    if (flashed) return;
    TP_FindPoint();

	if (POINT_EMPTY())
		return;
	else if (INPOINT(eyes) || INPOINT(quaded) || INPOINT (pented)) // anyone with powerup
	{
		TP_Msg_EnemyPowerup_f(); // we use tp_enemypwr because it checks for all cases of player + powerup =)
		return;
	}
	else
	{
		at_location = "at $[{%y}$] %E"; // this has to be established at the beginning because it's the same for all cases except when you see enemy.
	
		if (INPOINT(enemy))
		{
			point = "%E " tp_ib_name_enemy;
			at_location = "at $[{%y}$]"; // here we change at_location because our point says how many enemies we have.
		}
		else if (INPOINT(teammate))
		{
			if (tp_name_teammate.string[0])
				point = tp_ib_name_teammate;
			else
				point = va ("{&c0b0%s&cfff}", Macro_PointName());
		}
		else if (INPOINTPOWERUP() || INPOINTWEAPON() || INPOINTARMOR() || INPOINTAMMO() || INPOINT(pack) || INPOINT(mh) || INPOINT(flag) || INPOINT (disp) || INPOINT(sentry) || INPOINT(runes)) // How can I do if INPONIT(anything) or if INPOINT() not empty?
		//  flag, disp, sent, rune
		{
			if (INPOINT(quad))			point = tp_ib_name_quad;
			else if (INPOINT(pent))		point = tp_ib_name_pent;
			else if (INPOINT(ring))		point = tp_ib_name_ring;
			
			else if (INPOINT(rl))		point = tp_ib_name_rl;
			else if (INPOINT(lg))		point = tp_ib_name_lg;
			else if (INPOINT(gl))		point = tp_ib_name_gl;
			else if (INPOINT(sng))		point = tp_ib_name_sng;
			else if (INPOINT(pack))		point = tp_ib_name_backpack;
			
			else if (INPOINT(ra))		point = tp_ib_name_ra;
			else if (INPOINT(ya))		point = tp_ib_name_ya;
			else if (INPOINT(ga))		point = tp_ib_name_ga;
			
			else if (INPOINT(rockets)) 	point = "{$point}";
			else if (INPOINT(cells)) 	point = "{$point}";
			else if (INPOINT(nails)) 	point = "{$point}";
			
			else if (INPOINT(mh))		point = tp_ib_name_mh;
			
			//TF
			else if (INPOINT(flag))		point = tp_ib_name_flag; // note we cannot tell if it's enemy or team flag
			else if (INPOINT(disp))		point = "{$point}";	// note we cannot tell if it's enemy or team disp
			else if (INPOINT(sentry))	point = "{$point}"; // note we cannot tell if it's enemy or team sent
			
			//ctf, other
			else if (INPOINT(rune1))	point = "$tp_name_rune1";
			else if (INPOINT(rune2))	point = "$tp_name_rune2";
			else if (INPOINT(rune3))	point = "$tp_name_rune3";
			else if (INPOINT(rune4))	point = "$tp_name_rune4";
		}
		else point = "{$point}"; // this should never happen
	}
	
		if (HAVE_POWERUP())
			powerup = "$colored_short_powerups";
		else
			powerup = "";
	
	//led(1) item(2) at loc(3)
	TP_Send_TeamSay("%s %s %s", powerup, point, at_location);
}


GLOBAL void TP_Msg_Need_f (void)
{
    MSGPART need = "";
 
	if (DEAD())
	{
		return; // if you're dead, you need better aim :E
	}
	else
	{
		TP_GetNeed();
	}
	
	if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells) || NEED(shells) || NEED(nails))
	{
		need = "{need} %u";
	}
	else
	{
		return;
	}
	
	TP_Send_TeamSay("%s", need);
}

// The following define allows us to make as many functions as we want and get the message "powerup message location"
#define TP_MSG_GENERIC(type) TP_Send_TeamSay("%s"type" $[{%%l}$]", (HAVE_POWERUP() ? "$colored_short_powerups " : ""))

GLOBAL void TP_Msg_YesOk_f (void) { TP_MSG_GENERIC("{&c9ffyes/ok&cfff}"); } //cyan yes/ok
GLOBAL void TP_Msg_YouTake_f (void) { TP_MSG_GENERIC("you take"); }
GLOBAL void TP_Msg_Waiting_f (void) { TP_MSG_GENERIC("waiting"); }
GLOBAL void TP_Msg_Slipped_f (void) { TP_MSG_GENERIC("enemy slipped"); }
GLOBAL void TP_Msg_Replace_f (void) { TP_MSG_GENERIC("{&c9ffreplace&cfff}"); } //cyan replace
GLOBAL void TP_Msg_Trick_f (void) { TP_MSG_GENERIC("trick"); }
GLOBAL void TP_Msg_Safe_f (void) { TP_MSG_GENERIC("{&c0b0safe&cfff}"); } // green safe
GLOBAL void TP_Msg_Help_f (void) { TP_MSG_GENERIC("{&cff0help&cfff}"); } // yellow help
GLOBAL void TP_Msg_Coming_f (void) { TP_MSG_GENERIC("{&cf50coming&cfff}"); } // orange coming


///////////////////////////////////
//////// End teamplay scripts ////////
///////////////////////////////////


GLOBAL const char *TP_MSG_Colored_Armor(void)
{
	MSGPART msg = "";
 
	if (HAVE_GA())
		msg = COLORED(0b0,%a);
	else if (HAVE_YA())
		msg = COLORED(ff0,%a);
	else if (HAVE_RA())
		msg = COLORED(e00,%a);
	else
		msg = "0";
 
    return msg;
}

GLOBAL const char * TP_MSG_Colored_Powerup(void)
{
    MSGPART msg = "";

    if (HAVE_QUAD() && HAVE_PENT() && HAVE_RING())
		msg = tp_ib_name_quad " " tp_ib_name_ring " " tp_ib_name_pent;
	else if (HAVE_QUAD() && HAVE_PENT())
		msg = tp_ib_name_quad " " tp_ib_name_pent;
	else if (HAVE_QUAD() && HAVE_RING())
		msg = tp_ib_name_quad " " tp_ib_name_ring;
	else if (HAVE_PENT() && HAVE_RING())
		msg = tp_ib_name_ring " " tp_ib_name_pent;
	else if (HAVE_QUAD())
		msg = tp_ib_name_quad;
	else if (HAVE_PENT())
		msg = tp_ib_name_pent;
	else if (HAVE_RING())
		msg = tp_ib_name_ring;
	else
		msg = "";

    return msg;
}

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
		msg = tp_ib_name_r tp_ib_name_p;
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
