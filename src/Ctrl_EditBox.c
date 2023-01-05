/*
Copyright (C) 2011 azazello

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

#include "quakedef.h"
#include "utils.h"
#include "Ctrl_EditBox.h"
#include "keys.h"


char buf[MAX_EDITTEXT+1];

void CEditBox_Init(CEditBox *e, int width, int max)
{
	e->width = width;
	e->max = max;
	e->pos = 0;
	e->disp = 0;
	e->text[0] = 0;
}

void CEditBox_Draw(CEditBox *e, int x, int y, qbool active)
{
	while (e->pos > e->disp + e->width - 1)
		e->disp ++;
	if (e->disp  >  e->pos)
		e->disp = e->pos;
	if (e->disp  >  strlen(e->text) - e->width + 1)
		e->disp = strlen(e->text) - e->width + 1;
//	if (e->disp  <  0)
//		e->disp = 0;

	snprintf(buf, sizeof (buf), "%-*.*s", e->width, e->width, active ? e->text+e->disp : e->text);
	Draw_String(x, y, buf);

	if (active)
		Draw_Character(x+8*(e->pos-e->disp), y, 10+((int)(cls.realtime*4)&1));
}

void CEditBox_Key(CEditBox *e, int key, wchar unichar)
{

	switch (key) {
			case K_LEFTARROW:
				if (e->pos != 0) {
					e->pos--;
				}
				break;
			case K_RIGHTARROW:
				e->pos++;
				break;
			case K_HOME:
				e->pos = 0;
				break;
			case K_END:
				e->pos = strlen(e->text);
				break;
			case K_DEL:
				if (e->pos < strlen(e->text))
					memmove(e->text + e->pos,
						e->text + e->pos + 1,
						strlen(e->text + e->pos + 1) + 1);
				break;
			case K_BACKSPACE:
				if (e->pos > 0) {
					memmove(e->text + e->pos - 1,
						e->text + e->pos,
						strlen(e->text + e->pos) + 1);
					e->pos --;
				}
			break;
			case 'v':
			case 'V':
				if (keydown[K_CTRL]) {
					int len;
					char *clip = ReadFromClipboard();
					len = min(strlen(clip), e->max - strlen(e->text));
	
					if (len > 0) {
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

	if (!keydown[K_CTRL] &&
		key >= ' '  &&  key <= '}' &&
		strlen(e->text) < e->max)
	{
		memmove(e->text + e->pos + 1,
			e->text + e->pos,
			strlen(e->text + e->pos) + 1);

		e->text[e->pos] = unichar;

		e->pos++;
	}
}
