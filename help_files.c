/*
 * Help menu
 *  - browser tab
 *  - index tab
 *  - tutorials tab
 */

// maybe rename this file to menu_help.c

#include "quakedef.h"
#include "menu.h"
#include "EX_FileList.h"
#include "expat.h"
#include "xsd.h"
#include "document_rendering.h"
#include "Ctrl_PageViewer.h"
#include "Ctrl_Tab.h"

enum {
    HELPM_BROWSER,
	HELPM_INDEX,
    HELPM_TUTORIALS
};

CTab_t help_tab;

CPageViewer_t help_viewer;

void Help_Browser_Init(void)
{
    CPageViewer_Init(&help_viewer);
    CPageViewer_GoUrl(&help_viewer, "help/index.xml");
}

int Help_Browser_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
    return CPageViewer_Key(&help_viewer, key, unichar);
}

void Help_Browser_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
    CPageViewer_Draw(&help_viewer, x, y, w, h);
}

void Help_Browser_Focus(void)
{
    CTab_SetCurrentId(&help_tab, HELPM_BROWSER);
}

static qbool Help_Browser_Mouse_Event(const mouse_state_t *ms)
{
	return true;
}

filelist_t help_index_fl;
filelist_t help_tutorials_fl;

static void Help_Index_Init(void)
{
    FL_Init(&help_index_fl,	"./ezquake/help/manual");
    FL_AddFileType(&help_index_fl, 0, ".xml");
	FL_SetDirUpOption(&help_index_fl, false);
	FL_SetDirsOption(&help_index_fl, false);
}

static void Help_Tutorials_Init(void)
{
    FL_Init(&help_tutorials_fl,	"./ezquake/help/manual/demos");
    FL_AddFileType(&help_tutorials_fl, 0, ".mvd");
	FL_SetDirUpOption(&help_tutorials_fl, false);
	FL_SetDirsOption(&help_tutorials_fl, false);
}

static int Help_Index_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
    qbool processed;

    processed = FL_Key(&help_index_fl, key);

    if (!processed)
    {
        if (key == K_ENTER || key == K_MOUSE1)
        {
            CPageViewer_Go(&help_viewer, FL_GetCurrentPath(&help_index_fl), NULL);
            Help_Browser_Focus();

            processed = true;
        }
    }

    return processed;
}

static void Help_Index_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
    FL_Draw(&help_index_fl, x, y, w, h);
}

static qbool Help_Index_Mouse_Event(const mouse_state_t *ms)
{
    if (FL_Mouse_Event(&help_index_fl, ms)) {
        return true;
    } else if (ms->button_up >= 1 && ms->button_up <= 2) {
        Help_Index_Key(K_MOUSE1 - 1 + ms->button_up, 0, &help_tab, help_tab.pages + HELPM_INDEX);
        return true;
    }

    // this specially "eats" button_up event, there is no reason to process other events anyway
	return true;
}


static int Help_Tutorials_Key(int key, wchar unichar, CTab_t *tab, CTabPage_t *page)
{
    qbool processed;

    processed = FL_Key(&help_tutorials_fl, key);

    if (!processed)
    {
        if (key == K_ENTER || key == K_MOUSE1)
        {
			M_LeaveMenus();
			Cbuf_AddText(va("playdemo \"%s\"\n", FL_GetCurrentPath(&help_tutorials_fl)));

            processed = true;
        }
    }

    return processed;
}

static void Help_Tutorials_Draw(int x, int y, int w, int h, CTab_t *tab, CTabPage_t *page)
{
    FL_Draw(&help_tutorials_fl, x, y, w, h);
}

static qbool Help_Tutorials_Mouse_Event(const mouse_state_t *ms)
{
    if (FL_Mouse_Event(&help_tutorials_fl, ms)) {
        return true;
    } else if (ms->button_up >= 1 && ms->button_up <= 2) {
        Help_Tutorials_Key(K_MOUSE1 - 1 + ms->button_up, 0, &help_tab, help_tab.pages + HELPM_TUTORIALS);
        return true;
    }

    // this specially "eats" button_up event, there is no reason to process other events anyway
	return true;
}


CTabPage_Handlers_t help_browser_handlers = {
	Help_Browser_Draw,
	Help_Browser_Key,
	NULL,
	Help_Browser_Mouse_Event
};

CTabPage_Handlers_t help_index_handlers = {
	Help_Index_Draw,
	Help_Index_Key,
	NULL,
	Help_Index_Mouse_Event
};

CTabPage_Handlers_t help_tutorials_handlers = {
	Help_Tutorials_Draw,
	Help_Tutorials_Key,
	NULL,
	Help_Tutorials_Mouse_Event
};


void Menu_Help_Init(void)
{
    // initialize tab control
    CTab_Init(&help_tab);

    Help_Browser_Init();
    Help_Index_Init();
	Help_Tutorials_Init();

    CTab_AddPage(&help_tab, "Browser", HELPM_BROWSER, &help_browser_handlers);
	CTab_AddPage(&help_tab, "Index", HELPM_INDEX, &help_index_handlers);
	CTab_AddPage(&help_tab, "Tutorials", HELPM_TUTORIALS, &help_tutorials_handlers);

    CTab_SetCurrentId(&help_tab, HELPM_BROWSER);
}

void Menu_Help_Key(int key, wchar unichar)
{
	extern void M_Menu_Main_f (void);

    int handled = CTab_Key(&help_tab, key, unichar);

    if (!handled)
    {
        if (key == K_ESCAPE || key == K_MOUSE2)
        {
            M_Menu_Main_f();
        }
    }
}

void Menu_Help_Draw(int x, int y, int w, int h)
{
    CTab_Draw(&help_tab, x, y, w, h);
}

qbool Menu_Help_Mouse_Event(const mouse_state_t *ms)
{
	mouse_state_t nms = *ms;

    if (ms->button_up == 2) {
        Menu_Help_Key(K_MOUSE2, 0);
        return true;
    }

	// we are sending relative coordinates
	nms.x -= OPTPADDING;
	nms.y -= OPTPADDING;
	nms.x_old -= OPTPADDING;
	nms.y_old -= OPTPADDING;

	if (nms.x < 0 || nms.y < 0) return false;

	return CTab_Mouse_Event(&help_tab, &nms);
}
