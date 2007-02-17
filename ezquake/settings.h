// used by settings_page and menu_options modules

//
// Macros
// Use following macros to create array of 'setting' variables
// see settings_page.h for further info
//

// start advanced settings section
#define ADDSET_ADVANCED_SECTION() { stt_advmark, NULL, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// starts basic settings setion (terminates advanced settings section) (started by default)
#define ADDSET_BASIC_SECTION() { stt_basemark, NULL, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// separator: decorating purpose
#define ADDSET_SEPARATOR(label) { stt_separator, label, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// action: user can hit enter to execute function assigned to this setting
#define ADDSET_ACTION(label,fnc) { stt_action, label, false, NULL, 0, 0, 0, NULL, NULL, fnc, NULL }

// number: will show a slider allowing you to choose setting from range min..max 
#define ADDSET_NUMBER(label, var, min, max, step) { stt_num, label, false, &var, min, max, step, NULL, NULL, NULL, NULL }

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

// bind - not implemented (was too scared about all those ifdefs)
#define ADDSET_BIND(label, cmd) { stt_bind, label, false, NULL, 0, 0, 0, NULL, NULL, NULL, NULL, cmd }

// custom: completely customizable setting, define your own reading and writing function
// see below for function types
#define ADDSET_CUSTOM(label, readfnc, togglefnc) { stt_custom, label, false, NULL, 0, 0, 0, readfnc, togglefnc, NULL, NULL }


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
	stt_separator,	// decorating purpose only, needs only type+label then
	stt_num,		// integer or float variable, needs cvar, min, max and step are required
	stt_bool,		// simple boolean setting, needs cvar
	stt_custom,		// fully customizable setting, needs readfnc and togglefnc
	stt_named,		// named integer 0..max, max is number of elements in array of strings assigned to readfnc
	stt_action,		// function is assigned to this, pointer must be stored in togglefnc
	stt_string,		// string - fully editable by the user, needs only cvar
	stt_playercolor,// todo - named enum 0..13
	stt_bind,		// keybinding, not implemented
	stt_advmark,	// denotes advanced settings area
	stt_basemark   // denotes basic settings area
} setting_type;

typedef struct {
	setting_type type;	// see above; always required
	const char* label;	// to be displayed on screen; always required
	qbool advanced;		// is this settings advanced?
	cvar_t* cvar;		// assigned variable; required for num, bool, named
	float min;			// minimal value; required for num, named
	float max;			// maximal value; required for num, named
	float step;			// change step; required for num, named
	enum_readfnc readfnc;		// reading function pointer; required for enum
	enum_togglefnc togglefnc;	// toggle function pointer; required for enum
	action_fnc actionfnc;		// action function pointer; required for stt_action
	const char** named_ints;	// array of strings; required for sett_named
	const char*	varname;		// name of a non-static cvar_t, also used for command name for bind
	int top;					// distance of the setting from the top of the settings page
} setting;

typedef struct {
	setting* settings;	// array of settings
	int count;			// amount of elements in set_tab
	int marked;			// currently selected element in set_tab
	int viewpoint;		// where rendering start (internal)
	enum { SPM_NORMAL, SPM_BINDING } mode;
} settings_page;
