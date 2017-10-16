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
#include "gl_local.h"

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

    if (cls.timedemo  ||  !cls.demoplayback)
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

#define ATLAS_COUNT     4
#define ATLAS_WIDTH  2048
#define ATLAS_HEIGHT 2048

static cachepic_node_t *cachepics[CACHED_PICS_HDSIZE];
static cachepic_node_t wadpics[WADPIC_PIC_COUNT];
static cachepic_node_t charsetpics[MAX_CHARSETS];

static int  atlas_allocated[ATLAS_COUNT][ATLAS_WIDTH];
static byte atlas_texels[ATLAS_COUNT][ATLAS_WIDTH * ATLAS_HEIGHT * 4];
static byte atlas_dirty = 0;
static int  atlas_texnum[ATLAS_COUNT];

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
	searchpos = Q_malloc(sizeof(cachepic_node_t));

	searchpos->data.pic = Q_malloc(sizeof(mpic_t));
	memcpy(searchpos->data.pic, pic, sizeof(mpic_t));
	
	strlcpy(searchpos->data.name, path, sizeof(searchpos->data.name));
	searchpos->next = NULL; // Terminate the list.
	searchpos->refcount++;
	*nextp = searchpos;		// Connect to the list.

	return searchpos->data.pic;
}

void CachePics_DeInit(void) 
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

// Returns false if allocation failed.
static qbool CachePics_AllocBlock(int atlas_num, int w, int h, int *x, int *y)
{
	int i, j, best, best2;

	best = ATLAS_HEIGHT;

	for (i = 0; i < ATLAS_WIDTH - w; i++) {
		best2 = 0;

		for (j = 0; j < w; j++) {
			if (atlas_allocated[atlas_num][i + j] >= best)
				break;
			if (atlas_allocated[atlas_num][i + j] > best2)
				best2 = atlas_allocated[atlas_num][i + j];
		}

		if (j == w) {
			// This is a valid spot.
			*x = i;
			*y = best = best2;
		}
	}

	if (best + h > ATLAS_HEIGHT) {
		return false;
	}

	for (i = 0; i < w; i++) {
		atlas_allocated[atlas_num][*x + i] = best + h;
	}

	atlas_dirty |= (1 << atlas_num);

	return true;
}

int CachePics_AddToAtlas(mpic_t* pic)
{
	static char buffer[ATLAS_WIDTH * ATLAS_HEIGHT * 4];
	int width = pic->width, height = pic->height;
	int texWidth = 0, texHeight = 0;
	int i;

	// Find size of the source
	glBindTexture(GL_TEXTURE_2D, pic->texnum);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &texWidth);
	glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &texHeight);

	width = (pic->sh - pic->sl) * texWidth;
	height = (pic->th - pic->tl) * texHeight;

	if (texWidth > ATLAS_WIDTH || texHeight > ATLAS_HEIGHT) {
		return -1;
	}
	//Con_Printf(" > size = %d x %d = %d\n", width, height, width * height);

	// Allocate space in an atlas texture
	for (i = 0; i < ATLAS_COUNT; ++i) {
		int x_pos, y_pos;
		int padding = 1;

		if (CachePics_AllocBlock(i, width + (width == ATLAS_WIDTH ? 0 : padding), height + (height == ATLAS_HEIGHT ? 0 : padding), &x_pos, &y_pos)) {
			char* b = buffer;
			int xOffset, yOffset;

			//Con_Printf(" > > alloc %d %d\n", x_pos, y_pos);

			// Copy texture image
			glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

			for (yOffset = 0; yOffset < height; ++yOffset) {
				for (xOffset = 0; xOffset < width; ++xOffset) {
					int y = y_pos + yOffset;
					int x = x_pos + xOffset;
					int base = (x + y * ATLAS_WIDTH) * 4;
					int srcBase = (xOffset + pic->sl * texWidth + (yOffset + pic->tl * texHeight) * texWidth) * 4;

					atlas_texels[i][base] = buffer[srcBase];
					atlas_texels[i][base + 1] = buffer[srcBase + 1];
					atlas_texels[i][base + 2] = buffer[srcBase + 2];
					atlas_texels[i][base + 3] = buffer[srcBase + 3];
				}
			}

			if (false) {
				pic->sl = (x_pos + 0.25) / (float)ATLAS_WIDTH;
				pic->sh = (x_pos + width - 0.25) / (float)ATLAS_WIDTH;
				pic->tl = (y_pos + 0.25) / (float)ATLAS_HEIGHT;
				pic->th = (y_pos + height - 0.25) / (float)ATLAS_HEIGHT;
				pic->texnum = atlas_texnum[i];
			}
			else if (true) {
				pic->sl = (x_pos) / (float)ATLAS_WIDTH;
				pic->sh = (x_pos + width) / (float)ATLAS_WIDTH;
				pic->tl = (y_pos) / (float)ATLAS_HEIGHT;
				pic->th = (y_pos + height) / (float)ATLAS_HEIGHT;
				pic->texnum = atlas_texnum[i];
			}

			return i;
		}
		else {
			Con_Printf("*** FAILED TO PLACE: %d (%d x %d) ***\n", pic->texnum, width, height);
		}
	}

	return -1;
}

void CachePics_AtlasUpload(void)
{
	int i;

	for (i = 0; i < ATLAS_COUNT; ++i) {
		if (atlas_dirty & (1 << i)) {
			char id[64];
			// generate id
			snprintf(id, sizeof(id), "cachepics:%d", i);
			// 
			atlas_texnum[i] = GL_LoadTexture(id, ATLAS_WIDTH, ATLAS_HEIGHT, atlas_texels[i], TEX_ALPHA | TEX_NOSCALE, 4);
		}
	}
	atlas_dirty = 0;
}

void CachePics_Init(void)
{
	atlas_dirty = ~0;

	CachePics_AtlasUpload();
	Cmd_AddCommand("createatlas", CachePics_CreateAtlas);
}

void CachePics_InsertBySize(cachepic_node_t** sized_list, cachepic_node_t* node)
{
	int size_node = node->data.pic->width * node->data.pic->height;
	cachepic_node_t* current = *sized_list;
	int size_this;

	while (current) {
		size_this = current->data.pic->width * current->data.pic->height;
		if (size_this > size_node) {
			sized_list = &current->size_order;
			current = *sized_list;
			continue;
		}

		break;
	}

	node->size_order = current;
	*sized_list = node;
}

void CachePics_CreateAtlas(void)
{
	cachepic_node_t* sized_list = NULL;
	cachepic_node_t* cur;
	int i;
	int expected = 0, found = 0;

	// Delete old atlas textures
	memset(atlas_texels, 0, sizeof(atlas_texels));
	memset(atlas_allocated, 0, sizeof(atlas_allocated));
	memset(wadpics, 0, sizeof(wadpics));

	// Copy wadpic data over
	for (i = 0; i < WADPIC_PIC_COUNT; ++i) {
		extern wadpic_t wad_pictures[WADPIC_PIC_COUNT];

		if (wad_pictures[i].pic) {
			wadpics[i].data.pic = wad_pictures[i].pic;

			CachePics_InsertBySize(&sized_list, &wadpics[i]);
			++expected;
		}
	}

	// Copy text images over
	for (i = 0; i < MAX_CHARSETS; ++i) {
		extern mpic_t char_textures[MAX_CHARSETS];

		if (char_textures[i].texnum) {
			charsetpics[i].data.pic = &char_textures[i];

			CachePics_InsertBySize(&sized_list, &charsetpics[i]);
			++expected;
		}
	}

	// Create atlas textures
	for (i = 0; i < CACHED_PICS_HDSIZE; ++i) {
		cachepic_node_t *cur;

		for (cur = cachepics[i]; cur; cur = cur->next) {
			CachePics_InsertBySize(&sized_list, cur);
			++expected;
		}
	}

	for (cur = sized_list; cur; cur = cur->size_order) {
		++found;
	}

	Con_Printf("Found %d/%d\n", found, expected);

	for (cur = sized_list; cur; cur = cur->size_order) {
		int old_tex = cur->data.pic->texnum;
		mpic_t old = *cur->data.pic;
		int placed;

		//Con_Printf("%s\n", cur->data.name);

		placed = CachePics_AddToAtlas(cur->data.pic);

		/*if (placed >= 0) {
			Con_Printf("  placed @ atlas %d [%d, was %d]\n", placed, atlas_texnum[placed], old_tex);
			Con_Printf("  [%1.4f %1.4f > %1.4f %1.4f] now [%1.4f %1.4f > %1.4f %1.4f]\n", old.sl, old.sh, old.tl, old.th, cur->data.pic->sl, cur->data.pic->sh, cur->data.pic->tl, cur->data.pic->th);
		}
		else {
			Con_Printf("  Failed to place\n");
		}*/
	}

	{
		extern mpic_t* sb_ibar;

		Con_Printf("Created atlas, sb_ibar.tex = %d, wadpics[ibar] = %s\n", sb_ibar->texnum, wad_pictures[WADPIC_SB_IBAR].pic ? "yes" : "no");
	}

	// Upload atlas textures
	CachePics_AtlasUpload();
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

byte* COLOR_TO_RGBA(color_t i, byte rgba[4]) 
{
	rgba[0] = (i >> 0  & 0xFF);
	rgba[1] = (i >> 8  & 0xFF);
	rgba[2] = (i >> 16 & 0xFF);
	rgba[3] = (i >> 24 & 0xFF);

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

// ---------------------------
// OSD advanced net statistics
void SCR_NetStats(int x, int y, float period)
{
    char line[128];
    double t;

    // static data
    static double last_calculated;
    static int    ping_min, ping_max, ping_avg;
    static float  ping_dev;
    static float  f_min, f_max, f_avg;
    static int    lost_lost, lost_delta, lost_rate, lost_total;
    static int    size_all, size_in, size_out;
    static int    bandwidth_all, bandwidth_in, bandwidth_out;
    static int    with_delta;

    if (cls.state != ca_active)
        return;

    if (period < 0)
        period = 0;

    t = Sys_DoubleTime();
    if (t - last_calculated > period)
    {
        // recalculate

        net_stat_result_t result;

        last_calculated = t;

        CL_CalcNetStatistics(
            period,             // period of time
            network_stats,      // samples table
            NETWORK_STATS_SIZE, // number of samples in table
            &result);           // results

        if (result.samples == 0)
            return; // error calculating net

        ping_avg = (int)(result.ping_avg + 0.5);
        ping_min = (int)(result.ping_min + 0.5);
        ping_max = (int)(result.ping_max + 0.5);
        ping_dev = result.ping_dev;

        clamp(ping_avg, 0, 999);
        clamp(ping_min, 0, 999);
        clamp(ping_max, 0, 999);
        clamp(ping_dev, 0, 99.9);

        f_avg = (int)(result.ping_f_avg + 0.5);
        f_min = (int)(result.ping_f_min + 0.5);
        f_max = (int)(result.ping_f_max + 0.5);

        clamp(f_avg, 0, 99.9);
        clamp(f_min, 0, 99.9);
        clamp(f_max, 0, 99.9);

        lost_lost     = (int)(result.lost_lost  + 0.5);
        lost_rate     = (int)(result.lost_rate  + 0.5);
        lost_delta    = (int)(result.lost_delta + 0.5);
        lost_total    = (int)(result.lost_lost + result.lost_rate + result.lost_delta + 0.5);

        clamp(lost_lost,  0, 100);
        clamp(lost_rate,  0, 100);
        clamp(lost_delta, 0, 100);
        clamp(lost_total, 0, 100);

        size_in  = (int)(result.size_in  + 0.5);
        size_out = (int)(result.size_out + 0.5);
        size_all = (int)(result.size_in + result.size_out + 0.5);

        bandwidth_in  = (int)(result.bandwidth_in  + 0.5);
        bandwidth_out = (int)(result.bandwidth_out + 0.5);
        bandwidth_all = (int)(result.bandwidth_in + result.bandwidth_out + 0.5);


        clamp(size_in,  0, 999);
        clamp(size_out, 0, 999);
        clamp(size_all, 0, 999);
        clamp(bandwidth_in,  0, 99999);
        clamp(bandwidth_out, 0, 99999);
        clamp(bandwidth_all, 0, 99999);

        with_delta = result.delta;
    }

    Draw_Alt_String(x+36, y, "latency");
    y+=12;

    snprintf (line, sizeof (line), "min  %4.1f %3d ms", f_min, ping_min);
    Draw_String(x, y, line);
    y+=8;

    snprintf(line, sizeof (line), "avg  %4.1f %3d ms", f_avg, ping_avg);
    Draw_String(x, y, line);
    y+=8;

    snprintf(line, sizeof (line), "max  %4.1f %3d ms", f_max, ping_max);
    Draw_String(x, y, line);
    y+=8;

    snprintf(line, sizeof (line), "dev     %5.2f ms", ping_dev);
    Draw_String(x, y, line);
    y+=12;

    Draw_Alt_String(x+20, y, "packet loss");
    y+=12;

    snprintf(line, sizeof (line), "lost       %3d %%", lost_lost);
    Draw_String(x, y, line);
    y+=8;

    snprintf(line, sizeof (line), "rate cut   %3d %%", lost_rate);
    Draw_String(x, y, line);
    y+=8;

    if (with_delta)
        snprintf(line, sizeof (line), "bad delta  %3d %%", lost_delta);
    else
        strlcpy (line, "no delta compr", sizeof (line));
    Draw_String(x, y, line);
    y+=8;

    snprintf(line, sizeof (line), "total      %3d %%", lost_total);
    Draw_String(x, y, line);
    y+=12;


    Draw_Alt_String(x+4, y, "packet size/BPS");
    y+=12;

    snprintf(line, sizeof (line), "out    %3d %5d", size_out, bandwidth_out);
    Draw_String(x, y, line);
    y+=8;

    snprintf(line, sizeof (line), "in     %3d %5d", size_in, bandwidth_in);
    Draw_String(x, y, line);
    y+=8;

    snprintf(line, sizeof (line), "total  %3d %5d", size_all, bandwidth_all);
    Draw_String(x, y, line);
    y+=8;
}

const char* SCR_GetTime (const char *format)
{
	static char buf[128];
	time_t t;
	struct tm *ptm;

	time (&t);
	ptm = localtime (&t);
	if (!ptm) return "-:-";
	if (!strftime(buf, sizeof(buf)-1, format, ptm)) return "-:-";
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
		strlcpy (str, SecondsToMinutesString((int) abs(timelimit - cl.gametime + *gameclockoffset)), sizeof(str));

	return str;
}

char* SCR_GetDemoTime(void)
{
	static char str[9]; // '01:02:03\0'
	strlcpy (str, SecondsToMinutesString((int) (cls.demotime - demostarttime)), sizeof(str));
	return str;
}

int SCR_GetClockStringWidth(const char *s, qbool big, float scale)
{
	int w = 0;
	if (big) {
		while (*s) {
			w += (*s++ == ':') ? (16 * scale) : (24 * scale);
		}
	} else {
		w = strlen(s) * LETTERWIDTH * scale;
	}
	return w;
}

int SCR_GetClockStringHeight(qbool big, float scale)
{
	if (big) return (24 * scale);
	else return (LETTERWIDTH * scale);
}

const char* SCR_GetTimeString(int timetype, const char *format)
{
	switch (timetype) {
		case TIMETYPE_GAMECLOCK:
		case TIMETYPE_GAMECLOCKINV:
			return SCR_GetGameTime(timetype);

		case TIMETYPE_DEMOCLOCK:
			return SCR_GetDemoTime();

		case TIMETYPE_CLOCK: default:
			return SCR_GetTime(format);
	}
}

static qbool SCR_BlinkNow(void)
{
	SYSTEMTIME tm;

	GetLocalTime(&tm);
	return tm.wMilliseconds < 500;
}

// ------------------
// draw BIG clock
// style:
//  0 - normal
//  1 - red
void SCR_DrawBigClock(int x, int y, int style, int blink, float scale, const char *t)
{
    extern  mpic_t  *sb_nums[2][11];
    extern  mpic_t  *sb_colon/*, *sb_slash*/;
	qbool lblink = SCR_BlinkNow();

    if (style > 1)  style = 1;
    if (style < 0)  style = 0;

	while (*t)
    {
        if (*t >= '0'  &&  *t <= '9')
        {
			Draw_STransPic(x, y, sb_nums[style][*t-'0'], scale);
            x += 24*scale;
        }
        else if (*t == ':')
        {
            if (lblink || !blink)
				Draw_STransPic (x, y, sb_colon, scale);

			x += 16*scale;
        }
        else
		{
			Draw_SCharacter(x, y, *t+(style?128:0), 3*scale);
            x += 24*scale;
		}
        t++;
    }
}

// ------------------
// draw SMALL clock
// style:
//  0 - small white
//  1 - small red
//  2 - small yellow/white
//  3 - small yellow/red
void SCR_DrawSmallClock(int x, int y, int style, int blink, float scale, const char *t)
{
	qbool lblink = SCR_BlinkNow();
	int c;

    if (style > 3)  style = 3;
    if (style < 0)  style = 0;

    while (*t)
    {
		c = (int) *t;
        if (c >= '0'  &&  c <= '9')
        {
            if (style == 1)
                c += 128;
            else if (style == 2  ||  style == 3)
                c -= 30;
        }
        else if (c == ':')
        {
            if (style == 1  ||  style == 3)
                c += 128;
            if (lblink ||  !blink)
                ;
            else
                c = ' ';
        }
        Draw_SCharacter(x, y, c, scale);
        x+= 8*scale;
        t++;
    }
}

void SCR_DrawFill(int x, int y, int w, int h, int c, float opacity)
{
	Draw_AlphaFill(x, y, w, h, c, opacity);
}

#define SPEED_TAG_LENGTH		2
#define SPEED_OUTLINE_SPACING	SPEED_TAG_LENGTH
#define SPEED_FILL_SPACING		SPEED_OUTLINE_SPACING + 1
#define SPEED_WHITE				10
#define SPEED_TEXT_ONLY			1

#define	SPEED_TEXT_ALIGN_NONE	0
#define SPEED_TEXT_ALIGN_CLOSE	1
#define SPEED_TEXT_ALIGN_CENTER	2
#define SPEED_TEXT_ALIGN_FAR	3

// FIXME: hud-only now, can/should be moved
void SCR_DrawHUDSpeed (
	int x, int y, int width, int height,
	int type,
	float tick_spacing,
	float opacity,
	int vertical,
	int vertical_text,
	int text_align,
	byte color_stopped,
	byte color_normal,
	byte color_fast,
	byte color_fastest,
	byte color_insane,
	int style,
	float scale
)
{
	byte color_offset;
	byte color1, color2;
    int player_speed;
    vec_t *velocity;

    if (scr_con_current == vid.height) {
        return;     // console is full screen
	}

    // Get the velocity.
    if (cl.players[cl.playernum].spectator && Cam_TrackNum() >= 0) {
        velocity = cl.frames[cls.netchan.incoming_sequence & UPDATE_MASK].playerstate[Cam_TrackNum()].velocity;
    }
    else {
        velocity = cl.simvel;
    }

	// Calculate the speed
    if (!type)
	{
		// Based on XY.
        player_speed = sqrt(velocity[0]*velocity[0]
                          + velocity[1]*velocity[1]);
	}
    else
	{
		// Based on XYZ.
        player_speed = sqrt(velocity[0]*velocity[0]
                          + velocity[1]*velocity[1]
                          + velocity[2]*velocity[2]);
	}

	// Calculate the color offset for the "background color".
	if (vertical) {
		color_offset = height * (player_speed % 500) / 500;
	}
	else {
		color_offset = width * (player_speed % 500) / 500;
	}

	// Set the color based on the speed.
    switch (player_speed / 500)
    {
		case 0:
			color1 = color_stopped;
			color2 = color_normal;
			break;
		case 1:
			color1 = color_normal;
			color2 = color_fast;
			break;
		case 2:
			color1 = color_fast;
			color2 = color_fastest;
			break;
		default:
			color1 = color_fastest;
			color2 = color_insane;
			break;
    }

	// Draw tag marks.
	if (tick_spacing > 0.0 && style != SPEED_TEXT_ONLY)
	{
		float f;

		for(f = tick_spacing; f < 1.0; f += tick_spacing)
		{
			if(vertical)
			{
				// Left.
				SCR_DrawFill(x,				// x
					y + (int)(f * height),	// y
					SPEED_TAG_LENGTH,		// Width
					1,						// Height
					SPEED_WHITE,			// Color
					opacity);				// Opacity

				// Right.
				SCR_DrawFill(x + width - SPEED_TAG_LENGTH + 1,
					y + (int)(f * height),
					SPEED_TAG_LENGTH,
					1,
					SPEED_WHITE,
					opacity);
			}
			else
			{
				// Above.
				SCR_DrawFill(x + (int)(f * width),
					y,
					1,
					SPEED_TAG_LENGTH,
					SPEED_WHITE,
					opacity);

				// Below.
				SCR_DrawFill(x + (int)(f * width),
					y + height - SPEED_TAG_LENGTH + 1,
					1,
					SPEED_TAG_LENGTH,
					SPEED_WHITE,
					opacity);
			}
		}
	}

	//
	// Draw outline.
	//
	if (style != SPEED_TEXT_ONLY)
	{
		if(vertical)
		{
			// Left.
			SCR_DrawFill(x + SPEED_OUTLINE_SPACING,
				y,
				1,
				height,
				SPEED_WHITE,
				opacity);

			// Right.
			SCR_DrawFill(x + width - SPEED_OUTLINE_SPACING,
				y,
				1,
				height,
				SPEED_WHITE,
				opacity);
		}
		else
		{
			// Above.
			SCR_DrawFill(x,
				y + SPEED_OUTLINE_SPACING,
				width,
				1,
				SPEED_WHITE,
				opacity);

			// Below.
			SCR_DrawFill(x,
				y + height - SPEED_OUTLINE_SPACING,
				width,
				1,
				SPEED_WHITE,
				opacity);
		}
	}

	//
	// Draw fill.
	//
	if (style != SPEED_TEXT_ONLY)
	{
		if(vertical)
		{
			// Draw the right color (slower).
			SCR_DrawFill (x + SPEED_FILL_SPACING,
				y,
				width - (2 * SPEED_FILL_SPACING),
				height - color_offset,
				color1,
				opacity);

			// Draw the left color (faster).
			SCR_DrawFill (x + SPEED_FILL_SPACING,
				y + height - color_offset,
				width - (2 * SPEED_FILL_SPACING),
				color_offset,
				color2,
				opacity);
		}
		else
		{
			// Draw the right color (slower).
			SCR_DrawFill (x + color_offset,
				y + SPEED_FILL_SPACING,
				width - color_offset,
				height - (2 * SPEED_FILL_SPACING),
				color1,
				opacity);

			// Draw the left color (faster).
			SCR_DrawFill (x,
				y + SPEED_FILL_SPACING,
				color_offset,
				height - (2 * SPEED_FILL_SPACING),
				color2,
				opacity);
		}
	}

	// Draw the speed text.
	if(vertical && vertical_text)
	{
		int i = 1;
		int len = 0;

		// Align the text accordingly.
		switch(text_align)
		{
			case SPEED_TEXT_ALIGN_NONE:		return;
			case SPEED_TEXT_ALIGN_FAR:		y = y + height - 4*8; break;
			case SPEED_TEXT_ALIGN_CENTER:	y = Q_rint(y + height/2.0 - 16); break;
			case SPEED_TEXT_ALIGN_CLOSE:
			default: break;
		}

		len = strlen(va("%d", player_speed));

		// 10^len
		while(len > 0)
		{
			i *= 10;
			len--;
		}

		// Write one number per row.
		for(; i > 1; i /= 10)
		{
			int next;
			next = (i/10);

			// Really make sure we don't try division by zero :)
			if(next <= 0)
			{
				break;
			}

			Draw_SString(Q_rint(x + width/2.0 - 4 * scale), y, va("%1d", (player_speed % i) / next), scale);
			y += 8;
		}
	}
	else
	{
		// Align the text accordingly.
		switch(text_align)
		{
			case SPEED_TEXT_ALIGN_FAR:
				x = x + width - 4 * 8 * scale;
				break;
			case SPEED_TEXT_ALIGN_CENTER:
				x = Q_rint(x + width / 2.0 - 2 * 8 * scale);
				break;
			case SPEED_TEXT_ALIGN_CLOSE:
			case SPEED_TEXT_ALIGN_NONE:
			default: 
				break;
		}

		Draw_SString(x, Q_rint(y + height/2.0 - 4 * scale), va("%4d", player_speed), scale);
	}
}

// ================================================================
// Draws a word wrapped scrolling string inside the given bounds.
// ================================================================
void SCR_DrawWordWrapString(int x, int y, int y_spacing, int width, int height, int wordwrap, int scroll, double scroll_delay, char *txt, float scale)
{
	int wordlen = 0;                         // Length of words.
	int cur_x = -1;                          // the current x position.
	int cur_y = 0;                           // current y position.
	char c;                                  // Current char.
	int width_as_text = width / (8 * scale); // How many chars that fits the given width.
	int height_as_rows = 0;                  // How many rows that fits the given height.

	// Scroll variables.
	double t;                           // Current time.
	static double t_last_scroll = 0;	// Time at the last scroll.
	static int scroll_position = 0; 	// At what index to start printing the string.
	static int scroll_direction = 1;	// The direction the string is scrolling.
	static int last_text_length = 0;	// The last text length.
	int text_length = 0;                // The current text length.

	// Don't print all the char's ontop of each other :)
	if (!y_spacing) {
		y_spacing = 8 * scale;
	}

	height_as_rows = height / y_spacing;

	// Scroll the text.
	if (scroll) {
		text_length = strlen(txt);

		// If the text has changed since last time we scrolled
		// the scroll data will be invalid so reset it.
		if (last_text_length != text_length) {
			scroll_position = 0;
			scroll_direction = 1;
			last_text_length = text_length;
		}

		// Check if the text is longer that can fit inside the given bounds.
		if ((wordwrap && text_length > (width_as_text * height_as_rows)) // For more than one row.
			|| (!wordwrap && text_length > width_as_text)) // One row.
		{
			// Set what position to start printing the string at.
			if ((wordwrap && text_length - scroll_position >= width_as_text * height_as_rows) // More than one row.
				|| (!wordwrap && (text_length - scroll_position) >= width_as_text)) // One row.
			{
				txt += scroll_position;
			}

			// Get the time.
			t = Sys_DoubleTime();

			// Only advance the scroll position every n:th second.
			if((t - t_last_scroll) > scroll_delay) {
				t_last_scroll = t;

				if (scroll_direction) {
					scroll_position++;
				}
				else {
					scroll_position--;
				}
			}

			// Set the scroll direction.
			if ((wordwrap && (text_length - scroll_position) == width_as_text * height_as_rows)
				|| (!wordwrap && (text_length - scroll_position) == width_as_text))
			{
				scroll_direction = 0; // Left
			}
			else if(scroll_position == 0 )
			{
				scroll_direction = 1; // Right
			}
		}
	}

	// Print the string.
	while((c = *txt))
	{
		txt++;

		if (wordwrap == 1)
		{
			// count the word length.
			for (wordlen = 0; wordlen < width_as_text; wordlen++)
			{
				// Make sure the char isn't above 127.
				char ch = txt[wordlen] & 127;
				if ((wordwrap && (!txt[wordlen] || ch == 0x09 || ch == 0x0D ||ch == 0x0A || ch == 0x20)) || (!wordwrap && txt[wordlen] <= ' '))
				{
					break;
				}
			}
		}

		// Wordwrap if the word doesn't fit inside the textbox.
		if (wordwrap == 1 && (wordlen != width_as_text) && ((cur_x + wordlen) >= width_as_text))
		{
			cur_x = 0;
			cur_y += y_spacing;
		}

		switch(c)
		{
			case '\n' :
			case '\r' :
				// Next line for newline and carriage return.
				cur_x = 0;
				cur_y += y_spacing;
				break;
			default :
				// Move the position to draw at forward.
				cur_x++;
				break;
		}

		// Check so that the draw position isn't outside the textbox.
		if (cur_x >= width_as_text) {
			// Don't change row if wordwarp is enabled.
			if(!wordwrap) {
				return;
			}

			cur_x = 0;
			cur_y += y_spacing;
		}

		// Make sure the new line is within the bounds.
		if (cur_y < height) {
			Draw_SCharacter(x + (cur_x) * 8 * scale, y + cur_y, c, scale);
		}
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
		amount = Q_rint(abs((height * value) / max_value));
	else// left-right
		amount = Q_rint(abs((width * value) / max_value));

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

