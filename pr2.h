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
 */

#ifndef __PR2_H__
#define __PR2_H__


extern intptr_t sv_syscall(intptr_t arg, ...);
extern int sv_sys_callex(byte *data, unsigned int len, int fn, pr2val_t*arg);
typedef void (*pr2_trapcall_t)(byte* base, uintptr_t mask, pr2val_t* stack, pr2val_t*retval);

//extern int usedll;
extern cvar_t sv_progtype;
extern vm_t* sv_vm;


void		PR2_Init();
#define PR_Init PR2_Init
void		PR2_UnLoadProgs();
void		PR2_LoadProgs();
void		PR2_GameStartFrame();
#define PR_GameStartFrame PR2_GameStartFrame
void		PR2_LoadEnts(char *data);
#define PR_LoadEnts PR2_LoadEnts
void		PR2_GameClientConnect(int spec);
#define PR_GameClientConnect PR2_GameClientConnect
void		PR2_GamePutClientInServer(int spec);
#define PR_GamePutClientInServer PR2_GamePutClientInServer
void		PR2_GameClientDisconnect(int spec);
#define PR_GameClientDisconnect PR2_GameClientDisconnect
void		PR2_GameClientPreThink(int spec);
#define PR_GameClientPreThink PR2_GameClientPreThink
void		PR2_GameClientPostThink(int spec);
#define PR_GameClientPostThink PR2_GameClientPostThink
qbool		PR2_ClientCmd();
#define PR_ClientCmd PR2_ClientCmd
void		PR2_ClientKill();
#define PR_ClientKill PR2_ClientKill
qbool		PR2_ClientSay(int isTeamSay, char *message);
#define PR_ClientSay PR2_ClientSay
void		PR2_GameSetNewParms();
#define PR_GameSetNewParms PR2_GameSetNewParms
void		PR2_GameSetChangeParms();
#define PR_GameSetChangeParms PR2_GameSetChangeParms
void		PR2_EdictTouch(func_t f);
#define PR_EdictTouch PR2_EdictTouch
void		PR2_EdictThink(func_t f);
#define PR_EdictThink PR2_EdictThink
void		PR2_EdictBlocked(func_t f);
#define PR_EdictBlocked PR2_EdictBlocked
qbool 		PR2_UserInfoChanged();
#define PR_UserInfoChanged PR2_UserInfoChanged
void 		PR2_GameShutDown();
void 		PR2_GameConsoleCommand(void);
void		PR2_PausedTic(float duration);
#define PR_PausedTic PR2_PausedTic

char*		PR2_GetString(intptr_t);
#define		PR_GetString PR2_GetString
intptr_t	PR2_SetString(char*s);
#define PR_SetString PR2_SetString
void		PR2_RunError(char *error, ...);
void		ED2_Free(edict_t *ed);
edict_t*	ED2_Alloc();
void		ED2_ClearEdict(edict_t *e);
eval_t*		PR2_GetEdictFieldValue(edict_t *ed, char *field);
#define PR_GetEdictFieldValue PR2_GetEdictFieldValue
int			ED2_FindFieldOffset(char *field);
#define ED_FindFieldOffset ED2_FindFieldOffset
void 		PR2_InitProg();

#endif /* !__PR2_H__ */
