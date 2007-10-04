// $Id: xsd_document.c,v 1.9 2007-10-04 14:56:54 dkure Exp $

#include "quakedef.h"
#include "expat.h"
#include "xsd.h"
#include "xsd_document.h"


// general parser
static void OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts);
static void OnEndElement(void *userData, const XML_Char *name);
static void OnCharacterData(void *userData, const XML_Char *s, int len);

// BLOCKS parser
static void OnStartElement_Blocks(void *userData, const XML_Char *name, const XML_Char **atts);
static void OnEndElement_Blocks(void *userData, const XML_Char *name);
static void OnCharacterData_Blocks(void *userData, const XML_Char *s, int len);


// init xml_document_t struct
static void XSD_Document_Init(xml_document_t *document)
{
    memset(document, 0, sizeof(xml_document_t));
}

// create new document
xml_document_t * XSD_Document_New(void)
{
    xml_document_t *doc = (xml_document_t *) Q_malloc(sizeof(xml_document_t));
    XSD_Document_Init(doc);
    doc->document_type = Q_strdup("document");
    return doc;
}

static void XSD_Free_Tags(document_tag_t *t);

// free document_tag_p_t
static void XSD_Free_Tag_P(document_tag_p_t *p)
{
    XSD_Free_Tags(p->tags);
    Q_free(p);
}

// free document_tag_section_t
static void XSD_Free_Tag_Section(document_tag_section_t *s)
{
    if (s->id)
        Q_free(s->id);
    if (s->title)
        Q_free(s->title);
    XSD_Free_Tags(s->tags);
    Q_free(s);
}

// free document_tag_em_t
static void XSD_Free_Tag_Em(document_tag_em_t *i)
{
    XSD_Free_Tags(i->tags);
    Q_free(i);
}

// free document_tag_color_t
static void XSD_Free_Tag_Color(document_tag_color_t *c)
{
    XSD_Free_Tags(c->tags);
    Q_free(c);
}

// free document_tag_a_t
static void XSD_Free_Tag_A(document_tag_a_t *a)
{
    if (a->href)
        Q_free(a->href);
    XSD_Free_Tags(a->tags);
    Q_free(a);
}

// free document_tag_img_t
static void XSD_Free_Tag_Img(document_tag_img_t *i)
{
    XSD_Free_Tags(i->tags);
    Q_free(i);
}

// free document_tag_br_t
static void XSD_Free_Tag_Br(document_tag_br_t *br)
{
    Q_free(br);
}

// free document_tag_sp_t
static void XSD_Free_Tag_Sp(document_tag_sp_t *sp)
{
    Q_free(sp);
}

// free document_tag_hr_t
static void XSD_Free_Tag_Hr(document_tag_hr_t *hr)
{
    Q_free(hr);
}

// free document_tag_pre_t
static void XSD_Free_Tag_Pre(document_tag_pre_t *pre)
{
    if (pre->text)
        Q_free(pre->text);
    if (pre->alt)
        Q_free(pre->alt);
    Q_free(pre);
}

// free document_tag_h_t
static void XSD_Free_Tag_H(document_tag_h_t *h)
{
    XSD_Free_Tags(h->tags);
    Q_free(h);
}

// free document_tag_list_t
static void XSD_Free_Tag_List(document_tag_list_t *l)
{
    XSD_Free_Tags((document_tag_t *)l->items);
    Q_free(l);
}

// free document_tag_li_t
static void XSD_Free_Tag_Li(document_tag_li_t *l)
{
    XSD_Free_Tags(l->tags);
    Q_free(l);
}

// free document_tag_list_t
static void XSD_Free_Tag_Dict(document_tag_dict_t *l)
{
    XSD_Free_Tags((document_tag_t *)l->items);
    Q_free(l);
}

// free document_tag_di_t
static void XSD_Free_Tag_Di(document_tag_di_t *l)
{
    XSD_Free_Tags(l->name);
    XSD_Free_Tags(l->description);
    Q_free(l);
}

// free document_tag_text_t tag
static void XSD_Free_Tag_Text(document_tag_text_t *t)
{
    if (t->text)
        Q_free(t->text);
    Q_free(t);
}

// free tag chain
static void XSD_Free_Tags(document_tag_t *t)
{
    while (t)
    {
        document_tag_t *next = t->next;

        switch (t->type)
        {
        case tag_text:
            XSD_Free_Tag_Text((document_tag_text_t *) t);
            break;

        case tag_section:
            XSD_Free_Tag_Section((document_tag_section_t *) t);
            break;

        case tag_p:
            XSD_Free_Tag_P((document_tag_p_t *) t);
            break;

        case tag_hr:
            XSD_Free_Tag_Hr((document_tag_hr_t *) t);
            break;

        case tag_pre:
            XSD_Free_Tag_Pre((document_tag_pre_t *) t);
            break;

        case tag_h:
            XSD_Free_Tag_H((document_tag_h_t *) t);
            break;

        case tag_list:
            XSD_Free_Tag_List((document_tag_list_t *) t);
            break;

        case tag_li:
            XSD_Free_Tag_Li((document_tag_li_t *) t);
            break;

        case tag_dict:
            XSD_Free_Tag_Dict((document_tag_dict_t *) t);
            break;

        case tag_di:
            XSD_Free_Tag_Di((document_tag_di_t *) t);
            break;

        case tag_em:
            XSD_Free_Tag_Em((document_tag_em_t *) t);
            break;

        case tag_color:
            XSD_Free_Tag_Color((document_tag_color_t *) t);
            break;

        case tag_a:
            XSD_Free_Tag_A((document_tag_a_t *) t);
            break;

        case tag_img:
            XSD_Free_Tag_Img((document_tag_img_t *) t);
            break;

        case tag_br:
            XSD_Free_Tag_Br((document_tag_br_t *) t);
            break;

        case tag_sp:
            XSD_Free_Tag_Sp((document_tag_sp_t *) t);
            break;

        default:
            assert(0);
        }

        t = next;
    }
}

// free xml_document_t struct
void XSD_Document_Free(xml_t *doc)
{
    xml_document_t *document = (xml_document_t *) doc;

    if (document->title)
        Q_free(document->title);

    XSD_Free_Tags(document->content);

    if (document->document_type)
        Q_free(document->document_type);

    // delete document
    Q_free(document);
}

// find last tag in chain
static document_tag_t *GetLast(document_tag_t *tag)
{
    if (tag == NULL)
        return NULL;

    while (tag->next)
        tag = tag->next;
    return tag;
}

// ----------------------------------------------------------------------------
//
// BLOCKS parser
//
static void OnStartElement_Blocks(void *userData, const XML_Char *name, const XML_Char **atts)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_document_t *document = (xml_document_t *) stack->document;
    document_parser_specific_t *specific = (document_parser_specific_t *) stack->parser_specific;

    if (stack->path[0] == 0)
    {
        if (!strcmp(name, "p"))
        {
            const char *attr;

            document_tag_p_t *p = (document_tag_p_t *) Q_malloc(sizeof(document_tag_p_t));
            memset(p, 0, sizeof(document_tag_p_t));
            p->type = tag_p;

            // check indentation
            attr = XSD_GetAttribute(atts, "indent");
            if (attr == NULL  ||  !strcmp(attr, "false"))
                p->indent = false;
            else
                p->indent = true;

            // check alignment
            attr = XSD_GetAttribute(atts, "align");
            p->align = align_left;
            if (attr != NULL)
            {
                if (!strcmp(attr, "right"))
                    p->align = align_right;
                else  if (!strcmp(attr, "center"))
                    p->align = align_center;
            }

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) p;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) p;

            // and now parse this element with new parser
            // create sub-parser
            {
                xml_parser_stack_t *newstack;
                document_parser_specific_t *specific;

                newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
                specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

                specific->tag = &p->tags;

                // prepare user data
                XSD_InitStack(newstack);
                newstack->parser_specific = specific;
                newstack->parser = stack->parser;
                newstack->document = (xml_t *) document;
                newstack->oldUserData = userData;
                newstack->oldStartHandler = OnStartElement_Blocks;
                newstack->oldEndHandler = OnEndElement_Blocks;
                newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

                XML_SetUserData(stack->parser, newstack);
                XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
                XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
                XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

                return;
            }
        }
        
        if (!strcmp(name, "section"))
        {
            const char *attr;

            document_tag_section_t *p = (document_tag_section_t *) Q_malloc(sizeof(document_tag_section_t));
            memset(p, 0, sizeof(document_tag_section_t));
            p->type = tag_section;

            // check indentation
            attr = XSD_GetAttribute(atts, "indent");
            if (attr == NULL  ||  !strcmp(attr, "false"))
                p->indent = false;
            else
                p->indent = true;

            // check id
            attr = XSD_GetAttribute(atts, "id");
            if (attr)
                p->id = Q_strdup(attr);
            else
                p->id = Q_strdup(va("%___%d", specific->section_num++));

            // check title
            attr = XSD_GetAttribute(atts, "title");
            if (attr)
                p->title = Q_strdup(attr);
            else
                p->title = Q_strdup("<unnamed>");

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) p;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) p;

            // and now parse this element with new parser
            // create sub-parser
            {
                xml_parser_stack_t *newstack;
                document_parser_specific_t *specific;

                newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
                specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

                specific->tag = &p->tags;

                // prepare user data
                XSD_InitStack(newstack);
                newstack->parser_specific = specific;
                newstack->parser = stack->parser;
                newstack->document = (xml_t *) document;
                newstack->oldUserData = userData;
                newstack->oldStartHandler = OnStartElement_Blocks;
                newstack->oldEndHandler = OnEndElement_Blocks;
                newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

                XML_SetUserData(stack->parser, newstack);
                XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
                XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
                XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

                return;
            }
        }
        
        if (!strcmp(name, "list"))
        {
            const char *attr;

            // create new list and add to tag list
            document_tag_list_t *list = (document_tag_list_t *) Q_malloc(sizeof(document_tag_list_t));
            memset(list, 0, sizeof(document_tag_list_t));
            list->type = tag_list;

            // check bullet type
            list->bullet = list_bullet_none;
            attr = XSD_GetAttribute(atts, "bullet");
            if (attr == NULL  ||  !strcmp(attr, "dot"))
                list->bullet = list_bullet_dot;
            else if (!strcmp(attr, "none"))
                list->bullet = list_bullet_none;
            else if (!strcmp(attr, "number"))
                list->bullet = list_bullet_number;
            else if (!strcmp(attr, "letter"))
                list->bullet = list_bullet_letter;
            else if (!strcmp(attr, "big-letter"))
                list->bullet = list_bullet_bigletter;

            // check separator type
            attr = XSD_GetAttribute(atts, "separator");
            list->separator = list_separator_none;
            if (attr == NULL  ||  !strcmp(attr, "none"))
                list->separator = list_separator_none;
            else if (!strcmp(attr, "dot"))
                list->separator = list_separator_dot;
            else if (!strcmp(attr, "par"))
                list->separator = list_separator_par;

            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) list;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) list;
        }

        if (!strcmp(name, "dict"))
        {
            const char *attr;

            // create new dict and add to tag list
            document_tag_dict_t *dict = (document_tag_dict_t *) Q_malloc(sizeof(document_tag_dict_t));
            memset(dict, 0, sizeof(document_tag_dict_t));
            dict->type = tag_dict;

            // check bullet type
            dict->bullet = list_bullet_none;
            attr = XSD_GetAttribute(atts, "bullet");
            if (attr == NULL  ||  !strcmp(attr, "none"))
                dict->bullet = list_bullet_none;
            else if (!strcmp(attr, "dot"))
                dict->bullet = list_bullet_dot;
            else if (!strcmp(attr, "number"))
                dict->bullet = list_bullet_number;
            else if (!strcmp(attr, "letter"))
                dict->bullet = list_bullet_letter;
            else if (!strcmp(attr, "big-letter"))
                dict->bullet = list_bullet_bigletter;

            // check separator type
            attr = XSD_GetAttribute(atts, "separator");
            dict->separator = list_separator_none;
            if (attr == NULL  ||  !strcmp(attr, "none"))
                dict->separator = list_separator_none;
            else if (!strcmp(attr, "dot"))
                dict->separator = list_separator_dot;
            else if (!strcmp(attr, "par"))
                dict->separator = list_separator_par;

            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) dict;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) dict;
        }

        if (!strcmp(name, "hr"))
        {
            document_tag_hr_t *hr = (document_tag_hr_t *) Q_malloc(sizeof(document_tag_hr_t));
            memset(hr, 0, sizeof(document_tag_hr_t));
            hr->type = tag_hr;

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) hr;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) hr;
        }

        if (!strcmp(name, "pre"))
        {
            const char *attr;

            document_tag_pre_t *pre = (document_tag_pre_t *) Q_malloc(sizeof(document_tag_pre_t));
            memset(pre, 0, sizeof(document_tag_pre_t));
            pre->type = tag_pre;

            // check alt
            attr = XSD_GetAttribute(atts, "alt");
            if (attr != NULL)
                pre->alt = Q_strdup(attr);

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) pre;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) pre;
        }

        if (!strcmp(name, "h"))
        {
            const char *attr;

            document_tag_h_t *h = (document_tag_h_t *) Q_malloc(sizeof(document_tag_h_t));
            memset(h, 0, sizeof(document_tag_h_t));
            h->type = tag_h;

            // check alignment
            attr = XSD_GetAttribute(atts, "align");
            h->align = align_left;
            if (attr != NULL)
            {
                if (!strcmp(attr, "right"))
                    h->align = align_right;
                else  if (!strcmp(attr, "center"))
                    h->align = align_center;
            }

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) h;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) h;

            // and now parse this element with new parser
            // create sub-parser
            {
                xml_parser_stack_t *newstack;
                document_parser_specific_t *specific;

                newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
                specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

                specific->tag = &h->tags;

                // prepare user data
                XSD_InitStack(newstack);
                newstack->parser_specific = specific;
                newstack->parser = stack->parser;
                newstack->document = (xml_t *) document;
                newstack->oldUserData = userData;
                newstack->oldStartHandler = OnStartElement_Blocks;
                newstack->oldEndHandler = OnEndElement_Blocks;
                newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

                XML_SetUserData(stack->parser, newstack);
                XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
                XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
                XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

                return;
            }
        }

        if (!strcmp(name, "em"))
        {
            document_tag_em_t *i = (document_tag_em_t *) Q_malloc(sizeof(document_tag_em_t));
            memset(i, 0, sizeof(document_tag_em_t));
            i->type = tag_em;

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) i;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) i;

            // and now parse this element with new parser
            // create sub-parser
            {
                xml_parser_stack_t *newstack;
                document_parser_specific_t *specific;

                newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
                specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

                specific->tag = &i->tags;

                // prepare user data
                XSD_InitStack(newstack);
                newstack->parser_specific = specific;
                newstack->parser = stack->parser;
                newstack->document = (xml_t *) document;
                newstack->oldUserData = userData;
                newstack->oldStartHandler = OnStartElement_Blocks;
                newstack->oldEndHandler = OnEndElement_Blocks;
                newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

                XML_SetUserData(stack->parser, newstack);
                XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
                XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
                XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

                return;
            }
        }

        if (!strcmp(name, "color"))
        {
            document_tag_color_t *i = (document_tag_color_t *) Q_malloc(sizeof(document_tag_color_t));
            memset(i, 0, sizeof(document_tag_color_t));
            i->type = tag_color;

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) i;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) i;

            // and now parse this element with new parser
            // create sub-parser
            {
                xml_parser_stack_t *newstack;
                document_parser_specific_t *specific;

                newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
                specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

                specific->tag = &i->tags;

                // prepare user data
                XSD_InitStack(newstack);
                newstack->parser_specific = specific;
                newstack->parser = stack->parser;
                newstack->document = (xml_t *) document;
                newstack->oldUserData = userData;
                newstack->oldStartHandler = OnStartElement_Blocks;
                newstack->oldEndHandler = OnEndElement_Blocks;
                newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

                XML_SetUserData(stack->parser, newstack);
                XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
                XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
                XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

                return;
            }
        }

        if (!strcmp(name, "a"))
        {
            const char *attr;

            document_tag_a_t *i = (document_tag_a_t *) Q_malloc(sizeof(document_tag_a_t));
            memset(i, 0, sizeof(document_tag_a_t));
            i->type = tag_a;

            // check href
            attr = XSD_GetAttribute(atts, "href");
            if (attr)
                i->href = Q_strdup(attr);

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) i;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) i;

            // and now parse this element with new parser
            // create sub-parser
            {
                xml_parser_stack_t *newstack;
                document_parser_specific_t *specific;

                newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
                specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

                specific->tag = &i->tags;

                // prepare user data
                XSD_InitStack(newstack);
                newstack->parser_specific = specific;
                newstack->parser = stack->parser;
                newstack->document = (xml_t *) document;
                newstack->oldUserData = userData;
                newstack->oldStartHandler = OnStartElement_Blocks;
                newstack->oldEndHandler = OnEndElement_Blocks;
                newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

                XML_SetUserData(stack->parser, newstack);
                XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
                XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
                XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

                return;
            }
        }

        if (!strcmp(name, "img"))
        {
            document_tag_img_t *i = (document_tag_img_t *) Q_malloc(sizeof(document_tag_img_t));
            memset(i, 0, sizeof(document_tag_img_t));
            i->type = tag_img;

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) i;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) i;

            // and now parse this element with new parser
            // create sub-parser
            {
                xml_parser_stack_t *newstack;
                document_parser_specific_t *specific;

                newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
                specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

                specific->tag = &i->tags;

                // prepare user data
                XSD_InitStack(newstack);
                newstack->parser_specific = specific;
                newstack->parser = stack->parser;
                newstack->document = (xml_t *) document;
                newstack->oldUserData = userData;
                newstack->oldStartHandler = OnStartElement_Blocks;
                newstack->oldEndHandler = OnEndElement_Blocks;
                newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

                XML_SetUserData(stack->parser, newstack);
                XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
                XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
                XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

                return;
            }
        }

        if (!strcmp(name, "br"))
        {
            document_tag_br_t *i = (document_tag_br_t *) Q_malloc(sizeof(document_tag_br_t));
            memset(i, 0, sizeof(document_tag_br_t));
            i->type = tag_br;

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) i;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) i;
        }

        if (!strcmp(name, "sp"))
        {
            document_tag_sp_t *i = (document_tag_sp_t *) Q_malloc(sizeof(document_tag_sp_t));
            memset(i, 0, sizeof(document_tag_sp_t));
            i->type = tag_sp;

            // add to list
            if (specific->tag[0] == NULL)
                specific->tag[0] = (document_tag_t *) i;
            else
                GetLast(specific->tag[0])->next = (document_tag_t *) i;
        }
    }

    if (!strcmp(stack->path, "/list")  &&  !strcmp(name, "li"))
    {
        xml_parser_stack_t *newstack;
        document_tag_list_t *list;
        document_tag_li_t *item;

        // find last elemend (LIST)
        list = (document_tag_list_t *) GetLast(specific->tag[0]);
        assert(list->type == tag_list);

        // create new LI and add to list
        item = (document_tag_li_t *) Q_malloc(sizeof(document_tag_li_t));
        memset(item, 0, sizeof(document_tag_li_t));
        item->type = tag_li;
        if (list->items == NULL)
            list->items = item;
        else
            GetLast((document_tag_t *)list->items)->next = (document_tag_t *)item;

        // start new parser for LI
        {
            document_parser_specific_t *specific;
            newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
            specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

            specific->tag = &item->tags;

            // prepare user data
            XSD_InitStack(newstack);
            newstack->parser_specific = specific;
            newstack->parser = stack->parser;
            newstack->document = (xml_t *) document;
            newstack->oldUserData = userData;
            newstack->oldStartHandler = OnStartElement_Blocks;
            newstack->oldEndHandler = OnEndElement_Blocks;
            newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

            XML_SetUserData(stack->parser, newstack);
            XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
            XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
            XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

            return;
        }
    }

    if (!strcmp(stack->path, "/dict")  &&  !strcmp(name, "di"))
    {
        document_tag_dict_t *dict;
        document_tag_di_t *item;

        // find last elemend (DICT)
        dict = (document_tag_dict_t *) GetLast(specific->tag[0]);
        assert(dict->type == tag_dict);

        // create new DI and add to dict
        item = (document_tag_di_t *) Q_malloc(sizeof(document_tag_di_t));
        memset(item, 0, sizeof(document_tag_di_t));
        item->type = tag_di;
        if (dict->items == NULL)
            dict->items = item;
        else
            GetLast((document_tag_t *)dict->items)->next = (document_tag_t *)item;
    }

    if (!strcmp(stack->path, "/dict/di")  &&  !strcmp(name, "name"))
    {
        xml_parser_stack_t *newstack;
        document_tag_dict_t *dict;
        document_tag_di_t *item;

        // find last elemend (DICT)
        dict = (document_tag_dict_t *) GetLast(specific->tag[0]);
        assert(dict->type == tag_dict);

        // find last element (DI)
        item = (document_tag_di_t *) GetLast((document_tag_t *)dict->items);
        assert(item->type == tag_di);

        // start new parser for DI/name
        {
            document_parser_specific_t *specific;
            newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
            specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

            specific->tag = &item->name;

            // prepare user data
            XSD_InitStack(newstack);
            newstack->parser_specific = specific;
            newstack->parser = stack->parser;
            newstack->document = (xml_t *) document;
            newstack->oldUserData = userData;
            newstack->oldStartHandler = OnStartElement_Blocks;
            newstack->oldEndHandler = OnEndElement_Blocks;
            newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

            XML_SetUserData(stack->parser, newstack);
            XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
            XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
            XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

            return;
        }
    }

    if (!strcmp(stack->path, "/dict/di")  &&  !strcmp(name, "description"))
    {
        xml_parser_stack_t *newstack;
        document_tag_dict_t *dict;
        document_tag_di_t *item;

        // find last elemend (DICT)
        dict = (document_tag_dict_t *) GetLast(specific->tag[0]);
        assert(dict->type == tag_dict);

        // find last element (DI)
        item = (document_tag_di_t *) GetLast((document_tag_t *)dict->items);
        assert(item->type == tag_di);

        // start new parser for DI/name
        {
            document_parser_specific_t *specific;
            newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
            specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

            specific->tag = &item->description;

            // prepare user data
            XSD_InitStack(newstack);
            newstack->parser_specific = specific;
            newstack->parser = stack->parser;
            newstack->document = (xml_t *) document;
            newstack->oldUserData = userData;
            newstack->oldStartHandler = OnStartElement_Blocks;
            newstack->oldEndHandler = OnEndElement_Blocks;
            newstack->oldCharacterDataHandler = OnCharacterData_Blocks;

            XML_SetUserData(stack->parser, newstack);
            XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
            XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
            XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

            return;
        }
    }

    XSD_OnStartElement(stack, name, atts);
}

static void OnEndElement_Blocks(void *userData, const XML_Char *name)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
//    xml_document_t *document = (xml_document_t *) stack->document;
//    document_parser_specific_t *specific = (document_parser_specific_t *) stack->parser_specific;

    // if element ends and our stack is empty, it means we
    // should return to parent stack & parser
    if (stack->path[0] == 0)
    {
        XSD_RestoreStack(stack);
        Q_free(stack->parser_specific);
        Q_free(stack);
        return;
    }

    XSD_OnEndElement(stack, name);
}

static void OnCharacterData_Blocks(void *userData, const XML_Char *s, int len)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
//    xml_document_t *document = (xml_document_t *) stack->document;
    document_parser_specific_t *specific = (document_parser_specific_t *) stack->parser_specific;

    if (stack->path[0] == 0)
    {
        document_tag_text_t *text;
        document_tag_t *last = GetLast(specific->tag[0]);

        if (last == NULL)
        {
            // create new text tag and start chain with it
            text = (document_tag_text_t *) Q_malloc(sizeof(document_tag_text_t));
            memset(text, 0, sizeof(document_tag_text_t));
            text->type = tag_text;
            specific->tag[0] = (document_tag_t *) text;
        }
        else if (last->type == tag_text)
        {
            // append to this tag
            text = (document_tag_text_t *) last;
        }
        else
        {
            // create new text tag and add it to the chain
            text = (document_tag_text_t *) Q_malloc(sizeof(document_tag_text_t));
            memset(text, 0, sizeof(document_tag_text_t));
            text->type = tag_text;
            last->next = (document_tag_t *) text;
        }

        text->text = XSD_AddText(text->text, s, len);
    }

    if (!strcmp(stack->path, "/pre"))
    {
        document_tag_pre_t *pre;

        // find last element (pre)
        pre = (document_tag_pre_t *) GetLast(specific->tag[0]);
        assert(pre->type = tag_pre);

        // and add text to it
        pre->text = XSD_AddText(pre->text, s, len);
    }
}


// ----------------------------------------------------------------------------
//
// general parser
//

static void OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_document_t *document = (xml_document_t *) stack->document;
//    document_parser_specific_t *specific = (document_parser_specific_t *) stack->parser_specific;

    if (stack->path[0] == 0)
        document->document_type = Q_strdup(name);

    if (!strcmp(stack->path, "/document/head/authors")  &&  !strcmp(name, "author"))
    {
        xml_document_author_t *author = (xml_document_author_t *) Q_malloc(sizeof(xml_document_author_t));
        memset(author, 0, sizeof(xml_document_author_t));

        if (document->authors == NULL)
            document->authors = author;
        else
        {
            xml_document_author_t *last = (xml_document_author_t *)
                GetLast((document_tag_t *)document->authors);
            last->next = author;
        }
    }

    if (XSD_IsIn(stack->path, "/document"))
    {
        if (!strcmp(name, "body"))
        {
            // create sub-parser
            xml_parser_stack_t *newstack;
            document_parser_specific_t *specific;

            newstack = (xml_parser_stack_t *) Q_malloc(sizeof(xml_parser_stack_t));
            specific = (document_parser_specific_t *) Q_malloc(sizeof(document_parser_specific_t));

            specific->tag = &document->content;

            // prepare user data
            XSD_InitStack(newstack);
            newstack->parser_specific = specific;
            newstack->parser = stack->parser;
            newstack->document = (xml_t *) document;
            newstack->oldUserData = userData;
            newstack->oldStartHandler = OnStartElement;
            newstack->oldEndHandler = OnEndElement;
            newstack->oldCharacterDataHandler = OnCharacterData;

            XML_SetUserData(stack->parser, newstack);
            XML_SetStartElementHandler(stack->parser, OnStartElement_Blocks);
            XML_SetEndElementHandler(stack->parser, OnEndElement_Blocks);
            XML_SetCharacterDataHandler(stack->parser, OnCharacterData_Blocks);

            return;
        }
    }

    XSD_OnStartElement(stack, name, atts);
}

static void OnEndElement(void *userData, const XML_Char *name)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_document_t *document = (xml_document_t *) stack->document;

    // strip spaces from elements already loaded
    if (!strcmp(stack->path, "/document/title"))
        document->title = XSD_StripSpaces(document->title);

    XSD_OnEndElement(stack, name);
}

static void OnCharacterData(void *userData, const XML_Char *s, int len)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_document_t *document = (xml_document_t *) stack->document;

    if (!strcmp(stack->path, "/document/head/title"))
        document->title = XSD_AddText(document->title, s, len);

    if (!strcmp(stack->path, "/document/head/date"))
        document->date = XSD_AddText(document->date, s, len);

    if (!strcmp(stack->path, "/document/head/authors/author"))
    {
        xml_document_author_t *a = (xml_document_author_t *)
            GetLast((document_tag_t *)document->authors);

        if (!strcmp(stack->path, "/document/head/authors/name"))
            a->name = XSD_AddText(a->name, s, len);
        if (!strcmp(stack->path, "/document/head/authors/email"))
            a->name = XSD_AddText(a->email, s, len);
        if (!strcmp(stack->path, "/document/head/authors/im"))
            a->name = XSD_AddText(a->im, s, len);
    }
}

// read document content from file, return 0 if error
#ifndef WITH_FTE_VFS
xml_t * XSD_Document_LoadFromHandle(FILE *f, int filelen) {
#else
xml_t * XSD_Document_LoadFromHandle(vfsfile_t *v, int filelen) {
	vfserrno_t err;
#endif
    xml_document_t *document;
    XML_Parser parser = NULL;
    int len;
	int pos = 0;
    char buf[XML_READ_BUFSIZE];
    xml_parser_stack_t parser_stack;

    // create blank document
    document = (xml_document_t *) Q_malloc(sizeof(xml_document_t));
    XSD_Document_Init(document);

    // initialize XML parser
    parser = XML_ParserCreate(NULL);
    if (parser == NULL)
        goto error;
    XML_SetStartElementHandler(parser, OnStartElement);
    XML_SetEndElementHandler(parser, OnEndElement);
    XML_SetCharacterDataHandler(parser, OnCharacterData);

    // prepare user data
    XSD_InitStack(&parser_stack);
    parser_stack.document = (xml_t *) document;
    parser_stack.parser = parser;
    XML_SetUserData(parser, &parser_stack);

#ifndef WITH_FTE_VFS
    while ((len = fread(buf, 1, min(XML_READ_BUFSIZE, filelen-pos), f)) > 0)
#else
    while ((len = VFS_READ(v, buf, min(XML_READ_BUFSIZE, filelen-pos), &err)) > 0)
#endif
    {
        if (XML_Parse(parser, buf, len, 0) != XML_STATUS_OK)
            goto error;

		pos += len;
    }
    if (XML_Parse(parser, NULL, 0, 1) != XML_STATUS_OK)
        goto error;

    XML_ParserFree(parser);

    return (xml_t *) document;

error:

    if (parser)
        XML_ParserFree(parser);
    XSD_Document_Free((xml_t *)document);

    return NULL;
}

// read document content from file, return 0 if error
xml_document_t * XSD_Document_Load(char *filename)
{
    xml_document_t *document;
#ifndef WITH_FTE_VFS
    FILE *f = NULL;
	int len;

    if ((len = FS_FOpenFile(filename, &f)) < 0)
		return NULL;
    document = (xml_document_t *) XSD_Document_LoadFromHandle(f, len);
    fclose(f);
#else
	vfsfile_t *f;

	if (!(f = FS_OpenVFS(filename, "rb", FS_ANY))) {
		return NULL;
	}
    document = (xml_document_t *) XSD_Document_LoadFromHandle(f, VFS_GETLEN(f));
	VFS_CLOSE(f);
#endif

    return document;
}
