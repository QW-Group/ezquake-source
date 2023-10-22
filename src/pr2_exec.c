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

#ifndef CLIENTONLY
#ifdef USE_PR2

#include "qwsvdef.h"
#include "vm_local.h"

gameData_t gamedata;
extern field_t *fields;

// 0 = pr1 (qwprogs.dat etc), 1 = native (.so/.dll), 2 = q3vm (.qvm), 3 = q3vm (.qvm) with JIT
cvar_t sv_progtype = { "sv_progtype","0" };

// 0 = standard, 1 = pr2 mods set string_t fields as byte offsets to location of actual strings
cvar_t sv_pr2references = {"sv_pr2references", "0"};

void ED2_PrintEdicts (void);
void PR2_Profile_f (void);
void ED2_PrintEdict_f (void);
void ED_Count (void);
void VM_VmInfo_f( void );

void PR2_Init(void)
{
	int p;
	int usedll;
	Cvar_Register(&sv_progtype);
	Cvar_Register(&sv_progsname);
	Cvar_Register(&sv_pr2references);
	Cvar_Register(&vm_rtChecks);
#ifdef WITH_NQPROGS
	Cvar_Register(&sv_forcenqprogs);
#endif

	p = SV_CommandLineProgTypeArgument();

	if (p && p < COM_Argc())
	{
		usedll = Q_atoi(COM_Argv(p + 1));

		if (usedll > VMI_COMPILED || usedll < VMI_NONE)
			usedll = VMI_NONE;
		Cvar_SetValue(&sv_progtype,usedll);
	}

	Cmd_AddCommand ("edict", ED2_PrintEdict_f);
	Cmd_AddCommand ("edicts", ED2_PrintEdicts);
	Cmd_AddCommand ("edictcount", ED_Count);
	Cmd_AddCommand ("profile", PR2_Profile_f);
	Cmd_AddCommand ("mod", PR2_GameConsoleCommand);

	Cmd_AddCommand ("vminfo", VM_VmInfo_f);
	memset(pr_newstrtbl, 0, sizeof(pr_newstrtbl));
}

void PR2_Profile_f(void)
{
	if(!sv_vm)
	{
		PR_Profile_f();
		return;
	}
}

//===========================================================================
// PR2_GetString: only called to get direct addresses now
//===========================================================================
char *PR2_GetString(intptr_t num)
{
	if(!sv_vm)
		return PR1_GetString(num);

	switch (sv_vm->type)
	{
	case VMI_NONE:
		return PR1_GetString(num);

	case VMI_NATIVE:
		if (num) {
			return (char *)num;
		}
		else {
			return "";
		}

	case VMI_BYTECODE:
	case VMI_COMPILED:
		if (num <= 0)
			return "";
		return VM_ExplicitArgPtr(sv_vm, num);
	}

	return NULL;
}

intptr_t PR2_EntityStringLocation(string_t offset, int max_size)
{
	if (offset > 0 && offset < pr_edict_size * sv.max_edicts - max_size) {
		return ((intptr_t)sv.game_edicts + offset);
	}

	return 0;
}

intptr_t PR2_GlobalStringLocation(string_t offset)
{
	// FIXME: the mod has allocated this memory, don't have max size
	if (offset > 0) {
		return ((intptr_t)pr_global_struct + offset);
	}

	return 0;
}

char *PR2_GetEntityString(string_t num)
{

	if(!sv_vm)
		return PR1_GetString(num);

	switch (sv_vm->type)
	{
	case VMI_NONE:
		return PR1_GetString(num);

	case VMI_NATIVE:
		if (num) {
			if (sv_vm->pr2_references) {
				char** location = (char**)PR2_EntityStringLocation(num, sizeof(char*));
				if (location && *location) {
					return *location;
				}
			}
#ifndef idx64
			else {
				return (char *) (num);
			}
#endif
		}
		return "";

	case VMI_BYTECODE:
	case VMI_COMPILED:
		if (num <= 0)
			return "";
		if (sv_vm->pr2_references) {
			num = *(string_t*)PR2_EntityStringLocation(num, sizeof(string_t));
		}
		return VM_ExplicitArgPtr(sv_vm, num);
	}

	return NULL;
}

//===========================================================================
// PR2_SetString
// !!!!IMPOTANT!!!!
// Server change string pointers in mod memory only in trapcall(strings passed from mod, and placed in mod memory).
// Never pass pointers outside of the mod memory to mod, this does not work in QVM in 64 bit server.
//===========================================================================
void PR2_SetEntityString(edict_t* ed, string_t* target, char* s)
{
	if (!sv_vm) {
		PR1_SetString(target, s);
		return;
	}
}
void PR2_SetGlobalString(string_t* target, char* s)
{
	if (!sv_vm) {
		PR1_SetString(target, s);
		return;
	}
}

/*
=================
PR2_LoadEnts
=================
*/
extern const char *pr2_ent_data_ptr;

void PR2_LoadEnts(char *data)
{
	if (sv_vm)
	{
		pr2_ent_data_ptr = data;

		//Init parse
		VM_Call(sv_vm, 0, GAME_LOADENTS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	}
	else
	{
		PR1_LoadEnts(data);
	}
}

//===========================================================================
// GameStartFrame
//===========================================================================
void PR2_GameStartFrame(qbool isBotFrame)
{
	if (isBotFrame && (!sv_vm || sv_vm->type == VMI_NONE || gamedata.APIversion < 15)) {
		return;
	}

	if (sv_vm)
		VM_Call(sv_vm, 2, GAME_START_FRAME, (int) (sv.time * 1000), (int)isBotFrame, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameStartFrame();
}

//===========================================================================
// GameClientConnect
//===========================================================================
void PR2_GameClientConnect(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, 1, GAME_CLIENT_CONNECT, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientConnect(spec);
}

//===========================================================================
// GamePutClientInServer
//===========================================================================
void PR2_GamePutClientInServer(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, 1, GAME_PUT_CLIENT_IN_SERVER, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GamePutClientInServer(spec);
}

//===========================================================================
// GameClientDisconnect
//===========================================================================
void PR2_GameClientDisconnect(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, 1, GAME_CLIENT_DISCONNECT, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientDisconnect(spec);
}

//===========================================================================
// GameClientPreThink
//===========================================================================
void PR2_GameClientPreThink(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, 1, GAME_CLIENT_PRETHINK, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientPreThink(spec);
}

//===========================================================================
// GameClientPostThink
//===========================================================================
void PR2_GameClientPostThink(int spec)
{
	if (sv_vm)
		VM_Call(sv_vm, 1, GAME_CLIENT_POSTTHINK, spec, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameClientPostThink(spec);
}

//===========================================================================
// ClientCmd return false on unknown command
//===========================================================================
qbool PR2_ClientCmd(void)
{
	if (sv_vm)
		return VM_Call(sv_vm, 0, GAME_CLIENT_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		return PR1_ClientCmd();
}

//===========================================================================
// ClientKill
//===========================================================================
void PR2_ClientKill(void)
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
		return VM_Call(sv_vm, 1, GAME_CLIENT_SAY, isTeamSay, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		return PR1_ClientSay(isTeamSay, message);
}

//===========================================================================
// GameSetNewParms
//===========================================================================
void PR2_GameSetNewParms(void)
{
	if (sv_vm)
		VM_Call(sv_vm, 0, GAME_SETNEWPARMS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameSetNewParms();
}

//===========================================================================
// GameSetNewParms
//===========================================================================
void PR2_GameSetChangeParms(void)
{
	if (sv_vm)
		VM_Call(sv_vm, 0, GAME_SETCHANGEPARMS, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
		VM_Call(sv_vm, 0, GAME_EDICT_TOUCH, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_EdictTouch(f);
}

//===========================================================================
// EdictThink
//===========================================================================
void PR2_EdictThink(func_t f)
{
	if (sv_vm)
		VM_Call(sv_vm, 0, GAME_EDICT_THINK, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_EdictThink(f);
}

//===========================================================================
// EdictBlocked
//===========================================================================
void PR2_EdictBlocked(func_t f)
{
	if (sv_vm)
		VM_Call(sv_vm, 0, GAME_EDICT_BLOCKED, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_EdictBlocked(f);
}

//===========================================================================
// UserInfoChanged
//===========================================================================
qbool PR2_UserInfoChanged(int after)
{
	if (sv_vm)
		return VM_Call(sv_vm, 1, GAME_CLIENT_USERINFO_CHANGED, after, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		return PR1_UserInfoChanged(after);
}

//===========================================================================
// GameShutDown
//===========================================================================
void PR2_GameShutDown(void)
{
	if (sv_vm)
		VM_Call(sv_vm, 0, GAME_SHUTDOWN, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_GameShutDown();
}

//===========================================================================
// UnLoadProgs
//===========================================================================
void PR2_UnLoadProgs(void)
{
	if (sv_vm)
	{
		VM_Free( sv_vm );
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
void PR2_LoadProgs(void)
{
	sv_vm = VM_Create(VM_GAME, sv_progsname.string, PR2_GameSystemCalls, sv_progtype.value );

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
		VM_Call(sv_vm, 0, GAME_CONSOLE_COMMAND, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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
		VM_Call(sv_vm, 1, GAME_PAUSED_TIC, (int)(duration*1000), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	else
		PR1_PausedTic(duration);
}

void PR2_ClearEdict(edict_t* e)
{
	if (sv_vm && sv_vm->pr2_references && (sv_vm->type == VMI_NATIVE || sv_vm->type == VMI_BYTECODE || sv_vm->type == VMI_COMPILED)) {
		int old_self = pr_global_struct->self;
		pr_global_struct->self = EDICT_TO_PROG(e);
		VM_Call(sv_vm, 0, GAME_CLEAR_EDICT, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
		pr_global_struct->self = old_self;
	}
}

//===========================================================================
// InitProgs
//===========================================================================

#define GAME_API_VERSION_MIN 16

void LoadGameData(intptr_t gamedata_ptr)
{
#ifdef idx64
	gameData_vm_t* gamedata_vm;

	if (sv_vm->type == VMI_BYTECODE || sv_vm->type == VMI_COMPILED)
	{
		gamedata_vm = (gameData_vm_t *)PR2_GetString(gamedata_ptr);
		gamedata.ents = (intptr_t)gamedata_vm->ents_p;
		gamedata.global = (intptr_t)gamedata_vm->global_p;
		gamedata.fields = (intptr_t)gamedata_vm->fields_p;
		gamedata.APIversion = gamedata_vm->APIversion;
		gamedata.sizeofent = gamedata_vm->sizeofent;
		gamedata.maxentities = gamedata_vm->maxentities;
		return;
	}
#endif
	gamedata = *(gameData_t *)PR2_GetString(gamedata_ptr);
}

void LoadFields(void)
{
#ifdef idx64
	if (sv_vm->type == VMI_BYTECODE || sv_vm->type == VMI_COMPILED)
	{
		field_vm_t *fieldvm_p;
		field_t	*f;
		int num = 0;
		fieldvm_p = (field_vm_t*)PR2_GetString((intptr_t)gamedata.fields);
		while (fieldvm_p[num].name) {
			num++;
		}
		f = fields = (field_t *)Hunk_Alloc(sizeof(field_t) * (num + 1));
		while (fieldvm_p->name){
			f->name = (stringptr_t)fieldvm_p->name;
			f->ofs =  fieldvm_p->ofs;
			f->type = (fieldtype_t)fieldvm_p->type;
			f++;
			fieldvm_p++;
		}
		f->name = 0;
		return;
	}
#endif
	fields = (field_t*)PR2_GetString((intptr_t)gamedata.fields);
}

extern void PR2_FS_Restart(void);

void PR2_InitProg(void)
{
	extern cvar_t sv_pr2references;

	intptr_t gamedata_ptr;

	Cvar_SetValue(&sv_pr2references, 0.0f);

	if (!sv_vm) {
		PR1_InitProg();
		return;
	}

	PR2_FS_Restart();

	gamedata.APIversion = 0;
	gamedata_ptr = (intptr_t) VM_Call(sv_vm, 2, GAME_INIT, (int)(sv.time * 1000), (int)(Sys_DoubleTime() * 100000), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
	if (!gamedata_ptr) {
		SV_Error("PR2_InitProg: gamedata == NULL");
	}

	LoadGameData(gamedata_ptr);
	if (gamedata.APIversion < GAME_API_VERSION_MIN || gamedata.APIversion > GAME_API_VERSION) {
		if (GAME_API_VERSION_MIN == GAME_API_VERSION) {
			SV_Error("PR2_InitProg: Incorrect API version (%i should be %i)", gamedata.APIversion, GAME_API_VERSION);
		}
		else {
			SV_Error("PR2_InitProg: Incorrect API version (%i should be between %i and %i)", gamedata.APIversion, GAME_API_VERSION_MIN, GAME_API_VERSION);
		}
	}

	sv_vm->pr2_references = gamedata.APIversion >= 15 && (int)sv_pr2references.value;
#ifdef idx64
	if (sv_vm->type == VMI_NATIVE && (!sv_vm->pr2_references || gamedata.APIversion < 15))
		SV_Error("PR2_InitProg: Native prog must support sv_pr2references for 64bit mode (mod API version (%i should be 15+))", gamedata.APIversion);
#endif
	pr_edict_size = gamedata.sizeofent;
	Con_DPrintf("edict size %d\n", pr_edict_size);
	sv.game_edicts = (entvars_t *)(PR2_GetString((intptr_t)gamedata.ents));
	pr_global_struct = (globalvars_t*)PR2_GetString((intptr_t)gamedata.global);
	pr_globals = (float *)pr_global_struct;
	LoadFields();

	sv.max_edicts = MAX_EDICTS;
	if (gamedata.APIversion >= 14) {
		sv.max_edicts = min(sv.max_edicts, gamedata.maxentities);
	}
	else {
		sv.max_edicts = min(sv.max_edicts, 512);
	}
}

#endif /* USE_PR2 */

#endif // !CLIENTONLY
