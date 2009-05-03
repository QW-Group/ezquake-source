
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
double Demo_GetSpeed(void);
extern cvar_t cl_demospeed;

// The control tree.
static ez_tree_t	democontrol_tree;

// Controls.
static ez_control_t *root;				// Root of the control tree (transparent, as big as the conwidth/height, 
										// just here to be able to get all mouse events on the screen).
static ez_window_t	*window;			// The demo controls window.
static ez_slider_t	*demo_slider;		// The demo time slider.
static ez_label_t	*timelabel;			// The text label that shows the current time.
static ez_label_t	*hover_timelabel;	// The text label that shows the time where the cursor is positioned on the slider.
static ez_label_t	*speed_title_label;	// The title text label for the speed.
static ez_label_t	*speed_label;		// The text label that shows the current demo speed in percent.

static ez_control_t	*button_container;	// A container to easily move all the buttons around together.
static ez_button_t	*fast_button;		// Faster button.
static ez_button_t	*slow_button;		// Slower button.
static ez_button_t	*play_button;		// Play button.
static ez_button_t	*pause_button;		// Pause button.
static ez_button_t	*stop_button;		// Stop button.
//static ez_button_t	*prev_button;		// Play next demo in the playlist.
//static ez_button_t	*next_button;		// Play previous demo in the playlist.

// State.
static qbool slider_updating = false;
static qbool democontrols_on = false;
static double previous_demospeed = 1.0;

static void DemoControls_SetTimeLabelText(ez_label_t *label, double time)
{
	EZ_label_SetText(label, va("%02d:%02d", Q_rint(time / 60), Q_rint((int)time % 60)));
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

static int DemoControls_SliderChanged(ez_control_t *self, void *payload, void *ext_event_info)
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

static int DemoControls_Slider_OnMouseEnter(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	EZ_control_SetVisible((ez_control_t *)hover_timelabel, true);
	return 0;
}

static int DemoControls_Slider_OnMouseLeave(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	EZ_control_SetVisible((ez_control_t *)hover_timelabel, false);
	return 0;
}

static int DemoControls_Slider_OnMouseHover(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	ez_slider_t *demo_slider = (ez_slider_t *)self;
	float demo_length = CL_GetDemoLength();
	double demotime = ((EZ_slider_GetPositionFromMouse(demo_slider, ms->x, ms->y) * demo_length) / (double)demo_slider->max_value);

	EZ_control_SetPosition((ez_control_t *)hover_timelabel, (ms->x - self->absolute_x), -8);
	EZ_label_SetText(hover_timelabel, va("%02d:%02d", Q_rint(demotime / 60), max(0, Q_rint((int)demotime % 60))));

	return 0;
}

static void DemoControls_SetDemoSpeed(double speed)
{
	speed = bound(0.0, speed, 10.0);

	if (speed != previous_demospeed)
	{
		previous_demospeed = Demo_GetSpeed();
	}

	Cvar_SetValue(&cl_demospeed, speed);	
	EZ_label_SetText(speed_label, va("%i%%", Q_rint(Demo_GetSpeed() * 100)));
}

static int DemoControls_SpeedButton_OnMouseClick(ez_control_t *self, void *payload, mouse_state_t *ms)
{	
	float delta = (self == ((ez_control_t *)fast_button)) ? 0.1 : -0.1;	
	
	DemoControls_SetDemoSpeed(cl_demospeed.value + delta);
	
	return 1;
}

static int DemoControls_PlayButton_OnMouseClick(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	if ((previous_demospeed != 0.0) && (Demo_GetSpeed() == 0.0))
	{
		DemoControls_SetDemoSpeed(previous_demospeed);
	}
	else
	{
		// We're not paused so just reset the speed when clicking the play button.
		DemoControls_SetDemoSpeed(1.0);
	}
	
	return 1;
}

static int DemoControls_PauseButton_OnMouseClick(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	DemoControls_SetDemoSpeed(0.0);	
	return 1;
}

static int DemoControls_StopButton_OnMouseClick(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	Cbuf_AddText("disconnect");
	return 1;
}

// TODO: Fade these gradually instead.
static int DemoControls_Window_OnMouseEnter(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	EZ_control_SetOpacity(self, 1.0);
	return 0;
}

static int DemoControls_Window_OnMouseLeave(ez_control_t *self, void *payload, mouse_state_t *ms)
{
	EZ_control_SetOpacity(self, 0.5);
	return 0;
}

static void DemoControls_Init(void)
{
	#define DEMO_BUTTON_GAP		5
	#define DEMO_BUTTON_HEIGHT	16
	#define DEMO_BUTTON_WIDTH	24

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
		EZ_control_AddOnMouseHover((ez_control_t *)demo_slider, DemoControls_Slider_OnMouseHover, NULL);
		EZ_control_AddOnMouseEnter((ez_control_t *)demo_slider, DemoControls_Slider_OnMouseEnter, NULL);
		EZ_control_AddOnMouseLeave((ez_control_t *)demo_slider, DemoControls_Slider_OnMouseLeave, NULL);

		// When we click a point on the slider, make the slider handle jump there.
		EZ_slider_SetJumpToClick(demo_slider, true);
	}

	// Demo time label.
	{
		timelabel = EZ_label_Create(&democontrol_tree, root, 
			"Demo time label", "", -15, 5, 40, 16,  
			control_focusable | control_contained | control_resizeable, 
			0, "");

		EZ_label_SetReadOnly(timelabel, true);
		EZ_label_SetTextSelectable(timelabel, false);
		EZ_control_SetAnchor((ez_control_t *)timelabel, anchor_right | anchor_top);
	}

	// Demo hover time label (when the slider is hovered).
	{
		ez_control_t *tmp;
		hover_timelabel = EZ_label_Create(&democontrol_tree, (ez_control_t *)demo_slider, 
			"Hover demo time label", "", 0, 0, 40, 8, 0, 0, "");

		tmp = (ez_control_t *)hover_timelabel;

		EZ_label_SetReadOnly(hover_timelabel, true);
		EZ_label_SetTextSelectable(hover_timelabel, false);
		EZ_label_SetTextColor(hover_timelabel, 125, 125, 0, 255);
		EZ_control_SetAnchor(tmp, anchor_left | anchor_top);
		EZ_control_SetVisible(tmp, false);
		EZ_control_SetContained(tmp, false);
		EZ_control_SetBackgroundColor(tmp, 0, 0, 0, 125);
		EZ_control_SetFocusable(tmp, false);
	}

	// Demo speed title label.
	{
		speed_title_label = EZ_label_Create(&democontrol_tree, root, 
			"Demo speed title label", "", -(15 + 24), 15, 64, 16,  
			control_contained | control_resizeable, 
			0, "");

		EZ_label_SetAutoSize(speed_title_label, true);
		EZ_label_SetText(speed_title_label, "Speed:");
		EZ_label_SetTextColor(speed_title_label, 255, 150, 0, 255);
		EZ_label_SetReadOnly(speed_title_label, true);
		EZ_label_SetTextSelectable(speed_title_label, false);
		EZ_control_SetAnchor((ez_control_t *)speed_title_label, anchor_right | anchor_top);
	}

	// Demo speed label.
	{
		speed_label = EZ_label_Create(&democontrol_tree, root, 
			"Demo speed label", "", -5, 15, 24, 16,  
			control_contained | control_resizeable, 
			0, "");

		EZ_label_SetAutoSize(speed_label, true);
		EZ_label_SetText(speed_label, va("%i%%", Q_rint(Demo_GetSpeed() * 100)));
		EZ_label_SetReadOnly(speed_label, true);
		EZ_label_SetTextSelectable(speed_label, false);
		EZ_control_SetAnchor((ez_control_t *)speed_label, anchor_right | anchor_top);
	}

	// Buttons.
	{
		// Button container.
		{
			button_container = EZ_control_Create(&democontrol_tree, root, 
				"Demo button container", NULL, 10, 25, 200, 16, control_contained | control_focusable);

			EZ_control_SetAnchor(button_container, anchor_left | anchor_right);
		}		

		// Slower button.
		{
			slow_button = EZ_button_Create(&democontrol_tree, button_container,
				"Slower button", "Slows down demo playback", 0, 0, DEMO_BUTTON_WIDTH, DEMO_BUTTON_HEIGHT, control_contained);

			EZ_button_SetTextAlignment(slow_button, middle_center);
			EZ_button_SetText(slow_button, " <<");
			EZ_control_SetAnchor((ez_control_t *)slow_button, anchor_top);
			EZ_control_AddOnMouseClick((ez_control_t *)slow_button, DemoControls_SpeedButton_OnMouseClick, NULL);
		}

		// Play button.
		{
			play_button = EZ_button_Create(&democontrol_tree, button_container,
				"Play button", "Starts demo playback or resets it to normal speed", 
				(DEMO_BUTTON_WIDTH + DEMO_BUTTON_GAP), 0, 
				DEMO_BUTTON_WIDTH, DEMO_BUTTON_HEIGHT, control_contained);

			EZ_button_SetTextAlignment(play_button, middle_center);
			EZ_button_SetText(play_button, " >");
			EZ_control_SetAnchor((ez_control_t *)play_button, anchor_top);
			EZ_control_AddOnMouseClick((ez_control_t *)play_button, DemoControls_PlayButton_OnMouseClick, NULL);
		}

		// Pause button.
		{
			pause_button = EZ_button_Create(&democontrol_tree, button_container,
				"Pause button", "Pauses demo playback", 
				2 * (DEMO_BUTTON_WIDTH + DEMO_BUTTON_GAP), 0, 
				DEMO_BUTTON_WIDTH, DEMO_BUTTON_HEIGHT, control_contained);

			EZ_button_SetTextAlignment(pause_button, middle_center);
			EZ_button_SetText(pause_button, " ||");
			EZ_control_SetAnchor((ez_control_t *)pause_button, anchor_top);
			EZ_control_AddOnMouseClick((ez_control_t *)pause_button, DemoControls_PauseButton_OnMouseClick, NULL);
		}

		// Stop button.
		{
			stop_button = EZ_button_Create(&democontrol_tree, button_container,
				"Stop button", "Stop demo playback", 
				3 * (DEMO_BUTTON_WIDTH + DEMO_BUTTON_GAP), 0, 
				DEMO_BUTTON_WIDTH, DEMO_BUTTON_HEIGHT, control_contained);

			EZ_button_SetTextAlignment(stop_button, middle_center);
			EZ_button_SetText(stop_button, " []");
			EZ_control_SetAnchor((ez_control_t *)stop_button, anchor_top);
			EZ_control_AddOnMouseClick((ez_control_t *)stop_button, DemoControls_StopButton_OnMouseClick, NULL);
		}

		// Faster button.
		{
			fast_button = EZ_button_Create(&democontrol_tree, button_container,
				"Faster button", "Speeds up demo playback", 
				4 * (DEMO_BUTTON_WIDTH + DEMO_BUTTON_GAP), 0, 24, 16, control_contained);

			EZ_button_SetTextAlignment(fast_button, middle_center);
			EZ_button_SetText(fast_button, " >>");
			EZ_control_SetAnchor((ez_control_t *)fast_button, anchor_top);
			EZ_control_AddOnMouseClick((ez_control_t *)fast_button, DemoControls_SpeedButton_OnMouseClick, NULL);
		}
	}

	// Window.
	{
		window = EZ_window_Create(&democontrol_tree, root, 
			"Demo controls window", NULL, 50, (vid.conheight - 100), 250, 100, control_resize_h | control_resize_v);
		EZ_control_SetBackgroundColor((ez_control_t *)window, 0, 0, 0, 150);

		EZ_control_AddOnMouseEnter((ez_control_t *)window, DemoControls_Window_OnMouseEnter, NULL);
		EZ_control_AddOnMouseLeave((ez_control_t *)window, DemoControls_Window_OnMouseLeave, NULL);

		// Add our precious children :D		
		EZ_window_AddChild(window, (ez_control_t *)demo_slider);
		EZ_window_AddChild(window, (ez_control_t *)timelabel);
		EZ_window_AddChild(window, (ez_control_t *)speed_title_label);
		EZ_window_AddChild(window, (ez_control_t *)speed_label);
		EZ_window_AddChild(window, (ez_control_t *)button_container);

		// Size the children to fit within the window properly.
		EZ_control_SetSize((ez_control_t *)demo_slider, (window->window_area->virtual_width - 64), 8);
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

