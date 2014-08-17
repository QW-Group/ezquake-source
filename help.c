/*
This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

	$Id: help.c,v 1.16 2007-09-30 22:59:23 disconn3ct Exp $
*/

/*
 * Variables and commands help system
 */

// for QuakeWorld Tutorial menu, look into help_files.c

#include "quakedef.h"
#include "expat.h"
#include "xsd.h"

#define CONSOLE_HELP_MARGIN 2

void Help_DescribeCmd(xml_command_t *cmd)
{
    command_argument_t *arg;

	if (!cmd) return;

    // description
    con_margin = CONSOLE_HELP_MARGIN;
    Com_Printf("%s\n", cmd->description);
    con_margin = 0;

    // syntax
    Com_Printf("\n");
    con_ormask = 128;
    Com_Printf("syntax\n");
    con_ormask = 0;
    con_margin = CONSOLE_HELP_MARGIN;
    Com_Printf("%s %s\n", cmd->name, cmd->syntax);
    con_margin = 0;

    // arguments
    if (cmd->arguments)
    {
        Com_Printf("\n");
        con_ormask = 128;
        Com_Printf("arguments\n");
        con_ormask = 0;
        arg = cmd->arguments;
        while (arg)
        {
            con_ormask = 128;
            con_margin = CONSOLE_HELP_MARGIN;
            Com_Printf("%s", arg->name);
            con_ormask = 0;
            con_margin += CONSOLE_HELP_MARGIN;
            Com_Printf(" - %s\n", arg->description);
            con_margin = 0;
            arg = arg->next;
        }
    }

    // remarks
    if (cmd->remarks)
    {
        Com_Printf("\n");
        con_ormask = 128;
        Com_Printf("remarks\n");
        con_ormask = 0;
        con_margin = CONSOLE_HELP_MARGIN;
        Com_Printf("%s\n", cmd->remarks);
        con_margin = 0;
    }
}

void Help_DescribeVar(xml_variable_t *var)
{
	variable_enum_value_t *val;

	if (!var) return;

	// description
    con_margin = CONSOLE_HELP_MARGIN;
    Com_Printf("%s\n", var->description);
    con_margin = 0;

    // value
    Com_Printf("\n");
    con_ormask = 128;
    Com_Printf("value\n");
    con_ormask = 0;
    con_margin = CONSOLE_HELP_MARGIN;
    switch (var->value_type)
    {
    case t_string:
        Com_Printf("%s\n", var->value.string_description);
        break;

    case t_integer:
        Com_Printf("%s\n", var->value.integer_description);
        break;

    case t_float:
		Com_Printf("%s\n", var->value.float_description);
        break;

    case t_boolean:
		if (var->value.boolean_value.false_description && var->value.boolean_value.true_description
			&&
			(strlen(var->value.boolean_value.false_description) || strlen(var->value.boolean_value.true_description)))
		{
			con_ormask = 128;
			Com_Printf("0");
			con_ormask = 0;
			con_margin += CONSOLE_HELP_MARGIN;
			Com_Printf(" - %s\n", var->value.boolean_value.false_description);
			con_margin -= CONSOLE_HELP_MARGIN;
			con_ormask = 128;
			Com_Printf("1");
			con_ormask = 0;
			con_margin += CONSOLE_HELP_MARGIN;
			Com_Printf(" - %s\n", var->value.boolean_value.true_description);
			con_margin -= CONSOLE_HELP_MARGIN;
		}
		else	// johnnycz - many cvar doc pages has been made by an automatic process and they have boolean as their type as a default
		{
			con_margin += CONSOLE_HELP_MARGIN;
			Com_Printf("Boolean value. Use ");
			con_ormask = 128;
			Com_Printf("0 or 1");
			con_ormask = 0;
			Com_Printf(" as a value.\n");
			con_margin -= CONSOLE_HELP_MARGIN;
		}
        break;

    case t_enum:
        val = var->value.enum_value;
        while (val)
        {
            con_ormask = 128;
            Com_Printf("%s", val->name);
            con_ormask = 0;
            con_margin += CONSOLE_HELP_MARGIN;
            Com_Printf(" - %s\n", val->description);
            con_margin -= CONSOLE_HELP_MARGIN;
            val = val->next;
        }
        break;
    }
    con_margin = 0;

    // remarks
	if (var->remarks && strlen(var->remarks))
    {
        Com_Printf("\n");
        con_ormask = 128;
        Com_Printf("remarks\n");
        con_ormask = 0;
        con_margin = CONSOLE_HELP_MARGIN;
        Com_Printf("%s\n", var->remarks);
        con_margin = 0;
    }

}

void Help_DescribeCvar (cvar_t *v)
{
	xml_variable_t *var = XSD_Variable_Load(va("help/variables/%s.xml", v->name));
	Help_DescribeVar(var);
}

void Help_VarDescription (const char *varname, char* buf, size_t bufsize)
{
	extern void CharsToBrown(char*, char*);
	extern cvar_t menu_advanced;
	xml_variable_t *var;
	variable_enum_value_t *cv;

	var = XSD_Variable_Load (va ("help/variables/%s.xml", varname));
	
	if(menu_advanced.integer && strlen(varname)){
		strlcat(buf, "Variable name: ", bufsize);
		strlcat(buf, varname, bufsize);
		CharsToBrown(buf, buf + strlen (buf) - strlen(varname));
		strlcat(buf, "\n", bufsize);
	}
	
	if (!var)
		return;

	if (var->description && strlen (var->description) > 1) {
		strlcat (buf, var->description, bufsize);
		strlcat (buf, "\n", bufsize);
	}

	if (var->remarks) {
		strlcat (buf, "remarks: ", bufsize);
		strlcat (buf, var->remarks, bufsize);
		strlcat (buf, "\n", bufsize);
	}

	switch (var->value_type) {
	case t_boolean:
		if (var->value.boolean_value.false_description) {
			strlcat (buf, "0: ", bufsize);
			strlcat (buf, var->value.boolean_value.false_description, bufsize);
			strlcat (buf, "\n", bufsize);
		}

		if (var->value.boolean_value.true_description) {
			strlcat (buf, "1: ", bufsize);
			strlcat (buf, var->value.boolean_value.true_description, bufsize);
			strlcat (buf, "\n", bufsize);
		}

		break;

	case t_float:
		if (var->value.float_description) {
			strlcat (buf, var->value.float_description, bufsize);
			strlcat (buf, "\n", bufsize);
		}

		break;

	case t_integer:
		if (var->value.integer_description) {
			strlcat (buf, var->value.integer_description, bufsize);
			strlcat (buf, "\n", bufsize);
		}

		break;

	case t_string:
		if (var->value.string_description) {
			strlcat (buf, var->value.string_description, bufsize);
			strlcat (buf, "\n", bufsize);
		}

		break;

	case t_enum:
		cv = var->value.enum_value;
		while(cv) {
			if (cv->name && cv->description) {
				strlcat (buf, cv->name, bufsize);
				strlcat (buf, ":", bufsize);
				strlcat (buf, cv->description, bufsize);
				strlcat (buf, "\n", bufsize);
			}

			cv = cv->next;
		}

		break;
	}

	XSD_Variable_Free ((xml_t *) var);
}

void Help_Describe_f(void)
{
    qbool found = false;
    char *name;
    xml_command_t *cmd;
    xml_variable_t *var;

    if (Cmd_Argc() != 2)
    {
        Com_Printf("usage: /describe <command|variable>\ne.g. /describe ignore\n");
        return;
    }

    name = Cmd_Argv(1);
    
    // check for command
    cmd = XSD_Command_Load(va("help/commands/%s.xml", name));
    if (cmd)
    {
        found = true;
        
        // name
        con_ormask = 128;
        Com_Printf("%s is a command\n", cmd->name);
        con_ormask = 0;

		Help_DescribeCmd(cmd);

        XSD_Command_Free( (xml_t *)cmd );
    }

    // check for variable
    var = XSD_Variable_Load(va("help/variables/%s.xml", name));
    if (var)
    {
        if (found)
            Com_Printf("\n\n\n");

        found = true;
        
        // name
        con_ormask = 128;
        Com_Printf("%s is a variable\n", var->name);
        con_ormask = 0;

		Help_DescribeVar(var);

        XSD_Variable_Free( (xml_t *)var );
    }

    // if no found
    if (!found)
        Com_Printf("Nothing found.\n");
}

void Help_Init(void)
{
    Cmd_AddCommand("describe", Help_Describe_f);
}
