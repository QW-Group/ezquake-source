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
 
	
*/

#ifndef CLIENTONLY
#include "qwsvdef.h"

static void SV_BotWriteDamage(client_t* c, int i);
static void SV_FlushReliableDatagram(void);

#define CHAN_AUTO   0
#define CHAN_WEAPON 1
#define CHAN_VOICE  2
#define CHAN_ITEM   3
#define CHAN_BODY   4

/*
=============================================================================
 
Con_Printf redirection
 
=============================================================================
*/

char	outputbuf[OUTPUTBUF_SIZE];

redirect_t	sv_redirected;
static int	sv_redirectbufcount;

qbool SV_SkipCommsBotMessage(client_t* client);
extern cvar_t sv_phs, sv_reliable_sound;

/*
==================
SV_FlushRedirect
==================
*/
void SV_FlushRedirect (void)
{
	char send1[OUTPUTBUF_SIZE + 6];

	if (sv_redirected == RD_PACKET)
	{
		send1[0] = 0xff;
		send1[1] = 0xff;
		send1[2] = 0xff;
		send1[3] = 0xff;
		send1[4] = A2C_PRINT;
		memcpy (send1 + 5, outputbuf, strlen(outputbuf) + 1);

		NET_SendPacket (NS_SERVER, strlen(send1) + 1, send1, net_from);
	}
	else if (sv_redirected == RD_CLIENT && sv_redirectbufcount < MAX_REDIRECTMESSAGES)
	{
		ClientReliableWrite_Begin (sv_client, svc_print, strlen(outputbuf)+3);
		ClientReliableWrite_Byte (sv_client, PRINT_HIGH);
		ClientReliableWrite_String (sv_client, outputbuf);
		sv_redirectbufcount++;
	}
	else if (sv_redirected == RD_MOD)
	{
		//return;
	}
	else if (sv_redirected > RD_MOD && sv_redirectbufcount < MAX_REDIRECTMESSAGES)
	{
		client_t *cl;

		cl = svs.clients + sv_redirected - RD_MOD - 1;

		if (cl->state == cs_spawned)
		{
			ClientReliableWrite_Begin (cl, svc_print, strlen(outputbuf)+3);
			ClientReliableWrite_Byte (cl, PRINT_HIGH);
			ClientReliableWrite_String (cl, outputbuf);
			sv_redirectbufcount++;
		}
	}

	// clear it
	outputbuf[0] = 0;
}


/*
==================
SV_BeginRedirect
 
  Send Con_Printf data to the remote client
  instead of the console
==================
*/
void SV_BeginRedirect (redirect_t rd)
{
	sv_redirected = rd;
	outputbuf[0] = 0;
	sv_redirectbufcount = 0;

}

void SV_EndRedirect (void)
{
	SV_FlushRedirect ();
	sv_redirected = RD_NONE;
}

qbool SV_AddToRedirect(char *msg)
{
	if (!sv_redirected)
		return false;

	// FIXME: probably we should check client's MTU instead of fixed MIN_MTU.
	if (strlen(msg) + strlen(outputbuf) > /* MAX_MSGLEN */ MIN_MTU - 10)
		SV_FlushRedirect ();

	strlcat(outputbuf, msg, sizeof(outputbuf));

	return true;
}

#ifdef SERVERONLY

/*
================
Con_Printf

Handles cursor positioning, line wrapping, etc
================
*/
#define	MAXPRINTMSG	4096
void Con_Printf (char *fmt, ...)
{
	va_list argptr;
	char msg[MAXPRINTMSG];

	va_start (argptr,fmt);
	vsnprintf (msg, MAXPRINTMSG, fmt, argptr);
	va_end (argptr);

	// add to redirected message
	if (SV_AddToRedirect(msg))
		return; // added.

	Sys_Printf ("%s", msg);	// also echo to debugging console
	SV_Write_Log(CONSOLE_LOG, 0, msg);

	// dumb error message to log file if
	if (sv_error)
		SV_Write_Log(ERROR_LOG, 1, msg);
}

/*
================
Con_DPrintf

A Con_Printf that only shows up if the "developer" cvar is set
================
*/
void Con_DPrintf (char *fmt, ...)
{
	va_list		argptr;
	char		msg[MAXPRINTMSG];

	if (!(int)developer.value)
		return;

	va_start (argptr,fmt);
	vsnprintf (msg, MAXPRINTMSG, fmt, argptr);
	va_end (argptr);

	Con_Printf ("%s", msg);
}

#endif // SERVERONLY

/*
=============================================================================
 
EVENT MESSAGES
 
=============================================================================
*/

static void SV_PrintToClient(client_t *cl, int level, char *string)
{
	if (cl->state < cs_preconnected)
	{
		Sys_Printf("SV_PrintToClient: client not ready.");
		return;
	}

	ClientReliableWrite_Begin (cl, svc_print, strlen(string)+3);
	ClientReliableWrite_Byte (cl, level);
	ClientReliableWrite_String (cl, string);
}


/*
=================
SV_ClientPrintf
 
Sends text across to be displayed if the level passes
=================
*/
void SV_ClientPrintf (client_t *cl, int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	if (level < cl->messagelevel)
		return;

	va_start (argptr,fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	if (sv.mvdrecording)
	{
		if (MVDWrite_Begin (dem_single, cl - svs.clients, strlen(string)+3))
		{
			MVD_MSG_WriteByte (svc_print);
			MVD_MSG_WriteByte (level);
			MVD_MSG_WriteString (string);
		}
	}

	SV_PrintToClient(cl, level, string);
}

void SV_ClientPrintf2 (client_t *cl, int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	if (level < cl->messagelevel)
		return;

	va_start (argptr,fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	SV_PrintToClient(cl, level, string);
}


/*
=================
SV_BroadcastPrintf

Sends text to all active clients
=================
*/
char *parse_mod_string(char *str);
void SV_DoBroadcastPrintf (int level, int flags, char *string)
{
	char		*fraglog;
	static char	string2[1024] = {0};
	client_t	*cl;
	int			i;

	if (!(flags & BPRINT_IGNORECONSOLE))
		Sys_Printf ("%s", string);	// print to the console

	if (!(flags & BPRINT_IGNORECLIENTS))
	{
		for (i=0, cl = svs.clients ; i<MAX_CLIENTS ; i++, cl++)
		{
			if (level < cl->messagelevel)
				continue;
			if (cl->state < cs_connected)
				continue;

			SV_PrintToClient(cl, level, string); // this does't go to mvd demo
		}
	}

	if (!(flags & BPRINT_IGNOREINDEMO))
	{
		if (flags & BPRINT_QTVONLY)
		{
			sizebuf_t		msg;
			byte			msg_buf[1024];

			SZ_InitEx(&msg, msg_buf, sizeof(msg_buf), true);
			MSG_WriteByte (&msg, svc_print);
			MSG_WriteByte (&msg, level);
			MSG_WriteString (&msg, string);

			DemoWriteQTV(&msg);
		}
		else
		{
			if (sv.mvdrecording)
			{
				if (MVDWrite_Begin (dem_all, 0, strlen(string)+3))
				{
					MVD_MSG_WriteByte (svc_print);
					MVD_MSG_WriteByte (level);
					MVD_MSG_WriteString (string);
				}
			}
		}
	}

	//	SV_Write_Log(MOD_FRAG_LOG, 1, "=== SV_BroadcastPrintf ===\n");
	//	SV_Write_Log(MOD_FRAG_LOG, 1, va("%d\n===>", time(NULL)));
	//	SV_Write_Log(MOD_FRAG_LOG, 1, string);
	//	SV_Write_Log(MOD_FRAG_LOG, 1, "<===\n");
	if (string[0] && logs[MOD_FRAG_LOG].sv_logfile)
	{
		if (string[strlen(string) - 1] == '\n')
		{
			strlcat(string2, string, sizeof(string2));
			//			SV_Write_Log(MOD_FRAG_LOG, 1, "=== SV_BroadcastPrintf ==={\n");
			//			SV_Write_Log(MOD_FRAG_LOG, 1, string2);
			//			SV_Write_Log(MOD_FRAG_LOG, 1, "}==========================\n");
			if ((fraglog = parse_mod_string(string2)))
			{
				SV_Write_Log(MOD_FRAG_LOG, 1, fraglog);
				Q_free(fraglog);
			}
			string2[0] = 0;
		}
		else
			strlcat(string2, string, sizeof(string2));
	}
	//	SV_Write_Log(MOD_FRAG_LOG, 1, "==========================\n\n");
}

void SV_BroadcastPrintf (int level, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	SV_DoBroadcastPrintf (level, 0, string);
}

void SV_BroadcastPrintfEx (int level, int flags, char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];

	va_start (argptr,fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	SV_DoBroadcastPrintf (level, flags, string);
}

/*
=================
SV_BroadcastCommand
 
Queues connection commands for live clients only. MVD/QTV change maps through
serverdata and must not receive these commands after their new gamestate.
=================
*/
void SV_BroadcastCommand (char *fmt, ...)
{
	va_list		argptr;
	char		string[1024];
	client_t	*client;
	int			i, length;

	if (!sv.state)
		return;
	va_start (argptr,fmt);
	vsnprintf (string, sizeof(string), fmt, argptr);
	va_end (argptr);

	/* Preserve ordering with earlier reliable broadcasts before bypassing them. */
	SV_FlushReliableDatagram();
	length = strlen(string) + 2;
	for (i = 0, client = svs.clients; i < MAX_CLIENTS; i++, client++) {
		if (client->state < cs_preconnected) {
			continue;
		}
		ClientReliableWrite_Begin(client, svc_stufftext, length);
		ClientReliableWrite_String(client, string);
	}
}


/*
=================
SV_MulticastExFiltered

Routes prepared messages by visibility and reliability, selecting each recipient's
wire format and filtering unsupported extended sounds. Callers own the buffers.

MULTICAST_ALL	same as broadcast
MULTICAST_PVS	send to clients potentially visible from org
MULTICAST_PHS	send to clients potentially hearable from org
=================
*/
static void SV_MulticastExFiltered(vec3_t origin, int to, const char *cl_reliable_key,
                                  qbool extended_sound, const sizebuf_t *legacy,
                                  const sizebuf_t *rd, qbool force_csqc)
{
	client_t*   client;
	byte*       mask;
	int         leafnum;
	int         j;
	qbool       reliable;
	vec3_t      vieworg;
	qbool       mvd_only = false;
#ifdef SV_HAS_CSQC
	qbool       csqc_only = false;
#endif
	const sizebuf_t *message;

	reliable = false;

	switch (to)
	{
	case MULTICAST_ALL_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_ALL:
		mask = NULL;		// everything
		break;

	case MULTICAST_PHS_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_PHS:
		mask = CM_LeafPHS (CM_PointInLeaf(origin));
		break;

	case MULTICAST_PVS_R:
		reliable = true;	// intentional fallthrough
	case MULTICAST_PVS:
		mask = CM_LeafPVS (CM_PointInLeaf (origin));
		break;
	case MULTICAST_MVD_HIDDEN:
		mask = NULL;
		mvd_only = true;
		break;

	default:
		mask = NULL;
		SV_Error ("SV_Multicast: bad to:%i", to);
	}

#ifdef SV_HAS_CSQC
	// A CSQC packet (svc_fte_cgamepacket) is the whole multicast when it is the
	// first byte of the buffer (the byte before any payload is always the svc
	// code, so this can never false-positive on payload). Detect it here as well
	// as when the caller forces the CSQC path: this keeps a stray CSQC buffer
	// from reaching non-CSQC clients even if the write-time routing was bypassed.
	// We do NOT rewrite sv.multicast in place here: the MVD/hidden paths below
	// must get the original buffer unchanged (recorded demos keep plain svc 76/
	// 83 so they play back elsewhere, and a hidden block whose first byte is 83
	// must not be mangled). The sized (90) conversion is applied only when
	// copying to a live CSQC client under sv_csqcdebug, mirroring FTE's
	// per-destination net_preparse.
	csqc_only = force_csqc ||
		(legacy->cursize > 0 && legacy->data[0] == svc_fte_cgamepacket);
#else
	(void)force_csqc;
#endif

	// send the data to all relevent clients
	for (j = 0, client = svs.clients; j < MAX_CLIENTS && !mvd_only; j++, client++)
	{
		int trackent = 0;

		if (client->state != cs_spawned)
			continue;
		if (SV_SkipCommsBotMessage(client))
			continue;
#ifdef SV_HAS_CSQC
		// svc_fte_cgamepacket is CSQC-only: skip a client that did not enable
		// CSQC (it may have negotiated the ext but never sent enablecsqc, so it
		// has no csprogs to parse the packet).
		if (csqc_only && !((client->fteprotocolextensions & FTE_PEXT_CSQC) && client->csqcactive))
			continue;
#endif
		if (extended_sound && !legacy->cursize
		    && !(client->fteprotocolextensions & FTE_PEXT_SOUNDDBL)
		    && !(client->fteprotocolextensions2 & FTE_PEXT2_REPLACEMENTDELTAS))
			continue; // The legacy sound format cannot represent this ID.

		if (!mask)
			goto inrange; // multicast to all

		// in case of trackent we have to reflect his origin so PHS work right.
		if (fofs_trackent)
		{
			trackent = ((eval_t *)((byte *)(client->edict)->v + fofs_trackent))->_int;
			if (trackent < 1 || trackent > MAX_CLIENTS || svs.clients[trackent - 1].state != cs_spawned)
				trackent = 0;
		}

		if (trackent)
		{
			VectorAdd (svs.clients[trackent - 1].edict->v->origin, svs.clients[trackent - 1].edict->v->view_ofs, vieworg);
		}
		else
		{
			VectorAdd (client->edict->v->origin, client->edict->v->view_ofs, vieworg);
		}

		if (to == MULTICAST_PHS_R || to == MULTICAST_PHS)
		{
			vec3_t delta;
			VectorSubtract(origin, vieworg, delta);
			if (VectorLength(delta) <= 1024)
				goto inrange;
		}

		leafnum = CM_Leafnum(CM_PointInLeaf(vieworg));
		if (leafnum)
		{
			// -1 is because pvs rows are 1 based, not 0 based like leafs
			leafnum = leafnum - 1;
			if ( !(mask[leafnum>>3] & (1<<(leafnum&7)) ) )
			{
				// Con_Printf ("supressed multicast\n");
				continue;
			}
		}

inrange:
		message = rd && ((client->fteprotocolextensions2 & FTE_PEXT2_REPLACEMENTDELTAS)
		    || (extended_sound && (client->fteprotocolextensions & FTE_PEXT_SOUNDDBL)))
		    ? rd : legacy;
		{
			// per-destination view of the payload: normally sv.multicast as-is;
			// a cgamepacket to a live CSQC client becomes sized under
			// sv_csqcdebug (MVD/hidden keep the plain form below).
			byte *data = message->data;
			int   len = message->cursize;
#ifdef SV_HAS_CSQC
			byte  sized[MAX_MSGLEN + 2];
			if (csqc_only && (int)sv_csqcdebug.value && (client->fteprotocolextensions & FTE_PEXT_CSQC)
				&& message->cursize + 2 <= (int)sizeof(sized))
			{
				// [83][payload] -> [90][lenlo][lenhi][payload]
				sized[0] = svc_fte_cgamepacket_sized;
				sized[1] = message->cursize - 1;
				sized[2] = (message->cursize - 1) >> 8;
				memcpy(sized + 3, message->data + 1, message->cursize - 1);
				data = sized;
				len = message->cursize + 2;
			}
#endif
			if (reliable || (cl_reliable_key && *cl_reliable_key && strcmp("0", Info_Get(&client->_userinfo_ctx_, cl_reliable_key))))
			{
				ClientReliableCheckBlock(client, len);
				ClientReliableWrite_SZ(client, data, len);
			}
			else
				SZ_Write (&client->datagram, data, len);
		}
	}

	if (sv.mvdrecording && (!extended_sound || legacy->cursize
	    || (demo.recorder.fteprotocolextensions & FTE_PEXT_SOUNDDBL)
	    || (demo.recorder.fteprotocolextensions2 & FTE_PEXT2_REPLACEMENTDELTAS))) {
		message = rd && ((demo.recorder.fteprotocolextensions2 & FTE_PEXT2_REPLACEMENTDELTAS)
		    || (extended_sound && (demo.recorder.fteprotocolextensions & FTE_PEXT_SOUNDDBL)))
		    ? rd : legacy;
		if (mvd_only) {
			mvdhidden_block_header_t header;
			header.length = message->cursize - 2;
			// header.type_id = ...; < up to the mod to fill this part in

			// write to dem_multiple(0), which will be skipped by all major clients (ezQuake, FTE, fod)
			if (MVDWrite_HiddenBlockBegin(message->cursize + sizeof(header.length))) {
				MVD_SZ_Write(&header.length, sizeof(header.length));
				MVD_SZ_Write(message->data, message->cursize);
			}
		}
#ifdef SV_HAS_CSQC
		else if (csqc_only && !SV_WantsQCStats(&demo.recorder)) {
			// CSQC-only message to a stock-compatible recorder: keep it out of
			// the MVD/QTV stream
		}
#endif
		else if (reliable) {
			if (MVDWrite_Begin(dem_all, 0, message->cursize)) {
				MVD_SZ_Write(message->data, message->cursize);
			}
		}
		else {
			SZ_Write(&demo.datagram, message->data, message->cursize);
		}
	}
}

void SV_MulticastEx (vec3_t origin, int to, const char *cl_reliable_key)
{
	SV_MulticastExFiltered(origin, to, cl_reliable_key, false, &sv.multicast, NULL, false);
	SZ_Clear(&sv.multicast);
#ifdef SV_HAS_CSQC
	sv.multicast_csqc = false;
#endif
}

#ifdef SV_HAS_CSQC
/*
=======================
SV_CSQCMulticast

Dispatch a mod's CSQC packet (svc_fte_cgamepacket) to clients that negotiated
FTE_PEXT_CSQC and have CSQC active, plus the MVD recorder when sv_mvd_csqc is
enabled. Other clients never receive it.
=======================
*/
void SV_CSQCMulticast (vec3_t origin, int to)
{
	SV_MulticastExFiltered(origin, to, NULL, false, &sv.multicast, NULL, true);
	SZ_Clear(&sv.multicast);
	sv.multicast_csqc = false;
}
#endif

void SV_Multicast (vec3_t origin, int to)
{
	SV_MulticastEx(origin, to, NULL);
}

/* Preserve the original hook ID for RD and the historical alias for legacy. */
void SV_SendNQBeam(int entity, const byte *coordinates)
{
	vec3_t origin = {0, 0, 0};
	byte legacy_data[4 + 6 * sizeof(float)], rd_data[5 + 6 * sizeof(float)];
	sizebuf_t legacy, rd;
	int legacy_entity = entity > 1023 ? 0 : entity;

	SZ_Init(&legacy, legacy_data, sizeof(legacy_data));
	MSG_WriteByte(&legacy, svc_temp_entity);
	MSG_WriteByte(&legacy, TE_LIGHTNING1);
	MSG_WriteShort(&legacy, (short)(legacy_entity - 1288));
	SZ_Write(&legacy, coordinates, 6 * msg_coordsize);

	SZ_Init(&rd, rd_data, sizeof(rd_data));
	MSG_WriteByte(&rd, svc_temp_entity);
	MSG_WriteByte(&rd, TEQW_BEAM);
	MSG_RD_WriteEntityRef(&rd, entity);
	SZ_Write(&rd, coordinates, 6 * msg_coordsize);

	SV_FlushReliableDatagram();
	SV_MulticastExFiltered(origin, MULTICAST_ALL_R, NULL, false, &legacy, &rd, false);
}

void SV_SendNQExplosion2(const byte *payload)
{
	vec3_t origin = {0, 0, 0};
	byte legacy_data[2 + 3 * sizeof(float)], rd_data[4 + 3 * sizeof(float)];
	sizebuf_t legacy, rd;
	int coord_bytes = 3 * msg_coordsize;

	SZ_Init(&legacy, legacy_data, sizeof(legacy_data));
	MSG_WriteByte(&legacy, svc_temp_entity);
	MSG_WriteByte(&legacy, TE_EXPLOSION);
	SZ_Write(&legacy, payload, coord_bytes);

	/* NetQuake effect 12 is blood in QW. Only the FTE form carries the palette. */
	SZ_Init(&rd, rd_data, sizeof(rd_data));
	MSG_WriteByte(&rd, svc_temp_entity);
	MSG_WriteByte(&rd, TEQW_EXPLOSION2);
	SZ_Write(&rd, payload, coord_bytes + 2);

	SV_FlushReliableDatagram();
	SV_MulticastExFiltered(origin, MULTICAST_ALL_R, NULL, false, &legacy, &rd, false);
}

void SV_MulticastNQMessage(byte *data, int length)
{
	vec3_t origin = {0, 0, 0};
	sizebuf_t msg;

	SZ_Init(&msg, data, length);
	msg.cursize = length;
	SV_FlushReliableDatagram();
	SV_MulticastExFiltered(origin, MULTICAST_ALL_R, NULL, false, &msg, NULL, false);
}

/* Build both rail representations before the common PHS delivery path. */
void SV_RailTrail(vec3_t start, vec3_t end, int player, int color)
{
	byte legacy_data[4 + 6 * sizeof(float)], rd_data[2 + 6 * sizeof(float)];
	sizebuf_t legacy, rd;
	int i;

	SZ_Init(&legacy, legacy_data, sizeof(legacy_data));
	MSG_WriteByte(&legacy, svc_temp_entity);
	MSG_WriteByte(&legacy, TE_LIGHTNING1);
	MSG_WriteShort(&legacy, -264 + 8 * player + color);

	SZ_Init(&rd, rd_data, sizeof(rd_data));
	MSG_WriteByte(&rd, svc_temp_entity);
	MSG_WriteByte(&rd, TEQW_RAILTRAIL);
	for (i = 0; i < 3; i++) {
		MSG_WriteCoord(&legacy, start[i]);
		MSG_WriteCoord(&rd, start[i]);
	}
	for (i = 0; i < 3; i++) {
		MSG_WriteCoord(&legacy, end[i]);
		MSG_WriteCoord(&rd, end[i]);
	}
	SV_MulticastExFiltered(start, MULTICAST_PHS, NULL, false, &legacy, &rd, false);
}

void SV_StartParticle (vec3_t org, vec3_t dir, int color, int count,
					   int replacement_te, int replacement_count)
{
	int		i, v;
	qbool	send_count;

	//if (AllClientsWantSVCParticle())
	if (0)
	{
		MSG_WriteByte (&sv.multicast, nq_svc_particle);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
		for (i=0 ; i<3 ; i++)
		{
			v = dir[i]*16;
			if (v > 127)
				v = 127;
			else if (v < -128)
				v = -128;
			MSG_WriteChar (&sv.multicast, v);
		}
		MSG_WriteByte (&sv.multicast, count);
		MSG_WriteByte (&sv.multicast, color);
	}
	else
	{
		if (replacement_te == TE_EXPLOSION || replacement_te == TE_LIGHTNINGBLOOD)
			send_count = false;
		else if (replacement_te == TE_BLOOD || replacement_te == TE_GUNSHOT)
			send_count = true;
		else
			return;		// don't send anything

		MSG_WriteByte (&sv.multicast, svc_temp_entity);
		MSG_WriteByte (&sv.multicast, replacement_te);
		if (send_count)
			MSG_WriteByte (&sv.multicast, replacement_count);
		MSG_WriteCoord (&sv.multicast, org[0]);
		MSG_WriteCoord (&sv.multicast, org[1]);
		MSG_WriteCoord (&sv.multicast, org[2]);
	}

	SV_Multicast (org, MULTICAST_PVS);
}


/* Resolve a precached ambient sample without changing published sound IDs. */
int SV_AmbientSoundIndex(const char *sample)
{
	int index;

	for (index = 0; index < MAX_SOUNDS && sv.sound_precache[index]; ++index) {
		if (!strcmp(sv.sound_precache[index], sample)) {
			break;
		}
	}
	if (index == MAX_SOUNDS || !sv.sound_precache[index]) {
		return -1;
	}
	return index;
}

/* Static sounds use their legacy form unless the ID needs SOUNDDBL. */
void SV_WriteStaticSound(sizebuf_t *msg, const float origin[3], int sound_id,
                         int volume, int attenuation)
{
	int i;

	if (sound_id > 255) {
		MSG_WriteByte(msg, svc_fte_spawnstaticsound2);
		MSG_WriteByte(msg, STATIC_SOUND_LARGEINDEX);
	}
	else {
		MSG_WriteByte(msg, svc_spawnstaticsound);
	}
	for (i = 0; i < 3; ++i) {
		MSG_WriteCoord(msg, origin[i]);
	}
	if (sound_id > 255) {
		MSG_WriteShort(msg, sound_id);
	}
	else {
		MSG_WriteByte(msg, sound_id);
	}
	MSG_WriteByte(msg, volume);
	MSG_WriteByte(msg, attenuation);
}

/* Bounded sound precache and wire encoding shared by signon and MVD. */
int SV_SoundPrecacheCount(char *const *sounds, int capacity)
{
	int count = 0;

	while (count + 1 < capacity && sounds[count + 1]) {
		++count;
	}
	return count;
}

static void SV_WriteSoundPacket(sizebuf_t *msg, int entity, int channel,
                         int sound_id, int volume, float attenuation,
                         const float origin[3], qbool extended)
{
	int packed = (entity << 3) | channel;
	int index;

	if (extended) {
		/* A packed entity/channel word only has thirteen entity bits. */
		int flags = (sound_id > 255 ? NQSND_LARGESOUND : 0) | (entity >= 8192 ? NQSND_LARGEENTITY : 0);

		if (volume != DEFAULT_SOUND_PACKET_VOLUME) {
			flags |= NQSND_VOLUME;
		}
		if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION) {
			flags |= NQSND_ATTENUATION;
		}
		MSG_WriteByte(msg, svc_fte_soundextended);
		MSG_WriteByte(msg, flags);
	}
	else {
		MSG_WriteByte(msg, svc_sound);
		if (volume != DEFAULT_SOUND_PACKET_VOLUME) {
			packed |= SND_VOLUME;
		}
		if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION) {
			packed |= SND_ATTENUATION;
		}
		MSG_WriteShort(msg, packed);
	}
	if (volume != DEFAULT_SOUND_PACKET_VOLUME) {
		MSG_WriteByte(msg, volume);
	}
	if (attenuation != DEFAULT_SOUND_PACKET_ATTENUATION) {
		MSG_WriteByte(msg, attenuation * 64);
	}
	if (extended) {
		if (entity >= 8192) {
			MSG_RD_WriteEntityRef(msg, entity);
			MSG_WriteByte(msg, channel);
		}
		else {
			MSG_WriteShort(msg, packed);
		}
		if (sound_id > 255)
			MSG_WriteShort(msg, sound_id);
		else
			MSG_WriteByte(msg, sound_id);
	}
	else {
		MSG_WriteByte(msg, sound_id);
	}
	for (index = 0; index < 3; ++index) {
		MSG_WriteCoord(msg, origin[index]);
	}
}

/*
==================
SV_StartSound
 
Each entity can have eight independant sound sources, like voice,
weapon, feet, etc.
 
Channel 0 is an auto-allocate channel, the others override anything
already running on that entity/channel pair.
 
An attenuation of 0 will play full volume everywhere in the level.
Larger attenuations will drop off.  (max 4 attenuation)
 
==================
*/
void SV_StartSound (edict_t *entity, int channel, char *sample, int volume, float attenuation)
{
	byte legacy_data[64];
	sizebuf_t legacy;
	qbool extended;
	int     sound_num;
	int     i;
	int     ent;
	vec3_t  origin;
	qbool   use_phs;
	qbool   reliable = false;

	if (volume < 0 || volume > 255)
		SV_Error ("SV_StartSound: volume = %i", volume);

	if (attenuation < 0 || attenuation > 4)
		SV_Error ("SV_StartSound: attenuation = %f", attenuation);

	if (channel < 0 || channel > 15)
		SV_Error ("SV_StartSound: channel = %i", channel);

	// find precache number for sound
	for (sound_num=1 ; sound_num<MAX_SOUNDS
	        && sv.sound_precache[sound_num] ; sound_num++)
		if (!strcmp(sample, sv.sound_precache[sound_num]))
			break;

	if ( sound_num == MAX_SOUNDS || !sv.sound_precache[sound_num] )
	{
		Con_Printf ("SV_StartSound: %s not precached\n", sample);
		return;
	}

	ent = NUM_FOR_EDICT(entity);

	if ((channel & 8) || !(int)sv_phs.value)	// no PHS flag
	{
		if (channel & 8)
			reliable = true; // sounds that break the phs are reliable
		use_phs = false;
		channel &= 7;
	}
	else
		use_phs = true;

	//	if (channel == CHAN_BODY || channel == CHAN_VOICE)
	//		reliable = true;

	// use the entity origin unless it is a bmodel or a trigger
	if (entity->v->solid == SOLID_BSP || (entity->v->solid == SOLID_TRIGGER && entity->v->modelindex == 0))
	{
		for (i=0 ; i<3 ; i++)
			origin[i] = entity->v->origin[i]+0.5*(entity->v->mins[i]+entity->v->maxs[i]);
	}
	else
	{
		VectorCopy (entity->v->origin, origin);
	}

	extended = ent >= 1024 || sound_num > 255;
	SV_WriteSoundPacket(&sv.multicast, ent, channel, sound_num, volume,
	                    attenuation, origin, extended);
	SZ_Init(&legacy, legacy_data, sizeof(legacy_data));
	if (!extended) {
		legacy = sv.multicast;
	} else if (ent < 2048 && sound_num <= 255) {
		/* Legacy has eleven entity bits before its attenuation/volume flags. */
		SV_WriteSoundPacket(&legacy, ent, channel, sound_num, volume,
		                    attenuation, origin, false);
	}

	if (use_phs)
		SV_MulticastExFiltered (origin, reliable ? MULTICAST_PHS_R : MULTICAST_PHS, sv_reliable_sound.value ? "rsnd" : NULL,
		                        extended, &legacy, extended ? &sv.multicast : NULL, false);
	else
		SV_MulticastExFiltered (origin, reliable ? MULTICAST_ALL_R : MULTICAST_ALL, sv_reliable_sound.value ? "rsnd" : NULL,
		                        extended, &legacy, extended ? &sv.multicast : NULL, false);
	SZ_Clear(&sv.multicast);
#ifdef SV_HAS_CSQC
	sv.multicast_csqc = false;
#endif
}


/*
===============================================================================
 
FRAME UPDATES
 
===============================================================================
*/

int		sv_nailmodel, sv_supernailmodel, sv_playermodel;

void SV_FindModelNumbers (void)
{
	int		i;

	sv_nailmodel = -1;
	sv_supernailmodel = -1;
	sv_playermodel = -1;

	for (i=0 ; i<MAX_MODELS ; i++)
	{
		if (!sv.model_precache[i])
			break;
		if (!strcmp(sv.model_precache[i],"progs/spike.mdl"))
			sv_nailmodel = i;
		if (!strcmp(sv.model_precache[i],"progs/s_spike.mdl"))
			sv_supernailmodel = i;
		if (!strcmp(sv.model_precache[i],"progs/player.mdl"))
			sv_playermodel = i;
	}
}


/*
==================
SV_WriteClientdataToMessage
 
==================
*/
void SV_WriteClientdataToMessage (client_t *client, sizebuf_t *msg)
{
	int		i, clnum;
	edict_t	*other;
	edict_t	*ent;

	ent = client->edict;

	clnum = NUM_FOR_EDICT(ent) - 1;

	// send the chokecount for r_netgraph
	if (client->chokecount)
	{
		MSG_WriteByte (msg, svc_chokecount);
		MSG_WriteByte (msg, client->chokecount);
		client->chokecount = 0;
	}

	// send a damage message if the player got hit this frame
	if (ent->v->dmg_take || ent->v->dmg_save)
	{
		other = PROG_TO_EDICT(ent->v->dmg_inflictor);
		MSG_WriteByte (msg, svc_damage);
		MSG_WriteByte (msg, ent->v->dmg_save);
		MSG_WriteByte (msg, ent->v->dmg_take);
		for (i=0 ; i<3 ; i++)
			MSG_WriteCoord (msg, other->v->origin[i] + 0.5*(other->v->mins[i] + other->v->maxs[i]));

		ent->v->dmg_take = 0;
		ent->v->dmg_save = 0;
	}

	// add this to server demo
	if (sv.mvdrecording && msg->cursize)
	{
		if (MVDWrite_Begin(dem_single, clnum, msg->cursize))
		{
			MVD_SZ_Write(msg->data, msg->cursize);
		}
	}

	// a fixangle might get lost in a dropped packet.  Oh well.
	if (ent->v->fixangle)
	{
		ent->v->fixangle = 0;
		demo.fixangle[clnum] = true;

		MSG_WriteByte(msg, svc_setangle);

#ifdef MVD_PEXT1_HIGHLAGTELEPORT
		if (client->mvdprotocolextensions1 & MVD_PEXT1_HIGHLAGTELEPORT) {
			if (fofs_teleported) {
				client->lastteleport_teleport = ((eval_t *)((byte *)(client->edict)->v + fofs_teleported))->_int;
				if (client->lastteleport_teleport) {
					MSG_WriteByte(msg, 1); // signal a teleport
				}
				else {
					MSG_WriteByte(msg, 2); // respawn
				}
				client->lastteleport_outgoingseq = client->netchan.outgoing_sequence;
				client->lastteleport_incomingseq = client->netchan.incoming_sequence;
				client->lastteleport_teleportyaw = (client->edict)->v->angles[YAW] - client->lastcmd.angles[YAW];

				((eval_t *)((byte *)(client->edict)->v + fofs_teleported))->_int = 0;
				SV_RotateCmd(client, &client->lastcmd);
			}
			else {
				MSG_WriteByte(msg, 0); // we don't know, so no changes made...
			}
		}
#endif

		for (i=0 ; i < 3 ; i++)
			MSG_WriteAngle (msg, ent->v->angles[i] );

		if (sv.mvdrecording)
		{
			MSG_WriteByte (&demo.datagram, svc_setangle);
			MSG_WriteByte (&demo.datagram, clnum);
			for (i=0 ; i < 3 ; i++)
				MSG_WriteAngle (&demo.datagram, ent->v->angles[i] );
		}
	}

	// Z_EXT_TIME protocol extension
	// every now and then, send an update so that extrapolation
	// on client side doesn't stray too far off
#ifdef FTE_PEXT_ACCURATETIMINGS
	if (client->fteprotocolextensions & FTE_PEXT_ACCURATETIMINGS)
	{	//the fte pext causes the server to send out accurate timings, allowing for perfect interpolation.
		if (sv.physicstime - client->lastservertimeupdate > 0)
		{
			MSG_WriteByte(msg, svc_updatestatlong);
			MSG_WriteByte(msg, STAT_TIME);
			MSG_WriteLong(msg, (int)(sv.physicstime * 1000));

			client->lastservertimeupdate = sv.physicstime;
		}
	}
	else
#endif
	if ((SERVER_EXTENSIONS & Z_EXT_SERVERTIME) && (client->extensions & Z_EXT_SERVERTIME))
	{	//the zquake ext causes the server to send out peridoic timings, allowing for moderatly accurate game time.
		if (realtime - client->lastservertimeupdate > 5)
		{
			MSG_WriteByte(msg, svc_updatestatlong);
			MSG_WriteByte(msg, STAT_TIME);
			MSG_WriteLong(msg, (int) (sv.time * 1000));

			client->lastservertimeupdate = realtime;
		}
	}
}

/*
=======================
SV_ItemsStat

Items stat for sbar, shared by client and demo stats so they stay in sync.
=======================
*/
int SV_ItemsStat (edict_t *ent)
{
	uint32_t items = (uint32_t)(int)ent->v->items;

	// Both the sigil bits and items2 can set bit 31.
	items |= (uint32_t)(int)PR_GLOBAL(serverflags) << 28;

	if (fofs_items2) // ZQ_ITEMS2 extension
		items |= (uint32_t)(int)EdictFieldFloat(ent, fofs_items2) << 23;

	return (int)items;
}

/*
=======================
SV_UpdateClientStats
 
Performs a delta update of the stats array.  This should only be performed
when a reliable message can be delivered this frame.
=======================
*/
#ifdef SV_HAS_CSQC
// etype codes passed by mods (FTE convention; mvdsv's own etype_t lacks ev_integer)
#define CSQC_EV_STRING		1
#define CSQC_EV_FLOAT		2
#define CSQC_EV_VECTOR		3
#define CSQC_EV_ENTITY		4
#define CSQC_EV_INTEGER		8

typedef struct
{
	int		type;		// CSQC_EV_*
	int		statnum;	// 32..255
	int		fieldofs;	// clientstat: offset into entvars (0 if pointerstat)
	void	*ptr;		// pointerstat: resolved host pointer (NULL if clientstat)
	qbool	isfield;	// true = clientstat (per-client field), false = pointerstat (global)
} qcstat_t;

// wire kind of a registered statnum (drives the emit opcode): 0 = int (nothing
// registered), 1 = float, 2 = string. Filled by SV_QCStatEval, cleared with the
// registrations.
static qcstat_t qcstats[MAX_CL_STATS];
static unsigned int numqcstats;
static signed char qcstat_kind[MAX_CL_STATS];

int SV_QCStatKind (int statnum)
{
	if (statnum < 0 || statnum >= MAX_CL_STATS)
		return QCSTAT_KIND_INT;
	return qcstat_kind[statnum];
}

// byte size of the value the mod reads for a given stat type (used to bound
// the field offset against the entvars block)
static int qcstat_type_size(int type)
{
	switch (type)
	{
	case CSQC_EV_FLOAT:
	case CSQC_EV_ENTITY:
	case CSQC_EV_INTEGER:
	case CSQC_EV_STRING:
		return 4;
	case CSQC_EV_VECTOR:
		return 12;
	default:
		return -1;
	}
}

// progs (re)loaded (new map / new mod): drop all registered stats so stale
// pointerstat pointers into the old VM are never dereferenced and re-registration
// on the next map does not hit "Too many csqc stats".
void SV_ClearQCStats(void)
{
	numqcstats = 0;
	memset(qcstat_kind, 0, sizeof(qcstat_kind));
}

static void SV_QCStatEval(int type, int statnum, int fieldofs, void *ptr, qbool isfield)
{
	unsigned int i;

	if (statnum < 32 || statnum >= MAX_CL_STATS)
	{
		Con_Printf("csqc stat %i out of range (32..%i)\n", statnum, MAX_CL_STATS - 1);
		return;
	}

	if (type == CSQC_EV_VECTOR && statnum + 2 >= MAX_CL_STATS)
	{	// a vector stat occupies 3 consecutive slots (statnum..statnum+2)
		Con_Printf("csqc vector stat %i needs slots %i..%i (max %i)\n", statnum, statnum, statnum + 2, MAX_CL_STATS - 1);
		return;
	}

	for (i = 0; i < numqcstats; i++)
		if (qcstats[i].statnum == statnum)
			break;

	if (i == numqcstats)
	{
		if (i == sizeof(qcstats) / sizeof(qcstats[0]))
		{
			Con_Printf("Too many csqc stats specified\n");
			return;
		}
		numqcstats++;
	}

	qcstats[i].type = type;
	qcstats[i].statnum = statnum;
	qcstats[i].fieldofs = fieldofs;
	qcstats[i].ptr = ptr;
	qcstats[i].isfield = isfield;

	// wire kind drives the emit opcode (a vector occupies 3 float slots)
	qcstat_kind[statnum] = (type == CSQC_EV_STRING) ? QCSTAT_KIND_STRING
	                     : (type == CSQC_EV_FLOAT || type == CSQC_EV_VECTOR) ? QCSTAT_KIND_FLOAT
	                     : QCSTAT_KIND_INT;
	if (type == CSQC_EV_VECTOR)
	{
		qcstat_kind[statnum + 1] = QCSTAT_KIND_FLOAT;
		qcstat_kind[statnum + 2] = QCSTAT_KIND_FLOAT;
	}
}

// clientstat: register a per-client stat from a field offset into the mod's entvars
void SV_QCStatFieldIdx(int type, unsigned int fieldindex, int statnum)
{
	// The engine later reads (ent->v + fieldofs) up to sz bytes. The offset is
	// NOT validated against pr_edict_size here: mods register clientstats during
	// GAME_INIT, which runs before PR2_InitProg assigns pr_edict_size (so it is 0
	// on the first map and all registrations would be dropped) and a gamedir
	// switch can leave a stale larger value. Bound instead at use time in
	// SV_UpdateQCStats. Only the value type is checked here.
	if (qcstat_type_size(type) < 0)
	{
		Con_Printf("csqc clientstat type %d unsupported\n", type);
		return;
	}

	SV_QCStatEval(type, statnum, fieldindex, NULL, true);
}

// pointerstat: register a global stat from a resolved host pointer
void SV_QCStatPtr(int type, void *ptr, int statnum)
{
	SV_QCStatEval(type, statnum, 0, ptr, false);
}

// globalstat: resolve a global by name. PR2 has no name-based global lookup,
// so this is a no-op that logs (use pointerstat instead).
void SV_QCStatGlobal(int type, const char *name, int statnum)
{
	Con_Printf("globalstat \"%s\" unsupported on PR2, use pointerstat\n", name);
}

void SV_UpdateQCStats(edict_t *ent, int *statsi, float *statsf, const char **statss)
{
	unsigned int i;

	for (i = 0; i < numqcstats; i++)
	{
		eval_t *eval;
		qcstat_t *q = &qcstats[i];

		if (q->isfield)
		{
			int sz = qcstat_type_size(q->type);

			// bound at use, not at registration - GAME_INIT runs
			// before pr_edict_size is assigned, and a gamedir switch may leave a
			// stale (larger) value. Skip a field that is no longer within the
			// current entvars block instead of reading out of bounds.
			if (sz < 0 || q->fieldofs < 0
				|| (unsigned int)q->fieldofs + (unsigned int)sz > (unsigned int)pr_edict_size)
				continue;

			eval = (eval_t *)((byte *)ent->v + q->fieldofs);
		}
		else
			eval = (eval_t *)q->ptr;

		if (!eval)
			continue;

		switch (q->type)
		{
		case CSQC_EV_FLOAT:
			statsf[q->statnum] = eval->_float;
			break;
		case CSQC_EV_VECTOR:
			statsf[q->statnum + 0] = eval->vector[0];
			statsf[q->statnum + 1] = eval->vector[1];
			statsf[q->statnum + 2] = eval->vector[2];
			break;
		case CSQC_EV_ENTITY:
			{
				// the field value is mod data and may be out of range or hold a
				// non-entity float; never let it index sv.edicts (FTE returns
				// world for out-of-range values instead of crashing).
				unsigned int idx = (unsigned int)eval->edict / pr_edict_size;
				statsi[q->statnum] = (idx < (unsigned int)sv.num_edicts) ? (int)idx : 0;
				break;
			}
		case CSQC_EV_INTEGER:
			statsi[q->statnum] = eval->_int;
			break;
		case CSQC_EV_STRING:
			// a QC string field holds a string_t reference; resolve it (PR2)
#ifdef USE_PR2
			statss[q->statnum] = PR2_GetString(eval->string);
#else
			statss[q->statnum] = PR_GetEntityString(eval->string);
#endif
			break;
		default:
			break;
		}
	}
}

// Whether a stats destination (a live client or the MVD recorder) wants the
// clientstat/pointerstat range 32..255 emitted. Live clients: gate on the
// negotiated FTE_PEXT_CSQC ext - a CSQC-capable client that never runs csqc
// ignores the extra stats, and csqcactive implies the ext anyway. The
// recorder only gets FTE_PEXT_CSQC when sv_mvd_csqc is set (SV_MVD_Record),
// which is also what keeps stock ezQuake/QTV demos compatible.
qbool SV_WantsQCStats (client_t *client)
{
	return (client->fteprotocolextensions & FTE_PEXT_CSQC) != 0;
}
#endif

void SV_UpdateClientStats (client_t *client)
{
	edict_t *ent;
	int statsi[MAX_CL_STATS], i;
#ifdef SV_HAS_CSQC
	float statsf[MAX_CL_STATS];
	const char *statss[MAX_CL_STATS];

	memset (statsf, 0, sizeof(statsf));
	memset (statss, 0, sizeof(statss));
#endif

	memset (statsi, 0, sizeof(statsi));

	ent = client->edict;

	// if we are a spectator and we are tracking a player, we get his stats
	// so our status bar reflects his
	if (client->spectator && client->spec_track > 0)
		ent = svs.clients[client->spec_track - 1].edict;

	// in case of trackent we have to reflect his stats like for spectator.
	if (fofs_trackent)
	{
		int trackent = ((eval_t *)((byte *)(client->edict)->v + fofs_trackent))->_int;
		if (trackent < 1 || trackent > MAX_CLIENTS || svs.clients[trackent - 1].state != cs_spawned)
			trackent = 0;

		if (trackent)
			ent = svs.clients[trackent - 1].edict;
	}

	statsi[STAT_HEALTH] = ent->v->health;
	statsi[STAT_WEAPON] = SV_ModelIndex(PR_GetEntityString(ent->v->weaponmodel));
	statsi[STAT_AMMO] = ent->v->currentammo;
	statsi[STAT_ARMOR] = ent->v->armorvalue;
	statsi[STAT_SHELLS] = ent->v->ammo_shells;
	statsi[STAT_NAILS] = ent->v->ammo_nails;
	statsi[STAT_ROCKETS] = ent->v->ammo_rockets;
	statsi[STAT_CELLS] = ent->v->ammo_cells;
	if (!client->spectator || client->spec_track > 0)
		statsi[STAT_ACTIVEWEAPON] = ent->v->weapon;
	statsi[STAT_ITEMS] = SV_ItemsStat(ent);

	/* RD has no legacy PF_DEAD/PF_GIB viewheight override. */
	if (ent->v->health > 0 || client->spectator
	    || (client->fteprotocolextensions2 & FTE_PEXT2_REPLACEMENTDELTAS))
		statsi[STAT_VIEWHEIGHT] = ent->v->view_ofs[2];
#ifdef SV_HAS_CSQC
	// clientstat/pointerstat registered stats (32..255), only for CSQC clients.
	if (SV_WantsQCStats (client))
		SV_UpdateQCStats (ent, statsi, statsf, statss);
#endif

	for (i=0 ; i<MAX_CL_STATS ; i++)
	{
#ifdef SV_HAS_CSQC
		// CSQC float/string stats use their own wire opcodes, and only a
		// destination that negotiated the extension receives them: qcstat_kind[]
		// is global, so without this gate a stock (non-CSQC) client of a CSQC
		// mod would get opcode 78/79 and die with "Illegible server message".
		if (SV_WantsQCStats(client) && qcstat_kind[i] == QCSTAT_KIND_FLOAT)
		{
			if (statsf[i] != client->statsf[i])
			{
				int iv = (int)statsf[i];

				client->statsf[i] = statsf[i];
				client->stats[i] = iv;	// keep the int cache in sync
				if (statsf[i] && statsf[i] != (float)iv)
				{
					ClientReliableWrite_Begin(client, svc_fte_updatestatfloat, 6);
					ClientReliableWrite_Byte(client, i);
					ClientReliableWrite_Float(client, statsf[i]);
				}
				else if (iv >= 0 && iv <= 255)
				{
					ClientReliableWrite_Begin(client, svc_updatestat, 3);
					ClientReliableWrite_Byte(client, i);
					ClientReliableWrite_Byte(client, iv);
				}
				else
				{
					ClientReliableWrite_Begin(client, svc_updatestatlong, 6);
					ClientReliableWrite_Byte(client, i);
					ClientReliableWrite_Long(client, iv);
				}
			}
			continue;
		}
		if (SV_WantsQCStats(client) && qcstat_kind[i] == QCSTAT_KIND_STRING)
		{
			const char *s = statss[i] ? statss[i] : "";

			if (!client->statss[i] || strcmp(client->statss[i], s))
			{
				if (client->statss[i])
					Q_free(client->statss[i]);
				// store an empty string as "" (not NULL): NULL must mean
				// "never sent", otherwise an empty value would be re-emitted
				// every frame (the !statss[i] test would stay true).
				client->statss[i] = Q_strdup(s);
				ClientReliableWrite_Begin(client, svc_fte_updatestatstring, 3 + (int)strlen(s));
				ClientReliableWrite_Byte(client, i);
				ClientReliableWrite_String(client, (char *)s);
			}
			continue;
		}
#endif
		if (statsi[i] != client->stats[i])
		{
			client->stats[i] = statsi[i];
			if (statsi[i] >=0 && statsi[i] <= 255)
			{
				ClientReliableWrite_Begin(client, svc_updatestat, 3);
				ClientReliableWrite_Byte(client, i);
				ClientReliableWrite_Byte(client, statsi[i]);
			}
			else
			{
				ClientReliableWrite_Begin(client, svc_updatestatlong, 6);
				ClientReliableWrite_Byte(client, i);
				ClientReliableWrite_Long(client, statsi[i]);
			}
		}
	}
}

/*
=======================
SV_SendClientDatagram
=======================
*/
void SV_SendClientDatagram (client_t *client, int client_num)
{
	byte		buf[MAX_DATAGRAM];
	sizebuf_t	msg;
#ifdef SV_HAS_CSQC
	int sequence = client->netchan.outgoing_sequence;
#endif
#ifdef FTE_PEXT2_VOICECHAT
	int full_limit;
#endif
	//	packet_t	*pack;

	SZ_InitEx(&msg, buf, sizeof(buf), true);

	// for faster downloading skip half the frames
	/*if (client->download && client->netchan.outgoing_sequence & 1)
	{
		// we're sending fake invalid delta update, so that client won't update screen
		MSG_WriteByte (&msg, svc_deltapacketentities);
		MSG_WriteByte (&msg, 0);
		MSG_WriteShort (&msg, 0);

		Netchan_Transmit (&client->netchan, msg.cursize, buf);
	}
	*/

	if (!SV_SkipCommsBotMessage(client)) {
		// add the client specific data to the datagram
		SV_WriteClientdataToMessage(client, &msg);

		// send over all the objects that are in the PVS
		// this will include clients, a packetentities, and
		// possibly a nails update
		SV_WriteEntitiesToClient(client, &msg, false);

#ifdef FTE_PEXT2_VOICECHAT
		full_limit = msg.maxsize;
		if (!client->datagram.overflowed)
			msg.maxsize = max(msg.cursize, full_limit - client->datagram.cursize);
		SV_VoiceSendPacket(client, &msg);
		msg.maxsize = full_limit;
#endif
	}

	// Append queued multicast only if it fits, preserving the world update.
	if (client->datagram.overflowed)
		Con_Printf ("WARNING: datagram overflowed for %s\n", client->name);
	else if (!msg.overflowed && client->datagram.cursize <= msg.maxsize - msg.cursize)
		SZ_Write (&msg, client->datagram.data, client->datagram.cursize);
	SZ_Clear (&client->datagram);

	// send deltas over reliable stream
	if (Netchan_CanReliable (&client->netchan))
		SV_UpdateClientStats (client);

	if (msg.overflowed)
	{
		Con_Printf ("WARNING: msg overflowed for %s\n", client->name);
		SV_RD_DroppedFrame(client, client->netchan.incoming_sequence);
		SZ_Clear (&msg);
#ifdef SV_HAS_CSQC
		// The CSQC updates logged for this datagram were dropped with it, but
		// the client still acks the (now empty) packet, so re-flag them here
		// instead of waiting for an ack that will never report them lost.
		SV_CSQC_DroppedPacket (client, client->netchan.outgoing_sequence);
#endif
	}

	// send the datagram
	if (!Netchan_Transmit(&client->netchan, msg.cursize, buf)) {
		SV_RD_DroppedFrame(client, client->netchan.incoming_sequence);
#ifdef SV_HAS_CSQC
		/* Recover a pre-existing CSQC omission bug, independent of RD. */
		SV_CSQC_DroppedPacket(client, sequence);
#endif
	}
}

/*
=======================
SV_FlushReliableDatagram

Delivers the staged broadcast reliable data to each connected client, writes
the same data to an MVD recording, and clears the shared staging buffer.
=======================
*/
static void SV_FlushReliableDatagram (void)
{
	int j;
	client_t *client;

	if (!sv.reliable_datagram.cursize)
		return;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		/* Free and zombie slots cannot receive reliable traffic, while
		 * preconnected clients need roster updates before completing signon. */
		if (client->state < cs_preconnected)
			continue;

		ClientReliableCheckBlock(client, sv.reliable_datagram.cursize);
		ClientReliableWrite_SZ(client, sv.reliable_datagram.data, sv.reliable_datagram.cursize);
	}

	if (sv.mvdrecording)
	{
		if (MVDWrite_Begin(dem_all, 0, sv.reliable_datagram.cursize))
		{
			MVD_SZ_Write(sv.reliable_datagram.data, sv.reliable_datagram.cursize);
		}
	}

	SZ_Clear (&sv.reliable_datagram);
}

/*
=======================
SV_QueueFullClientUpdate

Appends a complete client roster update to the shared reliable staging buffer.
Flushes the current batch first to keep roster-generated batches strictly
below the smallest client reliable buffer size.
=======================
*/
static void SV_QueueFullClientUpdate (client_t *client)
{
	byte data[MAX_MSGLEN];
	sizebuf_t msg;

	SZ_Init(&msg, data, sizeof(data));
	SV_FullClientUpdate(client, &msg);

	if (sv.reliable_datagram.cursize + msg.cursize >= MIN_MTU)
		SV_FlushReliableDatagram();

	SZ_Write(&sv.reliable_datagram, msg.data, msg.cursize);
}

/*
=======================
SV_UpdateToReliableMessages
=======================
*/
static void SV_UpdateToReliableMessages (void)
{
	int i, j;
	client_t *client;
	edict_t *ent;

	// check for changes to be sent over the reliable streams to all clients
	for (i=0, sv_client = svs.clients ; i<MAX_CLIENTS ; i++, sv_client++)
	{
		/* Keep connection-time sendinfo pending until the client is spawned.
		 * Free/zombie states are deferred roster-removal notifications. */
		if (sv_client->sendinfo
			&& (sv_client->state == cs_free
				|| sv_client->state == cs_zombie
				|| sv_client->state == cs_spawned))
		{
			sv_client->sendinfo = false;
			SV_QueueFullClientUpdate (sv_client);
		}

		if (sv_client->state != cs_spawned)
			continue;

		ent = sv_client->edict;

		if (sv_client->old_frags != (int)ent->v->frags)
		{
			for (j=0, client = svs.clients ; j<MAX_CLIENTS ; j++, client++)
			{
				if (client->state < cs_preconnected)
					continue;
				ClientReliableWrite_Begin(client, svc_updatefrags, 4);
				ClientReliableWrite_Byte(client, i);
				ClientReliableWrite_Short(client, (int) ent->v->frags);
			}

			if (sv.mvdrecording)
			{
				if (MVDWrite_Begin(dem_all, 0, 4))
				{
					MVD_MSG_WriteByte(svc_updatefrags);
					MVD_MSG_WriteByte(i);
					MVD_MSG_WriteShort((int)ent->v->frags);
				}
			}

			sv_client->old_frags = (int) ent->v->frags;
		}

		// maxspeed/entgravity changes
		if (fofs_gravity && sv_client->entgravity != EdictFieldFloat(ent, fofs_gravity))
		{
			sv_client->entgravity = EdictFieldFloat(ent, fofs_gravity);
			ClientReliableWrite_Begin(sv_client, svc_entgravity, 5);
			ClientReliableWrite_Float(sv_client, sv_client->entgravity);
			if (sv.mvdrecording)
			{
				if (MVDWrite_Begin(dem_single, i, 5))
				{
					MVD_MSG_WriteByte(svc_entgravity);
					MVD_MSG_WriteFloat(sv_client->entgravity);
				}
			}
		}

		if (fofs_maxspeed && sv_client->maxspeed != EdictFieldFloat(ent, fofs_maxspeed))
		{
			sv_client->maxspeed = EdictFieldFloat(ent, fofs_maxspeed);
			ClientReliableWrite_Begin(sv_client, svc_maxspeed, 5);
			ClientReliableWrite_Float(sv_client, sv_client->maxspeed);
			if (sv.mvdrecording)
			{
				if (MVDWrite_Begin(dem_single, i, 5))
				{
					MVD_MSG_WriteByte(svc_maxspeed);
					MVD_MSG_WriteFloat(sv_client->maxspeed);
				}
			}
		}

	}

	if (sv.datagram.overflowed)
		SZ_Clear (&sv.datagram);

	SV_FlushReliableDatagram();

	// Append broadcast datagrams to spawned clients.
	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state != cs_spawned)
			continue;

		SZ_Write(&client->datagram, sv.datagram.data, sv.datagram.cursize);
	}

	if (sv.mvdrecording)
		SZ_Write(&demo.datagram, sv.datagram.data, sv.datagram.cursize); // FIXME: ???

	SZ_Clear (&sv.datagram);
}

//#ifdef _WIN32
//#pragma optimize( "", off )
//#endif



/*
=======================
SV_SendClientMessages
=======================
*/
void SV_SendClientMessages (void)
{
	int			i, j;
	client_t	*c;

	if (sv.state != ss_active)
		return;

	// update frags, names, etc
	SV_UpdateToReliableMessages ();

	if (fofs_visibility) {
		for (i = 0; i < MAX_CLIENTS; ++i) {
			((eval_t *)((byte *)(svs.clients[i].edict)->v + fofs_visibility))->_int = 0;
		}
	}

	// build individual updates
	for (i=0, c = svs.clients ; i<MAX_CLIENTS ; i++, c++)
	{
		if (!c->state)
			continue;

#ifdef SV_HAS_CSQC
		SV_ProcessSendFlags (c);
#endif

		if (c->drop)
		{
			SV_DropClient(c);
			c->drop = false;
			continue;
		}

		// check to see if we have a backbuf to stick in the reliable
		if (c->num_backbuf)
		{
			// will it fit?
			if (c->netchan.message.cursize + c->backbuf_size[0] <
			        c->netchan.message.maxsize)
			{

				Con_DPrintf("%s: backbuf %d bytes\n",
				            c->name, c->backbuf_size[0]);

				// it'll fit
				SZ_Write(&c->netchan.message, c->backbuf_data[0],
				         c->backbuf_size[0]);

				//move along, move along
				for (j = 1; j < c->num_backbuf; j++)
				{
					memcpy(c->backbuf_data[j - 1], c->backbuf_data[j],
					       c->backbuf_size[j]);
					c->backbuf_size[j - 1] = c->backbuf_size[j];
				}

				c->num_backbuf--;
				if (c->num_backbuf)
				{
					memset(&c->backbuf, 0, sizeof(c->backbuf));
					c->backbuf.data = c->backbuf_data[c->num_backbuf - 1];
					c->backbuf.cursize = c->backbuf_size[c->num_backbuf - 1];
					c->backbuf.maxsize = c->netchan.message.maxsize;
				}
			}
		}

#ifdef USE_PR2
		if (c->isBot) {
			// Write damage to bot clients too (for mvd playback)
			SV_BotWriteDamage(c, i);

			SZ_Clear(&c->netchan.message);
			SZ_Clear(&c->datagram);
			c->num_backbuf = 0;

			// Need to tell mod what the bot would have seen
			SV_SetVisibleEntitiesForBot(c);
			continue;
		}
#endif
		// if the reliable message overflowed,
		// drop the client
		if (c->netchan.message.overflowed)
		{
			SZ_Clear (&c->netchan.message);
			SZ_Clear (&c->datagram);
			SV_BroadcastPrintf (PRINT_HIGH, "%s overflowed\n", c->name);
			Con_Printf ("WARNING: reliable overflow for %s\n",c->name);
			SV_DropClient (c);
			c->send_message = true;
			c->netchan.cleartime = 0;	// don't choke this message
		}

		// only send messages if the client has sent one
		// and the bandwidth is not choked
		if (!c->send_message)
			continue;
		c->send_message = false;	// try putting this after choke?
		if (!sv.paused && !Netchan_CanPacket (&c->netchan))
		{
			c->chokecount++;
			continue;		// bandwidth choke
		}

		if (c->state == cs_spawned)
			SV_SendClientDatagram (c, i);
		else {
			Netchan_Transmit (&c->netchan, c->datagram.cursize, c->datagram.data);	// just update reliable
			c->datagram.cursize = 0;
		}
	}

#ifdef SV_HAS_CSQC
	if (sv.mvdrecording)
		SV_ProcessSendFlags (&demo.recorder);
	SV_CleanupEnts ();
#endif
}

static void SV_BotWriteDamage(client_t* c, int i)
{
	edict_t* ent = c->edict;

	if (c->edict->v->dmg_take || c->edict->v->dmg_save) {
		if (ent->v->dmg_take || ent->v->dmg_save) {
			int length = 3 + 3 * msg_coordsize;

			if (MVDWrite_Begin(dem_single, i, length)) {
				edict_t* other = PROG_TO_EDICT(ent->v->dmg_inflictor);

				MVD_MSG_WriteByte(svc_damage);
				MVD_MSG_WriteByte(ent->v->dmg_save);
				MVD_MSG_WriteByte(ent->v->dmg_take);
				for (i = 0; i < 3; i++)
					MVD_MSG_WriteCoord(other->v->origin[i] + 0.5 * (other->v->mins[i] + other->v->maxs[i]));
			}

			ent->v->dmg_take = 0;
			ent->v->dmg_save = 0;
		}
	}
}

void SV_MVDPings (void)
{
	client_t *client;
	int j;

	for (j = 0, client = svs.clients; j < MAX_CLIENTS; j++, client++)
	{
		if (client->state != cs_spawned)
			continue;

		if (MVDWrite_Begin (dem_all, 0, 7))
		{
			MVD_MSG_WriteByte (svc_updateping);
			MVD_MSG_WriteByte (j);
			MVD_MSG_WriteShort(SV_CalcPing(client));
			MVD_MSG_WriteByte (svc_updatepl);
			MVD_MSG_WriteByte (j);
			MVD_MSG_WriteByte (client->lossage);
		}
	}
}

void MVD_WriteStats(void)
{
	/* The recorder samples these HUD values. Other stats, such as level
	 * counters, must keep the state sent by their own messages. */
	static const uint32_t player_stats = (1u << STAT_HEALTH) | (1u << STAT_WEAPON)
		| (1u << STAT_AMMO) | (1u << STAT_ARMOR) | (1u << STAT_SHELLS)
		| (1u << STAT_NAILS) | (1u << STAT_ROCKETS) | (1u << STAT_CELLS)
		| (1u << STAT_ACTIVEWEAPON) | (1u << STAT_VIEWHEIGHT) | (1u << STAT_ITEMS);
	client_t	*c;
	int i, j;
	edict_t		*ent;
	int			statsi[MAX_CL_STATS];
	// legacy 32 stats unless the recorder is explicitly CSQC
#ifdef SV_HAS_CSQC
	float		statsf[MAX_CL_STATS];
	const char	*statss[MAX_CL_STATS];
	int			n = SV_WantsQCStats (&demo.recorder) ? MAX_CL_STATS : MAX_WIRE_STATS;
#else
	int			n = MAX_WIRE_STATS;
#endif

	for (i = 0, c = svs.clients ; i < MAX_CLIENTS ; i++, c++)
	{
		if (c->state != cs_spawned)
			continue;	// datagrams only go to spawned

		if (c->spectator)
			continue;

		ent = c->edict;
		memset (statsi, 0, sizeof(statsi));
#ifdef SV_HAS_CSQC
		memset (statsf, 0, sizeof(statsf));
		memset (statss, 0, sizeof(statss));
#endif

		statsi[STAT_HEALTH] = ent->v->health;
		statsi[STAT_WEAPON] = SV_ModelIndex(PR_GetEntityString(ent->v->weaponmodel));
		statsi[STAT_AMMO] = ent->v->currentammo;
		statsi[STAT_ARMOR] = ent->v->armorvalue;
		statsi[STAT_SHELLS] = ent->v->ammo_shells;
		statsi[STAT_NAILS] = ent->v->ammo_nails;
		statsi[STAT_ROCKETS] = ent->v->ammo_rockets;
		statsi[STAT_CELLS] = ent->v->ammo_cells;
		statsi[STAT_ACTIVEWEAPON] = ent->v->weapon;

		/* RD carries corpse/gib viewheight explicitly instead of legacy flags. */
		if (ent->v->health > 0 || SV_MVD_UsesRD()) {
			statsi[STAT_VIEWHEIGHT] = ent->v->view_ofs[2];
		}

		statsi[STAT_ITEMS] = SV_ItemsStat(ent);

#ifdef SV_HAS_CSQC
		// clientstat/pointerstat registered stats (32..255) for the recorder.
		if (SV_WantsQCStats (&demo.recorder))
			SV_UpdateQCStats (ent, statsi, statsf, statss);
#endif

		for (j = 0 ; j < n; j++)
		{
#ifdef SV_HAS_CSQC
			// float/string stats use their own wire opcodes
			if (SV_QCStatKind(j) == QCSTAT_KIND_FLOAT)
			{
				if (statsf[j] != demo.statsf[i][j])
				{
					int iv = (int)statsf[j];

					demo.statsf[i][j] = statsf[j];
					demo.stats[i][j] = iv;
					if (statsf[j] && statsf[j] != (float)iv)
					{
						if (MVDWrite_Begin(dem_stats, i, 6))
						{
							MVD_MSG_WriteByte(svc_fte_updatestatfloat);
							MVD_MSG_WriteByte(j);
							MVD_MSG_WriteFloat(statsf[j]);
						}
					}
					else if (iv >= 0 && iv <= 255)
					{
						if (MVDWrite_Begin(dem_stats, i, 3))
						{
							MVD_MSG_WriteByte(svc_updatestat);
							MVD_MSG_WriteByte(j);
							MVD_MSG_WriteByte(iv);
						}
					}
					else
					{
						if (MVDWrite_Begin(dem_stats, i, 6))
						{
							MVD_MSG_WriteByte(svc_updatestatlong);
							MVD_MSG_WriteByte(j);
							MVD_MSG_WriteLong(iv);
						}
					}
				}
				continue;
			}
			if (SV_QCStatKind(j) == QCSTAT_KIND_STRING)
			{
				const char *s = statss[j] ? statss[j] : "";

				if (!demo.statss[i][j] || strcmp(demo.statss[i][j], s))
				{
					if (demo.statss[i][j])
						Q_free(demo.statss[i][j]);
					// empty value stored as "" (see SV_UpdateClientStats)
					demo.statss[i][j] = Q_strdup(s);
					if (MVDWrite_Begin(dem_stats, i, 3 + (int)strlen(s)))
					{
						MVD_MSG_WriteByte(svc_fte_updatestatstring);
						MVD_MSG_WriteByte(j);
						MVD_MSG_WriteString(s);
					}
				}
				continue;
			}
#endif
			if (statsi[j] != demo.stats[i][j] ||
				(demo.resend_player_stats[i] && j < MAX_WIRE_STATS && (player_stats & (1u << j))))
			{
				demo.stats[i][j] = statsi[j];
				if (statsi[j] >= 0 && statsi[j] <= 255)
				{
					if (MVDWrite_Begin(dem_stats, i, 3))
					{
						MVD_MSG_WriteByte(svc_updatestat);
						MVD_MSG_WriteByte(j);
						MVD_MSG_WriteByte(statsi[j]);
					}
				}
				else
				{
					if (MVDWrite_Begin(dem_stats, i, 6))
					{
						MVD_MSG_WriteByte(svc_updatestatlong);
						MVD_MSG_WriteByte(j);
						MVD_MSG_WriteLong(statsi[j]);
					}
				}
			}
		}
		demo.resend_player_stats[i] = false;
	}
}

void DestFlush(qbool compleate);
void SV_MVD_RunPendingConnections(void);
void QTV_ReadDests( void );

void SV_SendDemoMessage(void)
{
	int			i, cls = 0;
	client_t	*c;
	sizebuf_t	msg;
	byte		msg_buf[MAX_MVD_SIZE]; // data without mvd header

	float		min_fps;
	extern		cvar_t sv_demofps, sv_demoIdlefps;
	extern		cvar_t sv_demoPings;

	if (sv.state != ss_active)
		return;

	SV_MVD_RunPendingConnections();

	if (!sv.mvdrecording)
	{
		DestFlush(false); // well, this may help close some fucked up dests
		return;
	}

	for (i = 0, c = svs.clients; i < MAX_CLIENTS; i++, c++)
	{
		if (c->state != cs_spawned)
			continue;	// datagrams only go to spawned

		cls |= 1 << i;
	}

	// if no players or paused, use idle fps
	if (cls && !sv.paused)
		min_fps = max(4.0, (int)sv_demofps.value ? (int)sv_demofps.value : 20.0);
	else
		min_fps = bound(4.0, (int)sv_demoIdlefps.value, 30);

	if (curtime - demo.curtime < 1.0 / min_fps) {
		return;
	}

	SZ_InitEx(&msg, msg_buf, sizeof(msg_buf), true);

	if ((int)sv_demoPings.value)
	{
		if (curtime - demo.pingtime > sv_demoPings.value)
		{
			SV_MVDPings();
			demo.pingtime = curtime;
		}
	}

	MVD_WriteStats();

	// send over all the objects that are in the PVS
	// this will include clients, a packetentities, and
	// possibly a nails update

	if (!demo.recorder.delta_sequence)
		demo.recorder.delta_sequence = -1;

	if (!SV_MVD_UsesRD()) {
		SV_WriteEntitiesToClient (&demo.recorder, &msg, true);

		if (msg.overflowed)
		{
			Sys_Printf("WARNING: msg overflowed in SV_SendDemoMessage\n");
		}
		else
		{
			if (msg.cursize)
			{
				if (MVDWrite_Begin(dem_all, 0, msg.cursize))
				{
					MVD_SZ_Write(msg.data, msg.cursize);
				}
			}
		}
	}
	else {
		SV_WriteNQMuzzleFlashes(NULL);
	}

	SZ_Clear(&msg);

	// copy the accumulated multicast datagram
	// for this client out to the message
	if (demo.datagram.overflowed)
	{
		Sys_Printf("WARNING: demo.datagram overflowed in SV_SendDemoMessage\n");
	}
	else
	{
		if (demo.datagram.cursize)
		{
			if (MVDWrite_Begin(dem_all, 0, demo.datagram.cursize))
			{
				MVD_SZ_Write (demo.datagram.data, demo.datagram.cursize);
			}
		}
	}

	SZ_Clear (&demo.datagram);

	if (SV_MVD_UsesRD()) {
		SV_MVD_FlushCurrentFrame();
		if (!SV_MVD_WriteRDEntities(false)) {
			SV_MVDStop(4, false);
			return;
		}
	}

	demo.recorder.delta_sequence = demo.recorder.netchan.incoming_sequence&255;
	demo.recorder.netchan.incoming_sequence++;
	if (!SV_MVD_UsesRD()) {
		demo.frames[demo.parsecount & UPDATE_MASK].time = sv.time;
		demo.frames[demo.parsecount & UPDATE_MASK].paused = sv.paused;
		demo.frames[demo.parsecount & UPDATE_MASK].pause_duration = (int)bound(0, (curtime - demo.curtime) * 1000.0f, 255);
	}
	demo.curtime = curtime;
	demo.time = sv.time;

	// let's not wait so much time (was 60)
	if (demo.parsecount - demo.lastwritten > 5) {
		SV_MVDWritePackets(1);
	}

	// flush once per demo frame
	DestFlush(false);

	// read QTV input once per demo frame
	QTV_ReadDests();

	if (!sv.mvdrecording)
		return;

	demo.parsecount++;
}


//#ifdef _WIN32
//#pragma optimize( "", on )
//#endif



/*
=======================
SV_SendMessagesToAll
 
FIXME: does this sequence right?
=======================
*/
void SV_SendMessagesToAll (void)
{
	int			i;
	client_t	*c;

	for (i=0, c = svs.clients ; i<MAX_CLIENTS ; i++, c++)
		if (c->state >= cs_connected)		// FIXME: should this only send to active?
			c->send_message = true;

	SV_SendClientMessages ();
}

#endif // !CLIENTONLY
