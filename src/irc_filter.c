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

#include "quakedef.h"

cvar_t irc_filter_join_part_messages = {"irc_filter_join_part_messages", "0"};
cvar_t irc_filter_quit_messages = {"irc_filter_quit_messages", "0"};
cvar_t irc_filter_private_messages = {"irc_filter_private_messages", "0"};
cvar_t irc_filter_notice_messages = {"irc_filter_notice_messages", "0"};
 
void IRC_filter_register_cvars() {
  Cvar_Register(&irc_filter_join_part_messages);
  Cvar_Register(&irc_filter_quit_messages);
  Cvar_Register(&irc_filter_private_messages);
  Cvar_Register(&irc_filter_notice_messages);
}

// These methods are deliberately a lot broader than the cvars above.

int IRC_filter_show_chanop_messages() {
  return (irc_filter_join_part_messages.integer == 0);
}

int IRC_filter_show_connection_messages() {
  return (irc_filter_quit_messages.integer == 0);
}

int IRC_filter_show_private_messages() {
  return (irc_filter_private_messages.integer == 0);
}

int IRC_filter_show_notice_messages() {
  return (irc_filter_notice_messages.integer == 0);
}

#endif
