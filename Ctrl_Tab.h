// Tab Control

#ifndef __CTRL_TAB_H__
#define __CTRL_TAB_H__

#include "keys.h"

#define TAB_MAX_NAME_LENGTH     15
#define TAB_MAX_TABS            16


struct CTab_s;
struct CTabPage_s;

typedef void (*CTabPage_OnShowType) (void);
typedef void (*CTabPage_DrawType)(int x, int y, int w, int h, struct CTab_s *, struct CTabPage_s *);
typedef int  (*CTabPage_KeyType)(int key, wchar unichar, struct CTab_s *, struct CTabPage_s *);
typedef qbool (*CTabPage_MouseMoveType) (const mouse_state_t * ms);

typedef struct CTabPage_Handlers_s
{
    CTabPage_DrawType   draw;
    CTabPage_KeyType    key;
    CTabPage_OnShowType onshow;
	CTabPage_MouseMoveType mousemove;
}
CTabPage_Handlers_t;

typedef struct CTabPage_s
{
    char    name[TAB_MAX_NAME_LENGTH+1];
    int     id;
	CTabPage_Handlers_t handlers;
}
CTabPage_t;

typedef struct CTab_s
{
    CTabPage_t  pages[TAB_MAX_TABS];
    int         nPages;
    int         activePage;
	int			lastViewedPage;
	int			hoveredPage;
	int			width;
	int			height;
    struct { int x, y, x2, y2; } navi_boxes[TAB_MAX_TABS];
    void *tag;
}
CTab_t;


// initialize control
void CTab_Init(CTab_t *);

// clear control
void CTab_Clear(CTab_t *);

// create new control
CTab_t * CTab_New(void);

// free control
void CTab_Free(CTab_t *);

// add tab
void CTab_AddPage(CTab_t *, const char *name, int id, const CTabPage_Handlers_t *handlers);
// draw control
void CTab_Draw(CTab_t *, int x, int y, int w, int h);

// process key
int CTab_Key(CTab_t *, int key, wchar unichar);

// process mouse move event
qbool CTab_Mouse_Event(CTab_t *, const mouse_state_t *ms);

// get current page
CTabPage_t * CTab_GetCurrent(CTab_t *);

// get current page id
int CTab_GetCurrentId(CTab_t *);

// get current page name
char * CTab_GetCurrentPage(CTab_t *);

// set current page by ID
void CTab_SetCurrentId(CTab_t *, int);

// set current page by string
void CTab_SetCurrentPage(CTab_t *, char *);



#endif // __CTRL_TAB_H__
