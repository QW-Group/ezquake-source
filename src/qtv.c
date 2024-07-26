/*
Copyright (C) 2011 qqshka

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
/*
	Support for FTE QuakeTV

	$Id: qtv.c,v 1.20 2007-10-28 02:45:19 qqshka Exp $
*/

#include "quakedef.h"
#include "qtv.h"
#include "input.h"
#include "teamplay.h"
#include "fs.h"
#include "utils.h"

cvar_t  qtv_buffertime       = { "qtv_buffertime",       "0.5" };
cvar_t  qtv_prebuffertime    = { "qtv_prebuffertime",    "0" };
cvar_t  qtv_chatprefix       = { "qtv_chatprefix",       "$[{QTV}$] " };
cvar_t  qtv_gamechatprefix   = { "qtv_gamechatprefix",   "$[{QTV>game}$] " };
cvar_t  qtv_skipchained      = { "qtv_skipchained",      "1" };
cvar_t  qtv_adjustbuffer     = { "qtv_adjustbuffer",     "1" };
cvar_t  qtv_adjustminspeed   = { "qtv_adjustminspeed",   "0" };
cvar_t  qtv_adjustmaxspeed   = { "qtv_adjustmaxspeed",   "999" };
cvar_t  qtv_adjustlowstart   = { "qtv_adjustlowstart",   "0.3" };
cvar_t  qtv_adjusthighstart  = { "qtv_adjusthighstart",  "1" };
cvar_t  qtv_say_team         = { "qtv_say_team",         "0" };
cvar_t  qtv_allow_pause      = { "qtv_allow_pause",      "0" }; // ignore cl_demospeed during QTV playback by default

cvar_t  qtv_event_join       = { "qtv_event_join",        " &c2F2joined&r"};
cvar_t  qtv_event_leave      = { "qtv_event_leave",       " &cF22left&r"};
cvar_t  qtv_event_changename = { "qtv_event_changename",  " &cFF0changed name to&r "};

extern qbool qtv_playback_paused;

void Qtvusers_f (void);
void QtvStartDelay_f(void);
void QtvEndDelay_f(void);

void QTV_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_QTV);

	Cvar_Register(&qtv_buffertime);
	Cvar_Register(&qtv_prebuffertime);
	Cvar_Register(&qtv_chatprefix);
	Cvar_Register(&qtv_gamechatprefix);
	Cvar_Register(&qtv_skipchained);
	Cvar_Register(&qtv_adjustbuffer);
	Cvar_Register(&qtv_adjustminspeed);
	Cvar_Register(&qtv_adjustmaxspeed);
	Cvar_Register(&qtv_adjustlowstart);
	Cvar_Register(&qtv_adjusthighstart);
	Cvar_Register(&qtv_say_team);
	Cvar_Register(&qtv_allow_pause);

	Cvar_Register(&qtv_event_join);
	Cvar_Register(&qtv_event_leave);
	Cvar_Register(&qtv_event_changename);

	Cvar_ResetCurrentGroup();

	Cmd_AddCommand("qtvusers", Qtvusers_f);
	Cmd_AddCommand("+qtv_delay", QtvStartDelay_f);
	Cmd_AddCommand("-qtv_delay", QtvEndDelay_f);
}

//=================================================

char *QTV_CL_HEADER(float qtv_ver, int qtv_ezquake_ext)
{
	static char header[1024];

	snprintf(header, sizeof(header), "QTV\n" "VERSION: %g\n" QTV_EZQUAKE_EXT ": %d\n", qtv_ver, qtv_ezquake_ext);

	return header;
}

//=================================================

// ripped from FTEQTV, original name is SV_ConsistantMVDData
// return non zero if we have at least one message
// ms - will contain ms
int ConsistantMVDDataEx(unsigned char *buffer, int remaining, int *ms, int max_messages)
{
	qbool warn = true;
	int lengthofs;
	int length;
	int available = 0;

	if (ms) {
		ms[0] = 0;
	}

	while ( 1 ) {
		if (remaining < 2) {
			return available;
		}

		//buffer[0] is time

		switch (buffer[1]&dem_mask)
		{
		case dem_set:
			length = 10;
			goto gottotallength;
		case dem_multiple:
			lengthofs = 6;
			break;
		default:
			lengthofs = 2;
			break;
		}

		if (lengthofs+4 > remaining) {
			return available;
		}

		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);

		if (length > MAX_MVD_SIZE && warn) {
			Com_Printf("Corrupt mvd, length: %d\n", length);
			warn = false;
		}

		length += lengthofs+4;

gottotallength:
		if (remaining < length) {
			return available;
		}

		if (ms) {
			ms[0] += buffer[0];
		}
			
		remaining -= length;
		available += length;
		buffer    += length;

		if (max_messages && available >= max_messages) {
			return available;
		}
	}
}

int ConsistantMVDData(unsigned char *buffer, int remaining, int max_packets)
{
	return ConsistantMVDDataEx(buffer, remaining, NULL, max_packets);
}

//=================================================

extern vfsfile_t *playbackfile;

void QTV_ForwardToServerEx (qbool skip_if_no_params, qbool use_first_argument)
{
	char data[1024 + 100] = {0}, text[1024], *s;
	sizebuf_t buf;

	if (    cls.mvdplayback != QTV_PLAYBACK
		|| !playbackfile /* || cls.qtv_ezquake_ext & QTV_EZQUAKE_EXT_CLC_STRINGCMD ???*/
	   )
		return;

	if (skip_if_no_params)
		if (Cmd_Argc() < 2)
			return;

	// lowercase command
	for (s = Cmd_Argv(0); *s; s++)
		*s = (char) tolower(*s);

	if (cls.state == ca_disconnected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (strcmp(Cmd_Argv(0), "say_team") == 0 && !qtv_say_team.integer) {
		Com_Printf("Cannot send team messages. Use qtv_say_team 1 to override.\n");
		return;
	}

	SZ_Init(&buf, (byte*) data, sizeof(data));

	s = TP_ParseMacroString (Cmd_Args());
	s = TP_ParseFunChars (s, true);

	text[0] = 0; // *cat is dangerous, ensure we empty buffer before use it

	if (use_first_argument)
		strlcat(text, Cmd_Argv(0), sizeof(text));

	if (s[0])
	{
		strlcat(text, " ", sizeof(text));
		strlcat(text, s,   sizeof(text));
	}

	MSG_WriteShort  (&buf, 2 + 1 + strlen(text) + 1); // short + byte + null terminated string
	MSG_WriteByte   (&buf, qtv_clc_stringcmd);
	MSG_WriteString (&buf, text);

	VFS_WRITE(playbackfile, buf.data, buf.cursize);
}

#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize( "", off )
#endif
void QTV_Say_f (void)
{
	tokenizecontext_t tmpcontext;

	// save context, so we can later restore it
	Cmd_SaveContext(&tmpcontext);

	// in our last tests, this check was not necessary
	// and even lead to issues
	// so we are disabling it temporarily to see if everything works ok without it
#if 0
	// get rid of quotes, if any
	char *s = Cmd_Args();
	if (0 && s[0] == '\"' && s[(len = strlen(s))-1] == '\"' && len > 2)
	{
		int len;
		char text[1024] = {0};
		snprintf(text, sizeof(text), "%s %s", Cmd_Argv(0), s + 1);
		if ((len = strlen(text)))
			text[len - 1] = 0;
		Cmd_TokenizeString(text);
	}
#endif

	QTV_ForwardToServerEx (true, true);

	// restore
	Cmd_RestoreContext(&tmpcontext);
}
#if defined(_MSC_VER) && !defined(__clang__)
#pragma optimize( "", on )
#endif

void QTV_Cmd_ForwardToServer (void)
{
	QTV_ForwardToServerEx (false, true);
}

// don't forward the first argument
void QTV_Cl_ForwardToServer_f (void)
{
	QTV_ForwardToServerEx (false, false);
}

void QTV_Cmd_Printf(int qtv_ext, char *fmt, ...)
{
	va_list argptr;
	char msg[1024] = {0};
	tokenizecontext_t tmpcontext;

	if (cls.mvdplayback != QTV_PLAYBACK || (qtv_ext & cls.qtv_ezquake_ext) != qtv_ext)
		return; // no point for this, since it not qtv playback or qtv server do not support it

	va_start (argptr, fmt);
	vsnprintf (msg, sizeof(msg), fmt, argptr);
	va_end (argptr);

	// save context, so we can later restore it
	Cmd_SaveContext(&tmpcontext);

	Cmd_TokenizeString(msg);
	QTV_Cmd_ForwardToServer ();

	// restore
	Cmd_RestoreContext(&tmpcontext);
}

//=================================================

static qtvuser_t *qtvuserlist = NULL;

static qtvuser_t *QTV_UserById(int id)
{
	qtvuser_t *current;

	for (current = qtvuserlist; current; current = current->next)
		if (current->id == id)
			return current;

	return NULL;
}

static void QTV_SetUser(qtvuser_t *to, qtvuser_t *from)
{
	*to = *from;
}

// allocate data and set fields, perform linkage to qtvuserlist
// Well, instead of QTV_NewUser(int id, char *name, ...) I pass params with single qtvuser_t *user struct, well its OK for current struct.
static qtvuser_t *QTV_NewUser(qtvuser_t *user)
{
	// check, may be user alredy exist, so reuse it
	qtvuser_t *newuser = QTV_UserById(user->id);

	if (!newuser)
	{
		// user does't exist, alloc data
		newuser = Q_malloc(sizeof(*newuser));

		QTV_SetUser(newuser, user);

		// perform linkage
		newuser->next = qtvuserlist;
		qtvuserlist = newuser;
	}
	else
	{
		// we do not need linkage, just save current
		qtvuser_t *oldnext = newuser->next; // we need save this before assign all fields

		QTV_SetUser(newuser, user);

		newuser->next = oldnext;
	}

	return newuser;
}

// free data, perform unlink if requested
static void QTV_FreeUser(qtvuser_t *user, qbool unlink)
{
	if (!user)
		return;

	if (unlink)
	{
		qtvuser_t *next, *prev, *current;

		prev = NULL;
		current = qtvuserlist;

		for ( ; current; )
		{
			next = current->next;

			if (user == current)
			{
				if (prev)
					prev->next = next;
				else
					qtvuserlist = next;

				break;
			}

			prev = current;
			current = next;
		}
	}

	Q_free(user);
}

// free whole qtvuserlist
void QTV_FreeUserList(void)
{
	qtvuser_t *next, *current;

	current = qtvuserlist;

	for ( ; current; current = next)
	{
		next = current->next;
		QTV_FreeUser(current, false);
	}

	qtvuserlist = NULL;
}

#define QTV_EVENT_PREFIX "QTV: "

// user join qtv
void QTV_JoinEvent(qtvuser_t *user)
{
	// make it optional message
	if (!qtv_event_join.string[0])
		return;

	// do not show "user joined" at moment of connection to QTV, it mostly QTV just spammed userlist to us.
	if (cls.state <= ca_demostart)
		return;

	if (QTV_UserById(user->id))
	{
		// we alredy have this user, do not double trigger
		return;
	}

	Com_Printf("%s%s%s\n", QTV_EVENT_PREFIX, user->name, qtv_event_join.string);
}

// user leaved/left qtv
void QTV_LeaveEvent(qtvuser_t *user)
{
	qtvuser_t *olduser;

	// make it optional message
	if (!qtv_event_leave.string[0])
		return;

	if (!(olduser = QTV_UserById(user->id)))
	{
		// we do not have this user
		return;
	}

	Com_Printf("%s%s%s\n", QTV_EVENT_PREFIX, olduser->name, qtv_event_leave.string);
}

// user changed name on qtv
void QTV_ChangeEvent(qtvuser_t *user)
{
	qtvuser_t *olduser;

	// well, too spammy, make it as option
	if (!qtv_event_changename.string[0])
		return;

	if (!(olduser = QTV_UserById(user->id)))
	{
		// we do not have this user yet
		Com_DPrintf("qtv: change event without olduser\n");
		return;
	}

	Com_Printf("%s%s%s%s\n", QTV_EVENT_PREFIX, olduser->name, qtv_event_changename.string, user->name);
}

void Parse_QtvUserList(char *s)
{
	qtvuser_t		tmpuser;
	qtvuserlist_t	action;
	int				cnt = 1;
	
	memset(&tmpuser, 0, sizeof(tmpuser));

	// action id [\"name\"]

	Cmd_TokenizeString( s );

	action 		= atoi( Cmd_Argv( cnt++ ) );
	tmpuser.id	= atoi( Cmd_Argv( cnt++ ) );
	strlcpy(tmpuser.name, Cmd_Argv( cnt++ ), sizeof(tmpuser.name)); // name is optional in some cases

	switch ( action )
	{
		case QUL_ADD:
			QTV_JoinEvent(&tmpuser);
			QTV_NewUser(&tmpuser);

		break;
		
		case QUL_CHANGE:
			QTV_ChangeEvent(&tmpuser);
			QTV_NewUser(&tmpuser);

		break;

		case QUL_DEL:
			QTV_LeaveEvent(&tmpuser);
			QTV_FreeUser(QTV_UserById(tmpuser.id), true);

		break;

		default:
			Com_Printf("Parse_QtvUserList: unknown action %d\n", action);

		return;
	}
}

// FIXME: make sexy GUI instead!
void Qtvusers_f (void)
{
	qtvuser_t *current;
	int c;

	if (cls.state == ca_disconnected)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.mvdplayback != QTV_PLAYBACK)
	{
		CL_SendClientCommand(true, "%s %s", Cmd_Argv(0), Cmd_Args());
		return;
	}

	c = 0;
	Com_Printf ("userid name\n");
	Com_Printf ("------ ----\n");

	for (current = qtvuserlist; current; current = current->next)
	{
		Com_Printf ("%6i %s\n", current->id, current->name);
		c++;
	}

	Com_Printf ("%i total users\n", c);
}

qbool QTV_FindBestNick (const char *nick, char *result, size_t result_len)
{
	int best = 999999;
	char name[128], *match;

	qtvuser_t *current = NULL, *bestplayer = NULL;

	result[0] = 0;

	for (current = qtvuserlist; current; current = current->next)
	{
		if (!current->name[0])
			continue;

		strlcpy(name, current->name, sizeof(name));
		RemoveColors(name, sizeof (name));
		for (match = name; match[0]; match++)
			match[0] = tolower(match[0]);

		if (!name[0])
			continue;

		if ((match = strstr(name, nick)) && match - name < best)
		{
			best = match - name;
			bestplayer = current;
		}
	}

	if (bestplayer)
	{
		strlcpy(result, bestplayer->name, result_len);
		return true;
	}

	return false;
}

void QtvStartDelay_f(void)
{
	if (cls.mvdplayback != QTV_PLAYBACK) {
		Con_Printf("Can only be used during QTV playback.\n");
		return;
	}

	qtv_playback_paused = true;
}

void QtvEndDelay_f(void)
{
	int ms;

	if (Demo_BufferSize(&ms)) {
		Cvar_SetValue(&qtv_buffertime, ms / 1000.0f);
		Con_Printf("QTV delay set to %3.1fs.\n", qtv_buffertime.value);
	}
	else {
		Con_Printf("No QTV data in buffer: no delay set.\n");
	}

	qtv_playback_paused = false;
}

