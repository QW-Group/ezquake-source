#include "quakedef.h"
#include "xsd_variable.h"


// init xml_variable_t struct
static void XSD_Variable_Init(xml_variable_t *document)
{
    memset(document, 0, sizeof(xml_variable_t));
}

// free xml_variable_t struct
void XSD_Variable_Free(xml_t *doc)
{
    xml_variable_t *document = (xml_variable_t *) doc;

    if (document->name)
        free(document->name);

    if (document->description)
        free(document->description);

    if (document->remarks)
        free(document->remarks);

    switch (document->value_type)
    {
    case t_boolean:
        if (document->value.boolean_value.true_description)
            free(document->value.boolean_value.true_description);
        if (document->value.boolean_value.false_description)
            free(document->value.boolean_value.false_description);
        break;
    case t_integer:
        if (document->value.integer_description)
            free(document->value.integer_description);
        break;
    case t_float:
        if (document->value.float_description)
            free(document->value.float_description);
        break;
    case t_string:
        if (document->value.string_description)
            free(document->value.string_description);
        break;
    case t_enum:
        while (document->value.enum_value)
        {
            variable_enum_value_t *next = document->value.enum_value->next;
            free(document->value.enum_value);
            document->value.enum_value = next;
        }
        break;
    }

    if (document->document_type)
        free(document->document_type);

    // delete document
    free(document);
}

static void OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
    xml_parser_stack_t *stack = (xml_parser_stack_t *) userData;
    xml_variable_t *document = (xml_variable_t *) stack->document;

    if (stack->path[0] == 0)
        document->document_type = strdup(name);

    if (!strcmp(stack->path, "/variable/value"))
    {
        if (!strcmp(name, "string"))
            document->value_type = t_string;
        if (!strcmp(name, "integer"))
            document->value_type = t_integer;
        if (!strcmp(name, "float"))
            document->value_type = t_float;
        if (!strcmp(name, "boolean"))
            document->value_type = t_boolean;
        if (!strcmp(name, "enum"))
            document->value_type = t_enum;
    }
    if (!strcmp(stack->path, "/variable/value/enum"))
    {
        // create new enum
        variable_enum_value_t *val = (variable_enum_value_t *) malloc(sizeof(variable_enum_value_t));
        memset(val, 0, sizeof(variable_enum_value_t));

        if (document->value.enum_value == NULL)
            document->value.enum_value = val;
        else
        {
            variable_enum_value_t *prev = document->value.enum_value;
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
    xml_variable_t *document = (xml_variable_t *) stack->document;

    // strip spaces from elements already loaded
    if (!strcmp(stack->path, "/variable/name"))
        document->name = XSD_StripSpaces(document->name);
    if (!strcmp(stack->path, "/variable/description"))
        document->description = XSD_StripSpaces(document->description);
    if (!strcmp(stack->path, "/variable/remarks"))
        document->remarks = XSD_StripSpaces(document->remarks);

    if (!strcmp(stack->path, "/variable/value/string"))
        document->value.string_description = XSD_StripSpaces(document->value.string_description);
    if (!strcmp(stack->path, "/variable/value/integer"))
        document->value.integer_description = XSD_StripSpaces(document->value.integer_description);
    if (!strcmp(stack->path, "/variable/value/float"))
        document->value.float_description = XSD_StripSpaces(document->value.float_description);
    if (!strcmp(stack->path, "/variable/value/boolean/true"))
        document->value.boolean_value.true_description = XSD_StripSpaces(document->value.boolean_value.true_description);
    if (!strcmp(stack->path, "/variable/value/boolean/false"))
        document->value.boolean_value.false_description = XSD_StripSpaces(document->value.boolean_value.false_description);
    if (!strcmp(stack->path, "/variable/value/enum/value"))
    {
        variable_enum_value_t *last = document->value.enum_value;
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
    xml_variable_t *document = (xml_variable_t *) stack->document;

    if (!strcmp(stack->path, "/variable/name"))
        document->name = XSD_AddText(document->name, s, len);
    if (!strcmp(stack->path, "/variable/description"))
        document->description = XSD_AddText(document->description, s, len);
    if (!strcmp(stack->path, "/variable/remarks"))
        document->remarks = XSD_AddText(document->remarks, s, len);

    if (!strcmp(stack->path, "/variable/value/string"))
        document->value.string_description = XSD_AddText(document->value.string_description, s, len);
    if (!strcmp(stack->path, "/variable/value/integer"))
        document->value.integer_description = XSD_AddText(document->value.integer_description, s, len);
    if (!strcmp(stack->path, "/variable/value/float"))
        document->value.float_description = XSD_AddText(document->value.float_description, s, len);
    if (!strcmp(stack->path, "/variable/value/boolean/true"))
        document->value.boolean_value.true_description = XSD_AddText(document->value.boolean_value.true_description, s, len);
    if (!strcmp(stack->path, "/variable/value/boolean/false"))
        document->value.boolean_value.false_description = XSD_AddText(document->value.boolean_value.false_description, s, len);
    if (!strncmp(stack->path, "/variable/value/enum/value", strlen("/variable/value/enum/value")))
    {
        variable_enum_value_t *last = document->value.enum_value;
        while (last->next)
            last = last->next;

        if (!strcmp(stack->path, "/variable/value/enum/value/name"))
            last->name = XSD_AddText(last->name, s, len);
        if (!strcmp(stack->path, "/variable/value/enum/value/description"))
            last->description = XSD_AddText(last->description, s, len);
    }
}

// read variable content from file, return 0 if error
xml_t * XSD_Variable_LoadFromHandle(FILE *f)
{
    xml_variable_t *document;
    XML_Parser parser = NULL;
    int len;
    char buf[XML_READ_BUFSIZE];
    xml_parser_stack_t parser_stack;

    // create blank document
    document = (xml_variable_t *) malloc(sizeof(xml_variable_t));
    XSD_Variable_Init(document);

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

    while ((len = fread(buf, 1, XML_READ_BUFSIZE, f)) > 0)
    {
        if (XML_Parse(parser, buf, len, 0) != XML_STATUS_OK)
            goto error;
    }
    if (XML_Parse(parser, NULL, 0, 1) != XML_STATUS_OK)
        goto error;

    XML_ParserFree(parser);

    return (xml_t *) document;

error:

    if (parser)
        XML_ParserFree(parser);
    XSD_Variable_Free((xml_t *)document);

    return NULL;
}

// read variable content from file, return 0 if error
xml_variable_t * XSD_Variable_Load(char *filename)
{
    FILE *f = NULL;
    xml_variable_t *document;

    if (FS_FOpenFile(filename, &f) < 0)
        return NULL;

    document = (xml_variable_t *) XSD_Variable_LoadFromHandle(f);
    fclose(f);
    return document;
}

// convert to xml_document_t
xml_document_t * XSD_Variable_Convert(xml_t *doc)
{
    xml_document_t *ret;
    xml_variable_t *var = (xml_variable_t *) doc;

    ret = XSD_Document_New();

    // make head
    ret->title = strdup(var->name);

    // make body
    {
        document_tag_text_t *text;

        text = (document_tag_text_t *) malloc(sizeof(document_tag_text_t));
        memset(text, 0, sizeof(document_tag_text_t));
        text->type = tag_text;
        text->text = strdup(var->description);
        ret->content = (document_tag_t *) text;
    }

    return ret;
}
