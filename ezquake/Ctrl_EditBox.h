/*
 * EditBox functions
 *
 */

#ifndef __CTRL_EDITBOX_H__
#define __CTRL_EDITBOX_H__

#define MAX_EDITTEXT 255

typedef struct CEditBox_s
{
    char text[MAX_EDITTEXT+1];
    int width;
    int max;
    int pos;
    int disp;
} CEditBox;

void CEditBox_Init(CEditBox *e, int width, int max)
;
void CEditBox_Draw(CEditBox *e, int x, int y, qboolean active)
;
void CEditBox_Key(CEditBox *e, int key)
;

#endif // __CTRL_EDITBOX_H__
