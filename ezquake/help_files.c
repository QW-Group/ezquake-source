// help, browser tab

#include "quakedef.h"


filelist_t help_filelist;

cvar_t  help_files_showsize		= {"help_files_showsize",		"1"};
cvar_t  help_files_showdate		= {"help_files_showdate",		"1"};
cvar_t  help_files_showtime		= {"help_files_showtime",		"0"};
cvar_t  help_files_sortmode		= {"help_files_sortmode",		"1"};
cvar_t  help_files_showstatus	= {"help_files_showstatus",		"1"};
cvar_t  help_files_stripnames	= {"help_files_stripnames",		"1"};
cvar_t  help_files_interline	= {"help_files_interline",		"0"};
cvar_t  help_files_scrollnames	= {"&help_files_scrollnames",	"0"};
cvar_t  help_files_dircolor		= {"&help_files_dircolor",		"255 255 255 255"};

void Help_Files_Init(void)
{
	Cvar_SetCurrentGroup(CVAR_GROUP_HELP_BROWSER);
    Cvar_Register(&help_files_showsize);
    Cvar_Register(&help_files_showdate);
    Cvar_Register(&help_files_showtime);
    Cvar_Register(&help_files_sortmode);
    Cvar_Register(&help_files_showstatus);
    Cvar_Register(&help_files_stripnames);
    Cvar_Register(&help_files_interline);
	Cvar_Register(&help_files_scrollnames);
	Cvar_ResetCurrentGroup();

    FL_Init(&help_filelist,
        &help_files_sortmode,
        &help_files_showsize,
        &help_files_showdate,
        &help_files_showtime,
        &help_files_stripnames,
        &help_files_interline,
        &help_files_showstatus,
		&help_files_scrollnames,
		&help_files_dircolor,
		NULL, // No ZIP color needed for help files.
		"./ezquake/help/manual");
    FL_AddFileType(&help_filelist, 0, ".xml");
}

int Help_Files_Key(int key, CTab_t *tab, CTabPage_t *page)
{
    qbool processed;

    processed = FL_Key(&help_filelist, key);

    if (!processed)
    {
        if (key == K_ENTER)
        {
            CPageViewer_Go(&help_viewer, FL_GetCurrentPath(&help_filelist), NULL);
            Help_Browser_Focus();

            processed = true;
        }
    }

    return processed;
}

void Help_Files_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
    FL_Draw(&help_filelist, x, y, w, h);
}
