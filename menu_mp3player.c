//=============================================================================
// MP3 PLAYER MENU
//=============================================================================


#include "quakedef.h"
#include "menu.h"
#include "mp3_player.h"
#include "utils.h"


#ifdef WITH_MP3_PLAYER

#define M_MP3_CONTROL_HEADINGROW    8
#define M_MP3_CONTROL_MENUROW        (M_MP3_CONTROL_HEADINGROW + 56)
#define M_MP3_CONTROL_COL            104
#define M_MP3_CONTROL_NUMENTRIES    12
#define M_MP3_CONTROL_BARHEIGHT        (200 - 24)

static int mp3_cursor = 0;
static int last_status;

void MP3_Menu_DrawInfo(void);
void M_Menu_MP3_Playlist_f(void);


void M_Menu_MP3_Control_Draw (void) {
	char songinfo_scroll[38 + 1], *s = NULL;
	int i, scroll_index, print_time;
	float frac, elapsed, realtime;

	static char lastsonginfo[128], last_title[128];
	static float initial_time;
	static int last_length, last_elapsed, last_total, last_shuffle, last_repeat;

	s = va("%s CONTROL", mp3_player->PlayerName_AllCaps);
	M_Print ((320 - 8 * strlen(s)) >> 1, M_MP3_CONTROL_HEADINGROW, s);

	M_Print (8, M_MP3_CONTROL_HEADINGROW + 16, "\x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");


	if (!MP3_IsActive()) {
		s = va("%s LIBRARIES NOT FOUND", mp3_player->PlayerName_AllCaps);
		M_PrintWhite((320 - 24 * 8) >> 1, M_MP3_CONTROL_HEADINGROW + 40, s);
		return;
	}

	realtime = Sys_DoubleTime();

	last_status = MP3_GetStatus();

	if (last_status == MP3_PLAYING)
		M_PrintWhite(312 - 7 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Playing");
	else if (last_status == MP3_PAUSED)
		M_PrintWhite(312 - 6 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Paused");
	else if (last_status == MP3_STOPPED)
		M_PrintWhite(312 - 7 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Stopped");
	else
		M_PrintWhite(312 - 11 * 8, M_MP3_CONTROL_HEADINGROW + 8, "Not Running");

	if (last_status == MP3_NOTRUNNING) {
		s = va("%s is not running", mp3_player->PlayerName_LeadingCaps);
		M_Print ((320 - 8 * strlen(s)) >> 1, 40, s);
		M_PrintWhite (56, 72, "Press");
		M_Print (56 + 48, 72, "ENTER");
		s = va("to start %s", mp3_player->PlayerName_NoCaps);
		M_PrintWhite (56 + 48 + 48, 72, s);
		M_PrintWhite (56, 84, "Press");
		M_Print (56 + 48, 84, "ESC");
		M_PrintWhite (56 + 48 + 32, 84, "to exit this menu");
		// FIXME: This warning is pointless on linux... but is useful on windows
		//M_Print (16, 116, "The variable");
		//M_PrintWhite (16 + 104, 116, mp3_dir.name);
		//M_Print (16 + 104 + 8 * (strlen(mp3_dir.name) + 1), 116, "needs to");
		//s = va("be set to the path for %s first", mp3_player->PlayerName_NoCaps);
		//M_Print (20, 124, s);
		return;
	}

	s = MP3_Menu_SongtTitle();
	if (!strcmp(last_title, s = MP3_Menu_SongtTitle())) {
		elapsed = 3.5 * max(realtime - initial_time - 0.75, 0);
		scroll_index = (int) elapsed;
		frac = bound(0, elapsed - scroll_index, 1);
		scroll_index = scroll_index % last_length;
	} else {
		snprintf(lastsonginfo, sizeof(lastsonginfo), "%s  ***  ", s);
		strlcpy(last_title, s, sizeof(last_title));
		last_length = strlen(lastsonginfo);
		initial_time = realtime;
		frac = scroll_index = 0;
	}

	if ((!mp3_scrolltitle.value || last_length <= 38 + 7) && mp3_scrolltitle.value != 2) {
		char name[38 + 1];
		strlcpy(name, last_title, sizeof(name));
		M_PrintWhite(max(8, (320 - (last_length - 7) * 8) >> 1), M_MP3_CONTROL_HEADINGROW + 32, name);
		initial_time = realtime;
	} else {
		for (i = 0; i < sizeof(songinfo_scroll) - 1; i++)
			songinfo_scroll[i] = lastsonginfo[(scroll_index + i) % last_length];
		songinfo_scroll[sizeof(songinfo_scroll) - 1] = 0;
		M_PrintWhite(12 -  (int) (8 * frac), M_MP3_CONTROL_HEADINGROW + 32, songinfo_scroll);
	}

	if (mp3_showtime.value) {
		MP3_GetOutputtime(&last_elapsed, &last_total);
		if (last_total == -1)
			goto menu_items;

		print_time = (mp3_showtime.value == 2) ? last_total - last_elapsed : last_elapsed;
		M_PrintWhite(8, M_MP3_CONTROL_HEADINGROW + 8, SecondsToMinutesString(print_time));

		if (mp3_showtime.value != 2)
			M_PrintWhite(48, M_MP3_CONTROL_HEADINGROW + 8, va("/%s", SecondsToMinutesString(last_total)));
	}
menu_items:
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW, "Play");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 8, last_status == MP3_PAUSED ? "Unpause" : "Pause");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 16, "Stop");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 24, "Next Track");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 32, "Prev Track");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 40, "Fast Forwd");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 48, "Rewind");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 56, "Volume");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 64, "Shuffle");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 72, "Repeat");
	M_Print (M_MP3_CONTROL_COL, M_MP3_CONTROL_MENUROW + 88, "View Playlist");

	M_DrawCharacter (M_MP3_CONTROL_COL - 8, M_MP3_CONTROL_MENUROW + mp3_cursor * 8, FLASHINGARROW());

	M_DrawSlider(M_MP3_CONTROL_COL + 96, M_MP3_CONTROL_MENUROW + 56, bound(0, Media_GetVolume(), 1));

	MP3_GetToggleState(&last_shuffle, &last_repeat);
	M_PrintWhite (M_MP3_CONTROL_COL + 88, M_MP3_CONTROL_MENUROW + 64, last_shuffle ? "On" : "Off");
	M_PrintWhite (M_MP3_CONTROL_COL + 88, M_MP3_CONTROL_MENUROW + 72, last_repeat ? "On" : "Off");

	M_DrawCharacter (16, M_MP3_CONTROL_BARHEIGHT, 128);
	for (i = 0; i < 35; i++)
		M_DrawCharacter (24 + i * 8, M_MP3_CONTROL_BARHEIGHT, 129);
	M_DrawCharacter (320 - 16, M_MP3_CONTROL_BARHEIGHT, 130);
	M_DrawCharacter (17 + 286 * ((float) last_elapsed / last_total), M_MP3_CONTROL_BARHEIGHT, 131);
}

qbool M_Menu_MP3_Control_Mouse_Event(const mouse_state_t *ms)
{
	if (ms->button_up == 2)
		M_LeaveMenu(m_main);

	return true;
}

qbool M_Menu_MP3_Playlist_Mouse_Event(const mouse_state_t *ms)
{
	if (ms->button_up == 2)
		M_LeaveMenu(m_main);

	return true;
}

void M_Menu_MP3_Control_Key(int key) {
	float volume;
	extern int m_topmenu;

	if (!MP3_IsActive() || last_status == MP3_NOTRUNNING) {
		switch(key) {
			case K_BACKSPACE:
				m_topmenu = m_none;
			case K_MOUSE2:
			case K_ESCAPE:
				M_LeaveMenu (m_main);
				break;

			case '`':
			case '~':
				key_dest = key_console;
				m_state = m_none;
				break;

			case K_MOUSE1:
			case K_ENTER:
				if (MP3_IsActive())
					MP3_Execute_f();
				break;
		}
		return;
	}

	con_suppress = true;
	switch (key) {
		case K_BACKSPACE:
			m_topmenu = m_none;
		case K_MOUSE2:
		case K_ESCAPE:
			M_LeaveMenu (m_main);
			break;
		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;
		case K_HOME:
		case K_PGUP:
			mp3_cursor = 0;
			break;
		case K_END:
		case K_PGDN:
			mp3_cursor = M_MP3_CONTROL_NUMENTRIES - 1;
			break;
		case K_DOWNARROW:
		case K_MWHEELDOWN:
			if (mp3_cursor == M_MP3_CONTROL_NUMENTRIES - 1) {
				mp3_cursor = 0;
			} else {
				if (mp3_cursor < M_MP3_CONTROL_NUMENTRIES - 1)
					mp3_cursor++;
				if (mp3_cursor == M_MP3_CONTROL_NUMENTRIES - 2)
					mp3_cursor++;
			}
			break;
		case K_UPARROW:
		case K_MWHEELUP:
			if (mp3_cursor == 0) {
				mp3_cursor = M_MP3_CONTROL_NUMENTRIES - 1;
			} else {
				if (mp3_cursor > 0)
					mp3_cursor--;
				if (mp3_cursor == M_MP3_CONTROL_NUMENTRIES - 2)
					mp3_cursor--;
			}
			break;
		case K_MOUSE1:
		case K_ENTER:
			switch (mp3_cursor) {
				case 0:  MP3_Play_f(); break;
				case 1:  MP3_Pause_f(); break;
				case 2:  MP3_Stop_f(); break;
				case 3:  MP3_Next_f(); break;
				case 4:  MP3_Prev_f(); break;
				case 5:  MP3_FastForward_f(); break;
				case 6:  MP3_Rewind_f(); break;
				case 7:  break;
				case 8:  MP3_ToggleShuffle_f(); break;
				case 9:  MP3_ToggleRepeat_f(); break;
				case 10: break;
				case 11: M_Menu_MP3_Playlist_f(); break;
			}
			break;
		case K_RIGHTARROW:
			switch(mp3_cursor) {
				case 7:
					volume = bound(0, Media_GetVolume(), 1);
					Media_SetVolume(volume + 0.02);
					break;
				default:
					MP3_FastForward_f();
					break;
			}
			break;
		case K_LEFTARROW:
			switch(mp3_cursor) {
				case 7:
					volume = bound(0, Media_GetVolume(), 1);
					Media_SetVolume(volume - 0.02);
					break;
				default:
					MP3_Rewind_f();
					break;
			}
			break;
		case 'r': MP3_ToggleRepeat_f(); break;
		case 's': MP3_ToggleShuffle_f(); break;
		case KP_LEFTARROW: case 'z': MP3_Prev_f(); break;
		case KP_5: case 'x': MP3_Play_f(); break;
		case 'c': MP3_Pause_f(); break;
		case 'v': MP3_Stop_f();     break;
		case 'V': MP3_FadeOut_f();    break;
		case KP_RIGHTARROW: case 'b': MP3_Next_f(); break;
		case KP_HOME: MP3_Rewind_f(); break;
		case KP_PGUP: MP3_FastForward_f(); break;
	}
	con_suppress = false;
}

void M_Menu_MP3_Control_f (void){
	M_EnterMenu (m_mp3_control);
}

#define PLAYLIST_MAXENTRIES        2048
#define PLAYLIST_MAXLINES        17
#define PLAYLIST_HEADING_ROW    8

#define PLAYLIST_MAXTITLE    (32 + 1)

static int playlist_size = 0;
static int playlist_cursor = 0, playlist_base = 0;
static char playlist_entries[PLAYLIST_MAXLINES][MP3_MAXSONGTITLE];


void M_Menu_MP3_Playlist_MoveBase(int absolute);
void M_Menu_MP3_Playlist_MoveCursor(int absolute);
void M_Menu_MP3_Playlist_MoveBaseRel(int offset);
void M_Menu_MP3_Playlist_MoveCursorRel(int offset);

/**
 * Center the playlist cursor on the current song
 */
static void Center_Playlist(void) {
	int current;
	int curosor_position;

	MP3_GetPlaylistInfo(&current, NULL);
	M_Menu_MP3_Playlist_MoveBase(current);

	/* Calculate the corect cursor position */
	/* We can display all songs so its just the current index */
	if (current < PLAYLIST_MAXLINES)
		curosor_position = current;
	/* We are at the end of the playlist */
	else if (playlist_size - (current) < PLAYLIST_MAXLINES)
		curosor_position = PLAYLIST_MAXLINES - (playlist_size - (current));
	/* We are in the middle of the playlist & the bottom is the current song*/
	else 
		curosor_position = 0;
	M_Menu_MP3_Playlist_MoveCursor(curosor_position);
}



/**
 * Move the cursor to an absolute position
 */
void M_Menu_MP3_Playlist_MoveCursor(int absolute) {
	playlist_cursor = bound(0, absolute, 
								min((PLAYLIST_MAXLINES - 1), playlist_size));
}

/**
 * Move the base of the playlist to an absolute position in the playlist
 */
void M_Menu_MP3_Playlist_MoveBase(int absolute) {
	int i;

	MP3_GetPlaylistInfo(NULL, &playlist_size);
	playlist_base = bound(0, absolute, playlist_size-PLAYLIST_MAXLINES);

	for (i = playlist_base; i < min(playlist_size, PLAYLIST_MAXLINES+playlist_base); i++) {
		MP3_GetSongTitle(i+1, playlist_entries[i%PLAYLIST_MAXLINES], sizeof(*playlist_entries));
	}

}


/**
 * Move the cursor by an offset relative to the current position
 */
void M_Menu_MP3_Playlist_MoveCursorRel(int offset) {
	/* Already at the bottom */
	
	// jump to the top if we use downarrow/downscroll
	if ( (offset == 1) && (playlist_base + playlist_cursor == playlist_size - 1) )
	{
		M_Menu_MP3_Playlist_MoveCursor(0);
		M_Menu_MP3_Playlist_MoveBase(0);
		return;
	}

	if (playlist_base + playlist_cursor + offset > playlist_size - 1) {
		// Already at the top
	} else if (playlist_cursor + offset > PLAYLIST_MAXLINES - 1) {
		/* Need to scroll down */
		M_Menu_MP3_Playlist_MoveBaseRel(offset);
		playlist_cursor = PLAYLIST_MAXLINES - 1;
	} else if (playlist_cursor + offset < 0) {
		/* Need to scroll up*/
		
		// jump to the bottom if we use uparrow/upscroll
		if ( (offset == -1) && (playlist_cursor == 0) && (playlist_base == 0) ){
			M_Menu_MP3_Playlist_MoveCursor(playlist_size-1);
			M_Menu_MP3_Playlist_MoveBase(playlist_size-1);
			return;
		}

		M_Menu_MP3_Playlist_MoveBaseRel(offset);
		playlist_cursor = 0;
	} else {
		playlist_cursor += offset;
	}
}

/**
 * Move the base of the playlist relative to the current position
 */
void M_Menu_MP3_Playlist_MoveBaseRel(int offset) {
	int i;

	/* Make sure we don't exced the bounds */
	if (playlist_base + offset < 0) {
		offset = 0 - playlist_base;
	} else if (playlist_base + PLAYLIST_MAXLINES + offset > playlist_size) {
		offset = playlist_size - (playlist_base + PLAYLIST_MAXLINES);
	}

	if (offset < 0) {
		for (i = playlist_base - 1; i >= playlist_base + offset; i--) {
			MP3_GetSongTitle(i+1, playlist_entries[i % PLAYLIST_MAXLINES], 
							sizeof(*playlist_entries));
		}
	} else { // (offset > 0)
		int track_num = playlist_base + 1 + PLAYLIST_MAXLINES;
		for (i = playlist_base; i < playlist_base + offset; i++, track_num++) {
			MP3_GetSongTitle(track_num, playlist_entries[i%PLAYLIST_MAXLINES], 
							sizeof(*playlist_entries));
		}
	}

	playlist_base += offset;
}

/**
 * Reset the playlist, and read from the begining
 */
void M_Menu_MP3_Playlist_Read(void) {
	MP3_CachePlaylist();

	M_Menu_MP3_Playlist_MoveBase(0);
	M_Menu_MP3_Playlist_MoveCursor(0);
}

/**
 * Draw the playlist menu, with the current playing song in white
 */
void M_Menu_MP3_Playlist_Draw(void) {
	int		index, print_time, i, count;
	char 	name[PLAYLIST_MAXTITLE], *s;
	float 	realtime;

	static int last_status,last_elapsed, last_total, last_current;

	realtime = Sys_DoubleTime();

	last_status = MP3_GetStatus();

	if (last_status == MP3_NOTRUNNING) {
		M_Menu_MP3_Control_f();
		return;
	}

	s = va("%s PLAYLIST", mp3_player->PlayerName_AllCaps);
	M_Print ((320 - 8 * strlen(s)) >> 1, PLAYLIST_HEADING_ROW, s);
	M_Print (8, PLAYLIST_HEADING_ROW + 16, "\x1d\x1e\x1e\x1f \x1d\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1e\x1f");

	if (last_status == MP3_PLAYING)
		M_PrintWhite(312 - 7 * 8, PLAYLIST_HEADING_ROW + 8, "Playing");
	else if (last_status == MP3_PAUSED)
		M_PrintWhite(312 - 6 * 8, PLAYLIST_HEADING_ROW + 8, "Paused");
	else if (last_status == MP3_STOPPED)
		M_PrintWhite(312 - 7 * 8, PLAYLIST_HEADING_ROW + 8, "Stopped");

	if (mp3_showtime.value) {
		MP3_GetOutputtime(&last_elapsed, &last_total);
		if (last_total == -1)
			goto menu_items;

		print_time = (mp3_showtime.value == 2) ? last_total - last_elapsed : last_elapsed;
		M_PrintWhite(8, PLAYLIST_HEADING_ROW + 8, SecondsToMinutesString(print_time));

		if (mp3_showtime.value != 2)
			M_PrintWhite(48, M_MP3_CONTROL_HEADINGROW + 8, va("/%s", SecondsToMinutesString(last_total)));
	}
menu_items:
	if (!playlist_size) {
		M_Print (92, 32, "Playlist is empty");
		return;
	}

	MP3_GetPlaylistInfo(&last_current, NULL);

	for (index = playlist_base, count = 0;
			index < playlist_size && count < PLAYLIST_MAXLINES;
			index++, count++) {
		char *spaces;

		/* Pad the index count */
		if (index + 1 < 10)
			spaces = "   ";
		else if (index + 1 < 100)
			spaces = "  ";
		else if (index + 1 < 1000)
			spaces = " ";
		else
			spaces = "";

		/* Limit the title to PLAYLIST_MAXTITLE */
		strlcpy(name, playlist_entries[index % PLAYLIST_MAXLINES],sizeof(name));

		/* Print the current playing song in white */
		if (last_current == index)
			M_PrintWhite(16, PLAYLIST_HEADING_ROW + 24 
					+ (index - playlist_base) * 8, 
				va("%s%d %s", spaces, index + 1, name));
		else
			M_Print(16, PLAYLIST_HEADING_ROW + 24 
					+ (index - playlist_base) * 8, 
				va("%s%d %s", spaces, index + 1, name));
	}
	M_DrawCharacter(8, PLAYLIST_HEADING_ROW + 24 + playlist_cursor * 8, 
			FLASHINGARROW());

	M_DrawCharacter(16, M_MP3_CONTROL_BARHEIGHT, 128);
	for (i = 0; i < 35; i++)
		M_DrawCharacter(24 + i * 8, M_MP3_CONTROL_BARHEIGHT, 129);
	M_DrawCharacter(320 - 16, M_MP3_CONTROL_BARHEIGHT, 130);
	M_DrawCharacter(17 + 286 * ((float) last_elapsed / last_total), 
			M_MP3_CONTROL_BARHEIGHT, 131);
}

void M_Menu_MP3_Playlist_Key (int k) {
	extern int m_topmenu;
	con_suppress = true;

	switch (k) {
		case K_BACKSPACE:
			m_topmenu = m_none;
		case K_ESCAPE:
			M_LeaveMenu (m_mp3_control);
			break;

		case '`':
		case '~':
			key_dest = key_console;
			m_state = m_none;
			break;

		case K_UPARROW:
		case K_MWHEELUP:
			M_Menu_MP3_Playlist_MoveCursorRel(-1);
			break;

		case K_DOWNARROW:
		case K_MWHEELDOWN:
			M_Menu_MP3_Playlist_MoveCursorRel(1);
			break;

		case K_HOME:
			M_Menu_MP3_Playlist_MoveCursor(0);
			M_Menu_MP3_Playlist_MoveBase(0);
			break;

		case K_END:
			M_Menu_MP3_Playlist_MoveCursor(playlist_size-1);
			M_Menu_MP3_Playlist_MoveBase(playlist_size-1);
			break;

		case K_PGUP:
			M_Menu_MP3_Playlist_MoveBaseRel(-(PLAYLIST_MAXLINES));
			break;

		case K_PGDN:
			if (playlist_size > PLAYLIST_MAXLINES)
				M_Menu_MP3_Playlist_MoveBaseRel(PLAYLIST_MAXLINES);
			break;

		case K_ENTER:
			if (!playlist_size)
				break;
			Cbuf_AddText(va("mp3_playtrack %d\n", playlist_cursor + playlist_base + 1));
			break;
		case K_SPACE: M_Menu_MP3_Playlist_Read(); Center_Playlist();break;
		case 'r': MP3_ToggleRepeat_f(); break;
		case 's': MP3_ToggleShuffle_f(); break;
		case KP_LEFTARROW: case 'z': MP3_Prev_f(); break;
		case KP_5: case 'x': MP3_Play_f(); break;
		case 'c': MP3_Pause_f(); break;
		case 'v': MP3_Stop_f();     break;
		case 'V': MP3_FadeOut_f();    break;
		case KP_RIGHTARROW: case 'b': MP3_Next_f(); break;
		case KP_HOME: case K_LEFTARROW:  MP3_Rewind_f(); break;
		case KP_PGUP: case K_RIGHTARROW: MP3_FastForward_f(); break;
	}
	con_suppress = false;
}

void M_Menu_MP3_Playlist_f (void){
	if (!MP3_IsActive()) {
		M_Menu_MP3_Control_f();
		return;
	}

	M_Menu_MP3_Playlist_Read();
	M_EnterMenu (m_mp3_playlist);
	Center_Playlist();
}

#endif // WITH_MP3_PLAYER
