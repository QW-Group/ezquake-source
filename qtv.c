/*
	Support for FTE QuakeTV

	$Id: qtv.c,v 1.20 2007-10-28 02:45:19 qqshka Exp $
*/

#include "quakedef.h"
#include "winquake.h"
#include "qtv.h"
#include "teamplay.h"
#include "fs.h"

cvar_t	qtv_buffertime		= {"qtv_buffertime", "0.5"};
cvar_t	qtv_chatprefix		= {"qtv_chatprefix", "$[{QTV}$] "};
cvar_t  qtv_adjustbuffer	= {"qtv_adjustbuffer", "0"};


void QTV_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_MVD); // FIXME: add qtv group instead
	
	Cvar_Register(&qtv_buffertime);
	Cvar_Register(&qtv_chatprefix);
	Cvar_Register(&qtv_adjustbuffer);

	Cvar_ResetCurrentGroup();
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
int ConsistantMVDDataEx(unsigned char *buffer, int remaining, int *ms)
{
	qbool warn = true;
	int lengthofs;
	int length;
	int available = 0;

	if (ms)
		ms[0] = 0;

	while( 1 )
	{
		if (remaining < 2)
		{
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

		if (lengthofs+4 > remaining)
		{
			return available;
		}

		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);

		length += lengthofs+4;

		if (length > MAX_MVD_SIZE && warn)
		{
			Com_Printf("Corrupt mvd, length: %d\n", length);
			warn = false;
		}

gottotallength:
		if (remaining < length)
		{
			return available;
		}

		if (ms)
			ms[0] += buffer[0];
			
		remaining -= length;
		available += length;
		buffer    += length;
	}
}

int ConsistantMVDData(unsigned char *buffer, int remaining)
{
	return ConsistantMVDDataEx(buffer, remaining, NULL);
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

void QTV_Say_f (void)
{
	char *s = Cmd_Args();
	char text[1024] = {0};
	int len;
	tokenizecontext_t tmpcontext;

	// save context, so we can later restore it
	Cmd_SaveContext(&tmpcontext);

	// get rid of quotes, if any
	if (s[0] == '\"' && s[(len = strlen(s))-1] == '\"' && len > 2)
	{
		snprintf(text, sizeof(text), "%s %s", Cmd_Argv(0), s + 1);
		if ((len = strlen(text)))
			text[len - 1] = 0;
		Cmd_TokenizeString(text);
	}

	QTV_ForwardToServerEx (true, true);

	// restore
	Cmd_RestoreContext(&tmpcontext);
}

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
