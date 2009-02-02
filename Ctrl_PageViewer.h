/*
 * EditBox functions
 *
 */

#ifndef __CTRL_PAGEVIEWER_H__
#define __CTRL_PAGEVIEWER_H__



typedef struct CPageViewer_page_s
{
    char *url;
    xml_document_t *doc;

    document_rendered_t rendered;

    int current_line;
    int width;
    qbool should_render;

    // current settings
    int last_height;

    // previous page in history
    struct CPageViewer_page_s *next;
}
CPageViewer_page_t;

typedef struct CPageViewer_s
{
    CPageViewer_page_t *page;

    // options
    qbool show_title;
    qbool show_status;

    // navigation mode
    qbool navigation_mode;
    document_rendered_link_t **current_links;
    int current_link_index;
    int current_links_total;
}
CPageViewer_t;

// initialize CPageViewer control
void CPageViewer_Init(CPageViewer_t *);

// create new viewer
CPageViewer_t * CPageViewer_New(void);

// load document by url
void CPageViewer_GoUrl(CPageViewer_t *viewer, char *url);

// load document by xml_document_t
void CPageViewer_Go(CPageViewer_t *viewer, char *url, xml_document_t *doc);

// go back
void CPageViewer_Back(CPageViewer_t *viewer, int level);

// draw control
void CPageViewer_Draw(CPageViewer_t *viewer, int x, int y, int w, int h);

// handle keyboard
qbool CPageViewer_Key(CPageViewer_t *viewer, int key, wchar unichar);

// clear control
void CPageViewer_Clear(CPageViewer_t *viewer);

// delete control
void CPageViewer_Free(CPageViewer_t *viewer);

#endif // __CTRL_PAGEVIEWER_H__
