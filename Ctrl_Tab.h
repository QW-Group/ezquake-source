// Tab Control

#ifndef __CTRL_TAB_H__
#define __CTRL_TAB_H__


#define TAB_MAX_NAME_LENGTH     15
#define TAB_MAX_TABS            16


struct CTab_s;
struct CTabPage_s;


typedef void (*CTabPage_DrawType)(int x, int y, int w, int h, struct CTab_s *, struct CTabPage_s *);
typedef int  (*CTabPage_KeyType)(int key, struct CTab_s *, struct CTabPage_s *);


typedef struct CTabPage_s
{
    char    name[TAB_MAX_NAME_LENGTH+1];
    int     id;

    CTabPage_DrawType   drawFunc;
    CTabPage_KeyType    keyFunc;

    void    *tag;
}
CTabPage_t;


typedef struct CTab_s
{
    CTabPage_t  pages[TAB_MAX_TABS];
    int         nPages;
    int         activePage;
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
void CTab_AddPage(CTab_t *, char *name, int id, void *tag,
                  CTabPage_DrawType, CTabPage_KeyType);
// draw control
void CTab_Draw(CTab_t *, int x, int y, int w, int h);

// process key
int CTab_Key(CTab_t *, int key);

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
