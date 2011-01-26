/*
Internet Relay Chat Support

Copyright (C) 2009 johnnycz

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
/// initializes the IRC module, creates libircclient session 
void IRC_Init(void);

/// process new data waiting on the socket
void IRC_Update(void);

/// return the name of the active (currently selected) channel from the channel list
char* IRC_GetCurrentChan(void);

/// select following channel from the channels list as the current channel
void IRC_NextChan(void);

/// select previous channel from the channels list as the current channel
void IRC_PrevChan(void);
#endif

