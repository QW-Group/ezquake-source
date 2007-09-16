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

	$Id: auth.c,v 1.18 2007-09-16 14:03:03 disconn3ct Exp $
*/

#include "quakedef.h"
#include "modules.h"
#include "utils.h"
#include "version.h"


cvar_t auth_validateclients = {"auth_validate", "1"};		
cvar_t auth_viewcrc = {"auth_viewcrc", "0"};				
cvar_t auth_warninvalid = {"auth_warninvalid", "0"};

static void Auth_UnauthenticateClient (int userid);
static void Auth_AuthenticateClient (int userid);
static void Auth_Verify_Clients_f (void);

#define AUTH_NOTHING	0
#define AUTH_BADFORMAT	1
#define AUTH_BADCRC		2
#define AUTH_GOODCRC	3

extern void PaddedPrint (char *s);

char *Auth_Generate_Crc (void)
{
	static char hash[30];
	char *failsafe = "";
	signed_buffer_t *p;

	if (!Modules_SecurityLoaded ())
		return failsafe;

	p = Security_Generate_Crc (cl.playernum, cl.players[cl.playernum].userinfo, cl.serverinfo);

	if (!VerifyData(p))
		return failsafe;

	memcpy (hash, (char *) (p->buf), sizeof (hash));

	return hash;
}

static qbool verify_response (int index, char *hash)
{
	int *n;
	signed_buffer_t *p;
	qbool retval, failsafe = false;

	if (!Modules_SecurityLoaded ())
		return failsafe;

	p = Security_Verify_Response (index, hash, cl.players[index].userinfo, cl.serverinfo);

	if (!VerifyData (p))
		return failsafe;

	n = (int *) (p->buf);
	retval = *n ? true : false;

	return retval;
}

static int Auth_CheckString (char *id, wchar *i_hate_wide_chars, int flags, int offset, int *out_slot, char *out_data, int out_size)
{
#define HASH_SIZE 30
#define CRC_TEXT "  crc: "
	int len, slot;
	char name[32], *index;
	char hash[HASH_SIZE];
	char s[1024];
	wchar *windex;

	if (!Modules_SecurityLoaded ())
		return AUTH_NOTHING;

	snprintf (s, sizeof (s), "%s", wcs2str (i_hate_wide_chars));

	if (!auth_validateclients.integer || flags != 1 || strncmp (s + offset, id, strlen (id)))
		return AUTH_NOTHING;

	len = bound (0, offset - 2, sizeof (name) - 1);
	strlcpy (name, s, len + 1);

	if ((slot = Player_NametoSlot (name)) == PLAYER_NAME_NOMATCH)
		return AUTH_NOTHING;

	if (out_slot)
		*out_slot = slot;

	if (!(index = strstr(s + offset, CRC_TEXT)) ||
		strlen (index) != HASH_SIZE + strlen (CRC_TEXT) ||
		index[HASH_SIZE + strlen (CRC_TEXT) - 1] != '\n')
		return AUTH_BADFORMAT;

	memcpy (hash, index + strlen (CRC_TEXT), sizeof (hash));

	if (out_data)
		strlcpy (out_data, s + offset + strlen (id), bound (1, index - (s + offset + strlen (id)) + 1, out_size));

	if (!auth_viewcrc.integer) {
		windex = qwcsstr (i_hate_wide_chars + offset, str2wcs(CRC_TEXT));

		windex[0] = 0x0A;
		windex[1] = 0;
	}

	return (verify_response (slot, hash)) ? AUTH_GOODCRC : AUTH_BADCRC;
}


static void Auth_CheckFServerResponse (wchar *s, int flags, int offset)
{
	int response, slot;
	char data[16], *port;

	response = Auth_CheckString ("ezQuake f_server response: ", s, flags, offset, &slot, data, sizeof (data));
	if (response == AUTH_NOTHING || (!cls.demoplayback && slot == cl.playernum))
		return;

	if (response == AUTH_BADFORMAT || response == AUTH_BADCRC) {
		Auth_UnauthenticateClient(slot);
	} else if (response == AUTH_GOODCRC) {
		Auth_AuthenticateClient (slot);

		if ((port = strchr(data, ':')))
			*port = 0;

		strlcpy (cl.players[slot].f_server, data, sizeof (cl.players[slot].f_server));
	}
}

static void Auth_UnauthenticateClient (int slot)
{
	if (strncmp (Info_ValueForKey(cl.players[slot].userinfo, "*client"), "ezQuake", 7) && auth_validateclients.integer != 2)
		return;

	cl.players[slot].validated = false;
	cl.players[slot].f_server[0] = 0;
	if (auth_warninvalid.integer)
		Com_Printf ("Warning Invalid Client: User %s  (userid: %d)\n", cl.players[slot].name, Player_SlottoId (slot));
}

static void Auth_AuthenticateClient (int slot)
{
	if (strncmp (Info_ValueForKey(cl.players[slot].userinfo, "*client"), "ezQuake", 7) && auth_validateclients.integer != 2)
		return;

	cl.players[slot].validated = true;
}

static void Auth_CheckAuthResponse (wchar *s, int flags, int offset)
{
	int response, slot;

	response = Auth_CheckString ("ezQuake " VERSION_NUMBER " ", s, flags, offset, &slot, NULL, 0);
	if (response == AUTH_NOTHING || (!cls.demoplayback && slot == cl.playernum))
		return;

	if (response == AUTH_BADFORMAT || response == AUTH_BADCRC)
		Auth_UnauthenticateClient (slot);
	else if (response == AUTH_GOODCRC)
		Auth_AuthenticateClient (slot);
}

static void Auth_Verify_Clients_f(void)
{
	int i;
	int authClients[MAX_CLIENTS], unauthClients[MAX_CLIENTS], otherClients[MAX_CLIENTS];
	int authClientsCount, unauthClientsCount, otherClientsCount;

	if (!Modules_SecurityLoaded ()) {
		Com_Printf_State (PRINT_FAIL, "Security module not initialized\n");
		return;
	}

	authClientsCount = unauthClientsCount = otherClientsCount = 0;
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0] && !cl.players[i].spectator && (cls.demoplayback || i != cl.playernum)) {
			if (auth_validateclients.integer != 2 && strncmp (Info_ValueForKey (cl.players[i].userinfo, "*client"), "ezQuake", 7)) {
				otherClients[otherClientsCount++] = i;
			} else {
				if (cl.players[i].validated)
					authClients[authClientsCount++] = i;
				else if (auth_validateclients.integer != 2)
					unauthClients[unauthClientsCount++] = i;
			}
		}
	}

	if (authClientsCount || auth_validateclients.integer == 2) {
		Com_Printf ("\x02" "Authenticated ezQuake Clients:\n");
		for (i = 0; i < authClientsCount; i++)
			PaddedPrint (cl.players[authClients[i]].name);
	}

	if (con.x)
		Com_Printf ("\n");

	if (auth_validateclients.integer != 2) {
		if (unauthClientsCount) {
			Com_Printf ("\x02" "Unauthenticated ezQuake Clients:\n");
			for (i = 0; i < unauthClientsCount; i++)
				PaddedPrint (cl.players[unauthClients[i]].name);
		}

		if (con.x)
			Com_Printf ("\n");

		if (otherClientsCount) {
			Com_Printf ("\x02" "Non ezQuake Clients:\n");
			for (i = 0; i < otherClientsCount; i++)
				PaddedPrint (cl.players[otherClients[i]].name);
		}

		if (con.x)
			Com_Printf ("\n");
	}

	if ((auth_validateclients.integer != 2 && authClientsCount + unauthClientsCount + otherClientsCount > 0) || auth_validateclients.integer == 2)
		Com_Printf ("\n");
}

void Auth_Init (void)
{
	Cvar_SetCurrentGroup (CVAR_GROUP_CHAT);
	Cvar_Register (&auth_viewcrc);
	Cvar_ResetCurrentGroup ();

	Cvar_Register (&auth_validateclients);
	Cvar_Register (&auth_warninvalid);

	Cmd_AddCommand ("validate_clients", Auth_Verify_Clients_f);
}

void Auth_CheckResponse (wchar *s, int flags, int offset)
{
	Auth_CheckAuthResponse (s, flags, offset);
	Auth_CheckFServerResponse (s, flags, offset);
}
