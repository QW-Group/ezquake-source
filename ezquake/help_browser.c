// help, browser tab

#include "quakedef.h"


CPageViewer_t help_viewer;


void Help_Browser_Init(void)
{
    CPageViewer_Init(&help_viewer);
    CPageViewer_GoUrl(&help_viewer, "help/index.xml");
}

int Help_Browser_Key(int key, CTab_t *tab, CTabPage_t *page)
{
    return CPageViewer_Key(&help_viewer, key);
}

void Help_Browser_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
    CPageViewer_Draw(&help_viewer, x, y, w, h);
}

void Help_Browser_Focus(void)
{
    CTab_SetCurrentId(&help_tab, help_browser);
}
