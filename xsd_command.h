#ifndef __XSD_COMMAND__H__
#define __XSD_COMMAND__H__

#include "xsd.h"

typedef struct command_argument_s
{
    char *name;
    char *description;
    struct command_argument_s *next;
}
command_argument_t;

typedef struct xml_command_s
{
    char *document_type;

    // -----------------

    char *name;
    char *description;
    char *syntax;
    command_argument_t *arguments;
    char *remarks;
}
xml_command_t;


// free xml_command_t struct
void XSD_Command_Free(xml_t *);

// read document content from file, return NULL if error
#ifndef WITH_FTE_VFS
xml_t *XSD_Command_LoadFromHandle(FILE *f, int len);
#else
xml_t *XSD_Command_LoadFromHandle(vfsfile_t *v, int filelen);
#endif

// read document content from file, return NULL if error
xml_command_t * XSD_Command_Load(char *filename);

#endif // __XSD_COMMAND__H__
