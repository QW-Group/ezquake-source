/*
Copyright (C) 2011 VULTUREIIC

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
// $Id: xsd.c,v 1.14 2007-10-29 12:48:12 cokeman1982 Exp $

#include "quakedef.h"
#include "expat.h"
#include "xsd.h"

typedef xml_t * (*XSD_DocumentLoadType)(vfsfile_t *v, int len);
typedef void (*XSD_DocumentFreeType)(xml_t *);
typedef xml_document_t * (*XSD_DocumentConvertType)(xml_t *);

typedef struct xsd_mapping_s
{
    char *document_type;
    XSD_DocumentLoadType load_function;
    XSD_DocumentFreeType free_function;
    XSD_DocumentConvertType convert_function;
}
xsd_mapping_t;

xsd_mapping_t xsd_mappings[] = {
    {"variable", XSD_Variable_LoadFromHandle, XSD_Variable_Free, XSD_Variable_Convert},
    {"command", XSD_Command_LoadFromHandle, XSD_Command_Free, NULL},
    {"document", XSD_Document_LoadFromHandle, XSD_Document_Free, NULL},
    {NULL, NULL, NULL}
};

// get value attribute from the array
const char *XSD_GetAttribute(const char **atts, const char *name)
{
    while (*atts)
    {
        if (!strcmp(*atts, name))
            return *(atts+1);

        atts += 2;
    }

    return NULL;
}

// add text (strcat) with realocation
char *XSD_AddText(char *dst, const char *src, int src_len)
{
    char *buf;
	size_t len;

    if (dst == NULL)
    {
        buf = (char *) Q_malloc(src_len + 1);
        memcpy(buf, src, src_len);
        buf[src_len] = 0;
    }
    else
    {
		len = 1 + src_len + strlen(dst);
        buf = (char *) Q_malloc(len);
        SDL_strlcpy (buf, dst, len);
        memcpy(buf+strlen(buf), src, src_len);
        buf[src_len+strlen(dst)] = 0;
        Q_free(dst);
    }

    return buf;
}

// test if character is valid XML space
int XSD_IsSpace(char c)
{
    if (c == ' '  ||  c == '\t'  ||  c == '\n'  ||  c == '\r')
        return 1;
    return 0;
}

// strip spaces multiple spaces from in-between words
char *XSD_StripSpaces (char *str)
{
    char *buf, *ret;
    unsigned int p = 0, q = 0;

    if (str == NULL)
        return str;

    buf = (char *) Q_malloc(strlen(str)+1);
    for (p=0; p < strlen(str); p++)
    {
        if (XSD_IsSpace(str[p]))
        {
            if (q == 0  ||  XSD_IsSpace(buf[q-1]))
                ;
            else
                buf[q++] = ' ';
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

// check if we are somewhere inside given element
int XSD_IsIn(char *path, char *subPath)
{
    size_t sublen = strlen(subPath);

    // if path is shorter than subpath
    if (strlen(path) < sublen)
        return 0;

    // if it matches
    if (!strncmp(path, subPath, sublen)  &&
        (path[sublen] == 0  ||  path[sublen] == '/'))
    {
        return 1;
    }

    // no match
    return 0;
}

// initialize parser stack
void XSD_InitStack(xml_parser_stack_t *stack)
{
    memset(stack, 0, sizeof(xml_parser_stack_t));
}

// restore parser handlers from previous one in stack
void XSD_RestoreStack(xml_parser_stack_t *stack)
{
    XML_SetStartElementHandler(stack->parser, stack->oldStartHandler);
    XML_SetEndElementHandler(stack->parser, stack->oldEndHandler);
    XML_SetCharacterDataHandler(stack->parser, stack->oldCharacterDataHandler);
    XML_SetUserData(stack->parser, stack->oldUserData);
}

// call when element starts
void XSD_OnStartElement(xml_parser_stack_t *stack, const XML_Char *name, const XML_Char **atts)
{
    SDL_strlcat(stack->path, "/", sizeof (stack->path));
    SDL_strlcat(stack->path, name, sizeof (stack->path));
}

// call when element ends
void XSD_OnEndElement(xml_parser_stack_t *stack, const XML_Char *name)
{
    char *t = strrchr(stack->path, '/');
    *t = 0;
}

static void XSD_DetectType_OnStartElement(void *userData, const XML_Char *name, const XML_Char **atts)
{
    char *type = (char *) userData;

    if (type[0] == 0)
        strcpy(type, name);
}

// load document, auto-recognizing its type, returns NULL in case of failure
xml_t * XSD_LoadDocument(char *filename)
{
    xml_t *ret = NULL;
    int i;
	vfsfile_t *f;
    XML_Parser parser = NULL;
    int len;
	int filelen;
    char buf[XML_READ_BUFSIZE];
    char document_type[1024];
	
	// FIXME: D-Kure, does FS_ANY handle both the above cases
	if (!(f = FS_OpenVFS(filename, "rb", FS_ANY))) {
		return NULL;
	}
	filelen = VFS_GETLEN(f);

    // initialize XML parser
    parser = XML_ParserCreate(NULL);
	if (parser == NULL) {
		Com_Printf("could not open2\n");
        goto error;
	}
    XML_SetStartElementHandler(parser, XSD_DetectType_OnStartElement);
    XML_SetUserData(parser, document_type);

    document_type[0] = 0;

    while (document_type[0] == 0  &&  (len = VFS_READ(f, buf, XML_READ_BUFSIZE, NULL)) > 0)
    {
		if (XML_Parse(parser, buf, len, 0) != XML_STATUS_OK) {
			Com_Printf("could not open3\n");
            goto error;
		}
    }
	if (document_type[0] == 0) {
		Com_Printf("could not open4\n");
        goto error;
	}

    // now we know what document type it is...

    // parser is no more needed
    XML_ParserFree(parser);
    parser = NULL;

    // fseek to the beginning of the file
	VFS_SEEK(f, 0, SEEK_SET);

    // execute loading parser
    i = 0;
    while (xsd_mappings[i].document_type != NULL)
    {
        if (!strcmp(xsd_mappings[i].document_type, document_type))
        {
			//???
            ret = xsd_mappings[i].load_function(f, filelen);
            break;
        }
        i++;
    }

    if (ret)
    {
		VFS_CLOSE(f);
        return ret;
    }

error:
    if (f)
		VFS_CLOSE(f);

    if (parser)
        XML_ParserFree(parser);
    return NULL;
}

// load document and convert it to xml_document_t
xml_document_t * XSD_LoadDocumentWithXsl(char *filename)
{
    xml_t *doc;
    xml_document_t *document;

    doc = XSD_LoadDocument(filename);

    if (doc == NULL)
        return NULL;

    if (!strcmp(doc->document_type, "document"))
        return (xml_document_t *) doc; // no conversion

    // convert
    document = XSD_XslConvert(doc);
    XSD_FreeDocument(doc);
    return document;

}

// free document loaded with XSD_LoadDocument
void XSD_FreeDocument(xml_t *document)
{
    int i = 0;
    while (xsd_mappings[i].document_type != NULL)
    {
        if (!strcmp(xsd_mappings[i].document_type, document->document_type))
        {
            xsd_mappings[i].free_function(document);
            return;
        }
        i++;
    }
}

// convert any known xml document to "document" type
xml_document_t * XSD_XslConvert(xml_t *doc)
{
    int i = 0;
    while (xsd_mappings[i].document_type != NULL)
    {
        if (!strcmp(xsd_mappings[i].document_type, doc->document_type))
        {
            if (xsd_mappings[i].convert_function)
                return (xml_document_t *)xsd_mappings[i].convert_function(doc);
            else
                return NULL;
        }
        i++;
    }

    return NULL;
}
