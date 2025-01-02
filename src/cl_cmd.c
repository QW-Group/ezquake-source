/*
Copyright (C) 1996-1997 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: cl_cmd.c,v 1.58 2007-10-11 05:55:47 dkure Exp $
*/

#include <time.h>
#include "quakedef.h"
#include "sha1.h"
#include "gl_model.h"
#include "teamplay.h"
#include "config_manager.h"
#include "rulesets.h"
#include "version.h"
#include "utils.h"
#include "menu.h"
#include "qtv.h"
#include "fs.h"

/*time type to be used for rcon encryption*/
#define __qtime_t uint64_t

void SCR_RSShot_f (void);
void CL_ProcessServerInfo(void);
void SV_Serverinfo_f(void);
void S_StopAllSounds(void);


cvar_t cl_sayfilter_coloredtext = {"cl_sayfilter_coloredtext", "0"};
typedef enum coloredtextfilterlevel_e
{
	coltextfilter_none = 0,
	coltextfilter_color = 1,
	coltextfilter_colorwhite = 2
} coloredtextfilterlevel_e;
#define SAYSTRING_UNCOLORED_FILTERMARK	"#u"
#define SAYSTRING_COLORED_FILTERMARK	"#c"

cvar_t cl_sayfilter_sendboth = {"cl_sayfilter_sendboth", "0"};

//adds the current command line as a clc_stringcmd to the client message.
//things like kill, say, etc, are commands directed to the server,
//so when they are typed in at the console, they will need to be forwarded.
void Cmd_ForwardToServer (void) {
	char *s;

	if (cls.mvdplayback == QTV_PLAYBACK) {
		QTV_Cmd_ForwardToServer();
		return;
	}

	if (cls.state == ca_disconnected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	// lowercase command
	for (s = Cmd_Argv(0); *s; s++)
		*s = (char) tolower(*s);
	SZ_Print (&cls.netchan.message, Cmd_Argv(0));
	if (Cmd_Argc() > 1) {
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}

// don't forward the first argument
void CL_ForwardToServer_f (void) {
// Added by VVD {
	char* server_string;
	char client_time_str[sizeof(__qtime_t) * 2 + 1] = { 0 };
	int i, server_string_len;
	extern cvar_t cl_crypt_rcon;
	__qtime_t client_time = 0;
// Added by VVD }

	if (cls.mvdplayback == QTV_PLAYBACK) {
		QTV_Cl_ForwardToServer_f();
		return;
	}

	if (cls.state == ca_disconnected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	if (cls.demoplayback)
		return;		// not really connected

	if (Cmd_Argc() > 1) {
		if (strcasecmp(Cmd_Argv(1), "snap") == 0) {
			SCR_RSShot_f ();
			return;
		}

//bliP ->
		if (strcasecmp(Cmd_Argv(1), "fileul") == 0) {
			CL_StartFileUpload ();
			return;
		}
//<-
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
/* johnnycz: disabled due to security reasons -- fixme
		if (strcasecmp(Cmd_Argv(1), "download") == 0 && Cmd_Argc() > 2)
		{
			strlcpy(cls.downloadname, Cmd_Argv(2), sizeof(cls.downloadname));
			COM_StripExtension(cls.downloadname, cls.downloadtempname);
			strlcat(cls.downloadtempname, ".tmp", sizeof(cls.downloadtempname));
			cls.downloadtype = dl_single;
			//snprintf (cls.downloadname, sizeof(cls.downloadname), "%s", Cmd_Argv(2));
			//strlcpy (cls.downloadtempname, cls.downloadname, sizeof(cls.downloadtempname));
		}
*/
// Added by VVD {
		if (cl_crypt_rcon.value && strcasecmp(Cmd_Argv(1), "techlogin") == 0 && Cmd_Argc() > 2)
		{
			time((time_t *)&client_time);
			for (client_time_str[0] = i = 0; i < sizeof(client_time); i++) {
				char tmp[3];
				snprintf(tmp, sizeof(tmp), "%02X", (unsigned int)((client_time >> (i * 8)) & 0xFF));
				strlcat(client_time_str, tmp, sizeof(client_time_str));
			}

			server_string_len = Cmd_Argc() + strlen(Cmd_Argv(1)) + DIGEST_SIZE * 2 + 16;
			for (i = 3; i < Cmd_Argc(); ++i)
				server_string_len += strlen(Cmd_Argv(i));
			server_string = (char *) Q_malloc(server_string_len);

			SHA1_Init();
			SHA1_Update((unsigned char *)Cmd_Argv(1));
			SHA1_Update((unsigned char *)" ");
			SHA1_Update((unsigned char *)Cmd_Argv(2));
			SHA1_Update((unsigned char *)client_time_str);
			SHA1_Update((unsigned char *)" ");
			for (i = 3; i < Cmd_Argc(); ++i)
			{
				SHA1_Update((unsigned char *)Cmd_Argv(i));
				SHA1_Update((unsigned char *)" ");
			}

			snprintf(server_string, server_string_len, "%s %s%s ",
				Cmd_Argv(1), SHA1_Final(), client_time_str);
			for (i = 3; i < Cmd_Argc(); ++i)
			{
				strlcat(server_string, Cmd_Argv(i), server_string_len);
				strlcat(server_string, " ", server_string_len);
			}
			SZ_Print (&cls.netchan.message, server_string);
			Q_free(server_string);
		}
		else
// Added by VVD }
			SZ_Print (&cls.netchan.message, Cmd_Args());
	}
}

/// filters white markup chars from the string
static void CL_Cmd_SayString_FilterWhite(char *s)
{
	char *rp = s;	// read pointer
	char *wp = s;	// write pointer
	char c;			// current char

	while ((c = *rp++)) {
		if (c == '{' || c == '}') continue;
		*wp++ = c;
	}

	*wp = '\0';
}

/// filters '&cfa5'-like colored markup from the string
static void CL_Cmd_SayString_FilterColoredText(char *s)
{
	char *rp = s;	// read pointer
	char *wp = s;	// write pointer
	char c;			// current char

	while ((c = *rp++)) {
		if (c == '&' && *rp == 'c') {
			rp++;	// skip 'c'
			if (*rp) rp++; else break;	// skip red value
			if (*rp) rp++; else break;	// skip green value
			if (*rp) rp++; else break;	// skip blue value
			continue;
		}
		*wp++ = c;
	}

	*wp = '\0';
}

// applies colored text filters on the string
static void CL_Cmd_SayString_ApplyFilters(char *s)
{
	if (cl_sayfilter_coloredtext.integer != coltextfilter_none) {
		CL_Cmd_SayString_FilterColoredText(s);
	}
	if (cl_sayfilter_coloredtext.integer == coltextfilter_colorwhite) {
		CL_Cmd_SayString_FilterWhite(s);
	}
}

// inserts an appendix to the string
// if there already is an appendix (starting with #), it will be overwritten
// if the string is enclosed in quotes ("), they will be kept
static void CL_Cmd_SayString_InsertAppendix(char *s, char *appendix)
{
	char *p = strchr(s, '#');
	size_t l = strlen(s);
	qbool endquote = s[l-1] == '\"';

	if (!p) p = s + l - 1;

	while ((*p++ = *appendix++)) {}
	p--;
	if (endquote)
		*p++ = '\"';
	*p = '\0';
}

// sends saystring with optional appendix
static void CL_Cmd_SayString_SendBase(char *s, char *appendix)
{
	if (*s && *s < 32) {
		SZ_Print (&cls.netchan.message, "\"");
		if (appendix) {
			CL_Cmd_SayString_InsertAppendix(s, appendix);
		}
		SZ_Print (&cls.netchan.message, s);
		SZ_Print (&cls.netchan.message, "\"");
	} else {
		if (appendix) {
			CL_Cmd_SayString_InsertAppendix(s, appendix);
		}
		SZ_Print (&cls.netchan.message, s);
	}
}

/// returns true if there is a color markup in the string
static qbool CL_Cmd_SayString_IsColored(char *s)
{
	while(*s) {
		if (*s == '&' && s[1] == 'c') return true;
		s++;
	}
	return false;
}

// this function presumes there's at least two more bytes available memory
// after the end of the string
static void CL_Cmd_SayString_Send(char *s)
{
	if (cl_sayfilter_sendboth.integer && cl_sayfilter_coloredtext.integer
		&& !strcmp("say_team", Cmd_Argv(0)) && CL_Cmd_SayString_IsColored(s)) {
		CL_Cmd_SayString_SendBase(s, SAYSTRING_COLORED_FILTERMARK);
		CL_Cmd_SayString_ApplyFilters(s);
		
		// send header again
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Argv(0));
		SZ_Print (&cls.netchan.message, " ");
		
		CL_Cmd_SayString_SendBase(s, SAYSTRING_UNCOLORED_FILTERMARK);
	} else {
		CL_Cmd_SayString_ApplyFilters(s);
		CL_Cmd_SayString_SendBase(s, NULL);
	}
}

//Handles both say and say_team
void CL_Say_f (void) {
	char *s, msg[1024], qmsg[1024];
	int tmp;
	qbool qizmo = false;
	extern cvar_t cl_fakename;
	extern cvar_t cl_fakename_suffix;

	if (cls.mvdplayback == QTV_PLAYBACK) {
		QTV_Say_f();
		return;
	}

	if (Cmd_Argc() < 2)
		return;

	if (cls.state == ca_disconnected) {
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	// lowercase command
	for (s = Cmd_Argv(0); *s; s++)
		*s = (char) tolower(*s);
	SZ_Print (&cls.netchan.message, Cmd_Argv(0));

	if (CL_ConnectedToProxy()) {	
		for (s = Cmd_Argv(1); *s == ' '; s++)
			;
		if (!strncmp(s, ".stuff", 6) || !strncmp(s, ",stuff", 6) || strstr(s, ":stuff"))
			return;		

		qizmo = (!strncmp(s, "proxy:", 6) || s[0] == ',' || s[0] == '.');
	}

	
	if (!qizmo && cl_floodprot.value && cl_fp_messages.value > 0 && cl_fp_persecond.value > 0) {
		tmp = cl.whensaidhead - min(cl_fp_messages.value, 10) + 1;
		if (tmp < 0)
			tmp += 10;
		if (cl.whensaid[tmp] && (cls.realtime - cl.whensaid[tmp]) < (1.02 * cl_fp_persecond.value)) {
			Com_Printf("Flood Protection\n");
			return;
		}
	}
	

	SZ_Print (&cls.netchan.message, " ");

	s = TP_ParseMacroString (Cmd_Args());
	s = TP_ParseFunChars (s, true);

    /* The string in 's' can be both wrapped in quotes or without wrapper quotes,
       but to make cl_fakename works properly, we need it to be always wrapped in quotes.
       messagemode/messagemode2 commands send text in quotes,
       say/say_team typed in the console sends text without quotes
       So if quotes are missing, we add them.
    */
    // first char isn't quote, last char isn't quote or string is shorter then 2 chars
    if (s[0] != '\"' || s[strlen(s)-1] != '\"' || s[1] == '\0')
    {
        snprintf(qmsg, sizeof(qmsg), "\"%s\"", s);
        s = qmsg;
    }

    // if team message mode and teamname is set and message is not custom mm2 message...
	if (!strcasecmp(Cmd_Argv(0), "say_team") && !cl.spectator && cl_fakename.string[0] && !strchr(s, '\x0d'))
	{
		char c_fn[1024], c_fna[1024], c_msg[1024];
        size_t len = strlen(s) - 1; // cut the trailing quote (")

        len = bound(0, len, sizeof(c_fn));

		// TP_ParseFunChars wants a string < 1024 chars (fix it?)
        strlcpy (c_fn, cl_fakename.string, sizeof(c_fn));
        strlcpy (c_fna, cl_fakename_suffix.string, sizeof(c_fna));
		
        // 1) save the message text, because TP_ParseFunChars will overwrite the temp memory
        // 2) cut the leading quote (+1) and also the trailing quote (len is 1 char shorter)
        strlcpy (c_msg, s+1, len);

		snprintf (msg, sizeof(msg), "\x0d%s%s", TP_ParseFunChars(strcat(c_fn, c_fna), true), c_msg);

		s = msg;
	}

	CL_Cmd_SayString_Send(s);
	
	if (!qizmo) {
		cl.whensaidhead++;
		if (cl.whensaidhead > 9)
			cl.whensaidhead = 0;
		cl.whensaid[cl.whensaidhead] = cls.realtime;
	}
	
}

void CL_Pause_f (void) {
	if (cls.demoplayback)
		cl.paused ^= PAUSED_DEMO;
	else
		Cmd_ForwardToServer();
}

//packet <destination> <contents>
//Contents allows \n escape character
void CL_Packet_f(void) {
// TCPCONNECT -->
	int tcpsock;
// <--TCPCONNECT
	netadr_t adr;
	char send[2048], *in, *out;

	if (Cmd_Argc() != 3) {
		Com_Printf("packet <destination> <contents>\n");
		return;
	}

	if (cbuf_current && cbuf_current != &cbuf_svc && Rulesets_RestrictPacket()) {
		Com_Printf("Packet command is disabled during match\n");
		return;
	}

	if (!NET_StringToAdr(Cmd_Argv(1), &adr)) {
		Com_Printf("Bad address\n");
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort(PORT_SERVER);

	send[0] = send[1] = send[2] = send[3] = 0xFF;

	in = Cmd_Argv(2);
	out = send + 4;

	while (*in && out - send < sizeof(send) - 2) {
		if (in[0] == '\\' && in[1]) {
			switch(in[1]) {
				case 'n' : *out++ = '\n'; break;
				case 't' : *out++ = '\t'; break;
				case '\\' : *out++ = '\\'; break;
				default : *out++ = in[0]; *out++ = in[1]; break;
			}
			in += 2;
		} else {
			*out++ = *in++;
		}
	}
	*out = 0;

// TCPCONNECT -->
	//extra code to stop the packet command from sending to the server via tcp
	tcpsock = cls.sockettcp;
	cls.sockettcp = -1;
	NET_SendPacket (NS_CLIENT, out-send, send, adr);
	cls.sockettcp = tcpsock;
// <--TCPCONNECT
}


void CL_PrintQStatReply (char *s) {
	char *p;
	int n, numplayers;
	int userid, frags, time, ping, topcolor, bottomcolor;
	char name[33], skin[17];

	Com_Printf ("\n");
	Com_Printf ("-------------------------------------\n");

	con_ormask = 128;
	Com_Printf ("qstat %s:\n", NET_AdrToString(net_from));
	con_ormask = 0;

	// count players
	numplayers = -1;
	p = s;
	while (*p) if (*p++ == '\n') numplayers++;

	// extract serverinfo string
	s = strtok (s, "\n");

	Com_Printf ("hostname   %s\n", Info_ValueForKey(s, "hostname"));
	if (*(p = Info_ValueForKey(s, "*gamedir")) && strcmp(p, "qw"))
		Com_Printf ("gamedir    %s\n", p);
	Com_Printf ("map        %s\n", Info_ValueForKey(s, "map"));
	if (*(p = Info_ValueForKey(s, "status")))
		Com_Printf ("status     %s\n", p);
	Com_Printf ("deathmatch %s\n", Info_ValueForKey(s, "deathmatch"));
	Com_Printf ("teamplay   %s\n", Info_ValueForKey(s, "teamplay"));
	Com_Printf ("timelimit  %s\n", Info_ValueForKey(s, "timelimit"));
	Com_Printf ("fraglimit  %s\n", Info_ValueForKey(s, "fraglimit"));
	if ((n = Q_atoi(Info_ValueForKey(s, "needpass")) & 3) != 0)
		Com_Printf ("needpass   %s%s%s\n", n & 1 ? "player" : "",
			n == 3 ? ", " : "", n & 2 ? "spectator" : "");
	if (Q_atoi(Info_ValueForKey(s, "needpass")) & 1)
		Com_Printf ("player password required\n");
	if (Q_atoi(Info_ValueForKey(s, "needpass")) & 2)
		Com_Printf ("spectator password required\n");

	Com_Printf ("players    %i/%s\n", numplayers, Info_ValueForKey(s, "maxclients"));

	p = strtok (NULL, "\n");

	if (p)
	{
		con_ormask = 128;
		Com_Printf ("\nping time frags name\n");
		con_ormask = 0;
		Com_Printf ("-------------------------------------\n");

		while (p)
		{
			sscanf (p, "%d %d %d %d \"%32[^\"]\" \"%16[^\"]\" %d %d",
				&userid, &frags, &time, &ping, (char *)&name, (char *)&skin, &topcolor, &bottomcolor);
			Com_Printf("%4d %4d %4d  %-16.16s\n", ping, time, frags, name);
			p = strtok (NULL, "\n");
		}
		Com_Printf ("-------------------------------------\n");
	}

	Com_Printf ("\n");
}

/*
====================
CL_QStat_f

qstat <destination>
====================
*/
double	qstat_senttime = 0;

void CL_QStat_f (void)
{
	char	send[10] = {0xff, 0xff, 0xff, 0xff, 's', 't', 'a', 't', 'u', 's'};
	netadr_t	adr;

	if (Cmd_Argc() < 2)
	{
		Com_Printf ("usage: qstat <server>\n");
		return;
	}

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Com_Printf ("Bad address\n");
		return;
	}

	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	NET_SendPacket (NS_CLIENT, 10, send, adr);

	qstat_senttime = curtime;
}


//Send the rest of the command line over as an unconnected command.
void CL_Rcon_f (void) {

	char message[1024];
	char client_time_str[sizeof(__qtime_t) * 2 + 1];
	int i, i_from;
	netadr_t to;
	extern cvar_t rcon_password, rcon_address, cl_crypt_rcon;
	__qtime_t client_time = 0;

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;
	strlcat (message, "rcon ", sizeof(message));

// Added by VVD {
	if (cl_crypt_rcon.value)
	{
		time((time_t *)&client_time);
		for (client_time_str[0] = i = 0; i < sizeof(client_time); i++) {
			char tmp[3];
			snprintf(tmp, sizeof(tmp), "%02X", (unsigned int)((client_time >> (i * 8)) & 0xFF));
			strlcat(client_time_str, tmp, sizeof(client_time_str));
		}
		
		SHA1_Init();
		SHA1_Update((unsigned char *)"rcon ");
		if (rcon_password.string[0])
		{
			SHA1_Update((unsigned char *)rcon_password.string);
			SHA1_Update((unsigned char *)client_time_str);
			i_from = 1;
		}
		else // first arg must be pass in such case, so handle this
		{
			SHA1_Update((unsigned char *)Cmd_Argv(1));
			SHA1_Update((unsigned char *)client_time_str);
			i_from = 2;
		}
		SHA1_Update((unsigned char *)" ");
		for (i = i_from; i < Cmd_Argc(); i++)
		{
			SHA1_Update((unsigned char *)Cmd_Argv(i));
			SHA1_Update((unsigned char *)" ");
		}
		strlcat (message, SHA1_Final(), sizeof(message));
		strlcat (message, client_time_str, sizeof(message));
		strlcat (message, " ", sizeof(message));
	}
	else {
		i_from = 1;
 		if (rcon_password.string[0]) {
			strlcat (message, rcon_password.string, sizeof(message));
			strlcat (message, " ", sizeof(message));
		}
	}
	for (i = i_from; i < Cmd_Argc(); i++)
	{
		strlcat (message, Cmd_Argv(i), sizeof(message));
		strlcat (message, " ", sizeof(message));
	}
// } Added by VVD

	if (cls.state >= ca_connected) {
		to = cls.netchan.remote_address;
	} else {
		if (!strlen(rcon_address.string)) {
			Com_Printf ("You must either be connected or set 'rcon_address' to issue rcon commands\n");
			return;
		}
		NET_StringToAdr (rcon_address.string, &to);
		if (to.port == 0)
			to.port = BigShort (PORT_SERVER);
	}

	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, to);
}

qbool CL_Download_Accept(const char *filename)
{
	if (strstr(filename, "..") || !strcmp(filename, "") || filename[0] == '/' || strchr(filename, '\\') || strchr(filename, ':') || strstr(filename, "//")) {
		Com_Printf("Warning: Invalid characters in filename \"%s\"\n", filename);
		return false;
	}

	if (!CL_IsDownloadableFileExtension(filename)) {
		Com_Printf("Warning: Non-allowed file \"%s\" skipped. Add \"%s\" to cl_allow_downloads to allow the file to be downloaded\n", filename, COM_FileExtension(filename));
		return false;
	}

	vfsfile_t *f = FS_OpenVFS(filename, "rb", FS_ANY);
	if (f) {
		VFS_CLOSE(f);
		return false;
	}

	return true;
}

void CL_Download_f (void){
	char *dir; // we save to demo_dir or gamedir
	char *filename; // which file to dl, will be sent to server
	char ondiskname[sizeof(cls.downloadname)]; // hack, save file to "right" place
	extern char *CL_DemoDirectory(void);

	if (cls.state == ca_disconnected) {
		Com_Printf ("Must be connected.\n");
		return;
	}

	filename = Cmd_Argv(1);
	strlcpy(ondiskname, filename, sizeof(ondiskname)); // in most cases this is same as filename

	if (Cmd_Argc() != 2 || !filename[0] || !CL_Download_Accept(filename)) {
		Com_Printf ("Usage: %s <datafile>\n", Cmd_Argv(0));
		return;
	}

	// hack: save demos in demo_dir
	if (
				    Utils_RegExpMatch("\\.mvd(\\.(gz|bz2|rar|zip))?$", filename)
				 || Utils_RegExpMatch("\\.(qwd|qwz|dem)$", filename)
			) {
		dir = CL_DemoDirectory(); // seems filename is a demo
		// so, we save file in /dir/<demo_dir> instead of /dir/<demo_dir>/demos
		strlcpy(ondiskname, COM_SkipPath(filename), sizeof(ondiskname));
	}
	else
		dir = cls.gamedir; // not a demo

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left

	cls.downloadtype      = dl_single;
	cls.downloadmethod    = DL_QW; // by default its DL_QW, if server support DL_QWCHUNKED it will be changed.
	cls.downloadstarttime = Sys_DoubleTime();

	snprintf(cls.downloadname, sizeof(cls.downloadname), "%s/%s", dir, ondiskname);
	COM_StripExtension(cls.downloadname, cls.downloadtempname, sizeof(cls.downloadtempname));
	strlcat(cls.downloadtempname, ".tmp", sizeof(cls.downloadtempname));

	if (cls.mvdplayback == QTV_PLAYBACK)
	{
		QTV_Cmd_Printf(QTV_EZQUAKE_EXT_DOWNLOAD, "download \"%s\"", filename);
	}
	else
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, va("download \"%s\"", filename));
	}
}

void CL_User_f (void) {
	int uid, i;

	if (Cmd_Argc() != 2) {
		Com_Printf ("Usage: %s <username / userid>\n", Cmd_Argv(0));
		return;
	}

	uid = atoi(Cmd_Argv(1));

	for (i = 0; i < MAX_CLIENTS; i++) {
		if (!cl.players[i].name[0])
			continue;
		if (cl.players[i].userid == uid	|| !strcmp(cl.players[i].name, Cmd_Argv(1)) ) {
			Info_Print (cl.players[i].userinfo);
			return;
		}
	}
	Com_Printf ("User not in server.\n");
}

void CL_Users_f (void) {
	int i, c;

	c = 0;
	Com_Printf ("userid frags name\n");
	Com_Printf ("------ ----- ----\n");
	for (i = 0; i < MAX_CLIENTS; i++) {
		if (cl.players[i].name[0]) {
			Com_Printf ("%6i %4i %s\n", cl.players[i].userid, cl.players[i].frags, cl.players[i].name);
			c++;
		}
	}

	Com_Printf ("%i total users\n", c);
}

void CL_Color_f (void) {
	extern cvar_t topcolor, bottomcolor;
	int top, bottom;

	switch (Cmd_Argc())
	{
		case 1:
			Com_Printf ("\"color\" is \"%s %s\"\n",
				Info_ValueForKey (cls.userinfo, "topcolor"),
				Info_ValueForKey (cls.userinfo, "bottomcolor") );
			Com_Printf ("color <0-13> [0-13]\n");
			return;
		case 2:
			top = bottom = Q_atoi(Cmd_Argv(1));
			break;
		default:
			top = Q_atoi(Cmd_Argv(1));
			bottom = Q_atoi(Cmd_Argv(2));
	}

	top &= 15;
	top = min(top, 13);
	bottom &= 15;
	bottom = min(bottom, 13);

	Cvar_SetValue (&topcolor, top);
	Cvar_SetValue (&bottomcolor, bottom);
}

//usage: fullinfo \name\unnamed\topcolor\0\bottomcolor\1, etc
void CL_FullInfo_f (void) {
	char key[512], value[512], *o, *s;

	if (Cmd_Argc() != 2) {
		Com_Printf ("fullinfo <complete info string>\n");
		return;
	}

	s = Cmd_Argv(1);
	if (*s == '\\')
		s++;
	while (*s) {
		o = key;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (!*s) {
			Com_Printf ("MISSING VALUE\n");
			return;
		}

		o = value;
		s++;
		while (*s && *s != '\\')
			*o++ = *s++;
		*o = 0;

		if (*s)
			s++;

		if (!strcasecmp(key, pmodel_name) || !strcasecmp(key, emodel_name))
			continue;

		Info_SetValueForKey (cls.userinfo, key, value, MAX_INFO_STRING);
	}
}

//Allow clients to change userinfo
void CL_SetInfo_f (void) {
	if (Cmd_Argc() == 1) {
		Info_Print (cls.userinfo);
		Com_Printf ("[%i/196]\n", strlen(cls.userinfo));
		return;
	}
	if (Cmd_Argc() != 3) {
		Com_Printf ("Usage: %s [ <key> <value> ]\n", Cmd_Argv(0));
		return;
	}
	if (!strcasecmp(Cmd_Argv(1), pmodel_name) || !strcmp(Cmd_Argv(1), emodel_name))
		return;

	Info_SetValueForKey (cls.userinfo, Cmd_Argv(1), Cmd_Argv(2), MAX_INFO_STRING);
	if (cls.state >= ca_connected)
		Cmd_ForwardToServer ();
}


void CL_UserInfo_f (void) {
	if (Cmd_Argc() != 1) {
		Com_Printf("%s : no arguments expected\n", Cmd_Argv(0));
		return;
	}
	Info_Print (cls.userinfo);
}

void CL_Quit_f (void) {
	extern cvar_t cl_confirmquit;

	if (cl_confirmquit.value)
		M_Menu_Quit_f ();
	else
		Host_Quit ();
}

// QW262 -->
/*
============
CL_Userdir_f

============
*/
void CL_Userdir_f (void)
{
	if (cls.state > ca_disconnected || Cmd_Argc() == 1) {
		Com_Printf("Current userdir: %s\n", userdirfile);
	} else {
		int u = Q_atoi(Cmd_Argv(2));
		if (u < 0 || u > 5)
			Com_Printf("Invalid userdir type\n");
		else
			FS_SetUserDirectory (Cmd_Argv(1), Cmd_Argv(2));
	}
}
// <-- QW262

void CL_Serverinfo_f (void) 
{
	#ifndef CLIENTONLY
	if (cls.state < ca_connected || com_serveractive) 
	{
		SV_Serverinfo_f();
		return;
	}
	#endif // CLIENTONLY

	if (cls.state >= ca_onserver && cl.serverinfo[0])
		Info_Print (cl.serverinfo);
	else		
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
}

//============================================================================

typedef struct
{
	const char	*name;
	int			bit;
} z_ext_map_t;

static z_ext_map_t z_map[] =
{
	{ "PM_TYPE",		Z_EXT_PM_TYPE },
	{ "PM_TYPE_NEW",	Z_EXT_PM_TYPE_NEW },
	{ "VIEWHEIGHT",		Z_EXT_VIEWHEIGHT },
	{ "SERVERTIME",		Z_EXT_SERVERTIME },
	{ "PITCHLIMITS",	Z_EXT_PITCHLIMITS },
	{ "JOIN_OBSERVE",	Z_EXT_JOIN_OBSERVE },
	{ "PF_ONGROUND",	Z_EXT_PF_ONGROUND },
	{ "VWEP",			Z_EXT_VWEP },
	{ "PF_SOLID",		Z_EXT_PF_SOLID },
};

static int z_map_cnt = sizeof(z_map)/sizeof(z_map[0]);

int get_z_ext_list(int bits, char *buf, int bufsize)
{
	int i, cnt;

	buf[0] = 0; // hope buf size at least one byte

	for (i = cnt = 0; i < z_map_cnt; i++)
	{
		if (!z_map[i].bit || z_map[i].bit != (z_map[i].bit & bits))
			continue; // not match

		if (cnt)
			strlcat(buf, " ", bufsize);
		strlcat(buf, z_map[i].name, bufsize);
		cnt++;
	}

	return cnt;
}

void CL_Z_Ext_List_f (void)
{
	char buf[1024] = {0};

	Com_Printf("ZQuake protocol extensions:\n");
	Com_Printf("%s\n", get_z_ext_list(cl.z_ext, buf, sizeof(buf)) ? buf : "NONE");
}

//============================================================================

void CL_InitCommands (void) {
	// general commands
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("download", CL_Download_f);
	Cmd_AddCommand ("qstat", CL_QStat_f);
	Cmd_AddCommand ("packet", CL_Packet_f);
	Cmd_AddCommand ("rcon", CL_Rcon_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("quit", CL_Quit_f);
	Cmd_AddCommand ("say", CL_Say_f);
	Cmd_AddCommand ("say_team", CL_Say_f);
	Cmd_AddCommand ("serverinfo", CL_Serverinfo_f);
	Cmd_AddCommand ("skins", Skin_Skins_f);
	Cmd_AddCommand ("showskins", Skin_ShowSkins_f);
	Cmd_AddCommand ("user", CL_User_f);
	Cmd_AddCommand ("users", CL_Users_f);
	Cmd_AddCommand ("version", CL_Version_f);

	// client info setting
	Cmd_AddCommand ("color", CL_Color_f);
	Cmd_AddCommand ("fullinfo", CL_FullInfo_f);
	Cmd_AddCommand ("setinfo", CL_SetInfo_f);
	Cmd_AddCommand ("userinfo", CL_UserInfo_f);
// QW262 -->
	Cmd_AddCommand ("userdir", CL_Userdir_f);
// <-- QW262
	// forward to server commands
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("fly", NULL);

	//  Windows commands
	Cmd_AddCommand ("windows", VID_Minimize);

	Cmd_AddCommand ("z_ext_list", CL_Z_Ext_List_f);

	Cvar_SetCurrentGroup(CVAR_GROUP_COMMUNICATION);
	Cvar_Register(&cl_sayfilter_coloredtext);
	Cvar_Register(&cl_sayfilter_sendboth);
	Cvar_ResetCurrentGroup();
}

/*
==============================================================================
SERVER COMMANDS

Server commands are commands stuffed by server into client's cbuf
We use a separate command buffer for them -- there are several
reasons for that:
1. So that partially stuffed commands are always executed properly
2. Not to let players cheat in TF (v_cshift etc don't work in console)
3. To hide some commands the user doesn't need to know about, like
changing, fullserverinfo, nextul, stopul
==============================================================================
*/

//Just sent as a hint to the client that they should drop to full console
void CL_Changing_f (void) {
	cl.intermission = 0;

	// don't change when downloading
	if (cls.download) {
		// we were on server
		if (cls.state == ca_active) {
			// drop to full console
			// not active anymore, but not disconnected
			cls.state = ca_connected;
#ifdef DEBUG_MEMORY_ALLOCATIONS
			Sys_Printf("\nevent,active(changing)\n");
#endif

			if (!com_serveractive) {
				// notice mapname not valid yet
				Cvar_ForceSet(&host_mapname, "");
			}
		}
		return;
	}

	S_StopAllSounds();

	// MVDs starting during map change can have /changing from the previous map
	if (!(cls.mvdplayback && cls.state == ca_onserver)) {
		// not active anymore, but not disconnected
		cls.state = ca_connected;
	}

	if (!com_serveractive) {
		Cvar_ForceSet(&host_mapname, ""); // notice mapname not valid yet
	}

	Com_Printf ("\nChanging map...\n");
}

//Sent by server when serverinfo changes
void CL_FullServerinfo_f (void) {
	char *p;

	if (Cmd_Argc() != 2)
		return;

	strlcpy (cl.serverinfo, Cmd_Argv(1), sizeof(cl.serverinfo));

	p = Info_ValueForKey (cl.serverinfo, "*cheats");
	if (*p)
		Com_Printf ("== Cheats are enabled ==\n");

	CL_ProcessServerInfo ();
}

void CL_R_DrawViewModel_f (void) {
	extern cvar_t cl_filterdrawviewmodel;

	if (cl_filterdrawviewmodel.value)
		return;
	Cvar_Command ();
}

typedef struct {
	char	*name;
	void	(*func) (void);
} svcmd_t;

svcmd_t svcmds[] = {
	{"changing", CL_Changing_f},
	{"fullserverinfo", CL_FullServerinfo_f},
	{"nextul", CL_NextUpload},
	{"stopul", CL_StopUpload},
//	{"fov", CL_Fov_f},
	{"r_drawviewmodel", CL_R_DrawViewModel_f},
	{"fileul", CL_StartFileUpload}, //bliP
	{NULL, NULL}
};

//Called by Cmd_ExecuteString if cbuf_current == &cbuf_svc
qbool CL_CheckServerCommand (void) {
	svcmd_t	*cmd;
	char *s;

	s = Cmd_Argv (0);
	for (cmd = svcmds; cmd->name; cmd++) {
		if (!strcmp (s, cmd->name) ) {
			cmd->func();
			return true;
		}
	}

	return false;
}

usermainbuttons_t CL_GetLastCmd(int player_slot)
{
	usercmd_t cmd = { 0 };
	usermainbuttons_t ret;
	static int last_impulse;
	static double impulse_time;

	extern void MVD_FlushUserCommands (void);

	if (cls.mvdplayback && cl.mvd_time_offset) {
		memset (&ret, 0, sizeof (ret));

		// Check for latest
		MVD_FlushUserCommands ();

		ret.attack = (cl.mvd_user_cmd[0] & 1);
		ret.jump = (cl.mvd_user_cmd[0] & 2);
		ret.forward = (cl.mvd_user_cmd[0] & 4);
		ret.back = (cl.mvd_user_cmd[0] & 8);
		ret.right = (cl.mvd_user_cmd[0] & 16);
		ret.left = (cl.mvd_user_cmd[0] & 32);
		ret.up = (cl.mvd_user_cmd[0] & 64);
		ret.down = (cl.mvd_user_cmd[0] & 128);
		return ret;
	}

	if (player_slot >= 0 && player_slot < MAX_CLIENTS) {
		frame_t* frame = &cl.frames[cl.validsequence & UPDATE_MASK];
		if (frame->playerstate[player_slot].messagenum == cl.parsecount) {
			cmd = frame->playerstate[player_slot].command;
		}
	}
	else {
		cmd = cl.frames[(cls.netchan.outgoing_sequence - 1) & UPDATE_MASK].cmd;
	}

	ret.attack = cmd.buttons & BUTTON_ATTACK;
	ret.jump = cmd.buttons & BUTTON_JUMP;
	ret.up = cmd.upmove > 0;
	ret.down = cmd.upmove < 0;
	ret.forward = cmd.forwardmove > 0;
	ret.back = cmd.forwardmove < 0;
	ret.left = cmd.sidemove < 0;
	ret.right = cmd.sidemove > 0;

	if (cmd.impulse) {
		last_impulse = cmd.impulse;
		impulse_time = cls.realtime;
	}
	if (!(last_impulse && cls.realtime >= impulse_time &&
		cls.realtime <= impulse_time + 0.2))
		last_impulse = 0;
	return ret;
}
