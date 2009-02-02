
// initialize the multiplayer menu
void Menu_MultiPlayer_Init(void);

void Menu_MultiPlayer_Draw (void);

void Menu_MultiPlayer_Key(int key, wchar unichar);

qbool Menu_MultiPlayer_Mouse_Event(const mouse_state_t *ms);

void Menu_MultiPlayer_SwitchToServersTab(void);

typedef enum
{
	SBPG_SERVERS,	// Servers page
	SBPG_SOURCES,	// Sources page
	SBPG_PLAYERS,	// Players page
	SBPG_OPTIONS,	// Options page
	SBPG_CREATEGAME   // Host Game page
} sb_tab_t;
extern CTab_t sb_tab;

