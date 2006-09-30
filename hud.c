//
// HUD commands
//


#include "quakedef.h"
#include "common_draw.h"

#define sbar_last_width 320  // yeah yeah I know, *garbage* -> leave it be :>

#define  num_align_strings_x  5
char *align_strings_x[] = {
    "left",
    "center",
    "right",
    "before",
    "after"
};

#define  num_align_strings_y  6
char *align_strings_y[] = {
    "top",
    "center",
    "bottom",
    "before",
    "after",
    "console"
};

#define  num_snap_strings  9
char *snap_strings[] = {
    "screen",
    "top",
    "view",
    "sbar",
    "ibar",
    "hbar",
    "sfree",
    "ifree",
    "hfree",
};

// hud elements list
hud_t *hud_huds = NULL;

qbool HUD_RegExpMatch(const char *regexp, const char *matchstring)
{
	int offsets[1];
	pcre *re;
	char *error = NULL;
	int erroffset;
	int match = 0;

	re = pcre_compile(
			regexp,				// The pattern.
			PCRE_CASELESS,		// Case insensitive.
			&error,				// Error message.
			&erroffset,			// Error offset.
			NULL);				// use default character tables.

	if(error)
	{
		Q_free(error);
		error = NULL;

		Q_free(re);
		re = NULL;
		return false;
	}

	if((match = pcre_exec(re, NULL, matchstring, strlen(matchstring), 0, 0, offsets, 1)) >= 0)
	{
		Q_free(re);
		re = NULL;
		return true;
	}

	Q_free(re);
	re = NULL;
	return false;
}

qbool HUD_RegExpGetGroup(const char *regexp, const char *matchstring, const char **resultstring, int *resultlength, int group)
{
	int offsets[HUD_REGEXP_OFFSET_COUNT];
	pcre *re;
	char *error;
	int erroffset;
	int match = 0;

	re = pcre_compile(
			regexp,				// The pattern.
			PCRE_CASELESS,		// Case insensitive.
			&error,				// Error message.
			&erroffset,			// Error offset.
			NULL);				// use default character tables.

	if(error)
	{
		Q_free(error);
		error = NULL;

		Q_free(re);
		re = NULL;
		return false;
	}

	if((match = pcre_exec(re, NULL, matchstring, strlen(matchstring), 0, 0, offsets, HUD_REGEXP_OFFSET_COUNT)) >= 0)
	{
		int substring_length = 0;
		substring_length = pcre_get_substring (matchstring, offsets, match, group, resultstring);
		
		if (resultlength != NULL)
		{
			(*resultlength) = substring_length;
		}

		Q_free(re);
		re = NULL;

		return (substring_length != PCRE_ERROR_NOSUBSTRING && substring_length != PCRE_ERROR_NOMEMORY);
	}

	Q_free(re);
	re = NULL;
	return false;
}

char *HUD_ColorNameToRGB(char *color_name)
{
	if(!strncmp("red", color_name, 3))
	{
		return HUD_COLOR_RED;
	}
	else if(!strncmp("green", color_name, 5))
	{
		return HUD_COLOR_GREEN;
	}
	else if(!strncmp("blue", color_name, 4))
	{
		return HUD_COLOR_BLUE;
	}
	else if(!strncmp("black", color_name, 5))
	{
		return HUD_COLOR_BLACK;
	}
	else if(!strncmp("yellow", color_name, 6))
	{
		return HUD_COLOR_YELLOW;
	}
	else if(!strncmp("pink", color_name, 4))
	{
		return HUD_COLOR_PINK;
	}
	else if(!strncmp("white", color_name, 5))
	{
		return HUD_COLOR_WHITE;
	}

	return color_name;
}

void HUD_RGBValuesFromString(char *string, float *r, float *g, float *b, float *alpha)
{
	int offsets[HUD_REGEXP_OFFSET_COUNT];
	pcre *re;
	char *error;
	char *resultstring = NULL;
	int erroffset;
	int match = 0;

	re = pcre_compile(
			HUD_COLOR_REGEX,	// The pattern.
			PCRE_CASELESS,		// Case insensitive.
			&error,				// Error message.
			&erroffset,			// Error offset.
			NULL);				// use default character tables.

	if(error)
	{
		Q_free(error);
		error = NULL;

		Q_free(re);
		re = NULL;
		return;
	}

	if((match = pcre_exec(re, NULL, string, strlen(string), 0, 0, offsets, HUD_REGEXP_OFFSET_COUNT)) >= 0)
	{
		// R.
		if(pcre_get_substring (string, offsets, match, 1, &resultstring) > 0)
		{
			(*r) = min(Q_atoi(resultstring), 255) / 255.0;
		}
		else
		{
			(*r) = -1;
		}
		
		if(resultstring != NULL)
		{
			Q_free(resultstring);
			resultstring = NULL;
		}

		// G.
		if(pcre_get_substring (string, offsets, match, 2, &resultstring) > 0)
		{
			(*g) = min(Q_atoi(resultstring), 255) / 255.0;
		}
		else
		{
			(*g) = -1;
		}

		if(resultstring != NULL)
		{
			Q_free(resultstring);
			resultstring = NULL;
		}
		
		// B.
		if(pcre_get_substring (string, offsets, match, 3, &resultstring) > 0)
		{
			(*b) = min(Q_atoi(resultstring), 255) / 255.0;
		}
		else
		{
			(*b) = -1;
		}

		if(resultstring != NULL)
		{
			Q_free(resultstring);
			resultstring = NULL;
		}

		// Alpha.
		if(pcre_get_substring (string, offsets, match, 5, &resultstring) > 0)
		{
			(*alpha) = max(Q_atof(resultstring), 0);
		}
		else
		{
			(*alpha) = -1;
		}		

		if(resultstring != NULL)
		{
			Q_free(resultstring);
			resultstring = NULL;
		}
	}

	Q_free(re);
	re = NULL;
}

// hud plus func - show element
void HUD_Plus_f(void)
{
    char *t;
    hud_t *hud;

    if (Cmd_Argc() < 1)
        return;

    t = Cmd_Argv(0);
    if (strncmp(t, "+hud_", 5))
        return;

    hud = HUD_Find(t + 5);
    if (!hud)
    {
        // this should never happen...
        return;
    }

    if (!hud->show)
    {
        // this should never happen...
        return;
    }

    Cvar_Set(hud->show, "1");
}

// hud minus func - hide element
void HUD_Minus_f(void)
{
    char *t;
    hud_t *hud;

    if (Cmd_Argc() < 1)
        return;

    t = Cmd_Argv(0);
    if (strncmp(t, "-hud_", 5))
        return;

    hud = HUD_Find(t + 5);
    if (!hud)
    {
        // this should never happen...
        return;
    }

    if (!hud->show)
    {
        // this should never happen...
        return;
    }

    Cvar_Set(hud->show, "0");
}

// hud element func - describe it
// this also solves the TAB completion problem
void HUD_Func_f(void)
{
    int i;
    hud_t *hud;

    hud = HUD_Find(Cmd_Argv(0));

    if (!hud)
    {
        // this should never happen...
        Com_Printf("wtf?\n");
        return;
    }

    if (Cmd_Argc() > 1)
    {
        char buf[512];

        sprintf(buf, "hud_%s_%s", hud->name, Cmd_Argv(1));
        if (Cvar_FindVar(buf) != NULL)
        {
            Cbuf_AddText(buf);
            if (Cmd_Argc() > 2)
            {
                Cbuf_AddText(" ");
                Cbuf_AddText(Cmd_Argv(2));
            }
            Cbuf_AddText("\n");
        }
        else
        {
            Com_Printf("Trying \"%s\" - no such variable\n", buf);
        }
        return;
    }

    // description
    Com_Printf("%s\n\n", hud->description);

    // status
    if (hud->show != NULL)
        Com_Printf("Current status: %s\n",
            hud->show->value ? "shown" : "hidden");
    if (hud->frame != NULL)
        Com_Printf("Frame:          %s\n\n", hud->frame->string);
	if (hud->frame_color != NULL)
        Com_Printf("Frame color:          %s\n\n", hud->frame_color->string);

    // placement
    Com_Printf("Placement:        %s\n", hud->place->string);

    // alignment
    Com_Printf("Alignment (x y):  %s %s\n",
        hud->align_x->string, hud->align_y->string);

    // position
    Com_Printf("Offset (x y):     %d %d\n",
        (int)(hud->pos_x->value), (int)(hud->pos_y->value));

    // additional parameters
    if (hud->num_params > 0)
    {
        int prefix_l = strlen(va("hud_%s_", hud->name));
        Com_Printf("\nParameters:\n");
        for (i=0; i < hud->num_params; i++)
        {
            if (strlen(hud->params[i]->name) > prefix_l)
                Com_Printf("  %-15s %s\n", hud->params[i]->name + prefix_l,
                    hud->params[i]->string);
        }
    }
}

// find hud placement by string
// return 0 if error
int HUD_FindPlace(hud_t *hud)
{
    int i;
    hud_t *par;
    qbool out;
    char *t;

    // first try standard strings
    for (i=0; i < num_snap_strings; i++)
        if (!strcasecmp(hud->place->string, snap_strings[i]))
            break;
    if (i < num_snap_strings)
    {
        // found
        hud->place_num = i+1;
        hud->place_hud = NULL;
        return 1;
    }

    // then try another HUD element
    out = true;
    t = hud->place->string;
    if (hud->place->string[0] == '@')
    {
        // place inside
        out = false;
        t++;
    }
    par = hud_huds;
    while (par)
    {
        if (par != hud  &&  !strcmp(t, par->name))
        {
            hud->place_outside = out;
            hud->place_hud = par;
            hud->place_num = 1; // no meaning anyway
            return 1;
        }
        par = par->next;
    }

    // no way
    hud->place_num = 1; // screen
    hud->place_hud = NULL;
    return 0;
}

// find hud alignment by strings
// return 0 if error
int HUD_FindAlignX(hud_t *hud)
{
    int i;

    // first try standard strings
    for (i=0; i < num_align_strings_x; i++)
        if (!strcasecmp(hud->align_x->string, align_strings_x[i]))
            break;
    if (i < num_align_strings_x)
    {
        // found
        hud->align_x_num = i+1;
        return 1;
    }
    else
    {
        // error
        hud->align_x_num = 1;   // left
        return 0;
    }
}

int HUD_FindAlignY(hud_t *hud)
{
    int i;

    // first try standard strings
    for (i=0; i < num_align_strings_y; i++)
        if (!strcasecmp(hud->align_y->string, align_strings_y[i]))
            break;
    if (i < num_align_strings_y)
    {
        // found
        hud->align_y_num = i+1;
        return 1;
    }
    else
    {
        // error
        hud->align_y_num = 1;   // left
        return 0;
    }
}

// list hud elements
void HUD_List (void)
{
    hud_t *hud = hud_huds;

    Com_Printf("name            status\n");
    Com_Printf("--------------- ------\n");
    while (hud)
    {
        Com_Printf("%-15s %s\n", hud->name,
            hud->show->value ? "shown" : "hidden");
        hud = hud->next;
    }
}

// show the specified hud element
void HUD_Show_f (void)
{
    hud_t *hud;

    if (Cmd_Argc() != 2)
    {
        Com_Printf("Usage: show [<name> | all]\n");
        Com_Printf("Show given HUD element.\n");
        Com_Printf("use \"show all\" to show all elements.\n");
        Com_Printf("Current elements status:\n\n");
        HUD_List();
        return;
    }

    if (!strcasecmp(Cmd_Argv(1), "all"))
    {
        hud = hud_huds;
        while (hud)
        {
            Cvar_SetValue(hud->show, 1);
            hud = hud->next;
        }
    }
    else
    {
        hud = HUD_Find(Cmd_Argv(1));

        if (!hud)
        {
            Com_Printf("No such element: %s\n", Cmd_Argv(1));
            return;
        }

        Cvar_SetValue(hud->show, 1);
    }
}


// hide the specified hud element
void HUD_Hide_f (void)
{
    hud_t *hud;

    if (Cmd_Argc() != 2)
    {
        Com_Printf("Usage: hide [<name> | all]\n");
        Com_Printf("Hide given HUD element\n");
        Com_Printf("use \"hide all\" to hide all elements.\n");
        Com_Printf("Current elements status:\n\n");
        HUD_List();
        return;
    }

    if (!strcasecmp(Cmd_Argv(1), "all"))
    {
        hud = hud_huds;
        while (hud)
        {
            Cvar_SetValue(hud->show, 0);
            hud = hud->next;
        }
    }
    else
    {
        hud = HUD_Find(Cmd_Argv(1));

        if (!hud)
        {
            Com_Printf("No such element: %s\n", Cmd_Argv(1));
            return;
        }

        Cvar_SetValue(hud->show, 0);
    }
}

// move the specified hud element
void HUD_Toggle_f (void)
{
    hud_t *hud;

    if (Cmd_Argc() != 2)
    {
        Com_Printf("Usage: togglehud <name> | <variable>\n");
        Com_Printf("Show/hide given HUD element, or toggles variable value.\n");
        return;
    }

    hud = HUD_Find(Cmd_Argv(1));

    if (!hud)
    {
        // look for cvar
        cvar_t *var = Cvar_FindVar(Cmd_Argv(1));
        if (!var)
        {
            Com_Printf("No such element or variable: %s\n", Cmd_Argv(1));
            return;
        }
//        Cvar_Toggle(var);
	Cvar_Set (var, var->value ? "0" : "1");
        return;
    }

//    Cvar_Toggle(hud->show);
    Cvar_Set (hud->show, hud->show->value ? "0" : "1");
}

// move the specified hud element
void HUD_Move_f (void)
{
    hud_t *hud;

    if (Cmd_Argc() != 4 &&  Cmd_Argc() != 2)
    {
        Com_Printf("Usage: move <name> [<x> <y>]\n");
        Com_Printf("Set offset for given HUD element\n");
        return;
    }

    hud = HUD_Find(Cmd_Argv(1));

    if (!hud)
    {
        Com_Printf("No such element: %s\n", Cmd_Argv(1));
        return;
    }

    if (Cmd_Argc() == 2)
    {
        Com_Printf("Current %s offset is:\n", Cmd_Argv(1));
        Com_Printf("  x:  %s\n", hud->pos_x->string);
        Com_Printf("  y:  %s\n", hud->pos_y->string);
        return;
    }

    Cvar_SetValue(hud->pos_x, atof(Cmd_Argv(2)));
    Cvar_SetValue(hud->pos_y, atof(Cmd_Argv(3)));
}

// place the specified hud element
void HUD_Place_f (void)
{
    hud_t *hud;
    char temp[512];

    if (Cmd_Argc() < 2 || Cmd_Argc() > 3)
    {
        Com_Printf("Usage: move <name> [<area>]\n");
        Com_Printf("Place HUD element at given area.\n");
        Com_Printf("\nPossible areas are:\n");
        Com_Printf("  screen - screen area\n");
        Com_Printf("  top    - screen minus status bar\n");
        Com_Printf("  view   - view\n");
        Com_Printf("  sbar   - status bar\n");
        Com_Printf("  ibar   - inventory bar\n");
        Com_Printf("  hbar   - health bar\n");
        Com_Printf("  sfree  - status bar free area\n");
        Com_Printf("  ifree  - inventory bar free area\n");
        Com_Printf("  hfree  - health bar free area\n");
        Com_Printf("You can also use any other HUD element as a base alignment. In such case you should specify area as:\n");
        Com_Printf("  +elem  - if you want to place\n");
        Com_Printf("           it inside elem\n");
        Com_Printf("  -elem  - if you want to place\n");
        Com_Printf("           it outside elem\n");
        Com_Printf("Exaples:\n");
        Com_Printf("  place fps view\n");
        Com_Printf("  place fps -ping\n");
        return;
    }

    hud = HUD_Find(Cmd_Argv(1));

    if (!hud)
    {
        Com_Printf("No such element: %s\n", Cmd_Argv(1));
        return;
    }

    if (Cmd_Argc() == 2)
    {
        Com_Printf("Current %s placement: %s\n", hud->name, hud->place->string);
        return;
    }

    // place with helper
    strcpy(temp, hud->place->string);
    Cvar_Set(hud->place, Cmd_Argv(2));
    if (!HUD_FindPlace(hud))
    {
        Com_Printf("place: invalid area argument: %s\n", Cmd_Argv(2));
        Cvar_Set(hud->place, temp); // restore old value
    }
}

// align the specified hud element
void HUD_Align_f (void)
{
    hud_t *hud;

    if (Cmd_Argc() != 4  &&  Cmd_Argc() != 2)
    {
        Com_Printf("Usage: align <name> [<ax> <ay>]\n");
        Com_Printf("Set HUD element alignment\n");
        Com_Printf("\nPossible values for ax are:\n");
        Com_Printf("  left    - left area edge\n");
        Com_Printf("  center  - area center\n");
        Com_Printf("  right   - right area edge\n");
        Com_Printf("  before  - before area (left)\n");
        Com_Printf("  after   - after area (right)\n");
        Com_Printf("\nPossible values for ay are:\n");
        Com_Printf("  top     - screen top\n");
        Com_Printf("  center  - screen center\n");
        Com_Printf("  bottom  - screen bottom\n");
        Com_Printf("  before  - before area (top)\n");
        Com_Printf("  after   - after area (bottom)\n");
        Com_Printf("  console - below console\n");
        return;
    }

    hud = HUD_Find(Cmd_Argv(1));

    if (!hud)
    {
        Com_Printf("No such element: %s\n", Cmd_Argv(1));
        return;
    }

    if (Cmd_Argc() == 2)
    {
        Com_Printf("Current alignment for %s is:\n", Cmd_Argv(1));
        Com_Printf("  horizontal (x):  %s\n", hud->align_x->string);
        Com_Printf("  vertical (y):    %s\n", hud->align_y->string);
        return;
    }

    // validate and set
    Cvar_Set(hud->align_x, Cmd_Argv(2));
    if (!HUD_FindAlignX(hud))
        Com_Printf("align: invalid X alignment: %s\n", Cmd_Argv(2));

    Cvar_Set(hud->align_y, Cmd_Argv(3));
    if (!HUD_FindAlignY(hud))
        Com_Printf("align: invalid Y alignment: %s\n", Cmd_Argv(3));
}

// recalculate all elements
// should be called if some HUD parameters (like place)
// were changed directly by vars, not comands (like place)
// - after execing cfg or sth
void HUD_Recalculate(void)
{
    hud_t *hud = hud_huds;

    while (hud)
    {
        HUD_FindPlace(hud);
        HUD_FindAlignX(hud);
        HUD_FindAlignY(hud);

        hud = hud->next;
    }
}
void HUD_Recalculate_f(void)
{
    HUD_Recalculate();
}

// initialize
void HUD_Init(void)
{
    Cmd_AddCommand ("show", HUD_Show_f);
    Cmd_AddCommand ("hide", HUD_Hide_f);
    Cmd_AddCommand ("move", HUD_Move_f);
    Cmd_AddCommand ("place", HUD_Place_f);
    Cmd_AddCommand ("togglehud", HUD_Toggle_f);
    Cmd_AddCommand ("align", HUD_Align_f);
    Cmd_AddCommand ("hud_recalculate", HUD_Recalculate_f);

	// Register the hud items.
    CommonDraw_Init();
}

// calculate frame extents
void HUD_CalcFrameExtents(hud_t *hud, int width, int height,
                          int *fl, int *fr, int *ft, int *fb)
{
    if ((hud->flags  &  HUD_NO_GROW)  &&  hud->frame->value != 2)
    {
        *fl = *fr = *ft = *fb = 0;
        return;
    }

    if (hud->frame->value == 2) // treate text box separately
    {
        int ax = (width % 16);
        int ay = (height % 8);
        *fl = 8 + ax/2;
        *ft = 8 + ay/2;
        *fr = 8 + ax - ax/2;
        *fb = 8 + ay - ay/2;
    }
    else if (hud->frame->value > 0  &&  hud->frame->value <= 1)
    {
        int fx, fy;
        fx = 2;
        fy = 2;

        if (width > 8)
            fx <<= 1;

        if (height > 8)
            fy <<= 1;

        *fl = fx;
        *fr = fx;
        *ft = fy;
        *fb = fy;
    }
    else
    {
        // no frame at all
        *fl = *fr = *ft = *fb = 0;
    }
}

qbool HUD_OnChangeFrameColor(cvar_t *var, char *newval)
{
	char *new_color = HUD_ColorNameToRGB(newval); // converts "red" into "255 0 0", etc. or returns input as it was
	char buf[256];
	int hudname_len;
	hud_t* hud_elem;
	byte* b_colors;

	hudname_len = min(sizeof(buf), strlen(var->name)-strlen("_frame_color")-strlen("hud_"));
	strncpy(buf, var->name + 4, hudname_len);
	buf[hudname_len] = 0;
	hud_elem = HUD_Find(buf);

	b_colors = StringToRGB(new_color);

	hud_elem->frame_color_cache[0] = b_colors[0] / 255.0;
	hud_elem->frame_color_cache[1] = b_colors[1] / 255.0;
	hud_elem->frame_color_cache[2] = b_colors[2] / 255.0;

	return true;
}

// draw frame for HUD element
void HUD_DrawFrame(hud_t *hud, int x, int y, int width, int height)
{
    if (!hud->frame->value)
        return;

    if (hud->frame->value > 0  &&  hud->frame->value <= 1)
    {
        Draw_FadeBox(x, y, width, height,
			hud->frame_color_cache[0], hud->frame_color_cache[1], hud->frame_color_cache[2], hud->frame->value);
        return;
    }
    else
    {
        switch ((int)(hud->frame->value))
        {
        case 2:     // text box
            Draw_TextBox(x, y, width/8-2, height/8-2);
            break;
        default:    // more will probably come
            break;
        }
    }
}

// calculate element placement
qbool HUD_PrepareDrawByName(
    /* in  */ char *name, int width, int height,
    /* out */ int *ret_x, int *ret_y)
{
    hud_t *hud = HUD_Find(name);
    if (hud == NULL)
        return false; // error in C code
    return HUD_PrepareDraw(hud, width, height, ret_x, ret_y);
}

// calculate object extents and draws frame if needed
qbool HUD_PrepareDraw(
    /* in  */ hud_t *hud, int width, int height,
    /* out */ int *ret_x, int *ret_y)       // position
{
    extern vrect_t scr_vrect;
    int x, y;
    int fl, fr, ft, fb; // frame left, right, top and bottom
    int ax, ay, aw, ah; // area coordinates & sizes to align
    int bx, by, bw, bh; // accepted area to draw in

    if (cls.state < hud->min_state  ||  !hud->show->value)
        return false;

    HUD_CalcFrameExtents(hud, width, height, &fl, &fr, &ft, &fb);

    width  += fl + fr;
    height += ft + fb;

    //
    // placement
    //

    switch (hud->place_num)
    {
    default:
    case 1:     // screen
        bx = by = 0;
        bw = vid.width;
        bh = vid.height;
        break;
    case 2:     // top = screen - sbar
        bx = by = 0;
        bw = vid.width;
        bh = vid.height - sb_lines;
        break;
    case 3:     // view
        bx = scr_vrect.x;
        by = scr_vrect.y;
        bw = scr_vrect.width;
        bh = scr_vrect.height;
        break;
    case 4:     // sbar
        bx = 0;
        by = vid.height - sb_lines;
        bw = sbar_last_width;
        bh = sb_lines;
        break;
    case 5:     // ibar
        bw = sbar_last_width;
        bh = max(sb_lines - SBAR_HEIGHT, 0);
        bx = 0;
        by = vid.height - sb_lines;
        break;
    case 6:     // hbar
        bw = sbar_last_width;
        bh = min(SBAR_HEIGHT, sb_lines);
        bx = 0;
        by = vid.height - bh;
        break;
    case 7:     // sfree
        bx = sbar_last_width;
        by = vid.height - sb_lines;
        bw = vid.width - sbar_last_width;
        bh = sb_lines;
        break;
    case 8:     // ifree
        bw = vid.width - sbar_last_width;
        bh = max(sb_lines - SBAR_HEIGHT, 0);
        bx = sbar_last_width;
        by = vid.height - sb_lines;
        break;
    case 9:     // hfree
        bw = vid.width - sbar_last_width;
        bh = min(SBAR_HEIGHT, sb_lines);
        bx = sbar_last_width;
        by = vid.height - bh;
        break;
    }

    if (hud->place_hud == NULL)
    {
        // accepted boundaries are our area
        ax = bx;
        ay = by;
        aw = bw;
        ah = bh;
    }
    else
    {
        // out area is our parent area
        ax = hud->place_hud->lx;
        ay = hud->place_hud->ly;
        aw = hud->place_hud->lw;
        ah = hud->place_hud->lh;

        if (hud->place_outside)
        {
            ax -= hud->place_hud->al;
            ay -= hud->place_hud->at;
            aw += hud->place_hud->al + hud->place_hud->ar;
            ah += hud->place_hud->at + hud->place_hud->ab;
        }
    }

    //
    // horizontal pos
    //

    switch (hud->align_x_num)
    {
    default:
    case 1:     // left
        x = ax;
        break;
    case 2:     // center
        x = ax + (aw - width) / 2;
        break;
    case 3:     // right
        x = ax + aw - width;
        break;
    case 4:     // before
        x = ax - width;
        break;
    case 5:     // after
        x = ax + aw;
        break;
    }

    x += hud->pos_x->value;

    //
    // vertical pos
    //

    switch (hud->align_y_num)
    {
    default:
    case 1:     // top
        y = ay;
        break;
    case 2:     // center
        y = ay + (ah - height) / 2;
        break;
    case 3:     // bottom
        y = ay + ah - height;
        break;
    case 4:     // before
        y = ay - height;
        break;
    case 5:     // after
        y = ay + ah;
        break;
    case 6:     // console - below console
        y = max(ay, scr_con_current);
        break;
    }

    y += hud->pos_y->value;

    //
    // find if on-screen
    //

//    if (x < bx  ||  x + width > bx+bw  ||
//        y < by  ||  y + height > by+bh)
    if (x < 0  ||  x + width > vid.width  ||
        y < 0  ||  y + height > vid.height)
    {
        return false;
    }
    else
    {
        // draw frame
        HUD_DrawFrame(hud, x, y, width, height);

        // assign values
        *ret_x = x + fl;
        *ret_y = y + ft;

        // remember values for children
        hud->lx = x + fl;
        hud->ly = y + ft;
        hud->lw = width - fl - fr;
        hud->lh = height - ft - fb;
        hud->al = fl;
        hud->ar = fr;
        hud->at = ft;
        hud->ab = fb;

        // remember drawing sequence
        hud->last_draw_sequence = host_screenupdatecount;
        return true;
    }
}


cvar_t * HUD_CreateVar(char *hud_name, char *subvar, char *value)
{
    cvar_t *var;
    char buf[128];

    sprintf(buf, "hud_%s_%s", hud_name, subvar);
    var = (cvar_t *)Q_malloc(sizeof(cvar_t));
    memset(var, 0, sizeof(cvar_t));
    // set name
	var->name = Q_strdup(buf);

    // default
//    Cvar_SetDefault(var, value);    // sets value and default value
	var->string = Q_strdup(value);

//  var->group = CVAR_HUD;
//  var->hidden = true;
    var->flags = CVAR_ARCHIVE;

    Cvar_SetCurrentGroup(CVAR_GROUP_HUD);
    Cvar_Register(var);
    Cvar_ResetCurrentGroup();
    return var;
}

// add element to list
hud_t * HUD_Register(char *name, char *var_alias, char *description,
                     int flags, cactive_t min_state, int draw_order,
                     hud_func_type draw_func,
                     char *show, char *place, char *align_x, char *align_y,
                     char *pos_x, char *pos_y, char *frame, char *frame_color,
                     char *params, ...)
{
    // FIXME: should use zmalloc instaed?

    int i;
    va_list     argptr;
    hud_t  *hud;
    char *subvar;

    hud = (hud_t *) Q_malloc(sizeof(hud_t));
    memset(hud, 0, sizeof(hud_t));
    hud->next = hud_huds;
    hud_huds = hud;
    hud->min_state = min_state;
    hud->draw_order = draw_order;
    hud->draw_func = draw_func;

    hud->name = (char *) Q_malloc(strlen(name)+1);
    strcpy(hud->name, name);

    hud->description = (char *) Q_malloc(strlen(description)+1);
    strcpy(hud->description, description);

    hud->flags = flags;

    Cmd_AddCommand(name, HUD_Func_f);

    // create standard variables

    // place
    hud->place = HUD_CreateVar(name, "place", place);
    i = HUD_FindPlace(hud);
    if (i == 0)
    {
        // error in C code, probably parent should be registered earlier
        Con_SafePrintf("HUD_Register: couldn't find place for %s\n", hud->name);
        hud->place_num = 0;
        hud->place_hud = NULL;
    }

    // show
    if (show)
    {
        hud->show = HUD_CreateVar(name, "show", show);
/*
hexum -> *FIXME* Don't support aliases for now, only used for show_fps and r_netgraph anyhow
        if (var_alias)
            Cvar_RegisterAlias(hud->show, var_alias);
*/

        if (flags & HUD_PLUSMINUS)
        {
            // add plus and minus commands
            Cmd_AddCommand(Q_strdup(va("+hud_%s", name)), HUD_Plus_f);
            Cmd_AddCommand(Q_strdup(va("-hud_%s", name)), HUD_Minus_f);
        }
    }
    else
        hud->flags |= HUD_NO_SHOW;

    // X align & pos
    if (pos_x  &&  align_x)
    {
        hud->pos_x = HUD_CreateVar(name, "pos_x", pos_x);
        hud->align_x = HUD_CreateVar(name, "align_x", align_x);
    }
    else
        hud->flags |= HUD_NO_POS_X;

    // Y align & pos
    if (pos_y  &&  align_y)
    {
        hud->pos_y = HUD_CreateVar(name, "pos_y", pos_y);
        hud->align_y = HUD_CreateVar(name, "align_y", align_y);
    }
    else
        hud->flags |= HUD_NO_POS_Y;

    // frame
    if (frame)
    {
        hud->frame = HUD_CreateVar(name, "frame", frame);
        hud->params[hud->num_params++] = hud->frame;

		hud->frame_color = HUD_CreateVar(name, "frame_color", frame_color);
		hud->frame_color->OnChange = HUD_OnChangeFrameColor;
        hud->params[hud->num_params++] = hud->frame_color;
    }
    else
        hud->flags |= HUD_NO_FRAME;
    
    // create parameters
    subvar = params;
    va_start (argptr, params);

    while (subvar)
    {
        char *value = va_arg(argptr, char *);
        if (value == NULL  ||  hud->num_params >= HUD_MAX_PARAMS)
            Sys_Error("HUD_Register: HUD_MAX_PARAMS overflow");

        hud->params[hud->num_params] = HUD_CreateVar(name, subvar, value);
        hud->num_params ++;
        subvar = va_arg(argptr, char *);
    }
    
    va_end (argptr);

    return hud;
}

// find element in list
hud_t * HUD_Find(char *name)
{
    hud_t *hud = hud_huds;

    while (hud)
    {
        if (!strcmp(hud->name, name))
            return hud;
        hud = hud->next;
    }
    return NULL;
}

// retrieve hud cvar
cvar_t *HUD_FindVar(hud_t *hud, char *subvar)
{
    int i;
    char buf[128];

    snprintf(buf, sizeof(buf), "hud_%s_%s", hud->name, subvar);

    for (i=0; i < hud->num_params; i++)
        if (!strcmp(buf, hud->params[i]->name))
            return hud->params[i];

    return NULL;
}

// draws single HUD element
void HUD_DrawObject(hud_t *hud)     /* recurrent */
{
    extern qbool /*scr_drawdialog,*/ sb_showscores, sb_showteamscores;

    if (hud->last_try_sequence == host_screenupdatecount)
        return; // allready tried to draw this frame

    hud->last_try_sequence = host_screenupdatecount;

    // check if we should draw this
    if (!hud->show->value)
        return;
    if (cls.state < hud->min_state)
        return;
/*    if (scr_drawdialog        &&  !(hud->flags & HUD_ON_DIALOG))
        return;*/
    if (cl.intermission == 1  &&  !(hud->flags & HUD_ON_INTERMISSION))
        return;
    if (cl.intermission == 2  &&  !(hud->flags & HUD_ON_FINALE))
        return;
    if ((sb_showscores || sb_showteamscores)    &&  !(hud->flags & HUD_ON_SCORES))
        return;

    if (hud->place_hud)
    {
        // parent should be drawn it should be first
        HUD_DrawObject(hud->place_hud);

        // if parent was not drawn, we refuse to draw too
        if (hud->place_hud->last_draw_sequence < host_screenupdatecount)
            return;
    }

    // draw object itself - updates last_draw_sequence itself
    hud->draw_func(hud);

    // last_draw_sequence is update by HUD_PrepareDraw
    // if object was succesfully drawn (wasn't outside area etc..)
}

// draw all active elements
void HUD_Draw(void)
{
extern cvar_t scr_newHud;
    hud_t *hud;

    if (scr_newHud.value == 0)
	return;
    hud = hud_huds;

    while (hud)
    {
        // draw
        HUD_DrawObject(hud);

        // go to next
        hud = hud->next;
    }
}

int HUD_OrderFunc(const void * p_h1, const void * p_h2)
{
    const hud_t *h1 = *((hud_t **)p_h1);
    const hud_t *h2 = *((hud_t **)p_h2);

    return h1->draw_order - h2->draw_order;
}

// last phase of initialization
void HUD_InitFinish(void)
{
    // sort elements due to them draw_order
    int i;
    hud_t *huds[MAX_HUD_ELEMENTS];
    int count = 0;
    hud_t *hud;

    // copy to table
    hud = hud_huds;
    while (hud)
    {
        huds[count++] = hud;
        hud = hud->next;
    }

    if (count <= 0)
        return;

    // sort table
    qsort(huds, count, sizeof(huds[0]), HUD_OrderFunc);

    // back to list
    hud_huds = huds[0];
    hud = hud_huds;
    hud->next = NULL;
    for (i=1; i < count; i++)
    {
        hud->next = huds[i];
        hud = hud->next;
        hud->next = NULL;
    }

    // recalculate elements so vars are parsed
    HUD_Recalculate();
}
