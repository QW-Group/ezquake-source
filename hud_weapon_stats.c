
#include "quakedef.h"
#include "teamplay.h"
#include "hud.h"

void SCR_HUD_MultiLineString(hud_t* hud, const char* in, qbool large_font, int alignment, float scale);

typedef enum
{
	wpNONE = 0,
	wpAXE,
	wpSG,
	wpSSG,
	wpNG,
	wpSNG,
	wpGL,
	wpRL,
	wpLG,
	wpMAX
} weaponName_t;

typedef struct wpType_s {
	int hits;			// hits with this weapon, for SG and SSG this is count of bullets
	int attacks;		// all attacks with this weapon, for SG and SSG this is count of bullets
} wpType_t;

typedef struct ws_player_s {
	int 		client;

	wpType_t	wpn[wpMAX];
} ws_player_t;

static ws_player_t ws_clients[MAX_CLIENTS];

static weaponName_t WS_NameToNum(const char *name)
{
	if ( !strcmp(name, "axe") )
		return wpAXE;
	if ( !strcmp(name, "sg") )
		return wpSG;
	if ( !strcmp(name, "ssg") )
		return wpSSG;
	if ( !strcmp(name, "ng") )
		return wpNG;
	if ( !strcmp(name, "sng") )
		return wpSNG;
	if ( !strcmp(name, "gl") )
		return wpGL;
	if ( !strcmp(name, "rl") )
		return wpRL;
	if ( !strcmp(name, "lg") )
		return wpLG;

	return wpNONE;
}

// reads weapon stats string from server/demo
void Parse_WeaponStats(char *s)
{
	int		client, arg;
	weaponName_t wp;

	Cmd_TokenizeString( s );

	arg = 1;

	client = atoi( Cmd_Argv( arg++ ) );

	if (client < 0 || client >= MAX_CLIENTS)
	{
		Com_DPrintf("Parse_WeaponStats: wrong client %d\n", client);
		return;
	}

	ws_clients[ client ].client = client; // no, its not stupid

	wp = WS_NameToNum( Cmd_Argv( arg++ ) );

	if ( wp == wpNONE )
	{
		Com_DPrintf("Parse_WeaponStats: wrong weapon\n");
		return;
	}

	ws_clients[ client ].wpn[wp].attacks = atoi( Cmd_Argv( arg++ ) );
	ws_clients[ client ].wpn[wp].hits    = atoi( Cmd_Argv( arg++ ) );
}

// called when new map spawned
void SCR_ClearWeaponStats(void)
{
	memset(ws_clients, 0, sizeof(ws_clients));
}

// 
static void SCR_CreateWeaponStatsPlayerText(ws_player_t *ws_cl, char* input_string, char* output_string, int max_length)
{
	char *s, tmp2[MAX_MACRO_STRING];
	int new_length = 0;

	*output_string = '\0';
	if (!ws_cl) {
		return;
	}

	// this limit len of string because TP_ParseFunChars() do not check overflow
	strlcpy(tmp2, input_string , sizeof(tmp2));
	strlcpy(tmp2, TP_ParseFunChars(tmp2, false), sizeof(tmp2));

	for (s = tmp2; *s && new_length < max_length - 1; ++s) {
		int wp;
		qbool percentage = (*s == '%');

		if (*s != '%' && *s != '#') {
			output_string[new_length++] = *s;
			continue;
		}

		if (*s == s[1]) {
			output_string[new_length++] = *s;
			++s;
			continue;
		}

		++s;
		wp = (int)s[0] - '0'; 
		if (wp <= wpNONE || wp >= wpMAX) {
			continue; // unsupported weapon
		}

		if (percentage) {
			float accuracy = (ws_cl->wpn[wp].attacks ? 100.0f * ws_cl->wpn[wp].hits / ws_cl->wpn[wp].attacks : 0);

			new_length += snprintf(output_string + new_length, max_length - new_length - 1, "%.1f", accuracy);
		}
		else {
			new_length += snprintf(output_string + new_length, max_length - new_length - 1, "%d", ws_cl->wpn[wp].hits);
		}
	}
	output_string[new_length] = '\0';
}

void SCR_HUD_WeaponStats(hud_t *hud)
{
	char content[128];
	int x, y;
	int i;
	int alignment;

	static cvar_t
		*hud_weaponstats_format = NULL,
		*hud_weaponstats_textalign,
		*hud_weaponstats_scale;

	if (hud_weaponstats_format == NULL) {
		// first time
		hud_weaponstats_format = HUD_FindVar(hud, "format");
		hud_weaponstats_textalign = HUD_FindVar(hud, "textalign");
		hud_weaponstats_scale = HUD_FindVar(hud, "scale");
	}

	alignment = 0;
	if (!strcmp(hud_weaponstats_textalign->string, "right"))
		alignment = 1;
	else if (!strcmp(hud_weaponstats_textalign->string, "center"))
		alignment = 2;

	i = (cl.spectator ? Cam_TrackNum() : cl.playernum);
	if (i < 0 || i >= MAX_CLIENTS) {
		HUD_PrepareDraw(hud, 0, 0, &x, &y);
		return;
	}

	SCR_CreateWeaponStatsPlayerText(&ws_clients[i], hud_weaponstats_format->string, content, sizeof(content));

	SCR_HUD_MultiLineString(hud, content, false, hud_weaponstats_textalign->integer, hud_weaponstats_scale->value);
}

void OnChange_scr_weaponstats (cvar_t *var, char *value, qbool *cancel)
{
	extern void CL_UserinfoChanged (char *key, char *value);

	// do not allow set "wpsx" to "0" instead set it to ""
	CL_UserinfoChanged("wpsx", strcmp(value, "0") ? value : "");
}

static void SCR_MvdWeaponStatsOn_f(void)
{
	cvar_t* show = Cvar_Find("hud_weaponstats_show");
	if (show) {
		Cvar_Set (show, "1");
	}
}

static void SCR_MvdWeaponStatsOff_f(void)
{
	cvar_t* show = Cvar_Find("hud_weaponstats_show");
	if (show) {
		Cvar_Set (show, "0");
	}
}

void WeaponStats_HUDInit(void)
{
	cvar_t* show_cvar;

	HUD_Register("weaponstats", NULL, "Weapon stats",
		0, ca_active, 0, SCR_HUD_WeaponStats,
		"0", "screen", "center", "bottom", "0", "0", "0", "0 0 0", NULL,
		"format", "&c990sg&r:%2 &c099ssg&r:%3 &c900rl&r:#7 &c009lg&r:%8",
		"textalign", "center",
		"scale", "1",
		NULL
	);

	show_cvar = Cvar_Find("hud_weaponstats_show");
	if (show_cvar) {
		show_cvar->OnChange = OnChange_scr_weaponstats;
	}
}

void WeaponStats_CommandInit(void)
{
	// Compatibility with old configs
	Cvar_SetCurrentGroup(CVAR_GROUP_SCREEN);
	Cmd_AddLegacyCommand("scr_weaponstats_order", "hud_weaponstats_format");
	Cmd_AddLegacyCommand("scr_weaponstats_frame_color", "hud_weaponstats_frame_color");
	Cmd_AddLegacyCommand("scr_weaponstats_scale", "hud_weaponstats_scale");
	Cmd_AddLegacyCommand("scr_weaponstats_y", "hud_weaponstats_y");
	Cmd_AddLegacyCommand("scr_weaponstats_x", "hud_weaponstats_x");
	Cvar_ResetCurrentGroup();

	Cmd_AddCommand ("+cl_wp_stats", SCR_MvdWeaponStatsOn_f);
	Cmd_AddCommand ("-cl_wp_stats", SCR_MvdWeaponStatsOff_f);
}
