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

extern void CharsToBrown(char*, char*);
extern char* CharsToBrownStatic(char* in);

typedef enum {
	t_boolean,
	t_enum,         // "value1, value2, value_wih_spaces"
	t_integer,      // "min max"
	t_float,        // "min max"
	t_string,       // "max length"
	t_unknown       // (missing from docs)
} cvartype_t;

enum {
	helpdoc_variables,
	helpdoc_commands,
	helpdoc_cmdlineparams,
	helpdoc_macros,

	helpdoc_count
};

#define CONSOLE_HELP_MARGIN 2
static json_t* document_root[helpdoc_count];

typedef struct json_variable_s
{
	const char *name;
	const char *default_value;
	const char *description;
	int value_type;
	const json_t* values;
	const char *remarks;
	const char* group_id;
	json_t* json;
} json_variable_t;

typedef struct json_command_s
{
	const char *name;
	const char *description;
	const char *syntax;
	const json_t* arguments;
	const char *remarks;
} json_command_t;

typedef struct json_mapping_s {
	const char* name;
	int value;
} json_mapping_t;

static json_mapping_t variabletype_mappings[] = {
	{ "string", t_string },
	{ "integer", t_integer },
	{ "float", t_float },
	{ "boolean", t_boolean },
	{ "bool", t_boolean },
	{ "enum", t_enum }
};

#define CMDLINEPARAM_FLAGS_NONE       0
#define CMDLINEPARAM_FLAGS_INCOMPLETE 1

static json_mapping_t cmdlineparam_flags_mappings[] = {
	{ "incomplete", CMDLINEPARAM_FLAGS_INCOMPLETE }
};

#define CMDLINEPARAM_BUILDS_DEBUG     1
#define CMDLINEPARAM_BUILDS_RELEASE   2
#define CMDLINEPARAM_BUILDS_ALL       (CMDLINEPARAM_BUILDS_DEBUG | CMDLINEPARAM_BUILDS_RELEASE)

static json_mapping_t cmdlineparam_build_mappings[] = {
	{ "debug", CMDLINEPARAM_BUILDS_DEBUG },
	{ "release", CMDLINEPARAM_BUILDS_RELEASE }
};

#define CMDLINEPARAM_SYSTEMS_WINDOWS  1
#define CMDLINEPARAM_SYSTEMS_LINUX    2
#define CMDLINEPARAM_SYSTEMS_OSX      4
#define CMDLINEPARAM_SYSTEMS_ALL      (CMDLINEPARAM_SYSTEMS_WINDOWS | CMDLINEPARAM_SYSTEMS_LINUX | CMDLINEPARAM_SYSTEMS_OSX)

static json_mapping_t cmdlineparam_systems_mappings[] = {
	{ "windows", CMDLINEPARAM_SYSTEMS_WINDOWS },
	{ "linux", CMDLINEPARAM_SYSTEMS_LINUX },
	{ "osx", CMDLINEPARAM_SYSTEMS_OSX }
};

typedef struct json_cmdlineparam_s {
	const char* name;
	const char* description;
	const char* remarks;
	int systems;
	int builds;
	int flags;
	const char* arguments;
} json_cmdlineparam_t;

#define MACROS_FLAGS_NONE             0
#define MACROS_FLAGS_SPECTATORONLY    1

static json_mapping_t macros_flags_mappings[] = {
	{ "spectator-only", MACROS_FLAGS_SPECTATORONLY }
};

typedef struct json_macro_s {
	const char* name;
	const char* description;
	const char* remarks;
	const char* type;
	const json_t* related_cvars;
	int flags;

	json_t* json;
} json_macro_t;

#define HelpReadBitField(json, array_name, default_value, mappings) (JSON_ReadBitField(json_object_get(json, array_name), default_value, mappings, sizeof(mappings) / sizeof(mappings[0])))

static qbool JSON_MapValue(const char* string, json_mapping_t* mappings, int mapping_count, int* value)
{
	int i;

	if (string && string[0]) {
		for (i = 0; i < mapping_count; ++i) {
			if (!strcmp(string, mappings[i].name)) {
				*value = mappings[i].value;
				return true;
			}
		}
	}

	return false;
}

static int JSON_ReadBitField(const json_t* arr, int empty_value, json_mapping_t* mappings, int mapping_count)
{
	if (!json_is_array(arr)) {
		if (json_is_string(arr)) {
			int result = 0;

			if (JSON_MapValue(json_string_value(arr), mappings, mapping_count, &result)) {
				return result;
			}

			// error: name not valid
			return empty_value;
		}
		// else error: specified but not string|array
		return empty_value;
	}
	else if (json_array_size(arr) == 0) {
		return empty_value;
	}
	else {
		size_t index;
		const json_t* value;
		int result = 0;

		json_array_foreach(arr, index, value)
		{
			if (json_is_string(value)) {
				const char* stringvalue = json_string_value(value);
				int flagvalue = 0;

				if (JSON_MapValue(stringvalue, mappings, mapping_count, &flagvalue)) {
					result |= flagvalue;
				}
				// else error: string not valid
			}
			// else error: expected string
		}

		return result;
	}
}

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
	if (cmd->syntax) {
		con_ormask = 128;
		Com_Printf ("syntax\n");
		con_ormask = 0;
		con_margin = CONSOLE_HELP_MARGIN;
		Com_Printf ("%s %s\n", cmd->name, cmd->syntax);
		con_margin = 0;
	}

    // arguments
    if (cmd->arguments)
    {
		size_t i = 0;
		size_t argCount = json_array_size(cmd->arguments);

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
		size_t valueCount = json_array_size(var->values);
		size_t i = 0;

		Com_Printf("\n");
		con_ormask = 128;
		Com_Printf("values\n");
		con_ormask = 0;
		con_margin = CONSOLE_HELP_MARGIN;
		for (i = 0; i < valueCount; ++i) {
			const json_t* value       = json_array_get(var->values, i);
			const char*   name        = json_string_value(json_object_get(value, "name"));
			const char*   description = json_string_value(json_object_get(value, "description"));
			// if value is *, just show basic description
			if (strcmp(name, "*")) {
				con_ormask = 128;
				if (var->value_type == t_boolean && !strcmp(name, "false")) {
					Com_Printf("0");
				}
				else if (var->value_type == t_boolean && !strcmp(name, "true")) {
					Com_Printf("1");
				}
				else {
					Com_Printf("%s", name);
				}
				con_ormask = 0;
				con_margin += CONSOLE_HELP_MARGIN;
				Com_Printf(" - %s\n", description);
				con_margin -= CONSOLE_HELP_MARGIN;
			}
			else {
				Com_Printf("%s\n", description);
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

static void Help_DescribeMacro(const json_macro_t* macro)
{
	if (!macro) {
		return;
	}

	// description
	if (macro->description && strlen(macro->description)) {
		con_margin = CONSOLE_HELP_MARGIN;
		Com_Printf("%s\n", macro->description);
		con_margin = 0;
	}

	// value
	if (macro->type && strlen(macro->type)) {
		con_ormask = 128;
		Com_Printf("Type: ");
		con_ormask = 0;

		Com_Printf("%s\n", macro->type);
	}

	// remarks
	if ((macro->remarks && macro->remarks[0]) || macro->flags) {
		Com_Printf("\n");
		con_ormask = 128;
		Com_Printf("remarks\n");
		con_ormask = 0;
		con_margin = CONSOLE_HELP_MARGIN;
		if (macro->flags & MACROS_FLAGS_SPECTATORONLY) {
			Com_Printf("Limited to spectator mode only\n");
		}
		if (macro->remarks && macro->remarks[0]) {
			Com_Printf("%s\n", macro->remarks);
		}
		con_margin = 0;
	}

	if (macro->related_cvars) {
		int i;
		const json_t* cvar;

		Com_Printf("\n");
		con_ormask = 128;
		Com_Printf("related cvars:\n");
		con_ormask = 0;
		con_margin = CONSOLE_HELP_MARGIN;
		json_array_foreach(macro->related_cvars, i, cvar) {
			if (json_is_string(cvar)) {
				Com_Printf("%s\n", json_string_value(cvar));
			}
		}
		con_margin = 0;
	}
}

static void Help_DescribeCommandLineParam(const json_cmdlineparam_t* param)
{
	qbool system_comment = false;

	if (!param) {
		return;
	}

#if defined(_WIN32)
	system_comment = !(param->systems & CMDLINEPARAM_SYSTEMS_WINDOWS);
#elif defined(__APPLE__)
	system_comment = !(param->systems & CMDLINEPARAM_SYSTEMS_OSX);
#else
	system_comment = !(param->systems & CMDLINEPARAM_SYSTEMS_LINUX);
#endif

	// description
	if (param->description && strlen(param->description)) {
		con_margin = CONSOLE_HELP_MARGIN;
		Com_Printf("%s\n", param->description);
		con_margin = 0;
	}

	if (param->arguments && param->arguments[0]) {
		Com_Printf("\n");
		con_ormask = 128;
		Com_Printf("arguments: ");
		con_ormask = 0;
		Com_Printf("%s\n", param->arguments);
	}

	// remarks
	if ((param->remarks && param->remarks[0]) || param->flags || system_comment) {
		Com_Printf("\n");
		con_ormask = 128;
		Com_Printf("remarks\n");
		con_ormask = 0;
		con_margin = CONSOLE_HELP_MARGIN;
		if (system_comment) {
			Com_Printf("(not supported on this operating system)\n");
		}
		if (param->flags & CMDLINEPARAM_FLAGS_INCOMPLETE) {
			Com_Printf("(this documentation is flagged as incomplete)\n");
		}
		if (param->remarks && param->remarks[0]) {
			Com_Printf("%s\n", param->remarks);
		}
		con_margin = 0;
	}
}

static const json_command_t* JSON_Command_Load(const char* name)
{
	static json_command_t result;
	const json_t* command = NULL;

	command = json_object_get(document_root[helpdoc_commands], name);
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
	json_t* variable = NULL;
	const char* variableType = NULL;

	varsObj = json_object_get(document_root[helpdoc_variables], "vars");
	if (varsObj == NULL) {
		return NULL;
	}

	variable = json_object_get(varsObj, name);
	if (variable == NULL) {
		return NULL;
	}
	variableType = json_string_value(json_object_get(variable, "type"));

	memset(&result, 0, sizeof(result));
	result.default_value = json_string_value(json_object_get(variable, "default"));
	result.description = json_string_value(json_object_get(variable, "desc"));
	result.name = name;
	result.remarks = json_string_value(json_object_get(variable, "remarks"));
	if (!JSON_MapValue(variableType, variabletype_mappings, sizeof(variabletype_mappings) / sizeof(variabletype_mappings[0]), &result.value_type)) {
		result.value_type = t_unknown;
	}
	result.values = json_object_get(variable, "values");
	if (!json_is_array(result.values)) {
		result.values = NULL;
	}
	result.group_id = json_string_value(json_object_get(variable, "group_id"));
	result.json = variable;

	return &result;
}

const json_cmdlineparam_t* JSON_CommandLineParam_Load(const char* name)
{
	static json_cmdlineparam_t result;
	const json_t* json = NULL;

	json = json_object_get(document_root[helpdoc_cmdlineparams], name);
	if (json == NULL) {
		return NULL;
	}

	memset(&result, 0, sizeof(result));
	result.name = name;
	result.description = json_string_value(json_object_get(json, "description"));
	result.remarks = json_string_value(json_object_get(json, "remarks"));
	result.builds = HelpReadBitField(json, "builds", CMDLINEPARAM_BUILDS_ALL, cmdlineparam_build_mappings);
	result.systems = HelpReadBitField(json, "systems", CMDLINEPARAM_SYSTEMS_ALL, cmdlineparam_systems_mappings);
	result.flags = HelpReadBitField(json, "flags", CMDLINEPARAM_FLAGS_NONE, cmdlineparam_flags_mappings);
	result.arguments = json_string_value(json_object_get(json, "arguments"));

	return &result;
}

const json_macro_t* JSON_Macro_Load(const char* name)
{
	static json_macro_t result;
	json_t* json = NULL;

	json = json_object_get(document_root[helpdoc_macros], name);
	if (json == NULL) {
		return NULL;
	}

	memset(&result, 0, sizeof(result));
	result.name = name;
	result.description = json_string_value(json_object_get(json, "description"));
	result.remarks = json_string_value(json_object_get(json, "remarks"));
	result.flags = HelpReadBitField(json, "flags", 0, macros_flags_mappings);
	result.related_cvars = json_object_get(json, "related-cvars");
	if (!json_is_array(result.related_cvars) || json_array_size(result.related_cvars) == 0) {
		result.related_cvars = NULL;
	}
	result.json = json;

	return &result;
}

void Help_DescribeCvar (cvar_t *v)
{
	Help_DescribeVar(JSON_Variable_Load(v->name));
}

void Help_VarDescription (const char *varname, char* buf, size_t bufsize)
{
	extern cvar_t menu_advanced;
	const json_variable_t *var;

	var = JSON_Variable_Load (varname);
	
	if(menu_advanced.integer && strlen(varname)){
		strlcat(buf, CharsToBrownStatic("Variable Name: "), bufsize);
		strlcat(buf, varname, bufsize);
		strlcat(buf, "\n", bufsize);

		if (var->default_value) {
			strlcat(buf, CharsToBrownStatic("Default: "), bufsize);
			strlcat(buf, var->default_value, bufsize);
			strlcat(buf, "\n", bufsize);
		}
	}
	
	if (!var)
		return;

	if (var->description && strlen (var->description) > 1) {
		strlcat (buf, var->description, bufsize);
		strlcat (buf, "\n", bufsize);
	}

	if (var->values)
	{
		size_t valueCount = json_array_size(var->values);
		size_t i = 0;

		strlcat (buf, "\n", bufsize);
		strlcat (buf, CharsToBrownStatic("values"), bufsize);
		strlcat (buf, "\n", bufsize);
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
					strlcat (buf, CharsToBrownStatic("0"), bufsize);
				else if (var->value_type == t_boolean && !strcmp(name, "true"))
					strlcat (buf, CharsToBrownStatic("1"), bufsize);
				else
					strlcat (buf, CharsToBrownStatic((char*)name), bufsize);

				strlcat (buf, " - ", bufsize);
				strlcat (buf, description, bufsize);
				strlcat (buf, "\n", bufsize);
				break;
			}
		}
	}

	if (var->remarks) {
		strlcat(buf, CharsToBrownStatic("remarks"), bufsize);
		strlcat(buf, "\n", bufsize);
		strlcat(buf, var->remarks, bufsize);
		strlcat(buf, "\n", bufsize);
	}
}

static void Help_UnloadDocument(json_t** root)
{
	if (*root != NULL) {
		json_decref(*root);
		*root = NULL;
	}
}

static void Help_UnloadDocuments(void)
{
	int i;

	for (i = 0; i < helpdoc_count; ++i) {
		Help_UnloadDocument(&document_root[i]);
	}
}

#define DeclareHelpDocument(id, docname) \
{ \
	extern unsigned char docname ## _json[]; \
	extern unsigned int docname ## _json_len; \
\
	document_names[helpdoc_ ## id] = #docname; \
	document_content[helpdoc_ ## id] = docname ## _json; \
	document_length[helpdoc_ ## id] = docname ## _json_len; \
}

static void Help_LoadDocuments(void)
{
	json_error_t error;
	const char* document_names[helpdoc_count] = { 0 };
	unsigned char* document_content[helpdoc_count] = { 0 };
	unsigned int document_length[helpdoc_count] = { 0 };
	int i;

	DeclareHelpDocument(macros, help_macros)
	DeclareHelpDocument(commands, help_commands)
	DeclareHelpDocument(variables, help_variables)
	DeclareHelpDocument(cmdlineparams, help_cmdline_params)

	for (i = 0; i < helpdoc_count; ++i) {
		Help_UnloadDocument(&document_root[i]);

		document_root[i] = json_loadb((char*)document_content[i], document_length[i], 0, &error);
		if (document_root[i] == NULL) {
			Com_Printf("\020%s\021: JSON error on line %d: %s\n", document_names[i], error.line, error.text);
		}
		else if (!json_is_object(document_root[i])) {
			Com_Printf("\020%s\021: invalid JSON, root is not an object\n", document_names[i]);
			Help_UnloadDocument(&document_root[i]);
		}
	}
}

void Help_Describe_f(void)
{
    qbool found = false;
    char *name;

    if (Cmd_Argc() != 2)
    {
        Com_Printf("usage: /describe <command|variable>\ne.g. /describe ignore\n");
        return;
    }

    name = Cmd_Argv(1);
    
    // check for command
	{
		const json_command_t* cmd = JSON_Command_Load(name);
		if (cmd) {
			found = true;

			// name
			con_ormask = 128;
			Com_Printf("%s is a command\n", cmd->name);
			con_ormask = 0;

			Help_DescribeCmd(cmd);
		}
	}

    // check for variable
	{
		const json_variable_t* var = JSON_Variable_Load(name);
		if (var) {
			if (found) {
				Com_Printf("\n\n\n");
			}
			found = true;

			// name
			con_ormask = 128;
			Com_Printf("%s is a variable\n", var->name);
			con_ormask = 0;

			Help_DescribeVar(var);
		}
	}

	// check for macro
	{
		const json_macro_t* macro = JSON_Macro_Load(name);
		if (macro) {
			if (found) {
				Com_Printf("\n\n\n");
			}
			found = true;

			// name
			con_ormask = 128;
			Com_Printf("%s is a macro\n", macro->name);
			con_ormask = 0;

			Help_DescribeMacro(macro);
		}
	}

	// check for commandline arg
	{
		const json_cmdlineparam_t* param = JSON_CommandLineParam_Load(name);
		if (param) {
			if (found) {
				Com_Printf("\n\n\n");
			}
			found = true;

			// name
			con_ormask = 128;
			Com_Printf("%s is a command-line parameter\n", param->name);
			con_ormask = 0;

			Help_DescribeCommandLineParam(param);
		}
	}

    // if not found
	if (!found) {
		Com_Printf("Nothing found.\n");
	}
}

static qbool Help_VariableIgnoreByCvar(cvar_t* cvar)
{
	if (Cvar_GetFlags(cvar) & CVAR_USER_CREATED) {
		return true;
	}

	// don't expect complete documentation right now (tighten this in future)
	return (!strncmp(cvar->name, "hud_", 4) || !strncmp(cvar->name, "sv_", 3));
}

static qbool Help_VariableIgnoreByGroup(const json_variable_t* var, const json_t* groupsObj)
{
	// ignore documentation for server variables (tighten this in future)
	{
		const char* group_id = var->group_id;
		int i;

		for (i = 0; i < json_array_size(groupsObj); ++i) {
			const char* id = json_string_value(json_object_get(json_array_get(groupsObj, i), "id"));
			const char* major_group = json_string_value(json_object_get(json_array_get(groupsObj, i), "major-group"));

			if (id && group_id && !strcmp(id, group_id) && major_group) {
				if (!strcmp(major_group, "Server") || !strcmp(major_group, "Obselete")) {
					return true;
				}
			}
		}
	}

	return false;
}

// Check all variable values against help files
static void Help_VerifyConfig_f(void)
{
	json_t* varsObj = NULL;
	int num_errors = 0;
	const char* name = NULL;
	json_t* variable = NULL;

	if (!document_root[helpdoc_variables]) {
		return;
	}

	varsObj = json_object_get(document_root[helpdoc_variables], "vars");
	if (!varsObj) {
		return;
	}

	json_object_foreach(varsObj, name, variable)
	{
		cvar_t* cvar        = Cvar_Find(name);
		const char* type    = json_string_value(json_object_get(variable, "type"));
		json_t* examples    = json_object_get(variable, "values");
		size_t num_examples = json_is_array(examples) ? json_array_size(examples) : 0;

		// obsolete cvars might be in documentation so people can see notes using /describe
		if (cvar == NULL) {
			continue;
		}

		if (!strcmp("boolean", type) && cvar->value != 0 && cvar->value != 1) {
			con_ormask = 128;
			Com_Printf ("%s", name);
			con_ormask = 0;
			Com_Printf(": boolean, value %f\n", cvar->value);
			++num_errors;
		}
		else if (!strcmp("enum", type) && num_examples > 0) {
			qbool found = false;
			int i;

			for (i = 0; i < num_examples; ++i) {
				const json_t* value = json_array_get(examples, i);
				const char*   name = json_string_value(json_object_get(value, "name"));

				found |= !strcmp(name, cvar->string);
			}

			if (!found) {
				con_ormask = 128;
				Con_Printf("%s", name);
				con_ormask = 0;
				Con_Printf(": enum, value %s\n", cvar->string);
			}
		}
	}

	Con_Printf("%d variables with values that don't match documentation.\n", num_errors);
}

static void Help_FindConsoleVariableIssues(qbool generate)
{
	extern cvar_t* cvar_vars;
	cvar_t* cvar;

	int num_errors = 0;
	const json_variable_t* jsonVar;
	const json_t* groupsJsonObj = json_object_get(document_root[helpdoc_variables], "groups");
	json_t* variableJsonObj = json_object_get(document_root[helpdoc_variables], "vars");

	if (!variableJsonObj) {
		Con_Printf("'vars' missing from help_variables.json\n");
		return;
	}

	if (!groupsJsonObj) {
		Con_Printf("'Groups' missing from help_variables.json\n");
		return;
	}

	if (!generate) {
		Con_Printf("Variables with issues:\n");
		con_margin = CONSOLE_HELP_MARGIN;
	}

	for (cvar = cvar_vars; cvar; cvar = cvar->next) {
		if (Help_VariableIgnoreByCvar(cvar)) {
			continue;
		}

		jsonVar = JSON_Variable_Load(cvar->name);
		if (!jsonVar) {
			if (generate) {
				json_t* obj = json_object();
				json_object_set(obj, "group-id", json_string("0"));
				json_object_set(obj, "system-generated", json_boolean(1));
				json_object_set_new(obj, "default", json_string(cvar->defaultvalue));
				json_object_set_new(variableJsonObj, cvar->name, obj);
				++num_errors;
			}
			else {
				con_ormask = 128;
				Con_Printf("%s", cvar->name);
				con_ormask = 0;
				Con_Printf(": missing from help\n", cvar->name);
				++num_errors;
			}
			continue;
		}

		if (generate) {
			json_object_set_new(jsonVar->json, "default", json_string(cvar->defaultvalue));
			continue;
		}

		if (Help_VariableIgnoreByGroup(jsonVar, groupsJsonObj)) {
			continue;
		}

		{
			const char* desc = jsonVar->description;
			qbool valid_desc = (desc && desc[0]);
			qbool valid_type, enum_without_examples;
			size_t num_examples;

			valid_type = (jsonVar->value_type != t_unknown);
			num_examples = jsonVar->values ? json_array_size(jsonVar->values) : 0;
			enum_without_examples = (jsonVar->value_type == t_enum && num_examples == 0);
			if (!valid_type || !(valid_desc || num_examples) || enum_without_examples) {
				con_ormask = 128;
				Con_Printf("%s", cvar->name);
				con_ormask = 0;
				Con_Printf(":");
				if (!valid_type) {
					Con_Printf("invalid type");
				}
				else if (enum_without_examples) {
					Con_Printf("invalid examples for enum");
				}
				else if (!valid_desc && num_examples == 0) {
					Con_Printf("invalid description/examples");
				}
				else if (!valid_desc) {
					Con_Printf("invalid description");
				}
				else {
					Con_Printf("invalid examples");
				}
				Con_Printf("\n");
				++num_errors;
			}
		}
	}
	con_margin = 0;
	Con_Printf("Number of variables with issues: %d\n", num_errors);
}

static void Help_FindCommandIssues(qbool generate)
{
	extern cmd_function_t *cmd_functions;
	cmd_function_t *cmd_func = 0;
	int num_errors = 0;

	for (cmd_func = cmd_functions; cmd_func; (cmd_func = cmd_func->next)) {
		const json_command_t* help_cmd = JSON_Command_Load(cmd_func->name);
		extern void HUD_Plus_f(void);
		extern void HUD_Minus_f(void);
		extern void HUD_Func_f(void);

		if (cmd_func->function == HUD_Plus_f || cmd_func->function == HUD_Minus_f || cmd_func->function == HUD_Func_f) {
			// not interested in hud's system-generated commands for the moment
			continue;
		}

		if (!help_cmd) {
			// missing
			if (generate) {
				json_t* obj = json_object();
				json_object_set(obj, "system-generated", json_boolean(1));
				json_object_set_new(document_root[helpdoc_commands], cmd_func->name, obj);
			}
			else {
				if (num_errors == 0) {
					Con_Printf("Commands with issues:\n");
					con_margin = CONSOLE_HELP_MARGIN;
				}

				con_ormask = 128;
				Con_Printf("%s", cmd_func->name);
				con_ormask = 0;
				Con_Printf(": missing from documentation\n");
			}
			++num_errors;
		}
	}

	con_margin = 0;
	Con_Printf("Number of commands with issues: %d\n", num_errors);
}

static void Help_FindMacroIssues(qbool generate)
{
	int i;
	int issues = 0;

	for (i = 0; i < num_macros; i++) {
		const char* name = Cmd_MacroName(i);
		const json_macro_t* help_macro = JSON_Macro_Load(name);
		qbool teamplay = Cmd_MacroTeamplayRestricted(i);

		if (help_macro && generate) {
			json_object_set_new(help_macro->json, "teamplay-restricted", json_boolean(teamplay ? 1 : 0));
		}
		else if (generate) {
			json_t* obj = json_object();
			json_object_set(obj, "system-generated", json_boolean(1));
			json_object_set(obj, "teamplay-restricted", json_boolean(teamplay ? 1 : 0));
			json_object_set_new(document_root[helpdoc_macros], name, obj);
			++issues;
		}
		else if (!help_macro) {
			if (issues == 0) {
				Con_Printf("Macros with issues:\n");
				con_margin = CONSOLE_HELP_MARGIN;
			}

			con_ormask = 128;
			Con_Printf("%s", name);
			con_ormask = 0;
			Con_Printf(": missing from documentation\n");
			++issues;
		}
	}

	con_margin = 0;
	Con_Printf("Number of macros with issues: %d\n", issues);
}

static void Help_FindCommandLineParamIssues(qbool generate)
{
	int i;
	int issues = 0;

	for (i = 0; i < num_cmdline_params; i++) {
		const char* name = Cmd_CommandLineParamName(i);
		const json_cmdlineparam_t* help_param = JSON_CommandLineParam_Load(name);

		if (help_param) {
			continue;
		}

		if (generate) {
			json_t* obj = json_object();
			json_object_set(obj, "system-generated", json_boolean(1));
			json_object_set_new(document_root[helpdoc_cmdlineparams], name, obj);
			++issues;
		}
		else {
			if (issues == 0) {
				Con_Printf("Command-line parameters with issues:\n");
				con_margin = CONSOLE_HELP_MARGIN;
			}
			con_ormask = 128;
			Con_Printf("%s", name);
			con_ormask = 0;
			Con_Printf(": missing from documentation\n");
			++issues;
		}
	}

	con_margin = 0;
	Con_Printf("Number of command-line params with issues: %d\n", issues);
}

static void Help_Issues_f(void)
{
	qbool generate = Cmd_Argc() >= 1 && !strcmp(Cmd_Argv(1), "generate");

	Help_FindConsoleVariableIssues(generate);
	Help_FindCommandIssues(generate);
	Help_FindMacroIssues(generate);
	Help_FindCommandLineParamIssues(generate);

	if (generate) {
		json_dump_file(document_root[helpdoc_variables], "qw/help_variables.json", JSON_ENSURE_ASCII | JSON_SORT_KEYS | JSON_INDENT(2));
		json_dump_file(document_root[helpdoc_commands], "qw/help_commands.json", JSON_ENSURE_ASCII | JSON_SORT_KEYS | JSON_INDENT(2));
		json_dump_file(document_root[helpdoc_macros], "qw/help_macros.json", JSON_ENSURE_ASCII | JSON_SORT_KEYS | JSON_INDENT(2));
		json_dump_file(document_root[helpdoc_cmdlineparams], "qw/help_cmdline_params.json", JSON_ENSURE_ASCII | JSON_SORT_KEYS | JSON_INDENT(2));
	}
}

void Help_Init(void)
{
	Cmd_AddCommand("describe", Help_Describe_f);
	if (IsDeveloperMode()) {
		Cmd_AddCommand("dev_help_verify_config", Help_VerifyConfig_f);
		Cmd_AddCommand("dev_help_issues", Help_Issues_f);
	}

	Help_LoadDocuments();
}

void Help_Shutdown(void)
{
	Help_UnloadDocuments();
}
