/**

  Inbuilt teamplay messages module

  made by johnnycz, Up2nOgoOd[ROCK]
  last edit:
  $Id: tp_msgs.c,v 1.1.2.2 2007-05-05 22:38:41 johnnycz Exp $

*/

#include "quakedef.h"
#include "teamplay.h"

#define GLOBAL /* */
#define LOCAL static


#define HAVE_RL() (cl.stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER)
#define HAVE_LG() (cl.stats[STAT_ITEMS] & IT_LIGHTNING)
#define HOLD_GL() (cl.stats[STAT_ACTIVEWEAPON] == IT_GRENADE_LAUNCHER) // only used in tp_lost
#define HOLD_RL() (cl.stats[STAT_ACTIVEWEAPON] == IT_ROCKET_LAUNCHER)
#define HOLD_LG() (cl.stats[STAT_ACTIVEWEAPON] == IT_LIGHTNING)
 
#define HAVE_QUAD() (cl.stats[STAT_ITEMS] & IT_QUAD) //quad
#define HAVE_PENT() (cl.stats[STAT_ITEMS] & IT_INVULNERABILITY) //pent
#define HAVE_RING() (cl.stats[STAT_ITEMS] & IT_INVISIBILITY) //ring
#define HAVE_GA() (cl.stats[STAT_ITEMS] & IT_ARMOR1) //ga
#define HAVE_YA() (cl.stats[STAT_ITEMS] & IT_ARMOR2) //ya
#define HAVE_RA() (cl.stats[STAT_ITEMS] & IT_ARMOR3) //ra
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
#define tp_ib_name_mh	    COLORED(03F,mega)	// blue mega
#define tp_ib_name_backpack	COLORED(F0F,pack)	// purple backpack
#define tp_ib_name_quad	    COLORED(03F,quad)	// blue quad
#define tp_ib_name_pent	    COLORED(e00,pent)	// red pent
#define tp_ib_name_ring	    COLORED(ff0,ring)	// yellow ring
#define tp_ib_name_eyes	    COLORED(ff0,eyes)	// yellow eyes (remember, ring is when you see the ring, eyes is when someone has rings!)
#define tp_ib_name_enemy	COLORED(e00,enemy)	// red enemy
#define tp_ib_name_team	    COLORED(0b0,team)	// green "team" (with powerup)
#define tp_ib_name_teammate	COLORED(0b0,teammate)// green "teamate"
#define tp_ib_name_quaded	COLORED(03F,quaded)	// blue "quaded"
#define tp_ib_name_pented	COLORED(e00,pented)	// red "pented"
#define tp_ib_name_rlg      COLORED(f0f,rlg)    // purple rlg
#define tp_ib_name_safe	    COLORED(0b0,safe)	// green safe
#define tp_ib_name_help	    COLORED(ff0,help)	// yellow help

#define tp_sep_red		"$R$R"      // enemy, lost
#define tp_sep_green	"$G$G"      // killed quad/ring/pent enemy, safe
#define tp_sep_yellow	"$Y$Y"      // help
#define tp_sep_white	"$x04$x04"  // Two white bubbles, location of item
#define tp_sep_blue		"$B$B"      // 
 
typedef const char * MSGPART;
 
// will return short version of player's nick for teamplay messages
LOCAL char *TP_ShortNick(void)
{
    extern cvar_t cl_fakename;
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
 

GLOBAL void TP_Msg_Lost_f (void) // Is this function useful? tp_report does this already.
{
    MSGPART led = tp_sep_red;
    MSGPART msg1 = "";
	MSGPART msg2 = "";
 
    if (HAVE_QUAD())
        msg1 = tp_ib_name_quad " over ";
 
    if (HOLD_RL() || HOLD_LG() || HOLD_GL()) // gl could be useful too
        msg2 = "lost " COLORED(f0f,$weapon) " $[{%d}$] e:%E";
    else
        msg2 = "lost $[{%d}$] e:%E";
	//$R$R quad over(1) lost weapon(2)
    TP_Send_TeamSay("%s %s%s", led, msg1, msg2);
}


GLOBAL void TP_Msg_ReportComing(qbool report) // tp_report and tp_coming are similar, differences are led color, where %l is, and the word "coming"
{
    MSGPART msg1 = "";
	MSGPART msg2 = "";
	MSGPART msg3 = "";
	MSGPART msg4 = "";
	MSGPART msg5 = "";
 
	if (DEAD()) // no matter what, if dead report lost
		{
		TP_Msg_Lost_f();
		return;
		}
	else
	{
		if (report)
			msg1 = tp_sep_blue;
		else
			msg1 = tp_sep_white " {coming}";
	}

	if (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
		msg4 = " " tp_ib_name_team " $colored_powerups";
 
    if		(HAVE_RL() && HAVE_LG())	msg5 = " " tp_ib_name_rlg ":$rockets/$cells";
    else if (HAVE_RL())					msg5 = " " tp_ib_name_rl ":$rockets";
    else if (HAVE_LG())					msg5 = " " tp_ib_name_lg ":$cells";
    else								msg5 = "";
 
	msg2 = "%h/$colored_armor";
	msg3 = "$[{%l}$]";
 
	// $B$B(1) health(2)/armor location(3) powerup(4) rlg:x(5) //tp_report
	if (report)
		TP_Send_TeamSay("%s %s %s%s%s", msg1, msg2, msg3, msg4, msg5);
	// $W$W(1) coming(1) location(3) health(2)/armor powerup(5) rlg:x(6) //tp_coming
	else
		TP_Send_TeamSay("%s %s %s%s%s", msg1, msg3, msg2, msg4, msg5); // notice that in tp_coming, we report our location before anything else!
}
GLOBAL void TP_Msg_Report_f (void) { TP_Msg_ReportComing(true); }
GLOBAL void TP_Msg_Coming_f (void) { TP_Msg_ReportComing(false); } 


GLOBAL void TP_Msg_EnemyPowerup_f (void)
{		/*
		This is the "go-to" function!". It contains all possible scenarios for any player, teammate or enemy, with any combination of powerup.
		note: in cases where you have pent and enemy has quad (or vice versa), team powerup is displayed only, NOT ENEMY POWERUP ------------------------------------------ FIX FIX FIX FIX change this around
		Note: we are assuming map has no more than 1 of each (quad,pent,ring)
		Note: this function does not take into account only teammate or only enemy (without powerup). Do not change this behaviour.
		*/
    MSGPART msg1 = "";
	MSGPART msg2 = "";
 
	if (INPOINT(quaded) && INPOINT(pented) && INPOINT(eyes))
		/*
		Note we don't have && INPOINT(enemy) in the above if.
		This is because $point DOES NOT TELL YOU TEAMMATE/ENEMY if they have ring (because there is no way to know).
		Therefore, we are assuming enemy because this function is ENEMY powerup. So if user hits this by accident (when teammate has quad/pent with ring, then it's their fault.
		*/
		{
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_quaded " " tp_ib_name_pented " " tp_ib_name_eyes " " tp_ib_name_enemy " at $[{%y}$]";
		}
	else if (HAVE_QUAD() && HAVE_PENT() && HAVE_RING())
		{
		msg1 = tp_sep_green;
		msg2 = "team " tp_ib_name_quad " " tp_ib_name_pent " " tp_ib_name_ring;
		} 
	else if (INPOINT(quaded) && INPOINT(pented) && INPOINT(enemy))
		{
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_quaded " " tp_ib_name_pented " " tp_ib_name_enemy " at $[{%y}$]";
		}
	else if ((HAVE_QUAD() && HAVE_PENT()) || (INPOINT(quaded) && INPOINT(pented) && INPOINT(teammate)))
		{
		msg1 = tp_sep_green;
		msg2 = "team " tp_ib_name_quad " " tp_ib_name_pent;
		} 
	else if (INPOINT(quaded) && INPOINT(eyes))
		{ // again we assume enemy
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_quaded " " tp_ib_name_eyes " " tp_ib_name_enemy " at $[{%y}$]";
		}
	else if (HAVE_QUAD() && HAVE_RING())
		{
		msg1 = tp_sep_green;
		msg2 = "team " tp_ib_name_quad " " tp_ib_name_ring;
		}
	else if (INPOINT(pented) && INPOINT(eyes))
		{ // assume enemy
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_pented " " tp_ib_name_enemy " at $[{%y}$]";
		}
	else if (HAVE_RING() && HAVE_PENT())
		{
		msg1 = tp_sep_green;
		msg2 = "team " tp_ib_name_pent " " tp_ib_name_ring;
		}
	else if (HAVE_QUAD() || (INPOINT(quaded) && INPOINT(teammate)))
		{
		msg1 = tp_sep_green;
		msg2 = "team " tp_ib_name_quad;
		}
	else if (INPOINT(quaded) && INPOINT(enemy))
		{
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_quaded " " tp_ib_name_enemy " at $[{%y}$]";
		}
		else if (HAVE_PENT() || (INPOINT(pented) && INPOINT(teammate)))
		{
		msg1 = tp_sep_green;
		msg2 = "team " tp_ib_name_pent;	
		}
	else if (INPOINT(pented) && INPOINT(enemy))
		{
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_pented " " tp_ib_name_enemy " at $[{%y}$]";
		}
	else if (INPOINT(eyes))
		{ // assume enemy
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_enemy " " tp_ib_name_eyes " at $[{%y}$]";
		}
	else if (HAVE_RING())
		{
		msg1 = tp_sep_green;
		msg2 = "team " tp_ib_name_ring;
		}
	else
		{
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_enemy " {%q}"; // %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored correctly!)
		}

	TP_Send_TeamSay("%s %s", msg1, msg2);
} 


LOCAL void TP_Msg_SafeHelp(qbool safe)
{
	MSGPART msg1 = "";
	MSGPART msg2 = "";
	MSGPART msg3 = "";
	MSGPART msg4 = "";

	if (safe)
		{
			if (INPOINT(enemy))
				return; // if you see enemy, it's usually not safe
			else
				msg1 = tp_sep_green " " tp_ib_name_safe;
		}
	else
		msg1 = tp_sep_yellow " " tp_ib_name_help;
 
	msg2 = "$[{%l}$]";
 
	if (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
		msg3 = " " tp_ib_name_team " $colored_powerups";
 
    if		(HAVE_RL() && HAVE_LG())	msg4 = " " tp_ib_name_rlg ":$rockets/$cells";
    else if (HAVE_RL())					msg4 = " " tp_ib_name_rl ":$rockets";
    else if (HAVE_LG())					msg4 = " " tp_ib_name_lg ":$cells";
    else								msg4 = "";
	//(1)sep, (1)=safe/help (2)=loc 3=powerup 4=weap
	TP_Send_TeamSay("%s %s%s%s", msg1, msg2, msg3, msg4);
}
GLOBAL void TP_Msg_Safe_f (void) { TP_Msg_SafeHelp(true); }
GLOBAL void TP_Msg_Help_f (void) { TP_Msg_SafeHelp(false); }


LOCAL void TP_Msg_GetPentQuad(qbool quad)
{
	MSGPART msg1 = "";
 
	if (quad)
	{
		if (HAVE_QUAD() || (INPOINT(quaded) && (INPOINT(teammate) || INPOINT(enemy))))
			{
			TP_Msg_EnemyPowerup_f(); // send to tp_enemypwr
			return;
			}
		else if (INPOINT(eyes) && INPOINT(quaded))
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		else
			msg1 = "get " tp_ib_name_quad;
	}
	else
	{
		if (HAVE_PENT() || (INPOINT(pented) && (INPOINT(teammate) || INPOINT(enemy))))
			{
			TP_Msg_EnemyPowerup_f(); // send to tp_enemypwr
			return;
			}
		else if (INPOINT(eyes) && INPOINT(pented))
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		else
			msg1 = "get " tp_ib_name_pent;
	}
 
	//$Y$Y get powerup(1)
	TP_Send_TeamSay(tp_sep_yellow " %s", msg1);
}
GLOBAL void TP_Msg_GetQuad_f (void) { TP_Msg_GetPentQuad(true); }
GLOBAL void TP_Msg_GetPent_f (void) { TP_Msg_GetPentQuad(false); }


GLOBAL void TP_Msg_QuadDead_f (void)
{
    MSGPART msg1 = "";
 
	if (HAVE_QUAD() || INPOINT(quaded)) // If ANYONE has quad
		{
		TP_Msg_EnemyPowerup_f(); // tp_enemypwr can handle this & all cases regarding players/powerups
		return;
		}
	else msg1 = tp_ib_name_quad " dead";
 
	TP_Send_TeamSay(tp_sep_yellow " %s", msg1);
}


GLOBAL void TP_Msg_Took_f (void) // later: runes, flag
{
    MSGPART msg1 = "";
	MSGPART msg2 = "";
 
	if (TOOK_EMPTY())
		return;
	else if (TOOK(quad) || TOOK(pent) || TOOK(ring))
		{
		TP_Msg_EnemyPowerup_f(); // tp_enemypwr can handle all cases with player/powerup
		return;
		}
	else
	{
		msg2 = "$[{%l}$]";
		
		if	(TOOK(rl))											msg1 = tp_ib_name_rl;
		else if (TOOK(lg))										msg1 = tp_ib_name_lg;
		else if (TOOK(gl))										msg1 = tp_ib_name_gl;
		else if (TOOK(sng))										msg1 = tp_ib_name_sng;
		else if (TOOK(pack))    								msg1 = tp_ib_name_backpack;
		else if (TOOK(rockets) || TOOK(cells))					msg1 = "{$took}";
		else if (TOOK(mh))										msg1 = tp_ib_name_mh;
		else if (TOOK(ra))										msg1 = tp_ib_name_ra;
		else if (TOOK(ya))										msg1 = tp_ib_name_ya;
		else if (TOOK(ga))										msg1 = tp_ib_name_ga;
	}
	TP_Send_TeamSay(tp_sep_white " {took} %s at %s", msg1, msg2);
}


GLOBAL void TP_Msg_Point_f (void)
{
    MSGPART msg1 = "";
	MSGPART msg2 = "";
	MSGPART msg3 = "";
 
    if (flashed) return;
    TP_FindPoint();

	if (POINT_EMPTY())
		return;
	else if (INPOINT(eyes) || ((INPOINT(enemy) || INPOINT(teammate)) && (INPOINT(quaded) || INPOINT (pented)))) // enemy with powerup, or player with eyes
		{
		TP_Msg_EnemyPowerup_f(); // we use tp_enemypwr because it checks for all cases of player + powerup =)
		return;
		}
	else
	{ // the following are grouped into the if's by the color leds they will be.
		msg3 = "at $[{%y}$] e:%E"; // this has to be established at the beginning because it's the same for all cases except when you see enemy.
	
		if (INPOINT(enemy))
			{
				msg1 = tp_sep_red;
				msg2 = "%E " tp_ib_name_enemy;
				msg3 = "at $[{%y}$]"; // here we change msg3 because our msg2 says how many enemies we have.
			}
		else if (INPOINT(teammate))
			{
				msg1 = tp_sep_green;
				msg2 = tp_ib_name_teammate;
			}
		else if (INPOINTPOWERUP() || INPOINTWEAPON() || INPOINTARMOR() || INPOINTAMMO() || INPOINT(pack) || INPOINT(mh))
		//  flag, disp, sent, rune
			{
				msg1 = tp_sep_yellow; // if we see any of these items ready for pickup, then we are yellow led status (meaning notice)
					if (INPOINT(quad))			msg2 = tp_ib_name_quad;
					else if (INPOINT(pent))		msg2 = tp_ib_name_pent;
					else if (INPOINT(ring))		msg2 = tp_ib_name_ring;
					
					else if (INPOINT(rl))		msg2 = tp_ib_name_rl;
					else if (INPOINT(lg))		msg2 = tp_ib_name_lg;
					else if (INPOINT(gl))		msg2 = tp_ib_name_gl;
					else if (INPOINT(sng))		msg2 = tp_ib_name_sng;
					else if (INPOINT(pack))	msg2 = tp_ib_name_backpack;
					
					else if (INPOINT(ra))		msg2 = tp_ib_name_ra;
					else if (INPOINT(ya))		msg2 = tp_ib_name_ya;
					else if (INPOINT(ga))		msg2 = tp_ib_name_ga;
					
					else if (INPOINT(rockets)) 	msg2 = "{rockets}";
					else if (INPOINT(cells)) 	msg2 = "{cells}";
					else if (INPOINT(nails)) 	msg2 = "{nails}";
					
					else if (INPOINT(mh))		msg2 = tp_ib_name_mh; // why does this display as ga? BUG BUG BUGBUG BUG BUGBUG BUG BUGBUG BUG BUGBUG BUG BUGBUG BUG
					
					else msg2 = "$point"; // this should never happen
			}
	}
	
	//led(1) item(2) at loc(3)
	TP_Send_TeamSay("%s %s %s", msg1, msg2, msg3);
}


GLOBAL void TP_Msg_Need_f (void)
{
    MSGPART msg1 = "";
 
	// todo
	
	TP_Send_TeamSay(tp_sep_green " %s", msg1);
}

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
		msg = tp_ib_name_quad " " tp_ib_name_pent " " tp_ib_name_ring;
	else if (HAVE_QUAD() && HAVE_PENT())
		msg = tp_ib_name_quad " " tp_ib_name_pent;
	else if (HAVE_QUAD() && HAVE_RING())
		msg = tp_ib_name_quad " " tp_ib_name_ring;
	else if (HAVE_PENT() && HAVE_RING())
		msg = tp_ib_name_pent " " tp_ib_name_ring;
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
