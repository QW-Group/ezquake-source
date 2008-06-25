
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
#include "ez_button.h"
#include "demo_controls.h"

float CL_GetDemoLength(void);

static ez_tree_t	democontrol_tree;

// Controls.
static ez_control_t *root;				// Root of the control tree (transparent, as big as the conwidth/height, 
										// just here to be able to get all mouse events on the screen).
static ez_window_t	*window;			// The demo controls window.
static ez_slider_t	*demo_slider;		// The demo time slider.
static ez_label_t	*timelabel;			// The text label that shows the current time.

static ez_control_t	*button_container;	// A container to easily move all the buttons around together.
static ez_button_t	*fast_button;		// Faster button.
static ez_button_t	*slow_button;		// Slower button.
static ez_button_t	*play_button;		// Play button.
static ez_button_t	*pause_button;		// Pause button.
static ez_button_t	*stop_button;		// Stop button.
static ez_button_t	*prev_button;		// Play next demo in the playlist.
static ez_button_t	*next_button;		// Play previous demo in the playlist.

// State.
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
			
			int pos = Q_rint(demo_slider->max_value * (double)(cls.demotime - demostarttime) / max(1, demo_length));

			DemoControls_SetTimeLabelText(timelabel, current_demotime);

			slider_updating = true;
			EZ_slider_SetPosition(demo_slider, pos);
			slider_updating = false;
		}

		EZ_tree_EventLoop(&democontrol_tree);
	}
}

static int DemoControls_SliderChanged(ez_control_t *self, void *payload)
{
	ez_slider_t *demo_slider = (ez_slider_t *)self;

	if (!slider_updating)
	{
		float demo_length = CL_GetDemoLength();
		double newdemotime = ((demo_slider->slider_pos * demo_length) / (double)demo_slider->max_value);

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

	// Demo time slider.
	{
		demo_slider = EZ_slider_Create(&democontrol_tree, root, 
			"Demo slider", "", 5, 5, 8, 8, control_focusable | control_contained);

		EZ_control_SetAnchor((ez_control_t *)demo_slider, anchor_left | anchor_right);
		EZ_slider_SetMax(demo_slider, 1000);

		// Add a event handler for when the slider handle changes position.
		EZ_slider_AddOnSliderPositionChanged(demo_slider, DemoControls_SliderChanged, NULL);

		// When we click a point on the slider, make the slider handle jump there.
		EZ_slider_SetJumpToClick(demo_slider, true);
	}

	// Demo time label.
	{
		timelabel = EZ_label_Create(&democontrol_tree, root, 
			"Time label", "", -5, 5, 32, 16,  
			control_focusable | control_contained | control_resizeable, 
			0, "");

		EZ_label_SetReadOnly(timelabel, true);
		EZ_label_SetTextSelectable(timelabel, false);
		EZ_control_SetAnchor((ez_control_t *)timelabel, anchor_right | anchor_top);
	}

	// Buttons.
	{
		// Button container.
		{
			button_container = EZ_control_Create(&democontrol_tree, root, 
				"Demo button container", NULL, 5, 25, 40, 16, control_contained | control_focusable);

			EZ_control_SetAnchor(button_container, anchor_left | anchor_right);
			EZ_control_SetBackgroundColor(button_container, 0, 0, 0, 150);
		}

		// Faster button.
		{
			fast_button = EZ_button_Create(&democontrol_tree, button_container,
				"Faster button", "Speeds up demo playback", 0, 0, 24, 16, control_contained);

			EZ_button_SetTextAlignment(fast_button, middle_center);
			EZ_button_SetText(fast_button, " >>");
			EZ_control_SetAnchor((ez_control_t *)fast_button, anchor_top | anchor_right);
		}

		/*
		static ez_button_t	*fast_button;	// Faster button.
		static ez_button_t	*slow_button;	// Slower button.
		static ez_button_t	*play_button;	// Play button.
		static ez_button_t	*pause_button;	// Pause button.
		static ez_button_t	*stop_button;	// Stop button.
		static ez_button_t	*prev_button;	// Play next demo in the playlist.
		static ez_button_t	*next_button;	// Play previous demo in the playlist.
		*/
	}

	// Window.
	{
		window = EZ_window_Create(&democontrol_tree, root, 
			"Demo controls window", NULL, 50, vid.conheight - 100, 250, 100, control_resize_h | control_resize_v);
		EZ_control_SetBackgroundColor((ez_control_t *)window, 0, 0, 0, 150);

		// Add our precious children :D
		EZ_window_AddChild(window, (ez_control_t *)demo_slider);
		EZ_window_AddChild(window, (ez_control_t *)timelabel);
		EZ_window_AddChild(window, (ez_control_t *)button_container);

		// Size the children to fit within the window properly.
		EZ_control_SetSize((ez_control_t *)demo_slider, (window->window_area->virtual_width - 48), 8);
		EZ_control_SetSize((ez_control_t *)button_container, (window->window_area->virtual_width - 20), button_container->height);
	}

	// Make sure all the controls are properly position and sized.
	EZ_tree_Refresh(&democontrol_tree);

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

