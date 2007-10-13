/*
 * File List
 *
 * display directory contents, allows for changing dir,
 * drive etc..
 *
 * meant for use by MP3 player, mayby demo player too
 *
 * FileList have four colums:
 *  - file name (with ext)
 *  - size
 *  - date (last modification)
 *  - time (last modification)
 *
 * and allows to display different sets of columns
 * also do sorting
 *
 */

#ifndef __EX_FILELIST_H__
#define __EX_FILELIST_H__

#include "keys.h"
#include "Ctrl.h"

//
// maximum-s
//
#define  MAX_FILELIST_ENTRIES  1024
#define  MAX_FILELIST_TYPES    16
#define  MAX_FILELIST_DISPLAY  80
#define  MAX_EXTENSION_LENGTH  15
#define  MAX_SEARCH_STRING     64

//
// entry (file) type strucy
//
typedef struct filetype_s
{
    int         id;
    char        extension [MAX_EXTENSION_LENGTH+1];
}
filetype_t;


//
// file entry structure
//
typedef struct filedesc_s
{
    qbool			is_directory;
	
	#ifdef WITH_ZIP
	qbool			is_archive;
	#endif
    
	char            name[MAX_PATH+1];
    char            display[MAX_PATH+1];
    unsigned long   size;
    SYSTEMTIME      time;
    int             type_index;
}
filedesc_t;

#define FL_MODE_NORMAL		0
#define FL_MODE_DELETE		1
#define FL_MODE_COMPRESS	2
#define FL_MODE_DECOMPRESS	3

//
// Filelist structure
//
typedef struct filelist_s
{
    char			current_dir[MAX_PATH+1];
	char			current_archive[MAX_PATH+1];
	qbool			in_archive;
    qbool			error;          // Error reading dir
    qbool			need_refresh;   // Dir is reread in draw func
    qbool			need_resort;    // Dir is sorted in draw func

    filedesc_t		entries[MAX_FILELIST_ENTRIES];
    int				num_entries;
    int				current_entry;
    int				display_entry;  // First item displayed
    int             displayed_entries_count;    // ammount of entries that fit on the screen

    filetype_t		filetypes[MAX_FILELIST_TYPES];
    int				num_filetypes;

    // Some vars
    cvar_t *		sort_mode;
    cvar_t *		show_size;
    cvar_t *		show_date;
    cvar_t *		show_time;
    cvar_t *		strip_names;
    cvar_t *		interline;
    cvar_t *		show_status;
	cvar_t *		scroll_names;
	cvar_t *		file_color;
	cvar_t *		selected_color;
	cvar_t *		dir_color;
	#ifdef WITH_ZIP
	cvar_t *		archive_color;
	#endif
	qbool			show_dirup;
	qbool			show_dirs;

    // For PGUP/PGDN, filled by drawing func
    int				last_page_size;

    // For searching
    char			search_string[MAX_SEARCH_STRING+1];
    qbool			search_valid;   // If is in search mode
    qbool			search_dirty;   // If should research
    qbool			search_error;   // Not found
	int				mode;			// Are we deleting/compressing a file?

    // For cd ..
    char            cdup_name[MAX_PATH+1];
    qbool			cdup_find;

	// for mouse navigation
    int             list_y_offset;
	int				list_width;
	int				list_height;

    // associated scrollbar GUI element
    PScrollBar      scrollbar;
}
filelist_t;


//
// Drawing routine
//
void FL_Draw(filelist_t *, int x, int y, int w, int h);


//
// Send key to list
// returns: true if processed, false if ignored
//
qbool FL_Key(filelist_t *, int key);


//
// Send Mouse Move event to the list
// returns: true if processed, false if mouse pointed somewhere else
//
qbool FL_Mouse_Event(filelist_t *, const mouse_state_t *ms);

//
// Create file list
//
void FL_Init(filelist_t	*	fl,
             cvar_t *       sort_mode,
             cvar_t *       show_size,
             cvar_t *       show_date,
             cvar_t *       show_time,
             cvar_t *       strip_names,
             cvar_t *       interline,
             cvar_t *       show_status,
			 cvar_t *		scroll_names,
			 cvar_t *		demo_color,
			 cvar_t *		selected_color,
			 cvar_t *		dir_color,
#ifdef WITH_ZIP
			 cvar_t *		archive_color,
#endif
			 char *			 initdir);

//
// Add new file type (.qwd, .qwz, .mp3).
//
void FL_AddFileType(filelist_t *, int id, char *ext);


//
// Set current directory (and drive).
//
void FL_SetCurrentDir(filelist_t *, const char *dir);


//
// hides the ".." option to traverse in the dirs hierarchy
//
void FL_SetDirUpOption(filelist_t *fl, qbool show);


//
// hides the ".." option to traverse in the dirs hierarchy
//
void FL_SetDirsOption(filelist_t *fl, qbool show);


//
// Get current directory.
//
char *FL_GetCurrentDir(filelist_t *);


//
// Get current entry.
//
filedesc_t * FL_GetCurrentEntry(filelist_t *);


//
// Get current path.
//
char *FL_GetCurrentPath(filelist_t *);


//
// Get current display.
//
char *FL_GetCurrentDisplay(filelist_t *fl);


//
// Get current entry type.
//
int FL_GetCurrentEntryType(filelist_t *);


//
// Is current entry a dir ?
//
qbool FL_IsCurrentDir(filelist_t *);

#endif // __EX_FILELIST_H__
