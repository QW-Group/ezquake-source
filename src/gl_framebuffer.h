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
	// rendering
	framebuffer_std,
	framebuffer_std_ms,
	framebuffer_hud,
	framebuffer_hud_ms,
	// framebuffers used to resolve multisampling
	framebuffer_std_blit,
	framebuffer_std_blit_ms,
	framebuffer_hud_blit,
	framebuffer_hud_blit_ms,
	framebuffer_count
} framebuffer_id;

typedef enum {
	fbtex_standard,
	fbtex_depth,
	fbtex_bloom,
	fbtex_worldnormals,
	fbtex_count
} fbtex_id;

void GL_InitialiseFramebufferHandling(void);
qbool GL_FramebufferCreate(framebuffer_id id, int width, int height);
void GL_FramebufferDelete(framebuffer_id id);
void GL_FramebufferStartUsing(framebuffer_id id);
void GL_FramebufferStartUsingScreen(void);
texture_ref GL_FramebufferTextureReference(framebuffer_id id, fbtex_id tex_id);
int GL_FrameBufferWidth(framebuffer_id ref);
int GL_FrameBufferHeight(framebuffer_id ref);
const char* GL_FramebufferZBufferString(framebuffer_id ref);

void GL_FramebufferScreenDrawStart(void);
qbool GL_Framebuffer2DSwitch(void);
void GL_FramebufferPostProcessScreen(void);
qbool GL_FramebufferEnabled2D(void);
qbool GL_FramebufferEnabled3D(void);

qbool GL_FramebufferStartWorldNormals(framebuffer_id id);
qbool GL_FramebufferEndWorldNormals(framebuffer_id id);

int GL_FramebufferMultisamples(framebuffer_id framebuffer);
void GL_FramebufferDeleteAll(void);
int GL_FramebufferFxaaPreset(void);

#define USE_FRAMEBUFFER_SCREEN    1
#define USE_FRAMEBUFFER_3DONLY    2

#endif // __GL_FRAMEBUFFER_H__
