/*

Copyright (C) 2001-2002       A Nourai

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

	$Id: ignore.c,v 1.7 2007-03-11 06:01:40 disconn3ct Exp $
*/

#include "quakedef.h"
#include "ignore.h"
#include "utils.h"
#include "teamplay.h"


#define MAX_TEAMIGNORELIST	4
#define	FLOODLIST_SIZE		10

cvar_t		ignore_spec				= {"ignore_spec", "0"};		
cvar_t		ignore_qizmo_spec		= {"ignore_qizmo_spec", "0"};
cvar_t      ignore_qtv              = {"ignore_qtv", "0"};
cvar_t		ignore_mode				= {"ignore_mode", "0"};
cvar_t		ignore_flood_duration	= {"ignore_flood_duration", "4"};
cvar_t		ignore_flood			= {"ignore_flood", "0"};		
cvar_t		ignore_opponents		= {"ignore_opponents", "0"};

char ignoreteamlist[MAX_TEAMIGNORELIST][16 + 1];

typedef struct flood_s {
	char data[2048];
	float time;
} flood_t;

static flood_t floodlist[FLOODLIST_SIZE];
static int		floodindex;

extern void PaddedPrint (char *s);

static qbool IsIgnored(int slot) {
	return cl.players[slot].ignored;
}

static void Display_Ignorelist(void) {
	int i;

	Com_Printf ("\x02" "User Ignore List:\n");
	for (i = 0; i < MAX_CLIENTS; i++)
		if (cl.players[i].name[0] && cl.players[i].ignored)
			PaddedPrint(cl.players[i].name);
	if (con.x)
		Com_Printf ("\n");

#ifdef FTE_PEXT2_VOICECHAT
	Com_Printf ("\x02" "VOIP Ignore List:\n");
	for (i = 0; i < MAX_CLIENTS; i++)
		if (cl.players[i].name[0] && cl.players[i].vignored && !cl.players[i].ignored)
			PaddedPrint(cl.players[i].name);
	if (con.x)
		Com_Printf ("\n");
#endif

	Com_Printf ("\x02" "Team Ignore List:\n");
	for (i = 0; i < MAX_TEAMIGNORELIST && ignoreteamlist[i][0]; i++)
		PaddedPrint(ignoreteamlist[i]);
	if (con.x)
		Com_Printf ("\n");

	if (ignore_opponents.value)
		Com_Printf("\x02" "Opponents are Ignored\n");

	if (ignore_spec.value == 2 || (ignore_spec.value == 1 && !cl.spectator))
		Com_Printf ("\x02" "Spectators are Ignored\n");

	if (ignore_qizmo_spec.value)
		Com_Printf("\x02" "Qizmo spectators are Ignored\n");

	if (ignore_qtv.integer)
		Com_Printf("\x02" "QuakeTV observers are Ignored\n");

	Com_Printf("\n");
}

static qbool Ignorelist_Add(int slot) {
	if (IsIgnored(slot))
		return false;

	cl.players[slot].ignored = true;
#ifdef FTE_PEXT2_VOICECHAT
	cl.players[slot].vignored = true;
	S_Voip_Ignore(slot, true);
#endif
	return true;
}

static qbool Ignorelist_Del (int slot)
{
#ifdef FTE_PEXT2_VOICECHAT
	if (cl.players[slot].vignored) {
		cl.players[slot].vignored = false;
		S_Voip_Ignore (slot, false);
	}
#endif

	if (cl.players[slot].ignored == false)
		return false;

	cl.players[slot].ignored = false;
	return true;
}

static void Ignore_f(void) {
	int c, slot;

	if ((c = Cmd_Argc()) == 1) {
		Display_Ignorelist();
		return;
	} else if (c != 2) {
		Com_Printf("Usage: %s [userid | name]\n", Cmd_Argv(0));
		return;
	}

	if ((slot = Player_StringtoSlot(Cmd_Argv(1))) == PLAYER_ID_NOMATCH) {
		Com_Printf("%s : no player with userid %d\n", Cmd_Argv(0), Q_atoi(Cmd_Argv(1)));
	}
	else if (slot == PLAYER_NAME_NOMATCH) {
		Com_Printf("%s : no player with name %s\n", Cmd_Argv(0), Cmd_Argv(1));
	}
	else if (Ignorelist_Add (slot)) {
		Com_Printf ("Added user %s to ignore list\n", cl.players[slot].name);
	}
	else {
		Com_Printf ("User %s is already ignored\n", cl.players[slot].name);
	}
}

static void IgnoreList_f(void) {
	if (Cmd_Argc() != 1)
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
	else
		Display_Ignorelist();
}

static void Ignore_ID_f(void) {
	int c, userid, i, slot;
	char *arg;

	if ((c = Cmd_Argc()) == 1) {
		Display_Ignorelist();
		return;
	} else if (c != 2) {
		Com_Printf("Usage: %s [userid]\n", Cmd_Argv(0));
		return;
	}
	arg = Cmd_Argv(1);
	for (i = 0; arg[i]; i++) {
		if (!isdigit(arg[i])) {
			Com_Printf("Usage: %s [userid]\n", Cmd_Argv(0));
			return;
		}
	}
	userid = Q_atoi(arg);
	if ((slot = Player_IdtoSlot(userid)) == PLAYER_ID_NOMATCH) {
		Com_Printf("%s : no player with userid %d\n", Cmd_Argv(0), userid);
		return;
	}
	if (Ignorelist_Add(slot))
		Com_Printf("Added user %s to ignore list\n", cl.players[slot].name);
	else
		Com_Printf ("User %s is already ignored\n", cl.players[slot].name);
}

static void Unignore_f(void) {
	int c, slot;

	if ((c = Cmd_Argc()) == 1) {
		Display_Ignorelist();
		return;
	} else if (c != 2) {
		Com_Printf("Usage: %s [userid | name]\n", Cmd_Argv(0));
		return;
	}

	if ((slot = Player_StringtoSlot(Cmd_Argv(1))) == PLAYER_ID_NOMATCH) {
		Com_Printf("%s : no player with userid %d\n", Cmd_Argv(0), Q_atoi(Cmd_Argv(1)));
	} else if (slot == PLAYER_NAME_NOMATCH) {
		Com_Printf("%s : no player with name %s\n", Cmd_Argv(0), Cmd_Argv(1));
	} else {		
		if (Ignorelist_Del(slot))
			Com_Printf("Removed user %s from ignore list\n", cl.players[slot].name);
		else
			Com_Printf("User %s is not being ignored\n", cl.players[slot].name);
	}
}

static void Unignore_ID_f(void) {
	int c, i, userid, slot;
	char *arg;

	if ((c = Cmd_Argc()) == 1) {
		Display_Ignorelist();
		return;
	} else if (c != 2) {
		Com_Printf("Usage: %s [userid]\n", Cmd_Argv(0));
		return;
	}
	arg = Cmd_Argv(1);
	for (i = 0; arg[i]; i++) {
		if (!isdigit(arg[i])) {
			Com_Printf("Usage: %s [userid]\n", Cmd_Argv(0));
			return;
		}
	}
	userid = Q_atoi(arg);
	if ((slot = Player_IdtoSlot(userid)) == PLAYER_ID_NOMATCH) {
		Com_Printf("%s : no player with userid %d\n", Cmd_Argv(0), userid);
		return;
	}
	if (Ignorelist_Del(slot))
		Com_Printf("Removed user %s from ignore list\n", cl.players[slot].name);
	else
		Com_Printf("User %s is not being ignored\n", cl.players[slot].name);
}

static void Ignoreteam_f(void) {
	int c, i, j;
	char *arg;

	c = Cmd_Argc();
	if (c == 1) {
		Display_Ignorelist();
		return;
	} else if (c != 2) {
		Com_Printf("Usage: %s [team]\n", Cmd_Argv(0));
		return;
	}
	arg = Cmd_Argv(1);
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator && !strcmp(arg, cl.players[i].team)) {
			for (j = 0; j < MAX_TEAMIGNORELIST && ignoreteamlist[j][0]; j++) {
				if (!strncmp(arg, ignoreteamlist[j], sizeof(ignoreteamlist[j]) - 1)) {
					Com_Printf ("Team %s is already ignored\n", arg);
					return;
				}
			}
			if (j == MAX_TEAMIGNORELIST)
				Com_Printf("You cannot ignore more than %d teams\n", MAX_TEAMIGNORELIST);
			strlcpy(ignoreteamlist[j], arg, sizeof(ignoreteamlist[j]));
			if (j + 1 < MAX_TEAMIGNORELIST)
				ignoreteamlist[j + 1][0] = 0;			
			Com_Printf("Added team %s to ignore list\n", arg);
			return;
		}
	}
	Com_Printf("%s : no team with name %s\n", Cmd_Argv(0), arg);
}

static void Unignoreteam_f(void) {
	int i, c, j;
	char *arg;

	c = Cmd_Argc();
	if (c == 1) {
		Display_Ignorelist();
		return;
	} else if (c != 2) {
		Com_Printf("Usage: %s [team]\n", Cmd_Argv(0));
		return;
	}	
	arg = Cmd_Argv(1);
	for (i = 0; i < MAX_TEAMIGNORELIST && ignoreteamlist[i][0]; i++) {
		if (!strncmp(arg, ignoreteamlist[i], sizeof(ignoreteamlist[i]) - 1)) {
			for (j = i; j < MAX_TEAMIGNORELIST && ignoreteamlist[j][0]; j++) 
				;
			if ( --j >  i)
				strlcpy(ignoreteamlist[i], ignoreteamlist[j], sizeof(ignoreteamlist[i]));
			ignoreteamlist[j][0] = 0;			
			Com_Printf("Removed team %s from ignore list\n", arg);
			return;
		}
	}
	Com_Printf("Team %s is not being ignored\n", arg);
}

static void UnignoreAll_f (void) {
	int i;

	if (Cmd_Argc() != 1) {
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
		return;
	}
	for (i = 0; i < MAX_CLIENTS; i++)
		cl.players[i].ignored = false;
	Com_Printf("User ignore list cleared\n");
}

static void UnignoreteamAll_f (void) {
	if (Cmd_Argc() != 1) {
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
		return;
	}
	ignoreteamlist[0][0] = 0;
	Com_Printf("Team ignore list cleared\n");
}

char Ignore_Check_Flood(char *s, int flags, int offset) {
	int i, p, q, len;
	char name[MAX_INFO_STRING];

	if ( !(  
	 ( (ignore_flood.value == 1 && (flags == msgtype_normal || flags == msgtype_spec)) ||
	 (ignore_flood.value == 2 && flags != msgtype_unknown) )
	   )  )
		return NO_IGNORE_NO_ADD;

	if (flags == msgtype_normal || flags == msgtype_spec) {
		p = 0;
		q = offset - 3;
	} else if (flags == msgtype_team) {
		p = 1;
		q = offset - 4;
	} else if (flags == msgtype_specteam) {
		p = 7;
		q = offset -3;
	} else
		return NO_IGNORE_NO_ADD;

	len = bound (0, q - p + 1, MAX_INFO_STRING - 1);

	strlcpy(name, s + p, len + 1);
	if (!cls.demoplayback && !strcmp(name, Player_MyName())) {
		return NO_IGNORE_NO_ADD;
	}
	for (i = 0; i < FLOODLIST_SIZE; i++) {
		if (floodlist[i].data[0] && !strncmp(floodlist[i].data, s, sizeof(floodlist[i].data) - 1) &&
			cls.realtime - floodlist[i].time < ignore_flood_duration.value) {
			return IGNORE_NO_ADD;
		}
	}
	return NO_IGNORE_ADD;
}

void Ignore_Flood_Add(char *s) {

	floodlist[floodindex].data[0] = 0;
	strlcpy(floodlist[floodindex].data, s, sizeof(floodlist[floodindex].data));
	floodlist[floodindex].time = cls.realtime;
	floodindex++;
	if (floodindex == FLOODLIST_SIZE)
		floodindex = 0;
}


qbool Ignore_Message(char *s, int flags, int offset) {
	int slot, i, p, q, len;	
	char name[32];

	if (!ignore_mode.value && (flags & msgtype_team))
		return false;		


	if (ignore_spec.value == 2 && (flags == msgtype_spec || flags == msgtype_specteam))
		return true;
	else if (ignore_spec.value == 1 && (flags == msgtype_spec) && !cl.spectator)
		return true;
	else if (ignore_qtv.integer && (flags & msgtype_qtv))
		return true;

	if (flags == msgtype_normal || flags == msgtype_spec) {
		p = 0;
		q = offset - 3;
	} else if (flags == msgtype_team) {
		p = 1;
		q = offset - 4;
	} else if (flags == msgtype_specteam) {
		p = 7;
		q = offset - 3;
	} else {
		return false;
	}

	len = bound (0, q - p + 1, sizeof(name) - 1);
	strlcpy(name, s + p, len + 1);

	if ((slot = Player_NametoSlot(name)) == PLAYER_NAME_NOMATCH)
		return false;	

	if (cl.players[slot].ignored)
		return true;
	

	if (ignore_opponents.value && (
			(int) ignore_opponents.value == 1 ||
			(cls.state >= ca_connected && !cl.standby && !cls.demoplayback && !cl.spectator) // match?
			) && 
		flags == msgtype_normal && !cl.spectator && slot != cl.playernum &&
		(!cl.teamplay || strcmp(cl.players[slot].team, cl.players[cl.playernum].team))
		)
		return true;


	if (!cl.teamplay)
		return false;

	if (cl.players[slot].spectator || !strcmp(Player_MyName(), name))
		return false;	

	for (i = 0; i < MAX_TEAMIGNORELIST && ignoreteamlist[i][0]; i++) {
		if (!strncmp(cl.players[slot].team, ignoreteamlist[i], sizeof(ignoreteamlist[i]) - 1))
			return true;
	}

	return false;
}

void Ignore_ResetFloodList(void) {
	int i;

	for (i = 0; i < FLOODLIST_SIZE; i++)
		floodlist[i].data[0] = 0;
	floodindex = 0;
}

#ifdef FTE_PEXT2_VOICECHAT
static qbool Ignorelist_VAdd(int slot)
{
	if (cl.players[slot].vignored)
		return false;

	cl.players[slot].vignored = true;
	S_Voip_Ignore(slot, true);
	return true;
}

static qbool IgnoreList_VDel (int slot)
{
	if (!cl.players[slot].vignored)
		return false;

	cl.players[slot].vignored = false;
	S_Voip_Ignore(slot, false);
	return true;
}

static void VIgnoreToggleCommand (qbool ignore)
{
	int c, slot;

	if ((c = Cmd_Argc()) == 1) {
		Display_Ignorelist();
		return;
	}
	else if (c != 2) {
		Con_Printf("Usage: %s [userid | name]\n", Cmd_Argv(0));
		return;
	}

	if ((slot = Player_StringtoSlot(Cmd_Argv(1))) == PLAYER_ID_NOMATCH) {
		Con_Printf("%s : no player with userid %d\n", Cmd_Argv(0), Q_atoi(Cmd_Argv(1)));
	}
	else if (slot == PLAYER_NAME_NOMATCH) {
		Con_Printf("%s : no player with name %s\n", Cmd_Argv(0), Cmd_Argv(1));
	}
	else if (ignore ? (Ignorelist_VAdd (slot)) : (IgnoreList_VDel (slot))) {
		Con_Printf ("%s user %s %s ignore list\n", ignore ? "Added" : "Removed", cl.players[slot].name, ignore ? "to" : "from");
	}
	else {
		Con_Printf ("User %s is %s ignored\n", cl.players[slot].name, ignore ? "already" : "not");
	}
}

static void VIgnore_f(void)
{
	VIgnoreToggleCommand (true);
}

static void VUnignore_f (void)
{
	VIgnoreToggleCommand (false);
}
#endif

void Ignore_Init(void) {
	int i;

	for (i = 0; i < MAX_TEAMIGNORELIST; i++)
		ignoreteamlist[i][0] = 0;
	Ignore_ResetFloodList();

	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register (&ignore_flood_duration);
	Cvar_Register (&ignore_flood);
	Cvar_Register (&ignore_spec);
	Cvar_Register (&ignore_qizmo_spec);
	Cvar_Register (&ignore_qtv);
	Cvar_Register (&ignore_mode);
	Cvar_Register (&ignore_opponents);

	Cvar_ResetCurrentGroup();

#ifdef FTE_PEXT2_VOICECHAT
	Cmd_AddCommand ("ignore_voip", VIgnore_f);
	Cmd_AddCommand ("unignore_voip", VUnignore_f);
#endif
	Cmd_AddCommand ("ignore", Ignore_f);					
	Cmd_AddCommand ("ignorelist", IgnoreList_f);			
	Cmd_AddCommand ("unignore", Unignore_f);				
	Cmd_AddCommand ("ignore_team", Ignoreteam_f);		
	Cmd_AddCommand ("unignore_team", Unignoreteam_f);	
	Cmd_AddCommand ("unignoreAll", UnignoreAll_f);			
	Cmd_AddCommand ("unignoreAll_team", UnignoreteamAll_f);	
	Cmd_AddCommand ("unignore_id", Unignore_ID_f);		
	Cmd_AddCommand ("ignore_id", Ignore_ID_f);			
}
