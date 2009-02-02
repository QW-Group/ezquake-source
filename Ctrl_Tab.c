// Tab Control


#include "quakedef.h"
#include "Ctrl.h"
#include "Ctrl_Tab.h"
#include "keys.h"
#include "qsound.h"

// initialize control
void CTab_Init(CTab_t *tab)
{
    memset(tab, 0, sizeof(CTab_t));
	tab->lastViewedPage = -1;
	tab->hoveredPage = -1;
}


// clear control
void CTab_Clear(CTab_t *tab)
{
    // no dynamic data, just clear it
    CTab_Init(tab);
}


// create new control
CTab_t * CTab_New(void)
{
    CTab_t *tab = (CTab_t *) Q_malloc(sizeof(CTab_t));
    CTab_Init(tab);
    return tab;
}


// free control
void CTab_Free(CTab_t *tab)
{
    CTab_Clear(tab);
    Q_free(tab);
}


// add tab
void CTab_AddPage(CTab_t *tab, const char *name, int id, const CTabPage_Handlers_t *handlers)
{
    int i;
    CTabPage_t *page;

    if (tab->nPages >= TAB_MAX_TABS)
    {
        Sys_Error("CTab_AddPage: Maximum page number reached "
            "while adding %s", name);
        return;
    }

    for (i=0; i < tab->nPages; i++)
    {
        if (tab->pages[i].id == id)
            Sys_Error("CTab_AddPage: such id already exist");
        if (!strcmp(tab->pages[i].name, name))
            Sys_Error("CTab_AddPage: such name already exist");
    }

    page = &tab->pages[tab->nPages++];

    // set name
    strlcpy (page->name, name, sizeof (page->name));

    // set id
    page->id = id;

    // set handlers
    memcpy(&page->handlers, handlers, sizeof(CTabPage_Handlers_t));
}

// will draw tab navigation bar, remember labels positions
// and will return the width of the navigation bar
static int CTab_Draw_PageLinks(CTab_t *tab, int x, int y, int w, int h)
{
    char buf[TAB_MAX_NAME_LENGTH+3];
    int i, cx, cy, ww, wh;          // current x/y, word width/height
    qbool ap, hp;                   // active page / hovered page

    cx = 0; cy = 0;
    for (i = 0; i < tab->nPages; i++)
    {
        *buf = 0;
        ap = tab->activePage == i;
        hp = tab->hoveredPage == i;
        
        // add leading space/brace
        strlcat (buf, ap ? "\x10" : " ", sizeof (buf));
        
        // adds white or red variant of the page name to the buf string
        strlcat (buf, tab->pages[i].name, sizeof (buf));

        // add closing space/brace
        strlcat (buf, ap ? "\x11" : " ", sizeof (buf));
        
        ww = strlen(buf) * LETTERWIDTH;
        wh = LETTERHEIGHT;

        // this is not the first word we are printing and we don't fit on the line anymore
        if (cx && (cx + ww > w)) {
            // so wrap the line already
            cx = 0;
            cy += LETTERHEIGHT;
        }

        UI_Print(x + cx, y + cy, buf, ap || hp);

        // remember where the text was so we can point to it with the mouse later
        tab->navi_boxes[i].x = cx;
        tab->navi_boxes[i].y = cy;
        tab->navi_boxes[i].x2 = cx+ww-LETTERWIDTH;
        tab->navi_boxes[i].y2 = cy+wh;

        // we substract 1 letter here and above because we want only one (common) space
        // between the labels, not two (one for each)
        cx += ww - LETTERWIDTH;
    }

    return cy + LETTERHEIGHT;
}

// draw control
void CTab_Draw(CTab_t *tab, int x, int y, int w, int h)
{
    char line[1024];
	int nav_height;

	if (tab->activePage != tab->lastViewedPage && tab->activePage < tab->nPages)
	{
		CTabPage_OnShowType onshowFnc = tab->pages[tab->activePage].handlers.onshow;
		if (onshowFnc != NULL) onshowFnc();
		tab->lastViewedPage = tab->activePage;
	}

	tab->width = w;
	tab->height = h;

	nav_height = CTab_Draw_PageLinks(tab, x, y, w, h);

    // draw separator
    memset(line, '\x1E', w/8);
    line[w/8] = 0;
    line[w/8-1] = '\x1F';
    line[0] = '\x1D';
	// memcpy(line + 2, " \x10shift\x11+\x10tab\x11 ", min((w/8)-3, 15));
	// memcpy(line + w/8 - 2 - 7, " \x10tab\x11 ", 7);
    UI_Print_Center(x, y + nav_height, w, line, false);
	nav_height += 8;

    // draw page
    if (tab->pages[tab->activePage].handlers.draw != NULL)
        tab->pages[tab->activePage].handlers.draw(x, y + nav_height, w, h- nav_height, tab, &tab->pages[tab->activePage]);
}


// process key
int CTab_Key(CTab_t *tab, int key, wchar unichar)
{
    int handled;

	if (tab->hoveredPage >= 0 && key == K_MOUSE1)
	{
		tab->activePage = tab->hoveredPage;
		return true;
	}

    // we first call tabs handlers, because they might override
    // default tab keys for modal dialogs
    // tabs are responsible for not using "our" keys for other
    // purposes
    handled = false;
    if (tab->pages[tab->activePage].handlers.key != NULL)
        handled = tab->pages[tab->activePage].handlers.key(key, unichar, tab, &tab->pages[tab->activePage]);

    // then try our handlers
    if (!handled)
    {
            switch (key)
            {
			case K_PGUP:
			case K_MOUSE4:
				S_LocalSound ("misc/menu1.wav");
                tab->activePage--;
                handled = true;
                break;

			case K_PGDN:
			case K_MOUSE5:
				S_LocalSound ("misc/menu1.wav");
                tab->activePage++;
                handled = true;
                break;

			case K_LEFTARROW:
				S_LocalSound ("misc/menu1.wav");
				tab->activePage--;
				handled = true;
				break;

			case K_RIGHTARROW:
				S_LocalSound ("misc/menu1.wav");
				tab->activePage++;
				handled = true;
				break;

			case K_TAB:
				S_LocalSound ("misc/menu1.wav");
				if (isShiftDown()) tab->activePage--; else tab->activePage++;
				handled = true;
				break;
            }

            if (handled)
                tab->activePage = (tab->activePage + tab->nPages) % tab->nPages;
    }

    // return handled status
    return handled;
}

static qbool CTab_Navi_Mouse_Event (CTab_t *tab, const mouse_state_t *ms) 
{
    int i;
	if (!tab->width) return false;

    if (ms->button_up == 1) {
        CTab_Key(tab, K_MOUSE1, 0);
        return true;
    } else if (ms->button_up == 2) {
        CTab_Key(tab, K_MOUSE2, 0);
        return true;
    }

    for (i = 0; i < tab->nPages; i++)
    {
        if (!(tab->navi_boxes[i].x > ms->x || tab->navi_boxes[i].y > ms->y || tab->navi_boxes[i].x2 < ms->x || tab->navi_boxes[i].y2 < ms->y))
        {   // pointer is within the bounds
            tab->hoveredPage = i;
            return true;
        }
    }
	return false;
}

qbool CTab_Mouse_Event(CTab_t *tab, const mouse_state_t *ms)
{
    int nav_height = tab->navi_boxes[tab->nPages-1].y2;

    if (ms->x < 0 || ms->x > tab->width || ms->y < 0 || ms->y > tab->height)
        return false;

	if (ms->y <= nav_height)
    {   // pointer is in the navigation area
		return CTab_Navi_Mouse_Event(tab, ms);
	}
    else if (ms->y >= nav_height + LETTERHEIGHT) 
    {   // pointer is in the main area
		CTabPage_MouseMoveType mmf;
		mouse_state_t lms = *ms;

        lms.y -= nav_height + LETTERHEIGHT;
		tab->hoveredPage = -1;

		mmf = tab->pages[tab->activePage].handlers.mousemove;
		if (mmf)
			return mmf(&lms);
	}

    return false;
}

// get current page
CTabPage_t * CTab_GetCurrent(CTab_t *tab)
{
    return &tab->pages[tab->activePage];
}


// get current page id
int CTab_GetCurrentId(CTab_t *tab)
{
    return tab->pages[tab->activePage].id;
}


// get current page name
char * CTab_GetCurrentPage(CTab_t *tab)
{
    return tab->pages[tab->activePage].name;
}


// set current page by ID
void CTab_SetCurrentId(CTab_t *tab, int id)
{
    int i;
    for (i=0; i < tab->nPages; i++)
    {
        if (tab->pages[i].id == id)
        {
            tab->activePage = i;
            return;
        }
    }
    Sys_Error("CTab_SetCurrentId: id not found");
}


// set current page by string
void SetCurrentPage(CTab_t *tab, char *name)
{
    int i;
    for (i=0; i < tab->nPages; i++)
    {
        if (!strcmp(tab->pages[i].name, name))
        {
            tab->activePage = i;
            return;
        }
    }
    Sys_Error("CTab_SetCurrentName: name not found");
}

