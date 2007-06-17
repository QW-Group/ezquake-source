/*
	$Id: cl_screen.h,v 1.7 2007-06-17 18:21:39 cokeman1982 Exp $
*/

#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.width - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.height - sb_lines + 8 * var##_y.value : 8 * var##_y.value)

qbool SCR_OnChangeMVHudPos(cvar_t *var, char *newval);
void SCR_SetupAutoID (void);

// the current position of the mouse pointer
extern double cursor_x, cursor_y;
