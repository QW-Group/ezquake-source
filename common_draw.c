/*
Module for common graphics (soft and GL)

Copyright (C) 2011 ezQuake team and kazik

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

#include <time.h>
#include "quakedef.h"
#include "localtime.h"
#include "common_draw.h"
#include "stats_grid.h"
#include "utils.h"
#include "Ctrl.h"

#include "gl_model.h"

// FIXME: this is horrible - points to &hud_gameclock_offset->integer
int* gameclockoffset;

#if 0
void Draw_CenterString (int y, char *str)
{
    Draw_String((vid.width-(8*strlen(str)))/2, y, str);
}

void Draw_CenterAltString (int y, char *str)
{
    Draw_Alt_String((vid.width-(8*strlen(str)))/2, y, str);
}

/*
==================
SCR_DrawDemoStatus
draws demo name, status and progress
==================
*/
int SCR_DrawDemoStatus(void)
{
    extern cvar_t demo_statustime;
    char st[128];
    int x, y;
    int w, i;
    int mins, secs;

    if (cls.timedemo || !cls.demoplayback)
        return 0;

    if (!cls.demopaused  &&  (realtime - cls.demobartime > demo_statustime.value  ||  cls.demobartime < 0))
        return 0;

    w = min(((vid.width / 2) / 16) * 2, 40);
    //w = 20;

    x = vid.width / 2 - (w*8/2);
    y = vid.height - sb_lines - 8;

    // status
    if (cls.demopaused)
        Draw_String(vid.width/2-4*6, y-16, "paused");

    // speed
    snprintf (st, sizeof (st), "%d%%", cls.demospeed);
    Draw_String(x+8, y-16, st);

    // status - time
    mins = ((int)(hosttime-cls.demostarttime)) / 60;
    secs = ((int)(hosttime-cls.demostarttime)) % 60;
    snprintf (st, sizeof (st), "%d:%02d", mins, secs);
    Draw_String(x+8*(w-strlen(st)-1), y-16, st);

    // progress bar
    memset(st, '\x81', w);
    st[0] = '\x80';
    st[w-1] = '\x82';
    st[w] = 0;
    Draw_String(x, y-24, st);
    Draw_Character((int)(x + 8*((cls.demopos / cls.demolength)*(w-3)+1)), y-24, '\x83');

    // demo name
    strlcpy (st, cls.demoname, sizeof (st));
    st[vid.width/8] = 0;
    Draw_Alt_String(vid.width/2 - 4*strlen(st), y-4, st);

    return 1;
}

/*
==================
SCR_PrepareCrosshair
prepare crosshair shape in memory for drawing functions (both soft and GL)
==================
*/
#define CSET(x,y) tab[x+5][y+5]=1
void PrepareCrosshair(int num, byte tab[10][10])
{
    int i, j;
    for (i=0; i < 10; i++)
        for (j=0; j < 10; j++)
            tab[i][j] = 0;

    switch (num)
    {
    case 3:
        CSET(0-2,0-2);
        CSET(0-3,0-3);
        CSET(0-4,0-4);
        CSET(0+2,0+2);
        CSET(0+3,0+3);
        CSET(0+4,0+4);
        CSET(0+2,0-2);
        CSET(0+3,0-3);
        CSET(0+4,0-4);
        CSET(0-2,0+2);
        CSET(0-3,0+3);
        CSET(0-4,0+4);
        break;
    case 4:
        CSET(0 - 2, 0 - 2);
        CSET(0 - 3, 0 - 3);
        CSET(0 + 2, 0 + 2);
        CSET(0 + 3, 0 + 3);
        CSET(0 + 2, 0 - 2);
        CSET(0 + 3, 0 - 3);
        CSET(0 - 2, 0 + 2);
        CSET(0 - 3, 0 + 3);
        break;
    case 5:
        CSET(0, 0 - 2);
        CSET(0, 0 - 3);
        CSET(0, 0 - 4);
        CSET(0 + 2, 0 + 2);
        CSET(0 + 3, 0 + 3);
        CSET(0 + 4, 0 + 4);
        CSET(0 - 2, 0 + 2);
        CSET(0 - 3, 0 + 3);
        CSET(0 - 4, 0 + 4);
        break;
    case 6:
        CSET(0    , 0 - 1);
        CSET(0 - 1, 0    );
        CSET(0    , 0    );
        CSET(0 + 1, 0    );
        CSET(0    , 0 + 1);
        break;
    case 7:
        CSET(0 - 1, 0 - 1);
        CSET(0    , 0 - 1);
        CSET(0 + 1, 0 - 1);
        CSET(0 - 1, 0    );
        CSET(0    , 0    );
        CSET(0 + 1, 0    );
        CSET(0 - 1, 0 + 1);
        CSET(0    , 0 + 1);
        CSET(0 + 1, 0 + 1);
        break;
    case 8:
        CSET(0 + 1, 0 + 1);
        CSET(0    , 0 + 1);
        CSET(0 + 1, 0    );
        CSET(0    , 0    );
        break;
    case 9:
        CSET(0, 0);
        break;
    case 10:
        CSET(-2, -2);
        CSET(-2, -1);
        CSET(-1, -2);
        CSET(2, -2);
        CSET(2, -1);
        CSET(1, -2);
        CSET(-2, 2);
        CSET(-2, 1);
        CSET(-1, 2);

        CSET(2, 2);
        CSET(2, 1);
        CSET(1, 2);
        break;
    case 11:
        CSET(0, -1);
        CSET(1, -1);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(0, 2);
        CSET(1, 2);
        break;
    case 12:
        CSET(0, -1);
        CSET(1, -1);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(0, 2);
        CSET(1, 2);

        CSET(0, 0);
        CSET(1, 0);
        CSET(0, 1);
        CSET(1, 1);
        break;
    case 13:
        CSET(0, -2);
        CSET(1, -2);
        CSET(-1, -1);
        CSET(0, -1);
        CSET(1, -1);
        CSET(2, -1);
        CSET(-2, 0);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(3, 0);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(3, 1);
        CSET(-1, 2);
        CSET(0, 2);
        CSET(1, 2);
        CSET(2, 2);
        CSET(0, 3);
        CSET(1, 3);
        break;
    case 14:
        CSET(0, -2);
        CSET(1, -2);
        CSET(-1, -1);
        CSET(0, -1);
        CSET(1, -1);
        CSET(2, -1);
        CSET(-2, 0);
        CSET(-1, 0);
        CSET(2, 0);
        CSET(3, 0);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET(2, 1);
        CSET(3, 1);
        CSET(-1, 2);
        CSET(0, 2);
        CSET(1, 2);
        CSET(2, 2);
        CSET(0, 3);
        CSET(1, 3);

        CSET(0, 0);
        CSET(1, 0);
        CSET(0, 1);
        CSET(1, 1);
        break;
    case 15:
        CSET(-1, -3);
        CSET( 0, -3);
        CSET( 1, -3);

        CSET(-2, -2);
        CSET(-1, -2);
        CSET( 0, -2);
        CSET( 1, -2);
        CSET( 2, -2);

        CSET(-3, -1);
        CSET(-2, -1);
        CSET(-1, -1);
        CSET( 1, -1);
        CSET( 2, -1);
        CSET( 3, -1);

        CSET(-3, 0);
        CSET(-2, 0);
        CSET( 2, 0);
        CSET( 3, 0);

        CSET(-3, 1);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET( 1, 1);
        CSET( 2, 1);
        CSET( 3, 1);

        CSET(-2, 2);
        CSET(-1, 2);
        CSET( 0, 2);
        CSET( 1, 2);
        CSET( 2, 2);

        CSET(-1, 3);
        CSET( 0, 3);
        CSET( 1, 3);
        break;
    case 16:
        CSET(-1, -3);
        CSET( 0, -3);
        CSET( 1, -3);

        CSET(-2, -2);
        CSET(-1, -2);
        CSET( 0, -2);
        CSET( 1, -2);
        CSET( 2, -2);

        CSET(-3, -1);
        CSET(-2, -1);
        CSET(-1, -1);
        CSET( 1, -1);
        CSET( 2, -1);
        CSET( 3, -1);

        CSET(-3, 0);
        CSET(-2, 0);
        CSET( 2, 0);
        CSET( 3, 0);

        CSET(-3, 1);
        CSET(-2, 1);
        CSET(-1, 1);
        CSET( 1, 1);
        CSET( 2, 1);
        CSET( 3, 1);

        CSET(-2, 2);
        CSET(-1, 2);
        CSET( 0, 2);
        CSET( 1, 2);
        CSET( 2, 2);

        CSET(-1, 3);
        CSET( 0, 3);
        CSET( 1, 3);

        CSET(-1,  0);
        CSET( 1,  0);
        CSET( 0, -1);
        CSET( 0,  1);
        CSET( 0,  0);
        break;
    default:
        CSET(0 - 1, 0);
        CSET(0 - 3, 0);
        CSET(0 + 1, 0);
        CSET(0 + 3, 0);
        CSET(0, 0 - 1);
        CSET(0, 0 - 3);
        CSET(0, 0 + 1);
        CSET(0, 0 + 3);
        break;
    }
}

/*
==================
SCR_DrawClients
draws clients list with selected values from userinfo
==================
*/
void SCR_DrawClients(void)
{
	extern sb_showclients;
	int uid_w, clients;
	int i, y, x;
	char line[128], buf[64];

	if (!sb_showclients || cls.state == ca_disconnected)
		return;

	// prepare
	clients = 0;
	uid_w = 3;
	for (i=0; i < MAX_CLIENTS; i++)
	{
		int w;

		if (!cl.players[i].name[0])
			continue;

		clients++;
		w = strlen(va("%d", cl.players[i].userid));

		if (w > uid_w)
			uid_w = w;
	}

	y = (vid.height - sb_lines - 8 * (clients + 6));
	y = max (y, 0);
	x = (vid.width - 320) / 2 + 4;

	strlcpy (line, " # ", sizeof (line));
	snprintf (buf, sizeof (buf), "%*.*s ", uid_w, uid_w, "uid");
	strlcat (line, buf, sizeof (line));
	snprintf (buf, sizeof (buf), "%-*.*s ", 16-uid_w, 16-uid_w, "name");
	strlcat (line, buf, sizeof (line));
	strlcat (line, "team skin     rate", sizeof (line));

	Draw_String (x, y, line);
	y += 8;

	strlcpy (line, "\x1D\x1F \x1D", sizeof (line));
	snprintf (buf, sizeof (buf), "%*.*s", uid_w-2, uid_w-2, "\x1E\x1E\x1E\x1E");
	strlcat (line, buf, sizeof (line));
	strlcat (line, "\x1F \x1D", sizeof (line));
	snprintf (buf, sizeof (buf), "%*.*s", 16-uid_w-2, 16-uid_w-2, "\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E\x1E");
	strlcat (line, buf, sizeof (line));
    strlcat (line, "\x1F \x1D\x1E\x1E\x1F \x1D\x1E\x1E\x1E\x1E\x1E\x1E\x1F \x1D\x1E\x1E\x1F", sizeof (line));

	Draw_String(x, y, line);
    y += 8;

	for (i=0; i < MAX_CLIENTS; i++)
	{
		if (!cl.players[i].name[0])
			continue;

		if (y > vid.height - 8)
			break;

		line[0] = 0;

		snprintf (buf, sizeof (buf), "%2d ", i);
		strlcat (line, buf, sizeof (line));

		snprintf (buf, sizeof (buf), "%*d ", uid_w, cl.players[i].userid);
        strlcat(line, buf, sizeof (line));

		snprintf (buf, sizeof (buf), "%-*.*s ", 16-uid_w, 16-uid_w, cl.players[i].name);
		strlcat (line, buf, sizeof (line));

		snprintf(buf, sizeof (buf), "%-4.4s ", Info_ValueForKey(cl.players[i].userinfo, "team"));
		strlcat (line, buf, sizeof (line));

		if (cl.players[i].spectator)
			strlcpy (buf, "<spec>   ", sizeof (buf));
		else
			snprintf (buf, sizeof (buf), "%-8.8s ", Info_ValueForKey(cl.players[i].userinfo, "skin"));

		strlcat (line, buf, sizeof (line));

		snprintf (buf, sizeof (buf), "%4d", min(9999, atoi(Info_ValueForKey(cl.players[i].userinfo, "rate"))));
		strlcat (line, buf, sizeof (line));

		Draw_String (x, y, line);
		y += 8;
	}
}
#endif

cachepic_node_t *cachepics[CACHED_PICS_HDSIZE];

/*
 * Removes the pic from the cache if refcount hits zero,
 * otherwise decreases refcount.
 */
qbool CachePic_Remove(const char *path)
{
	int key = Com_HashKey(path) % CACHED_PICS_HDSIZE;
	cachepic_node_t *searchpos = cachepics[key];
	cachepic_node_t *old = NULL;

	while (searchpos) {
		if (!strcmp(searchpos->data.name, path)) {
			searchpos->refcount--;

			if (searchpos->refcount == 0) {
				if (old == NULL) {
					/* It's the first entry in list.
					 * Set new start to next item in list
					 * or NULL if we're the only entry.*/
					cachepics[key] = searchpos->next;
				} else {
					/* It's in the middle or end of the list.
					 * Need to make sure the prev entry points
					 * to the next one and skips us. */
					old->next = searchpos->next;
				}
				/* Free both data and our entry itself */
				Q_free(searchpos->data.pic);
				Q_free(searchpos);
			}
			return true;
		}
		old = searchpos;
		searchpos = searchpos->next;
	}
	return false;
}

// Same as CachePic_Remove, but without specifying the name
qbool CachePic_RemoveByPic(mpic_t* pic)
{
	int key;

	for (key = 0; key < CACHED_PICS_HDSIZE; ++key) {
		cachepic_node_t *searchpos = cachepics[key];
		cachepic_node_t *old = NULL;

		while (searchpos) {
			if (searchpos->data.pic == pic) {
				searchpos->refcount--;

				if (searchpos->refcount == 0) {
					if (old == NULL) {
						/* It's the first entry in list.
						* Set new start to next item in list
						* or NULL if we're the only entry.*/
						cachepics[key] = searchpos->next;
					}
					else {
						/* It's in the middle or end of the list.
						* Need to make sure the prev entry points
						* to the next one and skips us. */
						old->next = searchpos->next;
					}
					/* Free both data and our entry itself */
					Q_free(searchpos->data.pic);
					Q_free(searchpos);
				}
				return true;
			}
			old = searchpos;
			searchpos = searchpos->next;
		}
	}
	return false;
}

mpic_t *CachePic_Find(const char *path, qbool inc_refcount) 
{
	int key = Com_HashKey(path) % CACHED_PICS_HDSIZE;
	cachepic_node_t *searchpos = cachepics[key];

	while (searchpos) {
		if (!strcmp(searchpos->data.name, path)) {
			if (inc_refcount) {
				searchpos->refcount++;
			}
			return searchpos->data.pic;
		}
		searchpos = searchpos->next;
	}

	return NULL;
}

/* Copies data if necessary to make use of the refcount */
mpic_t *CachePic_Add(const char *path, mpic_t *pic) 
{
	int key = Com_HashKey(path) % CACHED_PICS_HDSIZE;
	cachepic_node_t *searchpos = cachepics[key];
	cachepic_node_t **nextp = cachepics + key;

	while (searchpos) {
		/* Check if we already have the entry */
		if (!strcmp(searchpos->data.name, path)) {
			searchpos->refcount++;
			return searchpos->data.pic;
		}
		nextp = &searchpos->next;
		searchpos = searchpos->next;
	}

	/* We didn't find a matching entry, create a new one */
	searchpos = Q_malloc_named(sizeof(cachepic_node_t), path);
	searchpos->data.pic = Q_malloc_named(sizeof(mpic_t), path);
	memcpy(searchpos->data.pic, pic, sizeof(mpic_t));
	
	strlcpy(searchpos->data.name, path, sizeof(searchpos->data.name));
	searchpos->next = NULL; // Terminate the list.
	searchpos->refcount++;
	*nextp = searchpos;		// Connect to the list.

	return searchpos->data.pic;
}

void CachePics_Shutdown(void)
{
	int i;
	cachepic_node_t *cur = NULL, *nxt = NULL;

	for (i = 0; i < CACHED_PICS_HDSIZE; i++)
	{
		for (cur = cachepics[i]; cur; cur = nxt)
		{
			nxt = cur->next;
			Q_free(cur->data.pic);
			Q_free(cur);
		}
	}

	memset(cachepics, 0, sizeof(cachepics));
}

const int COLOR_WHITE = 0xFFFFFFFF;

color_t RGBA_TO_COLOR(byte r, byte g, byte b, byte a) 
{
	return ((r << 0) | (g << 8) | (b << 16) | (a << 24)) & 0xFFFFFFFF;
}

color_t RGBAVECT_TO_COLOR(byte rgba[4])
{
	return ((rgba[0] << 0) | (rgba[1] << 8) | (rgba[2] << 16) | (rgba[3] << 24)) & 0xFFFFFFFF;
}

color_t RGBAVECT_TO_COLOR_PREMULT(byte rgba[4])
{
	byte premult[4];
	float alpha = rgba[3] / 255.0f;

	premult[0] = rgba[0] * alpha;
	premult[1] = rgba[1] * alpha;
	premult[2] = rgba[2] * alpha;
	premult[3] = rgba[3];

	return ((premult[0] << 0) | (premult[1] << 8) | (premult[2] << 16) | (premult[3] << 24)) & 0xFFFFFFFF;
}

color_t RGBAVECT_TO_COLOR_PREMULT_SPECIFIC(byte rgba[4], float alpha)
{
	byte premult[4];

	premult[0] = rgba[0] * alpha;
	premult[1] = rgba[1] * alpha;
	premult[2] = rgba[2] * alpha;
	premult[3] = 255 * alpha;

	return ((premult[0] << 0) | (premult[1] << 8) | (premult[2] << 16) | (premult[3] << 24)) & 0xFFFFFFFF;
}

byte* COLOR_TO_RGBA(color_t i, byte rgba[4]) 
{
	rgba[0] = (i >> 0  & 0xFF);
	rgba[1] = (i >> 8  & 0xFF);
	rgba[2] = (i >> 16 & 0xFF);
	rgba[3] = (i >> 24 & 0xFF);

	return rgba;
}

byte* COLOR_TO_RGBA_PREMULT(color_t i, byte rgba[4])
{
	float alpha;

	rgba[0] = (i >> 0 & 0xFF);
	rgba[1] = (i >> 8 & 0xFF);
	rgba[2] = (i >> 16 & 0xFF);
	rgba[3] = (i >> 24 & 0xFF);

	alpha = rgba[3] / 255.0f;
	rgba[0] *= alpha;
	rgba[1] *= alpha;
	rgba[2] *= alpha;

	return rgba;
}

float* COLOR_TO_FLOATVEC_PREMULT(color_t i, float rgba[4])
{
	float alpha = (i >> 24 & 0xFF) / 255.0f;

	rgba[0] = ((i >> 0 & 0xFF) * alpha) / 255.0f;
	rgba[1] = ((i >> 8 & 0xFF) * alpha) / 255.0f;
	rgba[2] = ((i >> 16 & 0xFF) * alpha) / 255.0f;
	rgba[3] = alpha;

	return rgba;
}

//
// Draw a subpic fitted inside a specified area.
//
void Draw_FitAlphaSubPic (int x, int y, int target_width, int target_height, 
						  mpic_t *gl, int srcx, int srcy, int src_width, int src_height, float alpha)
{
	float scale_x = (target_width / (float)src_width);
	float scale_y = (target_height / (float)src_height);

	Draw_SAlphaSubPic2(x, y, gl, srcx, srcy, src_width, src_height, scale_x, scale_y, alpha);

	return;
}

//
// Draws a subpic tiled inside of a specified target area.
//
void Draw_SubPicTiled(int x, int y, 
					int target_width, int target_height, 
					mpic_t *pic, int src_x, int src_y, 
					int src_width, int src_height,
					float alpha)
{
    int cx					= 0;
	int cy					= 0;
	int scaled_src_width	= src_width; //(src_width * scale);
	int scaled_src_height	= src_height; //(src_height * scale); 
	int draw_width;
	int draw_height;

	while (cy < target_height)
    {
		while (cx < target_width)
        {
			draw_width = min(src_width, (target_width - cx));
			draw_height = min(src_height, (target_height - cy));

			// TODO : Make this work properly in GL so that scale can be used (causes huge rounding issues).
			Draw_AlphaSubPic((x + cx), (y + cy), 
							pic, 
							src_x, src_y, 
							draw_width, draw_height, 
							alpha);

            cx += scaled_src_width;
        }

        cx = 0;
        cy += scaled_src_height;
    }
}

const char* SCR_GetTime (const char *format)
{
	static char buf[128];
	time_t t;
	struct tm *ptm;

	time (&t);
	ptm = localtime (&t);
	if (!ptm) {
		return "-:-";
	}
	if (!strftime(buf, sizeof(buf) - 1, format, ptm)) {
		return "-:-";
	}
	//snprintf(buf, sizeof (buf), "%2d:%02d:%02d", tm->wHour, tm->wMinute, tm->wSecond);
	return buf;
}

char* SCR_GetGameTime(int t)
{
	static char str[9];
	float timelimit;

	timelimit = (t == TIMETYPE_GAMECLOCKINV) ? 60 * Q_atof(Info_ValueForKey(cl.serverinfo, "timelimit")) + 1: 0;

	if (cl.countdown || cl.standby)
		strlcpy (str, SecondsToMinutesString(timelimit), sizeof(str));
	else
		strlcpy (str, SecondsToMinutesString((int)fabs(timelimit - cl.gametime + *gameclockoffset)), sizeof(str));

	return str;
}

char* SCR_GetDemoTime(void)
{
	static char str[9]; // '01:02:03\0'
	strlcpy (str, SecondsToMinutesString((int) (cls.demotime - demostarttime)), sizeof(str));
	return str;
}

char* SCR_GetHostTime(void)
{
	static char str[9]; // '01:02:03\0'
	strlcpy(str, SecondsToMinutesString((int)curtime), sizeof(str));
	return str;
}

char* SCR_GetConnectedTime(void)
{
	static char str[9]; // '01:02:03\0'
	float time = (cl.servertime_works) ? cl.servertime : cls.realtime;
	strlcpy(str, SecondsToHourString((int)time), sizeof(str));
	return str;
}

int SCR_GetClockStringWidth(const char *s, qbool big, float scale, qbool proportional)
{
	int w = 0;
	if (big) {
		while (*s) {
			w += (*s++ == ':') ? (16 * scale) : (24 * scale);
		}
	}
	else {
		w = Draw_StringLength(s, -1, scale, proportional);
	}
	return w;
}

int SCR_GetClockStringHeight(qbool big, float scale)
{
	if (big) {
		return (24 * scale);
	}
	else {
		// FIXME: proportional
		return (LETTERWIDTH * scale);
	}
}

const char* SCR_GetTimeString(int timetype, const char *format)
{
	switch (timetype) {
		case TIMETYPE_GAMECLOCK:
		case TIMETYPE_GAMECLOCKINV:
			return SCR_GetGameTime(timetype);

		case TIMETYPE_DEMOCLOCK:
			return SCR_GetDemoTime();

		case TIMETYPE_HOSTCLOCK:
			return SCR_GetHostTime();

		case TIMETYPE_CONNECTEDCLOCK:
			return SCR_GetConnectedTime();

		case TIMETYPE_CLOCK:
		default:
			return SCR_GetTime(format);
	}
}

//
// Finds the coordinates in the bigfont texture of a given character.
//
void Draw_GetBigfontSourceCoords(char c, int char_width, int char_height, int *sx, int *sy)
{
	if (c >= 'A' && c <= 'Z')
	{
		(*sx) = ((c - 'A') % 8) * char_width;
		(*sy) = ((c - 'A') / 8) * char_height;
	}
	else if (c >= 'a' && c <= 'z')
	{
		// Skip A-Z, hence + 26.
		(*sx) = ((c - 'a' + 26) % 8) * char_width;
		(*sy) = ((c - 'a' + 26) / 8) * char_height;
	}
	else if (c >= '0' && c <= '9')
	{
		// Skip A-Z and a-z.
		(*sx) = ((c - '0' + 26 * 2) % 8 ) * char_width;
		(*sy) = ((c - '0' + 26 * 2) / 8) * char_height;
	}
	else if (c == ':')
	{
		// Skip A-Z, a-z and 0-9.
		(*sx) = ((c - '0' + 26 * 2 + 10) % 8) * char_width;
		(*sy) = ((c - '0' + 26 * 2 + 10) / 8) * char_height;
	}
	else if (c == '/')
	{
		// Skip A-Z, a-z, 0-9 and :
		(*sx) = ((c - '0' + 26 * 2 + 11) % 8) * char_width;
		(*sy) = ((c - '0' + 26 * 2 + 11) / 8) * char_height;
	}
	else
	{
		(*sx) = -1;
		(*sy) = -1;
	}
}

qbool Draw_BigFontAvailable(void)
{
	// mcharset looks ugly in software rendering, therefore don't allow it in there
	return Draw_CachePicSafe(MCHARSET_PATH, false, true) != NULL;
}

void SCR_DrawWadString(int x, int y, float scale, const char *t)
{
    extern  mpic_t	*sb_nums[2][11];
    extern  mpic_t	*sb_colon, *sb_slash;
	qbool			red = false;

	while (*t)
    {
        if (*t >= '0'  &&  *t <= '9')
        {
			Draw_STransPic(x, y, sb_nums[red ? 1 : 0][*t-'0'], scale);
            x += 24*scale;
        }
        else if (*t == ':')
        {
			Draw_STransPic (x, y, sb_colon, scale);
			x += 16*scale;
        }
		else if (*t == '/')
		{
			Draw_STransPic(x, y, sb_slash, scale);
            x += 16*scale;
		}
		else if (*t == '-')
		{
			x -= 12*scale;
			Draw_STransPic (x, y, sb_nums[red ? 1 : 0][10], scale);
			x += 28*scale;
		}
		else if (*t == ' ')
		{
			x += 16*scale;
		}
		else if(*t == 'c')
		{
			red = true;
		}
		else if(*t == 'r')
		{
			red = false;
		}
        t++;
    }
}

// little shortcut for SCR_HUD_DrawBar* functions
void SCR_HUD_DrawBar(int direction, int value, float max_value, byte *color, int x, int y, int width, int height)
{
	int amount;

	if(direction >= 2)
		// top-down
		amount = Q_rint(fabs((height * value) / max_value));
	else// left-right
		amount = Q_rint(fabs((width * value) / max_value));

	if(direction == 0)
		// left->right
		Draw_AlphaFillRGB(x, y, amount, height, RGBAVECT_TO_COLOR(color));
	else if (direction == 1)
		// right->left
		Draw_AlphaFillRGB(x + width - amount, y, amount, height, RGBAVECT_TO_COLOR(color));
	else if (direction == 2)
		// down -> up
		Draw_AlphaFillRGB(x, y + height - amount, width, amount, RGBAVECT_TO_COLOR(color));
	else
		// up -> down
		Draw_AlphaFillRGB(x, y, width, amount, RGBAVECT_TO_COLOR(color));
}

