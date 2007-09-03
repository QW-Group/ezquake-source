#ifndef __XSD_VARIABLE__H__
#define __XSD_VARIABLE__H__

#include "xsd.h"
#include "xsd_document.h"

typedef enum
{
    t_boolean,
    t_enum,         // "value1, value2, value_wih_spaces"
    t_integer,      // "min max"
    t_float,        // "min max"
    t_string        // "max length"
}
cvartype_t; 

typedef struct variable_enum_value_s
{
    char *name;
    char *description;
    struct variable_enum_value_s *next;
}
variable_enum_value_t;

typedef struct xml_variable_s
{
    char *document_type;

    // -----------------

    char *name;
    char *description;
    cvartype_t value_type;
    union
    {
        char *string_description;
        char *integer_description;
        char *float_description;
        struct
        {
            char *true_description;
            char *false_description;
        } boolean_value;
        variable_enum_value_t *enum_value;
    } value;
    char *remarks;
}
xml_variable_t;


// free xml_variable_t struct
void XSD_Variable_Free(xml_t *);

// read variable content from file, return NULL if error
#ifndef WITH_FTE_VFS
xml_t * XSD_Variable_LoadFromHandle(FILE *f, int len);
#else
xml_t * XSD_Variable_LoadFromHandle(vfsfile_t *v, int filelen);
#endif

// read variable content from file, return NULL if error
xml_variable_t * XSD_Variable_Load(char *filename);

// convert to xml_document_t
xml_document_t * XSD_Variable_Convert(xml_t *doc);

#endif // __XSD_VARIABLE__H__
