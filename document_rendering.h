#ifndef __DOCUMENT_RENDERING_H__
#define __DOCUMENT_RENDERING_H__


typedef struct document_rendered_link_s
{
    int start;
    int end;
    document_tag_a_t *tag;

    struct document_rendered_link_s *next;
}
document_rendered_link_t;

typedef struct document_rendered_section_s
{
    int start;
    document_tag_section_t *tag;

    struct document_rendered_section_s *children;
    struct document_rendered_section_s *next;
}
document_rendered_section_t;


typedef struct document_renered_s
{
    // head
    char *title;
    int title_lines;

    // body
    char *text;
    int text_lines;

    // metadata
    document_rendered_link_t *links;
    document_rendered_section_t *sections;
}
document_rendered_t;

// renders document into memory buffer,
 int XSD_RenderDocument(document_rendered_t *r, xml_document_t *doc, int width);

// free rendered document
void XSD_RenderClear(document_rendered_t *);




#endif // __DOCUMENT_RENDERING_H__
