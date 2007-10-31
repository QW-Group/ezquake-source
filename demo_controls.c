
/*
Copyright (C) 2007 ezQuake team

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

$Id: demo_controls.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "ez_controls.h"

static ez_tree_t democontrol_tree;
static ez_slider_t *slider;
static qbool democontrols_on = false;

void DemoControls_Draw(void)
{
	if (democontrols_on && democontrol_tree.root)
	{
		extern float CL_GetDemoLength(void);
		float demo_length = CL_GetDemoLength();
		
		int pos = Q_rint(1000 * (float)(cls.demotime - demostarttime) / max(1, demo_length));

		EZ_slider_SetPosition(slider, pos);
		EZ_tree_EventLoop(&democontrol_tree);
	}
}

static void DemoControls_Init(void)
{
	ez_control_t *root = NULL;
	
	// Root.
	{
		root = EZ_control_Create(&democontrol_tree, NULL, "Root", "", 50, vid.conheight - 100, 250, 50, control_focusable | control_movable);
		EZ_control_SetBackgroundColor(root, 0, 0, 0, 50);
	}

	// Slider.
	{
		slider = EZ_slider_Create(&democontrol_tree, root, "Demo slider", "", 5, 5, 8, 8, 0);

		EZ_control_SetAnchor((ez_control_t *)slider, anchor_left | anchor_right);
		EZ_control_SetSize((ez_control_t *)slider, (root->width - 10), 8);
		EZ_slider_SetMax(slider, 1000);
	}

	key_dest = key_demo_controls;
}

static void DemoControls_Destroy(void)
{
	democontrols_on = false;
	key_dest = key_game;
	key_dest_beforecon = key_game;
	EZ_tree_Destroy(&democontrol_tree);
}

qbool DemoControls_MouseEvent(mouse_state_t *ms)
{
	return EZ_tree_MouseEvent(&democontrol_tree, ms);
}

static void DemoControls_Toggle(void)
{
	democontrols_on = !democontrols_on;

	if (democontrols_on)
	{
		DemoControls_Init();
	}
	else
	{
		key_dest = key_game;
		key_dest_beforecon = key_game;
		DemoControls_Destroy();
	}
}

qbool DemoControls_KeyEvent(int key, int unichar, qbool down)
{
	switch (key)
	{
		case K_ESCAPE :
			DemoControls_Destroy();
			return true;
	}

	return EZ_tree_KeyEvent(&democontrol_tree, key, unichar, down);
}

void DemoControls_f(void)
{
	DemoControls_Toggle();
}

