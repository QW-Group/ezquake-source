/*
	$Id: cl_screen.h,v 1.3 2006-12-06 01:07:46 cokeman1982 Exp $
*/

#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.width - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.height - sb_lines + 8 * var##_y.value : 8 * var##_y.value)

qbool SCR_OnChangeMVHudPos(cvar_t *var, char *newval);