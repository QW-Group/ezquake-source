/*
	$Id: cl_screen.h,v 1.6 2007-02-25 14:12:01 johnnycz Exp $
*/

#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.width - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.height - sb_lines + 8 * var##_y.value : 8 * var##_y.value)

qbool SCR_OnChangeMVHudPos(cvar_t *var, char *newval);

// the current position of the mouse pointer
extern double cursor_x, cursor_y;
