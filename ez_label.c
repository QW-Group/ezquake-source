
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

$Id: ez_scrollpane.c,v 1.78 2007/10/27 14:51:15 cokeman1982 Exp $
*/

#include "quakedef.h"
#include "keys.h"
#include "utils.h"
#include "common_draw.h"
#include "ez_controls.h"
#include "ez_label.h"

#ifdef _WIN32
#pragma warning( disable : 4189 )
#endif

// =========================================================================================
// Label
// =========================================================================================

//
// Label - Creates a label control and initializes it.
//
ez_label_t *EZ_label_Create(ez_tree_t *tree, ez_control_t *parent,
							  char *name, char *description,
							  int x, int y, int width, int height,
							  ez_control_flags_t flags, ez_label_flags_t text_flags,
							  const char *text)
{
	ez_label_t *label = NULL;

	// We have to have a tree to add the control to.
	if (!tree)
	{
		return NULL;
	}

	label = (ez_label_t *)Q_malloc(sizeof(ez_label_t));
	memset(label, 0, sizeof(ez_label_t));

	EZ_label_Init(label, tree, parent, name, description, x, y, width, height, flags, text_flags, text);
	
	return label;
}

//
// Label - Initializes a label control.
//
void EZ_label_Init(ez_label_t *label, ez_tree_t *tree, ez_control_t *parent,
				  char *name, char *description,
				  int x, int y, int width, int height,
				  ez_control_flags_t flags, ez_label_flags_t text_flags,
				  const char *text)
{
	// Initialize the inherited class first.
	EZ_control_Init(&label->super, tree, parent, name, description, x, y, width, height, flags);

	label->super.CLASS_ID = EZ_LABEL_ID;

	// Overriden events.
	CONTROL_REGISTER_EVENT(label, EZ_label_OnDraw, OnDraw, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnKeyDown, OnKeyDown, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnKeyUp, OnKeyUp, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnMouseDown, OnMouseDown, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnMouseUp, OnMouseUp, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnMouseHover, OnMouseHover, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnResize, OnResize, ez_control_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_Destroy, OnDestroy, ez_control_t);

	// Label specific events.
	CONTROL_REGISTER_EVENT(label, EZ_label_OnTextChanged, OnTextChanged, ez_label_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnCaretMoved, OnCaretMoved, ez_label_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnTextScaleChanged, OnTextScaleChanged, ez_label_t);
	CONTROL_REGISTER_EVENT(label, EZ_label_OnTextFlagsChanged, OnTextFlagsChanged, ez_label_t);

	((ez_control_t *)label)->ext_flags				|= control_contained;

	EZ_label_SetTextFlags(label, text_flags | label_selectable);
	EZ_label_SetTextScale(label, 1.0);
	EZ_label_DeselectText(label);

	if (text)
	{
		EZ_label_SetText(label, text);
	}
}

//
// Label - Destroys a label control.
//
int EZ_label_Destroy(ez_control_t *self, qbool destroy_children)
{
	ez_label_t *label = (ez_label_t *)self;

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDestroy, destroy_children);

	Q_free(label->text);

	EZ_eventhandler_Remove(label->event_handlers.OnCaretMoved, NULL, true);
	EZ_eventhandler_Remove(label->event_handlers.OnTextChanged, NULL, true);
	EZ_eventhandler_Remove(label->event_handlers.OnTextFlagsChanged, NULL, true);
	EZ_eventhandler_Remove(label->event_handlers.OnTextScaleChanged, NULL, true);

	// TODO: Can we just free a part like this here? How about children, will they be properly destroyed?
	EZ_control_Destroy(&label->super, destroy_children);

	return 0;
}

//
// Label - Sets the event handler for the OnTextChanged event.
//
void EZ_label_AddOnTextChanged(ez_label_t *label, ez_eventhandler_fp OnTextChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(label, EZ_CONTROL_HANDLER, OnTextChanged, ez_label_t, OnTextChanged, payload);
	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Label - Sets the event handler for the OnTextScaleChanged event.
//
void EZ_label_AddOnTextScaleChanged(ez_label_t *label, ez_eventhandler_fp OnTextScaleChanged, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(label, EZ_CONTROL_HANDLER, OnTextScaleChanged, ez_label_t, OnTextScaleChanged, payload);
	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Label - Sets the event handler for the OnCaretMoved event.
//
void EZ_label_AddOnTextOnCaretMoved(ez_label_t *label, ez_eventhandler_fp OnCaretMoved, void *payload)
{
	CONTROL_ADD_EVENTHANDLER(label, EZ_CONTROL_HANDLER, OnCaretMoved, ez_label_t, OnCaretMoved, payload);
	CONTROL_RAISE_EVENT(NULL, label, ez_control_t, OnEventHandlerChanged, NULL);
}

//
// Label - Calculates where in the label text that the wordwraps will be done.
//
static void EZ_label_CalculateWordwraps(ez_label_t *label)
{
	int i					= 0;
	int current_index		= -1;
	int last_index			= -1;
//	int current_col			= 0;
	int scaled_char_size	= label->scaled_char_size;

	label->num_rows			= 1;
	label->num_cols			= 0;

	if (label->text && (label->ext_flags & label_wraptext))
	{	
		// Wordwrap the string to the virtual size of the control and save the
		// indexes where each row ends in an array.
		while ((i < LABEL_MAX_WRAPS) && Util_GetNextWordwrapString(label->text, NULL, (current_index + 1), &current_index, LABEL_LINE_SIZE, label->super.virtual_width, scaled_char_size))
		{
			label->wordwraps[i].index = current_index;
			label->wordwraps[i].col = current_index - last_index;
			label->wordwraps[i].row = label->num_rows - 1;
			i++;

			// Find the number of rows and columns.
			label->num_cols = max(label->num_cols, label->wordwraps[i].col);
			label->num_rows++;

			last_index = current_index;
		}
	}
	else if (label->text)
	{
		// Normal non-wrapped text, still save new line locations.
		current_index = 0; // TODO : Will this be ok? Otherwise it will be -1, which definantly is bad :p

		while ((i < LABEL_MAX_WRAPS) && label->text[current_index])
		{
			if (label->text[current_index] == '\n')
			{
				label->wordwraps[i].index = current_index;
				label->wordwraps[i].col = current_index - last_index;
				label->wordwraps[i].row = label->num_rows - 1;
				i++;

				// Find the number of rows and columns.
				label->num_cols = max(label->num_cols, label->wordwraps[i].col);
				label->num_rows++;
			}
			
			current_index++;
		}
	}

	// Save the row/col information for the last row also.
	if (label->text)
	{
		label->wordwraps[i].index	= -1;
		label->wordwraps[i].row		= label->num_rows - 1;
		label->wordwraps[i].col		= label->text_length - label->wordwraps[max(0, i - 1)].index;
		label->num_cols				= max(label->num_cols, label->wordwraps[i].col);
	}
	else
	{
		// No text.
		label->wordwraps[0].index	= -1;
		label->wordwraps[0].row		= -1;
		label->wordwraps[0].col		= -1;
		label->num_cols				= 0;
		label->num_rows				= 0;
	}

	// Change the virtual height of the control to fit the text when wrapping.
	if (label->ext_flags & label_wraptext)
	{
		EZ_control_SetMinVirtualSize((ez_control_t *)label, label->super.virtual_width_min, scaled_char_size * (label->num_rows + 1));
	}
	else
	{
		EZ_control_SetMinVirtualSize((ez_control_t *)label, scaled_char_size * (label->num_cols + 1), scaled_char_size * (label->num_rows + 1));
	}
}

//
// Label - Sets if the label should use the large charset or the normal one.
//
void EZ_label_SetLargeFont(ez_label_t *label, qbool large_font)
{
	SET_FLAG(label->ext_flags, label_largefont, large_font);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);
}

//
// Label - Sets if the label should autosize itself to fit the text in the label.
//
void EZ_label_SetAutoSize(ez_label_t *label, qbool auto_size)
{
	SET_FLAG(label->ext_flags, label_autosize, auto_size);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);
}

//
// Label - Sets if the label should automatically add "..." at the end of the string when it doesn't fit in the label.
//
void EZ_label_SetAutoEllipsis(ez_label_t *label, qbool auto_ellipsis)
{
	SET_FLAG(label->ext_flags, label_autoellipsis, auto_ellipsis);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);
}

//
// Label - Sets if the text in the label should wrap to fit the width of the label.
//
void EZ_label_SetWrapText(ez_label_t *label, qbool wrap_text)
{
	SET_FLAG(label->ext_flags, label_wraptext, wrap_text);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);
}

//
// Label - Sets if the text in the label should be selectable.
//
void EZ_label_SetTextSelectable(ez_label_t *label, qbool selectable)
{
	SET_FLAG(label->ext_flags, label_selectable, selectable);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);
}

//
// Label - Sets if the label should be read only.
//
void EZ_label_SetReadOnly(ez_label_t *label, qbool read_only)
{
	SET_FLAG(label->ext_flags, label_readonly, read_only);
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);
}

//
// Label - Sets the text flags for the label.
//
void EZ_label_SetTextFlags(ez_label_t *label, ez_label_flags_t flags)
{
	label->ext_flags = flags;
	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);
}

//
// Label - Gets the text flags for the label.
//
ez_label_flags_t EZ_label_GetTextFlags(ez_label_t *label)
{
	return label->ext_flags;
}

//
// Label - Set the text scale for the label.
//
void EZ_label_SetTextScale(ez_label_t *label, float scale)
{
	label->scale = scale;

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextScaleChanged, NULL);
}

//
// Label - Sets the text color of a label.
//
void EZ_label_SetTextColor(ez_label_t *label, byte r, byte g, byte b, byte alpha)
{
	label->color.c = RGBA_TO_COLOR(r, g, b, alpha);
}

//
// Label - Hides the caret.
//
void EZ_label_HideCaret(ez_label_t *label)
{
	label->caret_pos.index = -1;
}

//
// Label - Deselects the text in the label.
//
void EZ_label_DeselectText(ez_label_t *label)
{
	label->int_flags	&= ~label_selecting;
	label->select_start = -1;
	label->select_end	= -1;
}

//
// Label - Gets the size of the selected text.
//
int EZ_label_GetSelectedTextSize(ez_label_t *label)
{
	if ((label->select_start > -1) && (label->select_end > -1))
	{
		return abs(label->select_end - label->select_start) + 1;
	}

	return 0;
}

//
// Label - Gets the selected text.
//
void EZ_label_GetSelectedText(ez_label_t *label, char *target, int target_size)
{
	if ((label->select_start > -1) && (label->select_end > -1))
	{
		int sublen = abs(label->select_end - label->select_start) + 1;
		
		if (sublen > 0)
		{
			snprintf(target, min(sublen, target_size), "%s", label->text + min(label->select_start, label->select_end));
		}
	}
}

//
// Label - Appends text at the given position in the text.
//
void EZ_label_AppendText(ez_label_t *label, int position, const char *append_text)
{
	int text_len		= label->text_length;
	int append_text_len	= strlen(append_text);

	clamp(position, 0, text_len);

	if (!label->text)
	{
		return;
	}

	// Reallocate the string to fit the new text.
	label->text = (char *)Q_realloc((void *)label->text, (text_len + append_text_len + 1) * sizeof(char));

	// Move the part of the string after the specified position.
	memmove(
		label->text + position + append_text_len,		// Destination, make room for the append text.
		label->text + position,							// Move everything after the specified position.
		(text_len + 1 - position) * sizeof(char));

	// Copy the append text into the newly created space in the string.
	memcpy(label->text + position, append_text, append_text_len * sizeof(char));

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged, NULL);
}

//
// Label - Removes text between two given indexes.
//
void EZ_label_RemoveText(ez_label_t *label, int start_index, int end_index)
{
	clamp(start_index, 0, label->text_length);
	clamp(end_index, start_index, label->text_length);

	if (!label->text)
	{
		return;
	}

	memmove(
		label->text + start_index,								// Destination.
		label->text + end_index,								// Source.
		(label->text_length - end_index + 1) * sizeof(char));	// Size.
	
	label->text = Q_realloc(label->text, (strlen(label->text) + 1) * sizeof(char));

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged, NULL);
}

//
// Label - Sets the text of a label.
//
void EZ_label_SetText(ez_label_t *label, const char *text)
{
	int text_len = strlen(text) + 1;

	Q_free(label->text);

	if (text)
	{
		label->text = Q_calloc(text_len, sizeof(char));
		strlcpy(label->text, text, text_len);
	}

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged, NULL);
}

//
// Label - The text flags for the label changed.
//
int EZ_label_OnTextFlagsChanged(ez_control_t *self, void *ext_event_info)
{
	ez_label_t *label = (ez_label_t *)self;

	// Deselect anything selected and hide the caret.
	if (!(label->ext_flags & label_selectable))
	{
		EZ_label_HideCaret(label);
		EZ_label_DeselectText(label);
	}
	
	// Make sure we recalculate the char size if we changed to large font.
	if (label->ext_flags & label_largefont)
	{
		CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextScaleChanged, NULL);
	}

	// The label will autosize on a text change, so trigger an event for that.
	if (label->ext_flags & label_autosize)
	{
		CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnTextChanged, NULL);
	}

	// Recalculate the line ends.
	if (label->ext_flags & label_wraptext)
	{
		CONTROL_RAISE_EVENT(NULL, self, ez_control_t, OnResize, NULL);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnTextFlagsChanged, NULL);

	return 0;
}

//
// Label - The scale of the text changed.
//
int EZ_label_OnTextScaleChanged(ez_control_t *self, void *ext_event_info)
{
	ez_label_t *label		= (ez_label_t *)self;
	int char_size			= (label->ext_flags & label_largefont) ? 64 : 8;

	label->scaled_char_size	= Q_rint(char_size * label->scale);
	
	// We need to recalculate the wordwrap stuff since the size changed.
	EZ_label_CalculateWordwraps(label);

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnTextScaleChanged, NULL);

	return 0;
}

//
// Label - Happens when the control has resized.
//
int EZ_label_OnResize(ez_control_t *self, void *ext_event_info)
{
	ez_label_t *label = (ez_label_t *)self;
	
	// Let the super class try first.
	EZ_control_OnResize(self, NULL);

	// Calculate where to wrap the text.
	EZ_label_CalculateWordwraps(label);

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnResize, NULL);

	return 0;
}

//
// Label - Draws a label control.
//
int EZ_label_OnDraw(ez_control_t *self, void *ext_event_info)
{
	int x, y, i = 0;
	char line[LABEL_LINE_SIZE];
	ez_label_t *label		= (ez_label_t *)self;
	int scaled_char_size	= label->scaled_char_size;
	int last_index			= -1;
	int curr_row			= 0;
	int curr_col			= 0;
	clrinfo_t text_color	= {RGBA_TO_COLOR(255, 255, 255, 255), 0}; // TODO : Set this in the struct instead.
	color_t selection_color	= RGBA_TO_COLOR(178, 0, 255, 125);
	color_t caret_color		= RGBA_TO_COLOR(255, 0, 0, 125);

	// Let the super class draw first.
	EZ_control_OnDraw(self, NULL);

	// Get the position we're drawing at.
	EZ_control_GetDrawingPosition(self, &x, &y);

	// Find any newlines and draw line by line.
	for (i = 0; i <= label->text_length; i++)
	{
		// Draw selection markers.
		if (label->ext_flags & label_selectable)
		{
			if (((label->select_start > -1) && (label->select_end > -1)			// Is something selected at all?
				&& (label->select_end != label->select_start))					// Only highlight if we have at least 1 char selected.
				&& (((i >= label->select_start) && (i < label->select_end))		// Is this index selected?
					|| ((i >= label->select_end) && (i < label->select_start)))
				)
			{
				// If this is one of the selected letters, draw a selection thingie behind it.
				// TODO : Make this an XOR drawing ontop of the text instead?
				Draw_AlphaFillRGB(
					x + (curr_col * scaled_char_size), 
					y + (curr_row * scaled_char_size), 
					scaled_char_size, scaled_char_size, 
					selection_color);
			}

			if (i == label->caret_pos.index)
			{
				// Draw the caret.
				Draw_AlphaFillRGB(
					x + (curr_col * scaled_char_size), 
					y + (curr_row * scaled_char_size), 
					max(5, Q_rint(scaled_char_size * 0.1)), scaled_char_size, 
					caret_color);
			}
		}

		if (i == label->wordwraps[curr_row].index || label->text[i] == '\0')
		{
			// We found the end of a line, copy the contents of the line to the line buffer.
			snprintf(line, min(LABEL_LINE_SIZE, (i - last_index) + 1), "%s", (label->text + last_index + 1));
			last_index = i;	// Skip the newline character

			if (label->ext_flags & label_largefont)
			{
				Draw_BigString(x, y + (curr_row * scaled_char_size), line, &text_color, 1, label->scale, 1, 0);
			}
			else
			{
				Draw_SColoredString(x, y + (curr_row * scaled_char_size), str2wcs(line), &text_color, 1, false, label->scale);
			}

			curr_row++;
			curr_col = 0;
		}
		else
		{
			curr_col++;
		}
	}

	// TODO : Remove this test stuff.
	{
		char tmp[1024];

		int sublen = abs(label->select_end - label->select_start);
		if (sublen > 0)
		{
			snprintf(tmp, sublen + 1, "%s", label->text + min(label->select_start, label->select_end));
			Draw_String(x + (self->width / 2), y + (self->height / 2) + 8, tmp);
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnDraw, NULL);

	return 0;
}

//
// Label - Finds the row and column for a specified index. The proper wordwrap indexes must've been calculated first.
//
static void EZ_label_FindRowColumnByIndex(ez_label_t *label, int index, int *row, int *column)
{
	int i = 0;

	// Find the row the index is on.
	if (row)
	{
		// Find the wordwrap that's closest to the index but still larger
		// that will be the end of the row that the index is on.
		while ((label->wordwraps[i].index != -1) && (label->wordwraps[i].index < index))
		{
			i++;
		}

		// Save the row. (The last row as -1 as index, but still has the correct row saved).
		(*row) = label->wordwraps[i].row;
	}

	if (column)
	{
		// We don't have the last rows end index saved explicitly, so get it here.
		int row_end_index = (label->wordwraps[i].index < 0) ? strlen(label->text) : label->wordwraps[i].index;

		// The column at the end of the row minus the difference between
		// the given index and the index at the end of the row gives us
		// what column the index is in.
		(*column) = label->wordwraps[i].col - (row_end_index - index);
	}
}

//
// Label - Finds the index in the label text at the specified row and column.
//
static int EZ_label_FindIndexByRowColumn(ez_label_t *label, int row, int column)
{
	int row_end_index = 0;

	// Make sure we don't do anythin stupid.
	if ((row < 0) || (row > (label->num_rows - 1)) || (column < 0)) // || (label->wordwraps[row].index < 0))
	{
		return -1;
	}

	// We don't have the last rows end index saved explicitly, so get it here.
	row_end_index = (label->wordwraps[row].index < 0) ? strlen(label->text) : label->wordwraps[row].index;

	if (column > label->wordwraps[row].col)
	{
		// There is no such column on the specified row
		// return the index of the last character on that row instead.
		return row_end_index;
	}

	// Return the index of the specified column.
	return  row_end_index - (label->wordwraps[row].col - column);
}

//
// Label - Finds at what text index the mouse was clicked.
//
static int EZ_label_FindMouseTextIndex(ez_label_t *label, mouse_state_t *ms)
{
	// TODO : Support big font gaps?
	int x, y;
	int row, col;

	// Get the position we're drawing at.
	EZ_control_GetDrawingPosition((ez_control_t *)label, &x, &y);

	// Find the row and column where the mouse is.
	row = ((ms->y - y) / label->scaled_char_size);
	col = ((ms->x - x) / label->scaled_char_size);

	// Give the last index if the mouse is past the last row.
	if (row > (label->num_rows - 1))
	{
		return label->text_length;
	}

	return EZ_label_FindIndexByRowColumn(label, row, col) + 1;
}

//
// Label - Set the caret position.
//
void EZ_label_SetCaretPosition(ez_label_t *label, int caret_pos)
{
	if (label->ext_flags & label_selectable)
	{
		label->caret_pos.index = caret_pos;
		clamp(label->caret_pos.index, -1, label->text_length);
	}
	else
	{
		label->caret_pos.index = -1;
		EZ_label_DeselectText(label);
	}

	CONTROL_RAISE_EVENT(NULL, label, ez_label_t, OnCaretMoved, NULL);
}

//
// Label - Moves the caret up or down.
//
static void EZ_label_MoveCaretVertically(ez_label_t *label, int amount)
{
	// Find the index of the character above/below that position,
	// and set the caret to that position, if there is no char
	// exactly above that point, jump to the end of the line
	// of the previous/next row instead.

	int new_caret_index = EZ_label_FindIndexByRowColumn(label, max(0, label->caret_pos.row + amount), label->caret_pos.col);
	
	if (new_caret_index >= 0)
	{
		EZ_label_SetCaretPosition(label, new_caret_index);
	}
}

//
// Label - The caret was moved.
//
int EZ_label_OnCaretMoved(ez_control_t *self, void *ext_event_info)
{
	ez_label_t *label		= (ez_label_t *)self;
	int scaled_char_size	= label->scaled_char_size;
	int row_visible_start	= 0;
	int row_visible_end		= 0;
	int	col_visible_start	= 0;
	int col_visible_end		= 0;
	int prev_caret_row		= label->caret_pos.row;
	int prev_caret_col		= label->caret_pos.col;
	int new_scroll_x		= self->virtual_x;
	int new_scroll_y		= self->virtual_y;
	int num_visible_rows	= Q_rint((float)self->height / scaled_char_size - 1);	// The number of currently visible rows.
	int num_visible_cols	= Q_rint((float)self->width / scaled_char_size - 1);

	// Find the new row and column of the caret.
	EZ_label_FindRowColumnByIndex(label, label->caret_pos.index, &label->caret_pos.row, &label->caret_pos.col);

	row_visible_start	= (self->virtual_y / scaled_char_size);
	row_visible_end		= (row_visible_start + num_visible_rows);

	// Vertical scrolling.
	if (label->caret_pos.row <= row_visible_start)
	{
		// Caret at the top of the visible part of the text.
		new_scroll_y = label->caret_pos.row * scaled_char_size;
	}
	else if (prev_caret_row >= label->caret_pos.row)
	{
		// Caret in the middle.
	}
	else if (label->caret_pos.row >= (row_visible_end - 1))
	{
		// Caret at the bottom of the visible part.
		new_scroll_y = (label->caret_pos.row - num_visible_rows) * scaled_char_size;

		// Special case if it's the last line. Make sure we're allowed to
		// see the entire row, by scrolling a bit extra at the end.
		if ((label->caret_pos.row == (label->num_rows - 1)) && (new_scroll_y < label->super.virtual_height_min))
		{
			new_scroll_y += Q_rint(0.2 * scaled_char_size);
		}
	}

	// Only scroll horizontally if the text is not wrapped.
	if (!(label->ext_flags & label_wraptext))
	{
		col_visible_start	= (self->virtual_x / scaled_char_size);
		col_visible_end		= (col_visible_start + num_visible_cols);

		if (label->caret_pos.col <= (col_visible_start + 1))
		{
			// At the start of the visible text (columns).
			new_scroll_x = (label->caret_pos.col - 1) * scaled_char_size;
		}
		else if (prev_caret_col >= label->caret_pos.col)
		{
			// In the middle of the visible text.
		}
		else if (label->caret_pos.col >= (col_visible_end - 1))
		{
			// At the end of the visible text.
			new_scroll_x = (label->caret_pos.col - num_visible_cols) * scaled_char_size;
		}
	}

	// Only set a new scroll if it changed.
	if ((new_scroll_x != self->virtual_x) || (new_scroll_y != self->virtual_y))
	{
		EZ_control_SetScrollPosition(self, new_scroll_x, new_scroll_y);
	}

	// Select while holding down shift.
	if (isShiftDown() && (label->select_start > -1))
	{
		label->int_flags |= label_selecting;
		label->select_end = label->caret_pos.index;
	}

	// Deselect any selected text if we're not in selection mode.
	if (!(label->int_flags & label_selecting))
	{
		EZ_label_DeselectText(label);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnCaretMoved, NULL);

	return 0;
}

//
// Label - The text changed in the label.
//
int EZ_label_OnTextChanged(ez_control_t *self, void *ext_event_info)
{
	ez_label_t *label = (ez_label_t *)self;

	// Make sure we have the correct text length saved.
	label->text_length = label->text ? strlen(label->text) : 0;

	// Find the new places to wrap on.
	EZ_label_CalculateWordwraps(label);

	if (label->ext_flags & label_autosize)
	{
		// Only resize after the number of rows if we're wrapping the text.
		if (label->ext_flags & label_wraptext)
		{
			EZ_control_SetSize(self, self->width, (label->scaled_char_size * label->num_rows));
		}
		else
		{
			EZ_control_SetSize(self, (label->scaled_char_size * label->num_cols), (label->scaled_char_size * label->num_rows));
		}
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, label, ez_label_t, OnTextChanged, NULL);

	return 0;
}

//
// Label - Handle a page up/dn key press.
//
static void EZ_label_PageUpDnKeyDown(ez_label_t *label, int key)
{
	int num_visible_rows = Q_rint((float)label->super.height / label->scaled_char_size);

	switch (key)
	{
		case K_PGUP :
		{
			EZ_label_MoveCaretVertically(label, -num_visible_rows);
			break;
		}
		case K_PGDN :
		{
			EZ_label_MoveCaretVertically(label, num_visible_rows);
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Gets the next word boundary.
//
static int EZ_label_GetNextWordBoundary(ez_label_t *label, int cur_pos, qbool forward)
{
	char *txt	= label->text;
	int dir		= forward ? 1 : -1;		

	// Make sure we're not out of bounds.
	clamp(cur_pos, 0, label->text_length);

	// Eat all the whitespaces.
	while ((cur_pos >= 0) && (cur_pos <= label->text_length) && ((txt[cur_pos] == ' ') || (txt[cur_pos] == '\n')))
	{
		cur_pos += dir;
	}
	
	// Find the next word boundary.
	while ((cur_pos >= 0) && (cur_pos <= label->text_length) && (txt[cur_pos] != ' ') && (txt[cur_pos] != '\n'))
	{
		cur_pos += dir;
	}

	return clamp(cur_pos, 0, label->text_length);
}

//
// Label - Handle a arrow key press.
//
static void EZ_label_ArrowKeyDown(ez_label_t *label, int key)
{
	switch(key)
	{
		case K_LEFTARROW :
		{
			if (isCtrlDown())
			{
				EZ_label_SetCaretPosition(label, EZ_label_GetNextWordBoundary(label, label->caret_pos.index, false));
			}
			else
			{
				EZ_label_SetCaretPosition(label, max(0, label->caret_pos.index - 1));
			}
			break;
		}
		case K_RIGHTARROW :
		{
			if (isCtrlDown())
			{
				EZ_label_SetCaretPosition(label, EZ_label_GetNextWordBoundary(label, label->caret_pos.index, true));
			}
			else
			{
				EZ_label_SetCaretPosition(label, label->caret_pos.index + 1);
			}
			break;
		}
		case K_UPARROW :
		{
			EZ_label_MoveCaretVertically(label, -1);
			break;
		}
		case K_DOWNARROW :
		{
			EZ_label_MoveCaretVertically(label, 1);
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Handle a HOME/END key press.
//
static void EZ_label_EndHomeKeyDown(ez_label_t *label, int key)
{
	switch(key)
	{
		case K_HOME :
		{
			if (isCtrlDown())
			{
				// Move the caret to the start of the text.
				EZ_label_SetCaretPosition(label, 0);
			}
			else
			{
				// Move the caret to the start of the current line.
				EZ_label_SetCaretPosition(label, label->caret_pos.index - label->caret_pos.col + 1);
			}
			break;
		}
		case K_END :
		{
			if (isCtrlDown())
			{
				// Move the caret to the end of the text.
				EZ_label_SetCaretPosition(label, label->text_length);
			}
			else
			{
				// Move the caret to the end of the line.
				int i = 0;
				int line_end_index = 0;

				// Find the last index of the current row.
				while ((label->wordwraps[i].index > 0) && (label->wordwraps[i].index < label->caret_pos.index))
				{
					i++;
				}

				// Special case for last line.
				line_end_index = (label->wordwraps[i].index < 0) ? label->text_length : label->wordwraps[i].index;
				EZ_label_SetCaretPosition(label, line_end_index);
			}
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Handle backspace/delete key presses.
//
static void EZ_label_BackspaceDeleteKeyDown(ez_label_t *label, int key)
{
	// Don't allow deleting if read only.
	if (label->ext_flags & label_readonly)
	{
		return;
	}

	if (LABEL_TEXT_SELECTED(label))
	{
		// Find the start and end of the selected text.
		int start = (label->select_start < label->select_end) ? label->select_start : label->select_end;
		int end	  = (label->select_start < label->select_end) ? label->select_end : label->select_start;

		// Remove a selected chunk of text.				
		EZ_label_RemoveText(label, start, end);
		
		// When deleting we need to move the cursor so that
		// it's at the "same point" in the text afterwards.
		if (label->select_start < label->select_end)
		{
			EZ_label_SetCaretPosition(label, label->select_start);
		}

		// Deselect what we just deleted.
		EZ_label_DeselectText(label);
	}
	else if (key == K_BACKSPACE)
	{
		// Remove a single character.
		EZ_label_RemoveText(label, label->caret_pos.index - 1, label->caret_pos.index);
		EZ_label_SetCaretPosition(label, label->caret_pos.index - 1);
	}
	else if (key == K_DEL)
	{
		EZ_label_RemoveText(label, label->caret_pos.index, label->caret_pos.index + 1);
	}
}

//
// Label - Handles Ctrl combination key presses.
//
static void EZ_label_CtrlComboKeyDown(ez_label_t *label, int key)
{
	char c = (char)key;

	switch (c)
	{
		case 'c' :
		case 'C' :
		{
			if (LABEL_TEXT_SELECTED(label))
			{
				// CTRL + C (Copy to clipboard).
				int selected_len = EZ_label_GetSelectedTextSize(label) + 1;
				char *selected_text = (char *)Q_malloc(selected_len * sizeof(char));

				EZ_label_GetSelectedText(label, selected_text, selected_len);

				CopyToClipboard(selected_text);
				break;
			}
		}
		case 'v' :
		case 'V' :
		{
			// CTRL + V (Paste from clipboard).
			char *pasted_text = ReadFromClipboard();
			EZ_label_AppendText(label, label->caret_pos.index, pasted_text);
			break;
		}
		case 'a' :
		case 'A' :
		{
			// CTRL + A (Select all).
			label->select_start = 0;
			label->select_end	= label->text_length;
			label->int_flags	|= label_selecting;
			EZ_label_SetCaretPosition(label, label->select_end);
			label->int_flags	&= ~label_selecting;
			break;
		}
		default :
		{
			break;
		}
	}
}

//
// Label - Handles input key was presses.
//
static void EZ_label_InputKeyDown(ez_label_t *label, int key)
{
	char c = (char)key;

	// Don't allow input if read only.
	if (label->ext_flags & label_readonly)
	{
		return;
	}

	// User typing text into the label.
	// TODO : Add support for input of all quake chars
	if ((label->caret_pos.index >= 0) 
		&& (	
				((c >= 'a') && (c <= 'z'))
				||	
				((c >= 'A') && (c <= 'Z'))
			)
		)
	{
		char char_str[2];				

		// First remove any selected text.
		if (LABEL_TEXT_SELECTED(label))
		{
			int start = (label->select_start < label->select_end) ? label->select_start : label->select_end;
			int end	  = (label->select_start < label->select_end) ? label->select_end : label->select_start;

			EZ_label_RemoveText(label, start, end);

			// Make sure the caret stays in the same place.
			EZ_label_SetCaretPosition(label, (label->select_start < label->select_end) ? label->select_start : label->select_end);
		}

		// Turn off selecting mode, since we don't want to be selecting
		// when typing capital letters for instance.
		EZ_label_DeselectText(label);

		// Add the text.
		snprintf(char_str, 2, "%c", (char)key);
		EZ_label_AppendText(label, label->caret_pos.index, char_str);
		EZ_label_SetCaretPosition(label, label->caret_pos.index + 1);
	}
}

//
// Label - Key down event.
//
int EZ_label_OnKeyDown(ez_control_t *self, int key, int unichar)
{
	ez_label_t *label	= (ez_label_t *)self;
	int key_handled		= false;

	key_handled = EZ_control_OnKeyDown(self, key, unichar);

	if (key_handled)
	{
		return true;
	}

	key_handled = true;

	// Only handle keys if text is selectable.
	if (label->ext_flags & label_selectable)
	{
		// Key down.
		switch (key)
		{
			case K_LEFTARROW :
			case K_RIGHTARROW :
			case K_UPARROW :
			case K_DOWNARROW :
			{
				EZ_label_ArrowKeyDown(label, key);
				break;
			}
			case K_PGDN :
			case K_PGUP :
			{
				EZ_label_PageUpDnKeyDown(label, key);
				break;
			}
			case K_HOME :
			case K_END :
			{
				EZ_label_EndHomeKeyDown(label, key);
				break;
			}
			case K_TAB :
			{
				break;
			}
			case K_LSHIFT :
			case K_RSHIFT :
			case K_SHIFT :
			{
				// Start selecting.
				label->select_start = label->caret_pos.index;
				label->int_flags |= label_selecting;
				break;
			}
			case K_RCTRL :
			case K_LCTRL :
			case K_CTRL :
			{
				break;
			}
			case K_DEL :
			case K_BACKSPACE :
			{
				EZ_label_BackspaceDeleteKeyDown(label, key);
				break;
			}
			default :
			{
				if (isCtrlDown())
				{
					EZ_label_CtrlComboKeyDown(label, key);
				}
				else
				{
					EZ_label_InputKeyDown(label, key);
				}
				break;
			}
		}
	}

	CONTROL_EVENT_HANDLER_CALL(&key_handled, self, ez_control_t, OnKeyDown, key, unichar);
	return key_handled;
}

//
// Label - Key up event.
//
int EZ_label_OnKeyUp(ez_control_t *self, int key, int unichar)
{
	ez_label_t *label	= (ez_label_t *)self;
	int key_handled		= false;

	key_handled = EZ_control_OnKeyUp(self, key, unichar);

	if (key_handled)
	{
		return true;
	}

	key_handled = true;

	// Key up.
	switch (key)
	{
		case K_SHIFT :
		{
			label->int_flags &= ~label_selecting;
			break;
		}
	}

	return key_handled;
}

//
// Label - The mouse is hovering within the bounds of the label.
//
int EZ_label_OnMouseHover(ez_control_t *self, mouse_state_t *ms)
{
	qbool mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseHover(self, ms);

	// Find at what index in the text where the mouse is currently over.
	if (label->int_flags & label_selecting)
	{
		label->select_end = EZ_label_FindMouseTextIndex(label, ms);
		EZ_label_SetCaretPosition(label, label->select_end);
	}

	CONTROL_EVENT_HANDLER_CALL(NULL, self, ez_control_t, OnMouseHover, ms);

	return mouse_handled;
}

//
// Label - Handles the mouse down event.
//
int EZ_label_OnMouseDown(ez_control_t *self, mouse_state_t *ms)
{
	int mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseDown(self, ms);

	// Find at what index in the text the mouse was clicked.
	label->select_start = EZ_label_FindMouseTextIndex(label, ms);

	// Reset the caret and selection end since we just started a new select / caret positioning.
	label->select_end = -1;
	EZ_label_HideCaret(label);

	// We just started to select some text.
	label->int_flags |= label_selecting;

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseDown, ms);

	return true;
}

//
// Label - Handles the mouse up event.
//
int EZ_label_OnMouseUp(ez_control_t *self, mouse_state_t *ms)
{
	int mouse_handled = false;
	ez_label_t *label = (ez_label_t *)self;

	// Call the super class first.
	mouse_handled = EZ_control_OnMouseUp(self, ms);

	// Find at what index in the text where the mouse was released.
	if ((self->int_flags & control_focused) && (label->int_flags & label_selecting))
	{
		label->select_end = EZ_label_FindMouseTextIndex(label, ms);
		
		EZ_label_SetCaretPosition(label, label->select_end);
	}
	else
	{
		label->select_start		= -1;
		label->select_end		= -1;
		EZ_label_SetCaretPosition(label, -1);
	}

	// We've stopped selecting.
	label->int_flags &= ~label_selecting;

	CONTROL_EVENT_HANDLER_CALL(&mouse_handled, self, ez_control_t, OnMouseUp, ms);

	return mouse_handled;
}


