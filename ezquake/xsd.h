#ifndef __XSD_H__
#define __XSD_H__


#define XML_READ_BUFSIZE 4096

typedef struct xml_s
{
    char *document_type;

    // ----------------

    // rest goes here...
}
xml_t;


// include different document types handlers
#include "xsd_variable.h"
#include "xsd_command.h"
#include "xsd_document.h"


#define PARSER_STACK_PATH_SIZE  1024    // temporary

typedef struct xml_parser_stack_s
{
    XML_Parser parser;
    xml_t *document;

    char path[PARSER_STACK_PATH_SIZE];

    // previous parser handlers
    // allows for switching parser functions during parsing
    XML_StartElementHandler oldStartHandler;
    XML_EndElementHandler oldEndHandler;
    XML_CharacterDataHandler oldCharacterDataHandler;
    void *oldUserData;

    // parser specific data
    void *parser_specific;
}
xml_parser_stack_t;


// get value attribute from the array
const char *XSD_GetAttribute(const char **atts, const char *name);

// add text (strcat) with realocation
char *XSD_AddText(char *dst, const char *src, int src_len);

// test if character is valid XML space
int XSD_IsSpace(char);

// strip spaces multiple spaces from in-between words
char *XSD_StripSpaces(char *);

// check if we are somewhere inside given element
int XSD_IsIn(char *path, char *subPath);

// initialize parser stack
void XSD_InitStack(xml_parser_stack_t *stack);

// restore parser handlers from previous one in stack
void XSD_RestoreStack(xml_parser_stack_t *stack);

// call when element starts
void XSD_OnStartElement(xml_parser_stack_t *stack, const XML_Char *name, const XML_Char **atts);

// call when element ends
void XSD_OnEndElement(xml_parser_stack_t *stack, const XML_Char *name);




// load document, auto-recognizing its type, returns NULL in case of failure
xml_t * XSD_LoadDocument(char *filename);

// load document and convert it to xml_document_t
xml_document_t * XSD_LoadDocumentWithXsl(char *filename);

// free document loaded with XSD_LoadDocument
void XSD_FreeDocument(xml_t *);

// convert any known xml document to "document" type
xml_document_t * XSD_XslConvert(xml_t *doc);



#endif // __XSD_H__
