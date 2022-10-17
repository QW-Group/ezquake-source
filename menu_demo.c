/*
Copyright (C) 2011 ezQuake team

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
	Demo Menu module

	Stands between menu.c and Ctrl_Tab.c

	Naming convention:
	function mask | usage    | purpose
	--------------|----------|-----------------------------
	Menu_Demo_*   | external | interface for menu.c
	CT_Demo_*     | external | interface for Ctrl_Tab.c
	CL_Demo_*_f   | external | interface for cl_demo.c
	M_Demo_*_f    | external | commands issued by the user
	Demo_         | internal | internal (static) functions

	made by:
		johnnycz, Dec 2006
*/

#include "quakedef.h"
#include "gl_model.h"
#include "settings.h"
#include "settings_page.h"
#include "EX_FileList.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "menu.h"
#include "keys.h"

#include "hash.h"
#include "fs.h"
#include "vfs.h"

#ifdef _WIN32
#define DEMO_TIME                 FILETIME
#else
#define DEMO_TIME                 time_t
#endif

//unused
//#define MAX_DEMO_NAME           MAX_OSPATH
//#define MAX_DEMO_FILES          2048
#define DEMO_MAXLINES             17

#define DEMO_PLAYLIST_NAME_MAX    16
#define DEMO_PLAYLIST_OPTIONS_MAX 5
#define	DEMO_PLAYLIST_MAX         256
#define DEMO_PLAYLIST_TAB_MAIN    0

#define DEMO_OPTIONS_MAX          2

#define DEMO_TAB_MAIN             0
#define DEMO_TAB_PLAYLIST         1
#define DEMO_TAB_OPTIONS          2
#define DEMO_TAB_MAX              2

extern int mvd_demo_track_run;

typedef enum
{
    DEMOPG_BROWSER,		// Browse page
	DEMOPG_PLAYLIST,	// Playlist page
	DEMOPG_ENTRY,		// Entry page
	DEMOPG_OPTIONS		// Options page
}	demo_tab_t;

typedef struct demo_playlist_s
{
	char name[MAX_PATH];
	char path[MAX_PATH];
	char trackname[DEMO_PLAYLIST_NAME_MAX];
} demo_playlist_t;

extern cvar_t     scr_scaleMenu;
extern int        menuwidth;
extern int        menuheight;

// Demo browser container
filelist_t demo_filelist;

// Demo menu container
CTab_t demo_tab;

// Playlist structures
demo_playlist_t demo_playlist[DEMO_PLAYLIST_MAX]; // TODO: A play list probably shouldn't be tied to a GUI, put this in cl_demo.c instead and allow adding demos from the console also.

char track_name[DEMO_PLAYLIST_NAME_MAX];
char default_track[DEMO_PLAYLIST_NAME_MAX];
qbool demo_playlist_started = false;

cvar_t    demo_playlist_loop = {"demo_playlist_loop","0"};
cvar_t    demo_playlist_track_name = {"demo_playlist_track_name",""};

// demo playlist options
settings_page demoplsett;
setting demoplsett_arr[] = {
	ADDSET_SEPARATOR("Playlist options"),
	ADDSET_BOOL("looping", demo_playlist_loop),
	ADDSET_STRING("default track", demo_playlist_track_name)
};

static int demo_playlist_base = 0;
static int demo_playlist_current_played = 0;
static int demo_playlist_cursor = 0;
static int demo_playlist_num = 0;
static int demo_playlist_opt_cursor = 0;
static int demo_playlist_started_test = 0;

char demo_track[DEMO_PLAYLIST_NAME_MAX];

// Demo Playlist Functions
static void M_Demo_Playlist_stop_f (void)
{
	if (!demo_playlist_started_test)
	{
		demo_playlist_started = false;
		demo_playlist_started_test = 0;
	}
}

static void Demo_Playlist_Start (int i)
{
	key_dest = key_game;
	m_state = m_none;
	demo_playlist_current_played = i;
	demo_playlist_started_test = 0 ;

	if (cls.demoplayback)
	{
		CL_Disconnect_f();
	}

	demo_playlist_started_test = 0 ;
	demo_playlist_started = true;
	strlcpy(track_name, demo_playlist[demo_playlist_current_played].trackname, sizeof(track_name));
	Cbuf_AddText (va("playdemo \"%s\"\n", demo_playlist[demo_playlist_current_played].path));
}

void CL_Demo_Playlist_f (void)
{
	demo_playlist_current_played++;

	strlcpy (track_name, demo_playlist[demo_playlist_current_played].trackname, sizeof(track_name));

	if (demo_playlist_current_played == demo_playlist_num  && demo_playlist_loop.value )
	{
		demo_playlist_current_played = 0;
		Cbuf_AddText (va("playdemo \"%s\"\n", demo_playlist[demo_playlist_current_played].path));
	}
	else if (demo_playlist_current_played == demo_playlist_num )
	{
		Com_Printf("End of demo playlist.\n");
		demo_playlist_started = false;
		demo_playlist_current_played = 0;
	}
	else
	{
		Cbuf_AddText (va("playdemo \"%s\"\n", demo_playlist[demo_playlist_current_played].path));
	}
}

void M_Demo_Playlist_Next_f (void)
{
	int tmp;

	if (!demo_playlist_started)
	{
		return;
	}

	tmp = demo_playlist_current_played + 1 ;

	if (tmp > demo_playlist_num - 1)
	{
		tmp = 0 ;
	}

	Demo_Playlist_Start(tmp);
}

void M_Demo_Playlist_Prev_f (void)
{
	int tmp;
	if (!demo_playlist_started)
	{
		return;
	}

	tmp = demo_playlist_current_played - 1 ;

	if (tmp < 0)
	{
		tmp = demo_playlist_num - 1 ;
	}

	Com_Printf("Prev %i\n", tmp);
	Demo_Playlist_Start(tmp);
}

void M_Demo_Playlist_Clear_f (void)
{
	if (demo_playlist_num == 0)
	{
		return;
	}

	memset (&demo_playlist, 0, sizeof(demo_playlist_t) * demo_playlist_num);

	demo_playlist_num = 0;
	demo_playlist_started = false;
}

void M_Demo_Playlist_Stop_f (void)
{
	M_Demo_Playlist_stop_f();
	Cbuf_AddText("disconnect\n");
}

static void Demo_Playlist_Setup_f (void)
{
	strlcpy (demo_track, demo_playlist[demo_playlist_cursor + demo_playlist_base].trackname, sizeof(demo_track));
	strlcpy (default_track, demo_playlist_track_name.string, min(16, sizeof(default_track)));
}

//
// Delete the current entry from the playlist.
//
static void Demo_Playlist_Del(int i)
{
	int y;

	if (i >= DEMO_PLAYLIST_MAX) {
		Com_Printf("Error: demo playlist item %d out of range (%d)\n", i, DEMO_PLAYLIST_MAX);
		return;
	}

	// Remove the playlist item.
	memset (&demo_playlist[i], 0, sizeof(demo_playlist[i]));

	for (y = i; y < (DEMO_PLAYLIST_MAX - 1) && demo_playlist[y+1].name[0] != '\0'; y++ )
	{
		memmove (&demo_playlist[y], &demo_playlist[y+1], sizeof(demo_playlist[y]));
		memset (&demo_playlist[y+1], 0, sizeof(demo_playlist[y+1]));
	}

	demo_playlist_num--;
	demo_playlist_num = max(0, demo_playlist_num);

	demo_playlist_cursor = bound(0, demo_playlist_cursor, demo_playlist_num - 1);
}

//
// Move the selected entry up a step in the playlist.
//
static void Demo_Playlist_Move_Up (int i)
{
	demo_playlist_t tmp;

	if (i == 0)
	{
		return;
	}

	memcpy (&tmp, &demo_playlist[i-1], sizeof(tmp));
	memcpy (&demo_playlist[i-1], &demo_playlist[i], sizeof(demo_playlist[i-1]));
	memcpy (&demo_playlist[i], &tmp, sizeof(demo_playlist[i]));
}

//
// Move the selected entry down a step in the playlist.
//
static void Demo_Playlist_Move_Down (int i)
{
	demo_playlist_t tmp;

	if(i + 1 == demo_playlist_num)
	{
		return;
	}

	memcpy (&tmp, &demo_playlist[i+1], sizeof(tmp));
	memcpy (&demo_playlist[i+1], &demo_playlist[i], sizeof(demo_playlist[i+1]));
	memcpy (&demo_playlist[i], &tmp, sizeof(demo_playlist[i]));
}

//
// Select the previous entry in the playlist.
//
static void Demo_Playlist_SelectPrev(void)
{
	demo_playlist_cursor -= DEMO_MAXLINES - 1;
	if (demo_playlist_cursor < 0)
	{
		demo_playlist_base += demo_playlist_cursor;
		demo_playlist_base = max (0, demo_playlist_base);
		demo_playlist_cursor = 0;
	}

	Demo_Playlist_Setup_f();
}

static void Demo_Playlist_SelectNext(void)
{
	demo_playlist_cursor += DEMO_MAXLINES - 1;

	if (demo_playlist_base + demo_playlist_cursor >= demo_playlist_num)
	{
		demo_playlist_cursor = demo_playlist_num - demo_playlist_base - 1;
	}

	if (demo_playlist_cursor >= DEMO_MAXLINES)
	{
		demo_playlist_base += demo_playlist_cursor - (DEMO_MAXLINES - 1);
		demo_playlist_cursor = DEMO_MAXLINES - 1;

		if (demo_playlist_base + demo_playlist_cursor >= demo_playlist_num)
		{
			demo_playlist_base = demo_playlist_num - demo_playlist_cursor - 1;
		}
	}

	Demo_Playlist_Setup_f();
}

/*
static void Demo_FormatSize (char *t) {
	char *s;

	for (s = t; *s; s++) {
		if (*s >= '0' && *s <= '9')
			*s = *s - '0' + 18;
		else
			*s |= 128;
	}
}
*/

// ============
// <draw pages>

void CT_Demo_Browser_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
    FL_Draw(&demo_filelist, x, y, w, h);
}


void CT_Demo_Playlist_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	int i, y2;

	if(demo_playlist_num == 0)
	{
		UI_Print_Center(x, y + 16, w, "Playlist is empty", false);
		UI_Print_Center(x, y + 32, w, "Use [Insert] or [Ctrl]+[Enter]", true);
		UI_Print_Center(x, y + 40, w, "in the demo browser to add a demo", true);
		// UI_Print_Center(x, y + 40, w, "to add demo to the playlist", true);
	}
	else
	{
		y = y - 48;
		for (i = 0; i <= demo_playlist_num - demo_playlist_base && i < DEMO_MAXLINES; i++)
		{
			y2 = 32 + i * 8 ;

			if (demo_playlist_cursor == i)
				M_Print (24, y + y2, demo_playlist[demo_playlist_base + i].name);
			else
				M_PrintWhite (24, y + y2, demo_playlist[demo_playlist_base + i].name);
		}

		M_DrawCharacter (8, y + 32 + demo_playlist_cursor * 8, FLASHINGARROW());
	}
}

void CT_Demo_Entry_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	int z;
	y = y - 32;

	if (demo_playlist_started && cls.demoplayback)
	{
		M_Print (24, y + 32, "Currently playing:");
		M_PrintWhite (24, y + 40, demo_playlist[demo_playlist_current_played].name);
	}
	else
	{
		M_Print (24, y + 32, "Not playing anything");
	}

	M_Print (24, y + 56, "Next     demo");
	M_Print (24, y + 64, "Previous demo");
	M_Print (24, y + 72, "Stop  playlist");
	M_Print (24, y + 80, "Clear playlist");

	if (demo_playlist_num > 0)
	{
		M_Print (24, y + 96, "Currently selected:");
		M_Print (24, y + 104, demo_playlist[demo_playlist_cursor].name);
	}
	else
	{
		M_Print (24, y + 96, "No demo in playlist");
	}

	if (strcasecmp(demo_playlist[demo_playlist_cursor].name + strlen(demo_playlist[demo_playlist_cursor].name) - 4, ".mvd"))
	{
		M_Print (24, y + 120, "Tracking only available with mvds");
	}
	else
	{
		M_Print (24, y + 120, "Track");
		M_DrawTextBox (160, y + 112, 16, 1);
		M_PrintWhite (168, y + 120, demo_track);
		if (demo_playlist_opt_cursor == 4 && demo_playlist_num > 0)
			M_DrawCharacter (168 + 8*strlen(demo_track), 120 + y, FLASHINGCURSOR());
	}

	z = demo_playlist_opt_cursor + (demo_playlist_opt_cursor >= 4 ? 4 : 0);
	z = y + 56 + z * 8;
	M_DrawCharacter (8, z, FLASHINGARROW());
}

void CT_Demo_Options_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
	Settings_Draw(x, y, w, h, &demoplsett); 
}

#define DEMOPAGEPADDING 4

// in the end leads calls one of the four functions above
void Menu_Demo_Draw (void)
{
	extern void Demo_Draw(int, int, int, int);

	int x, y, w, h;

	M_Unscale_Menu();

    // don't add padding on the right side so the scrolling is friendly
	w = vid.width - DEMOPAGEPADDING; // here used to be a limit to 512x... size, we've considered it useless
	h = vid.height - DEMOPAGEPADDING*2;
	x = DEMOPAGEPADDING;
	y = DEMOPAGEPADDING;

	CTab_Draw(&demo_tab, x, y, w, h);
}
// </draw pages>
// =============

static void Demo_AddDemoToPlaylist(const char* display_name, const char* path)
{
	if (demo_playlist_num >= DEMO_PLAYLIST_MAX) {
		Com_Printf("Playlist is full, cannot add \"%s\" to it. Max allowed demos in playlist is %d\n", display_name, DEMO_PLAYLIST_MAX);
		return;
	}

	snprintf(demo_playlist[demo_playlist_num].name, sizeof((*demo_playlist).name), "%s", display_name);
	snprintf(demo_playlist[demo_playlist_num].path, sizeof((*demo_playlist).path), "%s", path);
	demo_playlist_num++;
}

void Demo_AddDirToPlaylist (char *dir_path)
{
	extern void FL_ReadDir(filelist_t *fl);
	int i;
	filelist_t dir_filelist;

	if (!dir_path) {
		return;
	}

	// Bit of a hack, but we need values for cvars and what not for the FL_ functions to work
	// so borrow them from demo_filelist.
	memcpy (&dir_filelist, &demo_filelist, sizeof(dir_filelist));

	FL_SetCurrentDir (&dir_filelist, dir_path);
	FL_ReadDir (&dir_filelist);

	// Find the demos and add them to the playlist.
	for (i = 0; i < dir_filelist.num_entries; i++)
	{
		filedesc_t *f = &dir_filelist.entries[i];

		// Don't bother with zips and directories.
		if (f->is_directory
#ifdef WITH_ZIP
			|| f->is_archive
#endif
			)
		{
			// TODO: Make this recursive for dirs?
			continue;
		}

		Demo_AddDemoToPlaylist (f->display, f->name);
	}
}

#ifdef WITH_ZIP
void Demo_AddZipToPlaylist(const char *zip_path)
{
	unz_global_info global_info;
	unzFile         zip_file;
	char            filename[MAX_PATH_LENGTH];
	unz_file_info   file_info;
	char            full_queued_path[MAX_PATH_LENGTH];

	// Unpack the files to a temp path.
	zip_file = FS_ZipUnpackOpenFile(zip_path);

	// Get the number of files in the zip archive.
	if (unzGetGlobalInfo(zip_file, &global_info) == UNZ_OK) {
		int i;

		for (i = 0; i < global_info.number_entry; ++i) {
			if (i && unzGoToNextFile(zip_file) != UNZ_OK) {
				break;
			}

			if (unzGetCurrentFileInfo(zip_file, &file_info, filename, sizeof(filename), NULL, 0, NULL, 0) != UNZ_OK) {
				break;
			}

			if (!CL_DemoExtensionMatch(filename)) {
				continue;
			}

			strlcpy(full_queued_path, zip_path, sizeof(full_queued_path));
			strlcat(full_queued_path, "/", sizeof(full_queued_path));
			strlcat(full_queued_path, filename, sizeof(full_queued_path));
			Demo_AddDemoToPlaylist(COM_SkipPath(filename), filename);
		}
	}
}
#endif // WITH_ZIP

// ==============================
// <key processing for each page>

int CT_Demo_Browser_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
	extern void M_ToggleMenu_f (void);
	extern void M_LeaveMenu (int);
    qbool processed = false;

	// Special case for adding zips/dirs to playlist.
	if (key == K_INS || (key == K_ENTER && keydown[K_CTRL]))
	{
		#ifdef WITH_ZIP
		if (FS_IsArchive (FL_GetCurrentPath(&demo_filelist)))
		{
			// Zip.
			Demo_AddZipToPlaylist (FL_GetCurrentPath(&demo_filelist));
			return true;
		}
		else
		#endif // WITH_ZIP
		{
			if (FL_IsCurrentDir (&demo_filelist))
			{
				// Dir.
				Demo_AddDirToPlaylist (FL_GetCurrentPath(&demo_filelist));
				return true;
			}
		}
	}

	// See if the main filebrowser functions wants to
	// do something first, like enter a dir/zip.
    processed = FL_Key(&demo_filelist, key);

    if (!processed)
    {
		if (key == K_INS)
		{
			// Add the selected demo to the playlist.
			Demo_AddDemoToPlaylist (FL_GetCurrentDisplay (&demo_filelist), FL_GetCurrentPath (&demo_filelist));
		}
        else if (key == K_ENTER || key == K_MOUSE1)
        {
			if (keydown[K_CTRL])
			{
				// Add the selected demo to the playlist.
				Demo_AddDemoToPlaylist (FL_GetCurrentDisplay (&demo_filelist), FL_GetCurrentPath (&demo_filelist));
			}
			else if (keydown[K_SHIFT])
			{
				M_LeaveMenus();
				Cbuf_AddText (va("timedemo \"%s\"\n", FL_GetCurrentPath(&demo_filelist)));
			}
			else
			{
				M_LeaveMenus();
				Cbuf_AddText(va("playdemo \"%s\"\n", FL_GetCurrentPath(&demo_filelist)));
				processed = true;
			}
        }
    }

    return processed;
}

int CT_Demo_Playlist_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
	switch (key)
	{
		case K_UPARROW:
		case K_MWHEELUP:
		{
			if (keydown[K_CTRL] && demo_playlist_cursor + demo_playlist_base > 0)
				Demo_Playlist_Move_Up(demo_playlist_cursor + demo_playlist_base);

			if (demo_playlist_cursor > 0)
			{
				demo_playlist_cursor--;
			}
			else if (demo_playlist_base > 0)
			{
				demo_playlist_base--;
			}

			Demo_Playlist_Setup_f();
			break;
		}
		case K_DOWNARROW:
		case K_MWHEELDOWN:
		{
			if (keydown[K_CTRL] && demo_playlist_cursor + demo_playlist_base < demo_playlist_num)
				Demo_Playlist_Move_Down(demo_playlist_cursor + demo_playlist_base);

			if (demo_playlist_cursor + demo_playlist_base < demo_playlist_num - 1)
			{
				if (demo_playlist_cursor < DEMO_MAXLINES - 1)
					demo_playlist_cursor++;
				else
					demo_playlist_base++;
			}
			Demo_Playlist_Setup_f();
			break;
		}
		case K_HOME:
		{
			demo_playlist_cursor = 0;
			demo_playlist_base = 0;
			Demo_Playlist_Setup_f();
			break;
		}
		case K_END:
		{
			if (demo_playlist_num > DEMO_PLAYLIST_OPTIONS_MAX)
			{
				demo_playlist_cursor = DEMO_PLAYLIST_OPTIONS_MAX - 1;
				demo_playlist_base = demo_playlist_num - demo_playlist_cursor - 1;
			}
			else
			{
				demo_playlist_base = 0;
				demo_playlist_cursor = demo_playlist_num - 1;
			}
			Demo_Playlist_Setup_f();
			break;
		}
		case K_PGUP:
		{
			Demo_Playlist_SelectPrev();
			break;
		}
		case K_PGDN:
		{
			Demo_Playlist_SelectNext();
			break;
		}
		case K_ENTER:
		{
			Demo_Playlist_Start(demo_playlist_cursor + demo_playlist_base);
			break;
		}
		case K_DEL:
		{
			Demo_Playlist_Del(demo_playlist_cursor + demo_playlist_base);
			break;
		}
		default:
			return false;
	}

	return true;
}

int CT_Demo_Entry_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
	int l;

	switch (key)
	{
		case K_LEFTARROW: return false;
		case K_RIGHTARROW: return false;
		case K_UPARROW:
		case K_MWHEELUP:
		{
			demo_playlist_opt_cursor = demo_playlist_opt_cursor ? demo_playlist_opt_cursor - 1 : DEMO_PLAYLIST_OPTIONS_MAX - 1;
			break;
		}
		case K_DOWNARROW:
		case K_MWHEELDOWN:
		{
			demo_playlist_opt_cursor++;
			demo_playlist_opt_cursor = demo_playlist_opt_cursor % DEMO_PLAYLIST_OPTIONS_MAX;
			break;
		}
		case K_PGUP:
		{
			Demo_Playlist_SelectPrev();
			break;
		}
		case K_PGDN:
		{
			Demo_Playlist_SelectNext();
			break;
		}
		case K_ENTER:
		{
			if (demo_playlist_opt_cursor == 0)
				M_Demo_Playlist_Next_f();
			else if (demo_playlist_opt_cursor == 1)
				M_Demo_Playlist_Prev_f();
			else if (demo_playlist_opt_cursor == 2)
				M_Demo_Playlist_Stop_f();
			else if (demo_playlist_opt_cursor == 3)
				M_Demo_Playlist_Clear_f();
			break;
		}
		case K_BACKSPACE:
		{
			if (demo_playlist_opt_cursor == 4)
			{
				if (strlen(demo_track))
					demo_track[strlen(demo_track)-1] = 0;
				strlcpy(demo_playlist[demo_playlist_cursor + demo_playlist_base].trackname,demo_track,sizeof(demo_playlist->trackname));
			}
			break;
		}
		default:
		{
			if (key < 32 || key > 127)
				return false;

			if (demo_playlist_opt_cursor == 4)
			{
				l = strlen(demo_track);
				if (l < 15)
				{
					demo_track[l+1] = 0;
					demo_track[l] = key;
					strlcpy(demo_playlist[demo_playlist_cursor + demo_playlist_base].trackname,demo_track,sizeof(demo_playlist->trackname));
				}
				return true;
			}
			else
			{
				return false;
			}
		}
	}

	return true;
}

int CT_Demo_Options_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
	return Settings_Key(&demoplsett, key, unichar);
}

qbool CT_Demo_Browser_Mouse_Event(const mouse_state_t *ms)
{
    if (FL_Mouse_Event(&demo_filelist, ms)) {
        return true;
    } else if (ms->button_up >= 1 && ms->button_up <= 2) {
        CT_Demo_Browser_Key(K_MOUSE1 - 1 + ms->button_up, 0, &demo_tab, demo_tab.pages + DEMOPG_BROWSER);
        return true;
    }

    // this specially "eats" button_up event, there is no reason to process other events anyway
	return true;
}

qbool CT_Demo_Options_Mouse_Event(const mouse_state_t *ms)
{
	return Settings_Mouse_Event(&demoplsett, ms);
}

// will lead to call of one of the 4 functions above
void Menu_Demo_Key(int key, wchar unichar)
{
	extern void M_Menu_Main_f (void);

    int handled = CTab_Key(&demo_tab, key, unichar);

    if (!handled)
    {
		if (key == K_ESCAPE || key == K_MOUSE2)
        {
            M_Menu_Main_f();
        }
    }
}
// </key processing for each page>

qbool Menu_Demo_Mouse_Event(const mouse_state_t *ms)
{
	mouse_state_t nms = *ms;

    if (ms->button_up == 2) {
        Menu_Demo_Key(K_MOUSE2, 0);
        return true;
    }

	nms.x -= DEMOPAGEPADDING;
	nms.y -= DEMOPAGEPADDING;
	nms.x_old -= DEMOPAGEPADDING;
	nms.y_old -= DEMOPAGEPADDING;
	return CTab_Mouse_Event(&demo_tab, &nms);
}

CTabPage_Handlers_t demo_browser_handlers = {
	CT_Demo_Browser_Draw,
	CT_Demo_Browser_Key,
	NULL,
	CT_Demo_Browser_Mouse_Event
};

CTabPage_Handlers_t demo_playlist_handlers = {
	CT_Demo_Playlist_Draw,
	CT_Demo_Playlist_Key
};

CTabPage_Handlers_t demo_entry_handlers = {
	CT_Demo_Entry_Draw,
	CT_Demo_Entry_Key
};

CTabPage_Handlers_t demo_options_handlers = {   
	CT_Demo_Options_Draw,
	CT_Demo_Options_Key,
	NULL,
	CT_Demo_Options_Mouse_Event
};

// set new initial dir
void Menu_Demo_NewHome(const char *homedir)
{
	FL_SetCurrentDir(&demo_filelist, homedir);
}

void Menu_Demo_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cvar_Register (&demo_playlist_loop);
	Cvar_Register (&demo_playlist_track_name);
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("demo_playlist_stop", M_Demo_Playlist_Stop_f);
	Cmd_AddCommand ("demo_playlist_next", M_Demo_Playlist_Next_f);
	Cmd_AddCommand ("demo_playlist_prev", M_Demo_Playlist_Prev_f);
	Cmd_AddCommand ("demo_playlist_clear", M_Demo_Playlist_Clear_f);

	FL_Init(&demo_filelist, "./qw");
    FL_AddFileType(&demo_filelist, 0, ".qwd");
	FL_AddFileType(&demo_filelist, 1, ".qwz");
	FL_AddFileType(&demo_filelist, 2, ".mvd");
	FL_AddFileType(&demo_filelist, 3, ".dem");
	#ifdef WITH_ZLIB
	FL_AddFileType(&demo_filelist, 4, ".gz");
	#endif // WITH_ZLIB
	#ifdef WITH_ZIP
	FL_AddFileType(&demo_filelist, 4, ".zip");
	FL_AddFileType(&demo_filelist, 4, ".pk3");
	#endif // WITH_ZIP

	Settings_Page_Init(demoplsett, demoplsett_arr);

	// initialize tab control
    CTab_Init(&demo_tab);
	CTab_AddPage(&demo_tab, "Browser", DEMOPG_BROWSER, &demo_browser_handlers);
	CTab_AddPage(&demo_tab, "Playlist", DEMOPG_PLAYLIST, &demo_playlist_handlers);
	CTab_AddPage(&demo_tab, "Entry", DEMOPG_ENTRY, &demo_entry_handlers);
	CTab_AddPage(&demo_tab, "Options", DEMOPG_OPTIONS, &demo_options_handlers);
	CTab_SetCurrentId(&demo_tab, DEMOPG_BROWSER);
}

void Menu_Demo_Shutdown(void)
{
	FL_Shutdown(&demo_filelist);
	Settings_Shutdown(&demoplsett);
}

void CL_Demo_NextInPlaylist(void)
{
	if (demo_playlist_started) {
		CL_Demo_Playlist_f();
		mvd_demo_track_run = 0;
	}
}

void CL_Demo_Disconnected(void)
{
	demo_playlist_started = false;
	mvd_demo_track_run = 0;
}
