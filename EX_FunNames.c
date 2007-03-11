/*
    $Id: EX_FunNames.c,v 1.11 2007-03-11 06:01:38 disconn3ct Exp $
*/

#include "quakedef.h"
#include "keys.h"


// ----------------------------------------------
//
// Fun_GetDollar
// converts character to its fun representation
// when '$' sign is specified (or alt pressed)
//

int Fun_GetDollar(int c)
{
	if (c >= '0' && c <= '9')
		return c - 30;
	else {
		switch (c) {
				case '(':   return 128; break;
				case '=':   return 129; break;
				case ')':   return 130; break;
				case '<':   return 29;  break;
				case '-':   return 30;  break;
				case '>':   return 31;  break;
				case ',':   return 15;  break;
				case '.':   return 143; break;
				case '[':   return 16;  break;
				case ']':   return 17;  break;
				case 'a':   return 131; break;
				case 'c':   return 139; break;
				case 'd':   return 141; break;
				case 'g':   return 134; break;
				case 'r':   return 135; break;
				case 'y':   return 136; break;
				case 'b':   return 137; break;
				default:    return c;
		}
	}
}

// ----------------------------------------------
//
// Fun_ConvertKey
// converts key pressed to fun letter
// depending on the 'control and 'alt' key state
//

int Fun_ConvertKey(int key)
{
	int ret = key;

	if (isAltDown())       // alt
		ret = Fun_GetDollar(ret);

	if (isCtrlDown())    // control
		ret = ret | 128;

	return ret;
}




// ----------------------------------------------
//
// Fun_ConvertText
// converts text string to fun text string
// (fun is always the same length or shorter)
//

#define Case break;case
#define Default break;default

void Fun_ConvertText(char *name)
{
	char *res, *text;
	int s, d, len;

	if (name == NULL || strlen(name) <= 0)
		return;

	res = (char *) Q_malloc (strlen(name) + 10);
	text = (char *) Q_malloc (strlen(name) + 10);
	strlcpy(text, name, strlen(name));
	len = strlen(text);
	d = 0;
	for (s=0; s < len; ) {
		char c;
		c = text[s++];
		switch (c) {
				case '^':
				c = text[s++];
				res[d++] = (c > 33) ? c + 128 : c;
			Case '$':
				c = text[s++];
				if (c >= '0' && c <= '9') {
					res[d++] = c - '0' + 18;
					break;
				}
				switch (c) {
						case '$':   res[d++] = '$';
					Case '^':   res[d++] = '^';
					Case '(':   res[d++] = 128;
					Case '=':   res[d++] = 129;
					Case ')':   res[d++] = 130;
					Case '<':   res[d++] = 29;
					Case '-':   res[d++] = 30;
					Case '>':   res[d++] = 31;
					Case '.':   res[d++] = 143;
					Case ',':   res[d++] = 15;
					Case 'a':   res[d++] = 131;
					Case '_':   res[d++] = 160;
					Case 'c':   res[d++] = 139;
					Case 'd':   res[d++] = 141;
					Case '[':   res[d++] = 16;
					Case ']':   res[d++] = 17;
					Case 'g':   res[d++] = 134; // led
					Case 'r':   res[d++] = 135; // led
					Case 'y':   res[d++] = 136; // led
					Case 'b':   res[d++] = 137; // led
					Case 'x':   // hex code
						{
							int a, b;
							a = text[s++];
							a = (a > '9') ? a - 'a' + 10 : a - '0';
							b = text[s++];
							b = (b > '9') ? b - 'a' + 10 : b - '0';
							if (a < 0  ||  a > 15  ||  b < 0  ||  b > 15)
								break;
							res[d++] = a*16+b;
						}
					Default:    res[d++] = '$'; res[d++] = c;
				}
			Default:
				res[d++] = c;
		}
	}
	res[d] = 0;

	strlcpy(name, res, strlen(res));
	Q_free (res);
	Q_free (text);
}




// ----------------------------------------------
//
// Fun_ConvertCRLF
// Converts $\ and $/ sequences for mm2
// creates new text
//

char * Fun_ConvertCRLF(char *text)
{
	char *res;
	char *s, *d;

	if (text == NULL  ||  strlen(text) <= 0)
		return NULL;

	res = (char *) Q_malloc (strlen(text)+1);
	s = text;
	d = res;
	while (*s) {
		char c = *s++;

		if (c == '$') {
			c = *s++;
			if (c == '\\')
				*d++ = 13;
			if (c == '/')
				*d++ = 10;
		} else
			*d++ = c;
	}

	*d = 0;

	return res;
}



// ----------------------------------------------
//
// NoFake - converts CR to LF
//

void NoFake(char *text)
{
	char *t = text - 1;

	while (*++t)
		*t = (*t == 13) ? 10 : *t;
}


// ----------------------------------------------
//
// Fun to sort
// converts fun text to string prepared to sort
//

void FunToSort(char *text)
{
	char *tmp;
	char *s, *d;
	unsigned char c;
	tmp = (char *)Q_malloc(strlen(text) + 1);

	s = text;
	d = tmp;

	while ((c = (unsigned char)(*s++)) != 0) {
		if (c >= 18  &&  c <= 27)
			c += 30;
		else if (c >= 146  &&  c <= 155)
			c -= 98;
		else if (c >= 32  &&  c <= 126)
			c = tolower(c);
		else if (c >= 160  &&  c <= 254)
			c  = tolower(c-128);
		else {
			switch (c) {
					case 1:
					case 2:
					case 3:
					case 4:
					case 6:
					case 7:
					case 8:
					case 9:
					case 132:   // kwadrat
					c = 210; break;
					case 5:
					case 14:
					case 15:
					case 28:
					case 133:
					case 142:
					case 143:
					case 156:   // dot
					c = 201; break;
					case 29:
					case 157:   // <
					c = 202; break;
					case 30:
					case 158:   // -
					c = 203; break;
					case 31:
					case 159:   // >
					c = 204; break;
					case 128:   // '('
					c = 205; break;
					case 129:   // '='
					c = 206; break;
					case 130:   // ')'
					c = 207; break;
					case 131:   // '+'
					c = 208; break;
					case 127:
					case 255:   // <-
					c = 209; break;
					case 134:   // d1
					c = 211; break;
					case 135:   // d2
					c = 212; break;
					case 136:   // d3
					c = 213; break;
					case 137:   // d4
					c = 214; break;
					case 16:
					case 144:   // '['
					c = '['; break;
					case 17:
					case 145:   // ']'
					c = ']'; break;
					case 141:   // '>'
					c = 200; break;
					case 10:
					case 11:
					case 12:
					case 13:
					case 138:
					case 139:
					case 140:   // ' '
					c = ' '; break;
			}
		}

		*d++ = c;
	}
	*d = 0;

	strcpy(text, tmp);
	free(tmp);
}

