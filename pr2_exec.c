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
 *  
 */

#ifdef USE_PR2

#include "qwsvdef.h"

gameData_t *gamedata;

cvar_t	sv_progtype = {"sv_progtype","0"};	// bound the size of the
#ifdef QVM_PROFILE
extern cvar_t sv_enableprofile;
#endif
//int usedll;

void ED2_PrintEdicts (void);
void PR2_Profile_f (void);
void ED2_PrintEdict_f (void);
void ED_Count (void);
void PR2_Init(void)
{
	int p;
	int usedll;
	Cvar_Register(&sv_progtype);
	Cvar_Register(&sv_progsname);
#ifdef WITH_NQPROGS
	Cvar_Register(&sv_forcenqprogs);
#endif
#ifdef QVM_PROFILE
	Cvar_Register(&sv_enableprofile);
#endif

	p = COM_CheckParm ("-progtype");

	if (p && p < COM_Argc())
	{
		usedll = Q_atoi(COM_Argv(p + 1));

		if (usedll > 2)
			usedll = VM_NONE;
		Cvar_SetValue(&sv_progtype,usedll);
	}

	Cmd_AddCommand ("edict", ED2_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED2_PrintEdicts);
	Cmd_AddCommand ("edictcount", ED_Count);
	Cmd_AddCommand ("profile", PR2_Profile_f);
	Cmd_AddCommand ("mod", PR2_GameConsoleCommand);

	memset(pr_newstrtbl, 0, sizeof(pr_newstrtbl));
}

//===========================================================================
// PR2_GetString
//===========================================================================
char *PR2_GetString(intptr_t num)
{
	qvm_t *qvm;

	if(!sv_vm)
		return PR1_GetString(num);

	switch (sv_vm->type)
	{
	case VM_NONE:
		return PR1_GetString(num);

	case VM_NATIVE:
		if (num)
			return (char *) num;
		else
			return "";

	case VM_BYTECODE:
		if (!num)
			return "";
		qvm = (qvm_t*)(sv_vm->hInst);
		if ( num & ( ~qvm->ds_mask) )
		{
			Con_DPrintf("PR2_GetString error off %8x/%8x\n", num, qvm->len_ds );
			return "";
		}
		return (char *) (qvm->ds+ num);
	}

	return NULL;
}

//===========================================================================
// PR2_SetString
// FIXME for VM
//===========================================================================
intptr_t PR2_SetString(char *s)
{
	qvm_t *qvm;
	intptr_t off;
	if(!sv_vm)
		return PR1_SetString(s);

	switch (sv_vm->type)
	{
	case VM_NONE:
		return PR1_SetString(s);

	case VM_NATIVE:
		return (intptr_t) s;

	case VM_BYTECODE:
		qvm = (qvm_t*)(sv_vm->hInst);
		off = (byte*)s - qvm->ds;
		if (off &(~qvm->ds_mask))
			return 0;

		return off;
		break;
	}

	return 0;
}

/*
=================
PR2_LoadEnts
=================
*/
extern char *pr2_ent_data_ptr;

void PR2_LoadEnts(char *data)
{
	if (sv_vm)
	{
		pr2_ent_data_ptr = data;

		//Init parse
		VM_Call(sv_vm, GAME_LOADENTS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	else
	{
		PR1_LoadEnts(data);
	}
}

//===========================================================================
// GameStartFrame
//===========================================================================
void PR2_GameStartFrame()
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_START_FRAME, (int) (sv.time * 1000), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameStartFrame();
}

//===========================================================================
// GameClientConnect
//===========================================================================
void PR2_GameClientConnect(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_CLIENT_CONNECT, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientConnect(spec);
}

//===========================================================================
// GamePutClientInServer
//===========================================================================
void PR2_GamePutClientInServer(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_PUT_CLIENT_IN_SERVER, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GamePutClientInServer(spec);
}

//===========================================================================
// GameClientDisconnect
//===========================================================================
void PR2_GameClientDisconnect(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_CLIENT_DISCONNECT, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientDisconnect(spec);
}

//===========================================================================
// GameClientPreThink
//===========================================================================
void PR2_GameClientPreThink(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_CLIENT_PRETHINK, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientPreThink(spec);
}

//===========================================================================
// GameClientPostThink
//===========================================================================
void PR2_GameClientPostThink(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_CLIENT_POSTTHINK, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientPostThink(spec);
}

//===========================================================================
// ClientCmd return false on unknown command
//===========================================================================
qbool PR2_ClientCmd()
{
	if (sv_vm)
		return VM_Call(sv_vm, GAME_CLIENT_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		return PR1_ClientCmd();
}

//===========================================================================
// ClientKill
//===========================================================================
void PR2_ClientKill()
{
	if (sv_vm)
		PR2_ClientCmd(); // PR2 have some universal way for command execution unlike QC based mods.
	else
		PR1_ClientKill();
}

//===========================================================================
// ClientSay return false if say unhandled by mod
//===========================================================================
qbool PR2_ClientSay(int isTeamSay, char *message)
{
	//
	// message - used for QC based mods only.
	// PR2 mods get it from Cmd_Args() and such.
	//

	if (sv_vm)
		return VM_Call(sv_vm, GAME_CLIENT_SAY, isTeamSay, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		return PR1_ClientSay(isTeamSay, message);
}

//===========================================================================
// GameSetNewParms
//===========================================================================
void PR2_GameSetNewParms()
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_SETNEWPARMS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameSetNewParms();
}

//===========================================================================
// GameSetNewParms
//===========================================================================
void PR2_GameSetChangeParms()
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_SETCHANGEPARMS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
	{
		PR1_GameSetChangeParms();
	}
}

//===========================================================================
// EdictTouch
//===========================================================================
void PR2_EdictTouch(func_t f)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_EDICT_TOUCH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_EdictTouch(f);
}

//===========================================================================
// EdictThink
//===========================================================================
void PR2_EdictThink(func_t f)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_EDICT_THINK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_EdictThink(f);
}

//===========================================================================
// EdictBlocked
//===========================================================================
void PR2_EdictBlocked(func_t f)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_EDICT_BLOCKED, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_EdictBlocked(f);
}

//===========================================================================
// UserInfoChanged
//===========================================================================
qbool PR2_UserInfoChanged()
{
	if (sv_vm)
		return VM_Call(sv_vm, GAME_CLIENT_USERINFO_CHANGED, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		return PR1_UserInfoChanged();
}

//===========================================================================
// GameShutDown
//===========================================================================
void PR2_GameShutDown()
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameShutDown();
}

//===========================================================================
// UnLoadProgs
//===========================================================================
void PR2_UnLoadProgs()
{
	if (sv_vm)
	{
		VM_Unload( sv_vm );
		sv_vm = NULL;
	}
	else
	{
		PR1_UnLoadProgs();
	}
}

//===========================================================================
// LoadProgs
//===========================================================================
void PR2_LoadProgs()
{
	sv_vm = (vm_t *) VM_Load(sv_vm, (vm_type_t) (int) sv_progtype.value, sv_progsname.string, sv_syscall, sv_sys_callex);

	if ( sv_vm )
	{
		; // nothing.
	}
	else
	{
		PR1_LoadProgs ();
	}
}

//===========================================================================
// GameConsoleCommand
//===========================================================================
void PR2_GameConsoleCommand(void)
{
	int     old_other, old_self;
	client_t	*cl;
	int			i;

	if( sv_vm )
	{
		old_self = pr_global_struct->self;
		old_other = pr_global_struct->other;
		pr_global_struct->other = 0; //sv_cmd = SV_CMD_CONSOLE;
		pr_global_struct->self = 0;

		for (i = 0, cl = svs.clients; i < MAX_CLIENTS; i++, cl++)
		{
			if (!cl->state)
				continue;
			if ( cl->isBot )
				continue;

			if (NET_CompareAdr(cl->netchan.remote_address, net_from))
			{
				pr_global_struct->self = EDICT_TO_PROG(cl->edict);
				break;
			}
		}
		VM_Call(sv_vm, GAME_CONSOLE_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		pr_global_struct->self = old_self;
		pr_global_struct->other = old_other;
	}
}

//===========================================================================
// PausedTic
//===========================================================================
void PR2_PausedTic(float duration)
{
	if (sv_vm)
		VM_Call(sv_vm, GAME_PAUSED_TIC, duration*1000, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_PausedTic(duration);
}

#endif /* USE_PR2 */
