/*
Copyright (C) 2002 Quake done Quick

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef __MOVIE_AVI_H_

#define __MOVIE_AVI_H_

void Capture_InitAVI (void);
void Capture_Open (char *filename, char *codec, float fps, int audio_hz);
qboolean Capture_WriteVideo (int width, int height, unsigned char *rgba_pixel_buffer);
qboolean Capture_WriteAudio (int samples, unsigned char *stereo_16bit_sample_buffer);
void Capture_Close (void);

#endif
