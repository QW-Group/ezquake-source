#define	ELEMENT_X_COORD(var)	((var##_x.value < 0) ? vid.width - strlen(str) * 8 + 8 * var##_x.value: 8 * var##_x.value)
#define	ELEMENT_Y_COORD(var)	((var##_y.value < 0) ? vid.height - sb_lines + 8 * var##_y.value : 8 * var##_y.value)
#define Q_strncatz(dest, src)	\
	do {	\
		strncat(dest, src, sizeof(dest) - strlen(dest) - 1);	\
		dest[sizeof(dest) - 1] = 0;	\
	} while (0)
