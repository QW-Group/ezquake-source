// used by settings_page and menu_options modules

typedef enum  {
	stt_separator,
	stt_num,
	stt_bool,
	stt_enum,
	stt_named,
	stt_action
} setting_type;

typedef struct {
	setting_type type;
	const char* label;
	cvar_t* cvar;
	float min;
	float max;
	float step;
	const void *readfnc;
	void *togglefnc;
} setting ;

typedef struct {
	setting* set_tab;
	int set_count;
	int set_marked;
} settings_tab;
