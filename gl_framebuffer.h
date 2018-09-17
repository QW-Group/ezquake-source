/*
Copyright (C) 2011 ezQuake team

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

#ifndef __GL_FRAMEBUFFER_H__
#define __GL_FRAMEBUFFER_H__

typedef enum {
	framebuffer_none,
	framebuffer_std,
	framebuffer_hud,
	framebuffer_count
} framebuffer_id;

typedef enum {
	fbtex_standard,
	fbtex_bloom,
	fbtex_count
} fbtex_id;

void GL_InitialiseFramebufferHandling(void);
qbool GL_FramebufferCreate(framebuffer_id id, int width, int height, qbool is3D);
void GL_FramebufferDelete(framebuffer_id id);
void GL_FramebufferStartUsing(framebuffer_id id);
void GL_FramebufferStartUsingScreen(void);
texture_ref GL_FramebufferTextureReference(framebuffer_id id, fbtex_id tex_id);
int GL_FrameBufferWidth(framebuffer_id ref);
int GL_FrameBufferHeight(framebuffer_id ref);
void GL_FramebufferBlitSimple(framebuffer_id source, framebuffer_id destination);
const char* GL_FramebufferZBufferString(framebuffer_id ref);

void GL_FramebufferScreenDrawStart(void);
qbool GL_Framebuffer2DSwitch(void);
void GL_FramebufferPostProcessScreen(void);
qbool GL_FramebufferEnabled2D(void);
qbool GL_FramebufferEnabled3D(void);

#define USE_FRAMEBUFFER_SCREEN    1
#define USE_FRAMEBUFFER_3DONLY    2

#endif // __GL_FRAMEBUFFER_H__
