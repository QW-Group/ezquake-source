#include "quakedef.h"

#define CONSOLE_HELP_MARGIN 2

CTab_t help_tab;

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

void Help_VarDescription(const char *varname, char* buf, size_t bufleft)
{
	xml_variable_t *var;
	variable_enum_value_t *cv;

	var = XSD_Variable_Load(va("help/variables/%s.xml", varname));
	if (!var) return;

	if (var->description && strlen(var->description) > 1) {
		strncat(buf, var->description, bufleft);
		bufleft -= strlen(var->description);
		strncat(buf, "\n", bufleft--);
	}
	if (var->remarks) {
		strncat(buf, "remarks: ", bufleft);
		bufleft -= 9;
		strncat(buf, var->remarks, bufleft);
		bufleft -= strlen(var->remarks);
		strncat(buf, "\n", bufleft--);
	}
	switch (var->value_type) {
	case t_boolean:
		if (var->value.boolean_value.false_description) {
			strncat(buf, "0: ", bufleft); bufleft -= 3;
			strncat(buf, var->value.boolean_value.false_description, bufleft);
			bufleft -= strlen(var->value.boolean_value.false_description);
			strncat(buf, "\n", bufleft--);
		}
		if (var->value.boolean_value.true_description) {
			strncat(buf, "1: ", bufleft); bufleft -= 3;
			strncat(buf, var->value.boolean_value.true_description, bufleft);
			bufleft -= strlen(var->value.boolean_value.true_description);
			strncat(buf, "\n", bufleft--);
		}
		break;
	
	case t_float:
		if (var->value.float_description) {
			strncat(buf, var->value.float_description, bufleft);
			bufleft -= strlen(var->value.float_description);
			strncat(buf, "\n", bufleft--);
		}
		break;

	case t_integer:
		if (var->value.integer_description) {
			strncat(buf, var->value.integer_description, bufleft);
			bufleft -= strlen(var->value.integer_description);
			strncat(buf, "\n", bufleft--);
		}
		break;

	case t_string:
		if (var->value.string_description) {
			strncat(buf, var->value.string_description, bufleft);
			bufleft -= strlen(var->value.string_description);
			strncat(buf, "\n", bufleft--);
		}
		break;

	case t_enum:
		cv = var->value.enum_value;
		while(cv) {
			if (cv->name && cv->description) {
				strncat(buf, cv->name, bufleft); bufleft -= strlen(cv->name);
				strncat(buf, ":", bufleft--);
				strncat(buf, cv->description, bufleft); bufleft -= strlen(cv->description);
				strncat(buf, "\n", bufleft--);
				cv = cv->next;
			}
		}
		break;
	}

	XSD_Variable_Free( (xml_t *)var );
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


void Help_Key(int key)
{
	extern void M_Menu_Main_f (void);

    int handled = CTab_Key(&help_tab, key);

    if (!handled)
    {
        if (key == K_ESCAPE)
        {
            M_Menu_Main_f();
        }
    }
}

void Help_Draw(int x, int y, int w, int h)
{
    CTab_Draw(&help_tab, x, y, w, h);
}

void Help_Init(void)
{
    Cmd_AddCommand("describe", Help_Describe_f);

    // initialize tab control
    CTab_Init(&help_tab);

    Help_Browser_Init();
    Help_Files_Init();

    CTab_AddPage(&help_tab, "files", help_files, NULL, Help_Files_Draw, Help_Files_Key);
//    CTab_AddPage(&help_tab, "help", help_help, NULL, NULL, NULL);
    CTab_AddPage(&help_tab, "browser", help_browser, NULL, Help_Browser_Draw, Help_Browser_Key);
//    CTab_AddPage(&help_tab, "online", help_online, NULL, NULL, NULL);
//    CTab_AddPage(&help_tab, "options", help_options, NULL, NULL, NULL);

    Help_Browser_Focus();
}
