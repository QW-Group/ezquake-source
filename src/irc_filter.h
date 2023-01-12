/*
 Internet Relay Chat filtering methods
 
 Copyright (C) 2011 Andrew 'dies-el' Donaldson
 
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

#ifdef WITH_IRC

// Registers the cvars that these filters rely on.
void IRC_filter_register_cvars(void);

// Returns true if chanop messages (join/parts/etc) are to be shown
int IRC_filter_show_chanop_messages(void);

// Returns true if connection messages (quits) are to be shown
int IRC_filter_show_connection_messages(void);

// Returns true if private messages are to be shown
int IRC_filter_show_private_messages(void);

// Returns true if notices are to be shown
int IRC_filter_show_notice_messages(void);

#endif
