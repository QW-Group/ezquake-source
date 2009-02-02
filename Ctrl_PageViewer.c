/*
 * PageViewer control
 *
 */


#include "quakedef.h"
#include "Ctrl.h"
#include "expat.h"
#include "xsd.h"
#include "document_rendering.h"
#include "Ctrl_PageViewer.h"
#include "keys.h"


static void LeaveNavigationMode(CPageViewer_t *viewer)
{
    if (viewer->navigation_mode)
    {
        Q_free(viewer->current_links);
        viewer->current_link_index = 0;
        viewer->current_links_total = 0;
        viewer->navigation_mode = false;
    }
}

static void EnterNavigationMode(CPageViewer_t *viewer)
{
    int i;
    document_rendered_link_t *from, *to;
    int screen_start, screen_end;

    if (viewer->navigation_mode)
        LeaveNavigationMode(viewer);

    screen_start = viewer->page->current_line * viewer->page->width;
    screen_end = (viewer->page->current_line + viewer->page->last_height) * viewer->page->width;

    from = viewer->page->rendered.links;
    while (from)
    {
        if (from->start >= screen_start  ||  from->end >= screen_start)
            break;
        from = from->next;
    }

    if (from == NULL)
        return; // no navigation

    viewer->current_links_total = 0;
    to = from;
    while (to)
    {
        if (to->start >= screen_end)
            break;

        viewer->current_links_total++;
        to = to->next;
    }

    if (viewer->current_links_total == 0)
        return;

    // make links table
    viewer->current_links = (document_rendered_link_t **)
        Q_malloc(viewer->current_links_total*sizeof(document_rendered_link_t *));
    i = 0;
    while (from != to)
    {
        viewer->current_links[i++] = from;
        from = from->next;
    }
    viewer->navigation_mode = true;
    if (isShiftDown())
        viewer->current_link_index = viewer->current_links_total - 1;
    else
        viewer->current_link_index = 0;
}

static void MakeString(char *buf, byte *mem, int len)
{
    int i;
    for (i=0; i < len; i++)
        buf[i] = mem[i] == 0 ? ' ' : mem[i];
    buf[len] = 0;
}

static void FreePageRendered(CPageViewer_page_t *page)
{
    XSD_RenderClear(&page->rendered);
}

static void FreePage(CPageViewer_page_t *page)
{
    FreePageRendered(page);
    if (page->url)
        Q_free(page->url);
    if (page->doc)
        XSD_FreeDocument((xml_t *)page->doc);
    Q_free(page);
    return;
}

static void RenderDocument(CPageViewer_t *viewer, int width)
{
    // int lines;
    xml_document_t *doc;

    LeaveNavigationMode(viewer);
    FreePageRendered(viewer->page);

    viewer->page->current_line = 0;
    viewer->page->width = width;
    viewer->page->should_render = false;

    // load document
    if (!viewer->page->doc)
        viewer->page->doc = XSD_LoadDocumentWithXsl(viewer->page->url);

    if (!viewer->page->doc)
        goto error;

    doc = viewer->page->doc;

    if (!XSD_RenderDocument(&viewer->page->rendered, doc, width))
        goto error;

    return;

error:

    viewer->page->rendered.title = (char *) Q_malloc(viewer->page->width);
    memset(viewer->page->rendered.title, 0, width);
    memcpy(viewer->page->rendered.title, "Document loading error!", strlen("Document loading error!"));
    viewer->page->rendered.title_lines = 1;
    return;
}

static void AddPage(CPageViewer_t *viewer)
{
    CPageViewer_page_t *page = (CPageViewer_page_t *) Q_malloc(sizeof(CPageViewer_page_t));
    memset(page, 0, sizeof(CPageViewer_page_t));

    page->next = viewer->page;
    viewer->page = page;
}

void CPageViewer_Init(CPageViewer_t *viewer)
{
    memset(viewer, 0, sizeof(CPageViewer_t));

    viewer->show_status = true;
    viewer->show_title = true;
}

CPageViewer_t * CPageViewer_New(void)
{
    CPageViewer_t *viewer = (CPageViewer_t *) Q_malloc(sizeof(CPageViewer_t));

    CPageViewer_Init(viewer);
    return viewer;
}

// load document by url
void CPageViewer_GoUrl(CPageViewer_t *viewer, char *url)
{
    AddPage(viewer);

    viewer->page->url = Q_strdup(url);
    viewer->page->should_render = true;
}

// load document by xml_document_t
void CPageViewer_Go(CPageViewer_t *viewer, char *url, xml_document_t *doc)
{
    AddPage(viewer);

    viewer->page->url = Q_strdup(url);
    viewer->page->doc = doc;
    viewer->page->should_render = true;
}

void CPageViewer_Draw(CPageViewer_t *viewer, int x, int y, int w, int h)
{
    int i;
    document_rendered_link_t *link;
    int line;
    char buf[512];
    int sx, sy, sw, sh;

    // change x, y, w, h
    x = x + (w - 8*(w/8)) /2;
    w = w/8;
    y = y + (h - 8*(h/8)) /2;
    h = h/8;

    if (w < 20  ||  h < 10)
        return;

    if (viewer->page == NULL)
        return;

    sx = x;
    sy = y;
    sw = w;
    sh = h;

    if (viewer->page->width != w)
        viewer->page->should_render = true;

    if (viewer->page->should_render)
        RenderDocument(viewer, w);

    // current link, if in navigation mode
    link = viewer->navigation_mode ? viewer->current_links[viewer->current_link_index] : NULL;

    // draw title bar
    /*
    if (viewer->page->rendered.title  &&  viewer->show_title)
    {
        for (line = 0; line < viewer->page->rendered.title_lines; line++)
        {
            MakeString(buf, viewer->page->rendered.title + line*viewer->page->width, viewer->page->width);
            UI_Print(x, y + line*8, buf, false);
        }

        UI_MakeLine(buf, viewer->page->width/2);
        UI_Print_Center(x, y + line*8, viewer->page->width*8, buf, false);

        sh -= line + 1;
        sy += (line + 1) * 8;
    }
    */

    // calculate scrollable area yet
    if (viewer->show_status)
        sh -= 2;
    if (viewer->page->rendered.text_lines - viewer->page->current_line < sh)
        viewer->page->current_line -= sh - (viewer->page->rendered.text_lines - viewer->page->current_line);
    if (viewer->page->current_line < 0)
        viewer->page->current_line = 0;

    // draw status bar
    if (viewer->show_status)
    {
        if (viewer->navigation_mode)
        {
            if (strlen (link->tag->href) > w)
            {
                strlcpy (buf, link->tag->href, min (sizeof (buf), w-3));
                strlcat (buf, "...", sizeof (buf));
            }
            else
            {
                int offset = (w - strlen(link->tag->href)) / 2;
                memset(buf, ' ', offset);
                strlcpy (buf + offset, link->tag->href, sizeof (buf) - offset);
            }
            UI_Print(x, y + (h-1)*8, buf, false);
        }
        else
        {
            snprintf(buf, sizeof (buf), "%d lines  ", viewer->page->rendered.text_lines);
            if (sh >= viewer->page->rendered.text_lines)
                strlcat(buf, "[full]", sizeof (buf));
            else if (viewer->page->current_line == 0)
                strlcat(buf, "[top]", sizeof (buf));
            else if (viewer->page->current_line + sh == viewer->page->rendered.text_lines)
                strlcat(buf, "[bottom]", sizeof (buf));
            else
            {
                int percent = (100*(viewer->page->current_line + sh)) / viewer->page->rendered.text_lines;
                strlcat(buf, va("[%d%%]", percent), sizeof (buf));
            }

            UI_Print(x+8*(w-strlen(buf)-1), y + (h-1)*8, buf, false);

            if (viewer->page->doc  &&  viewer->page->doc->title)
            {
                int l = w - strlen(buf) - 3;
                if (strlen(viewer->page->doc->title) <= l)
                {
                    UI_Print(x+8, y + (h-1)*8, viewer->page->doc->title, false);
                }
                else
                {
                    strlcpy(buf, viewer->page->doc->title, min (sizeof (buf), l-3));
                    strlcat(buf, "...", sizeof (buf));
                    UI_Print(x+8, y + (h-1)*8, buf, false);
                }
            }
        }
        UI_MakeLine(buf, w);
        UI_Print_Center(x, y + (h-2)*8, w*8, buf, false);
    }

    // process with scrollable area
    for (line = 0;  line < min(sh, viewer->page->rendered.text_lines - viewer->page->current_line); line++)
    {
        int start, end;
	MakeString(buf, (byte *) viewer->page->rendered.text + (line+viewer->page->current_line)*viewer->page->width, viewer->page->width);

        if (link)
        {
            start = max(link->start, (viewer->page->current_line + line) * viewer->page->width);
            end = min(link->end, (viewer->page->current_line + line + 1) * viewer->page->width);
            if (end > start)
            {
                start -= (viewer->page->current_line + line) * viewer->page->width;
                end -= (viewer->page->current_line + line) * viewer->page->width;
                for (i=start; i < end; i++)
                    buf[i] ^= 128;
            }
        }

        UI_Print(sx, sy + line*8, buf, false);
    }
    viewer->page->last_height = sh;
}

qbool CPageViewer_Key(CPageViewer_t *viewer, int key, wchar unichar)
{
    qbool ret = false;

    if (viewer->page == NULL)
        return 0;

    if (viewer->navigation_mode)
    {
        char *href;
        switch (key)
        {
        case K_TAB:
            viewer->current_link_index += isShiftDown() ? -1 : 1;
            viewer->current_link_index = (viewer->current_link_index +
                viewer->current_links_total) % viewer->current_links_total;
            ret = true;
            break;
        case K_ENTER:
            href = viewer->current_links[viewer->current_link_index]->tag->href;            if (href)
            {
                LeaveNavigationMode(viewer);
                CPageViewer_GoUrl(viewer, href);
            }            ret = true;
            break;
        case K_SHIFT:
            break;
        default:
            LeaveNavigationMode(viewer);
            ret = true;
            break;
        }
    }

    if (!viewer->navigation_mode)
    {
        switch (key)
        {
        case K_UPARROW:
		case K_MWHEELUP:
            viewer->page->current_line--;
            ret = true;
            break;
        case K_DOWNARROW:
		case K_MWHEELDOWN:
            viewer->page->current_line++;
            ret = true;
            break;
        case K_PGUP:
            viewer->page->current_line -= viewer->page->last_height-1;
            ret = true;
            break;
        case K_PGDN:
            viewer->page->current_line += viewer->page->last_height-1;
            ret = true;
            break;
        case K_HOME:
            viewer->page->current_line = 0;
            ret = true;
            break;
        case K_END:
            viewer->page->current_line = viewer->page->rendered.text_lines;
            ret = true;
            break;
        case K_BACKSPACE:
            CPageViewer_Back(viewer, 1);
            ret = true;
            break;
        case K_TAB:
            EnterNavigationMode(viewer);
            ret = true;
            break;
        }
    }
    return ret;
}

// go back
void CPageViewer_Back(CPageViewer_t *viewer, int level)
{
    if (viewer->page == NULL)
        return;

    while (viewer->page->next && level--)
    {
        CPageViewer_page_t *page = viewer->page->next;

        FreePage(viewer->page);
        viewer->page = page;
    }
}

void CPageViewer_Clear(CPageViewer_t *viewer)
{
    CPageViewer_page_t *next;

    LeaveNavigationMode(viewer);

    // free all pages
    while (viewer->page)
    {
        next = viewer->page->next;

        FreePage(viewer->page);
        viewer->page = next;
    }

    // clear
    CPageViewer_Init(viewer);
}

void CPageViewer_Free(CPageViewer_t *viewer)
{
    // clear all data
    CPageViewer_Clear(viewer);

    // and free 'this'
    Q_free(viewer);
}
