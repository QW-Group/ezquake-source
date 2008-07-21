/*
 *  QW262
 *  Copyright (C) 2004  [sd] angel
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *
 *  $Id: pr2.h 695 2007-09-30 14:43:34Z tonik $
 */

#ifndef __PR2_H__
#define __PR2_H__


extern int sv_syscall(int arg, ...);
extern int sv_sys_callex(byte *data, unsigned int len, int fn, pr2val_t*arg);
typedef void (*pr2_trapcall_t)(byte* base, unsigned int mask, pr2val_t* stack, pr2val_t*retval);

//extern int usedll;
extern cvar_t sv_progtype;
extern vm_t* sv_vm;


void		PR2_Init();
void		PR2_UnLoadProgs();
void		PR2_LoadProgs();
void		PR2_GameStartFrame();
void		PR2_LoadEnts(char *data);
void		PR2_GameClientConnect(int spec);
void		PR2_GamePutClientInServer(int spec);
void		PR2_GameClientDisconnect(int spec);
void		PR2_GameClientPreThink(int spec);
void		PR2_GameClientPostThink(int spec);
qbool		PR2_ClientCmd();
qbool		PR2_ClientSay(int isTeamSay);
void		PR2_GameSetNewParms();
void		PR2_GameSetChangeParms();
void		PR2_EdictTouch();
void		PR2_EdictThink();
void		PR2_EdictBlocked();
qbool 		PR2_UserInfoChanged();
void 		PR2_GameShutDown();
void 		PR2_GameConsoleCommand(void);
void		PR2_PausedTic(float duration);

char*		PR2_GetString(int);
int			PR2_SetString(char*s);
void		PR2_RunError(char *error, ...);
void		ED2_Free(edict_t *ed);
edict_t*	ED2_Alloc();
void		ED2_ClearEdict(edict_t *e);
eval_t*		PR2_GetEdictFieldValue(edict_t *ed, char *field);
int			ED2_FindFieldOffset(char *field);
void 		PR2_InitProg();

#endif /* !__PR2_H__ */
