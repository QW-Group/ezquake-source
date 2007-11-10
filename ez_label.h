
#ifndef __EZ_LABEL_H__
#define __EZ_LABEL_H__
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

$Id: ez_label.h,v 1.55 2007-10-27 14:51:15 cokeman1982 Exp $
*/

#include "ez_controls.h"

// =========================================================================================
// Label
// =========================================================================================

typedef enum ez_label_flags_e
{
	label_largefont		= (1 << 0),		// Use the large font.
	label_autosize		= (1 << 1),		// Autosize the label to fit the text (if it's not wrapped).
	label_autoellipsis	= (1 << 2),		// Show ... at the end of the text if it doesn't fit within the label.
	label_wraptext		= (1 << 3),		// Wrap the text to fit inside the label.
	label_selectable	= (1 << 4),		// Is text selectable?
	label_readonly		= (1 << 5)		// Is input allowed?
} ez_label_flags_t;

#define LABEL_DEFAULT_FLAGS	(label_wraptext | label_selectable)

typedef enum ez_label_iflags_e
{
	label_selecting		= (1 << 0)		// Are we selecting text?
} ez_label_iflags_t;

#define LABEL_MAX_WRAPS		512
#define LABEL_LINE_SIZE		1024

// Is any text selected in the label?
#define LABEL_TEXT_SELECTED(label) ((label->select_start > -1) && (label->select_end > - 1) && abs(label->select_end - label->select_start) > 0)

typedef struct ez_label_eventcount_s
{
	int	OnTextChanged;
	int	OnCaretMoved;
	int	OnTextScaleChanged;
	int	OnTextFlagsChanged;
} ez_label_eventcount_t;

typedef struct ez_label_events_s
{
	ez_event_fp	OnTextChanged;			// Event raised when the text in the label is changed.
	ez_event_fp	OnCaretMoved;			// The caret was moved.
	ez_event_fp	OnTextScaleChanged;		// The scale of the text changed.
	ez_event_fp	OnTextFlagsChanged;		// The text flags changed.
} ez_label_events_t;

typedef struct ez_label_eventhandlers_s
{
	ez_eventhandler_t		*OnTextChanged;
	ez_eventhandler_t		*OnCaretMoved;
	ez_eventhandler_t		*OnTextScaleChanged;
	ez_eventhandler_t		*OnTextFlagsChanged;
} ez_label_eventhandlers_t;

typedef struct ez_label_textpos_s
{
	int index;
	int row;
	int col;
} ez_label_textpos_t;

typedef struct ez_label_s
{
	ez_control_t			super;						// Super class.

	ez_label_events_t		events;						// Specific events for the label control.
	ez_label_eventhandlers_t event_handlers;			// Specific event handlers for the label control.
	ez_label_eventcount_t	inherit_levels;
	ez_label_eventcount_t	override_counts;

	char					*text;						// The text to be shown in the label.
	int						text_length;				// The length of the label text (excluding \0).
	
	ez_label_flags_t		ext_flags;					// External flags for how the text in the label is formatted.
	ez_label_iflags_t		int_flags;					// Internal flags for the current state of the label.

	ez_label_textpos_t		wordwraps[LABEL_MAX_WRAPS];	// Positions in the text where line breaks are located.
	clrinfo_t				color;						// The text color of the label. // TODO : Make this a pointer?
	float					scale;						// The scale of the text in the label.
	int						scaled_char_size;			// The actual size of the characters in the text.
	int						select_start;				// At what index the currently selected text starts at 
														// (this can be greater than select_end).
	int						select_end;					// The end index of the selected text.
	ez_label_textpos_t		caret_pos;					// The position of the caret (index / row / column).
	int						num_rows;					// The current number of rows in the label.
	int						num_cols;					// The current number of cols (for the longest row).
} ez_label_t;

ez_label_t *EZ_label_Create(ez_tree_t *tree, ez_control_t *parent, 
							  char *name, char *description, 
							  int x, int y, int width, int height,
							  ez_control_flags_t flags, ez_label_flags_t text_flags,
							  char *text);

void EZ_label_Init(ez_label_t *label, ez_tree_t *tree, ez_control_t *parent, 
				  char *name, char *description, 
				  int x, int y, int width, int height,
				  ez_control_flags_t flags, ez_label_flags_t text_flags,
				  char *text);

//
// Label - Destroys a label control.
//
int EZ_label_Destroy(ez_control_t *self, qbool destroy_children);

//
// Label - Sets if the label should use the large charset or the normal one.
//
void EZ_label_SetLargeFont(ez_label_t *label, qbool large_font);

//
// Label - Sets if the label should autosize itself to fit the text in the label.
//
void EZ_label_SetAutoSize(ez_label_t *label, qbool auto_size);

//
// Label - Sets if the label should automatically add "..." at the end of the string when it doesn't fit in the label.
//
void EZ_label_SetAutoEllipsis(ez_label_t *label, qbool auto_ellipsis);

//
// Label - Sets if the text in the label should wrap to fit the width of the label.
//
void EZ_label_SetWrapText(ez_label_t *label, qbool wrap_text);

//
// Label - Sets the text flags for the label.
//
void EZ_label_SetTextFlags(ez_label_t *label, ez_label_flags_t flags);

//
// Label - Sets if the label should be read only.
//
void EZ_label_SetReadOnly(ez_label_t *label, qbool read_only);

//
// Label - Sets if the text in the label should be selectable.
//
void EZ_label_SetTextSelectable(ez_label_t *label, qbool selectable);

//
// Label - Sets the event handler for the OnTextChanged event.
//
void EZ_label_AddOnTextChanged(ez_label_t *label, ez_eventhandler_fp OnTextChanged, void *payload);

//
// Label - Sets the event handler for the OnTextScaleChanged event.
//
void EZ_label_AddOnTextScaleChanged(ez_label_t *label, ez_eventhandler_fp OnTextScaleChanged, void *payload);

//
// Label - Sets the event handler for the OnCaretMoved event.
//
void EZ_label_AddOnTextOnCaretMoved(ez_label_t *label, ez_eventhandler_fp OnCaretMoved, void *payload);

//
// Label - Hides the caret.
//
void EZ_label_HideCaret(ez_label_t *label);

//
// Label - Gets the size of the selected text.
//
int EZ_label_GetSelectedTextSize(ez_label_t *label);

//
// Label - Gets the selected text.
//
void EZ_label_GetSelectedText(ez_label_t *label, char *target, int target_size);

//
// Label - Deselects the text in the label.
//
void EZ_label_DeselectText(ez_label_t *label);

//
// Label - Set the text scale for the label.
//
void EZ_label_SetTextScale(ez_label_t *label, float scale);

//
// Label - Sets the color of the text in the label.
//
void EZ_label_SetTextColor(ez_label_t *self, byte r, byte g, byte b, byte alpha);

//
// Label - Sets the text of the label (will be reallocated and copied to the heap).
//
void EZ_label_SetText(ez_label_t *self, const char *text);

//
// Label - Gets the text flags for the label.
//
ez_label_flags_t EZ_label_GetTextFlags(ez_label_t *label);

//
// Label - Sets the text flags for the label.
//
void EZ_label_SetTextFlags(ez_label_t *label, ez_label_flags_t flags);

//
// Label - Appends text at the given position in the text.
//
void EZ_label_AppendText(ez_label_t *label, int position, const char *append_text);

//
// Label - Set the caret position.
//
void EZ_label_SetCaretPosition(ez_label_t *label, int caret_pos);

//
// Label - The text flags for the label changed.
//
int EZ_label_OnTextFlagsChanged(ez_control_t *self);

//
// Label - The scale of the text changed.
//
int EZ_label_OnTextScaleChanged(ez_control_t *self);

//
// Label - Happens when the control has resized.
//
int EZ_label_OnResize(ez_control_t *self);

//
// Label - On Draw.
//
int EZ_label_OnDraw(ez_control_t *label);

//
// Label - The caret was moved.
//
int EZ_label_OnCaretMoved(ez_control_t *self);

//
// Label - The text changed in the label.
//
int EZ_label_OnTextChanged(ez_control_t *self);

//
// Label - Key down event.
//
int EZ_label_OnKeyDown(ez_control_t *self, int key, int unichar);

//
// Label - Key up event.
//
int EZ_label_OnKeyUp(ez_control_t *self, int key, int unichar);

//
// Label - Key event.
//
int EZ_label_OnKeyEvent(ez_control_t *self, int key, int unichar, qbool down);

//
// Label - Handles the mouse down event.
//
int EZ_label_OnMouseDown(ez_control_t *self, mouse_state_t *ms);

//
// Label - Handles the mouse up event.
//
int EZ_label_OnMouseUp(ez_control_t *self, mouse_state_t *ms);

//
// Label - The mouse is hovering within the bounds of the label.
//
int EZ_label_OnMouseHover(ez_control_t *self, mouse_state_t *mouse_state);

#endif // __EZ_LABEL_H__

