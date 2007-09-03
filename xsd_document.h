#ifndef __XSD_DOCUMENT_H__
#define __XSD_DOCUMENT_H__

#include "xsd.h"

typedef enum
{
    // plain text
    tag_text,

    // block elements
    tag_section,
    tag_p,
    tag_list,
    tag_dict,
    tag_hr,
    tag_h,
    tag_pre,

    // inline elements
    tag_em,
    tag_a,
    tag_img,
    tag_color,
    tag_br,
    tag_sp,

    // other
    tag_li,
    tag_di
}
document_tag_type_t;

typedef enum
{
    align_left,
    align_center,
    align_right
}
document_align_t;


// This is a base for all tag structures. Every tag structure
// should inherit fields defined here. This one is abstract.
typedef struct document_tag_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------

    // rest of data follow
    // ...
}
document_tag_t;


typedef struct document_tag_text_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    char *text;
}
document_tag_text_t;


typedef struct document_tag_p_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
    qbool indent;
    document_align_t align;
}
document_tag_p_t;


typedef struct document_tag_section_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
    qbool indent;
    char *title;
    char *id;
}
document_tag_section_t;


typedef struct document_tag_hr_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
}
document_tag_hr_t;


typedef struct document_tag_h_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
    document_align_t align;
}
document_tag_h_t;


typedef struct document_tag_em_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
}
document_tag_em_t;


typedef struct document_tag_color_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
}
document_tag_color_t;


typedef struct document_tag_a_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
    char *href;
}
document_tag_a_t;


typedef struct document_tag_img_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
}
document_tag_img_t;


typedef struct document_tag_br_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
}
document_tag_br_t;


typedef struct document_tag_pre_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    char *text;
    char *alt;
}
document_tag_pre_t;


typedef struct document_tag_sp_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
}
document_tag_sp_t;


typedef enum
{
    list_bullet_none,
    list_bullet_dot,
    list_bullet_number,
    list_bullet_letter,
    list_bullet_bigletter
}
list_bullet_t;

typedef enum
{
    list_separator_none,
    list_separator_dot,
    list_separator_par
}
list_separator_t;

typedef struct document_tag_li_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *tags;
}
document_tag_li_t;

typedef struct document_tag_list_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_li_t *items;
    list_bullet_t bullet;
    list_separator_t separator;
}
document_tag_list_t;


typedef struct document_tag_di_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_t *name;
    document_tag_t *description;
}
document_tag_di_t;

typedef struct document_tag_dict_s
{
    document_tag_type_t type;
    struct document_tag_s *next;
    // --------------------
    document_tag_di_t *items;
    list_bullet_t bullet;
    list_separator_t separator;
}
document_tag_dict_t;


typedef struct xml_document_author_s
{
    document_tag_type_t type;   // nothing for this type
    struct xml_document_author_s *next;
    // --------------------

    char *name;
    char *email;
    char *im;
}
xml_document_author_t;

typedef struct xml_document_s
{
    char *document_type;

    // head
    char *title;
    char *date;
    xml_document_author_t *authors;

    // body
    document_tag_t *content;
}
xml_document_t;


// document parser specific data
typedef struct document_parser_specific_s
{
    document_tag_t **tag;
    int section_num;
}
document_parser_specific_t;


// create new document
xml_document_t * XSD_Document_New(void);

// free xml_document_t struct
void XSD_Document_Free(xml_t *);

// read document content from file, return NULL if error
#ifndef WITH_FTE_VFS
xml_t * XSD_Document_LoadFromHandle(FILE *f, int len);
#else
xml_t * XSD_Document_LoadFromHandle(vfsfile_t *v, int filelen);
#endif

// read document content from file, return NULL if error
xml_document_t * XSD_Document_Load(char *filename);

#endif // __XSD_DOCUMENT_H__
