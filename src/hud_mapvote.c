/*
Copyright (C) 1996-1997 Id Software, Inc.

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

$Id: hud_mapvote.c,v 1.0 2025-09-25 Bance $
*/

#include "quakedef.h"
#include "hud.h"
#include "hud_common.h"
#include "fonts.h"
#include "screen.h"
#include <stdio.h>
#include <string.h>

static cvar_t hud_mapvote_frame_color  = { "scr_mapvote_frame_color", "10 10 10 160", CVAR_COLOR };
static qbool vote_active = false;
static double vote_expire_time = 0.0; // absolute time when HUD element should disappear
static char map_vote_str[512];
char map_vote_map[64];

void SCR_MapVote(char *str) {
    vote_active = true;
    vote_expire_time = cls.realtime + 10.0; // 10 second lifetime per vote

    char *triggers[] = { "suggests map", "would rather play on" };
    int n_triggers = sizeof(triggers) / sizeof(triggers[0]);

    map_vote_map[0] = '\0';

    int i;
    for (i = 0; i < n_triggers; i++) {
        char qbuf[128];
        strncpy(qbuf, triggers[i], sizeof(qbuf) - 1);
        qbuf[sizeof(qbuf) - 1] = '\0';

        unsigned char *q_red = Q_redtext((unsigned char *)qbuf);

        char *found = strstr(str, (char *)q_red);
        if (found) {
            found += strlen((char *)q_red);
            while (*found == ' ') found++;

            strncpy(map_vote_map, found, sizeof(map_vote_map) - 1);
            map_vote_map[sizeof(map_vote_map) - 1] = '\0';

            size_t len = strlen(map_vote_map);
            if (len > 0 && map_vote_map[len - 1] == '\n') {
                map_vote_map[len - 1] = '\0';
            }

            break;
        }
    }
    sprintf(map_vote_str, "map vote for %s", map_vote_map);
}

void SCR_FinishMapVote(void) {
    vote_active = false;
    vote_expire_time = 0.0;
}

void SCR_HUD_DrawMapVote(hud_t *hud)
{
    if (!vote_active)
        return;

    if (cls.realtime >= vote_expire_time) {
        vote_active = false;
        return;
    }

    if (!cl.standby) {
        vote_active = false;
        vote_expire_time = 0.0;
        return;
    }

    int x, y;
    cvar_t *v_scale = HUD_FindVar(hud, "scale");
    float scale = bound(0.1f, v_scale->value, 10.0f);

    int base_char_w = 8;

    int text_h = (int)(FONTWIDTH * scale);
    int num_lines = 2;

    int pad_x = (int)(4 * scale);
    int pad_y = (int)(2 * scale);

    int conback_w = base_char_w * 25;
    int conback_h = (int)(conback_w * 9.0f / 16.0f);

    int box_w = conback_w + pad_x * 2;
    int box_h = text_h * num_lines + pad_y * 2 + conback_h;

    byte *col = hud_mapvote_frame_color.color;

    if (!HUD_PrepareDraw(hud, box_w, box_h, &x, &y))
        return;

    Draw_AlphaRectangleRGB(x, y, box_w, box_h, 0, true, RGBAVECT_TO_COLOR(col));

    float alpha = 1.0f;
    if (col) { alpha = (col[3] / 255.0f); if (alpha <= 0.0f) alpha = 1.0f; }

    Draw_SStringAlpha(x + pad_x, y + pad_y, map_vote_str, scale, alpha, 0);
    Draw_SStringAlpha(x + pad_x, y + pad_y + text_h, "use /agree to vote", scale, alpha, 0);

    Draw_MapVote(x + pad_x, y + pad_y + text_h * num_lines, conback_w, conback_h, alpha);
}


void MapVote_HudInit(void)
{
    Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
    Cvar_Register(&hud_mapvote_frame_color);

    HUD_Register(
        "mapvote", NULL, "Simple custom HUD element",
        0, ca_active, 0, SCR_HUD_DrawMapVote,
        "1", "screen", "right", "top", "0", "50", "0.2", "10 10 10 160", NULL,
        "scale", "1",
        NULL
    );
}
