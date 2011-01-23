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
#ifdef FRAMEBUFFERS

// Framebuffer struct.
typedef struct fb_s 
{
	GLint	frame_buffer;		// The GL ID of the frame buffer.
	GLint	depth_buffer;		// The GL ID of the depth buffer.
	int		texture;			// The texture ID of the texture we're drawing to.
	int		depthtex;			// The depth texture.
	int		width;				// Width the frame buffer is drawn at.
	int		height;				// Height the frame buffer is drawn at.
	int		realwidth;
	int		realheight;
	float	ratio_h;
	float	ratio_w;
	int		x;					// X position to draw the frame buffer contents at.
	int		y;					// Y position to draw the frame buffer contents at.
} fb_t;

//
// Initialize framebuffer stuff, Loads procadresses and such.
//
void Framebuffer_Init (void);

//
// Initializes the main frame buffer object, which can be reused
// in several places. Use Framebuffer_Create(...) and create a new
// buffer for specialized use.
//
void Framebuffer_Main_Init(void);

//
// Enable writing to a specified frame buffer object.
// (Instead of drawing to the back buffer)
//
void Framebuffer_Enable (fb_t *fbs);

//
// Disable drawing to a specified frame buffer object.
// (Go back to drawing to the normal back buffer)
//
void Framebuffer_Disable (fb_t *fbs);

//
// Draws the specified frame buffer object onto a polygon
// with the coordinates / bounds specified in it.
//
void Framebuffer_Draw (fb_t *fbs);

//
// Creates a new framebuffer object in GL and adds it's specs
// to the frame buffer struct.
//
void Framebuffer_Create (fb_t *fbs);

extern fb_t	main_fb; // The main framebuffer that can be used generally to draw things offscreen.

extern qbool use_framebuffer;

#endif // FRAMEBUFFERS
#endif // __GL_FRAMEBUFFER_H__
