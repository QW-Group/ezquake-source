/*
 *  Mac Quakeworld
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 *  $Id: mac_prefs.h,v 1.1 2005-08-30 04:09:20 disconn3ct Exp $
 */

#ifndef mac_prefs_h
#define mac_prefs_h

#define PREF_CMDLINE_LEN	512

typedef struct
{
	int	window;				// was in a window on last quit if true
	
	int	vid_mode;			// Menu selection # for the screen resolution
	int	color_depth;			// Menu selection # for monitor bit depth
	int	tex_depth;			// Menu selection # for GL texture depth
	int	device;				// Menu selection # for monitor
	int	sound_rate;			// Menu selection # for the sound sampling rate

	char	command_line[PREF_CMDLINE_LEN];
	
	Point	glwindowpos;			// Last position of window in windowed mode
	
} prefs_t;

extern prefs_t macPrefs;

void LoadPrefs ();
void SavePrefs ();
void PrefGetInt (CFStringRef key, int *value);
void PrefSetInt (CFStringRef key, int value);
void PrefGetStr (CFStringRef key, char *value, size_t size);
void PrefSetStr (CFStringRef key, const char *value);

#endif
