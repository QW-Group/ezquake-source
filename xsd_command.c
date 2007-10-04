// $Id: xsd_command.c,v 1.8 2007-10-04 14:56:54 dkure Exp $

#include "quakedef.h"
#include "expat.h"
#include "xsd_command.h"


// init xml_command_t struct
static void XSD_Command_Init(xml_command_t *document)
{
    memset(document, 0, sizeof(xml_command_t));
}

// free xml_command_t struct
void XSD_Command_Free(xml_t *doc)
{
    xml_command_t *document = (xml_command_t *) doc;

    if (document->name)
        Q_free(document->name);

    if (document->description)
        Q_free(document->description);

    if (document->remarks)
        Q_free(document->remarks);

    if (document->syntax)
        Q_free(document->syntax);

    while (document->arguments)
    {
        command_argument_t *next = document->arguments->next;
        Q_free(document->arguments);
        document->arguments = next;
    }

    if (document->document_type)
        Q_free(document->document_type);

    // delete document
    Q_free(document);
}

static void OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_command_t *document = (xml_command_t *) stack->document;

    if (stack->path[0] == 0)
        document->document_type = Q_strdup(name);

    if (!strcmp(stack->path, "/command/arguments"))
    {
        // create new argument
        command_argument_t *val = (command_argument_t *) Q_malloc(sizeof(command_argument_t));
        memset(val, 0, sizeof(command_argument_t));

        if (document->arguments == NULL)
            document->arguments = val;
        else
        {
            command_argument_t *prev = document->arguments;
            while (prev->next)
                prev = prev->next;
            prev->next = val;
        }
    }

    XSD_OnStartElement(stack, name, atts);
}

static void OnEndElement(void *userData, const XML_Char *name)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_command_t *document = (xml_command_t *) stack->document;

    // strip spaces from elements already loaded
    if (!strcmp(stack->path, "/command/name"))
        document->name = XSD_StripSpaces(document->name);
    if (!strcmp(stack->path, "/command/description"))
        document->description = XSD_StripSpaces(document->description);
    if (!strcmp(stack->path, "/command/remarks"))
        document->remarks = XSD_StripSpaces(document->remarks);
    if (!strcmp(stack->path, "/command/syntax"))
        document->syntax = XSD_StripSpaces(document->syntax);

    if (!strcmp(stack->path, "/command/arguments/argument"))
    {
        command_argument_t *last = document->arguments;
        while (last->next)
            last = last->next;

        last->name = XSD_StripSpaces(last->name);
        last->description = XSD_StripSpaces(last->description);
    }

    XSD_OnEndElement(stack, name);
}

static void OnCharacterData(void *userData, const XML_Char *s, int len)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_command_t *document = (xml_command_t *) stack->document;

    if (!strcmp(stack->path, "/command/name"))
        document->name = XSD_AddText(document->name, s, len);
    if (!strcmp(stack->path, "/command/description"))
        document->description = XSD_AddText(document->description, s, len);
    if (!strcmp(stack->path, "/command/remarks"))
        document->remarks = XSD_AddText(document->remarks, s, len);
    if (!strcmp(stack->path, "/command/syntax"))
        document->syntax = XSD_AddText(document->syntax, s, len);

    if (!strncmp(stack->path, "/command/arguments/argument", strlen("/command/arguments/argument")))
    {
        command_argument_t *last = document->arguments;
        while (last->next)
            last = last->next;

        if (!strcmp(stack->path, "/command/arguments/argument/name"))
            last->name = XSD_AddText(last->name, s, len);
        if (!strcmp(stack->path, "/command/arguments/argument/description"))
            last->description = XSD_AddText(last->description, s, len);
    }
}

// read command content from file, return 0 if error
#ifndef WITH_FTE_VFS
xml_t * XSD_Command_LoadFromHandle(FILE *f, int filelen) {
#else
xml_t * XSD_Command_LoadFromHandle(vfsfile_t *v, int filelen) {
	vfserrno_t err;
#endif
    xml_command_t *document;
    XML_Parser parser = NULL;
    int len;
	int pos = 0;
    char buf[XML_READ_BUFSIZE];
    xml_parser_stack_t parser_stack;

    // create blank document
    document = (xml_command_t *) Q_malloc(sizeof(xml_command_t));
    XSD_Command_Init(document);

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
    XSD_Command_Free((xml_t *)document);

    return NULL;
}

// read command content from file, return 0 if error
xml_command_t * XSD_Command_Load(char *filename)
{
    xml_command_t *document;
#ifndef WITH_FTE_VFS
    FILE *f = NULL;
	int len;

    if ((len = FS_FOpenFile(filename, &f)) < 0)
		return NULL;
    document = (xml_command_t *) XSD_Command_LoadFromHandle(f, len);
    fclose(f);
#else
	vfsfile_t *f;

	if (!(f = FS_OpenVFS(filename, "rb", FS_ANY))) {
		return NULL;
	}
    document = (xml_command_t *) XSD_Command_LoadFromHandle(f, VFS_GETLEN(f));
	VFS_CLOSE(f);
#endif

    return document;
}
