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
*/

#include "quakedef.h"
#include "winquake.h"
#include "utils.h"
#include "modules.h"

cvar_t	auth_validateclients	= {"auth_validate", "1"};		
cvar_t	auth_viewcrc			= {"auth_viewcrc", "0"};				
cvar_t	auth_warninvalid		= {"auth_warninvalid", "0"};

static void Auth_UnauthenticateClient(int userid);
static void Auth_AuthenticateClient(int userid);
static void Auth_Verify_Clients_f(void);
void Auth_Init(void);

#define AUTH_NOTHING	0
#define AUTH_BADFORMAT	1
#define AUTH_BADCRC		2
#define AUTH_GOODCRC	3


char *Auth_Generate_Crc(void) {
	static char hash[17], *failsafe = "";
	signed_buffer_t *p;

	if (!Modules_SecurityLoaded())
		return failsafe;

	p = Security_Generate_Crc();

	if (!VerifyData(p))
		return failsafe;

	memcpy(hash, (char *) (p->buf), sizeof(hash));

	return hash;
}

static qboolean verify_response(int index, unsigned char *hash) {
	int *n;
	signed_buffer_t *p;
	qboolean retval, failsafe = false;

	if (!Modules_SecurityLoaded())
		return failsafe;

	p = Security_Verify_Response(index, hash);

	if (!VerifyData(p))
		return failsafe;

	n = (int *) (p->buf);
	retval = *n ? true : false;
	return retval;
}

static int Auth_CheckString (char *id, char *s, int flags, int offset, int *out_slot, char *out_data, int out_size) {
	int len, slot;
	char name[32], *index;
	unsigned char hash[16];

	if (!Modules_SecurityLoaded())
		return AUTH_NOTHING;


	if (!auth_validateclients.value || flags != 1 || strncmp(s + offset, id, strlen(id)))
		return AUTH_NOTHING;

	len = bound (0, offset - 2, sizeof(name) - 1);
	Q_strncpyz(name, s, len + 1);

	if ((slot = Player_NametoSlot(name)) == PLAYER_NAME_NOMATCH)
		return AUTH_NOTHING;

	if (out_slot)
		*out_slot = slot;


	if (!(index = strstr(s + offset, "  crc: ")) || strlen(index) != 16 + 1 + 7 || index[16 + 7] != '\n')
		return AUTH_BADFORMAT;

	memcpy(hash, index + 7, 16);
	if (out_data)
		Q_strncpyz(out_data, s + offset + strlen(id), bound(1, index - (s + offset + strlen(id)) + 1, out_size));

	if (!auth_viewcrc.value) {
		index[0] = 0x0A;
		index[1] = 0;
	}

	return (verify_response(slot, hash)) ? AUTH_GOODCRC : AUTH_BADCRC;
}


static void Auth_CheckFServerResponse (char *s, int flags, int offset) {
	int response, slot;
	char data[16], *port;

	response = Auth_CheckString("FuhQuake f_server response: ", s, flags, offset, &slot, data, sizeof(data));
	if (response == AUTH_NOTHING || (!cls.demoplayback && slot == cl.playernum))
		return;

	if (response == AUTH_BADFORMAT || response == AUTH_BADCRC) {
		Auth_UnauthenticateClient(slot);
	} else if (response == AUTH_GOODCRC) {
		Auth_AuthenticateClient(slot);
		if ((port = strchr(data, ':')))
			*port = 0;
		Q_strncpyz(cl.players[slot].f_server, data, sizeof(cl.players[slot].f_server));
	}
}


static void Auth_UnauthenticateClient(int slot) {
	if (!strlen(Info_ValueForKey(cl.players[slot].userinfo, "*FuhQuake")) && auth_validateclients.value != 2)
		return;

	cl.players[slot].validated = false;
	cl.players[slot].f_server[0] = 0;
	if (auth_warninvalid.value)
		Com_Printf("Warning Invalid Client: User %s  (userid: %d)\n", cl.players[slot].name, Player_SlottoId(slot));
}

static void Auth_AuthenticateClient(int slot) {
	if (!strlen(Info_ValueForKey(cl.players[slot].userinfo, "*FuhQuake")) && auth_validateclients.value != 2)
		return;

	cl.players[slot].validated = true;
}

static void Auth_CheckAuthResponse (char *s, int flags, int offset) {
	int response, slot;

	response = Auth_CheckString("FuhQuake version ", s, flags, offset, &slot, NULL, 0);
	if (response == AUTH_NOTHING || (!cls.demoplayback && slot == cl.playernum))
		return;

	if (response == AUTH_BADFORMAT || response == AUTH_BADCRC)
		Auth_UnauthenticateClient(slot);
	else if (response == AUTH_GOODCRC)
		Auth_AuthenticateClient(slot);
}

static void Auth_Verify_Clients_f(void) {
	int i;
	int authClients[MAX_CLIENTS], unauthClients[MAX_CLIENTS], otherClients[MAX_CLIENTS];
	int authClientsCount, unauthClientsCount, otherClientsCount;
	extern void PaddedPrint (char *s);

	if (!Modules_SecurityLoaded()) {
		Com_Printf("Security module not initialized\n");
		return;
	}

	authClientsCount = unauthClientsCount = otherClientsCount = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator && (cls.demoplayback || i != cl.playernum)) {
			if (auth_validateclients.value != 2 && !strlen(Info_ValueForKey(cl.players[i].userinfo, "*FuhQuake"))) {
				otherClients[otherClientsCount++] = i;
			} else {
				if (cl.players[i].validated)
					authClients[authClientsCount++] = i;
				else if (auth_validateclients.value != 2)
					unauthClients[unauthClientsCount++] = i;
			}
		}
	}

	if (authClientsCount || auth_validateclients.value == 2) {
		Com_Printf ("\x02" "Authenticated FuhQuake Clients:\n");
		for (i = 0; i < authClientsCount; i++)
			PaddedPrint (cl.players[authClients[i]].name);
	}
	if (con.x)
		Com_Printf ("\n");

	if (auth_validateclients.value != 2) {

		if (unauthClientsCount) {
			Com_Printf ("\x02" "Unauthenticated FuhQuake Clients:\n");
			for (i = 0; i < unauthClientsCount; i++)
				PaddedPrint (cl.players[unauthClients[i]].name);
		}
		if (con.x)
			Com_Printf ("\n");

		if (otherClientsCount) {
			Com_Printf ("\x02" "Non FuhQuake Clients:\n");
			for (i = 0; i < otherClientsCount; i++)
				PaddedPrint (cl.players[otherClients[i]].name);
		}
		if (con.x)
			Com_Printf ("\n");
	}
	if ((auth_validateclients.value != 2 && authClientsCount + unauthClientsCount + otherClientsCount > 0) || auth_validateclients.value == 2)
		Com_Printf("\n");
}


void Auth_Init(void) {
	Cvar_SetCurrentGroup(CVAR_GROUP_CHAT);
	Cvar_Register(&auth_viewcrc);

	Cvar_ResetCurrentGroup();
	
	Cvar_Register(&auth_validateclients);
	Cvar_Register(&auth_warninvalid);

	Cmd_AddCommand ("validate_clients", Auth_Verify_Clients_f);
}

void Auth_CheckResponse (char *s, int flags, int offset) {
	Auth_CheckAuthResponse(s, flags, offset);
	Auth_CheckFServerResponse(s, flags, offset);
}
