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
    qbool        is_directory;
    char            name[_MAX_PATH+1];
    char            display[MAX_FILELIST_DISPLAY+1];
    unsigned long   size;
    SYSTEMTIME      time;
    int             type_index;
}
filedesc_t;

//
// filelist structure
//
typedef struct filelist_s
{
    char            current_dir[_MAX_PATH+1];
    qbool        error;          // error reading dir
    qbool        need_refresh;   // dir is reread in draw func
    qbool        need_resort;    // dir is sorted in draw func

    filedesc_t      entries[MAX_FILELIST_ENTRIES];
    int             num_entries;
    int             current_entry;
    int             display_entry;  // first item displayed

    filetype_t      filetypes[MAX_FILELIST_TYPES];
    int             num_filetypes;

    // some vars
    cvar_t *       sort_mode;
    cvar_t *       show_size;
    cvar_t *       show_date;
    cvar_t *       show_time;
    cvar_t *       strip_names;
    cvar_t *       interline;
    cvar_t *       show_status;

    // for PGUP/PGDN, filled by drawing func
    int             last_page_size;

    // for searching
    char            search_string[MAX_SEARCH_STRING+1];
    qbool        search_valid;   // if is in search mode
    qbool        search_dirty;   // if should research
    qbool        search_error;   // not found

    // for cd ..
    char            cdup_name[_MAX_PATH+1];
    qbool        cdup_find;
}
filelist_t;


//
// drawing routine
//
void FL_Draw(filelist_t *, int x, int y, int w, int h);


//
// send key to list
// returns: true if processed, false if ignored
//
qbool FL_Key(filelist_t *, int key);


//
// create file list
//
void FL_Init(filelist_t *fl,
             cvar_t *        sort_mode,
             cvar_t *        show_size,
             cvar_t *        show_date,
             cvar_t *        show_time,
             cvar_t *        strip_names,
             cvar_t *        interline,
             cvar_t *        show_status,
			 char *			 initdir);


//
// add new file type (.qwd, .qwz, .mp3)
//
void FL_AddFileType(filelist_t *, int id, char *ext);


//
// set current directory (and drive)
//
void FL_SetCurrentDir(filelist_t *, char *dir);


//
// get current directory
//
char *FL_GetCurrentDir(filelist_t *);


//
// get current entry
//
filedesc_t * FL_GetCurrentEntry(filelist_t *);


//
// get current path
//
char *FL_GetCurrentPath(filelist_t *);


//
// get current display
//
char *FL_GetCurrentDisplay(filelist_t *fl);


//
// get current entry type
//
int FL_GetCurrentEntryType(filelist_t *);


//
// is current entry a dir ?
//
qbool FL_IsCurrentDir(filelist_t *);


#endif // __EX_FILELIST_H__
