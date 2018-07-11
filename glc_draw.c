
#include "quakedef.h"
#include "gl_model.h"
#include "gl_local.h"

void GLC_DrawDisc(void)
{
	glDrawBuffer(GL_FRONT);
	Draw_Pic(vid.width - 24, 0, draw_disc);
	GL_EmptyImageQueue();
	glDrawBuffer(GL_BACK);
}
