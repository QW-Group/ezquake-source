/*
Copyright (C) 2011 johnnycz

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
// used by settings_page and menu_options modules

//
// Macros
// Use following macros to create array of 'setting' variables
// see settings_page.h for further info
//

#ifndef __SETTINGS_H__
#define __SETTINGS_H__

#include "Ctrl.h"

// start advanced settings section
#define ADDSET_ADVANCED_SECTION() { stt_advmark, NULL, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// starts basic settings setion (terminates advanced settings section) (started by default)
#define ADDSET_BASIC_SECTION() { stt_basemark, NULL, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// separator: decorating purpose
#define ADDSET_SEPARATOR(label) { stt_separator, label, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// blank: decorating purpose
#define ADDSET_BLANK() { stt_blank, NULL, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// action: user can hit enter to execute function assigned to this setting
#define ADDSET_ACTION(label,fnc,desc) { stt_action, label, false, NULL, 0, 0, 0, NULL, NULL, fnc, NULL, NULL, desc }

// number: will show a slider allowing you to choose setting from range min..max 
#define ADDSET_NUMBER(label, var, min, max, step) { stt_num, label, false, &var, min, max, step, NULL, NULL, NULL, NULL }

// intnumber: same like number, but we change a "int", not a "cvar_t"
#define ADDSET_INTNUMBER(label, var, min, max, step) { stt_intnum, label, false, (cvar_t *) &var, min, max, step, NULL, NULL, NULL, NULL }

// enum: custom name for each custom value .. works like "named", but you specify the range of values too
#define ADDSET_ENUM(label, var, strs) { stt_enum, label, false, &var, 0, (sizeof(strs)/sizeof(char*))/2-1, 1, NULL, NULL, NULL, strs }

// bool: will display on/off option
#define ADDSET_BOOL(label, var) { stt_bool, label, false, &var, 0, 0, 0, NULL, NULL, NULL, NULL }

// latebool: use for cvars that don't exist on compile time - be carefull when using this, may lead to crashes!
#define ADDSET_BOOLLATE(label, var) { stt_bool, label, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, #var }

// string: will show an edit box allowing you to edit teh value
#define ADDSET_STRING(label, var) { stt_string, label, false, &var, 0, 0, 0, NULL, NULL, NULL, NULL }

// named: give it array of strings, will assign values 0, 1, ... to the variable
#define ADDSET_NAMED(label, var, strs) { stt_named, label, false, &var, 0, sizeof(strs)/sizeof(char*)-1, 1, NULL, NULL, NULL, strs }

// color
#define ADDSET_COLOR(label, var) { stt_playercolor, label, false, &var, -1, 13, 1, NULL, NULL, NULL, NULL }

// key bind
#define ADDSET_BIND(label, cmd) { stt_bind, label, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, cmd }

// custom: completely customizable setting, define your own reading and writing function
// see below for function types
#define ADDSET_CUSTOM(label, readfnc, togglefnc, desc) { stt_custom, label, false, NULL, 0, 0, 0, readfnc, togglefnc, NULL, NULL, NULL, desc }

// skin setting
#define ADDSET_SKIN(label, var) { stt_skin, label, false, &var, 0, 0, 0, NULL, NULL, NULL, NULL, NULL }


//
// function types:
//

typedef const char* (*enum_readfnc) (void);
typedef void (*enum_togglefnc) (qbool);
typedef void (*action_fnc) (void);


//
// internal structures
//

typedef enum  {
	stt_separator,             // decorating purpose only, needs only type+label then
	stt_num,                   // integer or float variable, needs cvar, min, max and step are required
	stt_intnum,                // integer non-quake-variable
	stt_bool,                  // simple boolean setting, needs cvar
	stt_custom,                // fully customizable setting, needs readfnc and togglefnc
	stt_named,                 // named integer 0..max, max is number of elements in array of strings assigned to readfnc
	stt_enum,                  // named enum, pairs of "name", "value"
	stt_action,                // function is assigned to this, pointer must be stored in togglefnc
	stt_string,                // string - fully editable by the user, needs only cvar
	stt_playercolor,           // named enum 0..13
	stt_skin,                  // player skin
	stt_bind,                  // keybinding
	stt_advmark,               // denotes advanced settings area
	stt_basemark,              // denotes basic settings area
	stt_blank
} setting_type;

typedef struct {
	setting_type type;         // see above; always required
	const char* label;         // to be displayed on screen; always required
	qbool advanced;            // is this settings advanced?
	cvar_t* cvar;              // assigned variable; required for num, bool, named
	float min;                 // min value; required for num, named
	float max;                 // max value; required for num, named
	float step;                // change step; required for num, named
	enum_readfnc readfnc;      // reading function pointer; required for enum
	enum_togglefnc togglefnc;  // toggle function pointer; required for enum
	action_fnc actionfnc;      // action function pointer; required for stt_action
	const char** named_ints;   // array of strings; required for sett_named and stt_enum
	const char* varname;       // name of a non-static cvar_t, also used for command name for bind
	const char* description;   // manual-like description
	int top;                   // distance of the setting from the top of the settings page
} setting;

typedef struct {
	setting* settings;         // array of settings
	int count;                 // amount of elements in set_tab
	int marked;                // currently selected element in settings
	int viewpoint;		   // where rendering start (internal)
	PScrollBar  scrollbar;     // scrollbar gui element
	enum {
		SPM_NORMAL,
		SPM_BINDING,
		SPM_VIEWHELP,
		SPM_CHOOSESKIN
	} mode;
	int width;                 // last drawn width
	int height;                // last drawn height
	qbool mini;                // minimalistic version (doesn't display help and has infinite scrolling)
} settings_page;

char* SettingColorName(int color);

#endif // __SETTINGS_H__
