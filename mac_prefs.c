/*
 *  Mac OS X Quakeworld preferences
 *
 *  Copyright (C) 2005 Se7en
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
 *  $Id: mac_prefs.c,v 1.1 2005-09-09 13:14:57 disconn3ct Exp $
 */

#include <Carbon/Carbon.h>
#include <string.h>

#include "mac_prefs.h"

prefs_t macPrefs;

void
PrefGetInt (CFStringRef key, int *value)
{
	CFNumberRef value_ref;
	value_ref = CFPreferencesCopyAppValue (key, kCFPreferencesCurrentApplication);

	*value = 0;
	if (value_ref)
	{
		CFNumberGetValue (value_ref, kCFNumberIntType, value);
		CFRelease (value_ref);
	}
}

void
PrefSetInt (CFStringRef key, int value)
{
	CFNumberRef value_ref;
	value_ref = CFNumberCreate (NULL, kCFNumberIntType, &value);
	CFPreferencesSetAppValue (key, value_ref, kCFPreferencesCurrentApplication);
}

void
PrefGetStr (CFStringRef key, char *value, size_t size)
{
	Boolean res;
	CFStringRef value_ref;
	const char *str;

	*value = 0;

	value_ref = CFPreferencesCopyAppValue (key, kCFPreferencesCurrentApplication);
	if (!value_ref)
		return;

	str = CFStringGetCStringPtr (value_ref, kCFStringEncodingMacRoman);
	if (!str)
	{
		res = CFStringGetCString (value_ref, value, size, kCFStringEncodingMacRoman);
		if (!res)
			*value = 0;
	}
	else
		strlcpy (value, str, size);
}

void
PrefSetStr (CFStringRef key, const char *value)
{
	CFStringRef value_ref = CFStringCreateWithCString (NULL, value, kCFStringEncodingMacRoman);
	CFPreferencesSetAppValue (key, value_ref, kCFPreferencesCurrentApplication);
}

void
LoadPrefs ()
{
	memset (&macPrefs, 0, sizeof(macPrefs));

	PrefGetInt (CFSTR("window"), &macPrefs.window);
	PrefGetInt (CFSTR("device"), &macPrefs.device);
	PrefGetInt (CFSTR("vid_mode"), &macPrefs.vid_mode);
	PrefGetInt (CFSTR("color_depth"), &macPrefs.color_depth);
	PrefGetInt (CFSTR("tex_depth"), &macPrefs.tex_depth);
	PrefGetInt (CFSTR("sound_rate"), &macPrefs.sound_rate);

	PrefGetStr (CFSTR("command_line"), macPrefs.command_line, PREF_CMDLINE_LEN-1);
	macPrefs.command_line[PREF_CMDLINE_LEN-1] = 0;
}

void
SavePrefs ()
{
	PrefSetInt (CFSTR("window"), macPrefs.window);
	PrefSetInt (CFSTR("device"), macPrefs.device);
	PrefSetInt (CFSTR("vid_mode"), macPrefs.vid_mode);
	PrefSetInt (CFSTR("color_depth"), macPrefs.color_depth);
	PrefSetInt (CFSTR("tex_depth"), macPrefs.tex_depth);
	PrefSetInt (CFSTR("sound_rate"), macPrefs.sound_rate);

	PrefSetStr (CFSTR("command_line"), macPrefs.command_line);

	CFPreferencesAppSynchronize (kCFPreferencesCurrentApplication);
}
