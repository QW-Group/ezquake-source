/*
	Support for FTE QuakeTV

	$Id: qtv.c,v 1.11 2007-07-28 23:19:32 disconn3ct Exp $
*/

#include "quakedef.h"
#include "winquake.h"
#include "qtv.h"
#include "teamplay.h"
#include "fs.h"

cvar_t	qtv_buffertime = {"qtv_buffertime", "0.5"};


void QTV_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_MVD); // FIXME: add qtv group instead

	Cvar_Register(&qtv_buffertime);

	Cvar_ResetCurrentGroup();
}

//=================================================

// ripped from FTEQTV, original name is SV_ConsistantMVDData
// return non zero if we have at least one message
int ConsistantMVDData(unsigned char *buffer, int remaining)
{
	qbool warn = true;
	int lengthofs;
	int length;
	int available = 0;
	while(1)
	{
		if (remaining < 2)
			return available;

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
			return available;

		length = (buffer[lengthofs]<<0) + (buffer[lengthofs+1]<<8) + (buffer[lengthofs+2]<<16) + (buffer[lengthofs+3]<<24);

		length += lengthofs+4;
		if (length > 1400 && warn) {
			Com_Printf("Corrupt mvd\n");
			warn = false;
		}

gottotallength:
		if (remaining < length)
			return available;

		remaining -= length;
		available += length;
		buffer += length;
	}
}

//=================================================

extern vfsfile_t *playbackfile;

void QTV_ForwardToServerEx (qbool skip_if_no_params, qbool use_first_argument)
{
	char data[1024 + 100] = {0}, text[1024], *s;
	sizebuf_t buf;

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

	SZ_Init(&buf, (byte *) data, sizeof(data));

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
	QTV_ForwardToServerEx (true, true);
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
