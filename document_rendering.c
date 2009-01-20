#include "quakedef.h"


// document constants
#define DOCUMENT_INDENT     2


#define LINE_MAX_LEN    512

#include "expat.h"
#include "xsd.h"
#include "document_rendering.h"

typedef struct document_rendering_context_s
{
    // current buffer status
    int     line;
    int     line_pos;
    byte    *buf;
    int     nocopy;
    int     nometadata;
    int     width;

    // link table
    document_rendered_link_t *links;
    document_rendered_link_t *inline_links; // current inline links

    // section table
    document_rendered_section_t *sections;
    document_rendered_section_t *current_section;

    // where last blank line was added
    int     last_blank;

    // visual settings
    int     l_margin;
    int     r_margin;

    // shared line buffer
    char line_buf[LINE_MAX_LEN];

    // some state markers
    int     list_count;
    document_align_t align;
    int     do_color;
}
document_rendering_context_t;


char * Add_Inline_Tag(document_rendering_context_t *cx, char *text, document_tag_t *tag);
char *Add_Inline_String(char *text, char *string);
static void RenderBlockChain(document_rendering_context_t *cx, document_tag_t *tag);





// check if given tag is a block rather than inline
static int IsBlockElement(document_tag_t *tag)
{
    switch (tag->type)
    {
    case tag_section:
    case tag_p:
    case tag_list:
    case tag_dict:
    case tag_hr:
    case tag_pre:
    case tag_h:
        return 1;

    default:
        return 0;
    }
}

// finish current line and add it to the buffer
static void LineFeed(document_rendering_context_t *cx)
{
    if (cx->line_pos != cx->l_margin)
    {
        // check alignment
        if (cx->align != align_left)
        {
            int l, r;
            int offset;

            l = cx->l_margin;
            r = cx->width - cx->r_margin;
            while ((cx->line_buf[r-1] == 0  ||  cx->line_buf[r-1] == ' ')  &&
                r > l)
                r--;

            offset = 0;
            if (cx->align == align_right)
            {
                offset = cx->width-cx->r_margin-r;
            }
            if (cx->align == align_center)
            {
                offset = (cx->width-cx->r_margin-cx->l_margin - (r - l)) / 2;
            }

            if (offset)
            {
                document_rendered_link_t *l;

                memmove(cx->line_buf+offset, cx->line_buf, r);
                memset(cx->line_buf, 0, offset);

                // change links
                l = cx->inline_links;
                while (l)
                {
                    if (l->start >= cx->line*cx->width  &&  l->start < (cx->line+1)*cx->width)
                        l->start += offset;
                    if (l->end >= cx->line*cx->width  &&  l->end < (cx->line+1)*cx->width)
                        l->end += offset;

                    l = l->next;
                }
            }
        }

        // copy to buffer
        if (!cx->nocopy)
            memcpy(cx->buf + cx->line*cx->width, cx->line_buf, cx->width);

        // manage state variables
        cx->line ++;
        memset(cx->line_buf, 0, LINE_MAX_LEN);
        cx->line_pos = cx->l_margin;
    }
}

// add blank line, checking if there aren't any already
static void AddBlankLine(document_rendering_context_t *cx)
{
//    int i;

    // do not add blank as the first line
    if (cx->line == 0)
        goto skip;

    // check if previous line is not blank
    if (cx->line-1 == cx->last_blank)
        goto skip;

    // add line
    memset(cx->line_buf, 0, LINE_MAX_LEN);
    cx->last_blank = cx->line;
    cx->line ++;

skip:

    cx->line_pos = cx->l_margin;
}

// add link to the inline link table
static void AddInlineLink(document_rendering_context_t *cx, document_rendered_link_t *link)
{
    if (cx->nometadata)
    {
        Q_free(link);
    }
    else
    {
        document_rendered_link_t *last;

        last = cx->inline_links;

        if (last == NULL)
            cx->inline_links = link;
        else
        {
            while (last->next)
                last = last->next;
            last->next = link;
        }
    }
}

// strip spaces from inline text, making links working
char * StripInlineSpaces(char *str, document_rendered_link_t *links)
{
    int cut = 0;

    char *buf, *ret;
    int p=0, q=0;

    if (str == NULL)
        return str;

    buf = (char *) Q_malloc(strlen(str)+1);
    for (p=0; p < strlen(str); p++)
    {
        // offset links
        document_rendered_link_t *l = links;
        while (l)
        {
            if (l->start == p)
                l->start -= cut;
            if (l->end == p)
                l->end -= cut;

            l = l->next;
        }

        if (XSD_IsSpace(str[p]))
        {
            if (q == 0  ||  XSD_IsSpace(buf[q-1]))
                cut++;
            else
            {
                // add this char, but replace with pure space
                buf[q++] = ' ';
            }
        }
        else
            buf[q++] = str[p];
    }

    // strip spaces from the end
    while (q > 0  &&  XSD_IsSpace(buf[q-1]))
        q--;
    buf[q] = 0;

	ret = (char *) Q_strdup(buf);
    Q_free(buf);
    Q_free(str);
    return ret;
}

static char ChangeColor(char c)
{
    char low = c & 127;

    if (low > ' '  &&  low <= '~')
        return c ^ 128;

    return c;
}

static void Render_String(document_rendering_context_t *cx, char *text)
{
    int l;
    char c;
    document_rendered_link_t *link;

    char *txt = text;
    int t_width = cx->width - cx->l_margin - cx->r_margin;

    if (t_width < 8)    // witdth of text
        return;

    if (txt == NULL)
        return;

    // make links negative
    link = cx->inline_links;
    while (link)
    {
        link->start *= -1;
        link->end *= -1;
        link = link->next;
    }

    while ((c = *txt))
    {
        // count word length
        for (l=0 ; l < t_width; l++)
            if (isspace2(txt[l]) || txt[l]=='\n'+(char)128  ||  !txt[l])
                break;

        // word wrap
        if (l != t_width && (cx->line_pos + l > cx->width - cx->r_margin) )
            LineFeed(cx);

        // recalculate links indexes to absolute
        link = cx->inline_links;
        while (link)
        {
            int i = txt - text;

            if (link->start <= 0  &&  i >= -link->start)
                link->start = cx->line*cx->width + cx->line_pos;
            if (link->end <= 0  &&  i >= -link->end)
                link->end = cx->line*cx->width + cx->line_pos;
            link = link->next;
        }
        
        txt++;

        switch (c)
        {
        case (char)('\n' | 128):
            LineFeed(cx);
            break;

        case (char)(' ' | 128):
            cx->line_buf[cx->line_pos++] = ' ';
            break;

        default:
            cx->line_buf[cx->line_pos++] = c;
        }

        if (cx->line_pos >= cx->width - cx->r_margin)
        {
            LineFeed(cx);

            // we linefeed because of no space, so skip spaces which are next
            while (isspace2(*txt))
                txt++;
        }
    }
    LineFeed(cx);

    // add inline links to document links
    if (cx->links == NULL)
        cx->links = cx->inline_links;
    else
    {
        link = cx->links;
        while (link->next)
            link = link->next;
        link->next = cx->inline_links;
    }
    cx->inline_links = NULL;
}

// render P tag
static void Render_P(document_rendering_context_t *cx, document_tag_p_t *p)
{
    document_align_t old_align = cx->align;
    cx->align = p->align;

    if (p->indent)
    {
        cx->l_margin += DOCUMENT_INDENT;
        AddBlankLine(cx);
    }

    RenderBlockChain(cx, p->tags);

    if (p->indent)
    {
        cx->l_margin -= DOCUMENT_INDENT;
        AddBlankLine(cx);
    }

    cx->align = old_align;
}

// render Section tag
static void Render_Section(document_rendering_context_t *cx, document_tag_section_t *p)
{
    if (p->indent)
    {
        cx->l_margin += DOCUMENT_INDENT;
        AddBlankLine(cx);
    }

    RenderBlockChain(cx, p->tags);

    if (p->indent)
    {
        cx->l_margin -= DOCUMENT_INDENT;
        AddBlankLine(cx);
    }
}

// render H tag
static void Render_H(document_rendering_context_t *cx, document_tag_h_t *h)
{
    document_align_t old_align;
    int old_do_color;
    
    old_align = cx->align;
    cx->align = h->align;

    old_do_color = cx->do_color;
    cx->do_color = 1;

    RenderBlockChain(cx, h->tags);

    cx->do_color = old_do_color;
    cx->align = old_align;
}

// render PRE tag
static void Render_Pre(document_rendering_context_t *cx, document_tag_pre_t *pre)
{
    int     l_width, width;
    char    *text, *s, *d;
    
    if (!pre->text)
        return;

    // duplicate text and calculate width
    width = l_width = 0;
    s = pre->text;
    d = text = (char *) Q_malloc(strlen(s)+1);
    while (*s)
    {
        char c = *s++;

        // skip LF and TAB
        if (c == '\r'  ||  c == '\t')
            continue;

        // copy character to output
        *d++ = c=='\n' ? '\n'+128 : c;

        // check line width
        if (c == '\n')
        {
            if (l_width > width)
                width = l_width;
            l_width = 0;
        }
        else
        {
            l_width++;
        }
    }
    *d = 0;

    // check if our render context width allow for such image
    if (width <= cx->width - cx->l_margin - cx->r_margin - 1)
    {
        // render inline

        document_align_t align;
        int offset;

        // remporarily disable alignment, because we use our own
        align = cx->align;
        cx->align = align_left;

        // calculate alignment offset
        switch (align)
        {
        default:
        case align_left:
            offset = 0;
            break;
        case align_right:
            offset = cx->width - cx->l_margin - cx->r_margin - width - 1;
            break;
        case align_center:
            offset = (cx->width - cx->l_margin - cx->r_margin - width - 1) / 2;
            break;
        }

        cx->l_margin += offset;
        AddBlankLine(cx);
        Render_String(cx, text);

        cx->l_margin -= offset;
        AddBlankLine(cx);

        cx->align = align;
    }
    else
    {
        // render as link

        char *t = Q_strdup("");

        t = Add_Inline_String(t, "\x90");
        t = Add_Inline_String(t, pre->alt ? pre->alt : "picture");
        t = Add_Inline_String(t, "\x91");

        Render_String(cx, t);

        Q_free(t);
    }

    Q_free(text);
}

// render HR tag
static void Render_Hr(document_rendering_context_t *cx, document_tag_hr_t *hr)
{
    int i;

    document_align_t old_align = cx->align;
    cx->align = align_center;

    AddBlankLine(cx);

    cx->line_buf[cx->line_pos++] = 29;
    for (i=0; i < (cx->width - cx->l_margin - cx->r_margin) / 2; i++)
        cx->line_buf[cx->line_pos++] = 30;
    cx->line_buf[cx->line_pos++] = 31;

    LineFeed(cx);

    AddBlankLine(cx);

    cx->align = old_align;
}

// render LIST tag
static void Render_List(document_rendering_context_t *cx, document_tag_list_t *list)
{
    char separator[128];
    int indent;
    document_tag_li_t *item;
    int item_num;
    int num_width = 0;

    // first check, how much indentation we should use
    indent = 2;
    if (list->separator != list_separator_none)
        indent++;
    switch (list->bullet)
    {
    case list_bullet_none:
        break;

    case list_bullet_dot:
    case list_bullet_letter:
    case list_bullet_bigletter:
        indent++;
        break;

    case list_bullet_number:
        {
            // count list size
            item_num = 0;
            item = list->items;
            while (item)
            {
                item_num++;
                item = (document_tag_li_t *) item->next;
            }
            if (item_num == 0)
                break;
            num_width = log10(item_num) + 1;
            indent += num_width;
        }
    }

    cx->list_count++;
    cx->l_margin += indent;
    AddBlankLine(cx);

    item = list->items;
    item_num = 1;

    while (item)
    {
        // make separator
        strlcpy (separator, " ", sizeof (separator));
        switch (list->bullet)
        {
        case list_bullet_none:
            break;
        case list_bullet_dot:
            strlcat (separator, va("%c", (cx->list_count%2) ? 143 : 15), sizeof (separator));
            break;
        case list_bullet_letter:
            strlcat(separator, va("%c", (item_num % ('z'-'a'+1)) + 'a'), sizeof (separator));
            break;
        case list_bullet_bigletter:
            strlcat(separator, va("%c", (item_num % ('Z'-'A'+1)) + 'A'), sizeof (separator));
            break;
        case list_bullet_number:
            strlcat(separator, va("%*d", num_width, item_num), sizeof (separator));
            break;
        }
        switch (list->separator)
        {
        case list_separator_none:
            break;
        case list_separator_dot:
            strlcat(separator, ".", sizeof (separator));
            break;
        case list_separator_par:
            strlcat(separator, ")", sizeof (separator));
            break;
        }
        strlcat(separator, " ", sizeof (separator));

        memcpy(cx->line_buf + (cx->l_margin - indent), separator, strlen(separator));
        RenderBlockChain(cx, item->tags);

        item_num++;
        item = (document_tag_li_t *) item->next;
    }

    cx->l_margin -= indent;
    AddBlankLine(cx);
    cx->list_count--;
}

// render LIST tag
static void Render_Dict(document_rendering_context_t *cx, document_tag_dict_t *dict)
{
    char separator[128];
    int indent;
    document_tag_di_t *item;
    int item_num;
    int num_width = 0;

    // first check, how much indentation we should use
    indent = 2;
    if (dict->separator != list_separator_none)
        indent++;
    switch (dict->bullet)
    {
    case list_bullet_none:
        break;

    case list_bullet_dot:
    case list_bullet_letter:
    case list_bullet_bigletter:
        indent++;
        break;

    case list_bullet_number:
        {
            // count dict size
            item_num = 0;
            item = dict->items;
            while (item)
            {
                item_num++;
                item = (document_tag_di_t *) item->next;
            }
            if (item_num == 0)
                break;
            num_width = log10(item_num) + 1;
            indent += num_width;
        }
    }

    cx->list_count++;
    cx->l_margin += indent;
    AddBlankLine(cx);

    item = dict->items;
    item_num = 1;

    while (item)
    {
        int old_do_color;

        // make separator
        strlcpy(separator, " ", sizeof (separator));
        switch (dict->bullet)
        {
        case list_bullet_none:
            break;
        case list_bullet_dot:
            strlcat(separator, va("%c", (cx->list_count%2) ? 143 : 15), sizeof (separator));
            break;
        case list_bullet_letter:
            strlcat(separator, va("%c", (item_num % ('z'-'a'+1)) + 'a'), sizeof (separator));
            break;
        case list_bullet_bigletter:
            strlcat(separator, va("%c", (item_num % ('Z'-'A'+1)) + 'A'), sizeof (separator));
            break;
        case list_bullet_number:
            strlcat(separator, va("%*d", num_width, item_num), sizeof (separator));
            break;
        }
        switch (dict->separator)
        {
        case list_separator_none:
            break;
        case list_separator_dot:
            strlcat(separator, ".", sizeof (separator));
            break;
        case list_separator_par:
            strlcat(separator, ")", sizeof (separator));
            break;
        }
        strlcat(separator, " ", sizeof (separator));

        memcpy(cx->line_buf + (cx->l_margin - indent), separator, strlen(separator));
        old_do_color = cx->do_color;
        cx->do_color = 1;
        RenderBlockChain(cx, item->name);
        cx->do_color = old_do_color;

        // render description
        cx->l_margin += 2;
        AddBlankLine(cx);
        RenderBlockChain(cx, item->description);
        cx->l_margin -= 2;
        AddBlankLine(cx);

        item_num++;
        item = (document_tag_di_t *) item->next;
    }

    cx->l_margin -= indent;
    AddBlankLine(cx);
    cx->list_count--;
}


// adds string
char *Add_Inline_String (char *text, char *string)
{
	size_t size = strlen (text) + strlen (string) + 1;
    char *buf = (char *) Q_malloc (size);
    strlcpy (buf, text, size);
    Q_free(text);
    strlcat (buf, string, size);
    return buf;
}

// adds text element
char *Add_Inline_Text (document_rendering_context_t *cx, char *text, document_tag_text_t *tag)
{
	char *s, *d;
	char *buf;
	size_t size;

	size = strlen (text) + strlen (tag->text) + 1;
	buf = (char *) Q_malloc (size);
	strlcpy (buf, text, size);
	Q_free (text);

	d = buf + strlen(buf);
	s = tag->text;

	while (*s)
	{
		char c = *s++;

		if (cx->do_color)
			c = ChangeColor(c);

		*d++ = c;
	}

	*d = 0;

	return buf;
}

// adds EM element
char * Add_Inline_Em(document_rendering_context_t *cx, char *text, document_tag_em_t *tag)
{
    text = Add_Inline_String(text, "_");
    text = Add_Inline_Tag(cx, text, tag->tags);
    text = Add_Inline_String(text, "_");
    return text;
}

// adds COLOR element
char * Add_Inline_Color(document_rendering_context_t *cx, char *text, document_tag_color_t *tag)
{
    int old_do_color = cx->do_color;
    cx->do_color = true;

    text = Add_Inline_Tag(cx, text, tag->tags);

    cx->do_color = old_do_color;
    return text;
}

// adds A element
char * Add_Inline_A(document_rendering_context_t *cx, char *text, document_tag_a_t *tag)
{
    // create new link object
    document_rendered_link_t *link;

    link = (document_rendered_link_t *) Q_malloc(sizeof(document_rendered_link_t));
    memset(link, 0, sizeof(document_rendered_link_t));

    link->tag = tag;
    link->start = strlen(text) + 1;

    text = Add_Inline_String(text, "\x90");
    text = Add_Inline_Tag(cx, text, tag->tags);
    text = Add_Inline_String(text, "\x91");

    link->end = strlen(text) - 1;
    AddInlineLink(cx, link);

    return text;
}

// adds IMG element
char * Add_Inline_Img(document_rendering_context_t *cx, char *text, document_tag_img_t *tag)
{
    text = Add_Inline_String(text, "\x90");
    text = Add_Inline_Tag(cx, text, tag->tags);
    text = Add_Inline_String(text, "\x91");
    return text;
}

// adds BR element
char * Add_Inline_Br(document_rendering_context_t *cx, char *text, document_tag_br_t *tag)
{
    text = Add_Inline_String(text, va("%c", '\n' | 128));
    return text;
}

// adds SP element
char * Add_Inline_Sp(document_rendering_context_t *cx, char *text, document_tag_sp_t *tag)
{
    text = Add_Inline_String(text, va("%c", ' ' | 128));
    return text;
}

// add inline element
char * Add_Inline_Tag(document_rendering_context_t *cx, char *text, document_tag_t *tag)
{
    if (tag == NULL)
        return text;

    switch (tag->type)
    {
    case tag_text:
        text = Add_Inline_Text(cx, text, (document_tag_text_t *) tag);
        break;

    case tag_em:
        text = Add_Inline_Em(cx, text, (document_tag_em_t *) tag);
        break;

    case tag_color:
        text = Add_Inline_Color(cx, text, (document_tag_color_t *) tag);
        break;

    case tag_a:
        text = Add_Inline_A(cx, text, (document_tag_a_t *) tag);
        break;

    case tag_img:
        text = Add_Inline_Img(cx, text, (document_tag_img_t *) tag);
        break;

    case tag_br:
        text = Add_Inline_Br(cx, text, (document_tag_br_t *) tag);
        break;

    case tag_sp:
        text = Add_Inline_Sp(cx, text, (document_tag_sp_t *) tag);
        break;

    case tag_di:
    case tag_dict:
    case tag_h:
    case tag_hr:
    case tag_li:
    case tag_list:
    case tag_p:
    case tag_pre:
    case tag_section:
	/* TODO */
	break;
    }

    return text;
}

// renders inline chain, stops at first non-inline tag and returns this tag
static document_tag_t * RenderInlineChain(document_rendering_context_t *cx, document_tag_t *tag)
{
    char *text = Q_strdup("");

    while (tag && !IsBlockElement(tag))
    {
        text = Add_Inline_Tag(cx, text, tag);

        tag = tag->next;
    }

    text = StripInlineSpaces(text, cx->inline_links);
    Render_String(cx, text);
    LineFeed(cx);

    Q_free(text);

    return tag;
}

// render block chain
static void RenderBlockChain(document_rendering_context_t *cx, document_tag_t *tag)
{
    while (tag)
    {
        document_tag_t *next = tag->next;

        AddBlankLine(cx);

        if (IsBlockElement(tag))
        {
            // process tag with appropiate renderer
            switch (tag->type)
            {
            case tag_section:
                Render_Section(cx, (document_tag_section_t *) tag);
                break;

            case tag_p:
                Render_P(cx, (document_tag_p_t *) tag);
                break;

            case tag_list:
                Render_List(cx, (document_tag_list_t *) tag);
                break;

            case tag_dict:
                Render_Dict(cx, (document_tag_dict_t *) tag);
                break;

            case tag_hr:
                Render_Hr(cx, (document_tag_hr_t *) tag);
                break;

            case tag_pre:
                Render_Pre(cx, (document_tag_pre_t *) tag);
                break;

            case tag_h:
                Render_H(cx, (document_tag_h_t *) tag);
                break;

            default:    // skip this tag
                break;
            }
        }
        else
        {
            next = RenderInlineChain(cx, tag);
        }

        AddBlankLine(cx);

        tag = next;
    }
}

// renders document into memory buffer,
// returns total lines processed,
// with nocopy=true does not affect output buffer (it may be NULL),
// which may be used for calculating needed buffer size
static int XSD_RenderDocumentOnce(xml_document_t *doc, byte *buf, int width, int lines,
                                  document_rendered_link_t **link, document_rendered_section_t **section)
{
    document_rendering_context_t context;

//    int i;
//    int l;
//    int total_lines = 0;
//    qbool first = true;
//    document_tag_p_t *tag = (document_tag_p_t *) doc->content;
//    byte line[1024];

    memset(&context, 0, sizeof(context));
    context.buf = buf;
    context.line = 0;
    context.line_pos = 0;
    context.nocopy = lines==0 ? 1 : 0;
    context.nometadata = link==NULL  ||  section==NULL;
    context.last_blank = -1;

    context.l_margin = 0;
    context.r_margin = 0;
    context.width = width;

    context.list_count = 0;
    context.align = align_left;
    context.do_color = 0;

    memset(context.line_buf, 0, LINE_MAX_LEN);

    // zero the buffer
    if (lines > 0)
        memset(buf, 0, lines * width);

    RenderBlockChain(&context, doc->content);

    // skip last blank line
    if (context.line - 1 == context.last_blank)
        context.line--;

    // set links
    if (!context.nometadata)
    {
        *link = context.links;
        *section = context.sections;
    }

    return context.line;
}

// renders document into memory buffer,
int XSD_RenderDocument(document_rendered_t *ret, xml_document_t *doc, int width)
{
    int lines;

    memset(ret, 0, sizeof(document_rendered_t));

    // render document title
    if (doc->title)
    {
        int lines;
        document_tag_text_t *text;
        document_tag_p_t *p;
        xml_document_t *tdoc;

        tdoc = XSD_Document_New();

        // create p tag
        p = (document_tag_p_t *) Q_malloc(sizeof(document_tag_p_t));
        memset(p, 0, sizeof(document_tag_p_t));
        p->type = tag_p;
        p->align = align_center;

        tdoc->content = (document_tag_t *) p;

        // create text tag
        text = (document_tag_text_t *) Q_malloc(sizeof(document_tag_text_t));
        memset(text, 0, sizeof(document_tag_text_t));
        text->type = tag_text;
        text->text = Q_strdup(doc->title);

        p->tags = (document_tag_t *) text;

        lines = XSD_RenderDocumentOnce(tdoc, NULL, width, 0, NULL, NULL);
        if (lines > 0)
        {
            ret->title = (char *) Q_malloc(lines*width);
	    ret->title_lines = XSD_RenderDocumentOnce(tdoc,(byte *) ret->title, width, lines, NULL, NULL);
        }

        XSD_Document_Free((xml_t *)tdoc);
    }

    // render document body
    lines = XSD_RenderDocumentOnce(doc, NULL, width, 0, NULL, NULL);
    if (lines <= 0)
        goto error;
    ret->text = (char *) Q_malloc(lines*width);
    ret->text_lines = XSD_RenderDocumentOnce(doc,(byte *) ret->text, width, lines, &ret->links, &ret->sections);
    return 1;

error:
    XSD_RenderClear(ret);
    return 0;
}

void RenderFreeSection(document_rendered_section_t *section)
{
    document_rendered_section_t *next;

    if (section == NULL)
        return;

    while (section)
    {
        next = section->next;

        RenderFreeSection(section->children);
        Q_free(section);

        section = next;
    }
}

// free rendered document
void XSD_RenderClear(document_rendered_t *r)
{
    document_rendered_link_t *link, *next;

    if (r->text)
        Q_free(r->text);
    if (r->title)
        Q_free(r->title);
    
    link = r->links;
    while (link)
    {
        next = link->next;

        Q_free(link);
        link = next;
    }

    RenderFreeSection(r->sections);
}
