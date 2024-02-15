/*
Copyright (C) 2011 VultureIIC

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

#ifndef __VX_TRACKER_H__
#define __VX_TRACKER_H__

extern cvar_t amf_tracker_messages;
extern cvar_t amf_tracker_time;
extern cvar_t amf_tracker_align_right;
extern cvar_t amf_tracker_scale;

void VX_TrackerDeath(int player, int weapon, int count);
void VX_TrackerSuicide(int player, int weapon, int count);
void VX_TrackerFragXvsY(int player, int killer, int weapon, int player_wcount, int killer_wcount);
void VX_TrackerOddFrag(int player, int weapon, int wcount);

void VX_TrackerTK_XvsY(int player, int killer, int weapon, int p_count, int p_icount, int k_count, int k_icount);
void VX_TrackerOddTeamkill(int player, int weapon, int count);
void VX_TrackerOddTeamkilled(int player, int weapon);

void VX_TrackerFlagTouch(int count);
void VX_TrackerFlagDrop(int count);
void VX_TrackerFlagCapture(int count);

char* GetWeaponName(int num);
char* GetColoredWeaponName(int num, const byte *color);
const char* GetWeaponImageName(int num);
const char* GetWeaponTextName(int num);
void VX_TrackerThink(void);
void VX_TrackerInit(void);
void VX_TrackerClear(void);
void VX_TrackerStreak(int player, int count);
void VX_TrackerStreakEnd(int player, int killer, int count);
void VX_TrackerStreakEndOddTeamkilled(int player, int count);

// This needs improved...
void VX_TrackerPickupText(const char* line);

#define MAX_WEAPON_CLASSES		64
#define MAX_FRAG_DEFINITIONS	512
#define MAX_FRAGMSG_LENGTH		256
#define MAX_TRACKER_IMAGES      128

#endif // __VX_TRACKER_H__
