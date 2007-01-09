/*
	Proxy Menu module
	$Id: menu_proxy.h,v 1.1 2007-01-09 20:09:52 johnnycz Exp $
*/

// catches key presses sent to proxy
void Menu_Proxy_Key(int key);

// main proxy menu drawing function
void Menu_Proxy_Draw(void);

// send signal to the proxy to toggle the menu
void Menu_Proxy_Toggle(void);
