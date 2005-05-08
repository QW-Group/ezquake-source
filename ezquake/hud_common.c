//
// common HUD elements
// like clock etc..
//

#include "quakedef.h"
#include "hud.h"
#include "hud_common.h"
#include "common_draw.h"
#include "sbar.h"
#include "EX_misc.h"
#include "vx_stuff.h"
#ifndef STAT_MINUS
#define STAT_MINUS		10
#endif

hud_t *hud_netgraph;

// ----------------
// HUD planning
//

cvar_t hud_planmode = {"hud_planmode",   "0"};
int hud_stats[MAX_CL_STATS];

int HUD_Stats(int stat_num)
{
    if (hud_planmode.value)
        return hud_stats[stat_num];
    else
        return cl.stats[stat_num];
}

// ----------------
// HUD low levels
//

cvar_t hud_tp_need = {"hud_tp_need",   "0"};

/* tp need levels
int TP_IsHealthLow(void);
int TP_IsArmorLow(void);
int TP_IsAmmoLow(int weapon); */
extern cvar_t tp_need_health, tp_need_ra, tp_need_ya, tp_need_ga,
		tp_weapon_order, tp_need_weapon, tp_need_shells,
		tp_need_nails, tp_need_rockets, tp_need_cells;
int State_AmmoForWeapon(int weapon)
{
    switch (weapon)
    {
        case 2:
        case 3: return cl.stats[STAT_SHELLS];
        case 4:
        case 5: return cl.stats[STAT_NAILS];
        case 6:
        case 7: return cl.stats[STAT_ROCKETS];
        case 8: return cl.stats[STAT_CELLS];
        default: return 0;
    }
}
int TP_IsHealthLow(void)
{
    return cl.stats[STAT_HEALTH] <= tp_need_health.value;
}

int TP_IsArmorLow(void)
{
    if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR3))
        return cl.stats[STAT_ARMOR] <= tp_need_ra.value;
    if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR2))
        return cl.stats[STAT_ARMOR] <= tp_need_ya.value;
    if ((cl.stats[STAT_ARMOR] > 0) && (cl.stats[STAT_ITEMS] & IT_ARMOR1))
        return cl.stats[STAT_ARMOR] <= tp_need_ga.value;
    return 1;
}

int TP_IsWeaponLow(void)
{
    char *s = tp_weapon_order.string;
    while (*s  &&  *s != tp_need_weapon.string[0])
    {
        if (cl.stats[STAT_ITEMS] & (IT_SHOTGUN << (*s-'0'-2)))
            return false;
        s++;
    }
    return true;
}

int TP_IsAmmoLow(int weapon)
{
    int ammo = State_AmmoForWeapon(weapon);
    switch (weapon)
    {
    case 2:
    case 3:  return ammo <= tp_need_shells.value;
    case 4:
    case 5:  return ammo <= tp_need_nails.value;
    case 6:
    case 7:  return ammo <= tp_need_rockets.value;
    case 8:  return ammo <= tp_need_cells.value;
    default: return 0;
    }
}

qboolean HUD_HealthLow(void)
{
    if (hud_tp_need.value)
        return TP_IsHealthLow();
    else
        return HUD_Stats(STAT_HEALTH) <= 25;
}

qboolean HUD_ArmorLow(void)
{
    if (hud_tp_need.value)
        return (TP_IsArmorLow());
    else
        return (HUD_Stats(STAT_ARMOR) <= 25);
}

qboolean HUD_AmmoLow(void)
{
    if (hud_tp_need.value)
    {
        if (HUD_Stats(STAT_ITEMS) & IT_SHELLS)
            return TP_IsAmmoLow(2);
        else if (HUD_Stats(STAT_ITEMS) & IT_NAILS)
            return TP_IsAmmoLow(4);
        else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS)
            return TP_IsAmmoLow(6);
        else if (HUD_Stats(STAT_ITEMS) & IT_CELLS)
            return TP_IsAmmoLow(8);
        return false;
    }
    else
        return (HUD_Stats(STAT_AMMO) <= 10);
}

int HUD_AmmoLowByWeapon(int weapon)
{
    if (hud_tp_need.value)
        return TP_IsAmmoLow(weapon);
    else
    {
        int a;
        switch (weapon)
        {
        case 2:
        case 3:
            a = STAT_SHELLS; break;
        case 4:
        case 5:
            a = STAT_NAILS; break;
        case 6:
        case 7:
            a = STAT_ROCKETS; break;
        case 8:
            a = STAT_CELLS; break;
        default:
            return false;
        }
        return (HUD_Stats(a) <= 10);
    }
}

// ----------------
// DrawFPS
void SCR_HUD_DrawFPS(hud_t *hud)
{
    int x, y, width, height;
    char st[80];

    static cvar_t
        *hud_fps_show_min = NULL,
        *hud_fps_title;

    if (hud_fps_show_min == NULL)   // first time called
    {
        hud_fps_show_min = HUD_FindVar(hud, "show_min");
        hud_fps_title    = HUD_FindVar(hud, "title");
    }


    if (hud_fps_show_min->value)
        sprintf(st, "%3.1f\xf%3.1f", cls.min_fps + 0.05, cls.fps + 0.05);
    else
        sprintf(st, "%3.1f", cls.fps + 0.05);

    if (hud_fps_title->value)
        strcat(st, " fps");

    width = 8*strlen(st);
    height = 8;

    if (HUD_PrepareDraw(hud, strlen(st)*8, 8, &x, &y))
        Draw_String(x, y, st);
}
void SCR_HUD_DrawTracking(hud_t *hud)
{
    int x, y, width, height;
    char st[512];
	static cvar_t
		*hud_tracking_alttext;
	hud_tracking_alttext    = HUD_FindVar(hud, "alttext");
	if (strlen(hud_tracking_alttext->string) == 0) {
		Q_snprintfz(st, sizeof(st), "Tracking %-.13s", cl.players[spec_track].name);
		if (!cls.demoplayback)
			strcat (st, ", [JUMP] for next");
	} else {
		strcpy(st, hud_tracking_alttext->string);
	}
	width = 8*strlen(st);
    height = 8;
    if (cl.spectator && autocam == CAM_TRACK && HUD_PrepareDraw(hud, strlen(st)*8, 8, &x, &y))
        Draw_String(x, y, st);
}

void R_MQW_NetGraph(int outgoing_sequence, int incoming_sequence, int *packet_latency,
                int lost, int minping, int avgping, int maxping, int devping,
                int posx, int posy, int width, int height, int revx, int revy);
// ----------------
// Netgraph
void SCR_HUD_Netgraph(hud_t *hud)
{
    static cvar_t
        *par_width = NULL, *par_height,
        *par_swap_x, *par_swap_y,
        *par_ploss;

    if (par_width == NULL)  // first time
    {
        par_width  = HUD_FindVar(hud, "width");
        par_height = HUD_FindVar(hud, "height");
        par_swap_x = HUD_FindVar(hud, "swap_x");
        par_swap_y = HUD_FindVar(hud, "swap_y");
        par_ploss  = HUD_FindVar(hud, "ploss");
    }

    R_MQW_NetGraph(cls.netchan.outgoing_sequence, cls.netchan.incoming_sequence,
        packet_latency, par_ploss->value ? CL_CalcNet() : -1, -1, -1, -1, -1, -1,
        -1, (int)par_width->value, (int)par_height->value,
        (int)par_swap_x->value, (int)par_swap_y->value);
}

//---------------------
//
// draw HUD ping
//
void SCR_HUD_DrawPing(hud_t *hud)
{
    double t;
    static double last_calculated;
    static int ping_avg, pl, ping_min, ping_max;
    static float ping_dev;

    int width, height;
    int x, y;
    char buf[512];

    static cvar_t
        *hud_ping_period = NULL,
        *hud_ping_show_pl,
        *hud_ping_show_dev,
        *hud_ping_show_min,
        *hud_ping_show_max,
        *hud_ping_blink;

    if (hud_ping_period == NULL)    // first time
    {
        hud_ping_period   = HUD_FindVar(hud, "period");
        hud_ping_show_pl  = HUD_FindVar(hud, "show_pl");
        hud_ping_show_dev = HUD_FindVar(hud, "show_dev");
        hud_ping_show_min = HUD_FindVar(hud, "show_min");
        hud_ping_show_max = HUD_FindVar(hud, "show_max");
        hud_ping_blink    = HUD_FindVar(hud, "blink");
    }

    t = Sys_DoubleTime();
    if (t - last_calculated  >  hud_ping_period->value)
    {
        // recalculate

        net_stat_result_t result;
        float period;

        last_calculated = t;

        period = max(hud_ping_period->value, 0);

        CL_CalcNetStatistics(
            period,             // period of time
            network_stats,      // samples table
            NETWORK_STATS_SIZE, // number of samples in table
            &result);           // results

        if (result.samples == 0)
            return; // error calculating net

        ping_avg = (int)(result.ping_avg + 0.5);
        ping_min = (int)(result.ping_min + 0.5);
        ping_max = (int)(result.ping_max + 0.5);
        ping_dev = result.ping_dev;
        pl = result.lost_lost;

        clamp(ping_avg, 0, 999);
        clamp(ping_min, 0, 999);
        clamp(ping_max, 0, 999);
        clamp(ping_dev, 0, 99.9);
        clamp(pl, 0, 100);
    }

    buf[0] = 0;

    // blink
    if (hud_ping_blink->value)   // add dot
        strcat(buf, (last_calculated + hud_ping_period->value/2 > cls.realtime) ? "\x8f" : " ");

    // min ping
    if (hud_ping_show_min->value)
        strcat(buf, va("%d\xf", ping_min));

    // ping
    strcat(buf, va("%d", ping_avg));

    // max ping
    if (hud_ping_show_max->value)
        strcat(buf, va("\xf%d", ping_max));

    // unit
    strcat(buf, " ms");

    // standard deviation
    if (hud_ping_show_dev->value)
        strcat(buf, va(" (%.1f)", ping_dev));

    // pl
    if (hud_ping_show_pl->value)
        strcat(buf, va(" \x8f %d%%", pl));

    // display that on screen
    width = strlen(buf) * 8;
    height = 8;

    if (HUD_PrepareDraw(hud, width, height, &x, &y))
        Draw_String(x, y, buf);
}

//---------------------
//
// draw HUD clock
//
void SCR_HUD_DrawClock(hud_t *hud)
{
    int width, height;
    int x, y;

    static cvar_t
        *hud_clock_big = NULL,
        *hud_clock_style,
        *hud_clock_blink;

    if (hud_clock_big == NULL)    // first time
    {
        hud_clock_big   = HUD_FindVar(hud, "big");
        hud_clock_style = HUD_FindVar(hud, "style");
        hud_clock_blink = HUD_FindVar(hud, "blink");
    }

    if (hud_clock_big->value)
    {
        width = 24+24+16+24+24;
        height = 24;
    }
    else
    {
        width = 5*8;
        height = 8;
    }

    if (HUD_PrepareDraw(hud, width, height, &x, &y))
    {
        if (hud_clock_big->value)
            SCR_DrawBigClock(x, y, hud_clock_style->value, hud_clock_blink->value);
        else
            SCR_DrawSmallClock(x, y, hud_clock_style->value, hud_clock_blink->value);
    }
}

//---------------------
//
// network statistics
//
void SCR_HUD_DrawNetStats(hud_t *hud)
{
    int width, height;
    int x, y;

    static cvar_t *hud_net_period = NULL;

    if (hud_net_period == NULL)    // first time
    {
        hud_net_period = HUD_FindVar(hud, "period");
    }

    width = 16*8 ;
    height = 12 + 8 + 8 + 8 + 8 + 16 + 8 + 8 + 8 + 8 + 16 + 8 + 8 + 8;

    if (HUD_PrepareDraw(hud, width, height, &x, &y))
        SCR_NetStats(x, y, hud_net_period->value);
}

#define SPEED_WIDTH 160
//---------------------
//
// speed-o-meter
//
void SCR_HUD_DrawSpeed(hud_t *hud)
{
    int width, height;
    int x, y;

    static cvar_t *hud_speed_xyz = NULL;

    if (hud_speed_xyz == NULL)    // first time
    {
        hud_speed_xyz = HUD_FindVar(hud, "xyz");
    }

    width = SPEED_WIDTH;
    height = 15;

    if (HUD_PrepareDraw(hud, width, height, &x, &y))
        SCR_DrawSpeed2(x, y+3, hud_speed_xyz->value);
}


// =======================================================
//
//  s t a t u s   b a r   e l e m e n t s
//
//


// -----------
// gunz
//
void SCR_HUD_DrawGunByNum (hud_t *hud, int num, float scale, int style, int wide)
{
    extern mpic_t *sb_weapons[7][8];  // sbar.c
    int i = num - 2;
    int width, height;
    int x, y;
    char *tmp;

    scale = max(scale, 0.01);

    switch (style)
    {
    case 1:     // text
        width = 16 * scale;
        height = 8 * scale;
        if (!HUD_PrepareDraw(hud, width, height, &x, &y))
            return;
        if ( HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN<<i) )
        {
            switch (num)
            {
            case 2: tmp = "sg"; break;
            case 3: tmp = "bs"; break;
            case 4: tmp = "ng"; break;
            case 5: tmp = "sn"; break;
            case 6: tmp = "gl"; break;
            case 7: tmp = "rl"; break;
            case 8: tmp = "lg"; break;
            default: tmp = "";
            }

            if ( HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN<<i) )
                Draw_SString(x, y, tmp, scale);
            else
                Draw_SAlt_String(x, y, tmp, scale);
        }
        break;
    case 2:     // numbers
        width = 8 * scale;
        height = 8 * scale;
        if (!HUD_PrepareDraw(hud, width, height, &x, &y))
            return;
        if ( HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN<<i) )
        {
            if ( HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN<<i) )
                num += '0';
            else
                num += '0' + 128;
            Draw_SCharacter(x, y, num, scale);
        }
        break;
    default:    // classic - pictures
        width  = scale * (wide ? 48 : 24);
        height = scale * 16;

        if (!HUD_PrepareDraw(hud, width, height, &x, &y))
            return;

        if ( HUD_Stats(STAT_ITEMS) & (IT_SHOTGUN<<i) )
        {
            float   time;
            int     flashon;

            time = cl.item_gettime[i];
            flashon = (int)((cl.time - time)*10);
            if (flashon < 0)
                flashon = 0;
            if (flashon >= 10)
            {
                if ( HUD_Stats(STAT_ACTIVEWEAPON) == (IT_SHOTGUN<<i) )
                    flashon = 1;
                else
                    flashon = 0;
            }
            else
                flashon = (flashon%5) + 2;

            if (wide  ||  num != 8)
                Draw_SPic (x, y, sb_weapons[flashon][i], scale);
            else
                Draw_SSubPic (x, y, sb_weapons[flashon][i], 0, 0, 24, 16, scale);
        }
        break;
    }
}

void SCR_HUD_DrawGun2 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time callse
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawGunByNum (hud, 2, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun3 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawGunByNum (hud, 3, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun4 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawGunByNum (hud, 4, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun5 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawGunByNum (hud, 5, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun6 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawGunByNum (hud, 6, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun7 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawGunByNum (hud, 7, scale->value, style->value, 0);
}
void SCR_HUD_DrawGun8 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style, *wide;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
        wide  = HUD_FindVar(hud, "wide");
    }
    SCR_HUD_DrawGunByNum (hud, 8, scale->value, style->value, wide->value);
}
void SCR_HUD_DrawGunCurrent (hud_t *hud)
{
    int gun;
    static cvar_t *scale = NULL, *style, *wide;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
        wide  = HUD_FindVar(hud, "wide");
    }

    switch (HUD_Stats(STAT_ACTIVEWEAPON))
    {
    case IT_SHOTGUN << 0:   gun = 2; break;
    case IT_SHOTGUN << 1:   gun = 3; break;
    case IT_SHOTGUN << 2:   gun = 4; break;
    case IT_SHOTGUN << 3:   gun = 5; break;
    case IT_SHOTGUN << 4:   gun = 6; break;
    case IT_SHOTGUN << 5:   gun = 7; break;
    case IT_SHOTGUN << 6:   gun = 8; break;
    default: return;
    }
    SCR_HUD_DrawGunByNum (hud, gun, scale->value, style->value, wide->value);
}

// ----------------
// powerzz
//
void SCR_HUD_DrawPowerup(hud_t *hud, int num, float scale, int style)
{
    extern mpic_t *sb_items[32];
    int    x, y, width, height;
    int    c;

    scale = max(scale, 0.01);

    switch (style)
    {
    case 1:     // letter
        width = height = 8 * scale;
        if (!HUD_PrepareDraw(hud, width, height, &x, &y))
            return;
        if (HUD_Stats(STAT_ITEMS) & (1<<(17+num)))
        {
            switch (num)
            {
            case 0: c = '1'; break;
            case 1: c = '2'; break;
            case 2: c = 'r'; break;
            case 3: c = 'p'; break;
            case 4: c = 's'; break;
            case 5: c = 'q'; break;
            default: c = '?';
            }
            Draw_SCharacter(x, y, c, scale);
        }
        break;  
    default:    // classic - pics
        width = height = scale * 16;
        if (!HUD_PrepareDraw(hud, width, height, &x, &y))
            return;
        if (HUD_Stats(STAT_ITEMS) & (1<<(17+num)))
            Draw_SPic (x, y, sb_items[num], scale);
        break;
    }
}

void SCR_HUD_DrawKey1(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawPowerup(hud, 0, scale->value, style->value);
}
void SCR_HUD_DrawKey2(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawPowerup(hud, 1, scale->value, style->value);
}
void SCR_HUD_DrawRing(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawPowerup(hud, 2, scale->value, style->value);
}
void SCR_HUD_DrawPent(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawPowerup(hud, 3, scale->value, style->value);
}
void SCR_HUD_DrawSuit(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawPowerup(hud, 4, scale->value, style->value);
}
void SCR_HUD_DrawQuad(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawPowerup(hud, 5, scale->value, style->value);
}

// -----------
// sigils
//
void SCR_HUD_DrawSigil(hud_t *hud, int num, float scale, int style)
{
    extern mpic_t *sb_sigil[4];
    int     x, y;

    scale = max(scale, 0.01);

    switch (style)
    {
    case 1:     // sigil number
        if (!HUD_PrepareDraw(hud, 8*scale, 8*scale, &x, &y))
            return;
        if (HUD_Stats(STAT_ITEMS) & (1<<(28+num)))
            Draw_SCharacter(x, y, num + '0', scale);
        break;
    default:    // classic - picture
        if (!HUD_PrepareDraw(hud, 8*scale, 16*scale, &x, &y))
            return;
        if (HUD_Stats(STAT_ITEMS) & (1<<(28+num)))
            Draw_SPic(x, y, sb_sigil[num], scale);
        break;
    }
}

void SCR_HUD_DrawSigil1(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawSigil(hud, 0, scale->value, style->value);
}
void SCR_HUD_DrawSigil2(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawSigil(hud, 1, scale->value, style->value);
}
void SCR_HUD_DrawSigil3(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawSigil(hud, 2, scale->value, style->value);
}
void SCR_HUD_DrawSigil4(hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawSigil(hud, 3, scale->value, style->value);
}

// icons - active ammo, armor, face etc..
void SCR_HUD_DrawAmmoIcon(hud_t *hud, int num, float scale, int style)
{
    extern mpic_t *sb_ammo[4];
    int   x, y, width, height;

    scale = max(scale, 0.01);

    width = height = (style ? 8 : 24) * scale;

    if (!HUD_PrepareDraw(hud, width, height, &x, &y))
        return;

    if (style)
    {
        switch (num)
        {
        case 1: Draw_SAlt_String(x, y, "s", scale); break;
        case 2: Draw_SAlt_String(x, y, "n", scale); break;
        case 3: Draw_SAlt_String(x, y, "r", scale); break;
        case 4: Draw_SAlt_String(x, y, "c", scale); break;
        }
    }
    else
    {
        Draw_SPic (x, y, sb_ammo[num-1], scale);
    }
}
void SCR_HUD_DrawAmmoIconCurrent (hud_t *hud)
{
    int num;
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    if (HUD_Stats(STAT_ITEMS) & IT_SHELLS)
        num = 1;
    else if (HUD_Stats(STAT_ITEMS) & IT_NAILS)
        num = 2;
    else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS)
        num = 3;
    else if (HUD_Stats(STAT_ITEMS) & IT_CELLS)
        num = 4;
    else
        return;
    SCR_HUD_DrawAmmoIcon(hud, num, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon1 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawAmmoIcon(hud, 1, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon2 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawAmmoIcon(hud, 2, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon3 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawAmmoIcon(hud, 3, scale->value, style->value);
}
void SCR_HUD_DrawAmmoIcon4 (hud_t *hud)
{
    static cvar_t *scale = NULL, *style;
    if (scale == NULL)  // first time called
    {
        scale = HUD_FindVar(hud, "scale");
        style = HUD_FindVar(hud, "style");
    }
    SCR_HUD_DrawAmmoIcon(hud, 4, scale->value, style->value);
}

void SCR_HUD_DrawArmorIcon(hud_t *hud)
{
    extern mpic_t  *sb_armor[3];
    extern mpic_t  *draw_disc;
    int   x, y, width, height;

    int style;
    float scale;

    static cvar_t *v_scale = NULL, *v_style;
    if (v_scale == NULL)  // first time called
    {
        v_scale = HUD_FindVar(hud, "scale");
        v_style = HUD_FindVar(hud, "style");
    }

    scale = max(v_scale->value, 0.01);
    style = (int)(v_style->value);

    width = height = (style ? 8 : 24) * scale;

    if (!HUD_PrepareDraw(hud, width, height, &x, &y))
        return;

    if (style)
    {
        int c;

        if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY)
            c = '@';
        else  if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3)
            c = 'r';
        else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2)
            c = 'y';
        else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1)
            c = 'g';
        else return;

        c += 128;

        Draw_SCharacter(x, y, c, scale);
    }
    else
    {
        mpic_t  *pic;

        if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY)
            pic = draw_disc;
        else  if (HUD_Stats(STAT_ITEMS) & IT_ARMOR3)
            pic = sb_armor[2];
        else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR2)
            pic = sb_armor[1];
        else if (HUD_Stats(STAT_ITEMS) & IT_ARMOR1)
            pic = sb_armor[0];
        else return;

        Draw_SPic (x, y, pic, scale);
    }
}

// face
void SCR_HUD_DrawFace(hud_t *hud)
{
    extern mpic_t  *sb_faces[7][2]; // 0 is gibbed, 1 is dead, 2-6 are alive
                                    // 0 is static, 1 is temporary animation
    extern mpic_t  *sb_face_invis;
    extern mpic_t  *sb_face_quad;
    extern mpic_t  *sb_face_invuln;
    extern mpic_t  *sb_face_invis_invuln;

    int     f, anim;
    int     x, y;
    float   scale;

    static cvar_t *v_scale = NULL;
    if (v_scale == NULL)  // first time called
    {
        v_scale = HUD_FindVar(hud, "scale");
    }

    scale = max(v_scale->value, 0.01);

    if (!HUD_PrepareDraw(hud, 24*scale, 24*scale, &x, &y))
        return;

    if ( (HUD_Stats(STAT_ITEMS) & (IT_INVISIBILITY | IT_INVULNERABILITY) )
    == (IT_INVISIBILITY | IT_INVULNERABILITY) )
    {
        Draw_SPic (x, y, sb_face_invis_invuln, scale);
        return;
    }
    if (HUD_Stats(STAT_ITEMS) & IT_QUAD) 
    {
        Draw_SPic (x, y, sb_face_quad, scale);
        return;
    }
    if (HUD_Stats(STAT_ITEMS) & IT_INVISIBILITY) 
    {
        Draw_SPic (x, y, sb_face_invis, scale);
        return;
    }
    if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY) 
    {
        Draw_SPic (x, y, sb_face_invuln, scale);
        return;
    }

    if (HUD_Stats(STAT_HEALTH) >= 100)
        f = 4;
    else
        f = max(0, HUD_Stats(STAT_HEALTH)) / 20;
    
    if (cl.time <= cl.faceanimtime)
        anim = 1;
    else
        anim = 0;
    Draw_SPic (x, y, sb_faces[f][anim], scale);
}


// status numbers
void SCR_HUD_DrawNum(hud_t *hud, int num, qboolean low,
                     float scale, int style, int digits, char *s_align)
{
    extern mpic_t *sb_nums[2][11];

    int  i;
    char buf[8], *t;
    int  len;

    int width, height, x, y;
    int size;
    int align;

    clamp(num, -9999, 99999);

    scale = max(scale, 0.01);

    clamp(digits, 1, 5);

    align = 2;
    switch (tolower(s_align[0]))
    {
    default:
    case 'l':   // 'l'eft
        align = 0; break;
    case 'c':   // 'c'enter
        align = 1; break;
    case 'r':   // 'r'ight
        align = 2; break;
    }

    sprintf(buf, "%d", num);
    len = strlen(buf);
    if (len > digits)
    {
        t = buf;
        for (i=0; i < digits; i++)
            *t++ = '9';
        *t = 0;
        len = strlen(buf);
    }

    switch (style)
    {
    case 1:
        size = 8;
        break;
    default:
        size = 24;
        break;
    }

    width = digits * size;
    height = size;

    switch (style)
    {
    case 1:
        if (!HUD_PrepareDraw(hud, scale*width, scale*height, &x, &y))
            return;
        switch (align)
        {
        case 0: break;
        case 1: x += scale * (width - size * len) / 2; break;
        case 2: x += scale * (width - size * len); break;
        }
        if (low)
            Draw_SAlt_String(x, y, buf, scale);
        else
            Draw_SString(x, y, buf, scale);
        break;
    default:
        if (!HUD_PrepareDraw(hud, scale*width, scale*height, &x, &y))
            return;
        switch (align)
        {
        case 0: break;
        case 1: x += scale * (width - size * len) / 2; break;
        case 2: x += scale * (width - size * len); break;
        }
        for (i=0; i < len; i++)
        {
            if (low)
				if(buf[i] == '-') {
					Draw_STransPic (x, y, sb_nums[1][STAT_MINUS], scale);
				} else {
					Draw_STransPic (x, y, sb_nums[1][buf[i] - '0'], scale);
				}
            else
                Draw_STransPic (x, y, sb_nums[0][buf[i] - '0'], scale);
            x += 24 * scale;
        }
        break;
    }
}

void SCR_HUD_DrawHealth(hud_t *hud)
{
    static cvar_t *scale = NULL, *style, *digits, *align;
	static int value;
    if (scale == NULL)  // first time called
    {
        scale  = HUD_FindVar(hud, "scale");
        style  = HUD_FindVar(hud, "style");
        digits = HUD_FindVar(hud, "digits");
        align  = HUD_FindVar(hud, "align");
    }
	value = HUD_Stats(STAT_HEALTH);
    SCR_HUD_DrawNum(hud, (value < 0 ? 0 : value), HUD_HealthLow(),
        scale->value, style->value, digits->value, align->string);
}

void SCR_HUD_DrawArmor(hud_t *hud)
{
    int level;
    qboolean low;
    static cvar_t *scale = NULL, *style, *digits, *align;
    if (scale == NULL)  // first time called
    {
        scale  = HUD_FindVar(hud, "scale");
        style  = HUD_FindVar(hud, "style");
        digits = HUD_FindVar(hud, "digits");
        align  = HUD_FindVar(hud, "align");
    }

    if (HUD_Stats(STAT_HEALTH) > 0)
    {
        if (HUD_Stats(STAT_ITEMS) & IT_INVULNERABILITY)
        {
            level = 666;
            low = true;
        }
        else
        {
            level = HUD_Stats(STAT_ARMOR);
            low = HUD_ArmorLow();
        }
    }
    else
    {
        level = 0;
        low = true;
    }

    SCR_HUD_DrawNum(hud, level, low,
        scale->value, style->value, digits->value, align->string);
}

#ifdef GLQUAKE
void SCR_HUD_DrawHealthDamage(hud_t *hud)
{
	Draw_AMFStatLoss (STAT_HEALTH, hud);
	if (HUD_Stats(STAT_HEALTH) <= 0)
	{
		Amf_Reset_DamageStats();
	}
}

void SCR_HUD_DrawArmorDamage(hud_t *hud)
{
	Draw_AMFStatLoss (STAT_ARMOR, hud);
}
#endif

void SCR_HUD_DrawAmmo(hud_t *hud, int num,
                      float scale, int style, int digits, char *s_align)
{
    extern mpic_t *sb_ibar;
    int value;
    qboolean low;

    if (num < 1  ||  num > 4)
    {
        if (HUD_Stats(STAT_ITEMS) & IT_SHELLS)
            num = 1;
        else if (HUD_Stats(STAT_ITEMS) & IT_NAILS)
            num = 2;
        else if (HUD_Stats(STAT_ITEMS) & IT_ROCKETS)
            num = 3;
        else if (HUD_Stats(STAT_ITEMS) & IT_CELLS)
            num = 4;
        else
            return;
    }
    low = HUD_AmmoLowByWeapon(num * 2);
    value = HUD_Stats(STAT_SHELLS + num - 1);

    if (style < 2)
    {
        // simply draw number
        SCR_HUD_DrawNum(hud, value, low,
        scale, style, digits, s_align);
    }
    else
    {
        // else - draw classic ammo-count box with background
        char buf[8];
        int  x, y;

        scale = max(scale, 0.01);

        if (!HUD_PrepareDraw(hud, 42*scale, 11*scale, &x, &y))
            return;

        sprintf (buf, "%3i", value);
        Draw_SSubPic(x, y, sb_ibar, 3+((num-1)*48), 0, 42, 11, scale);
        if (buf[0] != ' ')  Draw_SCharacter (x +  7*scale, y, 18+buf[0]-'0', scale);
        if (buf[1] != ' ')  Draw_SCharacter (x + 15*scale, y, 18+buf[1]-'0', scale);
        if (buf[2] != ' ')  Draw_SCharacter (x + 23*scale, y, 18+buf[2]-'0', scale);
    }
}

void SCR_HUD_DrawAmmoCurrent(hud_t *hud)
{
    static cvar_t *scale = NULL, *style, *digits, *align;
    if (scale == NULL)  // first time called
    {
        scale  = HUD_FindVar(hud, "scale");
        style  = HUD_FindVar(hud, "style");
        digits = HUD_FindVar(hud, "digits");
        align  = HUD_FindVar(hud, "align");
    }
    SCR_HUD_DrawAmmo(hud, 0, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo1(hud_t *hud)
{
    static cvar_t *scale = NULL, *style, *digits, *align;
    if (scale == NULL)  // first time called
    {
        scale  = HUD_FindVar(hud, "scale");
        style  = HUD_FindVar(hud, "style");
        digits = HUD_FindVar(hud, "digits");
        align  = HUD_FindVar(hud, "align");
    }
    SCR_HUD_DrawAmmo(hud, 1, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo2(hud_t *hud)
{
    static cvar_t *scale = NULL, *style, *digits, *align;
    if (scale == NULL)  // first time called
    {
        scale  = HUD_FindVar(hud, "scale");
        style  = HUD_FindVar(hud, "style");
        digits = HUD_FindVar(hud, "digits");
        align  = HUD_FindVar(hud, "align");
    }
    SCR_HUD_DrawAmmo(hud, 2, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo3(hud_t *hud)
{
    static cvar_t *scale = NULL, *style, *digits, *align;
    if (scale == NULL)  // first time called
    {
        scale  = HUD_FindVar(hud, "scale");
        style  = HUD_FindVar(hud, "style");
        digits = HUD_FindVar(hud, "digits");
        align  = HUD_FindVar(hud, "align");
    }
    SCR_HUD_DrawAmmo(hud, 3, scale->value, style->value, digits->value, align->string);
}
void SCR_HUD_DrawAmmo4(hud_t *hud)
{
    static cvar_t *scale = NULL, *style, *digits, *align;
    if (scale == NULL)  // first time called
    {
        scale  = HUD_FindVar(hud, "scale");
        style  = HUD_FindVar(hud, "style");
        digits = HUD_FindVar(hud, "digits");
        align  = HUD_FindVar(hud, "align");
    }
    SCR_HUD_DrawAmmo(hud, 4, scale->value, style->value, digits->value, align->string);
}

// groups
void SCR_HUD_DrawGroup(hud_t *hud, int width, int height, char *picture, int tile, float alpha)
{
    int x, y;

    clamp(width, 1, 99999);
    clamp(height, 1, 99999);

    if (!HUD_PrepareDraw(hud, width, height, &x, &y))
        return;

    // additional drawing here will come..

    // add picture
    if (picture[0])
    {
        mpic_t *pic;
        Hunk_Check();
        pic = Draw_CachePic(va("gfx/%s", picture)/*, false*/);
        Hunk_Check();

/*        if (alpha < 1)
            Draw_SetColor4f(1,1,1,alpha);*/

        if (pic != NULL)
        {
            int pw, ph;

            if (tile)
            {
                // tiled
                int cx = 0, cy = 0;
                while (cy < height)
                {
                    while (cx < width)
                    {
                        pw = min(pic->width, width-cx);
                        ph = min(pic->height, height-cy);

                        if (pw >= pic->width  &&  ph >= pic->height)
                            Draw_TransPic(x+cx , y+cy, pic);
                        else
                            Draw_TransSubPic(x+cx, y+cy, pic, 0, 0, pw, ph);

                        cx += pic->width;
                    }

                    cx = 0;
                    cy += pic->height;
                }
            }
            else
            {
                // centered
                pw = min(width, pic->width);
                ph = min(height, pic->height);

                if (pw >= pic->width  &&  ph >= pic->height)
                    Draw_TransPic(x + (width-pw)/2, y + (height-ph)/2, pic);
                else
                    Draw_TransSubPic(x + (width-pw)/2, y + (height-ph)/2, pic, 0, 0, pw, ph);
            }
        }

/*        if (alpha < 1)
            Draw_RestoreColor();*/
    }

    return;
}
void SCR_HUD_Group1(hud_t *hud)
{
    static cvar_t *width = NULL, *height, *picture, *tile, *alpha;
    if (width == NULL)  // first time called
    {
        width  = HUD_FindVar(hud, "width");
        height = HUD_FindVar(hud, "height");
        picture = HUD_FindVar(hud, "picture");
        tile = HUD_FindVar(hud, "tile");
        alpha = HUD_FindVar(hud, "alpha");
    }
    SCR_HUD_DrawGroup(hud, width->value, height->value, picture->string, tile->value, alpha->value);
}
void SCR_HUD_Group2(hud_t *hud)
{
    static cvar_t *width = NULL, *height, *picture, *tile, *alpha;
    if (width == NULL)  // first time called
    {
        width  = HUD_FindVar(hud, "width");
        height = HUD_FindVar(hud, "height");
        picture = HUD_FindVar(hud, "picture");
        tile = HUD_FindVar(hud, "tile");
        alpha = HUD_FindVar(hud, "alpha");
    }
    SCR_HUD_DrawGroup(hud, width->value, height->value, picture->string, tile->value, alpha->value);
}
void SCR_HUD_Group3(hud_t *hud)
{
    static cvar_t *width = NULL, *height, *picture, *tile, *alpha;
    if (width == NULL)  // first time called
    {
        width  = HUD_FindVar(hud, "width");
        height = HUD_FindVar(hud, "height");
        picture = HUD_FindVar(hud, "picture");
        tile = HUD_FindVar(hud, "tile");
        alpha = HUD_FindVar(hud, "alpha");
    }
    SCR_HUD_DrawGroup(hud, width->value, height->value, picture->string, tile->value, alpha->value);
}
void SCR_HUD_Group4(hud_t *hud)
{
    static cvar_t *width = NULL, *height, *picture, *tile, *alpha;
    if (width == NULL)  // first time called
    {
        width  = HUD_FindVar(hud, "width");
        height = HUD_FindVar(hud, "height");
        picture = HUD_FindVar(hud, "picture");
        tile = HUD_FindVar(hud, "tile");
        alpha = HUD_FindVar(hud, "alpha");
    }
    SCR_HUD_DrawGroup(hud, width->value, height->value, picture->string, tile->value, alpha->value);
}
void SCR_HUD_Group5(hud_t *hud)
{
    static cvar_t *width = NULL, *height, *picture, *tile, *alpha;
    if (width == NULL)  // first time called
    {
        width  = HUD_FindVar(hud, "width");
        height = HUD_FindVar(hud, "height");
        picture = HUD_FindVar(hud, "picture");
        tile = HUD_FindVar(hud, "tile");
        alpha = HUD_FindVar(hud, "alpha");
    }
    SCR_HUD_DrawGroup(hud, width->value, height->value, picture->string, tile->value, alpha->value);
}

// player sorting
// for frags and players
typedef struct sort_teams_info_s
{
    char *name;
    int  frags;
    int  min_ping;
    int  avg_ping;
    int  max_ping;
    int  nplayers;
    int  top, bottom;   // leader colours
    int order;          // should not be here...
}
sort_teams_info_t;

typedef struct sort_players_info_s
{
    int playernum;
    sort_teams_info_t *team;
}
sort_players_info_t;

static sort_players_info_t sort_info_players[MAX_CLIENTS];
static sort_teams_info_t sort_info_teams[MAX_CLIENTS];
static sort_players_info_t *sorted_players[MAX_CLIENTS];
static sort_teams_info_t *sorted_teams[MAX_CLIENTS];
static int n_teams, n_players, n_spectators;

static qboolean isTeamplay()
{
    int teamplay = atoi(Info_ValueForKey(cl.serverinfo, "teamplay"));
    if (!teamplay)
        return false;
    return true;
}

static int ComparePlayers(sort_players_info_t *p1, sort_players_info_t *p2, qboolean byTeams)
{
    int d;
    player_info_t *i1 = &cl.players[p1->playernum];
    player_info_t *i2 = &cl.players[p2->playernum];

    if (i1->spectator  &&  !i2->spectator)
        d = -1;
    else if (!i1->spectator  &&  i2->spectator)
        d = 1;
    else if (i1->spectator  &&  i2->spectator)
    {
        d = strcmp(i1->name, i2->name);
    }
    else    // both are players
    {
        d = 0;

        if (byTeams)
            d = p1->team->frags - p2->team->frags;

        if (!d)
            d = i1->frags - i2->frags;

        if (!d)
            d = strcmp(i1->name, i2->name);
    }

    if (!d)
        d = p1->playernum - p2->playernum;

    return d;
}

static void Sort_Scoreboard(qboolean teamsort)
{
    int i, j;
    qboolean isteamplay;
    int team;

    n_teams = 0;
    n_players = 0;
    n_spectators = 0;
    isteamplay = isTeamplay();

    // sort teams
    for (i=0; i < MAX_CLIENTS; i++)
    {
        if (cl.players[i].name[0] && !cl.players[i].spectator)
        {
            // find players team
            for (team=0; team < n_teams; team++)
                if (!strcmp(cl.players[i].team, sort_info_teams[team].name)
                    &&  sort_info_teams[team].name[0])
                    break;
            if (team == n_teams)   // not found
            {
                team = n_teams++;
                sort_info_teams[team].avg_ping = 0;
                sort_info_teams[team].max_ping = 0;
                sort_info_teams[team].min_ping = 999;
                sort_info_teams[team].nplayers = 0;
				sort_info_teams[team].frags = 0;
				sort_info_teams[team].top = Sbar_TopColor(&cl.players[i]);
				sort_info_teams[team].bottom = Sbar_BottomColor(&cl.players[i]);
                sort_info_teams[team].name = cl.players[i].team;
                sorted_teams[team] = &sort_info_teams[team];
            }
            sort_info_teams[team].nplayers++;
            sort_info_teams[team].frags += cl.players[i].frags;
            sort_info_teams[team].avg_ping += cl.players[i].ping;
            sort_info_teams[team].min_ping = min(sort_info_teams[team].min_ping, cl.players[i].ping);
            sort_info_teams[team].max_ping = max(sort_info_teams[team].max_ping, cl.players[i].ping);

            // set player data
            sort_info_players[n_players+n_spectators].playernum = i;
            sort_info_players[n_players+n_spectators].team = &sort_info_teams[team];
            sorted_players[n_players+n_spectators] = &sort_info_players[n_players+n_spectators];
            if (cl.players[i].spectator)
                n_spectators++;
            else
                n_players++;
        }
    }

    // calc avg ping
    for (team = 0; team < n_teams; team++)
        sort_info_teams[team].avg_ping /= sort_info_teams[team].nplayers;

    // sort teams
    for (i=0; i < n_teams - 1; i++)
        for (j = i + 1; j < n_teams; j++)
            if (sorted_teams[i]->frags < sorted_teams[j]->frags)
            {
                sort_teams_info_t *k = sorted_teams[i];
                sorted_teams[i] = sorted_teams[j];
                sorted_teams[j] = k;
            }

    // order teams - blah
    for (i=0; i < n_teams; i++)
        sorted_teams[i]->order = i;

    // sort players
    for (i=0; i < n_players+n_spectators - 1; i++)
        for (j = i + 1; j < n_players+n_spectators; j++)
            if (ComparePlayers(sorted_players[i], sorted_players[j], (isteamplay && teamsort)) < 0)
            {
                sort_players_info_t *k = sorted_players[i];
                sorted_players[i] = sorted_players[j];
                sorted_players[j] = k;
            }
}


void SCR_HUD_DrawFrags(hud_t *hud)
{
    int width, height;
    int x, y;

    int rows, cols, cell_width, cell_height, space_x, space_y;
    int a_rows, a_cols; // actual

    static cvar_t
        *hud_frags_fraglimit = NULL,
        *hud_frags_cell_width,
        *hud_frags_cell_height,
        *hud_frags_rows,
        *hud_frags_cols,
        *hud_frags_space_x,
        *hud_frags_space_y,
        *hud_frags_vertical,
        *hud_frags_strip,
        *hud_frags_teamsort;

    if (hud_frags_fraglimit == NULL)    // first time
    {
        hud_frags_fraglimit     = HUD_FindVar(hud, "fraglimit");
        hud_frags_cell_width    = HUD_FindVar(hud, "cell_width");
        hud_frags_cell_height   = HUD_FindVar(hud, "cell_height");
        hud_frags_rows          = HUD_FindVar(hud, "rows");
        hud_frags_cols          = HUD_FindVar(hud, "cols");
        hud_frags_space_x       = HUD_FindVar(hud, "space_x");
        hud_frags_space_y       = HUD_FindVar(hud, "space_y");
        hud_frags_teamsort      = HUD_FindVar(hud, "teamsort");
        hud_frags_strip         = HUD_FindVar(hud, "strip");
        hud_frags_vertical      = HUD_FindVar(hud, "vertical");
    }

    rows = hud_frags_rows->value;
    clamp(rows, 1, MAX_CLIENTS);
    cols = hud_frags_cols->value;
    clamp(cols, 1, MAX_CLIENTS);
    cell_width = hud_frags_cell_width->value;
    clamp(cell_width, 28, 128);
    cell_height = hud_frags_cell_height->value;
    clamp(cell_height, 7, 32);
    space_x = hud_frags_space_x->value;
    clamp(space_x, 0, 128);
    space_y = hud_frags_space_y->value;
    clamp(space_y, 0, 128);

    Sort_Scoreboard(hud_frags_teamsort->value);

    if (hud_frags_strip->value)
    {
        if (hud_frags_vertical->value)
        {
            a_cols = min((n_players+rows-1) / rows, cols);
            a_rows = min(rows, n_players);
        }
        else
        {
            a_rows = min((n_players+cols-1) / cols, rows);
            a_cols = min(cols, n_players);
        }
    }
    else
    {
        a_rows = rows;
        a_cols = cols;
    }

    width  = a_cols*cell_width  + (a_cols+1)*space_x;
    height = a_rows*cell_height + (a_rows+1)*space_y;

    if (HUD_PrepareDraw(hud, width, height, &x, &y))
    {
        int i;
        int px;
        int py;
        int num = 0;
		int drawBrackets;

        for (i=0; i < min(n_players, a_rows*a_cols); i++)
        {
            char buf[32];
            int posy;

            player_info_t *info = &cl.players[sorted_players[num]->playernum];

            if (hud_frags_vertical->value)
            {
                if (i % a_rows == 0)
                {
                    px = x + space_x + (i/a_rows) * (cell_width+space_x);
                    py = y + space_y;
                }
            }
            else
            {
                if (i % a_cols == 0)
                {
                    px = x + space_x;
                    py = y + space_y + (i/a_cols) * (cell_height+space_y);
                }
            }

            Draw_Fill(px, py, cell_width, cell_height/2, Sbar_TopColor(info));
            Draw_Fill(px, py + cell_height/2, cell_width, cell_height - cell_height/2, Sbar_BottomColor(info));

            sprintf(buf, "%3d", info->frags);
            posy = py+(cell_height-8)/2 - 1;
            Draw_String(px+(cell_width-8*strlen(buf))/2, posy, buf);

			drawBrackets = 0;

			if (cls.demoplayback || cl.spectator) {
				if (spec_track == sorted_players[num]->playernum)
					drawBrackets = 1;
			} else {
				if (sorted_players[num]->playernum == cl.playernum)
					drawBrackets = 1;
			}

			if (drawBrackets)
            {
                int d = cell_width >= 32 ? 0 : 1;
                Draw_Character(px-2-2*d, posy, 16);
                Draw_Character(px+cell_width-8+1+d, posy, 17);
            }

            if (hud_frags_vertical->value)
                py += cell_height + space_y;
            else
                px += cell_width + space_x;

            num ++;
        }
    }
}

void SCR_HUD_DrawTeamFrags(hud_t *hud)
{
    int width, height;
    int x, y;

    int rows, cols, cell_width, cell_height, space_x, space_y;
    int a_rows, a_cols; // actual

    static cvar_t
        *hud_teamfrags_cell_width,
        *hud_teamfrags_cell_height,
        *hud_teamfrags_rows,
        *hud_teamfrags_cols,
        *hud_teamfrags_space_x,
        *hud_teamfrags_space_y,
        *hud_teamfrags_vertical,
        *hud_teamfrags_strip;

    if (hud_teamfrags_cell_width == 0)    // first time
    {
        hud_teamfrags_cell_width    = HUD_FindVar(hud, "cell_width");
        hud_teamfrags_cell_height   = HUD_FindVar(hud, "cell_height");
        hud_teamfrags_rows          = HUD_FindVar(hud, "rows");
        hud_teamfrags_cols          = HUD_FindVar(hud, "cols");
        hud_teamfrags_space_x       = HUD_FindVar(hud, "space_x");
        hud_teamfrags_space_y       = HUD_FindVar(hud, "space_y");
        hud_teamfrags_strip         = HUD_FindVar(hud, "strip");
        hud_teamfrags_vertical      = HUD_FindVar(hud, "vertical");
    }

    rows = hud_teamfrags_rows->value;
    clamp(rows, 1, MAX_CLIENTS);
    cols = hud_teamfrags_cols->value;
    clamp(cols, 1, MAX_CLIENTS);
    cell_width = hud_teamfrags_cell_width->value;
    clamp(cell_width, 28, 128);
    cell_height = hud_teamfrags_cell_height->value;
    clamp(cell_height, 7, 32);
    space_x = hud_teamfrags_space_x->value;
    clamp(space_x, 0, 128);
    space_y = hud_teamfrags_space_y->value;
    clamp(space_y, 0, 128);

    Sort_Scoreboard(1);

    if (hud_teamfrags_strip->value)
    {
        if (hud_teamfrags_vertical->value)
        {
            a_cols = min((n_teams+rows-1) / rows, cols);
            a_rows = min(rows, n_teams);
        }
        else
        {
            a_rows = min((n_teams+cols-1) / cols, rows);
            a_cols = min(cols, n_teams);
        }
    }
    else
    {
        a_rows = rows;
        a_cols = cols;
    }

    width  = a_cols*cell_width  + (a_cols+1)*space_x;
    height = a_rows*cell_height + (a_rows+1)*space_y;

    if (HUD_PrepareDraw(hud, width, height, &x, &y))
    {
        int i;
        int px;
        int py;
        int num = 0;
		int drawBrackets;

        for (i=0; i < min(n_teams, a_rows*a_cols); i++)
        {
            char buf[32];
            int posy;

            // player_info_t *info = &cl.players[sorted_players[num]->playernum];

            if (hud_teamfrags_vertical->value)
            {
                if (i % a_rows == 0)
                {
                    px = x + space_x + (i/a_rows) * (cell_width+space_x);
                    py = y + space_y;
                }
            }
            else
            {
                if (i % a_cols == 0)
                {
                    px = x + space_x;
                    py = y + space_y + (i/a_cols) * (cell_height+space_y);
                }
            }

            Draw_Fill(px, py, cell_width, cell_height/2, sorted_teams[num]->top);
            Draw_Fill(px, py + cell_height/2, cell_width, cell_height - cell_height/2, sorted_teams[num]->bottom);

            sprintf(buf, "%3d", sorted_teams[num]->frags);
            posy = py+(cell_height-8)/2 - 1;
            Draw_String(px+(cell_width-8*strlen(buf))/2, posy, buf);

			drawBrackets = 0;

			if (cls.demoplayback || cl.spectator) {
				if (!strcmp(cl.players[spec_track].team, sorted_teams[num]->name))
					drawBrackets = 1;
			} else {
				if (!strcmp(sorted_teams[num]->name, cl.players[cl.playernum].team))
					drawBrackets = 1;
			}

			if (drawBrackets)
            {
                int d = cell_width >= 32 ? 0 : 1;
                Draw_Character(px-2-2*d, posy, 16);
                Draw_Character(px+cell_width-8+1+d, posy, 17);
            }

            if (hud_teamfrags_vertical->value)
                py += cell_height + space_y;
            else
                px += cell_width + space_x;

            num ++;
        }
    }
}

// ----------------
// Init
// and add some common elements to hud (clock etc)
//

void CommonDraw_Init(void)
{
    int i;

    // variables
    Cvar_Register (&hud_planmode);
    Cvar_Register (&hud_tp_need);

    // init HUD STAT table
    for (i=0; i < MAX_CL_STATS; i++)
        hud_stats[i] = 0;
    hud_stats[STAT_HEALTH]  = 200;
    hud_stats[STAT_AMMO]    = 100;
    hud_stats[STAT_ARMOR]   = 200;
    hud_stats[STAT_SHELLS]  = 200;
    hud_stats[STAT_NAILS]   = 200;
    hud_stats[STAT_ROCKETS] = 100;
    hud_stats[STAT_CELLS]   = 100;
    hud_stats[STAT_ACTIVEWEAPON] = 32;
    hud_stats[STAT_ITEMS] = 0xffffffff - IT_ARMOR2 - IT_ARMOR1;

    // init clock
    HUD_Register("clock", NULL, "Shows current local time (hh:mm).",
        HUD_PLUSMINUS, ca_disconnected, 8, SCR_HUD_DrawClock,
        "0", "top", "right", "console", "0", "0", "0",
        "big",      "1",
        "style",    "0",
        "blink",    "1",
        NULL);

    // init ping
    HUD_Register("ping", NULL, "Shows most important net conditions, like ping and pl. Shown only when you are connected to a server.",
        HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawPing,
        "0", "top", "left", "bottom", "0", "0", "0",
        "period",       "1",
        "show_pl",      "1",
        "show_min",     "0",
        "show_max",     "0",
        "show_dev",     "0",
        "blink",        "1",
        NULL);
	HUD_Register("tracking", NULL, "Shows the name of tracked player.", 
		HUD_PLUSMINUS, ca_active, 9, SCR_HUD_DrawTracking,
		"0", "top", "left", "bottom", "0", "0", "0",
		"alttext", "",
		NULL);
    // init net
    HUD_Register("net", NULL, "Shows network statistics, like latency, packet loss, average packet sizes and bandwidth. Shown only when you are connected to a server.",
        HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawNetStats,
        "0", "top", "left", "center", "0", "0", "0.2",
        "period",  "1",
        NULL);

    // init net
    HUD_Register("speed", NULL, "Shows your current running speed. It is measured over XY or XYZ axis depending on \'xyz\' property.",
        HUD_PLUSMINUS, ca_active, 7, SCR_HUD_DrawSpeed,
        "0", "top", "center", "bottom", "0", "-5", "0",
        "xyz",  "0",
        NULL);

    // init guns
    HUD_Register("gun", NULL, "Part of your inventory - current weapon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGunCurrent,
        "0", "top", "center", "bottom", "0", "0", "0",
        "wide",  "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("gun2", NULL, "Part of your inventory - shotgun.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun2,
        "0", "top", "left", "bottom", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("gun3", NULL, "Part of your inventory - super shotgun.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun3,
        "0", "gun2", "after", "center", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("gun4", NULL, "Part of your inventory - nailgun.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun4,
        "0", "gun3", "after", "center", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("gun5", NULL, "Part of your inventory - super nailgun.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun5,
        "0", "gun4", "after", "center", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("gun6", NULL, "Part of your inventory - grenade launcher.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun6,
        "0", "gun5", "after", "center", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("gun7", NULL, "Part of your inventory - rocket launcher.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun7,
        "0", "gun6", "after", "center", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("gun8", NULL, "Part of your inventory - thunderbolt.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawGun8,
        "0", "gun7", "after", "center", "0", "0", "0",
        "wide",  "0",
        "style", "0",
        "scale", "1",
        NULL);

    // init powerzz
    HUD_Register("key1", NULL, "Part of your inventory - silver key.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawKey1,
        "0", "top", "top", "left", "0", "64", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("key2", NULL, "Part of your inventory - gold key.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawKey2,
        "0", "key1", "left", "after", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("ring", NULL, "Part of your inventory - invisibility.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawRing,
        "0", "key2", "left", "after", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("pent", NULL, "Part of your inventory - invulnerability.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawPent,
        "0", "ring", "left", "after", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("suit", NULL, "Part of your inventory - biosuit.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSuit,
        "0", "pent", "left", "after", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("quad", NULL, "Part of your inventory - quad damage.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawQuad,
        "0", "suit", "left", "after", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);

    // sigilzz
    HUD_Register("sigil1", NULL, "Part of your inventory - sigil 1.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil1,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("sigil2", NULL, "Part of your inventory - sigil 2.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil2,
        "0", "sigil1", "after", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("sigil3", NULL, "Part of your inventory - sigil 3.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil3,
        "0", "sigil2", "after", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("sigil4", NULL, "Part of your inventory - sigil 4.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawSigil4,
        "0", "sigil3", "after", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);

	// health
    HUD_Register("health", NULL, "Part of your status - health level.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawHealth,
        "0", "screen", "left", "top", "0", "0", "0",
        "style",  "0",
        "scale",  "1",
        "align",  "right",
        "digits", "3",
        NULL);

    // armor
    HUD_Register("armor", NULL, "Part of your inventory - armor level.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmor,
        "0", "screen", "left", "top", "0", "0", "0",
        "style",  "0",
        "scale",  "1",
        "align",  "right",
        "digits", "3",
        NULL);

#ifdef GLQUAKE
	// healthdamage
    HUD_Register("healthdamage", NULL, "Shows amount of damage done to your health.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawHealthDamage,
        "0", "health", "left", "before", "0", "0", "0",
        "style",  "0",
        "scale",  "1",
        "align",  "right",
        "digits", "3",
		"duration", "0.8",
        NULL);

    // armordamage
    HUD_Register("armordamage", NULL, "Shows amount of damage done to your armour.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmorDamage,
        "0", "armor", "left", "before", "0", "0", "0",
        "style",  "0",
        "scale",  "1",
        "align",  "right",
        "digits", "3",
		"duration", "0.8",
        NULL);
#endif

    // ammo/s
    HUD_Register("ammo", NULL, "Part of your inventory - ammo for active weapon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoCurrent,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        "align", "right",
        "digits", "3",
        NULL);
    HUD_Register("ammo1", NULL, "Part of your inventory - ammo - shells.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo1,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        "align", "right",
        "digits", "3",
        NULL);
    HUD_Register("ammo2", NULL, "Part of your inventory - ammo - nails.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo2,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        "align", "right",
        "digits", "3",
        NULL);
    HUD_Register("ammo3", NULL, "Part of your inventory - ammo - rockets.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo3,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        "align", "right",
        "digits", "3",
        NULL);
    HUD_Register("ammo4", NULL, "Part of your inventory - ammo - cells.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmo4,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        "align", "right",
        "digits", "3",
        NULL);

    // ammo icon/s
    HUD_Register("iammo", NULL, "Part of your inventory - ammo icon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIconCurrent,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);
    HUD_Register("iammo1", NULL, "Part of your inventory - ammo icon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon1,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "2",
        "scale", "1",
        NULL);
    HUD_Register("iammo2", NULL, "Part of your inventory - ammo icon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon2,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "2",
        "scale", "1",
        NULL);
    HUD_Register("iammo3", NULL, "Part of your inventory - ammo icon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon3,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "2",
        "scale", "1",
        NULL);
    HUD_Register("iammo4", NULL, "Part of your inventory - ammo icon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawAmmoIcon4,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "2",
        "scale", "1",
        NULL);

    // armor icon
    HUD_Register("iarmor", NULL, "Part of your inventory - armor icon.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawArmorIcon,
        "0", "screen", "left", "top", "0", "0", "0",
        "style", "0",
        "scale", "1",
        NULL);

    // player face
    HUD_Register("face", NULL, "Your bloody face.",
        HUD_INVENTORY, ca_active, 0, SCR_HUD_DrawFace,
        "0", "screen", "left", "top", "0", "0", "0",
        "scale", "1",
        NULL);

    // groups
    HUD_Register("group1", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group1,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group1",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group2", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group2,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group2",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group3", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group3,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group3",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group4", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group4,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group4",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group5", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group5,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group5",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group6", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group2,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group6",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group7", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group3,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group7",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group8", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group4,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group8",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);
    HUD_Register("group9", NULL, "Group element.",
        HUD_NO_GROW, ca_disconnected, 0, SCR_HUD_Group5,
        "0", "screen", "left", "top", "0", "0", ".5",
        "name", "group9",
        "width", "64",
        "height", "64",
        "picture", "",
        "tile", "0",
        "alpha", "1",
        NULL);

    HUD_Register("frags", NULL, "Show list of player frags in short form.",
        0, ca_active, 0, SCR_HUD_DrawFrags,
        "0", "screen", "left", "top", "0", "0", "0",
        "cell_width", "32",
        "cell_height", "8",
        "rows", "1",
        "cols", "4",
        "space_x", "1",
        "space_y", "1",
        "teamsort", "0",
        "fraglimit", "1",
        "strip", "1",
        "vertical", "0",
        NULL);
        
    HUD_Register("teamfrags", NULL, "Show list of team frags in short form.",
        0, ca_active, 0, SCR_HUD_DrawTeamFrags,
        "0", "screen", "left", "top", "0", "0", "0",
        "cell_width", "32",
        "cell_height", "8",
        "rows", "1",
        "cols", "2",
        "space_x", "1",
        "space_y", "1",
        "strip", "1",
        "vertical", "0",
        NULL);
        
/* hexum -> FIXME? this is used only for debug purposes, I wont bother to port it (it shouldnt be too difficult if anyone cares)
#ifdef GLQUAKE
#ifdef _DEBUG
    HUD_Register("framegraph", NULL, "Shows different frame times for debug/profiling purposes.",
        HUD_PLUSMINUS | HUD_ON_SCORES, ca_disconnected, 0, SCR_HUD_DrawFrameGraph,
        "0", "top", "left", "bottom", "0", "0", "2",
        "swap_x",       "0",
        "swap_y",       "0",
        "scale",        "14",
        "width",        "256",
        "height",       "64",
        "alpha",        "1",
        NULL);
#endif
#endif
*/

}
