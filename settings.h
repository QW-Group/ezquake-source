// used by settings_page and menu_options modules

//
// Macros
// Use following macros to create array of 'setting' variables
// see settings_page.h for further info
//

// separator: decorating purpose
#define ADDSET_SEPARATOR(label) { stt_separator, label, NULL, 0, 0, 0, NULL, NULL, NULL, NULL }

// action: user can hit enter to execute function assigned to this setting
#define ADDSET_ACTION(label,fnc) { stt_action, label, NULL, 0, 0, 0, NULL, NULL, fnc, NULL }

// number: will show a slider allowing you to choose setting from range min..max 
#define ADDSET_NUMBER(label, var, min, max, step) { stt_num, label, &var, min, max, step, NULL, NULL, NULL, NULL }

// bool: will display on/off option
#define ADDSET_BOOL(label, var) { stt_bool, label, &var, 0, 0, 0, NULL, NULL, NULL, NULL }

// named: give it array of strings, will assign values 0, 1, ... to the variable
#define ADDSET_NAMED(label, var, strs) { stt_named, label, &var, 0, sizeof(strs)/sizeof(char*)-1, 1, NULL, NULL, NULL, strs }

// custom: completely customizable setting, define your own reading and writing function
// see below for function types
#define ADDSET_CUSTOM(label, readfnc, togglefnc) { stt_custom, label, NULL, 0, 0, 0, readfnc, togglefnc, NULL, NULL }


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
	stt_action		// function is assigned to this, pointer must be stored in togglefnc
} setting_type;

typedef struct {
	setting_type type;	// see above; always required
	const char* label;	// to be displayed on screen; always required
	cvar_t* cvar;		// assigned variable; required for num, bool, named
	float min;			// minimal value; required for num, named
	float max;			// maximal value; required for num, named
	float step;			// change step; required for num, named
	enum_readfnc readfnc;		// reading function pointer; required for enum
	enum_togglefnc togglefnc;	// toggle function pointer; required for enum
	action_fnc actionfnc;		// action function pointer; required for stt_action
	const char** named_ints;	// array of strings; required for sett_named
	int top;					// for internal rendering purposes
} setting;

typedef struct {
	setting* settings;	// array of settings
	int count;			// amount of elements in set_tab
	int marked;			// currently selected element in set_tab
	int viewpoint;		// where rendering start (internal)
} settings_page;
