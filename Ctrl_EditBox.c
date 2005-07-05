/*
 * EditBox functions
 *
 */


#include "quakedef.h"
#include "EX_misc.h"
#include "Ctrl_EditBox.h"

#include "keys.h"

char buf[MAX_EDITTEXT+1];

int isThatCtrlDown(void)
{
    extern qboolean    keydown[256];
	return keydown[K_CTRL] || keydown[K_RCTRL];
}

void CEditBox_Init(CEditBox *e, int width, int max)
{
    e->width = width;
    e->max = max;
    e->pos = 0;
    e->disp = 0;
    e->text[0] = 0;
}

void CEditBox_Draw(CEditBox *e, int x, int y, qboolean active)
{
    while (e->pos  >  e->disp + e->width - 1)
        e->disp ++;
    if (e->disp  >  e->pos)
        e->disp = e->pos;
    if (e->disp  >  strlen(e->text) - e->width + 1)
        e->disp = strlen(e->text) - e->width + 1;
    if (e->disp  <  0)
        e->disp = 0;

    sprintf(buf, "%-*.*s", e->width, e->width, active ? e->text+e->disp : e->text);
    Draw_String(x, y, buf);

    if (active)
        Draw_Character(x+8*(e->pos-e->disp), y, 10+((int)(cls.realtime*4)&1));
}

void CEditBox_Key(CEditBox *e, int key)
{
    switch (key)
    {
    case K_LEFTARROW:
        e->pos--; break;
    case K_RIGHTARROW:
        e->pos++; break;
    case K_HOME:
        e->pos = 0; break;
    case K_END:
        e->pos = strlen(e->text); break;
    case K_DEL:
        if (e->pos < strlen(e->text))
            memmove(e->text + e->pos,
                    e->text + e->pos + 1,
                    strlen(e->text + e->pos + 1) + 1);
        break;
    case K_BACKSPACE:
        if (e->pos > 0)
        {
            memmove(e->text + e->pos - 1,
                    e->text + e->pos,
                    strlen(e->text + e->pos) + 1);
            e->pos --;
        }
        break;
    case 'v':
    case 'V':
        if (isThatCtrlDown())
        {
            int len;
            char *clip = ReadFromClipboard();
            len = min(strlen(clip), e->max - strlen(e->text));

            if (len > 0)
            {
                memmove(e->text + e->pos + len,
                        e->text + e->pos,
                        strlen(e->text + e->pos) + 1);
                memcpy(e->text + e->pos, clip, len);
                e->pos += len;
            }
        }
        break;
    }

    e->pos = max(e->pos, 0);
    e->pos = min(e->pos, strlen(e->text));

    if (!isThatCtrlDown()  &&
        key >= ' '  &&  key <= '}'  &&
        strlen(e->text) < e->max)
    {
        memmove(e->text + e->pos + 1,
                e->text + e->pos,
                strlen(e->text + e->pos) + 1);
        e->text[e->pos] = key;
        e->pos++;
    }
}
