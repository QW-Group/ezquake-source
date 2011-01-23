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
xml_t * XSD_Variable_LoadFromHandle(vfsfile_t *v, int filelen);

// read variable content from file, return NULL if error
xml_variable_t * XSD_Variable_Load(char *filename);

// convert to xml_document_t
xml_document_t * XSD_Variable_Convert(xml_t *doc);

#endif // __XSD_VARIABLE__H__
