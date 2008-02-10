/*
Copyright (C) 2001-2002 jogihoogi

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the included (GNU.txt) GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

$Id: mvd_autotrack.c,v 1.6 2007-09-27 23:18:03 johnnycz Exp $
*/

// MultiView Demo Autotrack system
// when observing a demo or QTV playback, will switch to best players automatically

#include "quakedef.h"
#include "parser.h"
#include "localtime.h"
#include "mvd_utils_common.h"

// mvd_autotrack cvars
cvar_t mvd_autotrack = {"mvd_autotrack", "0"};
cvar_t mvd_autotrack_instant = {"mvd_autotrack_instant", "0"};
cvar_t mvd_autotrack_1on1 = {"mvd_autotrack_1on1", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_1on1_values = {"mvd_autotrack_1on1_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};
cvar_t mvd_autotrack_2on2 = {"mvd_autotrack_2on2", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_2on2_values = {"mvd_autotrack_2on2_values", "1 2 3 2 3 5 8 8 1 2 3 500 900 1000"};
cvar_t mvd_autotrack_4on4 = {"mvd_autotrack_4on4", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_4on4_values = {"mvd_autotrack_4on4_values", "1 2 4 2 4 6 10 10 1 2 3 500 900 1000"};
cvar_t mvd_autotrack_custom = {"mvd_autotrack_custom", "%a * %A + 50 * %W + %p + %f"};
cvar_t mvd_autotrack_custom_values = {"mvd_autotrack_custom_values", "1 2 3 2 3 6 6 1 2 3 500 900 1000"};
cvar_t mvd_autotrack_lockteam = {"mvd_autotrack_lockteam", "0"};

cvar_t mvd_multitrack_1 = {"mvd_multitrack_1", "%f"};
cvar_t mvd_multitrack_1_values = {"mvd_multitrack_1_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};
cvar_t mvd_multitrack_2 = {"mvd_multitrack_2", "%W"};
cvar_t mvd_multitrack_2_values = {"mvd_multitrack_2_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};
cvar_t mvd_multitrack_3 = {"mvd_multitrack_3", "%h"};
cvar_t mvd_multitrack_3_values = {"mvd_multitrack_3_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};
cvar_t mvd_multitrack_4 = {"mvd_multitrack_4", "%A"};
cvar_t mvd_multitrack_4_values = {"mvd_multitrack_4_values", "1 2 3 2 3 5 8 8 1 2 3 0 0 0"};

#define PL_VALUES_COUNT 14
static float pl_values[PL_VALUES_COUNT];
#define axe_val     (pl_values[0])
#define sg_val      (pl_values[1])
#define ssg_val     (pl_values[2])
#define ng_val      (pl_values[3])
#define sng_val     (pl_values[4])
#define gl_val      (pl_values[5])
#define rl_val      (pl_values[6])
#define lg_val      (pl_values[7])
#define ga_val      (pl_values[8])
#define ya_val      (pl_values[9])
#define ra_val      (pl_values[10])
#define ring_val    (pl_values[11])
#define quad_val    (pl_values[12])
#define pent_val    (pl_values[13])

int multitrack_id_1 = 0;
int multitrack_id_2 = 0;
int multitrack_id_3 = 0;
int multitrack_id_4 = 0;

char *multitrack_val ;
char *multitrack_str ;
int last_track = 0;

static int currentplayer_num;

static int MVD_AutoTrackBW_f(int i){
	extern cvar_t tp_weapon_order;
	int j;
	player_info_t *bp_info;

	char *t[] = {tp_weapon_order.string, "78654321", NULL}, **s;

	bp_info = cl.players;

		for (s = t; *s; s++) {
			for (j = 0 ; j < strlen(*s) ; j++) {
				switch ((*s)[j]) {
					case '1': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_AXE) return axe_val; break;
					case '2': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SHOTGUN) return sg_val; break;
					case '3': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_SHOTGUN) return ssg_val; break;
					case '4': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_NAILGUN) return ng_val; break;
					case '5': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_SUPER_NAILGUN) return sng_val; break;
					case '6': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_GRENADE_LAUNCHER) return gl_val; break;
					case '7': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_ROCKET_LAUNCHER) return rl_val; break;
					case '8': if (mvd_new_info[i].p_info->stats[STAT_ITEMS] & IT_LIGHTNING) return lg_val; break;
				}
			}
		}
	return 0;
}

static expr_val MVD_Var_Vals(const char *n)
{
	int i = currentplayer_num;
    int stats = mvd_new_info[i].p_info->stats[STAT_ITEMS];
	double bp_at, bp_pw;

	// get armortype
	if      (stats & IT_ARMOR1) bp_at = ga_val;
	else if (stats & IT_ARMOR2) bp_at = ya_val;
	else if (stats & IT_ARMOR3)	bp_at = ra_val;
	else                        bp_at = 0;

	// get powerup type
	bp_pw=0;
	if (stats & IT_QUAD)            bp_pw += quad_val;
	if (stats & IT_INVULNERABILITY)	bp_pw += pent_val;
	if (stats & IT_INVISIBILITY)    bp_pw += ring_val;

	// not implemented yet:
    // D - average quad run time
    // e - average quad run frags
    // E - average quad run teamfrags
	// u - average pent run frags
	// U - average pent run teamfrags
	// q - Axe kills
	// Q - Shotgun kills
	// r - Supershotgun kills
	// R - Nailgun kills
	// s - Supernailgun kills
	// S - Grenadelauncher kills
	// t - Rocketlauncher kills
	// T - Lightninggun kills

	switch (*n) {
    /// armor ammount
	case 'a': return Get_Expr_Double(mvd_new_info[i].p_info->stats[STAT_ARMOR]);
	/// armor type
    case 'A': return Get_Expr_Double(bp_at);
	/// current run time
    case 'c': return Get_Expr_Double(mvd_new_info[i].info.runs[mvd_new_info[i].info.run].time);
	/// current run frags
    case 'C': return Get_Expr_Integer(mvd_new_info[i].info.runs[mvd_new_info[i].info.run].frags);
	/// current run teamfrags
	case 'd': return Get_Expr_Integer(mvd_new_info[i].info.runs[mvd_new_info[i].info.run].teamfrags);

	/// frags
    case 'f': return Get_Expr_Integer(mvd_new_info[i].p_info->frags);

	/// teamfrags
    case 'F': return Get_Expr_Integer(mvd_new_info[i].info.teamfrags);
		
	/// deaths
	case 'g': return Get_Expr_Integer(mvd_new_info[i].info.das.deathcount);
	/// health
    case 'h': return Get_Expr_Integer(mvd_new_info[i].p_info->stats[STAT_HEALTH]);

	/// taken super-shotguns
	case 'I': return Get_Expr_Integer(mvd_new_info[i].info.info[SSG_INFO].count);
	/// taken nailguns
    case 'j': return Get_Expr_Integer(mvd_new_info[i].info.info[NG_INFO].count);
	/// taken super-nailguns
    case 'J': return Get_Expr_Integer(mvd_new_info[i].info.info[SNG_INFO].count);
	/// taken grenade launchers
    case 'k': return Get_Expr_Integer(mvd_new_info[i].info.info[GL_INFO].count);
	/// # of lost grenade launchers
    case 'K': return Get_Expr_Integer(mvd_new_info[i].info.info[GL_INFO].lost);
	/// # of taken rocket launchers
    case 'l': return Get_Expr_Integer(mvd_new_info[i].info.info[RL_INFO].count);
    /// # of lost rocket launchers
	case 'L': return Get_Expr_Integer(mvd_new_info[i].info.info[RL_INFO].lost);
    case 'm': return Get_Expr_Integer(mvd_new_info[i].info.info[LG_INFO].count);
    case 'M': return Get_Expr_Integer(mvd_new_info[i].info.info[LG_INFO].lost);
    case 'n': return Get_Expr_Integer(mvd_new_info[i].info.info[MH_INFO].count);
    case 'N': return Get_Expr_Integer(mvd_new_info[i].info.info[GA_INFO].count);
    case 'o': return Get_Expr_Integer(mvd_new_info[i].info.info[YA_INFO].count);
    case 'O': return Get_Expr_Integer(mvd_new_info[i].info.info[RA_INFO].count);
    case 'p': return Get_Expr_Double(bp_pw);

	// v - average run time
	case 'v': return Get_Expr_Double(mvd_new_info[i].info.run_stats.all.avg_time);
	// V - average run frags
	case 'V': return Get_Expr_Double(mvd_new_info[i].info.run_stats.all.avg_frags);
	// w - average run teamfrags
	case 'w': return Get_Expr_Double(mvd_new_info[i].info.run_stats.all.avg_teamfrags);

    case 'W': return Get_Expr_Integer(MVD_AutoTrackBW_f(i));
    case 'x': return Get_Expr_Double(mvd_new_info[i].info.info[RING_INFO].count);
    case 'X': return Get_Expr_Double(mvd_new_info[i].info.info[RING_INFO].lost);
    case 'y': return Get_Expr_Double(mvd_new_info[i].info.info[QUAD_INFO].count);
    case 'Y': return Get_Expr_Double(mvd_new_info[i].info.info[QUAD_INFO].lost);
    case 'z': return Get_Expr_Double(mvd_new_info[i].info.info[PENT_INFO].count);
	}
	return Get_Expr_Integer(0);
}

static const parser_extra mvd_pars_extra = { MVD_Var_Vals, NULL };

static void MVD_GetValuesAndEquation(const char** v, const char** e) {
#define RET(x) { *v = mvd_autotrack_##x##_values.string; *e = mvd_autotrack_##x.string; }
	if (mvd_autotrack.integer == 1) {
		if      (mvd_cg_info.gametype == 0) RET(1on1)
		else if (mvd_cg_info.gametype == 1) RET(2on2)
		else if (mvd_cg_info.gametype == 3) RET(4on4)
		else								RET(4on4)
	}
	else if (mvd_autotrack.integer == 2)	RET(custom)
	else if (mvd_autotrack.integer == 3 && cl_multiview.value) {
		*v = multitrack_val; *e = multitrack_str; 
	} else									RET(4on4)
#undef RET
}

static void MVD_UpdatePlayerValues(void)
{
	int eval_error, i;
    const char* eq;
	const char* vals;
    double value;

	MVD_GetValuesAndEquation(&vals, &eq);

    // will extract user string "1 5 8 100.4 ..." into pl_values array which is
    // later accessed via macros like ra_val, quad_val, rl_val, ...
    if (COM_GetFloatTokens(vals, pl_values, PL_VALUES_COUNT) != PL_VALUES_COUNT)
    {
	    Com_Printf("mvd_autotrack aborting due to wrong use of mvd_autotrack_*_value\n");
		Cvar_SetValue(&mvd_autotrack,0);
		return;
    }

	for ( i=0; i<mvd_cg_info.pcount ; i++ )
    {
		// store global variable which is used in MVD_Var_Vals
		currentplayer_num = i;
		eval_error = Expr_Eval_Double(eq, &mvd_pars_extra, &value);

		if (eval_error != EXPR_EVAL_SUCCESS) {
			Com_Printf("Expression evaluation error: %s\n", Parser_Error_Description(eval_error));
			Cvar_SetValue(&mvd_autotrack,0);
			return;
		}

		mvd_new_info[i].value = value;
	}
}

static int MVD_GetBestPlayer(void)
{
	int initial, i, bp_id;
	float bp_val;

	if (last_track < 0 || last_track > mvd_cg_info.pcount)
		initial = 0;
	else initial = last_track;

	bp_val = mvd_new_info[initial].value;
	bp_id = mvd_new_info[initial].id;
	for ( i=0 ; i<mvd_cg_info.pcount ; i++ ) {
		if (bp_val < mvd_new_info[i].value) {
			if (mvd_autotrack_lockteam.integer && strcmp(mvd_new_info[i].p_info->team, cl.players[cl.viewplayernum].team))
				continue;

			bp_val = mvd_new_info[i].value;
			bp_id 	= mvd_new_info[i].id;
		}
	}
	return bp_id ;
}

static int MVD_FindBestPlayer_f(void) {
	MVD_UpdatePlayerValues();
	return MVD_GetBestPlayer();
}

#define HASRL(x) (x & IT_ROCKET_LAUNCHER)
#define HASPENT(x) (x & IT_INVULNERABILITY)
#define HASQUAD(x) (x & IT_QUAD)
#define HASWEAPON(x) ((x & IT_ROCKET_LAUNCHER ) || (x & IT_LIGHTNING))
int MVD_GetBetterPlayerSimple(int a, int b)
{
	int sa = cl.players[a].stats[STAT_ITEMS];
	int sb = cl.players[b].stats[STAT_ITEMS];

	if (HASPENT(sa) && HASRL(sa)) return a;
	if (HASPENT(sb) && HASRL(sb)) return b;
	if (HASQUAD(sa) && HASWEAPON(sa)) return a;
	if (HASQUAD(sb) && HASWEAPON(sb)) return b;
	if (HASPENT(sa)) return a;
	if (HASPENT(sb)) return b;
	if (HASQUAD(sa)) return a;
	if (HASQUAD(sb)) return b;
	if (HASWEAPON(sa)) return a;
	if (HASWEAPON(sb)) return b;

	return a;
}

static int MVD_FindBestPlayerSimple(void) {
	int i, b;

	b = cl.viewplayernum;
	for (i = 0; i < mvd_cg_info.pcount; i++) {
		if (mvd_autotrack_lockteam.integer && strcmp(mvd_new_info[i].p_info->team, cl.players[cl.viewplayernum].team))
			continue;
		b = MVD_GetBetterPlayerSimple(b, mvd_new_info[i].id);
	}

	return b;
}

static qbool MVD_TrackedHasNoWeapon() {
	int stat = cl.players[cl.viewplayernum].stats[STAT_ITEMS];
	if ((stat & IT_ROCKET_LAUNCHER) || (stat & IT_LIGHTNING)) return false;
	else return true;
}

static qbool MVD_SomeoneHasWeapon() {
	int i, stats;
	for (i = 0; i < mvd_cg_info.pcount; i++) {
		stats = mvd_new_info[i].p_info->stats[STAT_ITEMS];
		if ((stats & IT_ROCKET_LAUNCHER) || (stats & IT_LIGHTNING)) return true;
	}
	return false;
}

static qbool MVD_TrackedHasNoPowerup() {
	int stats = cl.players[cl.viewplayernum].stats[STAT_ITEMS];
	if ((stats & IT_QUAD) || (stats & IT_INVULNERABILITY)) return false;
	else return true;
}

static qbool MVD_SomeoneHasPowerup() {
	int i, stats;
	for (i = 0; i < mvd_cg_info.pcount; i++) {
		stats = mvd_new_info[i].p_info->stats[STAT_ITEMS];
		if ((stats & IT_QUAD) || (stats & IT_INVULNERABILITY)) return true;
	}
	return false;
}

static qbool MVD_SomeoneHasPentWithRL(void) {
	int i, stats;
	for (i = 0; i < mvd_cg_info.pcount; i++) {
		stats = mvd_new_info[i].p_info->stats[STAT_ITEMS];
		if ((stats & IT_ROCKET_LAUNCHER) && (stats & IT_INVULNERABILITY)) return true;
	}
	return false;
}

static qbool MVD_SwitchMoment(void) {
	if (MVD_TrackedHasNoWeapon() && MVD_SomeoneHasWeapon()) return true;
	else if (MVD_TrackedHasNoPowerup() && MVD_SomeoneHasPowerup()) return true;
	else if (MVD_SomeoneHasPentWithRL()) return true;
	else return false;
}



void MVD_AutoTrack_f(void) {
	char arg[64];
	int id;
	static double lastupdate = 0;

	#ifdef DEBUG
	printf("MVD_AutoTrack_f Started\n");
	#endif

	if (!mvd_autotrack.value)
		return;

	if (cl.standby || cl.countdown)
		return;

	// no need to recalculate the values in every frame
	if (cls.realtime - lastupdate < 0.5)
		return;

	lastupdate = cls.realtime;

	if (mvd_autotrack.value == 3 && cl_multiview.value > 0){
		if (cl_multiview.value >= 1 ){
			multitrack_str = mvd_multitrack_1.string;
			multitrack_val = mvd_multitrack_1_values.string;
			id = MVD_FindBestPlayer_f();
			if (id != multitrack_id_1){
				snprintf(arg, sizeof (arg), "track1 %i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_1 = id;
			}
		}
		if (cl_multiview.value >= 2 ){
			multitrack_str = mvd_multitrack_2.string;
			multitrack_val = mvd_multitrack_2_values.string;
			id = MVD_FindBestPlayer_f();
			if (id != multitrack_id_2){
				snprintf(arg, sizeof (arg), "track2 %i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_2 = id;
			}
		}
		if (cl_multiview.value >= 3 ){
			multitrack_str = mvd_multitrack_3.string;
			multitrack_val = mvd_multitrack_3_values.string;
			id = MVD_FindBestPlayer_f();
			if (id != multitrack_id_3){
				snprintf(arg, sizeof (arg), "track3 %i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_3 = id;
			}
		}
		if (cl_multiview.value >= 4 ){
			multitrack_str = mvd_multitrack_4.string;
	 		multitrack_val = mvd_multitrack_4_values.string;
			id = MVD_FindBestPlayer_f();
			if (id != multitrack_id_4){
				snprintf(arg, sizeof (arg), "track4 %i \n",id);
				Cbuf_AddText(arg);
				multitrack_id_4 = id;
			}
		}
	}

	else if (mvd_autotrack_instant.integer || MVD_SwitchMoment())// mvd_autotrack is 1 or 2 or 3
	{
		if (mvd_autotrack.integer == 4) 
			id = MVD_FindBestPlayerSimple();
		else
			id = MVD_FindBestPlayer_f();

		if (id != last_track || cl.viewplayernum != id) {
			snprintf(arg, sizeof (arg), "track \"%s\"\n",cl.players[id].name);
			Cbuf_AddText(arg);
			last_track = id;
		}
	}
	#ifdef DEBUG
	printf("MVD_AutoTrack_f Stopped\n");
	#endif
}

void MVD_AutoTrack_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_MVD);
	Cvar_Register (&mvd_autotrack);
	Cvar_Register (&mvd_autotrack_instant);
	Cvar_Register (&mvd_autotrack_1on1);
	Cvar_Register (&mvd_autotrack_1on1_values);
	Cvar_Register (&mvd_autotrack_2on2);
	Cvar_Register (&mvd_autotrack_2on2_values);
	Cvar_Register (&mvd_autotrack_4on4);
	Cvar_Register (&mvd_autotrack_4on4_values);
	Cvar_Register (&mvd_autotrack_custom);
	Cvar_Register (&mvd_autotrack_custom_values);
	Cvar_Register (&mvd_autotrack_lockteam);

	Cvar_Register (&mvd_multitrack_1);
	Cvar_Register (&mvd_multitrack_1_values);
	Cvar_Register (&mvd_multitrack_2);
	Cvar_Register (&mvd_multitrack_2_values);
	Cvar_Register (&mvd_multitrack_3);
	Cvar_Register (&mvd_multitrack_3_values);
	Cvar_Register (&mvd_multitrack_4);
	Cvar_Register (&mvd_multitrack_4_values);
	Cvar_ResetCurrentGroup();
}
