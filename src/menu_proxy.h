/*
Proxy Menu module

Copyright (C) 2011 johnnycz

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

// catches key presses sent to proxy
void Menu_Proxy_Key(int key);

// main proxy menu drawing function
void Menu_Proxy_Draw(void);

// send signal to the proxy to toggle the menu
void Menu_Proxy_Toggle(void);
