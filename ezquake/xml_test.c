#include "quakedef.h"
#include "xsd_variable.h"

void Test_XML(void)
{
    xml_t *document = NULL;

    xml_variable_t *var = NULL;
    xml_command_t *cmd = NULL;
    xml_document_t *doc = NULL;

    document = XSD_LoadDocument("help/variables/cl_smartjump.xml");

    var = (xml_variable_t *) document;

    XSD_FreeDocument(document);
    document = NULL;

    document = XSD_LoadDocument("help/commands/ignore.xml");

    cmd = (xml_command_t *) document;

    XSD_FreeDocument(document);
    document = NULL;

    document = XSD_LoadDocument("help/index.xml");

    doc = (xml_document_t *) document;

    XSD_FreeDocument(document);
    document = NULL;
}
