/*
Copyright (C) 2011 azazello and ezQuake team

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
/*
 * EditBox functions
 *
 *    $Id: Ctrl_EditBox.h,v 1.4 2006-05-13 07:31:57 disconn3ct Exp $
 */

#ifndef __CTRL_EDITBOX_H__
#define __CTRL_EDITBOX_H__

#define MAX_EDITTEXT 255

typedef struct CEditBox_s
{
	char text[MAX_EDITTEXT+1];
	unsigned int width;
	unsigned int max;
	unsigned int pos;
	unsigned int disp;
} CEditBox;

void CEditBox_Init(CEditBox *e, int width, int max);
void CEditBox_Draw(CEditBox *e, int x, int y, qbool active);
void CEditBox_Key(CEditBox *e, int key, wchar unichar);

#endif // __CTRL_EDITBOX_H__
