#ifndef __HELP_H__
#define __HELP_H__


#include "Ctrl_Tab.h"
#include "Ctrl_PageViewer.h"


typedef enum
{
    help_files,
    help_help,
    help_browser,
    help_online,
    help_options
}
help_tab_t;

CTab_t help_tab;

// initialize help system
void Help_Init(void);

// files
extern  filelist_t help_filelist;
void    Help_Files_Init(void);
int     Help_Files_Key(int key, CTab_t *tab, CTabPage_t *page);
void    Help_Files_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page);

// browser
extern  CPageViewer_t help_viewer;
void    Help_Browser_Init(void);
int     Help_Browser_Key(int key, CTab_t *tab, CTabPage_t *page);
void    Help_Browser_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page);
void    Help_Browser_Focus(void);


#endif // __HELP_H__
