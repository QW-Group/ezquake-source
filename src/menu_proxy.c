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
/*

	Proxy Menu module

	This stands rather apart from the Main Menu hierarchy,
	but we still use the menu structures.

	made by:
		johnnycz, Jan 2007
	last edit:
		$Id: menu_proxy.c,v 1.2 2007-03-11 06:01:41 disconn3ct Exp $

*/

#include "quakedef.h"
#include "menu.h"
#include "keys.h"


#define PROXYACCESS "say proxy:menu"
#define ProxyMenuToggle() Cbuf_AddText(PROXYACCESS "\n");
#define ProxyMenuCmd(x) Cbuf_AddText(PROXYACCESS " " x "\n")

// disconnection from proxy should make us leave menus
qbool CheckProxyConnection(void) {
	if (!CL_ConnectedToProxy()) {
		Com_Printf("You are not connected to a proxy.\n");
		M_LeaveMenus();
		return false;
	}

	return true;
}

void Menu_Proxy_Toggle(void)
{
	if (!CheckProxyConnection())
		return;

	ProxyMenuToggle();
}

void Menu_Proxy_Key(int key)
{
	if (!CheckProxyConnection())
		return;

	switch (key) {
	case K_ESCAPE: 
		ProxyMenuToggle(); 
		M_LeaveMenus();
		break;
	case K_MOUSE1: case K_ENTER:
		ProxyMenuCmd("select"); break;
	case K_BACKSPACE: ProxyMenuCmd("back"); break;

	case K_UPARROW: ProxyMenuCmd("up"); break;
	case K_DOWNARROW: ProxyMenuCmd("down"); break;
	case K_LEFTARROW: ProxyMenuCmd("left"); break;
	case K_RIGHTARROW: ProxyMenuCmd("right"); break;

	case K_PGDN: ProxyMenuCmd("pgdn"); break;
	case K_PGUP: ProxyMenuCmd("pgup"); break;
	case K_HOME: ProxyMenuCmd("home"); break;
	case K_END: ProxyMenuCmd("end"); break;
	case K_DEL: ProxyMenuCmd("delete"); break;
	case K_INS: ProxyMenuCmd("help"); break;
	}
}

void Menu_Proxy_Draw(void) { 
// proxy menu is usually drawn by centerprint messages
// but ofc this can be changed later .. don't forget to hack SCR_CheckDrawCenterString() then again
} 
