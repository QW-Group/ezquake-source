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

typedef struct framebuffer_ref_s {
	int index;
} framebuffer_ref;

void GL_InitialiseFramebufferHandling(void);
framebuffer_ref GL_FramebufferCreate(int width, int height, qbool is3D);
void GL_FramebufferDelete(framebuffer_ref* pref);
void GL_FramebufferStartUsing(framebuffer_ref ref);
void GL_FramebufferStartUsingScreen(void);
texture_ref GL_FramebufferTextureReference(framebuffer_ref ref, int index);
int GL_FrameBufferWidth(framebuffer_ref ref);
int GL_FrameBufferHeight(framebuffer_ref ref);
void GL_FramebufferBlitSimple(framebuffer_ref source, framebuffer_ref destination);

extern const framebuffer_ref null_framebuffer_ref;

#define GL_FramebufferReferenceIsValid(x) ((x).index)
#define GL_FramebufferReferenceInvalidate(ref) { (ref).index = 0; }
#define GL_FramebufferReferenceEqual(ref1, ref2) ((ref1).index == (ref2).index)
#define GL_FramebufferReferenceCompare(ref1, ref2) ((ref1).index < (ref2).index ? -1 : (ref1).index > (ref2).index ? 1 : 0)

void GLM_FramebufferScreenDrawStart(void);
qbool GL_Framebuffer2DSwitch(void);
void GLM_FramebufferPostProcessScreen(void);
qbool GL_FramebufferEnabled2D(void);

#define USE_FRAMEBUFFER_SCREEN    1
#define USE_FRAMEBUFFER_3DONLY    2

#endif // __GL_FRAMEBUFFER_H__
