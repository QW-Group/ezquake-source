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

	$Id: skin.c,v 1.24 2007-10-04 15:52:04 dkure Exp $
*/

#include "quakedef.h"
#ifdef GLQUAKE
#include "gl_model.h"
#include "gl_local.h"
#else
#include "r_model.h"
#include "r_local.h"
#endif
#include "teamplay.h"
#include "image.h"
#include "qtv.h"
#include "utils.h"


void OnChangeSkinForcing(cvar_t *var, char *string, qbool *cancel);

cvar_t	baseskin = {"baseskin", "base"};
cvar_t	noskins = {"noskins", "0"};

cvar_t	cl_name_as_skin = {"cl_name_as_skin", "0", 0, OnChangeSkinForcing};
cvar_t  enemyforceskins = {"enemyforceskins", "0", 0, OnChangeSkinForcing};
cvar_t  teamforceskins = {"teamforceskins", "0", 0, OnChangeSkinForcing};

char	allskins[MAX_OSPATH];

#define	MAX_CACHED_SKINS	128
skin_t	skins[MAX_CACHED_SKINS];

int		numskins;

// return type of skin forcing if it's allowed for the player in the POV
static int Skin_ForcingType(const char *team)
{
	if (teamforceskins.integer && TP_ThisPOV_IsHisTeam(team))
		return teamforceskins.integer;	// allow for teammates

	if (enemyforceskins.integer && !TP_ThisPOV_IsHisTeam(team)) {
		if (cls.demoplayback || cl.spectator) { // allow for demos
			return enemyforceskins.integer;
		} else {  // for gameplay respect the FPD
			if (!(cl.fpd & FPD_NO_FORCE_SKIN) && !(cl.fpd & FPD_NO_FORCE_COLOR))
				return enemyforceskins.integer;
			else
				return 0;
		}
	}

	// this was always only for demos
	if (cl_name_as_skin.integer && (cls.demoplayback || cl.spectator)) {
		return cl_name_as_skin.integer;
	}

	return 0;
}

// get player skin as player name or player id
char *Skin_AsNameOrId (player_info_t *sc) {
	static char name[MAX_OSPATH];
	int pn;
	char* mask;

	switch (Skin_ForcingType(sc->team)) {
	case 1: // get skin as player name
		Util_ToValidFileName(sc->name, name, sizeof(name));
		return name;
	break;

	case 2: // get skin as id
		snprintf(name, sizeof(name), "%d", sc->userid);
		return name;
	break;

	case 3: // get player's number (first teammate gets 1, second 2, ...)
		pn = TP_PlayersNumber(sc->userid, sc->team);
		if (pn) {
			mask = TP_ThisPOV_IsHisTeam(sc->team) ? "t%d" : "e%d";
			snprintf(name, sizeof(name), mask, pn);
			return name;
		}
	break;
	}

	return NULL;
}

char *Skin_FindName (player_info_t *sc) {
	int tracknum;
	static char name[MAX_OSPATH];

	if (allskins[0]) {
		strlcpy(name, allskins, sizeof(name));
	} else {
		char *s = Skin_AsNameOrId(sc);

		if (!s || !s[0])
			s = Info_ValueForKey(sc->userinfo, "skin");

		if (s && s[0])
			strlcpy(name, s, sizeof(name));
		else
			strlcpy(name, baseskin.string, sizeof(name));
	}

	if (cl.spectator && (tracknum = Cam_TrackNum()) != -1)
		skinforcing_team = cl.players[tracknum].team;
	else if (!cl.spectator)
		skinforcing_team = cl.players[cl.playernum].team;

	if (!cl.teamfortress && !(cl.fpd & FPD_NO_FORCE_SKIN)) {
		char *skinname = NULL;
		player_state_t *state;
		qbool teammate;

		teammate = (cl.teamplay && !strcmp(sc->team, skinforcing_team)) ? true : false;

		if (!cl.validsequence)
			goto nopowerups;

		state = cl.frames[cl.parsecount & UPDATE_MASK].playerstate + (sc - cl.players);

		if (state->messagenum != cl.parsecount)
			goto nopowerups;

		if ((state->effects & (EF_BLUE | EF_RED)) == (EF_BLUE | EF_RED) )
			skinname = teammate ? cl_teambothskin.string : cl_enemybothskin.string;
		else if (state->effects & EF_BLUE)
			skinname = teammate ? cl_teamquadskin.string : cl_enemyquadskin.string;
		else if (state->effects & EF_RED)
			skinname = teammate ? cl_teampentskin.string : cl_enemypentskin.string;

	nopowerups:
		if (!skinname || !skinname[0])
			skinname = teammate ? cl_teamskin.string : cl_enemyskin.string;
		if (skinname[0])
			strlcpy(name, skinname, sizeof(name));
	}

	if (strstr(name, "..") || *name == '.')
		strlcpy(name, baseskin.string, sizeof(name));

	return name;
}

//Determines the best skin for the given scoreboard slot, and sets scoreboard->skin
void Skin_Find_Ex (player_info_t *sc, char *skin_name) {
	skin_t *skin;
	int i;
	char name[MAX_OSPATH];

	if (!skin_name || !skin_name[0])
	{
		skin_name = baseskin.string;

		if (!skin_name[0])
			skin_name = "base";
	}

	strlcpy(name, skin_name, sizeof(name));
	COM_StripExtension(name, name);

	for (i = 0; i < numskins; i++)
	{
		if (!strcmp(name, skins[i].name))
		{
			if (sc)
				sc->skin = &skins[i];

			return;
		}
	}

	if (numskins == MAX_CACHED_SKINS)
	{	// ran out of spots, so flush everything
		Com_Printf ("MAX_CACHED_SKINS reached, flushing skins\n");
		Skin_Skins_f(); // this must set numskins to 0
	}

	skin = &skins[numskins];
	if (sc)
		sc->skin = skin;
	numskins++;

	memset (skin, 0, sizeof(*skin));
	strlcpy(skin->name, name, sizeof(skin->name));
}

void Skin_Find (player_info_t *sc)
{
	Skin_Find_Ex(sc, Skin_FindName(sc));
}


byte *Skin_PixelsLoad(char *name, int *max_w, int *max_h, int *bpp, int *real_width, int *real_height)
{
	byte *pic;

	*max_w = *max_h = *bpp = 0;

#ifdef GLQUAKE
	// PCX skins loads different, so using TEX_NO_PCX
	if ((pic = GL_LoadImagePixels (name, 0, 0, TEX_NO_PCX, real_width, real_height))) {
		// No limit in gl.
		*max_w	= *real_width;
		*max_h	= *real_height;
		*bpp	= 4; // 32 bit.

		return pic;
	}
#endif // GLQUAKE

#ifndef WITH_FTE_VFS
	if ((pic = Image_LoadPCX (NULL, 0, name, 0, 0, real_width, real_height))) 
#else
	if ((pic = Image_LoadPCX (NULL, name, 0, 0, real_width, real_height))) 
#endif
	{
		// PCX is limited.
		*max_w = 320;
		*max_h = 200;
		*bpp = 1; // 8 bit

		return pic;
	}

	return NULL;
}

#ifdef GLQUAKE

qbool skins_need_preache = true;

// HACK
void Skins_PreCache(void)
{
	int i;
	byte *tex;

	if (!skins_need_preache) // no need, we alredy done this
		return;

	skins_need_preache = false;

	// this must register skins with such names in skins[] array

	Skin_Find_Ex(NULL, cl_teamskin.string);
	Skin_Find_Ex(NULL, cl_enemyskin.string);
	Skin_Find_Ex(NULL, cl_teamquadskin.string);
	Skin_Find_Ex(NULL, cl_enemyquadskin.string);
	Skin_Find_Ex(NULL, cl_teampentskin.string);
	Skin_Find_Ex(NULL, cl_enemypentskin.string);
	Skin_Find_Ex(NULL, cl_teambothskin.string);
	Skin_Find_Ex(NULL, cl_enemybothskin.string);
	Skin_Find_Ex(NULL, baseskin.string);
	Skin_Find_Ex(NULL, "base");

	// now load all 24 bit skins in skins[] array

	for (i = 0; i < numskins; i++)
	{
		tex = Skin_Cache (&skins[i], false); // this precache skin file in mem

		if (!tex)
			continue; // nothing more we can do

		if (skins[i].bpp != 4) // we interesting in 24 bit skins only
			continue; 

		if (skins[i].texnum) // seems skin alredy loaded, at least we have some texture
			continue; 

		skins[i].texnum = GL_LoadTexture (skins[i].name, skins[i].width, skins[i].height, tex, (gl_playermip.integer ? TEX_MIPMAP : 0) | TEX_NOSCALE, 4);

		Com_DPrintf("skin precache: %s, texnum %d\n", skins[i].name, skins[i].texnum);
	}
}

#endif

// Returns a pointer to the skin bitmap, or NULL to use the default
byte *Skin_Cache (skin_t *skin, qbool no_baseskin) 
{
	int y, max_w, max_h, bpp, real_width = -1, real_height = -1;
	byte *pic = NULL, *out, *pix;
	char name[MAX_OSPATH];

	if (noskins.value == 1) // JACK: So NOSKINS > 1 will show skins, but
		return NULL;		// not download new ones.

	if (skin->failedload)
		return NULL;

	if ((out = (byte *) Cache_Check (&skin->cache)))
		return out;

	// not cached, load from HDD

	snprintf (name, sizeof(name), "skins/%s.pcx", skin->name);

	if (!(pic = Skin_PixelsLoad(name, &max_w, &max_h, &bpp, &real_width, &real_height)) || real_width > max_w || real_height > max_h) 
	{
		Q_free(pic);

		if (no_baseskin) 
		{
			skin->warned = true;
			return NULL; // well, we not set skin->failedload = true, that how I need it here
		}
		else if (!skin->warned)
		{
			Com_Printf ("Couldn't load skin %s\n", name);
		}

		skin->warned = true;
	}

	if (!pic) 
	{ 
		// Attempt load at least default/base.
		snprintf (name, sizeof(name), "skins/%s.pcx", baseskin.string);

		if (!(pic = Skin_PixelsLoad(name, &max_w, &max_h, &bpp, &real_width, &real_height)) || real_width > max_w || real_height > max_h) 
		{
			Q_free(pic);
			skin->failedload = true;
			return NULL;
		}
	}

	if (!(out = pix = (byte *) Cache_Alloc (&skin->cache, max_w * max_h * bpp, skin->name)))
		Sys_Error ("Skin_Cache: couldn't allocate");

	memset (out, 0, max_w * max_h * bpp);
	for (y = 0; y < real_height; y++, pix += (max_w * bpp))
	{
		memcpy (pix, pic + y * real_width * bpp, real_width * bpp);
	}

	Q_free (pic);
#ifdef GLQUAKE
	skin->bpp 	 = bpp;
	skin->width	 = real_width;
	skin->height = real_height;

	// load 32bit skin ASAP, so later we not affected by Cache changes, actually we don't need cache for 32bit skins at all
	//	skin->texnum = (bpp != 1) ? GL_LoadTexture (skin->name, skin->width, skin->height, pix, TEX_MIPMAP | TEX_NOSCALE, bpp) : 0;
	// FIXME: Above line does't work, texture loaded wrong, seems I need set some global gl states, but I dunno which,
	// so moved it to R_TranslatePlayerSkin() and here set texture to 0
	skin->texnum = 0;
#endif
	skin->failedload = false;

	return out;
}

void Skin_NextDownload (void) {
	player_info_t *sc;
	int i;

	if (cls.downloadnumber == 0)
		if (!com_serveractive || developer.value)
			Com_Printf ("Checking skins...\n");

	cls.downloadtype = dl_skin;

	for ( ; cls.downloadnumber >= 0 && cls.downloadnumber < MAX_CLIENTS; cls.downloadnumber++) {
		sc = &cl.players[cls.downloadnumber];
		if (!sc->name[0])
			continue;

		Skin_Find (sc);

		if (noskins.value)
			continue;

		if (Skin_Cache (sc->skin, true))
			continue; // we have it in cache, that mean we somehow able load this skin

		if (!CL_CheckOrDownloadFile(va("skins/%s.pcx", sc->skin->name)))
			return;		// started a download
	}

	cls.downloadtype = dl_none;

	// now load them in for real
	for (i = 0; i < MAX_CLIENTS; i++) {
		sc = &cl.players[i];
		if (!sc->name[0])
			continue;

		if (!sc->skin)
			Skin_Find (sc);

		Skin_Cache (sc->skin, false);
		sc->skin = NULL; // this way triggered skin loading, as i understand in R_TranslatePlayerSkin()
	}

	if (cls.state == ca_onserver /* && cbuf_current != &cbuf_main */) {	//only download when connecting
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("begin %i", cl.servercount));
	}
}

//Refind all skins, downloading if needed.
void Skin_Skins_f (void) {
	int i;

	for (i = 0; i < numskins; i++) {
		if (skins[i].cache.data)
			Cache_Free (&skins[i].cache);
	}
	numskins = 0;

#ifdef GLQUAKE
	skins_need_preache = true; // we need precache it ASAP
#endif

	cls.downloadnumber = 0;
	cls.downloadtype = dl_skin;
	Skin_NextDownload ();

	if (cls.mvdplayback == QTV_PLAYBACK && cbuf_current != &cbuf_main)
	{
		cls.qtv_donotbuffer = false;
	}
}

//Sets all skins to one specific one
void Skin_AllSkins_f (void) {
	if (Cmd_Argc() == 1) {
		Com_Printf("allskins set to \"%s\"\n", allskins);
		return;
	}
	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: %s [skin]\n", Cmd_Argv(0));
		return;
	}
	strlcpy (allskins, Cmd_Argv(1), sizeof(allskins));
	Skin_Skins_f();
}

//Just show skins which ezquake assign to each player, that depends on alot of variables and different conditions,
//so may be useful for checking settings
void Skin_ShowSkins_f (void) {
	int i, count, maxlen;

	maxlen = sizeof("name") - 1;

	for (i = 0; i < MAX_CLIENTS; i++)
		if (cl.players[i].name[0] && !cl.players[i].spectator)
			maxlen = bound(maxlen, strlen(cl.players[i].name), 17); // get len of longest name, but no more than 17

	for (i = count = 0; i < MAX_CLIENTS; i++)
		if (cl.players[i].name[0] && !cl.players[i].spectator) {
			if (!count)
				Com_Printf("\x02%-*.*s %s\n", maxlen, maxlen, "name", "skin");

			Com_Printf("%-*.*s %s\n", maxlen, maxlen, cl.players[i].name, Skin_FindName(&cl.players[i]));

			count++;
		}
}
