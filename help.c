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
#include <jansson.h>
#include "expat.h"
#include "xsd.h"

#define CONSOLE_HELP_MARGIN 2
static json_t *variables_root;
static json_t *command_root;

typedef struct json_variable_s
{
	const char *name;
	const char *description;
	cvartype_t value_type;
	const json_t* values;
	const char *remarks;
} json_variable_t;

typedef struct json_command_s
{
	const char *name;
	const char *description;
	const char *syntax;
	const json_t* arguments;
	const char *remarks;
} json_command_t;

static void Help_DescribeCmd(const json_command_t *cmd)
{
	if (!cmd) 
		return;

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
		int i = 0;
		int argCount = json_array_size(cmd->arguments);

        Com_Printf("\n");
        con_ormask = 128;
        Com_Printf("arguments\n");
        con_ormask = 0;

		for (i = 0; i < argCount; ++i) { 
			json_t* arg = json_array_get(cmd->arguments, i);

            con_ormask = 128;
            con_margin = CONSOLE_HELP_MARGIN;
            Com_Printf("%s", json_string_value(json_object_get(arg, "name")));
            con_ormask = 0;
            con_margin += CONSOLE_HELP_MARGIN;
            Com_Printf(" - %s\n", json_string_value(json_object_get(arg, "description")));
            con_margin = 0;
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

static void Help_DescribeVar(const json_variable_t *var)
{
	if (!var) 
		return;

	// description
	if (var->description && strlen(var->description))
	{
		con_margin = CONSOLE_HELP_MARGIN;
		Com_Printf("%s\n", var->description);
		con_margin = 0;
	}

    // value
	if (var->values)
	{
		int valueCount = json_array_size(var->values);
		int i = 0;

		Com_Printf("\n");
		con_ormask = 128;
		Com_Printf("value\n");
		con_ormask = 0;
		con_margin = CONSOLE_HELP_MARGIN;
		for (i = 0; i < valueCount; ++i) {
			const json_t* value = json_array_get(var->values, i);
			const char*   name = json_string_value(json_object_get(value, "name"));
			const char*   description = json_string_value(json_object_get(value, "description"));

			switch (var->value_type)
			{
			case t_string:
			case t_integer:
			case t_float:
				Com_Printf("%s\n", description);
				break;
			case t_boolean:
			case t_enum:
			case t_unknown:
				con_ormask = 128;
				if (var->value_type == t_boolean && !strcmp(name, "false"))
					Com_Printf("0");
				else if (var->value_type == t_boolean && !strcmp(name, "true"))
					Com_Printf("1");
				else
					Com_Printf("%s", name);
				con_ormask = 0;
				con_margin += CONSOLE_HELP_MARGIN;
				Com_Printf(" - %s\n", description);
				con_margin -= CONSOLE_HELP_MARGIN;
				break;
			}
		}
		con_margin = 0;
	}
	else if (var->value_type == t_boolean)
	{
		con_margin += CONSOLE_HELP_MARGIN;
		Com_Printf("Boolean value. Use ");
		con_ormask = 128;
		Com_Printf("0 or 1");
		con_ormask = 0;
		Com_Printf(" as a value.\n");
		con_margin -= CONSOLE_HELP_MARGIN;
	}

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

static const json_command_t* JSON_Command_Load(const char* name)
{
	static json_command_t result;
	const json_t* command = NULL;

	command = json_object_get(command_root, name);
	if (command == NULL)
		return NULL;

	memset(&result, 0, sizeof(result));
	result.name = name;
	result.description = json_string_value(json_object_get(command, "description"));
	result.syntax = json_string_value(json_object_get(command, "syntax"));
	result.remarks = json_string_value(json_object_get(command, "remarks"));
	result.arguments = json_object_get(command, "arguments");
	if (!json_is_array(result.arguments))
		result.arguments = NULL;

	return &result;
}

static const json_variable_t* JSON_Variable_Load(const char* name)
{
	static json_variable_t result;
	const json_t* varsObj = NULL;
	const json_t* variable = NULL;
	const char* variableType = NULL;

	varsObj = json_object_get(variables_root, "vars");
	if (varsObj == NULL)
		return NULL;

	variable = json_object_get(varsObj, name);
	if (variable == NULL)
		return NULL;

	variableType = json_string_value(json_object_get(variable, "type"));

	memset(&result, 0, sizeof(result));
	result.description = json_string_value(json_object_get(variable, "desc"));
	result.name = name;
	result.remarks = json_string_value(json_object_get(variable, "remarks"));
	if (!strcmp(variableType, "string"))
		result.value_type = t_string;
	else if (!strcmp(variableType, "integer"))
		result.value_type = t_integer;
	else if (!strcmp(variableType, "float"))
		result.value_type = t_float;
	else if (!strcmp(variableType, "boolean"))
		result.value_type = t_boolean;
	else if (!strcmp(variableType, "enum"))
		result.value_type = t_enum;
	else
		result.value_type = t_unknown;
	result.values = json_object_get(variable, "values");
	if (!json_is_array(result.values))
		result.values = NULL;

	return &result;
}

void Help_DescribeCvar (cvar_t *v)
{
	Help_DescribeVar(JSON_Variable_Load(v->name));
}

void Help_VarDescription (const char *varname, char* buf, size_t bufsize)
{
	extern void CharsToBrown(char*, char*);
	extern cvar_t menu_advanced;
	const json_variable_t *var;

	var = JSON_Variable_Load (varname);
	
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

	if (var->values)
	{
		int valueCount = json_array_size(var->values);
		int i = 0;

		strlcat (buf, "\n", bufsize);
		strlcat (buf, "value\n", bufsize);
		for (i = 0; i < valueCount; ++i) {
			const json_t* value = json_array_get(var->values, i);
			const char*   name = json_string_value(json_object_get(value, "name"));
			const char*   description = json_string_value(json_object_get(value, "description"));

			switch (var->value_type)
			{
			case t_string:
			case t_integer:
			case t_float:
				strlcat (buf, description, bufsize);
				strlcat (buf, "\n", bufsize);
				break;
			case t_boolean:
			case t_enum:
			default:
				if (var->value_type == t_boolean && !strcmp(name, "false"))
					strlcat (buf, "0", bufsize);
				else if (var->value_type == t_boolean && !strcmp(name, "true"))
					strlcat (buf, "1", bufsize);
				else
					strlcat (buf, name, bufsize);

				strlcat (buf, ": ", bufsize);
				strlcat (buf, description, bufsize);
				strlcat (buf, "\n", bufsize);
				break;
			}
		}
	}
}

static void Help_UnloadVariablesDoc(void)
{
	if (variables_root != NULL) {
		json_decref(variables_root);
		variables_root = NULL;
	}
}

static void Help_UnloadCommandDoc(void)
{
	if (command_root != NULL) {
		json_decref(command_root);
		command_root = NULL;
	}
}

static void Help_UnloadDocs(void)
{
	Help_UnloadVariablesDoc();
	Help_UnloadCommandDoc();
}

extern unsigned char help_variables_json[];
extern unsigned int help_variables_json_len;
extern unsigned char help_commands_json[];
extern unsigned int help_commands_json_len;

static void Help_LoadVariablesDoc(void)
{
	json_error_t error;

	Help_UnloadVariablesDoc();

	variables_root = json_loadb((char*)help_variables_json, help_variables_json_len, 0, &error);

	if (variables_root == NULL) {
		Com_Printf("error: JSON error on line %d: %s\n", error.line, error.text);
	}
	else if (!json_is_object(variables_root)) {
		Com_Printf("error: invalid JSON, root is not an object\n");
	}
	else {
		Com_DPrintf("help_loadDocs: variables okay\n");
		return;
	}

	Help_UnloadVariablesDoc();
}

static void Help_LoadCommandDoc(void)
{
	json_error_t error;
	
	Help_UnloadCommandDoc();

	command_root = json_loadb((char*)help_commands_json, help_commands_json_len, 0, &error);

	if (command_root == NULL) {
		Com_Printf("error: JSON error on line %d: %s\n", error.line, error.text);
	}
	else if (!json_is_object(variables_root)) {
		Com_Printf("error: invalid JSON, root is not an object\n");
	}
	else {
		Com_DPrintf("help_loadDocs: commands okay\n");
		return;
	}

	Help_UnloadCommandDoc();
}

static void Help_LoadDocs(void)
{
	Help_LoadVariablesDoc();
	Help_LoadCommandDoc();
}

void Help_Describe_f(void)
{
    qbool found = false;
    char *name;
    const json_command_t *cmd;
    const json_variable_t *var;

    if (Cmd_Argc() != 2)
    {
        Com_Printf("usage: /describe <command|variable>\ne.g. /describe ignore\n");
        return;
    }

    name = Cmd_Argv(1);
    
    // check for command
	cmd = JSON_Command_Load(name);
    if (cmd)
    {
        found = true;
        
        // name
        con_ormask = 128;
        Com_Printf("%s is a command\n", cmd->name);
        con_ormask = 0;

		Help_DescribeCmd(cmd);
    }

    // check for variable
	var = JSON_Variable_Load(name);
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
    }

    // if no found
    if (!found)
        Com_Printf("Nothing found.\n");
}

void Help_Missing_Commands(void)
{
	extern int Cmd_CommandCompare(const void *, const void *);
	extern cmd_function_t *cmd_functions;

	cmd_function_t *sorted_commands[4096];
	cmd_function_t *cmd_func = 0;
	int cmd_count = 0;

	for (cmd_func = cmd_functions; cmd_func && cmd_count < sizeof(sorted_commands) / sizeof(sorted_commands[0]); cmd_func = cmd_func->next) {
		const json_command_t* help_cmd = JSON_Command_Load(cmd_func->name);
		if (!help_cmd)
			sorted_commands[cmd_count++] = cmd_func;
	}

	if (cmd_count) {
		int i = 0;

		qsort(sorted_commands, cmd_count, sizeof(cmd_function_t *), Cmd_CommandCompare);

		Com_Printf("Commands missing documentation entries:\n");
		con_margin = CONSOLE_HELP_MARGIN;
		for (i = 0; i < cmd_count; ++i) {
			Com_Printf("%s\n", sorted_commands[i]->name);
		}
		con_margin = 0;
	}
	else {
		Com_Printf("All commands have entries in documentation.\n");
	}
}

void Help_Missing_Variables(void)
{
	extern int Cvar_CvarCompare(const void*, const void*);
	extern cvar_t *cvar_vars;

	cvar_t *sorted_cvars[4096];
	cvar_t *cvar = 0;
	int cvar_count = 0;

	for (cvar = cvar_vars; cvar && cvar_count < sizeof(sorted_cvars) / sizeof(sorted_cvars[0]); cvar = cvar->next) {
		const json_variable_t* help_cvar = JSON_Variable_Load(cvar->name);

		if (Cvar_GetFlags(cvar) & CVAR_USER_CREATED)
			continue;

		if (! help_cvar)
			sorted_cvars[cvar_count++] = cvar;
	}

	if (cvar_count) {
		int i = 0;

		qsort(sorted_cvars, cvar_count, sizeof(cvar_t *), Cvar_CvarCompare);

		Com_Printf("Variables missing documentation entries:\n");
		con_margin = CONSOLE_HELP_MARGIN;
		for (i = 0; i < cvar_count; ++i) {
			Com_Printf("%s\n", sorted_cvars[i]->name);
		}
		con_margin = 0;
	}
	else {
		Com_Printf("All variables have entries in documentation.\n");
	}
}

// Returns list of commands and variables that are missing from documentation
void Help_Missing_f(void)
{
	Help_Missing_Commands();
	Help_Missing_Variables();
}

void Help_Issues_Commands(void)
{
	// TODO: documentation for commands generally comes from .c functions, not really
	//       enough information to validate against documentation
}

void Help_Issues_Variables(void)
{
	static const char* validTypes[] = {
		"string", "integer", "float", "boolean", "enum"
	};
	extern cvar_t *cvar_vars;

	json_t* varsObj = NULL;
	json_t* groupsObj = NULL;
	cvar_t *cvar = NULL;
	int i = 0;
	int num_errors = 0;
	const char* name = NULL;
	json_t* variable = NULL;

	if (!variables_root) {
		return;
	}

	varsObj = json_object_get(variables_root, "vars");
	if (!varsObj) {
		return;
	}

	groupsObj = json_object_get(variables_root, "groups");
	if (!groupsObj)
		return;

	json_object_foreach(varsObj, name, variable) {
		const char* group_id = json_string_value(json_object_get(variable, "group-id"));
		const char* type     = json_string_value(json_object_get(variable, "type"));
		const char* desc     = json_string_value(json_object_get(variable, "desc"));
		json_t* examples     = json_object_get(variable, "values");
		qbool valid_type     = (type && strlen(type));
		qbool valid_desc     = (desc && strlen(desc));
		qbool num_examples   = examples && json_is_array(examples) ? json_array_size(examples) : 0;

		qbool enum_without_examples = type && !strcmp(type, "enum") && num_examples == 0;
		qbool non_enum_with_examples = type && strcmp(type, "enum") && num_examples;

		if (non_enum_with_examples) {
			if (!strcmp(type, "boolean")) {
				// booleans can have true/false in value section
				non_enum_with_examples = num_examples != 2;
			}
			else {
				// other types can have * as single example
				json_t* example   = json_array_get(examples, 0);
				const char* value = json_string_value(json_object_get(example, "name"));

				if (value && !strcmp(value, "*")) {
					non_enum_with_examples = false;
				}
			}
		}

		// old variables might still be in documentation, allows "/describe <var>" to explain why it is removed
		{
			qbool in_client = false;

			for (cvar = cvar_vars; cvar && !in_client; cvar = cvar->next) {
				in_client = !strcasecmp(cvar->name, name);
			}
			if (!in_client) {
				continue;
			}
		}

		// ignore documentation for server variables (tighten this in future)
		{
			qbool obselete_or_server_only = false;

			for (i = 0; i < json_array_size(groupsObj); ++i) {
				const char* id = json_string_value(json_object_get(json_array_get(groupsObj, i), "id"));
				const char* major_group = json_string_value(json_object_get(json_array_get(groupsObj, i), "major-group"));

				if (id && group_id && !strcmp(id, group_id)) {
					if (major_group && (!strcmp(major_group, "Server") || !strcmp(major_group, "Obselete"))) {
						obselete_or_server_only = true;
					}
				}
			}

			if (obselete_or_server_only) {
				continue;
			}
		}

		// don't expect complete documentation right now (tighten this in future)
		if (!strncmp(name, "hud_", 4) || !strncmp(name, "sv_", 3)) {
			continue;
		}

		if (valid_type) {
			valid_type = false;
			for (i = 0; i < sizeof(validTypes) / sizeof(validTypes[0]); ++i) {
				valid_type |= !strcmp(validTypes[i], type);
			}
		}

		if (!valid_type  || !(valid_desc || num_examples) || enum_without_examples || non_enum_with_examples) {
			if (num_errors == 0) {
				Con_Printf("Variables with issues:\n");
				con_margin = CONSOLE_HELP_MARGIN;
			}

			con_ormask = 128;
			Con_Printf("%s", name);
			con_ormask = 0;
			Con_Printf(": ");
			con_margin = CONSOLE_HELP_MARGIN * 2;
			if (!valid_type) {
				Con_Printf("invalid type (\"%s\")", type ? type : "");
			}
			else if (enum_without_examples || non_enum_with_examples) {
				Con_Printf("invalid examples for type \"%s\" (%d)", type, num_examples);
			}
			else {
				if (!valid_desc && num_examples == 0) {
					Con_Printf("invalid description/examples", name);
				}
				else if (!valid_desc) {
					Con_Printf("invalid description", name);
				}
				else {
					Con_Printf("invalid examples", name);
				}
			}
			con_margin = CONSOLE_HELP_MARGIN;
			Con_Printf("\n");
			++num_errors;
		}
	}

	con_margin = 0;
	Con_Printf("%d variables with issues.\n", num_errors);
}

void Help_Issues_f(void)
{
	Help_Issues_Commands();
	Help_Issues_Variables();
}

void Help_Init(void)
{
	Cmd_AddCommand("describe", Help_Describe_f);
	Cmd_AddCommand("help_missing", Help_Missing_f);
	Cmd_AddCommand("help_issues", Help_Issues_f);

	Help_LoadDocs();
}

void Help_Shutdown(void)
{
	Help_UnloadDocs();
}
