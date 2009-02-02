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
