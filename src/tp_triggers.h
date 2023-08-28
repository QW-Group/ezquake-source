/*
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

	$Id: tp_triggers.h,v 1.2 2007-05-13 13:41:44 johnnycz Exp $
*/

#define		RE_PRINT_LOW		1
#define		RE_PRINT_NEDIUM		2
#define		RE_PRINT_HIGH		4
#define		RE_PRINT_CHAT		8
#define		RE_PRINT_CENTER		16
#define		RE_PRINT_ECHO		32
#define		RE_PRINT_INTERNAL	64
#define		RE_PRINT_ALL		31 // all of the above except internal
#define		RE_FINAL			256 // do not look for other triggers if matching is succesful
#define		RE_REMOVESTR		512 // do not display string if matching is uccesful
#define		RE_NOLOG			1024 // do not log string if matching is succesful
#define		RE_ENABLED			2048 // trigger is enabled
#define		RE_NOACTION			4096 // do not call alias

typedef struct pcre_trigger_s {
	char					*name;
	char					*regexpstr;
	struct pcre_trigger_s*	next;
	pcre2_code*				regexp;
	unsigned				flags;
	float					min_interval;
	double					lasttime;
	int						counter;
} pcre_trigger_t;

typedef void internal_trigger_func (const char *s);

typedef struct pcre_internal_trigger_s {
	struct pcre_internal_trigger_s	*next;
	pcre2_code						*regexp;
	internal_trigger_func			*func;
	unsigned						flags;
} pcre_internal_trigger_t;

// re-triggers
qbool CL_SearchForReTriggers (const char *s, unsigned trigger_type);
// if true, string should not be displayed
pcre_trigger_t *CL_FindReTrigger (char *name);
void CL_RE_Trigger_ResetLasttime (void);

// message triggers
void TP_SearchForMsgTriggers (const char *s, int level);
