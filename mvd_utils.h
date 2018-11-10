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

$Id: mvd_utils.h,v 1.5 2007-09-24 21:41:17 johnnycz Exp $
*/

// main header for a group of MVD tools: mvd_utils, mvd_xmlstats, mvd_autotrack

void MVD_Screen (void);

// initialize the module, add variables and commands
void MVD_Utils_Init(void); 
void MVD_Mainhook(void);
void MVD_Stats_Cleanup(void);
void MVD_ClockList_TopItems_Draw(double time_limit, int style, int x, int y, float scale, int filter, qbool backpacks);
void MVD_ClockList_TopItems_DimensionsGet(double time_limit, int style, int *width, int *height, float scale, qbool backpacks);

// update match info structures
void MVD_Init_Info(int player_slot);
void MVD_GameStart(void);
void MVD_Initialise(void);

// //ktx event notifications
void MVDAnnouncer_MatchStart(void);
void MVDAnnouncer_ItemTaken(const char* s);
void MVDAnnouncer_StartTimer(const char* s);
void MVDAnnouncer_PackDropped(const char* s);
void MVDAnnouncer_Expired(const char* s);
void MVDAnnouncer_BackpackPickup(const char* s);

extern int powerup_cam_active,cam_1,cam_2,cam_3,cam_4;
extern cvar_t mvd_pc_view_1,mvd_pc_view_2,mvd_pc_view_3,mvd_pc_view_4;
