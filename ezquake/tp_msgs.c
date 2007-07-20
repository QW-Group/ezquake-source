/**

  Inbuilt teamplay messages module

  made by johnnycz, Up2nOgoOd[ROCK]
  last edit:
  $Id: tp_msgs.c,v 1.1.2.19 2007-07-20 01:05:18 himan Exp $

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
#define HAVEPOWERUP() (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
 
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
#define tp_ib_name_mh	    COLORED(03F,mega)	// blue mega
#define tp_ib_name_backpack	COLORED(F0F,pack)	// purple backpack
#define tp_ib_name_quad	    COLORED(03F,quad)	// blue quad
#define tp_ib_name_pent	    COLORED(e00,pent)	// red pent
#define tp_ib_name_ring	    COLORED(ff0,ring)	// yellow ring
#define tp_ib_name_q	    COLORED(03F,q)		// blue q for quad
#define tp_ib_name_p	    COLORED(e00,p)		// red p for pent
#define tp_ib_name_r	    COLORED(ff0,r)		// yellow r for ring
#define tp_ib_name_eyes	    COLORED(ff0,eyes)	// yellow eyes (remember, ring is when you see the ring, eyes is when someone has rings!)
#define tp_ib_name_flag	    COLORED(f60,flag)	// orange flag
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
#define tp_sep_white	"$W$W"		// Two white bubbles, location of item
#define tp_sep_blue		"$B$B"      // 
 
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
    MSGPART led = tp_sep_red;
    MSGPART msg1 = "";
	MSGPART msg2 = "";
	if (DEAD()) // make sure you're dead when you press this bind, else it's useless. this check exists in case player has a bind tp_lost, b/c in tp_report we always check if dead.
		{
		if (HAVE_QUAD())
			msg1 = tp_ib_name_quad " over ";
 
		if (HOLD_RL() || HOLD_LG() || HOLD_GL()) // gl could be useful too
			msg2 = "lost "  COLORED(f0f,$weapon) " $[{%d}$] e:%E";
		else
			msg2 = "lost $[{%d}$] e:%E";
		}
	else // if alive and reporting last death location
		msg1 = "lost $[{%d}$]";
		
	//$R$R quad over(1) lost loc weapon(2)
    TP_Send_TeamSay("%s %s%s", led, msg1, msg2);
}


GLOBAL void TP_Msg_ReportComing(qbool report)
{ // tp_report and tp_coming are similar, differences are led color, where %l is, and the word "coming"
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
			{
			msg1 = tp_sep_blue;
			msg4 = "$[{%l}$]";
			}
		else
			{
			msg1 = tp_sep_white;
			msg4 = "{coming} $[{%l}$]";
			}
		}

	if (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
		msg2 = " $colored_short_powerups";
	else
		msg2 = "";
 
    if		(HAVE_RL() && HAVE_LG())	msg5 = tp_ib_name_rlg ":$rockets/$cells";
    else if (HAVE_RL())					msg5 = tp_ib_name_rl ":$rockets";
    else if (HAVE_LG())					msg5 = tp_ib_name_lg ":$cells";
	else if (HAVE_GL())					msg5 = tp_ib_name_gl ":$rockets";
    else								msg5 = "";
 
	msg3 = "$colored_armor/%h";
	 
	// $B$B(1) powerup (2) health(3)/armor location(4) rlg:x(5) //tp_report
	if (report)
		TP_Send_TeamSay("%s%s %s %s %s", msg1, msg2, msg3, msg4, msg5);
	// $W$W(1) coming(1) powerup(2) location(4) health(3)/armor rlg:x(5) //tp_coming
	else
		TP_Send_TeamSay("%s%s %s %s %s", msg1, msg2, msg4, msg3, msg5); // notice that in tp_coming, we report our location before anything else!
}
GLOBAL void TP_Msg_Report_f (void) { TP_Msg_ReportComing(true); }
GLOBAL void TP_Msg_Coming_f (void) { TP_Msg_ReportComing(false); } 


GLOBAL void TP_Msg_EnemyPowerup_f (void) // might as well add flag to this monster. // need $point and $took to see red/blue flag!
{		// This is the "go-to" function!". It contains all possible scenarios for any player, teammate or enemy, with any combination of powerup.
	MSGPART msg1 = "";
	MSGPART msg2 = "";
	
	if (flashed) return;
    TP_FindPoint();
	
		/*
		Note we don't have && INPOINT(enemy) in the below if.
		This is because $point DOES NOT TELL YOU TEAMMATE/ENEMY if they have ring (because there is no way to know).
		Therefore, we are assuming enemy because this function is ENEMY powerup. So if user hits this by accident (when teammate has quad/pent with ring, then it's their fault.
		*/
		
	if (INPOINT(eyes)) // we assume enemy!
	{
		if (INPOINT(quaded) && INPOINT(pented))
			{
			msg1 = tp_sep_red;
			msg2 = tp_ib_name_quaded " " tp_ib_name_pented " " tp_ib_name_eyes " " tp_ib_name_enemy " at $[{%y}$]";
			}
		else if (INPOINT(quaded))
			{
			msg1 = tp_sep_red;
			msg2 = tp_ib_name_quaded " " tp_ib_name_eyes " " tp_ib_name_enemy " at $[{%y}$]";
			}
		else if (INPOINT(pented))
			{
			msg1 = tp_sep_red;
			msg2 = tp_ib_name_pented " " tp_ib_name_eyes " " tp_ib_name_enemy " at $[{%y}$]";
			}
		else
			{
			msg1 = tp_sep_red;
			msg2 = tp_ib_name_eyes " " tp_ib_name_enemy " at $[{%y}$]";
			}
	}
	else if (INPOINT(enemy))
	{
		if (INPOINT(quaded) && INPOINT(pented))
			{
			msg1 = tp_sep_red;
			msg2 = tp_ib_name_quaded " " tp_ib_name_pented " " tp_ib_name_enemy " at $[{%y}$]";
			}
		else if (INPOINT(quaded))
			{
			msg1 = tp_sep_red;
			msg2 = tp_ib_name_quaded " " tp_ib_name_enemy " at $[{%y}$]";
			}
		else if (INPOINT(pented))
			{
			msg1 = tp_sep_red;
			msg2 = tp_ib_name_pented " " tp_ib_name_enemy " at $[{%y}$]";
			}
	}
	else if (HAVEPOWERUP())
	{
		TP_GetNeed();
		if (DEAD()) // if you are dead with powerup, then you dont technically have it.
			{
			TP_Msg_Lost_f(); // this function will take care of it
			return;
			}		
		else if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells)) // all things we care for
			{
			msg1 = tp_sep_green;
			msg2 = tp_ib_name_team " $colored_powerups need %u";
			}
		else
			{
			msg1 = tp_sep_green;
			msg2 = tp_ib_name_team " $colored_powerups";
			}
	}
	else if (INPOINT(teammate))
	{
		if (INPOINT(quaded) && INPOINT(pented))
			{
			msg1 = tp_sep_green;
			msg2 = tp_ib_name_team " " tp_ib_name_quad " " tp_ib_name_pent;
			}
		else if (INPOINT(quaded))
			{
			msg1 = tp_sep_green;
			msg2 = tp_ib_name_team " " tp_ib_name_quad;
			}
		else if (INPOINT(pented))
			{
			msg1 = tp_sep_green;
			msg2 = tp_ib_name_team " " tp_ib_name_pent;	
			}
	}
	else
	{
		msg1 = tp_sep_red;
		msg2 = tp_ib_name_enemy " {%q}"; // %q is last seen powerup of enemy. defaults to quad, which is nice (but it won't be colored)
	}

	TP_Send_TeamSay("%s %s", msg1, msg2);
}


LOCAL void TP_Msg_SafeHelp(qbool safe)
{
	MSGPART msg1 = "";
	MSGPART msg2 = "";
	MSGPART msg3 = "";
	MSGPART msg4 = "";
	MSGPART msg5 = "";

	if (safe)
		{
		msg1 = tp_sep_green;
		msg5 = tp_ib_name_safe;
		}
	else // now help
		{
		msg1 = tp_sep_yellow;
		msg5 = tp_ib_name_help;
		}
	
	msg3 = "$[{%l}$]";
 
	if (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
		msg2 = " $colored_short_powerups";
	else
		msg2 = "";
 
    if		(HAVE_RL() && HAVE_LG())	msg4 = " " tp_ib_name_rlg ":$rockets/$cells";
    else if (HAVE_RL())					msg4 = " " tp_ib_name_rl ":$rockets";
    else if (HAVE_LG())					msg4 = " " tp_ib_name_lg ":$cells";
    else								msg4 = "";
	//(1)sep, (1)=safe/help (2)=loc 3=powerup 4=weap
	TP_Send_TeamSay("%s%s %s %s%s", msg1, msg2, msg5, msg3, msg4);
}
GLOBAL void TP_Msg_Safe_f (void) { TP_Msg_SafeHelp(true); }
GLOBAL void TP_Msg_Help_f (void) { TP_Msg_SafeHelp(false); }


LOCAL void TP_Msg_GetPentQuad(qbool quad)
{
	MSGPART msg1 = "";
 
	if (quad)
	{
		if (DEAD() && HAVE_QUAD())
			{ // player messed up, he's dead with quad, so there's no quad to get!
			TP_Msg_Lost_f();
			return;
			}
		else if (INPOINT(eyes) && INPOINT(quaded))
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		else if (INPOINT(quaded))
			{
			TP_Msg_EnemyPowerup_f(); // let tp_msgenemypwr handle it...
			return;
			}
		else
			msg1 = "get " tp_ib_name_quad;
	}
	else
	{
		if (INPOINT(eyes) && INPOINT(pented))
			return; // Don't know for sure if it's enemy or not, and can't assume like we do in tp_enemypwr because this isn't tp_ENEMYpwr
		else if (HAVE_PENT() || INPOINT(pented)) // if anyone has pent, as long as they dont have ring
			{
			TP_Msg_EnemyPowerup_f(); // send to tp_enemypwr
			return;
			}

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
 
	if (DEAD() && HAVE_QUAD()) // when you have quad and you die, before you spawn you're still glowing (in some mods/settings), meaning you have quad. this check is necessary!
		{
		TP_Msg_Lost_f();
		return;
		}
	else if (HAVE_QUAD() || INPOINT(quaded)) // If ANYONE has quad
		{
		TP_Msg_EnemyPowerup_f(); // tp_enemypwr can handle this & all cases regarding players/powerups
		return;
		}
	else msg1 = tp_ib_name_quad " dead";

	TP_Send_TeamSay(tp_sep_yellow " %s", msg1);
}



GLOBAL void TP_Msg_Took_f (void)
{
    MSGPART msg1 = "";
	MSGPART msg2 = "";
	MSGPART msg3 = "";
	MSGPART msg4 = "";
	MSGPART msg5 = "";
 
	if (TOOK_EMPTY())
		return;
	else if (TOOK(quad) || TOOK(pent) || TOOK(ring))
		{
		TP_GetNeed();
		if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells))
			{
			msg1 = tp_sep_green;
			msg2 = " " tp_ib_name_team " $colored_powerups need %u";
			}
		else
			{ // notice we can't send this check to tp_msgenemypwr, that's because if enemy with powerup is in your view, tp_enemypwr reports enemypwr first, but in this function you want to report TEAM quad.
			msg1 = tp_sep_green;
			msg2 = " " tp_ib_name_team " $colored_powerups";
			msg3 = "";
			}
		}
	else
	{
		if	(TOOK(rl))									msg2 = tp_ib_name_rl;
		else if (TOOK(lg))								msg2 = tp_ib_name_lg;
		else if (TOOK(gl))								msg2 = tp_ib_name_gl;
		else if (TOOK(sng))								msg2 = tp_ib_name_sng;
		else if (TOOK(pack))    						msg2 = tp_ib_name_backpack;
		else if (TOOK(rockets) || TOOK(cells))			msg2 = "{$took}";
		else if (TOOK(mh))								msg2 = tp_ib_name_mh;
		else if (TOOK(ra))								msg2 = tp_ib_name_ra;
		else if (TOOK(ya))								msg2 = tp_ib_name_ya;
		else if (TOOK(ga))								msg2 = tp_ib_name_ga;
		else if (TOOK(flag))							msg2 = tp_ib_name_flag;
		else 											msg2 = "{$took}"; // This should never happen
		
		if (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
			msg4 = " $colored_short_powerups";
		else
			msg4 = "";
		
		msg1 = tp_sep_white;
		msg3 = "at $[{%l}$]";
		msg5 = " took ";
	}
	TP_Send_TeamSay("%s%s%s%s %s", msg1, msg4, msg5, msg2, msg3);
}

extern cvar_t tp_name_teammate;
GLOBAL void TP_Msg_Point_f (void)
{
    MSGPART msg1 = "";
	MSGPART msg2 = "";
	MSGPART msg3 = "";
	MSGPART msg4 = "";
 
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
				if (tp_name_teammate.string[0])
					msg2 = tp_ib_name_teammate;
				else
					msg2 = va ("{&c0b0%s&cfff}", Macro_PointName());
			}
		else if (INPOINTPOWERUP() || INPOINTWEAPON() || INPOINTARMOR() || INPOINTAMMO() || INPOINT(pack) || INPOINT(mh) || INPOINT(flag) || INPOINT (disp) || INPOINT(sentry) || INPOINT(runes)) // This should cover everything we can point to. How can I do if INPONIT(anything) or if INPOINT() not empty?
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
					else if (INPOINT(pack))		msg2 = tp_ib_name_backpack;
					
					else if (INPOINT(ra))		msg2 = tp_ib_name_ra;
					else if (INPOINT(ya))		msg2 = tp_ib_name_ya;
					else if (INPOINT(ga))		msg2 = tp_ib_name_ga;
					
					else if (INPOINT(rockets)) 	msg2 = "{$point}";
					else if (INPOINT(cells)) 	msg2 = "{$point}";
					else if (INPOINT(nails)) 	msg2 = "{$point}";
					
					else if (INPOINT(mh))		msg2 = tp_ib_name_mh;
					
					//TF
					else if (INPOINT(flag))		msg2 = tp_ib_name_flag; // need to see between blue/red colors!
					else if (INPOINT(disp))		msg2 = "{$point}";	// note we can'te tell if it's enemy or team disp
					else if (INPOINT(sentry))	msg2 = "{$point}"; // note we can'te tell if it's enemy or team sent
					
					//ctf, other
					else if (INPOINT(rune1))	msg2 = "$tp_name_rune1";
					else if (INPOINT(rune2))	msg2 = "$tp_name_rune2";
					else if (INPOINT(rune3))	msg2 = "$tp_name_rune3";
					else if (INPOINT(rune4))	msg2 = "$tp_name_rune4";
			}
		else msg2 = "{$point}"; // this should never happen
	}
	
		if (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
			msg4 = " $colored_short_powerups";
		else
			msg4 = "";
	
	//led(1) item(2) at loc(3)
	TP_Send_TeamSay("%s%s %s %s", msg1, msg4, msg2, msg3);
}


GLOBAL void TP_Msg_Need_f (void)
{
    MSGPART msg1 = "";
 
	if (DEAD())
		return; // if you're dead, you need better aim :E
	else
		TP_GetNeed();
	
	if (NEED(health) || NEED(armor) || NEED_WEAPON() || NEED(rockets) || NEED(cells) || NEED(shells) || NEED(nails))
		msg1 = "need %u";
	else
		return;
	
	TP_Send_TeamSay(tp_sep_green " %s", msg1);
}

LOCAL void TP_Msg_TrickReplace(qbool trick)
{
    MSGPART msg1 = "";
	MSGPART msg2 = "";
	MSGPART msg3 = "";
	
	if (trick)
		msg1 = " {trick}";
	else
		msg1 = " {replace}";
		
	if (HAVE_QUAD() || HAVE_PENT() || HAVE_RING())
		msg3 = " $colored_short_powerups";
	else
		msg3 = "";
		
	msg2 = "$[{%l}$]";
	
	TP_Send_TeamSay(tp_sep_yellow "%s%s %s", msg3, msg1, msg2);
}
GLOBAL void TP_Msg_Trick_f (void) { TP_Msg_TrickReplace(true); }
GLOBAL void TP_Msg_Replace_f (void) { TP_Msg_TrickReplace(false); }
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
