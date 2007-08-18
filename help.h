#ifndef __HELP_H__
#define __HELP_H__

// variable description
void Help_VarDescription(const char *varname, char* buf, size_t bufleft);

// initialize help system
void Help_Init(void);

// help menu
void Menu_Help_Init(void);
void Menu_Help_Draw(int x, int y, int w, int h);
void Menu_Help_Key(int key);
qbool Menu_Help_Mouse_Event(const mouse_state_t *ms);

#endif // __HELP_H__
