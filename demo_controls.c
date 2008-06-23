
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
#include "ez_slider.h"
#include "ez_window.h"
#include "ez_label.h"
#include "demo_controls.h"

float CL_GetDemoLength(void);

static ez_tree_t democontrol_tree;
static ez_slider_t *slider;
static ez_control_t *root = NULL;
static ez_window_t *window = NULL;
static ez_label_t *timelabel = NULL;

static qbool slider_updating = false;
static qbool democontrols_on = false;

static void DemoControls_SetTimeLabelText(ez_label_t *label, double time)
{
	// TODO: Format this properly so that 5:3 => 05:03
	EZ_label_SetText(label, va("%d:%d", Q_rint(time / 60), Q_rint((int)time % 60)));
}

void DemoControls_Draw(void)
{
	if (democontrols_on && democontrol_tree.root)
	{
		// Position the slider based on the current demo time.
		{
			float demo_length = CL_GetDemoLength();
			float current_demotime = cls.demotime - demostarttime; // TODO: Make a function for this in cl_demo.c
			
			int pos = Q_rint(slider->max_value * (double)(cls.demotime - demostarttime) / max(1, demo_length));

			DemoControls_SetTimeLabelText(timelabel, current_demotime);

			slider_updating = true;
			EZ_slider_SetPosition(slider, pos);
			slider_updating = false;
		}

		EZ_tree_EventLoop(&democontrol_tree);
	}
}

static int DemoControls_SliderChanged(ez_control_t *self, void *payload)
{
	ez_slider_t *slider = (ez_slider_t *)self;

	if (!slider_updating)
	{
		float demo_length = CL_GetDemoLength();
		double newdemotime = ((slider->slider_pos * demo_length) / (double)slider->max_value);

		DemoControls_SetTimeLabelText(timelabel, newdemotime);

		CL_Demo_Jump(newdemotime, false);
	}

	return 0;
}

static void DemoControls_Init(void)
{
	// Root.
	{
		root = EZ_control_Create(&democontrol_tree, NULL, "Root", "", 0, 0, vid.conwidth, vid.conheight, 0);
	}

	// Slider.
	{
		slider = EZ_slider_Create(&democontrol_tree, root, "Demo slider", "", 5, 5, 8, 8, control_focusable | control_contained);

		EZ_control_SetAnchor((ez_control_t *)slider, anchor_left | anchor_right);
		EZ_slider_SetMax(slider, 1000);

		// Add a event handler for when the slider handle changes position.
		EZ_slider_AddOnSliderPositionChanged(slider, DemoControls_SliderChanged, NULL);

		// When we click a point on the slider, make the slider handle jump there.
		EZ_slider_SetJumpToClick(slider, true);
	}

	// Time label.
	{
		timelabel = EZ_label_Create(&democontrol_tree, root, 
			"Time label", "", 5, 20, 32, 16,  
			control_focusable | control_contained | control_resizeable, 
			0, "");

		EZ_label_SetReadOnly(timelabel, true);
	}

	// Window.
	{
		window = EZ_window_Create(&democontrol_tree, root, "Demo controls window", NULL, 50, vid.conheight - 100, 250, 100, control_resize_h | control_resize_v);
		EZ_control_SetBackgroundColor((ez_control_t *)window, 0, 0, 0, 150);

		// Add the slider as a child and size it to fit the window.
		EZ_window_AddChild(window, (ez_control_t *)slider);
		EZ_control_SetSize((ez_control_t *)slider, (window->window_area->width - 20), 8);

		EZ_window_AddChild(window, (ez_control_t *)timelabel);
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

