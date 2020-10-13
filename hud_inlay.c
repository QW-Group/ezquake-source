/*
Copyright (C) 2020 Joseph Pecoraro

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

// hud_inlay.c

#include "quakedef.h"
#include "hud.h"
#include "teamplay.h"
#include "tp_msgs.h"
#include "hud_common.h"
#include "vx_stuff.h"
#include "utils.h"
#include "gl_model.h"
#include "fonts.h"

#define FONT_WIDTH 8

typedef struct inlay_player_s {
    int client; // cl.players index.
    double time; // last received update.
    char armor[32];
    char health[32];
    char weapon[32];
    char powerups[32];
    char location[32];
    char msg[32];
} inlay_player_t;

inlay_player_t inlay_clients[MAX_CLIENTS];

static int inlay_last_update_time = 0;
static int inlay_seconds_since_last_update = 0;

// -------------

static cvar_t scr_teaminlay_align_right     = { "scr_teaminlay_align_right", "1" };
static cvar_t scr_teaminlay_frame_color     = { "scr_teaminlay_frame_color", "10 0 0 120", CVAR_COLOR };
static cvar_t scr_teaminlay_scale           = { "scr_teaminlay_scale",       "1" };
static cvar_t scr_teaminlay_y               = { "scr_teaminlay_y",           "0" };
static cvar_t scr_teaminlay_x               = { "scr_teaminlay_x",           "-300" };
static cvar_t scr_teaminlay_loc_width       = { "scr_teaminlay_loc_width",   "5" };
static cvar_t scr_teaminlay_name_width      = { "scr_teaminlay_name_width",  "6" };
static cvar_t scr_teaminlay_show_self       = { "scr_teaminlay_show_self",   "1" }; // FIXME: 1 for debugging
static cvar_t scr_teaminlay_proportional    = { "scr_teaminlay_proportional", "0"};

cvar_t scr_teaminlay = { "scr_teaminlay", "1" }; // Enable / disable hud elements.
cvar_t teaminlay = { "teaminlay", "1" }; // Enable / disable sending updates.
cvar_t teaminlay_interval = { "teaminlay_interval", "1" }; // Frequency of sending updates.

// -------------

qbool Inlay_Parse_Message(char* msg, int player_slot)
{
    // Ensure there are at least 4 commas for the sections.
    // Expecting: <armor>,<health>,<weapon>,<powerups>,<location>,<msg>
    const int expected = 5;
    int commas = 0;
    for (char* p = msg; *p; ++p) {
        if (*p == ',') {
            commas++;
            if (commas == expected) {
                break;
            }
        }
    }
    if (commas != expected) {
        Com_DPrintf("Invalid inlay message: %s\n", msg);
        return false;
    }

    inlay_player_t* player = &inlay_clients[player_slot];
    player->client = player_slot;
    player->time = r_refdef2.time;

    char *current_pos = msg;
    char *comma_pos;
    
    comma_pos = strstr(current_pos, ",");
    strlcpy(player->armor, current_pos, min(sizeof(player->armor), comma_pos - current_pos + 1));
    current_pos = comma_pos + 1;

    comma_pos = strstr(current_pos, ",");
    strlcpy(player->health, current_pos, min(sizeof(player->health), comma_pos - current_pos + 1));
    current_pos = comma_pos + 1;

    comma_pos = strstr(current_pos, ",");
    strlcpy(player->weapon, current_pos, min(sizeof(player->weapon), comma_pos - current_pos + 1));
    current_pos = comma_pos + 1;

    comma_pos = strstr(current_pos, ",");
    strlcpy(player->powerups, current_pos, min(sizeof(player->powerups), comma_pos - current_pos + 1));
    current_pos = comma_pos + 1;

    comma_pos = strstr(current_pos, ",");
    strlcpy(player->location, current_pos, min(sizeof(player->location), comma_pos - current_pos + 1));
    current_pos = comma_pos + 1;

    strlcpy(player->msg, current_pos, min(sizeof(player->msg), strlen(current_pos)));

    // DEBUG:
    // Com_DPrintf("BOGO armor: %s\n", player->armor);
    // Com_DPrintf("BOGO health: %s\n", player->health);
    // Com_DPrintf("BOGO weapon: %s\n", player->weapon);
    // Com_DPrintf("BOGO powerups: %s\n", player->powerups);
    // Com_DPrintf("BOGO location: %s\n", player->location);
    // Com_DPrintf("BOGO extra: %s\n", player->msg);
    // Com_DPrintf("BOGO time: %f\n", player->time);

    return true;
}

qbool Inlay_Handle_Message(char *s, int flags, int offset)
{
    // Only handle team messages.
    if (!(flags & msgtype_team))
        return false;

    // Only handle if #inlay#.
    char* substring = strstr(s, "#inlay#");
    if (!substring)
        return false;

    char name[32];
    int p = 1;
    int q = offset - 4;
    int len = bound (0, q - p + 1, sizeof(name) - 1);
    strlcpy(name, s + p, len + 1);

    int slot = Player_NametoSlot(name);
    // Com_DPrintf("BOGO: received inlay message for player %s (%d): %s\n", name, slot, substring);
    if (slot == PLAYER_NAME_NOMATCH)
        return false;

    if (!Inlay_Parse_Message(substring + strlen("#inlay# "), slot))
        return false;

    return true;
}

void Inlay_Send_Message(void)
{
    TP_MSG_Report_Inlay("#inlay#");
}

// This is the current client deciding to send an update.
void Inlay_Update(void)
{
    // Do not send updates if disabled.
    if (!teaminlay.integer)
        return;

    // Only update when we are connected to a server.
    if (cls.state != ca_active)
        return;

    // Do not send updates when we are a spectator.
    if (cl.spectator)
        return;

    // Update every 0-5 seconds based on setting.
    int seconds_per_update = bound(0, teaminlay_interval.integer, 5);

    int current_second = (int)curtime;
    if (current_second != inlay_last_update_time) {
        inlay_last_update_time = current_second;
        inlay_seconds_since_last_update++;
        if (inlay_seconds_since_last_update == seconds_per_update) {
            inlay_seconds_since_last_update = 0;
            Inlay_Send_Message();
        }
    }
}

static int SCR_HudDrawInlayPlayer(inlay_player_t *player, float x, int y, int maxname, int maxloc, qbool width_only, float scale, qbool proportional)
{
    if (!player)
        return 0;

    int clientid = player->client;
    if (clientid < 0 || clientid >= MAX_CLIENTS)
        return 0;

    float x_in = x;
    float font_width = scale * FONT_WIDTH;

    // Powerups.
	if (!width_only) {
        extern mpic_t *sb_items[32];
        // Always draw powerups aligned on the left instead of 3 different slots per-powerup.
        float temp_x = x;
		if (strstr(player->powerups, "quad")) {
			Draw_SPic(temp_x, y, sb_items[5], scale / 2);
            temp_x += font_width;
        }
		if (strstr(player->powerups, "pent")) {
			Draw_SPic(temp_x, y, sb_items[3], scale / 2);
		    temp_x += font_width;
        }
        if (strstr(player->powerups, "ring")) {
			Draw_SPic(temp_x, y, sb_items[2], scale / 2);
            temp_x += font_width;
        }
	}
	x += 3 * font_width;

    // Name.
    float name_width = maxname * font_width;
    if (!width_only)
        Draw_SStringAligned(x, y, TP_ParseFunChars(cl.players[clientid].name, false), scale, 1, proportional, text_align_right, x + name_width);
    x += name_width;

    // Space.
    x += font_width;

    // Location.
    float loc_width = maxloc * font_width;
    if (!width_only)
        Draw_SString(x, y, TP_ParseFunChars("$x10", false), scale, proportional);
    x += font_width;
    if (!width_only)
        Draw_SStringAligned(x, y, TP_ParseFunChars(player->location, false), scale, 1.0, proportional, text_align_left, x + loc_width);
    x += loc_width;
    if (!width_only)
        Draw_SString(x, y, TP_ParseFunChars("$x11", false), scale, proportional);
    x += font_width;

    // Space.
    x += font_width;

    // Armor.
    if (!width_only) {
        char *armor_str = player->armor;
        char armor_white_stripped[32];
        Util_SkipChars(armor_str, "{}", armor_white_stripped, 32);
        Draw_SStringAligned(x, y, armor_white_stripped, scale, 1.0f, proportional, text_align_right, x + 3 * font_width);
    }
    x += 3 * font_width;

    // Divider.
    if (!width_only)
        Draw_SString(x, y, "/", scale, proportional);
    x += font_width;

    // Health.
    if (!width_only)
        Draw_SStringAligned(x, y, TP_ParseFunChars(player->health, false), scale, 1.0, proportional, text_align_left, x + 3 * font_width);
    x += 3 * font_width;

    // Space.
    x += font_width;

    // Weapon.
    if (!width_only) {
        char *weap_str = player->weapon;
        char weap_white_stripped[32];
        Util_SkipChars(weap_str, "{}", weap_white_stripped, 32);
        Draw_SString(x, y, weap_white_stripped, scale, proportional);
    }
    x += 3 * font_width;

    // Extra message. (Left overflow)
    if (!width_only && player->msg[0]) {
        char *msg_str = player->msg;
        char msg_white_stripped[32];
        Util_SkipChars(msg_str, "{}", msg_white_stripped, 32);
        float msg_width = strlen_color(msg_white_stripped) * font_width;
        float overflow_x = x_in - font_width - msg_width;
        Draw_SString(overflow_x, y, msg_white_stripped, scale, proportional);
    }

    return x - x_in;
}

// This is the current client deciding to draw the inlay based on current info.
void SCR_Draw_Inlay(void)
{
    // Do not display inlay unless it is enabled and we are in a teamplay mode.
    if (!cl.teamplay || !scr_teaminlay.integer)
        return;

    // FIXME: Do we need any special case handling for MVD playback mode? Prolly not.
    // if (cls.mvdplayback)
    //     Update_Inlay_State_For_MVD();

    int max_loc_length = bound(0, scr_teaminlay_loc_width.integer, 30);
    int max_name_length = bound(0, scr_teaminlay_name_width.integer, 15);
    int max_name_length_seen = 0;

    // Determine what we need for this update.
    int slots[MAX_CLIENTS];
    int slots_len = 0;
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        // Skip players that haven't updated in a while.
        if (!inlay_clients[i].time || ((inlay_clients[i].time + TI_TIMEOUT) < r_refdef2.time))
            continue;

        // Skip spectators and invalid players.
        if (!cl.players[i].name[0] || cl.players[i].spectator)
            continue;

        // Skip current player unless allowed by setting.
        if (i == cl.playernum && !scr_teaminlay_show_self.integer)
            continue;

        slots[slots_len++] = i;

        max_name_length_seen = max(max_name_length_seen, strlen(cl.players[i].name));
    }

    // Debug
    // Com_DPrintf("BOGO SCR_Draw_Inlay - slots %d\n", slots_len);

    // No inlay data.
    if (!slots_len)
        return;

    // FIXME: Should we sort players to be consistent?

    // Shorten name size for optimial fit since it is on the leading edge.
    if (max_name_length_seen < max_name_length)
        max_name_length = max_name_length_seen;

    float scale = bound(0.1, scr_teaminlay_scale.value, 10);

    // Calculate positions.
    int y = vid.height * 0.6 + scr_teaminlay_y.value;
    int w = SCR_HudDrawInlayPlayer(&inlay_clients[slots[0]], 0, 0, max_name_length, max_loc_length, true, scale, scr_teaminlay_proportional.integer);
    int h = slots_len * scale;

    // Update horizontal position.
    int x = (scr_teaminlay_align_right.value ? (vid.width - w) - FONTWIDTH : FONTWIDTH);
    x += scr_teaminlay_x.value;

    // Draw background.
    byte *col = scr_teaminlay_frame_color.color;
    Draw_AlphaRectangleRGB(x, y, w, h * FONTWIDTH, 0, true, RGBAVECT_TO_COLOR(col));            

    // Draw player info lines..
    for (int i = 0; i < slots_len; ++i) {
        int slot = slots[i];
        SCR_HudDrawInlayPlayer(&inlay_clients[slot], x, y, max_name_length, max_loc_length, false, scale, scr_teaminlay_proportional.integer);
        y += FONTWIDTH * scale;
    }
}

void Inlay_GameStart(void)
{
    for (int i = 0; i < MAX_CLIENTS; ++i) {
        // Skip players that haven't updated in a while.
        inlay_player_t* player = &inlay_clients[i];
        if (!player->time || ((player->time + TI_TIMEOUT) < r_refdef2.time))
            continue;

        // Clear player status at start of game.
        player->armor[0] = '\0';
        player->health[0] = '\0';
        player->weapon[0] = '\0';
        player->powerups[0] = '\0';
        player->location[0] = '\0';
        player->msg[0] = '\0';
    }
}

void SCR_ClearInlay(void)
{
    memset(inlay_clients, 0, sizeof(inlay_clients));
}

void Inlay_HudInit(void)
{
    Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);

    Cvar_Register(&scr_teaminlay_align_right);
    Cvar_Register(&scr_teaminlay_frame_color);
    Cvar_Register(&scr_teaminlay_scale);
    Cvar_Register(&scr_teaminlay_y);
    Cvar_Register(&scr_teaminlay_x);
    Cvar_Register(&scr_teaminlay_loc_width);
    Cvar_Register(&scr_teaminlay_name_width);
    Cvar_Register(&scr_teaminlay_show_self);
    Cvar_Register(&scr_teaminlay_proportional);
    Cvar_Register(&scr_teaminlay);

    Cvar_Register(&teaminlay);
    Cvar_Register(&teaminlay_interval);

    extern cvar_t teaminlay_msg, teaminlay_msg_duration;
    Cvar_Register(&teaminlay_msg);
    Cvar_Register(&teaminlay_msg_duration);

    // FIXME: New Hud Editor?
    // HUD_Register(
    //     "inlay", NULL, "Show information about your team in short form.",
    //     0, ca_active, 0, SCR_HUD_DrawInlay,
    //     "0", "", "right", "center", "0", "0", "0.2", "20 20 20", NULL,
    //     "align_right", "0",
    //     "loc_width", "5",
    //     "name_width", "6",
    //     "show_self", "1",
    //     "scale", "1",
    //     "powerup_style", "1",
    //     "proportional", "0",
    //     NULL
    // );
}
