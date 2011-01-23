/*
Copyright (C) 2011 azazello and ezQuake team

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
xml_t *XSD_Command_LoadFromHandle(vfsfile_t *v, int filelen);

// read document content from file, return NULL if error
xml_command_t * XSD_Command_Load(char *filename);

#endif // __XSD_COMMAND__H__
