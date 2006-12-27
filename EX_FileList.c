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

#include "quakedef.h"
#include "localtime.h"
#include "EX_misc.h"
#include "EX_FunNames.h"

// column width
#define COL_SIZE        4
#define COL_DATE        8
#define COL_TIME        5

extern int isShiftDown(void);
extern int isCtrlDown(void);
extern int isAltDown(void);

extern void cvar_toggle (cvar_t *var);
extern void _splitpath (const char *path, char *drive, char *dir, char *file, char *ext);

//
// create list
//
 void FL_Init(filelist_t *fl,
             cvar_t *        sort_mode,
             cvar_t *        show_size,
             cvar_t *        show_date,
             cvar_t *        show_time,
             cvar_t *        strip_names,
             cvar_t *        interline,
             cvar_t *        show_status,
			 char *initdir)
{
    Sys_getcwd(fl->current_dir, _MAX_PATH);
	FL_SetCurrentDir(fl, initdir);
    fl->error = false;
    fl->need_refresh = true;
    fl->num_entries = 0;
    fl->current_entry = 0;
    fl->num_filetypes = 0;

    fl->sort_mode = sort_mode;
    fl->show_size = show_size;
    fl->show_date = show_date;
    fl->show_time = show_time;
    fl->strip_names = strip_names;
    fl->interline = interline;
    fl->show_status = show_status;
	fl->search_string[0] = 0;

    fl->last_page_size = 0;

    fl->search_valid = false;
    fl->cdup_find = false;
}


//
// set directory
//
 void FL_SetCurrentDir(filelist_t *fl, char *dir)
{
    char buf[_MAX_PATH+1];

    if (Sys_fullpath(buf, dir, _MAX_PATH+1) == NULL)
        return;

    if (strlen(buf) > _MAX_PATH)    // should never fail in this
        return;

	strlcpy (fl->current_dir, buf, sizeof(fl->current_dir));
    fl->need_refresh = true;
}


//
// get current directory
//
 char *FL_GetCurrentDir(filelist_t *fl)
{
    return fl->current_dir;
}


//
// add new file type (.qwd, .qwz, .mp3)
//
 void FL_AddFileType(filelist_t *fl, int id, char *ext)
{
    int num = fl->num_filetypes;

    if (num >= MAX_FILELIST_TYPES)
        return;

    if (strlen(ext) > MAX_EXTENSION_LENGTH)
        return;

    strcpy(fl->filetypes[num].extension, ext);
    fl->filetypes[num].id = id;

    fl->num_filetypes ++;
}


//
// get current entry
//
 filedesc_t * FL_GetCurrentEntry(filelist_t *fl)
{
    if (fl->num_entries <= 0)
        return NULL;

    return &fl->entries[fl->current_entry];
}


//
// get current path
//
 char *FL_GetCurrentPath(filelist_t *fl)
{
    if (fl->num_entries <= 0)
        return NULL;

    return fl->entries[fl->current_entry].name;
}


//
// get current display
//
 char *FL_GetCurrentDisplay(filelist_t *fl)
{
    if (fl->num_entries <= 0)
        return NULL;

    return fl->entries[fl->current_entry].display;
}


//
// get current entry type
//
 int FL_GetCurrentEntryType(filelist_t *fl)
{
    if (fl->num_entries <= 0)
        return -1;

    return fl->filetypes[fl->entries[fl->current_entry].type_index].id;
}


//
// is current entry a dir ?
//
 qbool FL_IsCurrentDir(filelist_t *fl)
{
    if (fl->num_entries <= 0)
        return true;    // we can handle that

    return fl->entries[fl->current_entry].is_directory;
}


void FL_StripFileName(filelist_t *fl, filedesc_t *f)
{
    int i;
    char namebuf[_MAX_FNAME];
    char extbuf[_MAX_EXT];
    char *t;

    _splitpath(f->name, NULL, NULL, namebuf, extbuf);

    if (fl->strip_names->value  &&  !f->is_directory)
    {
        char *s;

        for (i=0; i < strlen(namebuf); i++)
            if (namebuf[i] == '_')
                namebuf[i] = ' ';

        // remove spaces from the beginning
        s = namebuf;
        while (*s == ' ')
            s++;
        memmove(namebuf, s, strlen(s) + 1);

        // remove spaces from the end
        s = namebuf + strlen(namebuf);
        while (s > namebuf  &&  *(s-1) == ' ')
            s--;
        *s = 0;

        // remove few spaces in a row
        s = namebuf;
        while (s < namebuf + strlen(namebuf))
        {
            if (*s == ' ')
            {
                char *q = s+1;
                while (*q == ' ')
                    q++;

                if (q > s+1)
                    memmove(s+1, q, strlen(q)+1);
            }
            s++;
        }

        if (namebuf[0] == 0)
            strcpy(namebuf, "_");
    }

    // replace all non-standard characters with '_'
    for (i=0; i < strlen(namebuf); i++)
    {
        if (namebuf[i] < ' '  ||  namebuf[i] > '~')
            namebuf[i] = '_';
    }

    f->display[0] = 0;
    t = f->display;
    if (f->is_directory)
        *t++ = '/';
    for (i=0; i < strlen(namebuf)  &&  (t - f->display) < MAX_FILELIST_DISPLAY; i++)
        *t++ = namebuf[i];
    for (i=0; i < strlen(extbuf)  &&  (t - f->display) < MAX_FILELIST_DISPLAY; i++)
        *t++ = extbuf[i];
    *t = 0;
}


//
// goto specific file by its name
//
void FL_GotoFile(filelist_t *fl, char *name)
{
    int i;

    fl->current_entry = 0;
    if (!name[0])
        return;

    for (i=0; i < fl->num_entries; i++)
    {
        if (!strcmp(name, fl->entries[i].name))
        {
            fl->current_entry = i;
            return;
        }
    }

    return;
}


//
// file compare func
// set FL_CompareFunc_FileList before calling..
//
filelist_t * FL_CompareFunc_FileList;    // global
int FL_CompareFunc(const void * p_d1, const void * p_d2)
{
	extern int  SYSTEMTIMEcmp(const SYSTEMTIME *, const SYSTEMTIME *);
    int reverse = 0;
    filelist_t *fl = FL_CompareFunc_FileList;
    char *sort_string = fl->sort_mode->string;
    const filedesc_t *d1 = (filedesc_t *)p_d1;
    const filedesc_t *d2 = (filedesc_t *)p_d2;

    // directories always first
    if (d1->is_directory &&  !d2->is_directory)
        return -1;
    if (d2->is_directory  && !d1->is_directory)
        return 1;

    // directories sorted always by name, ascending
    if (d1->is_directory && d2->is_directory)
        return strcasecmp(d1->name, d2->name);

    while (true)
    {
        long int d;  // difference
        char c = *sort_string++;

        if (c & 128)
        {
            reverse = 1;
            c -= 128;
        }
        
        switch (c)
        {
            case '1':   // name
                d = strcasecmp(d1->name, d2->name); break;
            case '2':   // size
		        d = d1->size - d2->size;
                break;
            case '3':   // date / time
                d = SYSTEMTIMEcmp(&(d1->time), &(d2->time));
                break;
            case '4':   // type
            {
                char *ext1 = fl->filetypes[d1->type_index].extension;
                char *ext2 = fl->filetypes[d2->type_index].extension;
                d = strcasecmp(ext1, ext2);
                break;
            }
            default:
                d = d1 - d2;
        }

        if (d)
            return reverse ? -d : d;
    }
}


//
// sort directory
//
void FL_SortDir(filelist_t *fl)
{
    char name[_MAX_PATH+1] = "";

    if (fl->num_entries <= 0  ||
        fl->sort_mode->string == NULL  ||
        fl->sort_mode->string[0] == 0)
        return;

    FL_CompareFunc_FileList = fl;
    if (fl->current_entry >= 0  &&  fl->current_entry < fl->num_entries)
        strcpy(name, fl->entries[fl->current_entry].name);

    qsort(fl->entries, fl->num_entries, sizeof(filedesc_t), FL_CompareFunc);

    FL_GotoFile(fl, name);

    fl->need_resort = false;
}


//
// read directory
//
void FL_ReadDir(filelist_t *fl)
{
	#ifdef WITH_ZIP
	unzFile zip_file;
	qbool is_zip = false;
	filedesc_t *curr_entry = NULL;
	#endif

    int i;
    sys_dirent ent;
    unsigned long search;
    int temp;
    char  olddir[_MAX_PATH+1];

    fl->error = true;
    fl->need_refresh = false;
    fl->display_entry = 0;

	// Save the current directory. (we want to restore this later)
    if (Sys_getcwd(olddir, _MAX_PATH+1) == NULL)
	{
        return;
	}

	// Change to the new dir.
	#ifdef WITH_ZIP
	curr_entry = FL_GetCurrentEntry(fl);
	if (fl->current_entry > 0)
	{
		if (curr_entry != NULL && curr_entry->name != NULL)
		{
			is_zip = COM_ZipIsArchive (curr_entry->name);
		}
	}

	// If it's not a zip file and we cannot change to it, abort.
	if (!is_zip && !Sys_chdir (fl->current_dir))
	{
		return;
	}
	
	if (is_zip)
	{
		// Open the zip file.
		zip_file = COM_ZipUnpackOpenFile (curr_entry->name);

		if (zip_file == NULL)
		{
			return;
		}

		// Set the zip file as the current dir.
		strlcpy (fl->current_dir, curr_entry->name, sizeof(fl->current_dir));
	}
	#else 	
    if (!Sys_chdir(fl->current_dir))
	{
		// Set the current dir.
        return;
	}
	#endif // WITH_ZIP

	fl->num_entries = 0;
	fl->current_entry = 0;
    fl->error = false;

	#ifdef WITH_ZIP
	if (is_zip)
	{
		if (!COM_ZipGetFirst (zip_file, &ent))
		{
			goto finish;
		}
	}
	else
	#endif
	// Get the first entry in the dir.
	{
		search = Sys_ReadDirFirst(&ent);
		if (!search)
		{
			goto finish;
		}
	}

    do
    {
		// Pointer to the current file entry.
        filedesc_t *f = &fl->entries[fl->num_entries];
        f->type_index = -1;

		// Skip current/above dir and hidden files.
		if (!strcmp(ent.fname, ".") || !strcmp(ent.fname, "..") || ent.hidden)
		{
            goto skip;
		}

        // Find registered type if it's not a directory.
        if (!ent.directory)
        {
            for (i=0; i < fl->num_filetypes; i++)
            {
                char ext[_MAX_EXT];

                _splitpath(ent.fname, NULL, NULL, NULL, ext);
   
                if (!strcasecmp(fl->filetypes[i].extension, ext))
                {
                    f->type_index = i;
                    break;
                }
            }
        }

		// We're not interested in this file type since it wasn't registered.
        if (!ent.directory && f->type_index < 0)
		{
            goto skip; 
		}

        // We found a file that we're interested in so save the info about it
		// in the file description structure.
		{
			f->is_directory = ent.directory;

			#ifdef WITH_ZIP
			// If the found file is a zip file, treat it as a directory also.
			if (COM_ZipIsArchive (ent.fname)) 
			{
				f->is_directory = true;	
				
			}

			// If the parent to this file is a zip file, include it in the full path.
			// c:\quake\qw\demos\demo.zip\demo.mvd
			if (is_zip)
			{
				snprintf (f->name, sizeof(f->name), "%s%c%s", curr_entry->name, PATH_SEPARATOR, ent.fname);
			}
			else
			#endif // WITH_ZIP
			{
				Sys_fullpath(f->name, ent.fname, _MAX_PATH+1);
			}

			f->size = ent.size;
			memcpy(&f->time, &ent.time, sizeof(f->time));
		}

        // Find friendly name
        FL_StripFileName(fl, f);

        // Increase counter of how many files have been found.
        fl->num_entries++;
        if (fl->num_entries >= MAX_FILELIST_ENTRIES)
		{
            break;
		}
skip:
        // Get next filesystem entry
		#ifdef WITH_ZIP
		if (is_zip)
		{
			temp = COM_ZipGetNextFile (zip_file, &ent);
		}
		else
		#endif // WITH_ZIP
		{
			temp = Sys_ReadDirNext(search, &ent);
		}
    }
    while (temp > 0);

	#ifdef WITH_ZIP
	if (!is_zip)
	#endif 
	{
		// Close the handle for the directory.
		Sys_ReadDirClose(search);
	}

    fl->need_resort = true;

finish:
	#ifdef WITH_ZIP
	if (is_zip)
	{
		// Close the zip file.
		COM_ZipUnpackCloseFile (zip_file);
		//Sys_chdir (fl->current_dir);
		Sys_chdir (olddir);
	}
	else
	#endif
	{
		// Change the current dir back to what it was.
		Sys_chdir (olddir);
	}

    // Re-sort the file list if needed.
    if (fl->need_resort)
	{
        FL_SortDir (fl);
	}

    fl->current_entry = 0;  // resort could change that

    // Look for what we have to highlight if valid (cdup)
    if (fl->cdup_find)
    {
        int uplen = strlen (fl->cdup_name);
        fl->cdup_find = false;
        for (i=0; i < fl->num_entries; i++)
        {
            int clen = strlen (fl->entries[i].name);

            if (uplen > clen)
			{
                continue;
			}

            if (strcmp(fl->cdup_name, fl->entries[i].name + clen - uplen))
			{
                continue;
			}

            fl->current_entry = i;
            break;
        }
    }

    return;
}


//
// search by name for next item
//
qbool FL_Search(filelist_t *fl)
{
    int i;
    char tmp[512];

    fl->search_dirty = false;

    for (i=fl->current_entry; i < fl->num_entries; i++)
    {
        strncpy(tmp, fl->entries[i].display, 512);
        tmp[511] = 0;
        FunToSort(tmp);
        if (strstr(tmp, fl->search_string))
        {
            fl->current_entry = i;
            return true;
        }
    }
    fl->search_error = true;
    return false;
}


//
// FL_ChangeDir - changes directory
//
void FL_ChangeDir(filelist_t *fl, char *newdir)
{
	char olddir[_MAX_PATH+1];

	#ifdef WITH_ZIP
	char old_current_dir[_MAX_PATH+1];
	char zip_parent[_MAX_PATH+1];
	qbool is_zip = COM_ZipIsArchive (fl->current_dir);

	strlcpy (old_current_dir, fl->current_dir, sizeof(old_current_dir));
	#endif
 
	// Get the current dir from the OS and save it.
    if (Sys_getcwd(olddir, _MAX_PATH+1) == NULL)
	{
		return;
	}

	// Change to the current dir that we're in (might be different from the OS's).
	// If we're changing dirs in a relative fashion ".." for instance we need to 
	// be in this dir, and not the dir that the OS is in.
	#ifdef WITH_ZIP
	if (is_zip) 
	{
		// Zip file.

		// A zip file isn't a directory, but we're pretending it is. To be able to move around in
		// the filesystem we use the parent directory instead.
		strlcpy (zip_parent, fl->current_dir, 
			min(sizeof(zip_parent), strlen(fl->current_dir) - strlen (COM_SkipPath(fl->current_dir))));

		// Change to the parent directory of the zip.
		if (Sys_chdir(zip_parent) == 0)
		{
			return;
		}

		// If we were trying to change the directory to ".." (parenT) we have already done that.
		if (!strncmp (newdir, "..", 2))
		{
			newdir += 2;
		}

		// Make sure we don't have any slashes left at the start of the destination string.
		while (*newdir == '/' || *newdir == '\\')
		{
			newdir ++;
		}
	}
	else 
	#endif // WITH_ZIP
    if (Sys_chdir(fl->current_dir) == 0)
	{
		// Normal directory.
        return;
	}	

	// Change to the new dir requested.
	Sys_chdir (newdir);

	// Save the current dir we just changed to.
	Sys_getcwd (fl->current_dir, _MAX_PATH+1);

	// Go back to where the OS wants to be. 
    Sys_chdir (olddir);

    fl->need_refresh = true;
}


//
// FL_ChangeDirUp - cd ..
//
void FL_ChangeDirUp(filelist_t *fl)
{
    char *t;

    if (strlen(fl->current_dir) < 2)
        return;

    t = fl->current_dir + strlen(fl->current_dir) - 2;

    while (t > fl->current_dir  &&  *t != PATH_SEPARATOR)
        t--;

    if (*t != PATH_SEPARATOR)
        return;

    fl->cdup_find = true;
    strlcpy (fl->cdup_name, t, sizeof(fl->cdup_name));

    FL_ChangeDir(fl, "..");
}


//
// check current position
//
void FL_CheckPosition(filelist_t *fl)
{
    if (fl->current_entry < 0)
        fl->current_entry = 0;
    if (fl->current_entry >= fl->num_entries)
        fl->current_entry = fl->num_entries - 1;
}


//
// check display position
//
void FL_CheckDisplayPosition(filelist_t *fl, int lines)
{
    // FIXME: move list earlier..
    if (fl->current_entry > fl->display_entry + lines - 1)
        fl->display_entry = fl->current_entry - lines + 1;

    if (fl->display_entry > fl->num_entries - lines)
        fl->display_entry = max(fl->num_entries - lines, 0);

    if (fl->current_entry < fl->display_entry)
        fl->display_entry = fl->current_entry;
}


//
// keys handling
// returns: true if processed, false if ignored
//
 qbool FL_Key(filelist_t *fl, int key)
{
	int ce;

	if (fl->delete_mode)
	{
		switch(key) {
			case 'y':
			case 'Y':
			case K_ENTER:
				if (!FL_IsCurrentDir(fl)) {
					ce = fl->current_entry;		// remember where we were
					unlink(FL_GetCurrentPath(fl));
					FL_ReadDir(fl);				// reload dir
					fl->current_entry = ce - 1; // set previous position
					FL_CheckPosition(fl);
				}
				fl->delete_mode = false;
				return true;
			case 'n':
			case 'N':
			case K_ESCAPE:
				fl->delete_mode = false;
				return true;
		}
		
		return false;
	}

    // check for search
    if ((key >= ' ' && key <= '~') && (fl->search_valid || (!isAltDown() && !isCtrlDown() && !isShiftDown())))
    {
        int len;

        if (!fl->search_valid)
        {
            // start searching
            fl->search_valid = true;
            fl->search_error = false;
            fl->search_string[0] = 0;
        }

        len = strlen(fl->search_string);

        if (len < MAX_SEARCH_STRING  &&  !fl->search_error)
        {
            fl->search_string[len] = key;
            fl->search_string[len+1] = 0;
            fl->search_dirty = true;
        }

        return true;    // handled
    }
    else
    {
        fl->search_valid = false;   // finish search mode
    }

    // sorting mode / displaying columns
	if (key >= '1' && key <= '4') {
		if (isCtrlDown && !isAltDown() && !isShiftDown())
		{
			switch (key)
			{
			case '2':
				cvar_toggle(fl->show_size); break;
			case '3':
				cvar_toggle(fl->show_date); break;
			case '4':
				cvar_toggle(fl->show_time); break;
			default:
				break;
			}
			return true;
		}
		else if (!isCtrlDown() && isAltDown() && !isShiftDown())
		{
			char buf[128];

			strncpy(buf, fl->sort_mode->string, 32);
			if (key  ==  buf[0])
			{
				// reverse order
				buf[0] ^= 128;
			}
			else
			{
				// add new
				memmove(buf+1, buf, strlen(buf)+1);
				buf[0] = key;
			}
			buf[8] = 0;
			Cvar_Set(fl->sort_mode, buf);

			fl->need_resort = true;
			return true;
		}
	}

    // change drive
#ifdef _WIN32
    if (isAltDown()  &&  isCtrlDown()  &&
        tolower(key) >= 'a'  &&  tolower(key) <= 'z')
    {
        char newdir[_MAX_PATH+1];
        char olddir[_MAX_PATH+1];

        sprintf(newdir, "%c:\\", tolower(key));

        // validate
        if (Sys_getcwd(olddir, _MAX_PATH+1) == 0)
            return true;
        if (Sys_chdir(newdir) == 0)
            return true;
        Sys_chdir(olddir);

        // and set
        FL_SetCurrentDir(fl, newdir);
        fl->need_refresh = true;

        return true;
    }
#endif

    if (key == '\\'  ||  key == '/')
    {
        FL_ChangeDir(fl, va("%c", PATH_SEPARATOR));
        return true;
    }

    if (key == K_ENTER)
    {
        if (FL_IsCurrentDir(fl))
        {
            FL_ChangeDir(fl, FL_GetCurrentPath(fl));
            return true;
        }
        else
        {
			return false;
        }
    }

    if (key == K_BACKSPACE)
    {
        FL_ChangeDirUp(fl);
        return true;
    }

    if (key == K_UPARROW)
    {
        fl->current_entry--;
        FL_CheckPosition(fl);
        return true;
    }

    if (key == K_DOWNARROW)
    {
        fl->current_entry++;
        FL_CheckPosition(fl);
        return true;
    }

    if (key == K_PGUP)
    {
        fl->current_entry -= fl->last_page_size;
        FL_CheckPosition(fl);
        return true;
    }

    if (key == K_PGDN)
    {
        fl->current_entry += fl->last_page_size;
        FL_CheckPosition(fl);
        return true;
    }

    if (key == K_HOME)
    {
        fl->current_entry = 0;
        FL_CheckPosition(fl);
        return true;
    }

    if (key == K_END)
    {
        fl->current_entry = fl->num_entries - 1;
        FL_CheckPosition(fl);
        return true;
    }

	if (key == K_DEL)
	{
		if (!FL_IsCurrentDir(fl)) {
			fl->delete_mode = true;
		}
		return true;
	}

    return false;
}

// this is used only in FL_Draw below, EX_browser.c has Add_Column2
static void Add_Column(char *line, int *pos, char *t, int w)
{
    if ((*pos) - w - 1  <=  1)
        return;
    (*pos) -= w;
    memcpy(line+(*pos), t, min(w, strlen(t)));
    (*pos)--;
    line[*pos] = ' ';
}

//
// FileList drawing func
//
void FL_Draw(filelist_t *fl, int x, int y, int w, int h)
{
    int i;
    int listsize, pos, interline, inter_up, inter_dn, rowh;
    char line[1024];
    char sname[200], ssize[COL_SIZE+1], sdate[COL_DATE+1], stime[COL_TIME+1];

	if (fl->delete_mode)
	{
		UI_Print_Center(x, y + 8, w, "Are you sure you want to delete this file?", true);
		UI_Print_Center(x, y + 24, w, FL_GetCurrentDisplay(fl), false);
		UI_Print_Center(x, y + 40, w, "(Y/N)", true);
		return;
	}

    w = (w/8)*8;
    //h = (h/8)*8;
    fl->last_page_size = 0;

    // calc interline
    interline = fl->interline->value;
    interline = max(interline, 0);
    interline = min(interline, 6);
    inter_up = interline / 2;
    inter_dn = interline - inter_up;
    rowh = 8 + inter_up + inter_dn;
    listsize = h / rowh;

    // check screen boundaries and mimnimum size
    if (w < 160  ||  h < 80)
        return;
    if (x < 0  ||  y < 0  ||  x+w > vid.width  ||  y+h > vid.height)
        return;

    if (fl->need_refresh)
        FL_ReadDir(fl);
    if (fl->need_resort)
        FL_SortDir(fl);
    if (fl->search_dirty)
        FL_Search(fl);

    // print current dir
    if (strlen(fl->current_dir) <= w/8)
        strcpy(line, fl->current_dir);
    else
    {
        strncpy(line, fl->current_dir, 10);
        strcpy(line+10, "...");
        strncpy(line+13, fl->current_dir + strlen(fl->current_dir) - w/8 + 13, w/8);
    }
    line[w/8] = 0;
    UI_Print(x, y+inter_up, line, true);
    listsize--;

    // draw column titles
    pos = w/8;
    memset(line, ' ', pos);
    line[pos] = 0;

    if (fl->show_time->value)
        Add_Column(line, &pos, "time", COL_TIME);
    if (fl->show_date->value)
        Add_Column(line, &pos, "date", COL_DATE);
    if (fl->show_size->value)
        Add_Column(line, &pos, "  kb", COL_SIZE);

    memcpy(line, "name", min(pos, 4));
    UI_Print_Center(x, y+rowh+inter_dn, w, line, true);
    listsize--;

    if (fl->num_entries <= 0)
    {
        UI_Print_Center(x, y+2*rowh+inter_dn+4, w, "directory empty", false);
        return;
    }
    if (fl->error)
    {
        UI_Print_Center(x, y+2*rowh+inter_dn+4, w, "error reading directory", false);
        return;
    }

    if (fl->show_status->value)
        listsize -= 3;

    fl->last_page_size = listsize;  // remember for PGUP/PGDN

    // check / fix display position now
    FL_CheckPosition(fl);
    FL_CheckDisplayPosition(fl, listsize);

    for (i = 0; i < listsize; i++)
    {
        filedesc_t *entry;
        DWORD dwsize;
		char size[COL_SIZE+1], date[COL_DATE+1], time[COL_TIME+1];
        int filenum = fl->display_entry + i;

        if (filenum >= fl->num_entries)
            break;

        entry = &fl->entries[filenum];

        // extract date & time
        sprintf(date, "%02d-%02d-%02d", entry->time.wYear % 100,
                entry->time.wMonth, entry->time.wDay);
        sprintf(time, "%2d:%02d",
                entry->time.wHour, entry->time.wMinute);

        // extract size
        if (entry->is_directory)
        {
            strcpy(size, "<-->");
            if (filenum == fl->current_entry)
                strlcpy(ssize, "dir", sizeof(ssize));
        }
        else
        {
            dwsize = entry->size / 1024;
            if (dwsize > 9999)
            {
                dwsize /= 1024;
                dwsize = min(dwsize, 999);
                sprintf(size, "%3dm", dwsize);
                if (filenum == fl->current_entry)
                    snprintf(ssize, sizeof(ssize), "%d mb", dwsize);
            }
            else
            {
                sprintf(size, "%4d", dwsize);
                if (filenum == fl->current_entry)
                    snprintf(ssize, sizeof(ssize), "%d kb", dwsize);
            }
        }

        // display it
        pos = w/8;
        memset(line, ' ', pos);
        if (fl->show_time->value)
            Add_Column(line, &pos, time, COL_TIME);
        if (fl->show_date->value)
            Add_Column(line, &pos, date, COL_DATE);
        if (fl->show_size->value)
            Add_Column(line, &pos, size, COL_SIZE);

        memcpy(line, entry->display, min(pos, strlen(entry->display)));

        if (filenum == fl->current_entry)
            line[pos] = 141;

        line[w/8] = 0;
        UI_Print_Center(x, y+rowh*(i+2)+inter_dn, w, line, filenum==fl->current_entry);

        // remember for status bar
        if (filenum == fl->current_entry)
        {
            strcpy(sname, entry->display);
            strcpy(stime, time);
            sprintf(sdate, "%02d-%02d-%02d", entry->time.wYear % 100,
                entry->time.wMonth, entry->time.wDay);
        }
    }

    if (fl->show_status->value)
    {
        memset(line, '\x1E', w/8);
        line[w/8] = 0;
        line[w/8-1] = '\x1F';
        line[0] = '\x1D';
        UI_Print(x, y+h-3*rowh-inter_up, line, false);

        sname[w/8] = 0;
        UI_Print_Center(x, y+h-2*rowh-inter_up, w, sname, false);

        if (fl->search_valid)
        {	// some weird but nice-looking string in Quake font perhaps
			strcpy(line, "search for: ");   // seach for:
            if (fl->search_error)
            {
                strcat(line, "not found");
                line[w/8] = 0;
                UI_Print_Center(x, y+h-rowh-inter_up, w, line, false);
            }
            else
            {
                strcat(line, fl->search_string);
                line[w/8] = 0;
                UI_Print_Center(x, y+h-rowh-inter_up, w, line, true);
            }
        }
        else
        {
            sprintf(line, "%s \x8f modified: %s %s", ssize, sdate, stime);
            UI_Print_Center(x, y+h-rowh-inter_up, w, line, false);
        }
    }
}

// make ../backspace work fine (ala demoplayer)
// do not show hidden files
