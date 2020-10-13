/*
Copyright (C) 2011 johnnycz

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
  \file

  \brief
  Inbuilt teamplay messages module

  \author johnnycz
  \author Up2nOgOoD[ROCK]
**/

#ifndef __TP_MSGS_H__
#define __TP_MSGS_H__

extern void TP_Msg_Lost_f (void);
extern void TP_Msg_Report_f (void);
extern void TP_Msg_Report_Inlay_f (void);
extern void TP_Msg_Coming_f (void);
extern void TP_Msg_EnemyPowerup_f (void);
extern void TP_Msg_Safe_f (void);
extern void TP_Msg_KillMe_f (void);
extern void TP_Msg_Help_f (void);
extern void TP_Msg_GetQuad_f (void);
extern void TP_Msg_GetPent_f (void);
extern void TP_Msg_QuadDead_f (void);
extern void TP_Msg_Took_f (void);
extern void TP_Msg_Point_f (void);
extern void TP_Msg_Need_f (void);
extern void TP_Msg_YesOk_f (void);
extern void TP_Msg_NoCancel_f (void);
extern void TP_Msg_YouTake_f (void);
extern void TP_Msg_ItemSoon_f (void);
extern void TP_Msg_Waiting_f (void);
extern void TP_Msg_Slipped_f (void);
extern void TP_Msg_Replace_f (void);
extern void TP_Msg_Trick_f (void);
//TF messages
extern void TP_Msg_TFConced_f (void);

extern void TP_MSG_Report_Inlay(char*);

extern const char* TP_MSG_Colored_Armor(void);
extern const char * TP_MSG_Colored_Powerup(void);
extern const char * TP_MSG_Colored_Short_Powerups(void);

#endif // __TP_MSGS_H__
