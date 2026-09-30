/*
csqc_builtins.c -- клиентские builtins PR1VM (наш csprogs.dat, слой C, P2.x).

Builtins для клиентского инстанса: номера — baked из TF2003 csdefs.qc (= #N),
аргументы читаются из vm->globals[OFS_PARM0..], возврат в OFS_RETURN,
строки — через CSQC_Client_GetString/PR1VM_ClientSetString (S4) на активном инстансе.

P2.1: dprint/ftos/registercommand/tokenize/argv. Layers A/B are added here as
implemented (drawstring/getstatf/read builtins/sprintf are P2.2/P2.3).
*/

#ifndef CLIENTONLY
#include "qwsvdef.h"
#include "quakedef.h"	// client.h (cls: netchan/fteprotocolextensions/state) с нужными типами
#include <time.h>		// csqc_calltimeofday (#231)
#include <stdlib.h>		// strtod (#81/#117)
#include <ctype.h>		// tolower (#494 crc16 insensitive, #480/481)
#include <math.h>		// libm-математика (T1: #471-475/#532)
#include <string.h>		// strlen/strncmp/strcasecmp (T4: #228-230)
#ifndef _WIN32
#include <strings.h>		// strcasecmp/strncasecmp (T4: #229/230; MSVC — макросы в q_shared.h)
#endif
#include "keys.h"		// Key_KeynumToString/Key_StringToKeynum (Слой D шаг 3)
#include "qsound.h"		// S_LocalSoundWithVol (C3.1 #177)
#include "cl_tent.h"		// CL_CreateBeam (C3.3b #428-431)
#include "gl_model.h"		// custom_model_*/Mod_CustomModel (#431 no-op, C6.1)
#include "crc.h"		// CRC_Init/CRC_ProcessByte/CRC_Value (#494 crc16)
#include "screen.h"		// SCR_CenterPrint (#338 cprint)
#include "pr1vm.h"
#include "csqc_client.h"	// accessor'ы к клиентскому состоянию/выводу (Фаза 5)
#include "utils.h"		// HexToInt (#476/#477 strlennocol/strdecolorize)
#include "sbar.h"		// Sbar_ColorForMap (B10 topcolor_rgb/bottomcolor_rgb)
#include "net.h"		// NET_AdrToString (B10 serverkey "ip")

static pr1vm_t *CSQCVM_Active (void)
{
	return PR1VM_Active ();
}

// Phase 1 L1 P1a (ADR 0019 / docs/archive/ezquake_csqc_client_corebuiltins_plan.md):
// реюз чистых float/vector-тел серверных builtins на клиентском инстансе.
// Тела не трогают строки/edict/sv-состояние, а аргументы/возврат читают через
// G_* макросы (pr_globals) — attach в PR1VM_ExecuteProgram делает pr_globals
// указывающим на globals исполняемой (клиентской) VM, поэтому вызов корректен.
// Нестатические серверные PF_* объявлены в pr_cmds.c; здесь — extern-прототипы.
extern void PF_random (void);
extern void PF_normalize (void);
extern void PF_vlen (void);
extern void PF_vectoyaw (void);
extern void PF_vectoangles (void);
extern void PF_rint (void);
extern void PF_floor (void);
extern void PF_ceil (void);
extern void PF_fabs (void);
extern void PF_sin (void);
extern void PF_cos (void);
extern void PF_sqrt (void);
extern void PF_min (void);
extern void PF_max (void);
extern void PF_bound (void);
extern void PF_traceon (void);
extern void PF_traceoff (void);

// ADR 0019 (Этап 0): собственный токен-контекст клиентской VM (#441 tokenize /
// #442 argv). Серверный PR1 использует свой pr1_tokencontext (pr_cmds.c); здесь —
// свой, чтобы не разделять глобальный токен-буфер движка (Cmd_TokenizeString),
// которым пользуется консоль/обработка команд.
static tokenizecontext_t csqc_tokencontext;

static char *CSQCVM_Str (int ofs)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return NULL;
	return CSQC_Client_GetString (vm, *(int *)&vm->globals[ofs]);
}

static void CSQCVM_SetRetStr (char *s)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_RETURN], s);
}

// Varargs-конкатенация по конвенции PR1 (3 float-слота на аргумент), аналог
// серверного PF_VarString (pr_cmds.c). FTE: PF_VarString(prinst, first, pr_globals)
// — error/objerror/localcmd/cprint/print (first=0), infoadd (first=2).
static char *CSQCVM_VarString (int first)
{
	pr1vm_t *vm = CSQCVM_Active ();
	static char out[2048];
	int i;

	out[0] = 0;
	if (!vm)
		return out;
	for (i = first; i < vm->argc; i++)
	{
		char *s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (s)
			strlcat (out, s, sizeof (out));
	}
	return out;
}

// Entity values in the client VM are raw int bit offsets (N*edict_size), the same
// convention as the interpreter's entity opcodes (OP_STORE_ENT/PR1VM_ProgToEdict)
// and FTE (G_EDICT/G_INT). All builtins must read entity args and write entity
// returns through these helpers; a float cast/assignment corrupts the bits.
// Defined below (entity section); forward-declared for the #347/#459/te_beam users.
static int csqc_ent_of (pr1vm_t *vm, int parmofs);
static float *csqc_ent_field (pr1vm_t *vm, int entnum, const char *name);
static float *csqc_ent_ofs (pr1vm_t *vm, int entnum, int fldofs);	// C2: доступ по кэш-офсету
static void csqc_ret_entity (pr1vm_t *vm, int entnum);
static void csqc_add_one_entity (int e);	// Ф3: arena-эдикт -> ezq entity_t

/*
void(string s, ...) dprint = #25
*/
static void csqc_dprint (void)
{
	char *s = CSQCVM_Str (OFS_PARM0);
	if (s)
		Con_Printf ("%s", s);
}

/*
Порт Q_ftoa (fteqw engine/common/common.c:526) для #26 ftos: float → строка
без потери значащих цифр («infinite decimal places»), обрезка хвостовых нулей.
*/
static void csqc_q_ftoa (char *str, size_t maxlen, float in)
{
	unsigned int i = *((unsigned int *)&in);
	int signbit = (i & 0x80000000u) >> 31;
	int exp = (int)((i & 0x7F800000u) >> 23) - 127;
	int mantissa = (i & 0x007FFFFFu);
	char buf[64];
	char *p;

	if (exp == 128)
	{
		snprintf (buf, sizeof (buf), "%s%s", signbit ? "-" : "",
			mantissa == 0 ? "1.#INF" : "1.#NAN");
		strlcpy (str, buf, maxlen);
		return;
	}
	exp = -exp;
	exp = (int)(exp * 0.30102999957f);	// base 2 → base 10
	exp += 8;
	if (exp <= 0)
		snprintf (buf, sizeof (buf), "%.0f", in);
	else
	{
		char fmt[16];
		snprintf (fmt, sizeof (fmt), "%%.%if", exp);
		snprintf (buf, sizeof (buf), fmt, in);
		// обрезка хвостовых нулей и точки (как Q_ftoa)
		for (p = buf + strlen (buf) - 1; p > buf && *p == '0'; p--)
			*p = '\0';
		if (*p == '.')
			*p = '\0';
	}
	strlcpy (str, buf, maxlen);
}

/*
string(float val) ftos = #26 — FTE-паритет (PF_ftos, pr_bgcmd.c:4645):
целое значение → "%d"; иначе Q_ftoa (дробная часть не теряется).
*/
static void csqc_ftos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float v;
	char buf[64];
	if (!vm)
		return;
	v = vm->globals[OFS_PARM0];
	if (v >= -2147483648.0f && v <= 2147483647.0f && v == (float)(int)v)
		snprintf (buf, sizeof (buf), "%d", (int)v);
	else
		csqc_q_ftoa (buf, sizeof (buf), v);
	CSQCVM_SetRetStr (buf);
}

/*
void(string cmdname) registercommand = #352
*/
static void csqc_registercommand (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *cmd = CSQCVM_Str (OFS_PARM0);
	if (vm && cmd)
		CSQC_Client_RegisterCommand (cmd);
}

/*
float(string varname) cvar = #45

Возвращает значение cvar движка по имени (нет такого cvar — 0). Нужно модулю
для диагностических переключателей (напр. csqc_dbg) и конфига.
*/
static void csqc_cvar (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	vm->globals[OFS_RETURN] = (vm && name && name[0]) ? Cvar_Value (name) : 0;
}

/*
void(vector vang) makevectors = #1
(C6.1) FTE-паритет (pr_csqc.c:669 PF_cs_makevectors, табл. :6634): по вектору
углов (pitch,yaw,roll) пишет v_forward/v_right/v_up модуля. Внутренние глобалы
резолвит CSQC_Client_MakeVectors (нет объявления — no-op). Классический
низкий номер #1 теперь доступен модулям (серверный набор в клиент не грузится).
*/
static void csqc_makevectors (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_MakeVectors (&vm->globals[OFS_PARM0]);
}

/*
void(vector dir) vectorvectors = #432 (T3 Э3) — FTE-паритет (PF_vectorvectors,
pr_bgcmd.c:6559): нормализованный dir → v_forward модуля, ортогональные
v_right/v_up. Тело — CSQC_Client_VectorVectors.
*/
static void csqc_vectorvectors (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_VectorVectors (&vm->globals[OFS_PARM0]);
}

/*
float() random = #7 — FTE-паритет (PF_random, pr_bgcmd.c:6360).
Возвращает в (0,1): (rand&0x7fff)/0x8000 + 0.5/0x8000 — никогда 0 и 1
(в отличие от серверного ezq-PF_random, способного вернуть 1.0).
FTE optional: argc==1 → *x; argc>=2 → a + r*(b-a). Внутренний клиентский
wrapper — серверный PF_random не трогаем (общий с сервером).
*/
static void csqc_random (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float r;
	if (!vm)
		return;
	r = (float)(rand () & 0x7fff) / 0x8000 + (0.5f / 0x8000);
	if (vm->argc == 1)
		r *= vm->globals[OFS_PARM0];
	else if (vm->argc >= 2)
		r = vm->globals[OFS_PARM0] + r * (vm->globals[OFS_PARM1] - vm->globals[OFS_PARM0]);
	vm->globals[OFS_RETURN] = r;
}

/*
float(vector v [, optional entity reference]) vectoyaw = #13 — FTE-паритет
(PF_vectoyaw, pr_bgcmd.c:6775): yaw = (int)(atan2*180/π), <0 → +360.
FTE optional entity — gravity-axis; у нас клиент axis не ведёт (gravitydir нет):
идентичность-ось (дефолт FTE без gravitydir) — отклонение задокум.
*/
static void csqc_vectoyaw (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *v;
	float x, y, yaw;
	if (!vm)
		return;
	v = &vm->globals[OFS_PARM0];
	x = v[0];
	y = v[1];
	if (y == 0 && x == 0)
		yaw = 0;
	else
	{
		yaw = (float)(int)(atan2 (y, x) * 180 / M_PI);
		if (yaw < 0)
			yaw += 360;
	}
	vm->globals[OFS_RETURN] = yaw;
}

/*
vector(vector fwd [, optional vector up]) vectoangles = #51 — FTE-паритет
(PF_vectoangles pr_bgcmd.c:6822 → VectorAngles mathlib.c:294, meshpitch=1).
Optional up → roll. meshpitch/r_meshroll у нас игнорируются (=1) — отклонение.
*/
static void csqc_vectoangles (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const float *forward;
	float *up;
	float yaw, pitch, roll;
	float result[3];

	if (!vm)
		return;
	forward = &vm->globals[OFS_PARM0];
	up = (vm->argc >= 2) ? &vm->globals[OFS_PARM0 + 3] : NULL;

	if (forward[1] == 0 && forward[0] == 0)
	{
		if (forward[2] > 0)
		{
			pitch = -M_PI * 0.5;
			yaw = up ? atan2 (-up[1], -up[0]) : 0;
		}
		else
		{
			pitch = M_PI * 0.5;
			yaw = up ? atan2 (up[1], up[0]) : 0;
		}
		roll = 0;
	}
	else
	{
		float cp, sp, cy, sy;
		yaw = atan2 (forward[1], forward[0]);
		pitch = -atan2 (forward[2], sqrt (forward[0] * forward[0] + forward[1] * forward[1]));
		if (up)
		{
			float tleft[3], tup[3];
			cp = cos (pitch); sp = sin (pitch);
			cy = cos (yaw); sy = sin (yaw);
			tleft[0] = -sy; tleft[1] = cy; tleft[2] = 0;
			tup[0] = sp * cy; tup[1] = sp * sy; tup[2] = cp;
			roll = -atan2 (up[0] * tleft[0] + up[1] * tleft[1] + up[2] * tleft[2],
				up[0] * tup[0] + up[1] * tup[1] + up[2] * tup[2]);
		}
		else
			roll = 0;
	}
	pitch *= 180 / M_PI;
	yaw *= 180 / M_PI;
	roll *= 180 / M_PI;
	/* meshpitch=1: r_meshpitch/r_meshroll не применяем (отклонение) */
	if (pitch < 0) pitch += 360;
	if (yaw < 0) yaw += 360;
	if (roll < 0) roll += 360;
	result[0] = pitch; result[1] = yaw; result[2] = roll;
	vm->globals[OFS_RETURN] = result[0];
	vm->globals[OFS_RETURN + 1] = result[1];
	vm->globals[OFS_RETURN + 2] = result[2];
}

/*
float(string s) tokenize = #441
*/
static void csqc_tokenize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	if (s)
		Cmd_TokenizeStringEx (&csqc_tokencontext, s);
	vm->globals[OFS_RETURN] = Cmd_ArgcEx (&csqc_tokencontext);
}

/*
string(float n) argv = #442
*/
static void csqc_argv (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int n;
	if (!vm)
		return;
	n = (int)vm->globals[OFS_PARM0];
	CSQCVM_SetRetStr (Cmd_ArgvEx (&csqc_tokencontext, n));
}

/*
string(string s1, optional string s2, ...) strcat = #115
(P2.2) Конкатенация переданных строк (до vm->argc аргументов).
*/
static void csqc_strcat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[2048];
	int n, i, len = 0;

	if (!vm)
		return;
	n = vm->argc;
	if (n <= 0)
		n = 1;
	buf[0] = 0;
	// Параметры PR1 — каждые 3 float-слота на аргумент (как PF_VarString,
	// pr_cmds.c: OFS_PARM0 + i*3); argc = число аргументов.
	for (i = 0; i < n && i < 16; i++)
	{
		char *s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (s)
			len += snprintf (buf + len, sizeof (buf) - len, "%s", s);
		if (len >= (int)sizeof (buf) - 1)
			break;
	}
	CSQCVM_SetRetStr (buf);
}

/*
float(string s1, string sub, optional float startidx) strstrofs = #221
(P2.2) Возвращает позицию подстроки (0-based) или -1.
*/
/*
float(string s1, string sub, optional float startidx) strstrofs = #221 — FTE-паритет
(PF_strstrofs pr_bgcmd.c:4611): start вне [0,len] (и не 0) → −1.
*/
static void csqc_strstrofs (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *hay, *needle, *p;
	int start, len;

	if (!vm)
		return;
	hay = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	needle = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM1]);
	start = (vm->argc > 2) ? (int)vm->globals[OFS_PARM2] : 0;
	if (!hay)
		hay = "";
	if (!needle)
		needle = "";
	len = strlen (hay);
	if (start != 0 && (start < 0 || start > len))
	{
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	p = strstr (hay + start, needle);
	vm->globals[OFS_RETURN] = p ? (p - hay) : -1;
}

/*
string(float ccase, float redalpha, float redchars, string str, ...) strconv = #224
(FTE_STRINGS) Порт FTE PF_strconv + chrconv_number/chrconv_punct/chrchar_alpha
(pr_bgcmd.c:4427-4556): bulk-конверсия регистра/цвета. ccase 0 same/1 lower/2 upper;
redalpha 0 same/1 white/2 red/5 alternate/6 alternate-alternate; redchars — аналогично
для цифр. Аргумент-строка — vararg с позиции 3 (как FTE PF_VarString(prinst,3,...)).
*/
static int csqc_chrconv_number (int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 5:
	case 6:
	case 0:
		break;
	case 1:
		base = '0';
		break;
	case 2:
		base = '0' + 128;
		break;
	case 3:
		base = '0' - 30;
		break;
	case 4:
		base = '0' + 128 - 30;
		break;
	}
	return i + base;
}

static int csqc_chrconv_punct (int i, int base, int conv)
{
	i -= base;
	switch (conv)
	{
	default:
	case 0:
		break;
	case 1:
		base = 0;
		break;
	case 2:
		base = 128;
		break;
	}
	return i + base;
}

static int csqc_chrchar_alpha (int i, int basec, int baset, int convc, int convt, int charnum)
{
	i -= baset + basec;
	switch (convt)
	{
	default:
	case 0:
		break;
	case 1:
		baset = 0;
		break;
	case 2:
		baset = 128;
		break;
	case 5:
	case 6:
		baset = 128 * ((charnum & 1) == (convt - 5));
		break;
	}
	switch (convc)
	{
	default:
	case 0:
		break;
	case 1:
		basec = 'a';
		break;
	case 2:
		basec = 'A';
		break;
	}
	return i + basec + baset;
}

static void csqc_strconv (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int ccase, redalpha, rednum, len, i;
	const unsigned char *string;
	unsigned char resbuf[2048];
	unsigned char *result = resbuf;

	if (!vm)
		return;
	ccase = (int)vm->globals[OFS_PARM0];
	redalpha = (int)vm->globals[OFS_PARM1];
	rednum = (int)vm->globals[OFS_PARM2];
	string = (const unsigned char *)CSQCVM_VarString (3);
	len = strlen ((const char *)string);
	if (len >= (int)sizeof (resbuf))
		len = sizeof (resbuf) - 1;

	for (i = 0; i < len; i++, string++, result++)
	{
		if (*string >= '0' && *string <= '9')
			*result = csqc_chrconv_number (*string, '0', rednum);
		else if (*string >= '0' + 128 && *string <= '9' + 128)
			*result = csqc_chrconv_number (*string, '0' + 128, rednum);
		else if (*string >= '0' + 128 - 30 && *string <= '9' + 128 - 30)
			*result = csqc_chrconv_number (*string, '0' + 128 - 30, rednum);
		else if (*string >= '0' - 30 && *string <= '9' - 30)
			*result = csqc_chrconv_number (*string, '0' - 30, rednum);
		else if (*string >= 'a' && *string <= 'z')
			*result = csqc_chrchar_alpha (*string, 'a', 0, ccase, redalpha, i);
		else if (*string >= 'A' && *string <= 'Z')
			*result = csqc_chrchar_alpha (*string, 'A', 0, ccase, redalpha, i);
		else if (*string >= 'a' + 128 && *string <= 'z' + 128)
			*result = csqc_chrchar_alpha (*string, 'a', 128, ccase, redalpha, i);
		else if (*string >= 'A' + 128 && *string <= 'Z' + 128)
			*result = csqc_chrchar_alpha (*string, 'A', 128, ccase, redalpha, i);
		else if ((*string & 127) < 16 || !redalpha)
			*result = *string;
		else if (*string < 128)
			*result = csqc_chrconv_punct (*string, 0, redalpha);
		else
			*result = csqc_chrconv_punct (*string, 128, redalpha);
	}
	*result = 0;
	CSQCVM_SetRetStr ((char *)resbuf);
}

/*
float(float property, ...) getproperty = #309

Полный read-паритет FTE (PF_R_GetViewFlag, pr_csqc.c): чтение текущего состояния
рендера движка (r_refdef/cl/vid), а не «значения, поставленные модулем #303».
Числа VF_* — из TF2003-qvm/csqc/csdefs.qc (346-375). set-флаги (DRAWWORLD и пр.)
в getter-списке FTE отсутствуют → default 0; у нас тоже 0. 3D-сцена (#303/#304) —
вне скоупа (ADR 0018).
*/
#define CSQC_VF_MIN		1	// viewport top-left (x,y)
#define CSQC_VF_MIN_X		2
#define CSQC_VF_MIN_Y		3
#define CSQC_VF_SIZE		4	// viewport width/height
#define CSQC_VF_SIZE_X		5
#define CSQC_VF_SIZE_Y		6
#define CSQC_VF_VIEWPORT	7	// (width, height)
#define CSQC_VF_FOV		8	// (fov_x, fov_y)
#define CSQC_VF_FOV_X		9
#define CSQC_VF_FOV_Y		10
#define CSQC_VF_ORIGIN		11
#define CSQC_VF_ORIGIN_X	12
#define CSQC_VF_ORIGIN_Y	13
#define CSQC_VF_ORIGIN_Z	14
#define CSQC_VF_ANGLES		15
#define CSQC_VF_ANGLES_X	16
#define CSQC_VF_ANGLES_Y	17
#define CSQC_VF_ANGLES_Z	18
#define CSQC_VF_CL_VIEWANGLES	33
#define CSQC_VF_CL_VIEWANGLES_X	34
#define CSQC_VF_CL_VIEWANGLES_Y	35
#define CSQC_VF_CL_VIEWANGLES_Z	36
#define CSQC_VF_AFOV		203
#define CSQC_VF_SCREENVSIZE	204
#define CSQC_VF_SCREENPSIZE	205

static void csqc_getproperty (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float prop;
	float *r;

	if (!vm)
		return;
	prop = vm->globals[OFS_PARM0];
	r = &vm->globals[OFS_RETURN];
	r[0] = r[1] = r[2] = 0;
	if (cls.state != ca_active)
		return;

	switch ((int)prop)
	{
	case CSQC_VF_SCREENVSIZE:
	case CSQC_VF_SCREENPSIZE:
		// «виртуальный»/«физический» размер; в ezquake без OS-скейла — одно и то же.
		r[0] = vid.width;
		r[1] = vid.height;
		break;
	case CSQC_VF_FOV:
		r[0] = r_refdef.fov_x;
		r[1] = r_refdef.fov_y;
		break;
	case CSQC_VF_FOV_X:
		r[0] = r_refdef.fov_x;
		break;
	case CSQC_VF_FOV_Y:
		r[0] = r_refdef.fov_y;
		break;
	case CSQC_VF_AFOV:
		// FTE: r_refdef.afov; в ezquake его нет — приближённо cvar fov.
		r[0] = Cvar_Value ("fov");
		break;
	case CSQC_VF_ORIGIN:
		VectorCopy (r_refdef.vieworg, r);
		break;
	case CSQC_VF_ORIGIN_X:
		r[0] = r_refdef.vieworg[0];
		break;
	case CSQC_VF_ORIGIN_Y:
		r[0] = r_refdef.vieworg[1];
		break;
	case CSQC_VF_ORIGIN_Z:
		r[0] = r_refdef.vieworg[2];
		break;
	case CSQC_VF_ANGLES:
		VectorCopy (r_refdef.viewangles, r);
		break;
	case CSQC_VF_ANGLES_X:
		r[0] = r_refdef.viewangles[0];
		break;
	case CSQC_VF_ANGLES_Y:
		r[0] = r_refdef.viewangles[1];
		break;
	case CSQC_VF_ANGLES_Z:
		r[0] = r_refdef.viewangles[2];
		break;
	case CSQC_VF_CL_VIEWANGLES:
		VectorCopy (cl.viewangles, r);
		break;
	case CSQC_VF_CL_VIEWANGLES_X:
		r[0] = cl.viewangles[0];
		break;
	case CSQC_VF_CL_VIEWANGLES_Y:
		r[0] = cl.viewangles[1];
		break;
	case CSQC_VF_CL_VIEWANGLES_Z:
		r[0] = cl.viewangles[2];
		break;
	case CSQC_VF_VIEWPORT:	// FTE: grect.width/height
		r[0] = r_refdef.vrect.width;
		r[1] = r_refdef.vrect.height;
		break;
	case CSQC_VF_MIN:
		r[0] = r_refdef.vrect.x;
		r[1] = r_refdef.vrect.y;
		break;
	case CSQC_VF_MIN_X:
		r[0] = r_refdef.vrect.x;
		break;
	case CSQC_VF_MIN_Y:
		r[0] = r_refdef.vrect.y;
		break;
	case CSQC_VF_SIZE:
		r[0] = r_refdef.vrect.width;
		r[1] = r_refdef.vrect.height;
		break;
	case CSQC_VF_SIZE_X:
		r[0] = r_refdef.vrect.width;
		break;
	case CSQC_VF_SIZE_Y:
		r[0] = r_refdef.vrect.height;
		break;
	default:
		// set-флаги (DRAWWORLD/PERSPECTIVE/...) и без аналога/DP-legacy — 0
		// (в FTE getter-списка нет, default возвращает 0).
		break;
	}
}

/*
vector(vector v) unproject = #310 / vector(vector v) project = #311 (C5-E Ф1).
Экран↔мир через матрицы движка (R_GetModelviewMatrix/R_GetProjectionMatrix/
R_GetViewport, r_matrix.c). Неудача/вырожденная матрица -> '0 0 0'.
*/
static void csqc_project (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float sx = 0, sy = 0, sz = 0;
	if (!vm)
		return;
	if (!CSQC_Client_Project (&vm->globals[OFS_PARM0], &sx, &sy, &sz))
		sx = sy = sz = 0;
	vm->globals[OFS_RETURN + 0] = sx;
	vm->globals[OFS_RETURN + 1] = sy;
	vm->globals[OFS_RETURN + 2] = sz;
}

static void csqc_unproject (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float world[3] = { 0, 0, 0 };
	if (!vm)
		return;
	if (!CSQC_Client_Unproject (vm->globals[OFS_PARM0], vm->globals[OFS_PARM0 + 1],
		vm->globals[OFS_PARM0 + 2], world))
		world[0] = world[1] = world[2] = 0;
	vm->globals[OFS_RETURN + 0] = world[0];
	vm->globals[OFS_RETURN + 1] = world[1];
	vm->globals[OFS_RETURN + 2] = world[2];
}

/*
void() clearscene = #300 / void(float mask) addentities = #301 /
float(float property, ...) setproperty = #303 / void() renderscene = #304
No-op: 3D-рендер модуля не делаем (движок рисует сам), HUD — поверх.
*/
static void csqc_clearscene (void)
{
	// FTE: clearscene сбрасывает view-свойства (#303 setproperty) и rentity-список.
	CSQC_Client_ResetViewProps ();
	if (CSQC_Client_SceneActive ())
		CL_ClearScene ();
}
/*
void(float mask) addentities = #301 (Ф3 takeover).
FTE PF_R_AddEntityMask (pr_csqc.c:1380): mask&1 (MASK_DELTA=MASK_ENGINE) —
движковая сцена (CL_EmitEntities: мир/игроки/энтити); mask&2 (MASK_STDVIEWMODEL) —
движковая вьюмодель (C4 Э2, FTE CL_LinkViewModel); затем обход CSQC-эдиктов
арены по ВСЕМУ mask (`drawmask & mask`), как в FTE.
*/
static void csqc_addentities (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int mask, e;
	if (!vm || !CSQC_Client_SceneActive ())
		return;
	mask = (int)vm->globals[OFS_PARM0];
	if (mask & 1)
		CL_EmitEntitiesKeepScene ();	// B19: merge без CL_ClearScene (FTE PF_R_AddEntityMask)
	if (mask & 2)
		CSQC_Client_LinkViewModel ();
	for (e = 1; e < vm->num_edicts; e++)
	{
		float *dm;
		if (!CSQC_Client_EntUsed (e))
			continue;
		dm = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_DRAWMASK));
		if (!dm || !((int)dm[0] & mask))
			continue;
		csqc_add_one_entity (e);
	}
}
/*
float(float property, ...) setproperty = #303 (C5-E Ф1: подмножество view).
Обрабатываются VF_MIN/SIZE/VIEWPORT/FOV/ORIGIN/ANGLES (и _X/_Y/_Z); значения
применяются к r_refdef после V_CalcRefdef (cl_view.c) при активном CSQC
(лаг 1 кадр — CSQC_UpdateView в HUD-фазе; документировано). Прочие свойства —
no-op (как FTE default; 3D-сцена/ADR 0018 — Ф3).
*/
static void csqc_setproperty (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int prop, words, i;
	float args[6];
	if (!vm)
		return;
	prop = (int)vm->globals[OFS_PARM0];
	// после property: (argc-1) QC-аргументов по 3 слова; VF_VIEWPORT =
	// vector+vector (позиция+размер) = 6 слов (FTE pr_csqc.c:2542, csdefs VF_VIEWPORT).
	words = (vm->argc - 1) * 3;
	if (words < 0)
		words = 0;
	if (words > 6)
		words = 6;
	for (i = 0; i < words; i++)
		args[i] = vm->globals[OFS_PARM0 + 3 + i];
	// FTE PF_R_SetViewFlag: 1 для распознанного VF_, 0 для неизвестного
	// (pr_csqc.c:2389 и default:2691).
	vm->globals[OFS_RETURN] = CSQC_Client_SetViewProperty (prop, words, args) ? 1 : 0;
}
static void csqc_renderscene (void)
{
	// Ф3 (takeover): #304 renderscene выполняет 3D-рендер кадра (как FTE
	// PF_R_RenderScene -> R_RenderView). Вне takeover — no-op (движок рисует сам).
	if (CSQC_Client_SceneActive ())
		CSQC_Client_RenderScene ();
}

/*
float(vector position, string text, vector size, vector rgb,
      float alpha, float drawflag) drawstring = #326

Двойная сигнатура FTE PF_CL_drawcolouredstring (pr_menu.c:523): при `argc >= 6`
расширенная (rgb=P3, alpha=P4, flag=P5), иначе legacy-DP (белый цвет, alpha=P3,
flag=P4 при argc>=5). Рисуем строку в 2D-оверлее ezquake. Параметры PR1 — каждые
3 слова на аргумент: pos=0..2, text=3, size=6..8, rgb=9..11 (0..1 → байты),
alpha=12, drawflag=15. Масштаб — size.x/8 (ezq-шрифт uniform-only; size.y не
применяется — расхождение с FTE задокументировано в parity-audit §D.2).
Цвет выставляем явно (Draw_SetColor) — не зависит от scr_coloredText.
*/
static void csqc_drawstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int r, gg, b;
	float scale, alpha;
	char *s;
	if (!vm)
		return;
	g = vm->globals;
	s = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 3]);
	if (!s)
	{
		// FTE: null-строка -> -1 (pr_menu.c:553-557).
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	if (vm->argc >= 6)
	{
		r = (int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f);
		gg = (int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f);
		b = (int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f);
		alpha = g[OFS_PARM0 + 12];
	}
	else
	{
		// legacy-DP: (pos, text, size, alpha [, flag]) — белый цвет.
		r = gg = b = 255;
		alpha = g[OFS_PARM0 + 9];
	}
	// Слой D шаг 2: size.x -> scale (8px ячейка FTE); 0 => 1.
	scale = (g[OFS_PARM0 + 6] > 0) ? g[OFS_PARM0 + 6] / 8.0f : 1;
	CSQC_Client_DrawText (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1], s, r, gg, b, alpha, scale);
}

/*
float(float stnum) getstati = #330
FTE PF_cs_getstat_int (pr_csqc.c:2812): G_INT(OFS_RETURN) = stats[stnum] — возвращаются
raw int-биты (не число). Числовое значение модуль читает через getstatf (#331).
0..31 — cl.stats, 32..255 — ext-хранилище (CSQC_Client_GetStatInt).
*/
static void csqc_getstati (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	*(int *)&vm->globals[OFS_RETURN] = CSQC_Client_GetStatInt ((int)vm->globals[OFS_PARM0]);
}

/*
float(float stnum, optional float firstbit, optional float bitcount) getstatf = #331
Паритет FTE PF_cs_getstat_float (pr_csqc.c:2814-2843):
  без доп. аргументов — float-значение стата (statsf, приём stat wire 79);
  при firstbit/bitcount — бит-выборка из int-значения стата (getstatbits).
*/
static void csqc_getstatf (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int stnum;
	if (!vm)
		return;
	stnum = (int)vm->globals[OFS_PARM0];
	if (stnum < 0 || stnum >= MAX_EXTENDED_CL_STATS)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	if (vm->argc > 1)
	{
		// Точный int (не float-путь): большие int теряют младшие биты в float32
		// (FTE pr_csqc.c:2826 читает stats[] как int).
		int val = CSQC_Client_GetStatInt (stnum);
		int first = (int)vm->globals[OFS_PARM1];
		int count = (vm->argc > 2) ? (int)vm->globals[OFS_PARM2] : 1;
		if (first < 0)
			first = 0;
		if (count < 0)
			count = 0;
		if (count > 31)	// FTE делает (1<<count); clamp без UB
			count = 31;
		vm->globals[OFS_RETURN] = (float)((((unsigned int)val) & (((1u << count) - 1u) << first)) >> first);
	}
	else
		vm->globals[OFS_RETURN] = CSQC_Client_GetStatFloat (stnum);
}

/*
string(float firststnum) getstats = #332
Паритет FTE PF_cs_getstat_string при PEXT_CSQC (pr_csqc.c:2844-2854):
statsstr[stnum], приём stat wire 78 (svc_fte_updatestatstring). Legacy packed-int
вариант (4 int-стата, старые движки) не реализован — наш клиент всегда на
FTE_PEXT_CSQC (модуль запускается только при нём).
*/
static void csqc_getstats (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr ((char *)CSQC_Client_GetStatString ((int)vm->globals[OFS_PARM0]));
}

// ---------------------------------------------------------------- Слой D, шаг 1
// 2D-графика. Раскладка параметров — 3-словные ячейки от OFS_PARM0 (см.
// docs/archive/ezquake_csqc_client_layerd_2d_plan.md §ABI). Возвраты draw* —
// FTE-паритет (pr_menu.c): drawpic 1/0 (pic найден), drawfill/drawsubpic 1,
// drawcharacter 1 (0 — null-символ, -1).

/*
float(vector position, float character, vector size, vector rgb, float alpha,
     optional float drawflag) drawcharacter = #320
*/
static void csqc_drawcharacter (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	float scale;
	if (!vm)
		return;
	g = vm->globals;
	scale = (g[OFS_PARM0 + 6] > 0) ? g[OFS_PARM0 + 6] / 8.0f : 1;
	CSQC_Client_DrawCharacter (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1], (int)g[OFS_PARM0 + 3],
		(int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 12], scale);
	// FTE PF_CL_drawcharacter (pr_menu.c:980): null-символ -> -1, иначе 1.
	vm->globals[OFS_RETURN] = (g[OFS_PARM0 + 3] == 0) ? -1 : 1;
}

/*
float(vector position, string pic, vector size, vector rgb, float alpha,
     optional float drawflag) drawpic = #322
Возврат — pic найден (1) / нет (0), FTE PF_CL_drawpic (pr_menu.c:610).
*/
static void csqc_drawpic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	char *name;
	if (!vm)
		return;
	g = vm->globals;
	name = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 3]);
	vm->globals[OFS_RETURN] = CSQC_Client_DrawPic (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1],
		g[OFS_PARM0 + 6], g[OFS_PARM0 + 7], name,
		(int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 12]) ? 1 : 0;
}

/*
void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb,
     float alpha, optional float drawflag) drawsubpic = #328
Возврат — всегда 1 (FTE PF_CL_drawsubpic, pr_menu.c:688).
*/
static void csqc_drawsubpic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	char *name;
	if (!vm)
		return;
	g = vm->globals;
	name = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 6]);
	CSQC_Client_DrawSubPic (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1],
		g[OFS_PARM0 + 3], g[OFS_PARM0 + 4], name,
		g[OFS_PARM0 + 9], g[OFS_PARM0 + 10], g[OFS_PARM0 + 12], g[OFS_PARM0 + 13],
		(int)(bound (0, g[OFS_PARM0 + 15], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 16], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 17], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 18]);
	vm->globals[OFS_RETURN] = 1;
}

/*
float(vector position, vector size, vector rgb, float alpha,
     optional float drawflag) drawfill = #323
Возврат — всегда 1 (FTE PF_CL_drawfill, pr_menu.c:49).
*/
static void csqc_drawfill (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	CSQC_Client_DrawFill (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1],
		g[OFS_PARM0 + 3], g[OFS_PARM0 + 4],
		(int)(bound (0, g[OFS_PARM0 + 6], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 7], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 8], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 9]);
	vm->globals[OFS_RETURN] = 1;
}

/*
void(float width, vector pos1, vector pos2, vector rgb, float alpha,
     optional float drawflag) drawline = #315
FTE PF_CL_drawline (pr_menu.c:1063): width игнорируется (hairline).
*/
static void csqc_drawline (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	CSQC_Client_DrawLine (g[OFS_PARM0 + 3], g[OFS_PARM0 + 4], g[OFS_PARM0 + 6], g[OFS_PARM0 + 7],
		1,
		(int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f),
		(int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f),
		g[OFS_PARM0 + 12]);
}

/*
float(string text, float usecolours, optional vector fontsize) stringwidth = #327
*/
static void csqc_stringwidth (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *text;
	if (!vm)
		return;
	text = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	vm->globals[OFS_RETURN] = CSQC_Client_StringWidth (text ? text : "",
		vm->globals[OFS_PARM0 + 3] != 0, vm->globals[OFS_PARM0 + 6]);
}

/*
string(string name, optional float trywad) precache_pic = #317
Возвращает name, если пикча загрузилась (trywad игнорируется), иначе "".
*/
static void csqc_precache_pic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	if (name && CSQC_Client_PrecachePic (name))
		CSQCVM_SetRetStr (name);
	else
		CSQCVM_SetRetStr ("");
}

/*
L2 — «2D-графика доп» (2026-09-07; #316/#318/#319/#321/#324/#325/#329).
FTE-эталон — pr_menu.c PF_CL_* (iscachedpic 813, drawgetimagesize 1093, freepic 969,
drawrawstring 1019, drawsetcliparea 65, drawresetcliparea 87, drawrotpic_dp 762).
Обёртки-реализации — в CSQC_Client_* (csqc_client.c/.h).
*/

/* float(string name) iscachedpic = #316 — пикча уже в кэше (без загрузки) */
static void csqc_iscachedpic (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	vm->globals[OFS_RETURN] = (name && CSQC_Client_IsCachedPic (name)) ? 1 : 0;
}

/* vector(string picname) drawgetimagesize = #318 — (w,h,0) загруженной пикчи */
static void csqc_drawgetimagesize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	float w = 0, h = 0;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	if (name && CSQC_Client_PicSize (name, &w, &h))
	{
		vm->globals[OFS_RETURN + 0] = w;
		vm->globals[OFS_RETURN + 1] = h;
		vm->globals[OFS_RETURN + 2] = 0;
	}
	else
	{
		vm->globals[OFS_RETURN + 0] = 0;
		vm->globals[OFS_RETURN + 1] = 0;
		vm->globals[OFS_RETURN + 2] = 0;
	}
}

/* void(string name) freepic = #319 — no-op (FTE: тело пустое; пикчи шарятся) */
static void csqc_freepic (void)
{
	/* no-op (FTE-паритет: shader/pic могут использоваться в других местах) */
}

/*
void(vector position, string text, vector scale, vector rgb, float alpha,
     optional float flag) drawrawstring = #321
Раскладка как drawstring #326; «сырой» текст (без &c-префикса/парсинга).
*/
static void csqc_drawrawstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int r, gg, b;
	float scale;
	char *s;
	if (!vm)
		return;
	g = vm->globals;
	s = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0 + 3]);
	if (!s)
		return;
	r = (int)(bound (0, g[OFS_PARM0 + 9], 1) * 255.0f + 0.5f);
	gg = (int)(bound (0, g[OFS_PARM0 + 10], 1) * 255.0f + 0.5f);
	b = (int)(bound (0, g[OFS_PARM0 + 11], 1) * 255.0f + 0.5f);
	scale = (g[OFS_PARM0 + 6] > 0) ? g[OFS_PARM0 + 6] / 8.0f : 1;
	CSQC_Client_DrawRawText (g[OFS_PARM0 + 0], g[OFS_PARM0 + 1], s,
		r, gg, b, g[OFS_PARM0 + 12], scale);
}

/* void(float x, float y, float width, float height) drawsetcliparea = #324 */
static void csqc_drawsetcliparea (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_SetClipArea (vm->globals[OFS_PARM0 + 0], vm->globals[OFS_PARM0 + 3],
		vm->globals[OFS_PARM0 + 6], vm->globals[OFS_PARM0 + 9]);
	vm->globals[OFS_RETURN] = 1;
}

/* void() drawresetcliparea = #325 */
static void csqc_drawresetcliparea (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_ResetClipArea ();
	vm->globals[OFS_RETURN] = 1;
}

/*
void(vector pivot, string picname, vector size, vector mins, float angle,
     vector rgb, float alpha, optional float flag) drawrotpic_dp = #329
No-op: в ezq 2D-пути нет GL-ротации текстурированного quad (докум. отклонение).
*/
static void csqc_drawrotpic_dp (void)
{
	/* no-op (документировано; GL-ротация 2D-quad в ezq отсутствует) */
}

/*
string(string fmt, ...) sprintf = #627
Мини-форматтер (QC) с width/flags/precision — подмножество FTE PF_sprintf_internal
(pr_bgcmd.c:7295). Разбор %[flags][width][.precision]conv; flags - 0 + ' ' #;
width/precision — только литеральные (без '*'/'%$' — отклонение от FTE, задокументировано).
Конверсии d i u x X c s f g e + v ('x y z'); %o/%p/%S/%E/%F/%G/%V/length не поддержаны.
Неизвестная конверсия — директива verbatim. Аргументы — из парам-слотов
(OFS_PARM0 + 3*n), границы vm->argc; строки — CSQC_Client_GetString с валидацией offset.
*/
static char *csqc_sprintf_put_int (char *f, int v)
{
	char tmp[12];
	int n = 0;

	if (v <= 0)
	{
		*f++ = '0';
		return f;
	}
	while (v > 0 && n < (int)sizeof (tmp))
	{
		tmp[n++] = (char)('0' + v % 10);
		v /= 10;
	}
	while (n > 0)
		*f++ = tmp[--n];
	return f;
}

static void csqc_sprintf (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[2048];
	char tmp[512];
	const char *fmt, *p;
	int pn = 1;		// номер аргумента после fmt (base = OFS_PARM0 + pn*3)
	size_t o = 0;

	if (!vm)
		return;
	fmt = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	if (!fmt)
		fmt = "";

	for (p = fmt; *p && o < sizeof (buf) - 1; p++)
	{
		char conv, formatbuf[32], *f;
		const char *dir;
		int flags = 0, width = -1, prec = -1, haswidth = 0;

		if (*p != '%')
		{
			buf[o++] = *p;
			continue;
		}
		dir = p;
		p++;
		if (*p == '%')
		{
			buf[o++] = '%';
			continue;
		}

		// flags: # alternate, 0 zeropad, - left, ' ' space, + sign
		for (;; p++)
		{
			if (*p == '#' || *p == '0' || *p == '-' || *p == ' ' || *p == '+')
				flags |= 1 << (*p == '#' ? 0 : *p == '0' ? 1 : *p == '-' ? 2 : *p == ' ' ? 3 : 4);
			else
				break;
		}
		// width — только литеральное число (без '*')
		if (*p >= '1' && *p <= '9')
		{
			int nd = 0;
			width = 0;
			haswidth = 1;
			while (*p >= '0' && *p <= '9' && nd < 9)
				width = width * 10 + (*p++ - '0'), nd++;
			while (*p >= '0' && *p <= '9')
				p++;
			if (width > 2047)
				width = 2047;
		}
		// precision — только литеральное число
		if (*p == '.')
		{
			int nd = 0;
			p++;
			prec = 0;
			while (*p >= '0' && *p <= '9' && nd < 9)
				prec = prec * 10 + (*p++ - '0'), nd++;
			while (*p >= '0' && *p <= '9')
				p++;
			if (prec > 2047)
				prec = 2047;
		}
		conv = *p;
		if (!conv)
			break;

		// собрать C-формат: %[#][0][-][ ][+][width][.prec]
		f = formatbuf;
		*f++ = '%';
		if (conv != 's' && conv != 'c' && (flags & 1))
			*f++ = '#';
		if (flags & 2) *f++ = '0';
		if (flags & 4) *f++ = '-';
		if (flags & 8) *f++ = ' ';
		if (flags & 16) *f++ = '+';
		if (haswidth)
			f = csqc_sprintf_put_int (f, width);
		if (prec >= 0)
		{
			*f++ = '.';
			f = csqc_sprintf_put_int (f, prec);
		}

		if (conv == 'v')
		{
			double x = 0, y = 0, z = 0;
			char vfmt[192];
			*f++ = 'g';
			*f = 0;
			if (pn < vm->argc)
			{
				x = (double)vm->globals[OFS_PARM0 + pn * 3];
				y = (double)vm->globals[OFS_PARM0 + pn * 3 + 1];
				z = (double)vm->globals[OFS_PARM0 + pn * 3 + 2];
			}
			pn++;
			snprintf (vfmt, sizeof (vfmt), "%s %s %s", formatbuf, formatbuf, formatbuf);
			snprintf (tmp, sizeof (tmp), vfmt, x, y, z);
		}
		else if (conv == 's')
		{
			const char *s = "";
			*f++ = 's';
			*f = 0;
			if (pn < vm->argc)
			{
				int off = *(int *)&vm->globals[OFS_PARM0 + pn * 3];
				char *gs = CSQC_Client_GetString (vm, off);
				// Валидация: неотрицательный offset обязан лежать в строковой
				// области модуля; отрицательные — во временных таблицах.
				if (gs && off >= 0 && (unsigned)off >= (unsigned)vm->progs->numstrings)
					gs = NULL;
				if (gs)
					s = gs;
			}
			pn++;
			snprintf (tmp, sizeof (tmp), formatbuf, s);
		}
		else
		{
			int argok = (pn < vm->argc);
			float av = argok ? vm->globals[OFS_PARM0 + pn * 3] : 0;

			switch (conv)
			{
			case 'd':
			case 'i':
				*f++ = 'd';
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (int)av);
				pn++;
				break;
			case 'u':
				*f++ = 'u';
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (unsigned)(int)av);
				pn++;
				break;
			case 'x':
			case 'X':
				*f++ = conv;
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (unsigned)(int)av);
				pn++;
				break;
			case 'c':
				*f++ = 'c';
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (int)av);
				pn++;
				break;
			case 'f':
			case 'e':
			case 'g':
				*f++ = conv;
				*f = 0;
				snprintf (tmp, sizeof (tmp), formatbuf, (double)av);
				pn++;
				break;
			default:
				// неизвестная конверсия — директива verbatim
				{
					const char *q;
					for (q = dir; q <= p && o < sizeof (buf) - 1; q++)
						buf[o++] = *q;
				}
				continue;
			}
		}

		{
			size_t l = strlen (tmp);
			if (o + l >= sizeof (buf))
				l = sizeof (buf) - 1 - o;
			memcpy (buf + o, tmp, l);
			o += l;
		}
	}
	buf[o] = 0;
	CSQCVM_SetRetStr (buf);
}

/*
void(string evname, string evargs, ...) sendevent = #359
(E2) Реальная запись clcfte_qcrequest(81) — wire-контракт ftew PF_cs_sendevent
(pr_csqc.c:3794) / mvdsv SV_ReadQCRequest (sv_user.c:4616):
  [byte 81] затем до 6 аргументов "[byte type][значение]", затем [byte 0
  (ev_void-терминатор)] и [string evname].
Типы: 's'=1 ev_string+string, 'f'=2 ev_float+float, 'v'=3 ev_vector+3 floats,
'i'=8 ev_integer+long (raw-bits из float-слота, как ftew G_INT), 'e'=4
ev_entity+entity (R12/T1.5: arena-эдикт → серверный номер из поля .entnum;
невалид/freed → world(0), спека ext_csqc_1.txt:384; wire — как ftew
MSG_WriteEntity, common.c:1351-1363). Неизвестный символ (вкл. '\0') — break
(остаток не шлём; 'u'/'F'/'I'/'p' модуль не использует). Гварды: активный
коннект + договорённый FTE_PEXT_CSQC + cl_pext_csqc (сервер без CSQC иначе
дропает клиента, sv_user.c:5146).
Seat-байт (R12): ftew пишет 200+csqc_playerseat только при seat>0
(pr_csqc.c:3869-3872); ezq — single-seat (seat≡0, нет splitscreen/playerview) →
байт не пишется, как у обычного ftew-клиента (Q-I=a, N/A).
*/
#define CSQC_EV_VOID	0
#define CSQC_EV_STRING	1
#define CSQC_EV_FLOAT	2
#define CSQC_EV_VECTOR	3
#define CSQC_EV_ENTITY	4
#define CSQC_EV_INTEGER	8

/* R12/T1.5: entity-wire как ftew MSG_WriteEntity (common.c:1351-1363).
   Некорректный номер (в т.ч. отрицательный) вырождается в world(0) — модуль не
   должен ронять клиент; ftew-ный Host_EndGame при entnum>MAX_EDICTS сюда не
   переносим. */
static void csqc_sendevent_write_entity (int entnum)
{
	if (entnum < 0)
		entnum = 0;
	if (entnum >= 0x8000)
	{
		MSG_WriteShort (&cls.netchan.message, (entnum >> 8) | 0x8000);
		MSG_WriteByte (&cls.netchan.message, entnum & 0xff);
	}
	else
		MSG_WriteShort (&cls.netchan.message, entnum);
}

static void csqc_sendevent (void)
{
	extern cvar_t cl_pext_csqc;
	pr1vm_t *vm = CSQCVM_Active ();
	const char *evname, *argtypes;
	char c;
	int i;

	if (!vm)
		return;
	// B8 (FTE pr_csqc.c:3801): sendevent требует лишь активного соединения, не ca_active.
	// CSQC_Client_ConnectCheck зовёт CSQC_Init из CL_MakeActive до cls.state = ca_active
	// (cl_main.c:437-440) — события из CSQC_Init обязаны уходить на сервер.
	if (!cls.state)
		return;
	if (!cl_pext_csqc.value)
		return;
#ifdef PROTOCOL_VERSION_FTE
	if (!(cls.fteprotocolextensions & FTE_PEXT_CSQC))
		return;
#endif

	evname = CSQCVM_Str (OFS_PARM0);
	argtypes = CSQCVM_Str (OFS_PARM1);
	if (!evname || !argtypes)
		return;

	MSG_WriteByte (&cls.netchan.message, clcfte_qcrequest);

	for (i = 0; i < 6; i++)
	{
		int base = OFS_PARM2 + i * 3;
		c = argtypes[i];
		if (c == 's')
		{
			char *s = CSQC_Client_GetString (vm, *(int *)&vm->globals[base]);
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_STRING);
			MSG_WriteString (&cls.netchan.message, s ? s : "");
		}
		else if (c == 'f')
		{
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_FLOAT);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base]);
		}
		else if (c == 'v')
		{
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_VECTOR);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base + 0]);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base + 1]);
			MSG_WriteFloat (&cls.netchan.message, vm->globals[base + 2]);
		}
		else if (c == 'i')
		{
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_INTEGER);
			MSG_WriteLong (&cls.netchan.message, *(int *)&vm->globals[base]);
		}
		else if (c == 'e')
		{
			// R12/T1.5 (FTE pr_csqc.c:3860-3865): arena-эдикт → серверный номер
			// из поля .entnum; невалид/пусто → world(0).
			int slot = csqc_ent_of (vm, base);
			float *f = (slot > 0 && CSQC_Client_EntUsed (slot))
				? csqc_ent_field (vm, slot, "entnum") : NULL;
			MSG_WriteByte (&cls.netchan.message, CSQC_EV_ENTITY);
			csqc_sendevent_write_entity (f ? (int)(*f) : 0);
		}
		else
			break;
	}

	MSG_WriteByte (&cls.netchan.message, CSQC_EV_VOID);
	MSG_WriteString (&cls.netchan.message, evname);
}

/*
S1 read*-минимум: читают из текущего сетевого сообщения (как FTE). Remove
больше не через стрим — identity модуль берёт из self.entnum (ADR 0017 P2).
*/
static void csqc_readbyte (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3416-3424)
	{
		CSQC_Client_Abort ("PF_ReadByte is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadByte ();
}

/*
float() readchar = #361
(S2) Байт со знаком из текущего сетевого сообщения.
*/
static void csqc_readchar (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3426-3434)
	{
		CSQC_Client_Abort ("PF_ReadChar is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadChar ();
}

static void csqc_readshort (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3436-3444)
	{
		CSQC_Client_Abort ("PF_ReadShort is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadShort ();
}

static void csqc_readlong (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3462-3470)
	{
		CSQC_Client_Abort ("PF_ReadLong is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadLong ();
}

/*
float() readcoord = #364 / string() readstring = #366
(E1/S2) Координата/строка из текущего сетевого сообщения (нужны cgamepacket-echo
и типизированному 76-payload).
*/
static void csqc_readcoord (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3472-3480)
	{
		CSQC_Client_Abort ("PF_ReadCoord is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadCoord ();
}

/*
float() readangle = #365
(S2) Угол из текущего сетевого сообщения.
*/
static void csqc_readangle (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3552-3560)
	{
		CSQC_Client_Abort ("PF_ReadAngle is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadAngle ();
}

static void csqc_readstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3536-3544)
	{
		CSQC_Client_Abort ("PF_ReadString is not valid at this time");
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	s = MSG_ReadString ();
	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_RETURN], s);
}

/*
float() readfloat = #367
(S2) Полный float из текущего сетевого сообщения.
*/
static void csqc_readfloat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3482-3490)
	{
		CSQC_Client_Abort ("PF_ReadFloat is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = MSG_ReadFloat ();
}

/*
R7/T1.4b (#368): PEXT2_REPLACEMENTDELTAS нет в qwprot — локальная константа под
#ifndef (значение — FTE fteqw/engine/common/protocol.h:83). qwprot-субмодуль и
CL_SupportedFTEExtensions2 не трогаем: бит клиентом не анонсируется, ветка
forward-compat (live с mvdsv недостижима).
*/
#ifndef PEXT2_REPLACEMENTDELTAS
#define PEXT2_REPLACEMENTDELTAS 0x00000008
#endif

/*
R7/T1.4b (#368): номер эдикта из текущего потока — PEXT2-aware, паритет FTE
MSGCL_ReadEntity (fteqw/engine/common/common.c:1396-1404): при
PEXT2_REPLACEMENTDELTAS — MSG_ReadBigEntity (:1364-1374: short; бит 0x8000 →
(hi & 0x7fff)<<8 | byte), иначе обычный short (unsigned short, как FTE).
*/
static int CSQC_Client_ReadEntityNum (void)
{
	if (cls.fteprotocolextensions2 & PEXT2_REPLACEMENTDELTAS)
	{
		int num = MSG_ReadShort ();
		if (num & 0x8000)
			num = ((num & 0x7fff) << 8) | MSG_ReadByte ();
		return num;
	}
	return (unsigned short)MSG_ReadShort ();
}

static void csqc_readentitynum (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	if (!CSQC_Client_MayRead ())	// R7/T1.4a (FTE pr_csqc.c:3450-3460)
	{
		CSQC_Client_Abort ("PF_ReadEntityNum is not valid at this time");
		vm->globals[OFS_RETURN] = -1;
		return;
	}
	vm->globals[OFS_RETURN] = CSQC_Client_ReadEntityNum ();
}

/*
Слой D шаг 3 — ввод/интерфейс builtins. B6 (FTE-parity): #340/#341 работают в
QC/DP-домене клавиш (csdefs.qc:1377-1449) — вход/выход транслируется через
CSQC_Client_QCToKeynum/CSQC_Client_KeynumToQC (эталон — fteqw pr_clcmd.c:14/:218).

string(float keynum) keynumtostring = #340
QC-код -> имя клавиши (Key_KeynumToString для внутреннего keynum ezq).
CSQCVM_SetRetStr глубоко копирует в temp-ring инстанса.
*/
static void csqc_keynumtostring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr (Key_KeynumToString (CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0])));
}

/*
float(string keyname) stringtokeynum = #341
Имя клавиши -> QC-код; пустая строка/нет такого имени -> -1
(Key_StringToKeynum даёт -1, KeynumToQC сохраняет -1).
*/
static void csqc_stringtokeynum (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = name ? CSQC_Client_KeynumToQC (Key_StringToKeynum (name)) : -1;
}

/*
float() isdemo = #349
0 — не демо; 1 — обычное демо; 2 — MVD/QTV-просмотр (cls.mvdplayback: 1=MVD, 2=QTV).
Семантика совпадает с FTE PF_cl_playingdemo (pr_clcmd.c: DPB_NONE=0, DPB_MVD=2, иначе 1).
*/
static void csqc_isdemo (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = cls.mvdplayback ? 2 : (cls.demoplayback ? 1 : 0);
}

/*
string(string key) serverkey = #354
FTE PF_cl_serverkey_internal (pr_clcmd.c:1391): синтетические ключи ip/maxplayers/protocol/
dlstate, затем fallback на serverinfo. Deviation (accept+doc): protocol — упрощённая строка
(без разбора QW/ZQ/FTE), dlstate — только процент (FTE — многополевая строка).
*/
static void csqc_serverkey (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *key = CSQCVM_Str (OFS_PARM0);
	char buf[64];
	const char *v = NULL;
	if (!vm)
		return;
	if (!key)
		key = "";
	if (!strcmp (key, "ip"))				// FTE :1396
	{
		if (cls.demoplayback)
			v = cls.demoname;
		else
			v = NET_AdrToString (cls.netchan.remote_address);
	}
	else if (!strcmp (key, "maxplayers"))			// FTE :1430
		snprintf (buf, sizeof (buf), "%d", cl.sv_maxclients), v = buf;
	else if (!strcmp (key, "protocol"))			// FTE :1452 (упрощено)
		v = cls.demoplayback ? "QuakeWorld demo" : "QuakeWorld";
	else if (!strcmp (key, "dlstate"))			// FTE :1434 (упрощено)
	{
		if (!cls.download)
			v = "";
		else
			snprintf (buf, sizeof (buf), "%d", (int)cls.downloadpercent), v = buf;
	}
	else
		v = Info_ValueForKey (cl.serverinfo, key);
	CSQCVM_SetRetStr ((char *)(v ? v : ""));
}

/*
string(float playernum, string keyname) getplayerkeyvalue = #348
Значения scoreboard/userinfo игрока (cl.players[pnum]). Числовые ключи
frags/ping/userid/spectator — форматированием; name/team/topcolor/bottomcolor и
прочие — из userinfo (как FTE PF_cs_getplayerkey_internal, pr_csqc.c:4344).
Пустой слот / вне [0, MAX_CLIENTS) -> "" (пустая строка). Отклонение: pnum<0
(scoreboard-индекс fragsort) не поддержан -> "" (roadmap A6).
*/
/*
void(float sens) setsensitivityscaler = #346
Временный множитель чувствительности мыши (зум-аналог FTE PF_cs_setsensitivityscaler,
in_sensitivityscale). Значение применяет in_sdl2.c к sensitivity в игровом ветвлении.
*/
static void csqc_setsensitivityscaler (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_SetSensitivityScale (vm->globals[OFS_PARM0]);
}

/*
float(float inputsequencenum) getinputstate = #345
Заполняет input_* глобалы из истории отправленных usercmd (C5-A). seq = зеркало
cls.netchan.outgoing_sequence; валиден (servercommandframe, clientcommandframe]
(контракт модуля — движок диапазон не проверяет, спека ext_csqc_1.txt:262);
paused-guard как FTE (pr_csqc.c:4142). Возврат 0, если seq вне кольца истории
(64) или пауза. T2.2: clientcommandframe = последний отправленный seq (FTE
cl.movesequence), поэтому #345(clientcommandframe) отдаёт живой pending-кадр вне
CSQC_Input_Frame (в ring; у FTE — пересборка cl_pendingcmd).
*/
static void csqc_getinputstate (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	unsigned int seq;
	if (!vm)
		return;
	seq = (unsigned int)vm->globals[OFS_PARM0];
	vm->globals[OFS_RETURN] = CSQC_Client_ApplyInput (seq);
}

/*
void(entity ent) runstandardplayerphysics = #347
C5-B: FTE-семантика (pr_csqc.c:4185-4299) — PM_PlayerMove по input_*-глобалам
(модуль зовёт getinputstate(seq) перед вызовом), solid-набор мир+энт+игроки,
поля ent (.mins/.maxs/.gravity/.pmove_flags/.flags), запись .flags/.pmove_flags +
deprec pmove_org/vel/onground. Отклонения (нет полей в ezq pmove) — в csqc_client.c.
Entity-аргумент — сырые int-биты (csqc_ent_of, как FTE G_EDICT): float-чтение
обращало значение spawn() (int-биты N*edict_size) в 0 — квирк C5-B.
*/
static void csqc_runstandardplayerphysics (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int entnum;
	if (!vm)
		return;
	entnum = csqc_ent_of (vm, OFS_PARM0);
	CSQC_Client_RunPlayerPhysics (entnum);
}

/*
entity(float entnum) edict_num = #459
C2.1: entity-значение по номеру — сырые int-биты N*edict_size (как self в
SetEntityContext и spawn), FTE PF_edict_for_num пишет G_INT(OFS_RETURN), а не float.
Вне диапазона арены -> 0 (world).
*/
static void csqc_edict_num (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int entnum;
	if (!vm)
		return;
	entnum = (int)vm->globals[OFS_PARM0];
	if (entnum < 0 || vm->max_edicts <= 0 || entnum >= vm->max_edicts)
	{
		csqc_ret_entity (vm, 0);
		return;
	}
	csqc_ret_entity (vm, entnum);
}

/*
C2.2 — string-buffers #460-469 (DP). Хранилище в csqc_client (deep-copy);
builtins — тонкие обёртки (ABI i*3, возвраты строк через CSQCVM_SetRetStr).
*/
static int CSQCVM_ArgInt (int idx)
{
	pr1vm_t *vm = CSQCVM_Active ();
	return (vm && vm->argc > idx) ? (int)vm->globals[OFS_PARM0 + idx * 3] : 0;
}

static char *CSQCVM_ArgStr (int idx)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm || vm->argc <= idx)
		return NULL;
	return CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + idx * 3]);
}

// strbuf() buf_create = #460
static void csqc_buf_create (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		vm->globals[OFS_RETURN] = CSQC_Client_BufCreate ();
}

// void(strbuf bufhandle) buf_del = #461
static void csqc_buf_del (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufDel (CSQCVM_ArgInt (0));
}

// float(strbuf bufhandle) buf_getsize = #462
static void csqc_buf_getsize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		vm->globals[OFS_RETURN] = CSQC_Client_BufGetSize (CSQCVM_ArgInt (0));
}

// void(strbuf bufhandle_from, strbuf bufhandle_to) buf_copy = #463
static void csqc_buf_copy (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufCopy (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1));
}

// void(strbuf bufhandle, float sortprefixlen, float backward) buf_sort = #464
static void csqc_buf_sort (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufSort (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1), CSQCVM_ArgInt (2) != 0);
}

// string(strbuf bufhandle, string glue) buf_implode = #465
static void csqc_buf_implode (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[8192];
	char *glue;
	if (!vm)
		return;
	glue = CSQCVM_ArgStr (1);
	if (!CSQC_Client_BufImplode (CSQCVM_ArgInt (0), glue ? glue : "", buf, sizeof (buf)))
		buf[0] = 0;
	CSQCVM_SetRetStr (buf);
}

// string(strbuf bufhandle, float string_index) bufstr_get = #466
static void csqc_bufstr_get (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[2048];
	if (!vm)
		return;
	if (!CSQC_Client_BufGet (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1), buf, sizeof (buf)))
		buf[0] = 0;
	CSQCVM_SetRetStr (buf);
}

// void(strbuf bufhandle, float string_index, string str) bufstr_set = #467
static void csqc_bufstr_set (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	if (!vm)
		return;
	s = CSQCVM_ArgStr (2);
	CSQC_Client_BufSet (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1), s ? s : "");
}

// float(strbuf bufhandle, string str, float order) bufstr_add = #468
static void csqc_bufstr_add (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	if (!vm)
		return;
	s = CSQCVM_ArgStr (1);
	vm->globals[OFS_RETURN] = CSQC_Client_BufAdd (CSQCVM_ArgInt (0), s ? s : "",
		CSQCVM_ArgInt (2));
}

// void(strbuf bufhandle, float string_index) bufstr_free = #469
static void csqc_bufstr_free (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CSQC_Client_BufFree (CSQCVM_ArgInt (0), CSQCVM_ArgInt (1));
}

/*
C3.1.
void(string soundname, optional float channel, optional float volume) localsound = #177
FTE PF_cl_localsound (pr_clcmd.c:1059) = S_LocalSound2(name, chan, vol): local-звук.
ezquake: S_LocalSoundWithVol (snd_main.c:1085, precache по имени, канал local −1).
Отклонение: channel игнорируется; vol 0..1 (default 1).
*/
static void csqc_localsound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	char *name;
	float vol;
	if (!vm)
		return;
	g = vm->globals;
	name = CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM0]);
	vol = (vm->argc > 2) ? g[OFS_PARM0 + 6] : 1;
	if (name && name[0])
		S_LocalSoundWithVol (name, vol);
}

/*
float(vector org, float radius, vector lightcolours, optional float style, ...)
dynamiclight_add = #305
ezquake: CL_AllocDlight + поля (lt_custom, color=lightcolours*255, radius, 0.1s).
style/cubemap/pflags — нет аналога (вне скоупа, документировано). Возврат — индекс слота.
*/
static void csqc_dynamiclight_add (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	dlight_t *dl;
	if (!vm)
		return;
	g = vm->globals;
	dl = CL_AllocDlight (0);
	if (!dl)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	VectorCopy (&g[OFS_PARM0], dl->origin);
	dl->radius = g[OFS_PARM0 + 3];
	dl->die = cl.time + 0.1f;
	dl->type = lt_custom;
	dl->bubble = 0;
	dl->color[0] = (byte)bound (0, g[OFS_PARM0 + 6] * 255.0f, 255);
	dl->color[1] = (byte)bound (0, g[OFS_PARM0 + 7] * 255.0f, 255);
	dl->color[2] = (byte)bound (0, g[OFS_PARM0 + 8] * 255.0f, 255);
	vm->globals[OFS_RETURN] = (float)(int)(dl - cl_dlights) + 1;
}

/*
C3.2 — частицы #335-337. В ezquake нет реестра имён эффектов — свой мини-реестр
(имя -> палитровый цвет/базовое кол-во). #335 возвращает handle эффекта (idx+1,
нет -> -1); #336/#337 спавнят R_RunParticleEffect (аппроксимация; FTE-реестр не
портируем — документировано).
*/
typedef struct { const char *name; int color; int count; } csqc_peffect_t;
static const csqc_peffect_t s_peffects[] = {
	{ "blood",		73, 24 },
	{ "explosion",	226, 32 },
	{ "spark",		0,  10 },
	{ "gunshot",	0,  16 },
	{ "smoke",		0,  8 },
};
#define CSQC_NPEFFECTS	((int)(sizeof (s_peffects) / sizeof (s_peffects[0])))

// float(string effectname) particleeffectnum = #335
static void csqc_particleeffectnum (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name;
	int i;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	for (i = 0; name && i < CSQC_NPEFFECTS; i++)
		if (!strcmp (name, s_peffects[i].name))
		{
			vm->globals[OFS_RETURN] = i + 1;
			return;
		}
	vm->globals[OFS_RETURN] = -1;
}

static const csqc_peffect_t *csqc_peffect_byhandle (int h)
{
	return (h >= 1 && h <= CSQC_NPEFFECTS) ? &s_peffects[h - 1] : NULL;
}

// void(float effectnum, entity ent, vector start, vector end) trailparticles = #336
static void csqc_trailparticles (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	const csqc_peffect_t *e;
	vec3_t d;
	float len, step;
	int i, n;
	if (!vm)
		return;
	g = vm->globals;
	e = csqc_peffect_byhandle ((int)g[OFS_PARM0]);
	if (!e)
		return;
	d[0] = g[OFS_PARM0 + 9] - g[OFS_PARM0 + 6];
	d[1] = g[OFS_PARM0 + 10] - g[OFS_PARM0 + 7];
	d[2] = g[OFS_PARM0 + 11] - g[OFS_PARM0 + 8];
	len = sqrt (d[0]*d[0] + d[1]*d[1] + d[2]*d[2]);
	n = (len > 8) ? bound (1, (int)(len / 8.0f), 40) : 1;
	for (i = 0; i <= n; i++)
	{
		vec3_t p;
		step = (n) ? (float)i / n : 0;
		p[0] = g[OFS_PARM0 + 6] + d[0] * step;
		p[1] = g[OFS_PARM0 + 7] + d[1] * step;
		p[2] = g[OFS_PARM0 + 8] + d[2] * step;
		R_RunParticleEffect (p, vec3_origin, e->color, 1);
	}
}

// void(float effectnum, vector origin, optional vector dir, optional float count)
// pointparticles = #337
static void csqc_pointparticles (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	const csqc_peffect_t *e;
	int count;
	vec3_t dir;
	if (!vm)
		return;
	g = vm->globals;
	e = csqc_peffect_byhandle ((int)g[OFS_PARM0]);
	if (!e)
		return;
	count = (vm->argc > 3) ? (int)g[OFS_PARM0 + 9] : e->count;
	dir[0] = (vm->argc > 2) ? g[OFS_PARM0 + 6] : 0;
	dir[1] = (vm->argc > 2) ? g[OFS_PARM0 + 7] : 0;
	dir[2] = (vm->argc > 2) ? g[OFS_PARM0 + 8] : 0;
	R_RunParticleEffect (&g[OFS_PARM0 + 3], dir, e->color, bound (1, count, 4096));
}

/*
C3.3a — te_* аппроксимируемая группа (частицы/взрывы/spikes #405-427, кроме #426).
Аппроксимация: R_RunParticleEffect/R_ParticleExplosion/R_BlobExplosion/CL_ExplosionSprite
(палитровые цвета, bbox/направления приближённо). Не-мапящиеся (#426 и др.) не регистрируются.
*/
static unsigned int s_te_rnd = 1;
static float csqc_te_rand01 (void)
{
	s_te_rnd = s_te_rnd * 1103515245u + 12345u;
	return (float)((s_te_rnd >> 8) & 0xffff) / 65535.0f;
}

static void csqc_te_bbox_effect (float *mn, float *mx, float *vel, int how, int color)
{
	int i;
	for (i = 0; i < how && i < 512; i++)
	{
		vec3_t p;
		p[0] = mn[0] + (mx[0] - mn[0]) * csqc_te_rand01 ();
		p[1] = mn[1] + (mx[1] - mn[1]) * csqc_te_rand01 ();
		p[2] = mn[2] + (mx[2] - mn[2]) * csqc_te_rand01 ();
		R_RunParticleEffect (p, vel, color, 1);
	}
}

// #405 te_blood
static void csqc_te_blood (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	R_RunParticleEffect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], 73,
		bound (1, (int)g[OFS_PARM0 + 6], 4096));
}
// #406 te_bloodshower
static void csqc_te_bloodshower (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	vec3_t vel = { 0, 0, -100 };
	if (!vm)
		return;
	g = vm->globals;
	csqc_te_bbox_effect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], vel,
		bound (1, (int)g[OFS_PARM0 + 7], 4096), 73);
}
// #407 te_explosionrgb
static void csqc_te_explosionrgb (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_ParticleExplosion (&vm->globals[OFS_PARM0]);
}
// #408 te_particlecube
static void csqc_te_particlecube (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	csqc_te_bbox_effect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], &g[OFS_PARM0 + 6],
		bound (1, (int)g[OFS_PARM0 + 9], 4096), (int)g[OFS_PARM0 + 12]);
}
// #409/#410 rain/snow
static void csqc_te_rain (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	csqc_te_bbox_effect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], &g[OFS_PARM0 + 6],
		bound (1, (int)g[OFS_PARM0 + 9], 4096), (int)g[OFS_PARM0 + 12]);
}
// #411 te_spark
static void csqc_te_spark (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	R_RunParticleEffect (&g[OFS_PARM0], &g[OFS_PARM0 + 3], 0,
		bound (1, (int)g[OFS_PARM0 + 6], 4096));
}
// #412-415 quad-эффекты (org в w0) — белые частицы
static void csqc_te_quad (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	R_RunParticleEffect (&vm->globals[OFS_PARM0], vec3_origin, 255, 12);
}
// #416/#417 flash
static void csqc_te_smallflash (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CL_ExplosionSprite (&vm->globals[OFS_PARM0]);
}
static void csqc_te_customflash (void)
{
	csqc_te_smallflash ();
}
// #418 te_gunshot
static void csqc_te_gunshot (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int count;
	if (!vm)
		return;
	g = vm->globals;
	count = (vm->argc > 1) ? (int)g[OFS_PARM0 + 3] : 20;
	R_RunParticleEffect (&g[OFS_PARM0], vec3_origin, 0, bound (1, count, 4096));
}
// #419/420/423/424 spikes — цветные частицы
static void csqc_te_spike_color (int color)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_RunParticleEffect (&vm->globals[OFS_PARM0], vec3_origin, color, 10);
}
static void csqc_te_spike (void) { csqc_te_spike_color (255); }
static void csqc_te_superspike (void) { csqc_te_spike_color (255); }
static void csqc_te_wizspike (void) { csqc_te_spike_color (0); }
static void csqc_te_knightspike (void) { csqc_te_spike_color (0); }
// #421/#427 explosion
static void csqc_te_explosion (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_ParticleExplosion (&vm->globals[OFS_PARM0]);
}
static void csqc_te_explosion2 (void) { csqc_te_explosion (); }
// #422 tarexplosion / #425 lavasplash
static void csqc_te_tarexplosion (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		CL_ExplosionSprite (&vm->globals[OFS_PARM0]);
}
static void csqc_te_lavasplash (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (vm)
		R_BlobExplosion (&vm->globals[OFS_PARM0]);
}

/*
C3.3b — beams #428-431 (te_lightning1/2/3, te_beam): CL_CreateBeam(type, ent, start, end)
(cl_tent.c:439). own-entity -> entnum (сырые int-биты через csqc_ent_of). Аппроксимация.
#431-fix (C6.1): если модель эффекта отсутствует (напр. progs/beam.mdl в стенде) —
Con_Printf-варн и no-op, БЕЗ host error/disconnect (Mod_CustomModel(crash=false);
CL_CreateBeam сам грузит с crash=true и рвёт коннект).
*/
static custom_model_id_t CSQC_BeamModelId (int type)
{
	switch (type)
	{
	case 1: return custom_model_bolt;
	case 2: return custom_model_bolt2;
	case 3: return custom_model_bolt3;
	case 4:
	default: return custom_model_beam;
	}
}

static const char *CSQC_BeamModelName (int type)
{
	switch (type)
	{
	case 1: return "progs/bolt.mdl";
	case 2: return "progs/bolt2.mdl";
	case 3: return "progs/bolt3.mdl";
	case 4:
	default: return "progs/beam.mdl";
	}
}

static void csqc_te_beam_type (int type)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	int entnum;
	if (!vm)
		return;
	g = vm->globals;
	if (!Mod_CustomModel (CSQC_BeamModelId (type), false))
	{
		Con_Printf ("CSQC: te_beam type %d: %s not found - effect skipped\n",
			type, CSQC_BeamModelName (type));
		return;
	}
	entnum = csqc_ent_of (vm, OFS_PARM0);
	CL_CreateBeam (type, entnum, &g[OFS_PARM0 + 3], &g[OFS_PARM0 + 6]);
}
static void csqc_te_lightning1 (void) { csqc_te_beam_type (1); }
static void csqc_te_lightning2 (void) { csqc_te_beam_type (2); }
static void csqc_te_lightning3 (void) { csqc_te_beam_type (3); }
static void csqc_te_beam (void) { csqc_te_beam_type (4); }

static void csqc_getplayerkeyvalue (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int pnum;
	char *key;
	char buf[64];
	player_info_t *pi;
	char *v = NULL;

	if (!vm)
		return;
	pnum = (int)vm->globals[OFS_PARM0];
	key = CSQCVM_Str (OFS_PARM1);
	if (pnum < 0 || pnum >= MAX_CLIENTS || !key || !key[0])
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	pi = &cl.players[pnum];
	if (!pi->name[0])
	{
		CSQCVM_SetRetStr ("");	// пустой слот — игрока нет
		return;
	}
	// B10 (Э8.2): ключи FTE PF_cs_getplayerkey_internal (pr_csqc.c:4344): к уже
	// имевшимся frags/ping/userid/spectator/name + userinfo добавлены pl,
	// activetime, ignored, viewentity, topcolor_rgb/bottomcolor_rgb.
	if (!strcmp (key, "frags"))
		snprintf (buf, sizeof (buf), "%d", pi->frags), v = buf;
	else if (!strcmp (key, "ping"))
		snprintf (buf, sizeof (buf), "%d", pi->ping), v = buf;
	else if (!strcmp (key, "userid"))
		snprintf (buf, sizeof (buf), "%d", pi->userid), v = buf;
	else if (!strcmp (key, "spectator"))
		snprintf (buf, sizeof (buf), "%d", (int)pi->spectator), v = buf;
	else if (!strcmp (key, "pl"))				// packet loss (FTE :4375)
		snprintf (buf, sizeof (buf), "%d", (int)pi->pl), v = buf;
	else if (!strcmp (key, "activetime"))			// FTE :4382 (realtime - entertime)
		snprintf (buf, sizeof (buf), "%f", cls.realtime - pi->entertime), v = buf;
	else if (!strcmp (key, "ignored"))
		snprintf (buf, sizeof (buf), "%d", (int)pi->ignored), v = buf;
	else if (!strcmp (key, "viewentity"))			// FTE :4351 (DP-compat: pnum+1)
		snprintf (buf, sizeof (buf), "%d", pnum + 1), v = buf;
	else if (!strcmp (key, "topcolor_rgb") || !strcmp (key, "bottomcolor_rgb"))
	{
		// FTE :4392-4415 — палитра-цвет (real_*); DP-RGB (col>=16) не поддержан
		// (accept+doc). Формат — "'r g b'" (%g).
		int col = (key[0] == 't') ? (int)pi->real_topcolor : (int)pi->real_bottomcolor;
		if (col < 16)
		{
			int pal = Sbar_ColorForMap (col);
			snprintf (buf, sizeof (buf), "'%g %g %g'",
				host_basepal[pal * 3 + 0] / 255.0,
				host_basepal[pal * 3 + 1] / 255.0,
				host_basepal[pal * 3 + 2] / 255.0);
			v = buf;
		}
	}
	else if (!strcmp (key, "name"))
		v = pi->name;
	else
		v = Info_ValueForKey (pi->userinfo, key);	// team/topcolor/bottomcolor/...
	CSQCVM_SetRetStr (v ? v : "");
}

/*
void(float usecursor, optional string cursorimage, optional vector hotspot,
     optional float scale) setcursormode = #343
FTE (pr_clcmd.c PF_cl_setcursormode): освобождает/хватает мышь и настраивает курсор.
ezquake (A3.1): полная реализация — пока usecursor=1 и модуль активен в игре, мышь
не отдаётся OS-курсору и SCR_DrawCursor рисует курсор модуля (image/hotspot/scale);
при 0 мышь возвращается движку. Клики/InputEvent-канал модуля — C1.
ABI/scale — как FTE: hotspot — вектор (w6..8), масштаб читается из hotspot.z (w8),
отдельный float-арг (w9) игнорируется; scale <= 0 -> нативный размер курсора.
*/
static void csqc_setcursormode (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *g;
	if (!vm)
		return;
	g = vm->globals;
	CSQC_Client_SetCursorMode (g[OFS_PARM0] != 0,
		vm->argc > 1 ? CSQC_Client_GetString (vm, *(int *)&g[OFS_PARM1]) : NULL,
		vm->argc > 2 ? g[OFS_PARM2]     : 0,
		vm->argc > 2 ? g[OFS_PARM2 + 1] : 0,
		vm->argc > 2 ? g[OFS_PARM2 + 2] : 0);
}

/*
vector() getmousepos = #344
Позиция CSQC-курсора в координатах 2D-оверлея ezquake (см. GetCursorPos); z = 0.
FTE (pr_menu.c PF_cl_getmousepos): при абсолютном курсоре — позиция, иначе дельты
со сбросом. Отклонение (roadmap A3.2): всегда позиция (модуль в абсолютном режиме;
дельты/InputEvent-канал — C1).
*/
static void csqc_getmousepos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float x = 0, y = 0;
	if (!vm)
		return;
	CSQC_Client_GetCursorPos (&x, &y);
	vm->globals[OFS_RETURN] = x;
	vm->globals[OFS_RETURN + 1] = y;
	vm->globals[OFS_RETURN + 2] = 0;
}

/*
float(float x, float y) pow = #97 (Phase 1 L1 P1a; клиентский обработчик —
серверная PF_pow статическая; чистая математика)
*/
static void csqc_pow (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = pow (vm->globals[OFS_PARM0], vm->globals[OFS_PARM1]);
}

/*
vector() randomvec = #91 (Phase 1 L1 P1a; клиентский обработчик, как PF_randomvec)
*/
static void csqc_randomvec (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *r;
	if (!vm)
		return;
	r = &vm->globals[OFS_RETURN];
	do {
		r[0] = (rand () & 0x7fff) * (2.0 / 0x7fff) - 1.0;
		r[1] = (rand () & 0x7fff) * (2.0 / 0x7fff) - 1.0;
		r[2] = (rand () & 0x7fff) * (2.0 / 0x7fff) - 1.0;
	} while (DotProduct (r, r) >= 1);
}

/*
L2-тривиалы T1 — математика (2026-09-07; волна тривиал-кандидатов, L2-реестр).
FTE-эталон тел — fteqw/engine/common/pr_bgcmd.c (asin 6486, log 4776, anglemod 6534,
mod 6452, bitshift 6376, crc16 5772, gettimef 7266). Чистые float/string-функции без
edict/движкового состояния (исключения #494 crc16 / #519 gettimef отмечены в телах).
*/

/*
float(float x) asin = #471; float(float x) acos = #472
float(float x) atan = #473; float(float a, float b) atan2 = #474; float(float x) tan = #475
*/
static void csqc_asin (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = asin (vm->globals[OFS_PARM0]);
}
static void csqc_acos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = acos (vm->globals[OFS_PARM0]);
}
static void csqc_atan (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = atan (vm->globals[OFS_PARM0]);
}
static void csqc_atan2 (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = atan2 (vm->globals[OFS_PARM0], vm->globals[OFS_PARM1]);
}
static void csqc_tan (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = tan (vm->globals[OFS_PARM0]);
}

/*
float(float x, optional float base) log = #532
log(x); при 2-м аргументе — log_base(x) = log(x)/log(base) (PF_Logarithm).
*/
static void csqc_log (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	double r;
	if (!vm)
		return;
	r = log (vm->globals[OFS_PARM0]);
	if (vm->argc > 1)
		r /= log (vm->globals[OFS_PARM1]);
	vm->globals[OFS_RETURN] = (float)r;
}

/*
float(float v) anglemod = #102 — в [0,360) (PF_anglemod).
*/
static void csqc_anglemod (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float v;
	if (!vm)
		return;
	v = vm->globals[OFS_PARM0];
	while (v >= 360)
		v -= 360;
	while (v < 0)
		v += 360;
	vm->globals[OFS_RETURN] = v;
}

/*
float(float a, float n) mod = #245 — a - n*(int)(a/n); деление на 0 → warning + 0.
*/
static void csqc_mod (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float a, n;
	if (!vm)
		return;
	a = vm->globals[OFS_PARM0];
	n = vm->globals[OFS_PARM1];
	if (n == 0)
	{
		Con_Printf ("CSQC mod: mod by zero\n");
		vm->globals[OFS_RETURN] = 0;
	}
	else
		vm->globals[OFS_RETURN] = a - n * (float)(int)(a / n);
}

/*
float(float number, float quantity) bitshift = #218
quantity<0 → сдвиг вправо на −quantity, иначе влево (PF_bitshift).
*/
static void csqc_bitshift (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int mask, shift;
	if (!vm)
		return;
	mask = (int)vm->globals[OFS_PARM0];
	shift = (int)vm->globals[OFS_PARM1];
	if (shift < 0)
		mask >>= -shift;
	else
		mask <<= shift;
	vm->globals[OFS_RETURN] = mask;
}

/*
float(float insensitive, string str, ...) crc16 = #494
CRC16 (CCITT, poly 0x1021, init/xor 0xffff/0x0000 — тот же, что ezq CRC_* и FTE
hash_crc16); insensitive → строчные буквы перед подсчётом (FTE hash_crc16_lower).
Строки от 1-го аргумента конкатенируются (FTE PF_VarString). Возврат — значение crc.
*/
static void csqc_crc16 (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	unsigned short crc;
	char buf[4096];
	int i, insens, len = 0;
	const char *s;

	if (!vm)
		return;
	insens = (int)vm->globals[OFS_PARM0];
	buf[0] = 0;
	for (i = 1; i < vm->argc; i++)
	{
		s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (s)
			len += snprintf (buf + len, sizeof (buf) - len, "%s", s);
		if (len >= (int)sizeof (buf) - 1)
			break;
	}
	CRC_Init (&crc);
	for (i = 0; i < len; i++)
		CRC_ProcessByte (&crc, insens ? tolower ((int)(unsigned char)buf[i]) : (unsigned char)buf[i]);
	vm->globals[OFS_RETURN] = CRC_Value (crc);
}

/*
float(optional float timer) gettimef = #519 — время в секундах (float).
FTE PF_gettimed (pr_bgcmd.c:7248): timer 0/нет — realtime (кадр), 1 — wall-clock
с точностью до мс, 5 — sim-time (cl.time); остальное → realtime.
Отклонение (в parity): mode0 = cls.realtime ezquake (масштабируется cl_demospeed).
sys.h не включаем (конфликт dllfunction_t после quakedef) — прототип локальный.
*/
double Sys_DoubleTime (void);
static void csqc_gettimef (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int timer;
	if (!vm)
		return;
	timer = (vm->argc > 0) ? (int)vm->globals[OFS_PARM0] : 0;
	switch (timer)
	{
	case 1:
		vm->globals[OFS_RETURN] = (float)((double)(long long)(Sys_DoubleTime () * 1000.0) / 1000.0);
		break;
	case 5:
		vm->globals[OFS_RETURN] = (float)cl.time;
		break;
	default:
		vm->globals[OFS_RETURN] = (float)cls.realtime;
		break;
	}
}

/*
L2-тривиалы T2 — int/hex конверсии (#259-262, 2026-09-07; волна тривиал-кандидатов).
FTE-эталон — pr_bgcmd.c (itos 4701, stoi 4712, htos 4720, stoh 4731).
ABI: параметры/возврат типа int в классике передаются 4 байтами битового значения
(как строки), а не float-числом — читаем/пишем через *(int *)&globals[...].
*/

/*
string(int input) itos = #260 — "%d".
*/
static void csqc_itos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[32];
	if (!vm)
		return;
	snprintf (buf, sizeof (buf), "%d", *(int *)&vm->globals[OFS_PARM0]);
	CSQCVM_SetRetStr (buf);
}

/*
int(string input) stoi = #259 — atoi (возврат int-битами).
*/
static void csqc_stoi (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	*(int *)&vm->globals[OFS_RETURN] = atoi (s ? s : "");
}

/*
string(int input) htos = #262 — "%08x" (всегда 8 символов, без префикса).
*/
static void csqc_htos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[32];
	if (!vm)
		return;
	snprintf (buf, sizeof (buf), "%08x", *(unsigned int *)&vm->globals[OFS_PARM0]);
	CSQCVM_SetRetStr (buf);
}

/*
int(string input) stoh = #261 — strtoul base 16 (возврат int-битами).
*/
static void csqc_stoh (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	*(int *)&vm->globals[OFS_RETURN] = (int)strtoul (s ? s : "", NULL, 16);
}

/*
L2-тривиалы T3 — cvar-метаданные (#482/#495/#518, 2026-09-07; волна тривиал-кандидатов).
FTE-эталон — pr_bgcmd.c (cvar_defstring 1907, cvar_description 1918, cvar_type 1934).
Отклонения (parity): в ezq `cvar_t` нет description (→ #518 всегда "" и флаг
HASDESCRIPTION не ставится); PRIVATE-аналога FTE (NOTFROMSERVER/NOUNSAFEEXPAND) нет —
не выставляется.
*/

/*
string(string cvarname) cvar_defstring = #482
FTE: FindOrGet (создаёт, если нет), возврат default-значения (нет — "").
ezq: Cvar_Find / Cvar_Create (FindOrGet), возврат cvar_t.defaultvalue.
Отклонение (T3 Э6, doc): для `registercvar(name,value)` FTE отдаёт "" — это quirk
`PF_registercvar` (`pr_bgcmd.c:2001` читает value по гейту `callargc>2`, а 2-арг вызов
даёт value "") → `defaultstr=""`. ezq честно возвращает defaultvalue (value) — намеренно,
т.к. мимикрия quirk сломала бы дефолты модуля (`csqc_vm` и др.).
*/
static void csqc_cvar_defstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	cvar_t *v;
	if (!vm)
		return;
	v = (name && name[0]) ? Cvar_Find (name) : NULL;
	if (!v && name && name[0])
		v = Cvar_Create (name, "", 0);
	CSQCVM_SetRetStr ((v && v->defaultvalue) ? v->defaultvalue : "");
}

/*
float(string cvarname) cvar_type = #495
Флаги FTE (pr_common.h:225): EXISTS=1 SAVED=2 PRIVATE=4 ENGINE=8 HASDESCRIPTION=16
READONLY=32. Маппинг на ezq: SAVED = CVAR_ARCHIVE|CVAR_USER_ARCHIVE; ENGINE = не
CVAR_USER_CREATED/MOD_CREATED; READONLY = CVAR_ROM. cvar не обязан существовать.
*/
static void csqc_cvar_type (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	cvar_t *v;
	int ret = 0;
	if (!vm)
		return;
	v = (name && name[0]) ? Cvar_Find (name) : NULL;
	if (v)
	{
		ret |= 1;	// EXISTS
		if (v->flags & (CVAR_ARCHIVE | CVAR_USER_ARCHIVE))
			ret |= 2;	// SAVED
		if (v->flags & CVAR_ROM)
			ret |= 32;	// READONLY
		if (!(v->flags & (CVAR_USER_CREATED | CVAR_MOD_CREATED)))
			ret |= 8;	// ENGINE
	}
	vm->globals[OFS_RETURN] = ret;
}

/*
string(string cvarname) cvar_description = #518
FTE возвращает описание cvar; в ezq у cvar_t описаний нет — всегда "".
*/
static void csqc_cvar_description (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr ("");
}

/*
L2-тривиалы T4 — строки простые (2026-09-07; волна тривиал-кандидатов).
FTE-эталон — pr_bgcmd.c (strpad 4363, strncasecmp 4272/strncmp 4305, infoadd/infoget
4337/4348, strreplace 4968/strireplace 4997, chr2str 4560/str2chr 4579, strtolower
5066/strtoupper 5077). ASCII-семантика (UTF-8-ветки FTE — вне классики, отклонение
в parity). Строки возврата — temp-ring (SetRetStr).
*/

/*
float(string str, optional float index) str2chr = #222 — код символа; index<0 —
с конца; вне [0,len) → 0.
*/
static void csqc_str2chr (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	int len, idx;
	if (!vm)
		return;
	s = s ? s : "";
	len = strlen (s);
	idx = (vm->argc > 1) ? (int)vm->globals[OFS_PARM1] : 0;
	if (idx < 0)
		idx = len + idx;
	if (idx < 0 || idx >= len)
		vm->globals[OFS_RETURN] = 0;
	else
		vm->globals[OFS_RETURN] = (float)(unsigned char)s[idx];
}

/*
string(float chr, ...) chr2str = #223 — строка из кодов символов (каждый аргумент).
*/
static void csqc_chr2str (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[128];
	int i, n = 0;
	if (!vm)
		return;
	for (i = 0; i < vm->argc && i < 64 && n < (int)sizeof (buf) - 1; i++)
		buf[n++] = (char)(int)vm->globals[OFS_PARM0 + i * 3];
	buf[n] = 0;
	CSQCVM_SetRetStr (buf);
}

/*
string(float pad, string str1, ...) strpad = #225 — выравнивание конкатенации
строк к ширине |pad|: pad>0 — справа, pad<0 — слева (PF_strpad).
*/
static void csqc_strpad (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[4096], *d;
	int pad, len = 0, i;
	const char *s;

	if (!vm)
		return;
	pad = (int)vm->globals[OFS_PARM0];
	buf[0] = 0;
	d = buf;
	for (i = 1; i < vm->argc; i++)
	{
		s = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (!s)
			continue;
		len = strlen (s);
		if (d - buf + len >= (int)sizeof (buf) - 1)
			len = (int)sizeof (buf) - 1 - (d - buf);
		memcpy (d, s, len);
		d += len;
		*d = 0;
	}
	if (pad < 0)
	{
		pad = -pad - (int)(d - buf);
		if (pad > (int)sizeof (buf) - 1 - (d - buf))
			pad = (int)sizeof (buf) - 1 - (d - buf);
		if (pad > 0)
		{
			memmove (buf + pad, buf, (size_t)(d - buf) + 1);
			memset (buf, ' ', pad);
		}
	}
	else
	{
		pad -= (int)(d - buf);
		if (pad < 0)
			pad = 0;
		if (d - buf + pad >= (int)sizeof (buf) - 1)
			pad = (int)sizeof (buf) - 1 - (d - buf);
		memset (d, ' ', pad);
		d[pad] = 0;
	}
	CSQCVM_SetRetStr (buf);
}

/*
string(infostring old, string key, string value) infoadd = #226
string(infostring info, string key) infoget = #227
QW-infostring \key\value; FTE Info_* (pr_bgcmd.c). Прототипы Info_SetValueForStarKey
в common.h нет — локальный extern.
*/
void Info_SetValueForStarKey (char *s, char *key, char *value, int maxsize);
static void csqc_infoadd (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *info, *key, *val;
	char buf[1024];
	if (!vm)
		return;
	info = CSQCVM_Str (OFS_PARM0);
	key = CSQCVM_Str (OFS_PARM1);
	val = CSQCVM_VarString (2);
	strlcpy (buf, info ? info : "", sizeof (buf));
	Info_SetValueForStarKey (buf, key ? key : "", val ? val : "", sizeof (buf));
	CSQCVM_SetRetStr (buf);
}
static void csqc_infoget (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *info = CSQCVM_Str (OFS_PARM0);
	char *key = CSQCVM_Str (OFS_PARM1);
	if (!vm)
		return;
	CSQCVM_SetRetStr (Info_ValueForKey (info ? info : "", key ? key : ""));
}

/*
float(string s1, string s2, optional float len, optional float s1ofs, optional float s2ofs)
strcmp/strncmp = #228 (PF_strncmp, pr_bgcmd.c:4305)
*/
static void csqc_strncmp (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *a, *b;
	int len, aofs, bofs;
	int alen, blen;
	if (!vm)
		return;
	a = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	b = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	if (vm->argc <= 2)
	{
		vm->globals[OFS_RETURN] = strcmp (a, b);
		return;
	}
	len = (int)vm->globals[OFS_PARM2];
	aofs = (vm->argc > 3) ? (int)vm->globals[OFS_PARM3] : 0;
	bofs = (vm->argc > 4) ? (int)vm->globals[OFS_PARM4] : 0;
	alen = strlen (a);
	blen = strlen (b);
	if (aofs < 0 || (aofs && aofs > alen))
		aofs = alen;
	if (bofs < 0 || (bofs && bofs > blen))
		bofs = blen;
	vm->globals[OFS_RETURN] = strncmp (a + aofs, b, len);
}

/*
float(string s1, string s2) strcasecmp = #229
float(string s1, string s2, float len, optional float s1ofs, optional float s2ofs)
strncasecmp = #230 (PF_strncasecmp, pr_bgcmd.c:4272)
*/
static void csqc_strncasecmp (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *a, *b;
	int len, aofs, bofs;
	int alen, blen;
	if (!vm)
		return;
	a = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	b = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	if (vm->argc <= 2)
	{
		vm->globals[OFS_RETURN] = strcasecmp (a, b);
		return;
	}
	len = (int)vm->globals[OFS_PARM2];
	aofs = (vm->argc > 3) ? (int)vm->globals[OFS_PARM3] : 0;
	bofs = (vm->argc > 4) ? (int)vm->globals[OFS_PARM4] : 0;
	alen = strlen (a);
	blen = strlen (b);
	if (aofs < 0 || (aofs && aofs > alen))
		aofs = alen;
	if (bofs < 0 || (bofs && bofs > blen))
		bofs = blen;
	vm->globals[OFS_RETURN] = strncasecmp (a + aofs, b + bofs, len);
}

/*
string(string s) strtolower = #480 / strtoupper = #481 — ASCII (FTE — unicode).
*/
static void csqc_strtolower (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s = CSQCVM_Str (OFS_PARM0);
	char buf[8192];
	int i, n;
	if (!vm)
		return;
	n = strlen (s ? s : "");
	if (n >= (int)sizeof (buf))
		n = (int)sizeof (buf) - 1;
	for (i = 0; i < n; i++)
		buf[i] = tolower ((int)(unsigned char)s[i]);
	buf[n] = 0;
	CSQCVM_SetRetStr (buf);
}
static void csqc_strtoupper (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s = CSQCVM_Str (OFS_PARM0);
	char buf[8192];
	int i, n;
	if (!vm)
		return;
	n = strlen (s ? s : "");
	if (n >= (int)sizeof (buf))
		n = (int)sizeof (buf) - 1;
	for (i = 0; i < n; i++)
		buf[i] = toupper ((int)(unsigned char)s[i]);
	buf[n] = 0;
	CSQCVM_SetRetStr (buf);
}

/*
Стрипинг цветовой разметки для #476 strlennocol / #477 strdecolorize (T3 Э3, T4 `^`).
FTE-паритет: `&cRGB` (валидный 3-hex) / `&r` (логика r_draw_charset.c:331-365) и
colour/state-коды `^`: q3-цвета `^0-9`, `^xRRGGBB`, `^&XX` (extended FG/BG),
состояния `^b/^d/^m/^a/^h/^s/^r`, escape `^^`, плюс FTE-поведение неизвестного/
висячего `^` (ft eqw/engine/common/common.c:4169-4478, flags=0/keepmarkup=false).
Вне scope: links `^[..^]`, charset `u8:`/`k8:`, `^Uxxxx`/`^{xxxx}` — отклонение,
backlog docs/plans/ezquake_csqc_client_strcolor_markup.md. Возврат — длина
результата (байты, как FTE COM_DeFunString); out==NULL допустим (только подсчёт).
*/
static int CSQCVM_IsExtCode (char c)
{
	return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || c == '-';
}

static int CSQCVM_StripColor (const char *in, char *out, size_t outsize)
{
	size_t n = 0;

	if (!in)
		in = "";
	for (; *in; in++)
	{
		if (in[0] == '&' && in[1] == 'c'
			&& HexToInt (in[2]) >= 0 && HexToInt (in[3]) >= 0 && HexToInt (in[4]) >= 0)
		{
			in += 4;
			continue;
		}
		if (in[0] == '&' && in[1] == 'r')
		{
			in += 1;
			continue;
		}
		if (in[0] == '^')
		{
			char c1 = in[1];

			if (c1 >= '0' && c1 <= '9')			// ^0..^9 q3 colour
			{
				in += 1;
				continue;
			}
			if (c1 == 'x')						// ^xRGB valid -> strip 5; invalid -> strip "^x"
			{
				if (HexToInt (in[2]) >= 0 && HexToInt (in[3]) >= 0 && HexToInt (in[4]) >= 0)
					in += 4;
				else
					in += 1;
				continue;
			}
			if (c1 == '&')						// ^&XX extended FG/BG
			{
				if (CSQCVM_IsExtCode (in[2]) && CSQCVM_IsExtCode (in[3]))
				{
					in += 3;
					continue;
				}
				// invalid: '^' остаётся литералом, '&' обрабатывается на след. итерации
			}
			else if (c1 == 'b' || c1 == 'd' || c1 == 'm' || c1 == 'a'
				|| c1 == 'h' || c1 == 's' || c1 == 'r')
			{
				in += 1;
				continue;
			}
			else if (c1 == '^')					// ^^ -> ^
			{
				if (out && outsize && n + 1 < outsize)
					out[n] = '^';
				n++;
				in += 1;
				continue;
			}
			// unknown / end / out-of-scope: '^' литерал, следующий символ — как обычно
		}
		if (out && outsize && n + 1 < outsize)
			out[n] = *in;
		n++;
	}
	if (out && outsize)
		out[(n < outsize) ? n : outsize - 1] = 0;
	return (int)n;
}

/*
float(string s) strlennocol = #476 — FTE-паритет (PF_strlennocol, pr_bgcmd.c:5038):
длина строки без цветовых кодов.
*/
static void csqc_strlennocol (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (float)CSQCVM_StripColor (CSQCVM_Str (OFS_PARM0), NULL, 0);
}

/*
string(string s) strdecolorize = #477 — FTE-паритет (PF_strdecolorize, pr_bgcmd.c:5054):
строка с вырезанными цветовыми кодами.
*/
static void csqc_strdecolorize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[8192];

	if (!vm)
		return;
	CSQCVM_StripColor (CSQCVM_Str (OFS_PARM0), buf, sizeof (buf));
	CSQCVM_SetRetStr (buf);
}

/*
string(string input, string token) instr = #206 — FTE-паритет (PF_instr, pr_bgcmd.c:4945):
первое вхождение variadic-хвоста (с парма 1) в input; возврат — подстрока-остаток с позиции
вхождения, либо "" если не найдено.
*/
static void csqc_instr (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s1, *s2, *sub;

	if (!vm)
		return;
	s1 = CSQCVM_Str (OFS_PARM0);
	s2 = CSQCVM_VarString (1);
	if (!s1 || !s2)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	sub = strstr (s1, s2);
	CSQCVM_SetRetStr (sub ? (char *)sub : "");
}

/*
string(string search, string replace, string subject) strreplace = #484
string(string search, string replace, string subject) strireplace = #485
(PF_strreplace/strireplace: 4096-буфер, нерекурсивная замена).
*/
static void csqc_strreplace (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *search, *replace, *sub;
	char buf[4096], *result = buf;
	int searchlen, replacelen;
	if (!vm)
		return;
	search = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	replace = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	sub = CSQCVM_Str (OFS_PARM2) ? CSQCVM_Str (OFS_PARM2) : "";
	searchlen = strlen (search);
	replacelen = strlen (replace);
	if (searchlen)
	{
		while (*sub && result < buf + sizeof (buf) - replacelen - 2)
		{
			if (!strncmp (sub, search, searchlen))
			{
				sub += searchlen;
				memcpy (result, replace, replacelen);
				result += replacelen;
			}
			else
				*result++ = *sub++;
		}
		*result = 0;
	}
	else
		strlcpy (buf, sub, sizeof (buf));
	CSQCVM_SetRetStr (buf);
}
static void csqc_strireplace (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *search, *replace, *sub;
	char buf[4096], *result = buf;
	int searchlen, replacelen;
	if (!vm)
		return;
	search = CSQCVM_Str (OFS_PARM0) ? CSQCVM_Str (OFS_PARM0) : "";
	replace = CSQCVM_Str (OFS_PARM1) ? CSQCVM_Str (OFS_PARM1) : "";
	sub = CSQCVM_Str (OFS_PARM2) ? CSQCVM_Str (OFS_PARM2) : "";
	searchlen = strlen (search);
	replacelen = strlen (replace);
	if (searchlen)
	{
		while (*sub && result < buf + sizeof (buf) - replacelen - 2)
		{
			if (!strncasecmp (sub, search, searchlen))
			{
				sub += searchlen;
				memcpy (result, replace, replacelen);
				result += replacelen;
			}
			else
				*result++ = *sub++;
		}
		*result = 0;
	}
	else
		strlcpy (buf, sub, sizeof (buf));
	CSQCVM_SetRetStr (buf);
}

/*
Phase 1 L1 P1c — cvar/exec/ошибки. Client-handlers (строки через CSQC_Client_GetString,
без серверных зеркал). #28 coredump / #31 eprint — entity-отладка, уходят в P1d.
*/

/*
void(string err, ...) error = #10
Не-серверный вариант: печатаем и поднимаем host_error активной VM (клиентский
колбэк ставит errored — кадры отключаются). Отклонение от серверного PF_error:
без дампа self/edict и SV_Error.
*/
/*
void(string errortext) error = #10 — FTE-паритет (PF_error, pr_bgcmd.c:7196):
developer!=0 — нефатально (печать; FTE — debug-break, у нас печать и continue);
developer==0 — фатально (abort через host_error). Отклонение: FTE-стек не печатаем.
*/
static void csqc_error (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_VarString (0);
	if (!vm)
		return;
	Con_Printf ("CSQC error: %s\n", s ? s : "");
	if (!developer.value && vm->host_error)
		vm->host_error (vm, s ? s : "error");
}

/*
void(string err, ...) objerror = #11
Паритет FTE (PF_objerror, pr_csqc.c): фатальность зависит от cvar developer.
developer!=0 — нефатальна: печать в консоль, модуль продолжает (debug_trace
в FTE не воспроизводим). developer==0 — фатальна: печать + дисконнект клиента
(CSQC_Client_Abort: как FTE CSQC_Abort → Host_EndGame).
Отклонение от FTE: без дампа self/edict (ED_Print) перед сообщением.
*/
static void csqc_objerror (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_VarString (0);
	if (!vm)
		return;
	Con_Printf ("CSQC objerror: %s\n", s ? s : "");
	if (!developer.value)
		CSQC_Client_Abort (s ? s : "objerror");
}

/*
void(string str) localcmd = #46
Выполнение строки как команды движка — так же, как команды с сервера (svc_stufftext):
через cbuf_svc с фильтром cl_remote_capabilities (cmd.c Cmd_ExecuteStringEx), а не в
неограниченный cbuf_main. Отклонение от FTE: FTE использует RESTRICT_INSECURE
(exec-level), ezq — allowlist по имени (ADR 0019, раздел про localcmd).
*/
static void csqc_localcmd (void)
{
	char *s = CSQCVM_VarString (0);
	if (s && s[0])
		Cbuf_AddTextEx (&cbuf_svc, s);
}

/*
void(string cvarname, string value) cvar_set = #72
Как серверный PF_cvar_set (pr_cmds.c): если cvar нет — предупреждение.
*/
/*
void(string cvarname, string value) cvar_set = #72 — FTE-паритет (PF_cvar_set,
pr_bgcmd.c:1957): FTE использует FindOrGet — отсутствующий cvar создаётся.
Отклонение: CVAR_NOTFROMSERVER-guard не воспроизводим (клиент).
*/
static void csqc_cvar_set (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name, *val;
	cvar_t *var;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	val = CSQCVM_Str (OFS_PARM1);
	if (!name || !name[0])
		return;
	var = Cvar_Find (name);
	if (!var)
		var = Cvar_Create (name, "", 0);	// FindOrGet: создаём, если нет
	if (var)
		Cvar_Set (var, val ? val : "");
}

/*
float(string name, string value) registercvar = #93
Создание переменной движка (namespace общий), если ещё нет; возврат 1/0.
*/
static void csqc_registercvar (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name, *value;
	if (!vm)
		return;
	name = CSQCVM_Str (OFS_PARM0);
	value = CSQCVM_Str (OFS_PARM1);
	if (!name || !name[0])
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	if (Cvar_Find (name))
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	Cvar_Create (name, value ? value : "", 0);
	vm->globals[OFS_RETURN] = 1;
}

/*
float(string ext) checkextension = #99
FTE-паритет: PF_checkextension (pr_csqc.c:4579) ищет имя в QSG_Extensions (pr_bgcmd.c:8227)
и отдаёт extensioncheck() при наличии, иначе — все ли builtins расширения поддержаны.
Здесь — статическая таблица-зеркало подмножества, реализованного в ezq (Q1: no-op не
рекламируем; исключение Q5 — эффект-таблицы с заглушками te_teleport/te_lightningblood/
te_bloodqw). Имена — точные написания FTE, включая ведущий '_' у _DP_TE_*. EXT_CSQC —
спец-кейс (протокол, не builtins): 1 при активной CSQC-сессии (cls.fteprotocolextensions).
Осознанно НЕ рекламируем (FTE рекламирует, ezq — no-op): FRIK_FILE, FTE_QC_INTCONV,
_DP_TE_FLAMEJET/_DP_TE_PLASMABURN, DP_QC_STRINGBUFFERS (R6), DP_QC_FS_SEARCH(_PACKFILE),
DP_QC_GETSURFACE, DP_QC_FINDCHAIN(FLOAT)/FINDFLAGS/FINDCHAINFLAGS, DP_QC_COPYENTITY,
DP_QC_WHICHPACK, DP_QC_URI_ESCAPE, KRIMZON_SV_PARSECLIENTCOMMAND.
*/
static void csqc_checkextension (void)
{
	static const char *supported[] = {
		"DP_QC_SINCOSSQRTPOW",
		"DP_QC_MINMAXBOUND",
		"DP_QC_RANDOMVEC",
		"DP_REGISTERCVAR",
		"DP_QC_CVAR_STRING",
		"DP_QC_CVAR_DEFSTRING",
		"DP_QC_CVAR_TYPE",
		"DP_QC_EDICT_NUM",
		"DP_QC_ETOS",
		"DP_QC_FINDFLOAT",
		"DP_QC_STRFTIME",
		"DP_QC_STRREPLACE",
		"DP_QC_TOKENIZEBYSEPARATOR",
		"DP_QC_SPRINTF",
		"DP_QC_STRING_CASE_FUNCTIONS",
		"DP_QC_STRINGCOLORFUNCTIONS",
		"DP_QC_CRC16",
		"DP_QC_ASINACOSATANATAN2TAN",
		"DP_QC_CHANGEPITCH",
		"DP_QC_VECTORVECTORS",
		"DP_QC_TRACEBOX",
		"DP_QC_ENTITYDATA",
		"EXT_BITSHIFT",
		"DP_TE_BLOOD",
		"_DP_TE_BLOODSHOWER",
		"DP_TE_EXPLOSIONRGB",
		"DP_TE_PARTICLECUBE",
		"DP_TE_PARTICLERAIN",
		"DP_TE_PARTICLESNOW",
		"DP_TE_SPARK",
		"DP_TE_SMALLFLASH",
		"DP_TE_CUSTOMFLASH",
		"_DP_TE_QUADEFFECTS1",
		"FTE_STRINGS",
		"DP_TE_STANDARDEFFECTBUILTINS",
		"FTE_TE_STANDARDEFFECTBUILTINS",
		NULL
	};
	pr1vm_t *vm = CSQCVM_Active ();
	char *ext;
	int i;
	if (!vm)
		return;
	ext = CSQCVM_Str (OFS_PARM0);
	vm->globals[OFS_RETURN] = 0;
	if (!ext)
		return;
	if (!strcmp (ext, "EXT_CSQC"))		// FTE pr_bgcmd.c:8368 (check_pext_csqc)
	{
		vm->globals[OFS_RETURN] = (cls.fteprotocolextensions & FTE_PEXT_CSQC) ? 1 : 0;
		return;
	}
	for (i = 0; supported[i]; i++)
		if (!strcmp (supported[i], ext))
		{
			vm->globals[OFS_RETURN] = 1;
			return;
		}
}

/*
void() calltimeofday = #231
Как серверный PF_calltimeofday: если в модуле есть функция "timeofday" —
заполнить её аргументы (sec/min/hour/day/mon/year) локальным временем и вызвать.
*/
static void csqc_calltimeofday (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	dfunction_t *f;
	time_t t;
	struct tm *ltm;
	if (!vm)
		return;
	f = PR1VM_FindFunction (vm, "timeofday");
	if (!f)
		return;
	t = time (NULL);
	ltm = localtime (&t);
	if (!ltm)
		return;
	vm->globals[OFS_PARM0] = (float)ltm->tm_sec;
	vm->globals[OFS_PARM1] = (float)ltm->tm_min;
	vm->globals[OFS_PARM2] = (float)ltm->tm_hour;
	vm->globals[OFS_PARM3] = (float)ltm->tm_mday;
	vm->globals[OFS_PARM4] = (float)(ltm->tm_mon + 1);
	vm->globals[OFS_PARM5] = (float)(ltm->tm_year + 1900);
	PR1VM_ExecuteProgram (vm, (func_t)(f - vm->functions));
}

/*
Phase 1 L1 P1b — строки/конверсии. Client-handlers на per-instance строки
(PR1VM_Get/SetString). #118/#119: без GC — strzone = deep-copy в per-instance
кольцо (PR1VM_ClientSetString), strunzone = no-op (документированное отклонение).
*/

/*
string(vector v) vtos = #27 — FTE-паритет (PF_vtos pr_bgcmd.c:4768): "'%f %f %f'".
*/
static void csqc_vtos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char buf[64];
	if (!vm)
		return;
	snprintf (buf, sizeof (buf), "'%f %f %f'",
		vm->globals[OFS_PARM0], vm->globals[OFS_PARM0 + 1], vm->globals[OFS_PARM0 + 2]);
	CSQCVM_SetRetStr (buf);
}

/*
float(string s) stof = #81
*/
static void csqc_stof (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (float)strtod (s ? s : "", NULL);
}

/*
float(string s) strlen = #114
*/
static void csqc_strlen (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = s ? (float)strlen (s) : 0;
}

/*
string(string s, float start, float count) substring = #116
(логика серверного PF_substr, per-instance строки)
*/
/*
string(string s, float start, float count) substring = #116 — FTE-паритет
(PF_substring pr_bgcmd.c:4886): отрицательные start/length от конца, строгий
clamp (start>=slen || length<=0 → "").
*/
static void csqc_substring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	char buf[2048];
	int start, len, l;
	if (!vm)
		return;
	if (!s)
		s = "";
	start = (int)vm->globals[OFS_PARM1];
	len = (int)vm->globals[OFS_PARM2];
	l = strlen (s);
	if (start < 0)
		start = l + start;
	if (len < 0)
		len = l - start + (len + 1);
	if (start < 0)
		start = 0;
	if (start >= l || len <= 0 || l == 0)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	l -= start;
	if (len > l)
		len = l;
	strlcpy (buf, s + start, (size_t)len + 1);
	CSQCVM_SetRetStr (buf);
}

/*
vector(string s) stov = #117 — FTE pr_bgcmd.c:4740 (PF_VarString(0), парс из vtos-формата;
`'`-stop). Прототип в csdefs/FTE не variadic, поэтому extra-args из модуля недостижимы.
*/
static void csqc_stov (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *p = CSQCVM_VarString (0);
	double v[3];
	int i;
	char *end;
	if (!vm)
		return;
	if (!p)
		p = "";
	for (i = 0; i < 3; i++)
	{
		while (*p == ' ' || *p == '\t')
			p++;
		if (*p == '\'' || *p == '"')
			p++;
		v[i] = strtod (p, &end);
		if (end == p)
		{
			v[i] = 0;
			while (*p && *p != ' ')
				p++;
		}
		else
			p = end;
	}
	vm->globals[OFS_RETURN] = (float)v[0];
	vm->globals[OFS_RETURN + 1] = (float)v[1];
	vm->globals[OFS_RETURN + 2] = (float)v[2];
}

/*
string(string s) strzone = #118
Отклонение (нет GC на клиенте): deep-copy в per-instance кольцо (PR1VM_ClientSetString).
*/
static void csqc_strzone (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	CSQCVM_SetRetStr (s ? s : "");
}

/*
void(string s) strunzone = #119
Отклонение: no-op (нет GC/персистентного пула на клиенте).
*/
static void csqc_strunzone (void)
{
	/* no-op (ADR 0017 D7 / тема B: классическое кольцо без GC) */
}

/*
string(string varname) cvar_string = #448
*/
static void csqc_cvar_string (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	cvar_t *var;
	if (!vm)
		return;
	if (!name)
		name = "";
	// FTE pr_bgcmd.c:1892: отдаётся latched_string, если значение защёлкнуто.
	// PF_Cvar_FindOrGet (autocreate) и флаг CVAR_NOUNSAFEEXPAND — FTE-специфичны
	// (в ezq нет); см. parity-audit §D.2.
	var = Cvar_Find (name);
	CSQCVM_SetRetStr (var ? (var->latchedString ? var->latchedString : var->string) : "");
}

/*
Phase 1 L1 P1e — клиентские подсистемы. Best-effort на клиентские API ezquake;
отклонения от FTE документируются в parity-audit.
*/

/*
void() breakpoint = #6
Debugger: no-op на клиенте (движок не имеет QC-отладчика).
*/
static void csqc_breakpoint (void)
{
	/* no-op (документировано) */
}

/*
void(entity e, float chan, string samp, float vol, float atten) sound = #8
Отклонение: позиционный звук у entity не делаем (нет origin-поля без арены);
прекеш + локальное проигрывание как #177 (объём vol).
*/
static void csqc_sound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM2);
	float vol = 1;
	if (!vm)
		return;
	if (vm->argc > 3)
		vol = vm->globals[OFS_PARM0 + 9];
	if (n && n[0])
	{
		S_PrecacheSound (n);
		if (vol > 0)
			S_LocalSoundWithVol (n, vol);
	}
}

/*
void(string str) precache_sound = #19/#76 — FTE parity (PF_cs_PrecacheSound,
pr_csqc.c:3268): local precache + queue a missing sound for download
(PF_cs_PrecacheSound -> Sound_CheckDownload, cl_parse.c:1573).
void — OFS_RETURN is not written (module does not rely on the return value).
*/
static void csqc_precache_sound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM0);
	if (vm && n && n[0])
	{
		S_PrecacheSound (n);
		// FTE parity: queue the missing sound (`sound/<name>`). Caller-side guard on
		// `cls.download`: ezq download is single-slot (cl_parse.c:482), so a second request
		// would clobber the in-flight one. `*` = sexed sound (not downloadable).
		if (n[0] != '*' && !cls.download)
			CL_CheckOrDownloadFile (va ("sound/%s", n));
	}
}

static void csqc_precache_model (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM0);
	if (vm && n && n[0])
	{
		// CSQC_Client_ModelIndex returns 0 when the file is missing (Mod_ForName failed).
		// FTE parity (PF_cs_PrecacheModel_Internal, pr_csqc.c:3218): queue the model for
		// download. Same single-slot `cls.download` guard as precache_sound.
		if (!CSQC_Client_ModelIndex (n) && n[0] != '*' && !cls.download)
			CL_CheckOrDownloadFile (n);
	}
	CSQCVM_SetRetStr (n ? n : "");
}

/*
float(string modelname, optional float queryonly) getmodelindex = #200 (Ф3).
Индекс модели в CSQC-реестре (name→Mod_ForName); FTE PF_getmodelindex. queryonly!=0 —
только поиск уже зарегистрированной (без загрузки); иначе — зарегистрировать.
Отклонение: единый реестр поверх Mod_ForName (у FTE — отдельное пространство индексов).
*/
static void csqc_getmodelindex (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n;
	int queryonly;
	if (!vm)
		return;
	n = CSQCVM_Str (OFS_PARM0);
	queryonly = (vm->argc > 1) ? (int)vm->globals[OFS_PARM1] : 0;
	vm->globals[OFS_RETURN] = (float)(queryonly
		? CSQC_Client_ModelIndexKnown (n) : CSQC_Client_ModelIndex (n));
}

/*
string(float mdlindex) modelnameforindex = #334 (T3 Э3).
FTE-паритет (PF_cs_ModelnameForIndex, pr_csqc.c:3277): обратный резолв индекса.
Отклонение: у ezq единый положительный CSQC-реестр (getmodelindex возвращает
именно его индекс, комментарий csqc_client.c), у FTE — csqc-слоты < 0 и
server-precache >= 0. Поэтому порядок: CSQC-реестр → server cl.model_name[idx]
(сетевые/серверные индексы); idx<0 → "".
*/
static void csqc_modelnameforindex (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *name;
	int idx;

	if (!vm)
		return;
	idx = (int)vm->globals[OFS_PARM0];
	name = CSQC_Client_ModelNameForIndex (idx);
	if (!name && idx >= 0 && idx < MAX_MODELS)
		name = cl.model_name[idx];
	CSQCVM_SetRetStr ((char *)(name ? name : ""));
}

/*
float(string) precache_file (#68/#77) — FTE-паритет (PF_cs_precachefile →
CL_CheckOrEnqueDownloadFile): true=файл есть → 1; false=поставлен на скачивание → 0.
У нас — CL_CheckOrDownloadFile (cl_parse.c:482, тот же контракт: true если есть/не
качается, иначе шлёт download и false). Guard: при уже идущем скачивании (cls.download)
второй не стартуем → 0 (отклонение). Ограничения CL_Download_Accept — см. parity.
*/
static void csqc_precache_file (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n;

	if (!vm)
		return;
	n = CSQCVM_Str (OFS_PARM0);
	if (!n || !n[0])
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	if (cls.download)	// уже качается другой ресурс — не перебиваем
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	vm->globals[OFS_RETURN] = CL_CheckOrDownloadFile (n) ? 1.0f : 0.0f;
}

/*
void(vector pos, string samp, float vol, float atten) ambientsound = #74
Отклонение: без позиционного 3D — прекеш + локальное проигрывание (vol).
*/
static void csqc_ambientsound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *n = CSQCVM_Str (OFS_PARM1);
	float vol = 1;
	if (!vm)
		return;
	if (vm->argc > 2)
		vol = vm->globals[OFS_PARM0 + 6];
	if (n && n[0])
	{
		S_PrecacheSound (n);
		if (vol > 0)
			S_LocalSoundWithVol (n, vol);
	}
}

/*
void(vector pos, vector dir, float colour, float count) particle = #48
*/
static void csqc_particle (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	R_RunParticleEffect (&vm->globals[OFS_PARM0],
		&vm->globals[OFS_PARM0 + 3],
		(int)vm->globals[OFS_PARM0 + 6],
		(int)vm->globals[OFS_PARM0 + 9]);
}

/*
void(float lightstyle, string stylestring, optional vector rgb) lightstyle = #35
Отклонение: клиент не стилизует свет — no-op (документировано).
*/
static void csqc_lightstyle (void)
{
	/* no-op (документировано) */
}

/*
void(float pause) setpause = #531
Отклонение: на клиенте нет серверной паузы — no-op (документировано).
*/
static void csqc_setpause (void)
{
	/* no-op (документировано) */
}

/*
Phase 1 L1 P1d C1 — базовые entity на арене ADR 0017 (модульный резерв C0-A).
entity-значение PR1 = entnum*edict_size (int-биты). Типы полей (eprint): код 1 =
ev_string, 2 = ev_float, 3 = ev_vector, 4 = ev_entity (pr_comp.h etype_t).
*/

static int csqc_ent_of (pr1vm_t *vm, int parmofs)
{
	int v;
	if (!vm || vm->edict_size <= 0)
		return -1;
	v = *(int *)&vm->globals[parmofs];
	return v / vm->edict_size;
}

static float *csqc_ent_slot (pr1vm_t *vm, int entnum)
{
	if (!vm || !vm->game_edicts)
		return NULL;
	if (entnum < 0 || entnum >= vm->num_edicts)
		return NULL;
	return (float *)((byte *)vm->game_edicts + (size_t)entnum * vm->edict_size);
}

static float *csqc_ent_field (pr1vm_t *vm, int entnum, const char *name)
{
	int ofs;
	float *slot;
	if ((ofs = CSQC_Client_FindField (vm, name)) < 0)
		return NULL;
	slot = csqc_ent_slot (vm, entnum);
	return slot ? &slot[ofs] : NULL;
}

// C2 (Wave C): доступ к полю по кэш-офсету (CSQC_Client_FieldOfs) — без скана
// fielddefs на каждый вызов (горячий путь addentities).
static float *csqc_ent_ofs (pr1vm_t *vm, int entnum, int fldofs)
{
	float *slot;
	if (fldofs < 0)
		return NULL;
	slot = csqc_ent_slot (vm, entnum);
	return slot ? &slot[fldofs] : NULL;
}

static void csqc_ret_entity (pr1vm_t *vm, int entnum)
{
	*(int *)&vm->globals[OFS_RETURN] = entnum * vm->edict_size;
}

/*
Ф3 (takeover): arena-эдикт -> ezq entity_t -> cl_visents (#301 arena / #302).
FTE CopyCSQCEdictToEntity берёт .modelindex; у нас modelindex-библиотека ещё
заглушки (#200/#333), поэтому модель берём по `.model`-строке через Mod_ForName
(отклонение, отдельный шаг). C4 Этап 1: .predraw вызывается до чтения полей
(FTE pr_csqc.c:1450-1457), .renderflags мапится в ent.renderfx (подмножество CSQCRF_*).
.scale не применяется — вынесено отдельно (docs/plans/ezquake_csqc_client_scale.md).
*/
static void csqc_add_one_entity (int e)
{
	pr1vm_t *vm = CSQCVM_Active ();
	entity_t ent;
	float *slot, *f;
	char *mname;
	model_t *model;
	int ofs;
	int playernum = -1;	// Stage 4b: network player index (colormap 1..MAX_CLIENTS)

	if (!vm || e <= 0 || !CSQC_Client_EntUsed (e))
		return;

	// C4 Этап 1: .predraw (FTE pr_csqc.c:1450-1457). Возврат != PREDRAW_AUTOADD(0)
	// или удаление эдикта -> не добавлять. RF_NOAUTOADD в FTE удалён (pr_common.h:881)
	// в пользу возврата predraw — не проверяем. Function-значение поля — сырые int-биты
	// (EV_FUNCTION): читаем как int, а не через float (иначе denormal -> 0).
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_PREDRAW))) && *(int *)&f[0] > 0)
	{
		qbool removed = false;
		float pret = CSQC_Client_CallPredraw (e, *(int *)&f[0], &removed);
		if (removed || pret != 0)
			return;
	}

	slot = csqc_ent_slot (vm, e);
	if (!slot)
		return;
	// model: .modelindex (Ф3, FTE-паритет) с fallback на .model-строку
	model = NULL;
	if ((ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELINDEX)) >= 0)
	{
		int mi = (int)slot[ofs];
		if (mi > 0)
			model = CSQC_Client_ModelForIndex (mi);
	}
	if (!model)
	{
		if ((ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODEL)) < 0)
			return;
		mname = CSQC_Client_GetString (vm, (string_t)*(int *)&slot[ofs]);
		if (!mname || !mname[0])
			return;
		model = Mod_ForName (mname, false);
	}
	if (!model)
		return;

	memset (&ent, 0, sizeof (ent));
	ent.model = model;
	ent.colormap = vid.colormap;
	// Stage 4: .colormap as player index (1..MAX_CLIENTS) -> team translation
	// table + scoreboard (player skin), same as the engine player render
	// (cl_ents.c:1261-1262, 2223-2224; cl_nqdemo.c:1029-1030). The modhint guard
	// mirrors cl_ents.c:1258-1259: heads/gibs (h_player) stay unskinned.
	// FTE CopyCSQCEdictToEntity maps the index to playerindex/topcolour
	// (pr_csqc.c:861-879); values > MAX_CLIENTS (DP colormap) fall back to
	// vid.colormap / NULL scoreboard (documented deviation).
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_COLORMAP))))
	{
		int cm = (int)f[0];
		if (cm > 0 && cm <= MAX_CLIENTS && model->modhint == MOD_PLAYER)
		{
			ent.colormap = cl.players[cm - 1].translations;
			ent.scoreboard = &cl.players[cm - 1];
			playernum = cm - 1;
		}
	}
	ent.oldframe = ent.frame;
	ent.framelerp = -1;
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN))))		VectorCopy (f, ent.origin);
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ANGLES))))		VectorCopy (f, ent.angles);
	// Stage 4b: player render pitch = -viewangles/3, same as the engine player
	// render (cl_ents.c:2240) and the FTE CSQC bridge (pr_csqc.c:5626,
	// r_meshpitch=-1). Render-side only: does not touch the module's .angles
	// (used by #347/prediction). Roll is left 0 (FTE CSQC bridge also 0).
	if (playernum >= 0)
		ent.angles[PITCH] = -ent.angles[PITCH] / 3;
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_FRAME))))		ent.frame = ent.oldframe = (int)f[0];
	// Stage 4b: player frame interpolation — exact engine formula (CL_LinkPlayers,
	// cl_ents.c:2228-2237). Without it CSQC renders the raw frame while the engine
	// lerps, so the pose jumps when toggling csqc_delta. FTE has no jump because
	// engine and CSQC share cl.lerpplayers (pr_csqc.c:5617).
	if (playernum >= 0)
	{
		centity_t *cent = &cl_entities[playernum + 1];
		if (cent->frametime >= 0 && cent->frametime <= cl.time)
		{
			ent.oldframe = cent->oldframe;
			ent.framelerp = (cl.time - cent->frametime) * 10;
		}
		else
		{
			ent.oldframe = ent.frame;
			ent.framelerp = -1;
		}
	}
	// Stage 4b (engine-only): cl_deadbodyFilter for CSQC-owned players. The engine
	// player filter (CL_LinkPlayers, cl_ents.c:2188-2208) is skipped for owned
	// players, so apply the same semantics here. TF exception for mode 3
	// (!cl.teamfortress) mirrors the engine.
	if (playernum >= 0 && model->modhint == MOD_PLAYER)
	{
		int i = ent.frame;
		if (cl_deadbodyfilter.value == 3 && !cl.teamfortress)
		{
			if (ISDEAD(i))
				return;
		}
		if (cl_deadbodyfilter.value == 2)
		{
			if (ISDEAD(i))
				return;
		}
		else if (cl_deadbodyfilter.value == 1)
		{
			if (i == 49 || i == 60 || i == 69 || i == 84 || i == 93 || i == 102)
				return;
		}
	}
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SKIN))))		ent.skinnum = (int)f[0];
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_EFFECTS))))	ent.effects = (int)f[0];
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ALPHA))))		ent.alpha = f[0];
	// .scale: uniform render scale (FTE pr_csqc.c:841-844, 0 remapped to 1).
	// 0 stays 0 here — the render helper/culling treat 0 as unscaled.
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SCALE))))		ent.scale = f[0];
	// C4 Этап 1: .renderflags (CSQCRF_*) -> ent.renderfx (RF_*). Маппим доступное
	// подмножество (FTE pr_csqc.c:773-799); DEPTHHACK/EXTERNALMODEL/FIRSTPERSON/USEAXIS
	// без прямого ezq-аналога — отклонение (parity-audit).
	if ((f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_RENDERFLAGS))))
	{
		int rflags = (int)f[0];
		if (rflags & 1)		// CSQCRF_VIEWMODEL
			ent.renderfx |= RF_WEAPONMODEL;
		if (rflags & 32)	// CSQCRF_NOSHADOW
			ent.renderfx |= RF_NOSHADOW;
		if (rflags & 8)		// CSQCRF_ADDITIVE
			ent.renderfx |= RF_ADDITIVEBLEND;
	}

	// Stage 3 (docs/adr/0028): PVS/leaf + bbox culling, FTE EdictInFatPVS parity.
	// R_CSQC_BeginCull marks the leaves/frustum for this frame (once per frame).
	R_CSQC_BeginCull ();
	if (!R_CSQC_EntityVisible (&ent))
		return;

	CL_AddEntity (&ent);
}

/* void(entity ent) addentity = #302 (Ф3 takeover) */
static void csqc_addentity (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm || !CSQC_Client_SceneActive ())
		return;
	csqc_add_one_entity (csqc_ent_of (vm, OFS_PARM0));
}

/* entity() spawn = #14 */
static void csqc_spawn (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	csqc_ret_entity (vm, CSQC_Client_EntAlloc (vm));
}

/* void(entity e) remove = #15 (вне резерва — игнор, ADR 0017) */
static void csqc_remove (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_EntFree (vm, csqc_ent_of (vm, OFS_PARM0));
}

/* void(entity e, vector org) setorigin = #2 */
static void csqc_setorigin (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *f, *o;
	int e;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
	if (!f)
		return;
	o = &vm->globals[OFS_PARM0 + 3];
	f[0] = o[0]; f[1] = o[1]; f[2] = o[2];
}

/*
B20 (FTE-паритет): перенос bbox модели в поля арена-эдикта.
FTE csqc_setmodel копирует model->mins/maxs и size (pr_csqc.c:3129-3131);
PF_cs_SetSize пишет .size = maxs-mins (pr_csqc.c:2911). model == NULL ->
обнулить (FTE-ветка «model NULL», pr_csqc.c:3146-3150).
*/
static void csqc_model_bbox (pr1vm_t *vm, int e, model_t *model)
{
	float *fmn, *fmx, *fsz;

	if (!vm || e <= 0)
		return;
	fmn = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MINS));
	fmx = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MAXS));
	fsz = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SIZE));
	if (model)
	{
		if (fmn) { fmn[0] = model->mins[0]; fmn[1] = model->mins[1]; fmn[2] = model->mins[2]; }
		if (fmx) { fmx[0] = model->maxs[0]; fmx[1] = model->maxs[1]; fmx[2] = model->maxs[2]; }
		if (fsz)
		{
			fsz[0] = model->maxs[0] - model->mins[0];
			fsz[1] = model->maxs[1] - model->mins[1];
			fsz[2] = model->maxs[2] - model->mins[2];
		}
	}
	else
	{
		if (fmn) { fmn[0] = fmn[1] = fmn[2] = 0; }
		if (fmx) { fmx[0] = fmx[1] = fmx[2] = 0; }
		if (fsz) { fsz[0] = fsz[1] = fsz[2] = 0; }
	}
}

/* void(entity e, string m) setmodel = #3 */
static void csqc_setmodel (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s;
	int e, ofs, idx;
	float *slot;
	model_t *model;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	s = CSQCVM_Str (OFS_PARM0 + 3);
	slot = csqc_ent_slot (vm, e);
	if (!slot || !s)
		return;
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODEL);
	if (ofs >= 0)
		PR1VM_ClientSetString (vm, (string_t *)&slot[ofs], s);
	// Ф3: .modelindex из CSQC-реестра (рендер arena-эдиктов по индексу, FTE-паритет)
	idx = CSQC_Client_ModelIndex (s);
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELINDEX);
	if (ofs >= 0)
		slot[ofs] = (float)idx;
	// B20: bbox из модели + .modelflags (FTE pr_csqc.c:3096-3155, PF_cs_SetModel :3178).
	model = idx ? CSQC_Client_ModelForIndex (idx) : NULL;
	csqc_model_bbox (vm, e, model);
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELFLAGS);
	if (ofs >= 0 && model)
		slot[ofs] = (float)model->flags;
}

/* void(entity e, float mdlindex) setmodelindex = #333 (Ф3) */
static void csqc_setmodelindex (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *f, *slot;
	int e, idx, ofs;
	const char *name;
	model_t *model;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	idx = (int)vm->globals[OFS_PARM0 + 3];
	f = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MODELINDEX));
	if (f)
		f[0] = (float)idx;
	// B20 (FTE PF_cs_SetModelIndex, pr_csqc.c:3180-3186 -> csqc_setmodel): резолв
	// CSQC-реестра -> .model + bbox. Нерезолвнутый/неположительный индекс -> .model не
	// трогаем (паритет FTE early-return). Единое positive-пространство — ADR 0024.
	model = (idx > 0) ? CSQC_Client_ModelForIndex (idx) : NULL;
	if (!model)
		return;
	name = CSQC_Client_ModelNameForIndex (idx);
	slot = csqc_ent_slot (vm, e);
	ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_MODEL);
	if (ofs >= 0 && slot && name)
		PR1VM_ClientSetString (vm, (string_t *)&slot[ofs], (char *)name);
	csqc_model_bbox (vm, e, model);
}

/* void(entity e, vector min, vector max) setsize = #4 */
static void csqc_setsize (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *fmin, *fmax, *fsz, *mn, *mx;
	int e;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	fmin = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MINS));
	fmax = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_MAXS));
	if (!fmin || !fmax)
		return;
	mn = &vm->globals[OFS_PARM0 + 3];
	mx = &vm->globals[OFS_PARM0 + 6];
	fmin[0] = mn[0]; fmin[1] = mn[1]; fmin[2] = mn[2];
	fmax[0] = mx[0]; fmax[1] = mx[1]; fmax[2] = mx[2];
	// B20: .size = maxs - mins (FTE PF_cs_SetSize, pr_csqc.c:2911)
	fsz = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SIZE));
	if (fsz)
	{
		fsz[0] = mx[0] - mn[0];
		fsz[1] = mx[1] - mn[1];
		fsz[2] = mx[2] - mn[2];
	}
}

/* entity(entity e) nextent = #47 — модульный резерв (сетевые не «used») */
static void csqc_nextent (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	if (e < 0)
		e = 0;
	for (e++; e < vm->num_edicts; e++)
		if (CSQC_Client_EntUsed (e))
		{
			csqc_ret_entity (vm, e);
			return;
		}
	csqc_ret_entity (vm, 0);
}

/* entity(entity start, .string fld, string match) find = #18 (резерв; string-поля) */
static void csqc_find (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, f;
	char *s, *t;
	float *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = *(int *)&vm->globals[OFS_PARM0 + 3];
	s = CSQCVM_Str (OFS_PARM0 + 6);
	if (f < 0 || f >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_Find: bad field reference");
		return;
	}
	if (e < 0)
		e = 0;
	if (s)
		for (e++; e < vm->num_edicts; e++)
		{
			if (!CSQC_Client_EntUsed (e))
				continue;
			slot = csqc_ent_slot (vm, e);
			if (!slot)
				continue;
			t = CSQC_Client_GetString (vm, *(int *)&slot[f]);
			if (t && !strcmp (t, s))
			{
				csqc_ret_entity (vm, e);
				return;
			}
		}
	csqc_ret_entity (vm, 0);
}

/* SOLID/FL/MOVE-константы (паритет csdefs.qc). Вынесены выше csqc_findradius,
   который использует solid/FL_FINDABLE_NONSOLID (FTE pr_bgcmd.c:4040). */
#define CSQC_SOLID_NOT		0
#define CSQC_SOLID_TRIGGER	1
#define CSQC_SOLID_BSP		4
#define CSQC_FL_MONSTER		32
#define CSQC_FL_FINDABLE_NONSOLID	16384
#define CSQC_FL_ONGROUND	512
#define CSQC_MOVE_NOMONSTERS	1
#define CSQC_MOVE_MISSILE	2
#define CSQC_MOVE_HITMODEL	4
#define CSQC_MOVE_TRIGGERS	16
#define CSQC_MOVE_EVERYTHING	32
#define CSQC_MOVE_LAGGED	64

/* entity(vector org, float rad) findradius = #22 — резерв; chain если поле есть */
static void csqc_findradius (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *org, *o, *fld, rad, d;
	float *chslot;
	int e, chain_ofs, prev;
	if (!vm)
		return;
	org = &vm->globals[OFS_PARM0];
	rad = vm->globals[OFS_PARM0 + 3];
	chain_ofs = CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	prev = 0;	// голова цепочки; world(0) — терминатор (FTE pr_bgcmd.c:4012/4058)
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		o = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
		if (!o)
			continue;
		// FTE pr_bgcmd.c:4040 — не-solid пропускается, если нет FL_FINDABLE_NONSOLID.
		fld = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_SOLID));
		if (fld && (int)*fld == CSQC_SOLID_NOT)
		{
			fld = csqc_ent_ofs (vm, e, CSQC_Client_FieldOfs (vm, CSQC_FLD_FLAGS));
			if (!fld || !((int)*fld & CSQC_FL_FINDABLE_NONSOLID))
				continue;
		}
		d = (o[0]-org[0])*(o[0]-org[0]) + (o[1]-org[1])*(o[1]-org[1]) + (o[2]-org[2])*(o[2]-org[2]);
		if (d > rad * rad)
			continue;
		// FTE: ent.v.chain = chain; chain = ent -> возврат последнего (головы),
		// прочие совпадения доступны обходом `.chain` (FTE pr_bgcmd.c:4058/4088/4093).
		if (chain_ofs >= 0)
		{
			chslot = csqc_ent_slot (vm, e);
			if (chslot)
				*(int *)&chslot[chain_ofs] = prev * vm->edict_size;
		}
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/*
L2 — «Entity-поиск/копия» (#400/#402/#403/#449/#450). FTE oracle — pr_bgcmd.c:
PF_copyentity (:4213), PF_findchain (:1569), PF_findchainfloat (:1531),
PF_FindFlags (:1611), PF_findchainflags (:1493). Arena fields are float-word
offsets (ddef_t.ofs), entity value PR1 = slot*edict_size (ADR 0017). The chain
link writes the previous entity value into the chainfield (terminator world(0)),
same as csqc_findradius (#22).
*/

/* void(entity from, entity to) copyentity = #400 */
static void csqc_copyentity (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int from, to;
	float *src, *dst;
	if (!vm)
		return;
	from = csqc_ent_of (vm, OFS_PARM0);
	if (vm->argc <= 1)
		to = CSQC_Client_EntAlloc (vm);
	else
		to = csqc_ent_of (vm, OFS_PARM0 + 3);
	// FTE pr_bgcmd.c:4228-4231 — free source/dest is fatal (readonly/fieldsize
	// have no ezq-arena counterpart: no readonly, single edict_size).
	if (from <= 0 || !CSQC_Client_EntUsed (from))
	{
		CSQC_Client_Abort ("PF_copyentity: source is free");
		return;
	}
	if (to <= 0 || !CSQC_Client_EntUsed (to))
	{
		CSQC_Client_Abort ("PF_copyentity: destination is free");
		return;
	}
	src = csqc_ent_slot (vm, from);
	dst = csqc_ent_slot (vm, to);
	if (!src || !dst)
		return;
	memcpy (dst, src, vm->edict_size);
	// FTE pr_bgcmd.c:4235 World_LinkEdict — accept+doc: no client linking
	// (per-frame culling, ADR 0028). FTE returns dest; csdefs declares void.
	csqc_ret_entity (vm, to);
}

/* entity(.string field, string match, .entity chainfield) findchain = #402 */
static void csqc_findchain (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int f, cf, e, prev;
	char *s, *t;
	float *slot;
	if (!vm)
		return;
	f = *(int *)&vm->globals[OFS_PARM0];
	s = CSQCVM_Str (OFS_PARM0 + 3);
	cf = (vm->argc > 2) ? *(int *)&vm->globals[OFS_PARM0 + 6]
		: CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	if (f < 0 || f >= vm->progs->entityfields || cf < 0 || cf >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindChain: bad field reference");
		return;
	}
	prev = 0;
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		t = CSQC_Client_GetString (vm, *(int *)&slot[f]);
		if (!t)
			continue;
		if (strcmp (t, s ? s : ""))
			continue;
		*(int *)&slot[cf] = prev * vm->edict_size;
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/* entity(.float fld, float match, .entity chainfield) findchainfloat = #403 */
static void csqc_findchainfloat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int f, cf, e, prev;
	float match, *slot;
	if (!vm)
		return;
	f = *(int *)&vm->globals[OFS_PARM0];
	match = vm->globals[OFS_PARM0 + 3];
	cf = (vm->argc > 2) ? *(int *)&vm->globals[OFS_PARM0 + 6]
		: CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	if (f < 0 || f >= vm->progs->entityfields || cf < 0 || cf >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindChain: bad field reference");
		return;
	}
	prev = 0;
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		// FTE pr_bgcmd.c:1531 — float equality (not int bits, unlike findfloat #98).
		if (slot[f] != match)
			continue;
		*(int *)&slot[cf] = prev * vm->edict_size;
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/* entity(entity start, .float fld, float match) findflags = #449 */
static void csqc_findflags (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, f;
	float match, *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = *(int *)&vm->globals[OFS_PARM0 + 3];
	match = vm->globals[OFS_PARM0 + 6];
	if (f < 0 || f >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindFlags: bad field reference");
		return;
	}
	if (e < 0)
		e = 0;
	for (e++; e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		if (*(int *)&slot[f] & *(int *)&match)
		{
			csqc_ret_entity (vm, e);
			return;
		}
	}
	csqc_ret_entity (vm, 0);
}

/* entity(.float fld, float match, .entity chainfield) findchainflags = #450 */
static void csqc_findchainflags (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int f, cf, e, prev;
	float match, *slot;
	if (!vm)
		return;
	f = *(int *)&vm->globals[OFS_PARM0];
	match = vm->globals[OFS_PARM0 + 3];
	cf = (vm->argc > 2) ? *(int *)&vm->globals[OFS_PARM0 + 6]
		: CSQC_Client_FieldOfs (vm, CSQC_FLD_CHAIN);
	if (f < 0 || f >= vm->progs->entityfields || cf < 0 || cf >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindChain: bad field reference");
		return;
	}
	prev = 0;
	for (e = CSQC_Client_EntSpawnBase (); e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		if (!(*(int *)&slot[f] & *(int *)&match))
			continue;
		*(int *)&slot[cf] = prev * vm->edict_size;
		prev = e;
	}
	csqc_ret_entity (vm, prev);
}

/* void() changeyaw = #49 — no-op (отклонение; без серверной физики) */
static void csqc_changeyaw (void)
{
	/* no-op (документировано) */
}

/* void(entity e) makestatic = #69 — no-op (отклонение; клиент статик-энтов не ведёт) */
static void csqc_makestatic (void)
{
	/* no-op (документировано) */
}

/* string(entity e, string key) infokey = #80 — serverinfo (клиент без per-ent userinfo) */
static void csqc_infokey (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *key = CSQCVM_Str (OFS_PARM0 + 3);
	if (!vm)
		return;
	CSQCVM_SetRetStr (Info_ValueForKey (cl.serverinfo, key ? key : ""));
}

/* float(entity e) checkbottom = #40 — 0 (отклонение: нет серверного пола) */
static void csqc_checkbottom (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 0;
}

/* void(entity e) eprint = #31 — печать полей слота в консоль (по fielddefs) */
static void csqc_eprint (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, i, ofs;
	float *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	slot = csqc_ent_slot (vm, e);
	if (!slot)
	{
		Con_Printf ("eprint: bad entity %d\n", e);
		return;
	}
	Con_Printf ("eprint entity %d\n", e);
	for (i = 0; i < vm->progs->numfielddefs; i++)
	{
		char *fn = CSQC_Client_GetString (vm, vm->fielddefs[i].s_name);
		ofs = vm->fielddefs[i].ofs;
		if (!fn)
			continue;
		switch (vm->fielddefs[i].type)
		{
		case 1:	/* ev_string */
			Con_Printf ("  .%s = \"%s\"\n", fn,
				CSQC_Client_GetString (vm, *(int *)&slot[ofs]) ? CSQC_Client_GetString (vm, *(int *)&slot[ofs]) : "");
			break;
		case 2:	/* ev_float */
			Con_Printf ("  .%s = %g\n", fn, slot[ofs]);
			break;
		case 3:	/* ev_vector */
			Con_Printf ("  .%s = '%g %g %g'\n", fn, slot[ofs], slot[ofs + 1], slot[ofs + 2]);
			break;
		case 4:	/* ev_entity */
			Con_Printf ("  .%s = ent %d\n", fn, (int)slot[ofs]);
			break;
		}
	}
}

/* void() coredump = #28 — шапка модуля + занятость резерва */
static void csqc_coredump (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	Con_Printf ("coredump (CSQC): funcs %d globals %d fields %d pool_used %d\n",
		vm->progs->numfunctions, vm->progs->numglobals, vm->progs->numfielddefs,
		CSQC_Client_EntUsedCount ());
}

/*
Phase 1 L1 P1d C2 — мировые трассы/физика. С Шага 7 (часть 2) traceline/tracebox
учитывают entity-слой пула (dispatch MOVE_* + forent/owner-ignore + зеркало игроков,
см. csqc_trace_ents ниже); walkmove/droptofloor остаются мир-only (серверная семантика:
движение к полу/шаг не блокируется сущностями). Отклонения: AABB вместо hull,
HITMODEL→bbox, TRIGGERS без brush-мира, LAGGED нет, plane ent-попадания не пишется,
движение — своя трасса без полного PM_PlayerMove.
*/

static trace_t csqc_trace_fallback (vec3_t end)
{
	trace_t tr;
	memset (&tr, 0, sizeof (tr));
	tr.fraction = 1;
	VectorCopy (end, tr.endpos);
	return tr;
}

static trace_t csqc_world_trace (vec3_t start, vec3_t mins, vec3_t maxs, vec3_t end)
{
	cmodel_t *clip;
	hull_t *hull;
	trace_t tr;
	vec3_t offset, sl, el;
	qbool box = (mins != NULL && maxs != NULL);

	clip = cl.clipmodels[1];	// мировая clip-модель (hull'ы BSP)
	if (!clip)
		return csqc_trace_fallback (end);

	if (!box || (mins[0] == 0 && mins[1] == 0 && mins[2] == 0 &&
		maxs[0] == 0 && maxs[1] == 0 && maxs[2] == 0))
	{
		hull = &clip->hulls[0];
		return CM_HullTrace (hull, start, end);
	}

	/* box: hull[1] (player-clip) с offset по переданным mins/maxs */
	hull = &clip->hulls[1];
	VectorSubtract (hull->clip_mins, mins, offset);
	VectorSubtract (start, offset, sl);
	VectorSubtract (end, offset, el);
	tr = CM_HullTrace (hull, sl, el);
	VectorAdd (tr.endpos, offset, tr.endpos);
	return tr;
}

static void csqc_store_trace (pr1vm_t *vm, trace_t *tr)
{
	int o;
	// C2 (Wave C): офсеты из кэша (резолв при загрузке), не скан globaldefs.
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_FRACTION)) >= 0)
		vm->globals[o] = tr->fraction;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_ALLSOLID)) >= 0)
		vm->globals[o] = tr->allsolid;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_STARTSOLID)) >= 0)
		vm->globals[o] = tr->startsolid;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_INOPEN)) >= 0)
		vm->globals[o] = tr->inopen;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_INWATER)) >= 0)
		vm->globals[o] = tr->inwater;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_PLANE_DIST)) >= 0)
		vm->globals[o] = tr->plane.dist;
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_ENDPOS)) >= 0)
	{
		vm->globals[o] = tr->endpos[0];
		vm->globals[o + 1] = tr->endpos[1];
		vm->globals[o + 2] = tr->endpos[2];
	}
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_PLANE_NORMAL)) >= 0)
	{
		vm->globals[o] = tr->plane.normal[0];
		vm->globals[o + 1] = tr->plane.normal[1];
		vm->globals[o + 2] = tr->plane.normal[2];
	}
	if ((o = CSQC_Client_TraceGlobal (vm, CSQC_TRACEG_ENT)) >= 0)
	{
		// entity-значение = slot*edict_size (int-биты); 0 — world.
		*(int *)&vm->globals[o] = (tr->e.entnum > 0) ? tr->e.entnum * vm->edict_size : 0;
	}
}

/* Отрезок против AABB (slab); возвращает t в [0,1], false — нет пересечения. */
static qbool csqc_ray_aabb (vec3_t start, vec3_t dir, vec3_t bmin, vec3_t bmax, float *tout)
{
	float tmin = 0, tmax = 1;
	int i;
	for (i = 0; i < 3; i++)
	{
		float d = dir[i];
		float t1, t2, tmp;
		if (d > -1e-8 && d < 1e-8)
		{
			if (start[i] < bmin[i] || start[i] > bmax[i])
				return false;
			continue;
		}
		t1 = (bmin[i] - start[i]) / d;
		t2 = (bmax[i] - start[i]) / d;
		if (t1 > t2) { tmp = t1; t1 = t2; t2 = tmp; }
		if (t1 > tmin) tmin = t1;
		if (t2 < tmax) tmax = t2;
		if (tmin > tmax)
			return false;
	}
	*tout = tmin;
	return (tmin >= 0 && tmin <= 1);
}

/*
FTE-пул Шаг 7 (часть 2): dispatch MOVE_* + forent/owner-ignore над сущностями пула.
После мир-трассы проверить сущности (origin/mins/maxs) и, если ближе — перекрыть.
AABB-приближение (без hull/movetype-семантики; .solid/.flags/.owner — как в csdefs.qc).
moveflags — 3-й арг traceline / 5-й tracebox (маска MOVE_* FTE); forent — slot сущности,
которую и её владельца трасса не бьёт. boxmin/boxmax != NULL — tracebox: AABB сущности
расширяется на бокс (swept-приближение). Константы SOLID/FL/MOVE — определены выше
(перед csqc_findradius; паритет csdefs.qc).
*/

static void csqc_trace_ents (pr1vm_t *vm, vec3_t start, vec3_t end,
	int moveflags, int forent, vec3_t boxmin, vec3_t boxmax, trace_t *tr)
{
	vec3_t dir, bmin, bmax;
	float t;
	int e, i;
	int ofs_o, ofs_mn, ofs_mx, ofs_sol, ofs_fl, ofs_own;
	qbool nomon, everything, triggers, missile;

	if (!vm || !vm->game_edicts || tr->fraction <= 0 || vm->edict_size <= 0)
		return;
	nomon = !!(moveflags & CSQC_MOVE_NOMONSTERS);
	everything = !!(moveflags & CSQC_MOVE_EVERYTHING);
	triggers = !!(moveflags & CSQC_MOVE_TRIGGERS);
	missile = !!(moveflags & CSQC_MOVE_MISSILE);
	// NOMONSTERS не выходит сразу: FTE оставляет SOLID_BSP (см. фильтр ниже).
	if (forent < 0 || forent >= vm->num_edicts)
		forent = 0;	// world — forent-проверок нет

	ofs_o = CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN);
	ofs_mn = CSQC_Client_FieldOfs (vm, CSQC_FLD_MINS);
	ofs_mx = CSQC_Client_FieldOfs (vm, CSQC_FLD_MAXS);
	ofs_sol = CSQC_Client_FieldOfs (vm, CSQC_FLD_SOLID);
	ofs_fl = CSQC_Client_FieldOfs (vm, CSQC_FLD_FLAGS);
	ofs_own = CSQC_Client_FieldOfs (vm, CSQC_FLD_OWNER);
	if (ofs_o < 0 || ofs_mn < 0 || ofs_mx < 0)
		return;	// модуль без геометрии полей — entity-слой недоступен

	for (i = 0; i < 3; i++)
		dir[i] = end[i] - start[i];

	for (e = 1; e < vm->num_edicts; e++)
	{
		float *base, *org, *mn, *mx;
		int solf, flf;
		float inflate = 0;

		if (!CSQC_Client_EntUsed (e))
			continue;
		base = (float *)((byte *)vm->game_edicts + (size_t)e * vm->edict_size);
		org = base + ofs_o;
		mn = base + ofs_mn;
		mx = base + ofs_mx;

		// forent/owner-ignore (FTE csdefs.qc:523-525): не бьёт forent, его .owner и
		// любую сущность, чей .owner == forent.
		if (e == forent)
			continue;
		if (ofs_own >= 0)
		{
			int own;
			own = (int)base[ofs_own];
			if (own != 0 && own / vm->edict_size == forent)
				continue;	// ent, чей owner == forent
		}
		if (forent > 0 && ofs_own >= 0)
		{
			float *fbase = (float *)((byte *)vm->game_edicts + (size_t)forent * vm->edict_size);
			int fo = (int)fbase[ofs_own];
			if (fo != 0 && fo / vm->edict_size == e)
				continue;	// forent.owner == ent
		}

		solf = (ofs_sol >= 0) ? (int)base[ofs_sol] : CSQC_SOLID_NOT;
		flf = (ofs_fl >= 0) ? (int)base[ofs_fl] : 0;

		// FTE-фильтр (world.c:1937 World_ClipToLinks / :1528 World_ClipToEverything):
		// SOLID_NOT — всегда мимо; триггер бьётся только при MOVE_TRIGGERS/
		// MOVE_EVERYTHING И FL_FINDABLE_NONSOLID (world.c:1956-1959/:1541).
		if (solf == CSQC_SOLID_NOT)
			continue;
		if (solf == CSQC_SOLID_TRIGGER)
		{
			if (!(flf & CSQC_FL_FINDABLE_NONSOLID))
				continue;
			if (!everything && !triggers)
				continue;
		}
		// MOVE_NOMONSTERS оставляет SOLID_BSP (world.c:1970/:1547).
		if (nomon && solf != CSQC_SOLID_BSP)
			continue;
		// MISSILE: монстры с увеличенным размером (±15, как FTE).
		if (missile && (flf & CSQC_FL_MONSTER))
			inflate = 15;

		for (i = 0; i < 3; i++)
		{
			bmin[i] = org[i] + mn[i] - inflate + (boxmin ? boxmin[i] : 0);
			bmax[i] = org[i] + mx[i] + inflate + (boxmax ? boxmax[i] : 0);
		}
		if (!csqc_ray_aabb (start, dir, bmin, bmax, &t))
			continue;
		if (t > tr->fraction)
			continue;
		tr->fraction = t;
		for (i = 0; i < 3; i++)
			tr->endpos[i] = start[i] + dir[i] * t;
		tr->e.entnum = e;
		tr->allsolid = false;
		tr->startsolid = false;
	}
}

/* void(vector v1, vector v2, float flags, entity forent) traceline = #16 */
static void csqc_traceline (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	trace_t tr;
	int moveflags, forent;
	if (!vm)
		return;
	// ABI PR1: каждый параметр — 3-словный блок (param_index*3). traceline:
	// v1@0 v2@3 flags@6 forent@9.
	moveflags = (int)vm->globals[OFS_PARM0 + 6];
	forent = csqc_ent_of (vm, OFS_PARM0 + 9);
	tr = csqc_world_trace (&vm->globals[OFS_PARM0], NULL, NULL,
		&vm->globals[OFS_PARM0 + 3]);
	csqc_trace_ents (vm, &vm->globals[OFS_PARM0], &vm->globals[OFS_PARM0 + 3],
		moveflags, forent, NULL, NULL, &tr);
	csqc_store_trace (vm, &tr);
}

/* void(vector v1, vector mins, vector maxs, vector v2, float flags, entity forent) tracebox = #90 */
static void csqc_tracebox (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	trace_t tr;
	int moveflags, forent;
	if (!vm)
		return;
	// ABI PR1: каждый параметр — 3-словный блок (param_index*3). tracebox:
	// v1@0 mins@3 maxs@6 v2@9 flags@12 forent@15.
	moveflags = (int)vm->globals[OFS_PARM0 + 12];
	forent = csqc_ent_of (vm, OFS_PARM0 + 15);
	tr = csqc_world_trace (&vm->globals[OFS_PARM0],
		&vm->globals[OFS_PARM0 + 3], &vm->globals[OFS_PARM0 + 6],
		&vm->globals[OFS_PARM0 + 9]);
	csqc_trace_ents (vm, &vm->globals[OFS_PARM0], &vm->globals[OFS_PARM0 + 9],
		moveflags, forent, &vm->globals[OFS_PARM0 + 3], &vm->globals[OFS_PARM0 + 6], &tr);
	csqc_store_trace (vm, &tr);
}

/* float(vector org) pointcontents = #41 */
static void csqc_pointcontents (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	hull_t *hull;
	if (!vm)
		return;
	if (!cl.clipmodels[1])
	{
		vm->globals[OFS_RETURN] = CONTENTS_EMPTY;
		return;
	}
	hull = &cl.clipmodels[1]->hulls[0];
	vm->globals[OFS_RETURN] = CM_HullPointContents (hull, hull->firstclipnode,
		&vm->globals[OFS_PARM0]);
}
/* float(float yaw, float dist) walkmove = #32 (self, своя трасса) */
static void csqc_walkmove (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float yaw, dist, rad;
	vec3_t start, end;
	trace_t tr;
	float *org = NULL;
	int ofs, entnum;

	if (!vm)
		return;
	yaw = vm->globals[OFS_PARM0];
	dist = vm->globals[OFS_PARM1];
	ofs = PR1VM_FindGlobal (vm, "self");
	if (ofs >= 0)
	{
		entnum = *(int *)&vm->globals[ofs] / vm->edict_size;
		org = csqc_ent_ofs (vm, entnum, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
	}
	if (!org)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	rad = yaw * (M_PI / 180.0);
	VectorCopy (org, start);
	end[0] = org[0] + cos (rad) * dist;
	end[1] = org[1] + sin (rad) * dist;	// QW/FTE: yaw 0 = +x, yaw 90 = +y
	end[2] = org[2];
	tr = csqc_world_trace (start, NULL, NULL, end);
	if (tr.fraction < 1)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	VectorCopy (end, org);
	vm->globals[OFS_RETURN] = 1;
}

/* float() droptofloor = #34 (self; трасса вниз до земли) */
static void csqc_droptofloor (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	vec3_t start, end;
	trace_t tr;
	float *org = NULL;
	int ofs, entnum;

	if (!vm)
		return;
	ofs = PR1VM_FindGlobal (vm, "self");
	if (ofs >= 0)
	{
		entnum = *(int *)&vm->globals[ofs] / vm->edict_size;
		org = csqc_ent_ofs (vm, entnum, CSQC_Client_FieldOfs (vm, CSQC_FLD_ORIGIN));
	}
	if (!org)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	VectorCopy (org, start);
	end[0] = org[0]; end[1] = org[1]; end[2] = org[2] - 512;	// FTE CSQC pr_csqc.c:5262
	tr = csqc_world_trace (start, NULL, NULL, end);
	if (tr.fraction >= 1 || tr.fraction <= 0)
	{
		vm->globals[OFS_RETURN] = 0;
		return;
	}
	org[0] = tr.endpos[0]; org[1] = tr.endpos[1]; org[2] = tr.endpos[2];
	// FTE pr_csqc.c:5273-5274 — посадка на землю помечается FL_ONGROUND и
	// groundentity (мир = world(0); мир-трасса ezq сущностей не бьёт — подмножество).
	{
		float *ff = csqc_ent_ofs (vm, entnum, CSQC_Client_FieldOfs (vm, CSQC_FLD_FLAGS));
		float *gf = csqc_ent_field (vm, entnum, "groundentity");
		if (ff)
			*ff = (float)((int)*ff | CSQC_FL_ONGROUND);
		if (gf)
			*(int *)gf = (tr.e.entnum > 0) ? tr.e.entnum * vm->edict_size : 0;
	}
	vm->globals[OFS_RETURN] = 1;
}

/* void(float step) movetogoal = #67 — no-op (у модуля нет поля goalentity) */
static void csqc_movetogoal (void)
{
	/* no-op (документировано; нет .goalentity) */
}

/*
L2 ST — строки/токенизация (2026-09-07; см. docs/archive/ezquake_csqc_client_l2_roadmap.md).
FTE-эталон — pr_bgcmd.c: strftime 5088, tokenize_console 6214, tokenizebyseparator
6219, argv_start_index 6307, argv_end_index 6321. Отдельное хранилище span'ов для
#514/#479/#515/#516; существующие #441/#442 работают через Cmd-контекст и НЕ меняются
(раздельные механизмы — отклонение в parity).
*/
#define CSQC_TOK_MAX 128
static int s_tokn = 0;
static int s_tok_start[CSQC_TOK_MAX];
static int s_tok_end[CSQC_TOK_MAX];

// Консольная токенизация (спаны): пробелы/табы разделители; "..." — один токен.
static void csqc_tok_console_spans (const char *s)
{
	int i = 0, len = s ? (int)strlen (s) : 0, n = 0;
	s_tokn = 0;
	while (i < len && n < CSQC_TOK_MAX)
	{
		while (i < len && (s[i] == ' ' || s[i] == '\t'))
			i++;
		if (i >= len)
			break;
		s_tok_start[n] = i;
		if (s[i] == '"')
		{
			i++;
			while (i < len && s[i] != '"')
				i++;
			if (i < len)
				i++;
		}
		else
		{
			while (i < len && s[i] != ' ' && s[i] != '\t')
				i++;
		}
		s_tok_end[n] = i;
		n++;
	}
	s_tokn = n;
}

// tokenizebyseparator: split по любому сепаратору (пустые токены учитываются),
// спаны как у FTE (6219).
static void csqc_tok_sep_spans (const char *s, const char *sep[], int nsep)
{
	int i, len, tokstart, n, si;
	int seplen[7];

	s_tokn = 0;
	if (!s || !*s)
		return;
	len = (int)strlen (s);
	for (si = 0; si < nsep && si < 7; si++)
		seplen[si] = (int)strlen (sep[si]);
	i = 0;
	tokstart = 0;
	n = 0;
	for (;;)
	{
		int found = -1;
		if (i >= len)
			found = -2;			// конец строки
		else
		{
			for (si = 0; si < nsep && si < 7; si++)
				if (!strncmp (s + i, sep[si], seplen[si]))
				{
					found = si;
					break;
				}
		}
		if (found >= 0)
		{
			if (n < CSQC_TOK_MAX)
			{
				s_tok_start[n] = tokstart;
				s_tok_end[n] = i;
				n++;
			}
			i += seplen[found];
			tokstart = i;
			if (n >= CSQC_TOK_MAX)
				break;
		}
		else if (found == -2)
		{
			if (n < CSQC_TOK_MAX)
			{
				s_tok_start[n] = tokstart;
				s_tok_end[n] = len;
				n++;
			}
			break;
		}
		else
			i++;
	}
	s_tokn = n;
}

/* string(float uselocaltime, string format, ...) strftime = #478 */
static void csqc_strftime (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *p;
	char buf[2048];
	int i, o = 0;
	time_t t;
	struct tm *tm;
	if (!vm)
		return;
	t = time (NULL);
	tm = (vm->globals[OFS_PARM0] != 0) ? localtime (&t) : gmtime (&t);
	for (i = 1; i < vm->argc && o < (int)sizeof (buf) - 1; i++)
	{
		p = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + i * 3]);
		if (!p)
			continue;
		// msvc-совместимость (как FTE): %R/%F
		if (!strcmp (p, "%R"))
			p = "%H:%M";
		else if (!strcmp (p, "%F"))
			p = "%Y-%m-%d";
		o += snprintf (buf + o, sizeof (buf) - o, "%s", p);
	}
	if (!o)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	{
		char out[512];
		if (strftime (out, sizeof (out), buf, tm))
			CSQCVM_SetRetStr (out);
		else
			CSQCVM_SetRetStr (buf);	// некорректный формат — вернуть как есть
	}
}

/* float(string str) tokenize_console = #514 — как #441 (Cmd) + спаны */
static void csqc_tokenize_console (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	if (s)
		Cmd_TokenizeStringEx (&csqc_tokencontext, s);
	vm->globals[OFS_RETURN] = Cmd_ArgcEx (&csqc_tokencontext);
	csqc_tok_console_spans (s);
}

/* float(string s, string sep1, ...) tokenizebyseparator = #479 */
static void csqc_tokenizebyseparator (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *s = CSQCVM_Str (OFS_PARM0);
	const char *sep[7];
	int nsep = 0, i;
	if (!vm)
		return;
	for (i = 1; i < vm->argc && nsep < 7; i++)
		sep[nsep++] = CSQCVM_Str (OFS_PARM0 + i * 3);
	csqc_tok_sep_spans (s, sep, nsep);
	vm->globals[OFS_RETURN] = s_tokn;
}

/* float(float idx) argv_start_index = #515 / argv_end_index = #516 */
static void csqc_argv_start_index (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int idx = (int)vm->globals[OFS_PARM0];
	if (!vm)
		return;
	if (idx < 0)
		idx += s_tokn;
	if ((unsigned int)idx >= (unsigned int)s_tokn)
		vm->globals[OFS_RETURN] = -1;
	else
		vm->globals[OFS_RETURN] = s_tok_start[idx];
}
static void csqc_argv_end_index (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int idx = (int)vm->globals[OFS_PARM0];
	if (!vm)
		return;
	if (idx < 0)
		idx += s_tokn;
	if ((unsigned int)idx >= (unsigned int)s_tokn)
		vm->globals[OFS_RETURN] = -1;
	else
		vm->globals[OFS_RETURN] = s_tok_end[idx];
}

/*
L2 — «Ввод/клавиатура/меню» (2026-09-07; roadmap волна 2). FTE-эталон —
pr_clcmd.c (findkeysforcommand 388, getkeybind 431, setkeybind 438,
stringtokeynum 451, keynumtostring 469, getresolution 774, GetBindMap 969,
setmousetarget 989, getmousetarget 1007). ezq: keybindings[]/Key_* (keys.h);
bindmaps/модификаторов/перечисления режимов нет — no-op/аппроксимации (parity).
*/

/* string(float keynum) getkeybind = #342 — binding команда или "" (B6: вход — QC-код) */
static void csqc_getkeybind (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int keynum = CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0]);
	char *b;
	if (!vm)
		return;
	if (keynum < 0 || keynum >= UNKNOWN + 256)
		b = NULL;
	else
		b = keybindings[keynum];
	CSQCVM_SetRetStr (b ? b : "");
}

/* void(float keynum, string binding, optional float bindmap) setkeybind = #630 (B6: вход — QC-код) */
static void csqc_setkeybind (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int keynum = CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0]);
	char *binding = CSQCVM_Str (OFS_PARM1);
	if (!vm)
		return;
	if (keynum >= 0 && keynum < UNKNOWN + 256)
		Key_SetBinding (keynum, binding ? binding : "");
}

/* #520 keynumtostring_omgwtf / #609 keynumtostring_menu — как #340 (QC-домен, B6) */
static void csqc_keynumtostring_menu (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr (Key_KeynumToString (CSQC_Client_QCToKeynum ((int)vm->globals[OFS_PARM0])));
}

/* float(string key) stringtokeynum_menu = #614 — как #341 (QC-домен, B6) */
static void csqc_stringtokeynum_menu (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (name && name[0]) ? CSQC_Client_KeynumToQC (Key_StringToKeynum (name)) : -1;
}

/*
string(string command, optional float bindmap) findkeysforcommand = #521
string(string command, optional float bindmap) findkeysforcommand_dp = #610
B9 (FTE-parity): скан keybindings[]; возврат QC-кодов в формате FTE
` 'code' 'code'…` (fteqw pr_clcmd.c:388-408: `va(" '%i'", …)`).
*/
static void csqc_findkeysforcommand (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *cmd = CSQCVM_Str (OFS_PARM0);
	char buf[512];
	int i, o = 0;
	if (!vm)
		return;
	buf[0] = 0;
	if (cmd && cmd[0])
	{
		for (i = 0; i < UNKNOWN + 256 && o < (int)sizeof (buf) - 8; i++)
		{
			if (keybindings[i] && !strcmp (keybindings[i], cmd))
				o += snprintf (buf + o, sizeof (buf) - o, " '%d'", CSQC_Client_KeynumToQC (i));
		}
	}
	CSQCVM_SetRetStr (buf);
}

/* void(float trg) setmousetarget = #603 — no-op (курсор через #343) */
static void csqc_setmousetarget (void)
{
	/* no-op (отдельного mousetarget нет; курсор — #343) */
}

/* float() getmousetarget = #604 — 2 если CSQC-курсор активен, иначе 1 */
static void csqc_getmousetarget (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = CSQC_Client_CSQCCursor () ? 2 : 1;
}

/* vector(float vidmode, optional float forfullscreen) getresolution = #608 —
   возврат текущего разрешения (список режимов не перечисляем) */
static void csqc_getresolution (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = vid.width;
	vm->globals[OFS_RETURN + 1] = vid.height;
	vm->globals[OFS_RETURN + 2] = 0;
}

/* vector() getbindmaps = #631 — bindmaps нет: (0,0,0) */
static void csqc_getbindmaps (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = 0;
	vm->globals[OFS_RETURN + 1] = 0;
	vm->globals[OFS_RETURN + 2] = 0;
}

/* float(vector bindmaps) setbindmaps = #632 — no-op, возврат 1 */
static void csqc_setbindmaps (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 1;
}

/*
L2 — «Звук» (2026-09-07; roadmap волна 5). FTE-эталон — pr_csqc.c/pr_clcmd.c.
Реализовано: #483 pointsound (S_PrecacheSound + S_StartSound(0,0,…) с origin —
позиционный по origin, как FTE). No-op (нет аналога в ezq): #351 SetListener
(аудио-листенер фиксирован), #371 deltalisten (предикция EXT_CSQC_1), #533
getsoundtime / #534 soundlength (нет канальных таймингов/длины сэмпла).
*/

/*
void(vector origin, string sample, float volume, float attenuation,
     optional float pitchpct) pointsound = #483
FTE pr_csqc.c:4694: 5-й арг — pitch в процентах (100 = норм), кладётся в playback-rate
(chan->rate, snd_dma.c:2962). В ezq-sound нет pitch/rate (channel_t/микшер без rate) —
аргумент принимается для идентичности вызова и игнорируется (parity-audit §D.2).
*/
static void csqc_pointsound (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float *org;
	char *sample;
	sfx_t *sfx;
	float pitchpct = 0;
	if (!vm)
		return;
	org = &vm->globals[OFS_PARM0];
	sample = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0 + 3]);
	if (vm->argc >= 5)
		pitchpct = vm->globals[OFS_PARM0 + 12] * 0.01f;	// только для паритета вызова
	(void)pitchpct;
	if (!sample || !sample[0])
		return;
	sfx = S_PrecacheSound (sample);
	if (sfx)
		S_StartSound (0, 0, sfx, org, vm->globals[OFS_PARM0 + 6], vm->globals[OFS_PARM0 + 9]);
}

/* #351 SetListener / #371 deltalisten */
/*
void(vector origin, vector forward, vector right, vector up) setlistener = #351
C5-E Ф1: модуль задаёт аудио-листенер; cl_main.c использует его в S_Update, пока
модуль активен (иначе — движковый вид). FTE-паритет для предикции звука.
*/
static void csqc_setlistener (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQC_Client_SetListener (&vm->globals[OFS_PARM0 + 0], &vm->globals[OFS_PARM0 + 3],
		&vm->globals[OFS_PARM0 + 6], &vm->globals[OFS_PARM0 + 9]);
}
/*
float(string modelname, float(float isnew) updatecallback, float flags) deltalisten = #371
E1a: реальная регистрация (FTE PF_DeltaListen, pr_csqc.c:5810) — реестр callback'ов
по modelindex; движок вызывает их при обновлении сущностей (player_state-мост в
CSQC_Client_DeltaPlayers; delta-entities — E1b). modelname="*" — все модели;
func<=0/невалидный — снятие регистрации.
*/
static void csqc_deltalisten (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *model;
	int func;
	if (!vm)
		return;
	model = CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_PARM0]);
	func = *(int *)&vm->globals[OFS_PARM1];
	if (func < 0 || func >= vm->progs->numfunctions)
		func = 0;	// невалидный указатель — снятие/no-op
	CSQC_Client_DeltaListen (model, func, (int)vm->globals[OFS_PARM2]);
	vm->globals[OFS_RETURN] = 0;
}

/* #533 getsoundtime / #534 soundlength — no-op (нет канальных таймингов/длины) */
static void csqc_getsoundtime (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = -1;
}
static void csqc_soundlength (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 0;
}

/*
L2 — «Entity-рефлексия» (#496-500, 2026-09-07; roadmap — ранее отложено). FTE-эталон
pr_bgcmd.c:7694-7830 (FieldInfo + UglyValueString/ParseEval). Работаем по
vm->fielddefs[] (ddef_t: name/type/ofs в словах арены, etype_t ev_* из pr_comp.h).
*/
static ddef_t *csqc_fielddef (pr1vm_t *vm, unsigned int fidx)
{
	if (!vm || !vm->fielddefs || !vm->progs)
		return NULL;
	if (fidx >= (unsigned int)vm->progs->numfielddefs)
		return NULL;
	return &vm->fielddefs[fidx];
}

/* float() numentityfields = #496 */
static void csqc_numentityfields (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm || !vm->progs)
		return;
	vm->globals[OFS_RETURN] = vm->progs->numfielddefs;
}

/* string(float fieldnum) entityfieldname = #497 */
static void csqc_entityfieldname (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	char *s;
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	if (!f)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	s = CSQC_Client_GetString (vm, f->s_name);
	CSQCVM_SetRetStr (s ? s : "");
}

/* float(float fieldnum) entityfieldtype = #498 (полный ddef_t.type; FTE pr_bgcmd.c:7750) */
static void csqc_entityfieldtype (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	vm->globals[OFS_RETURN] = f ? (float)f->type : 0;
}

/* string(float fieldnum, entity ent) getentityfieldstring = #499 */
static void csqc_getentityfieldstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	float *slot;
	int slotidx, type, ofs;
	char buf[512];
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	slotidx = csqc_ent_of (vm, OFS_PARM1);
	if (!f || slotidx < 0)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	slot = csqc_ent_slot (vm, slotidx);
	if (!slot)
	{
		CSQCVM_SetRetStr ("");
		return;
	}
	type = f->type & 0xff;
	ofs = f->ofs;
	switch (type)
	{
	case ev_string:
		{
			char *s = CSQC_Client_GetString (vm, *(int *)&slot[ofs]);
			CSQCVM_SetRetStr (s ? s : "");
			return;
		}
	case ev_vector:
		snprintf (buf, sizeof (buf), "%g %g %g", slot[ofs], slot[ofs + 1], slot[ofs + 2]);
		break;
	case ev_entity:
		{
			int v = *(int *)&slot[ofs];
			snprintf (buf, sizeof (buf), "entity %d", (vm->edict_size > 0) ? v / vm->edict_size : v);
			break;
		}
	case ev_float:
	default:
		csqc_q_ftoa (buf, sizeof (buf), slot[ofs]);
		break;
	}
	CSQCVM_SetRetStr (buf);
}

/* float(float fieldnum, entity ent, string s) putentityfieldstring = #500 */
static void csqc_putentityfieldstring (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	ddef_t *f;
	float *slot;
	int slotidx, type, ofs;
	char *s = CSQCVM_Str (OFS_PARM2);
	if (!vm)
		return;
	f = csqc_fielddef (vm, (unsigned int)vm->globals[OFS_PARM0]);
	slotidx = csqc_ent_of (vm, OFS_PARM1);
	vm->globals[OFS_RETURN] = 0;
	if (!f || slotidx < 0)
		return;
	slot = csqc_ent_slot (vm, slotidx);
	if (!slot)
		return;
	type = f->type & 0xff;
	ofs = f->ofs;
	switch (type)
	{
	case ev_string:
		PR1VM_ClientSetString (vm, (string_t *)&slot[ofs], s ? s : "");
		break;
	case ev_vector:
		{
			float v[3] = { 0, 0, 0 };
			int n = 0;
			if (s)
				n = sscanf (s, "%f %f %f", &v[0], &v[1], &v[2]);
			if (n > 0)
				VectorCopy (v, &slot[ofs]);
			else
				return;
			break;
		}
	case ev_float:
	default:
		slot[ofs] = (s && s[0]) ? (float)atof (s) : 0;
		break;
	}
	vm->globals[OFS_RETURN] = 1;
}

/*
L2 — «BSP-поверхности» (2026-09-07). Все no-op: FTE читает геометрию brush-моделей
(surfaces/mesh/plane/texture, pr_bgcmd.c:953-1350); в ezq такого geometry-интерфейса
моделей нет. Регистрация — защита от «Bad builtin».
*/
static void csqc_bsp_nop_vec (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = 0;
	vm->globals[OFS_RETURN + 1] = 0;
	vm->globals[OFS_RETURN + 2] = 0;
}

/*
L2 — «Интроспекция/кон» (2026-09-07; roadmap продолжение L2). FTE-эталон —
pr_bgcmd.c (isfunction 3809, callfunction 3816, argescape 6349, checkcommand 7820),
pr_menu.c (con_* 1143+). Реализовано: #294 checkcommand (ezq: cmd→1, cvar→3,
alias недоступно→0), #295 argescape (своё quoting), #607 isfunction. No-op:
#391/#392/#393/#394 (multi-console FTE нет в ezq), #605 callfunction (reentrant
exec из builtin не поддержан — отложено).
*/

/* float(string name) checkcommand = #294 */
static void csqc_checkcommand (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	if (name && name[0] && Cmd_Exists (name))
		vm->globals[OFS_RETURN] = 1;
	else if (name && name[0] && Cvar_Find (name))
		vm->globals[OFS_RETURN] = 3;
	else
		vm->globals[OFS_RETURN] = 0;
}

/* string(string s) argescape = #295 — оборачивает в кавычки с экранированием */
static void csqc_argescape (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	const char *s = CSQCVM_Str (OFS_PARM0);
	char buf[4096];
	int i, n, need = 0;
	if (!vm)
		return;
	s = s ? s : "";
	n = strlen (s);
	for (i = 0; i < n; i++)
		if (s[i] == ' ' || s[i] == '\t' || s[i] == '"' || s[i] == '\\' || s[i] == ';')
		{
			need = 1;
			break;
		}
	if (!need)
	{
		CSQCVM_SetRetStr ((char *)s);
		return;
	}
	{
		char *d = buf;
		*d++ = '"';
		for (i = 0; i < n && d < buf + sizeof (buf) - 3; i++)
		{
			if (s[i] == '"' || s[i] == '\\')
				*d++ = '\\';
			*d++ = s[i];
		}
		*d++ = '"';
		*d = 0;
	}
	CSQCVM_SetRetStr (buf);
}

/* float(string name) isfunction = #607 — функция есть в модуле */
static void csqc_isfunction (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	char *name = CSQCVM_Str (OFS_PARM0);
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (name && name[0] && PR1VM_FindFunction (vm, name)) ? 1 : 0;
}

/*
L2 — «Свет/decals/скины» (2026-09-07; roadmap волна 5-финальная). Все номера —
документированные no-op/аппроксимации: в ezq нет decal/skin-файловых подсистем
FTE (`Mod_*Skin`, `CL_AddDecal`), readback-пикч и констант `lfield_*` для
`cl_dlights[]`. Регистрация — чтобы модуль не ловил «Bad builtin».
*/
static void csqc_light_nop_ret0 (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = 0;
}

/*
L2 — «Система/VM остаток» (2026-09-07; roadmap волна 3, минимум-скоуп). Реализовано
полностью: #98 findfloat (обход пула по float-полю, как FTE PF_FindFloat
pr_bgcmd.c:1643). #92 getlight — аппроксимация (сэмпла света нет → 0). No-op
(серверно-мировые/нет аналога): #64 tracetoss, #240 checkpvs, #278 terrain_edit,
#279 touchtriggers, #504 getentity. #206 instr/#496-500 (рефлексия FieldInfo) —
отложены отдельным шагом (сигнатура/механизм).
*/

/*
entity(entity start, .float fld, float match) findfloat = #98
(он же findentity у FTE). Возврат: следующий used-слот после start с равенством
значения float-поля; нет — world (0).
*/
static void csqc_findfloat (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int e, f;
	float match, *slot;
	if (!vm)
		return;
	e = csqc_ent_of (vm, OFS_PARM0);
	f = *(int *)&vm->globals[OFS_PARM0 + 3];
	match = vm->globals[OFS_PARM0 + 6];
	if (f < 0 || f >= vm->progs->entityfields)
	{
		CSQC_Client_Abort ("PF_FindFloat: bad field reference");
		return;
	}
	if (e < 0)
		e = 0;
	for (e++; e < vm->num_edicts; e++)
	{
		if (!CSQC_Client_EntUsed (e))
			continue;
		slot = csqc_ent_slot (vm, e);
		if (!slot)
			continue;
		// FTE pr_bgcmd.c:1672 сравнивает сырые 32-битные значения
		// (((int*)ed->v)[f] == G_INT(PARM2)), а не float (различие — -0.0/NaN).
		if (*(int *)&slot[f] == *(int *)&match)
		{
			csqc_ret_entity (vm, e);
			return;
		}
	}
	csqc_ret_entity (vm, 0);
}

/* vector(vector org) getlight = #92 — аппроксимация: сэмпла статик-света нет → 0 */
static void csqc_getlight_approx (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN + 0] = 0;
	vm->globals[OFS_RETURN + 1] = 0;
	vm->globals[OFS_RETURN + 2] = 0;
}

/* no-op: #64/#240/#278/#279 — серверно-мировые/нет аналога (см. parity) */
static void csqc_vmrest_nop (void)
{
	/* no-op (документировано) */
}

/*
__variant(float entnum, float fieldnum) getentity = #504 (C5-E Ф2).
FTE PF_getentity (pr_csqc.c:5862): интерп. состояние engine-сетевой сущности по
серверному номеру. Реализация — CSQC_Client_GetEntity (csqc_client.c); сюда
пишем 3 слова возврата (float-поля занимают первое). Поля без ezq-источника —
FTE-дефолт (отклонения в parity-audit).
*/
static void csqc_getentity (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	float out[3];
	if (!vm)
		return;
	CSQC_Client_GetEntity ((int)vm->globals[OFS_PARM0], (int)vm->globals[OFS_PARM1], out);
	vm->globals[OFS_RETURN + 0] = out[0];
	vm->globals[OFS_RETURN + 1] = out[1];
	vm->globals[OFS_RETURN + 2] = out[2];
}

/*
L2 — «Система/VM простые» (2026-09-07). FTE-эталон: etos pr_bgcmd.c:5029,
wasfreed/num_for_edict pr_bgcmd.c:3961/3970, print pr_bgcmd.c:4264, cprint
pr_csqc.c:662 (SCR_CenterPrint), isserver pr_clcmd.c:553. Entity-значение в нашей
классике = slot*edict_size; слот = ent_of (см. #459/#512 — слот-индексная семантика,
отклонение от FTE-«entnum», в parity).
*/

/* string(entity ent) etos = #65 — "entity <slot>" */
static void csqc_etos (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int slot;
	char buf[32];
	if (!vm)
		return;
	slot = csqc_ent_of (vm, OFS_PARM0);
	snprintf (buf, sizeof (buf), "entity %d", slot > 0 ? slot : 0);
	CSQCVM_SetRetStr (buf);
}

/* void(string s, ...) print = #339 — консоль (Con_Printf). */
static void csqc_print (void)
{
	char *s = CSQCVM_VarString (0);
	if (s && s[0])
		Con_Printf ("%s", s);
}

/* void(string s, ...) cprint = #338 — центр-экран (SCR_CenterPrint, как FTE). */
static void csqc_cprint (void)
{
	char *s = CSQCVM_VarString (0);
	SCR_CenterPrint (s ? s : "");
}

/* float() isserver = #350 — сервер запущен? (ezq-клиент включает сервер).
   Отклонение: без различения 0.5 (sv.allocated_client_slots нет в ezq). */
static void csqc_isserver (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	vm->globals[OFS_RETURN] = (sv.state != ss_dead) ? 1 : 0;
}

/* float(entity ent) wasfreed = #353 — слот освобождён (remove). */
static void csqc_wasfreed (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int slot;
	if (!vm)
		return;
	slot = csqc_ent_of (vm, OFS_PARM0);
	vm->globals[OFS_RETURN] = (slot > 0 && !CSQC_Client_EntUsed (slot)) ? 1 : 0;
}

/* float(entity ent) num_for_edict = #512 — слот-индекс (парный к #459). */
static void csqc_num_for_edict (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	int slot;
	if (!vm)
		return;
	slot = csqc_ent_of (vm, OFS_PARM0);
	vm->globals[OFS_RETURN] = slot > 0 ? slot : 0;
}

/* #63 changepitch — no-op (движение углов к idealpitch — серверная механика). */
static void csqc_changepitch (void)
{
	/* no-op (документировано; как changeyaw #49) */
}

/* #355 getentitytoken — deprecated/не нужен: возврат "" (""). */
static void csqc_nop_str (void)
{
	pr1vm_t *vm = CSQCVM_Active ();
	if (!vm)
		return;
	CSQCVM_SetRetStr ("");
}

void CSQCVM_RegisterBuiltins (pr1vm_t *vm)
{
	// #1 makevectors (C6.1, FTE-паритет) — до CSQC-специфичных.
	PR1VM_RegisterBuiltin (vm, 1, (builtin_t)csqc_makevectors); // #1 void() makevectors (QUAKE)

	// Phase 1 L1 P1a — реюз чистых float/vector серверных PF_* (см. extern выше):
	// attach в PR1VM_ExecuteProgram переключает pr_globals на исполняемую VM.
	// #7/#13/#51 — клиентские wrapper'ы (FTE-паритет возврата/арг), серверные
	// PF_random/PF_vectoyaw/PF_vectoangles общие с сервером — не трогаем.
	PR1VM_RegisterBuiltin (vm, 7,   (builtin_t)csqc_random); // #7 float() random (QUAKE)
	PR1VM_RegisterBuiltin (vm, 9,   (builtin_t)PF_normalize); // #9 vector(vector in) normalize (QUAKE)
	PR1VM_RegisterBuiltin (vm, 12,  (builtin_t)PF_vlen); // #12 float(vector v) vlen (QUAKE)
	PR1VM_RegisterBuiltin (vm, 13,  (builtin_t)csqc_vectoyaw); // #13 float(vector v) vectoyaw (QUAKE)
	PR1VM_RegisterBuiltin (vm, 29,  (builtin_t)PF_traceon); // #29 void() traceon (QUAKE)
	PR1VM_RegisterBuiltin (vm, 30,  (builtin_t)PF_traceoff); // #30 void() traceoff (QUAKE)
	PR1VM_RegisterBuiltin (vm, 36,  (builtin_t)PF_rint); // #36 float(float f) rint (QUAKE)
	PR1VM_RegisterBuiltin (vm, 37,  (builtin_t)PF_floor); // #37 float(float f) floor (QUAKE)
	PR1VM_RegisterBuiltin (vm, 38,  (builtin_t)PF_ceil); // #38 float(float f) ceil (QUAKE)
	PR1VM_RegisterBuiltin (vm, 43,  (builtin_t)PF_fabs); // #43 float(float f) fabs (QUAKE)
	PR1VM_RegisterBuiltin (vm, 51,  (builtin_t)csqc_vectoangles); // #51 vector(vector v) vectoangles (QUAKE)
	PR1VM_RegisterBuiltin (vm, 60,  (builtin_t)PF_sin); // #60 float(float angle) sin (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 61,  (builtin_t)PF_cos); // #61 float(float angle) cos (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 62,  (builtin_t)PF_sqrt); // #62 float(float value) sqrt (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 94,  (builtin_t)PF_min); // #94 float(float a, floats) min (DP_QC_MINMAXBOUND)
	PR1VM_RegisterBuiltin (vm, 95,  (builtin_t)PF_max); // #95 float(float a, floats) max (DP_QC_MINMAXBOUND)
	PR1VM_RegisterBuiltin (vm, 96,  (builtin_t)PF_bound); // #96 float(float minimum, float val, float maximum) bound (DP_QC_MINMAXBOUND)
	// #97 pow / #91 randomvec — тела в pr_cmds.c статические: лёгкие клиентские
	// обработчики (чистая математика, читают/пишут vm->globals).
	PR1VM_RegisterBuiltin (vm, 97,  (builtin_t)csqc_pow); // #97 float(float value) pow (DP_QC_SINCOSSQRTPOW)
	PR1VM_RegisterBuiltin (vm, 91,  (builtin_t)csqc_randomvec); // #91 vector() randomvec (DP_QC_RANDOMVEC)

	// Phase 1 L1 P1c — cvar/exec/ошибки (#10/#11/#46/#72/#93/#99/#231;
	// #28 coredump / #31 eprint — P1d).
	PR1VM_RegisterBuiltin (vm, 10,  (builtin_t)csqc_error); // #10 void(string errortext) error (QUAKE)
	PR1VM_RegisterBuiltin (vm, 11,  (builtin_t)csqc_objerror); // #11 void(string errortext) onjerror (QUAKE)
	PR1VM_RegisterBuiltin (vm, 46,  (builtin_t)csqc_localcmd); // #46 void(string str) localcmd (QUAKE)
	PR1VM_RegisterBuiltin (vm, 72,  (builtin_t)csqc_cvar_set); // #72 void(string cvarname, string valuetoset) cvar_set (QUAKE)
	PR1VM_RegisterBuiltin (vm, 93,  (builtin_t)csqc_registercvar); // #93 void(string cvarname, string defaultvalue) registercvar (DP_QC_REGISTERCVAR)
	PR1VM_RegisterBuiltin (vm, 99,  (builtin_t)csqc_checkextension); // #99 float(string extname) checkextension (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 231, (builtin_t)csqc_calltimeofday); // #231 void() calltimeofday

	// Phase 1 L1 P1b — строки/конверсии (#118/#119 — ring/no-op, отклонение).
	PR1VM_RegisterBuiltin (vm, 27,  (builtin_t)csqc_vtos); // #27 string(vector f) vtos (QUAKE)
	PR1VM_RegisterBuiltin (vm, 81,  (builtin_t)csqc_stof); // #81 float(string s) stof (FRIK_FILE or QW_ENGINE)
	PR1VM_RegisterBuiltin (vm, 114, (builtin_t)csqc_strlen); // #114 float(string str) strlen (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 116, (builtin_t)csqc_substring); // #116 string(string str, float start, float length) substring (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 117, (builtin_t)csqc_stov); // #117 vector(string str) stov (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 118, (builtin_t)csqc_strzone); // #118 string(string str) dupstring (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 119, (builtin_t)csqc_strunzone); // #119 void(string str) freestring (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 448, (builtin_t)csqc_cvar_string); // #448 string(float n) cvar_string (DP_QC_CVAR_STRING)

	// Phase 1 L1 P1e — клиентские подсистемы (no-op/отклонения — в parity-audit).
	PR1VM_RegisterBuiltin (vm, 6,   (builtin_t)csqc_breakpoint); // #6 void() debugbreak (QUAKE)
	PR1VM_RegisterBuiltin (vm, 8,   (builtin_t)csqc_sound); // #8 void(entity e, float chan, string samp, float vol, float atten) sound (QUAKE)
	PR1VM_RegisterBuiltin (vm, 19,  (builtin_t)csqc_precache_sound); // #19 void(string str) precache_sound (QUAKE)
	PR1VM_RegisterBuiltin (vm, 20,  (builtin_t)csqc_precache_model); // #20 void(string str) precache_model (QUAKE)
	PR1VM_RegisterBuiltin (vm, 35,  (builtin_t)csqc_lightstyle); // #35 void(float lightstyle, string stylestring) lightstyle (QUAKE)
	PR1VM_RegisterBuiltin (vm, 48,  (builtin_t)csqc_particle); // #48 void(vector org, vector dir, float colour, float count) particle (QUAKE)
	PR1VM_RegisterBuiltin (vm, 68,  (builtin_t)csqc_precache_file); // #68 void(string s) precache_file (QUAKE) (don't support)
	PR1VM_RegisterBuiltin (vm, 74,  (builtin_t)csqc_ambientsound); // #74 void (vector pos, string samp, float vol, float atten) ambientsound (QUAKE)
	PR1VM_RegisterBuiltin (vm, 75,  (builtin_t)csqc_precache_model); // #75 void(string str) precache_model2 (QUAKE)
	PR1VM_RegisterBuiltin (vm, 76,  (builtin_t)csqc_precache_sound); // #76 void(string str) precache_sound2 (QUAKE)
	PR1VM_RegisterBuiltin (vm, 77,  (builtin_t)csqc_precache_file); // #77 void(string str) precache_file2 (QUAKE)
	PR1VM_RegisterBuiltin (vm, 531, (builtin_t)csqc_setpause); // #531 ?

	// Phase 1 L1 P1d C1 — базовые entity на арене (резерв C0-A).
	PR1VM_RegisterBuiltin (vm, 2,   (builtin_t)csqc_setorigin); // #2 void(entity e, vector org) setorigin (QUAKE)
	PR1VM_RegisterBuiltin (vm, 3,   (builtin_t)csqc_setmodel); // #3 void(entity e, string modl) setmodel (QUAKE)
	PR1VM_RegisterBuiltin (vm, 4,   (builtin_t)csqc_setsize); // #4 void(entity e, vector mins, vector maxs) setsize (QUAKE)
	PR1VM_RegisterBuiltin (vm, 14,  (builtin_t)csqc_spawn); // #14 entity() spawn (QUAKE)
	PR1VM_RegisterBuiltin (vm, 15,  (builtin_t)csqc_remove); // #15 void(entity e) remove (QUAKE)
	PR1VM_RegisterBuiltin (vm, 18,  (builtin_t)csqc_find); // #18 entity(entity start, .string fld, string match) findstring (QUAKE)
	PR1VM_RegisterBuiltin (vm, 22,  (builtin_t)csqc_findradius); // #22 entity(vector org, float rad) findradius (QUAKE)
	PR1VM_RegisterBuiltin (vm, 28,  (builtin_t)csqc_coredump); // #28 void(void) coredump (QUAKE)
	PR1VM_RegisterBuiltin (vm, 31,  (builtin_t)csqc_eprint); // #31 void(entity e) eprint (QUAKE)
	PR1VM_RegisterBuiltin (vm, 40,  (builtin_t)csqc_checkbottom); // #40 float(entity e) checkbottom (QUAKE)
	PR1VM_RegisterBuiltin (vm, 47,  (builtin_t)csqc_nextent); // #47 entity(entity e) nextent (QUAKE)
	PR1VM_RegisterBuiltin (vm, 49,  (builtin_t)csqc_changeyaw); // #49 void() changeyaw (QUAKE)
	PR1VM_RegisterBuiltin (vm, 69,  (builtin_t)csqc_makestatic); // #69 void(entity e) makestatic (QUAKE)
	PR1VM_RegisterBuiltin (vm, 80,  (builtin_t)csqc_infokey); // #80 string(entity e, string keyname) infokey (QW_ENGINE) (don't support)

	// Phase 1 L1 P1d C2 — мировые трассы/физика (мир-only).
	PR1VM_RegisterBuiltin (vm, 16,  (builtin_t)csqc_traceline); // #16 void(vector v1, vector v2, float nomonst, entity forent) traceline (QUAKE)
	PR1VM_RegisterBuiltin (vm, 90,  (builtin_t)csqc_tracebox); // #90 void(vector start, vector mins, vector maxs, vector end, float nomonsters, entity ent) tracebox
	PR1VM_RegisterBuiltin (vm, 41,  (builtin_t)csqc_pointcontents); // #41 float(vector org) pointcontents (QUAKE)
	PR1VM_RegisterBuiltin (vm, 32,  (builtin_t)csqc_walkmove); // #32 float(float yaw, float dist) walkmove (QUAKE)
	PR1VM_RegisterBuiltin (vm, 34,  (builtin_t)csqc_droptofloor); // #34 float() droptofloor
	PR1VM_RegisterBuiltin (vm, 67,  (builtin_t)csqc_movetogoal); // #67 void(float step) movetogoal (QUAKE)

	PR1VM_RegisterBuiltin (vm, 25, (builtin_t)csqc_dprint); // #25 void(string s, ...) dprint (QUAKE)
	PR1VM_RegisterBuiltin (vm, 26, (builtin_t)csqc_ftos); // #26 string(float f) ftos (QUAKE)
	PR1VM_RegisterBuiltin (vm, 45, (builtin_t)csqc_cvar); // #45 float(string cvarname) cvar (QUAKE)

	// Слой D шаг 3 — ввод/интерфейс: #340 keynumtostring, #341 stringtokeynum.
	PR1VM_RegisterBuiltin (vm, 340, (builtin_t)csqc_keynumtostring); // #340 string(float keynum) keynumtostring (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 341, (builtin_t)csqc_stringtokeynum); // #341 float(string keyname) stringtokeynum (EXT_CSQC)
	// #349 isdemo, #354 serverkey.
	PR1VM_RegisterBuiltin (vm, 349, (builtin_t)csqc_isdemo); // #349 float() isdemo (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 354, (builtin_t)csqc_serverkey); // #354 string(string key) serverkey;
	// #343 setcursormode (A3.1: полная — курсор модуля в CSQC-оверлее).
	PR1VM_RegisterBuiltin (vm, 343, (builtin_t)csqc_setcursormode); // #343 void(float usecursor, optional string cursorimage, optional vector hotspot, optional float scale) setcursormode
	// #344 getmousepos (A3.2: read-путь позиции CSQC-курсора).
	PR1VM_RegisterBuiltin (vm, 344, (builtin_t)csqc_getmousepos); // #344 vector() getmousepos
	// #348 getplayerkeyvalue (A6).
	PR1VM_RegisterBuiltin (vm, 348, (builtin_t)csqc_getplayerkeyvalue); // #348 string(float playernum, string keyname) getplayerkeyvalue (EXT_CSQC)
	// C1.1 — #346 setsensitivityscaler.
	PR1VM_RegisterBuiltin (vm, 346, (builtin_t)csqc_setsensitivityscaler); // #346 void(float sens) setsensitivityscaler (EXT_CSQC)
	// C1.3 — #345 getinputstate.
	PR1VM_RegisterBuiltin (vm, 345, (builtin_t)csqc_getinputstate); // #345 float(float framenum) getinputstate (EXT_CSQC)
	// C1.4 — #347 runstandardplayerphysics.
	PR1VM_RegisterBuiltin (vm, 347, (builtin_t)csqc_runstandardplayerphysics); // #347 void() runstandardplayerphysics (EXT_CSQC)
	// C2.1 — #459 edict_num.
	PR1VM_RegisterBuiltin (vm, 459, (builtin_t)csqc_edict_num); // #459 entity(float entnum) edict_num
	// C2.2 — #460-469 string-buffers.
	PR1VM_RegisterBuiltin (vm, 460, (builtin_t)csqc_buf_create); // #460 float() buf_create
	PR1VM_RegisterBuiltin (vm, 461, (builtin_t)csqc_buf_del); // #461 void(float bufhandle) buf_del
	PR1VM_RegisterBuiltin (vm, 462, (builtin_t)csqc_buf_getsize); // #462 float(float bufhandle) buf_getsize
	PR1VM_RegisterBuiltin (vm, 463, (builtin_t)csqc_buf_copy); // #463 void(float bufhandle_from, float bufhandle_to) buf_copy
	PR1VM_RegisterBuiltin (vm, 464, (builtin_t)csqc_buf_sort); // #464 void(float bufhandle, float sortpower, float backward) buf_sort
	PR1VM_RegisterBuiltin (vm, 465, (builtin_t)csqc_buf_implode); // #465 string(float bufhandle, string glue) buf_implode
	PR1VM_RegisterBuiltin (vm, 466, (builtin_t)csqc_bufstr_get); // #466 string(float bufhandle, float string_index) bufstr_get
	PR1VM_RegisterBuiltin (vm, 467, (builtin_t)csqc_bufstr_set); // #467 void(float bufhandle, float string_index, string str) bufstr_set
	PR1VM_RegisterBuiltin (vm, 468, (builtin_t)csqc_bufstr_add); // #468 float(float bufhandle, string str, float order) bufstr_add
	PR1VM_RegisterBuiltin (vm, 469, (builtin_t)csqc_bufstr_free); // #469 void(float bufhandle, float string_index) bufstr_free
	// C3.1 — #177 localsound, #305 dynamiclight_add.
	PR1VM_RegisterBuiltin (vm, 177, (builtin_t)csqc_localsound); // #177 void(string soundname, optional float channel, optional float volume) localsound
	PR1VM_RegisterBuiltin (vm, 305, (builtin_t)csqc_dynamiclight_add); // #305 float(vector org, float radius, vector lightcolours) adddynamiclight (EXT_CSQC)
	// C3.2 — #335-337 частицы (мини-реестр).
	PR1VM_RegisterBuiltin (vm, 335, (builtin_t)csqc_particleeffectnum); // #335 float(string effectname) particleeffectnum (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 336, (builtin_t)csqc_trailparticles); // #336 void(float effectnum, entity ent, vector start, vector end) trailparticles (EXT_CSQC),
	PR1VM_RegisterBuiltin (vm, 337, (builtin_t)csqc_pointparticles); // #337 void(float effectnum, vector origin [, vector dir, float count]) pointparticles (EXT_CSQC)
	// C3.3a — te_* аппроксимируемая группа (#405-427, кроме #426).
	PR1VM_RegisterBuiltin (vm, 405, (builtin_t)csqc_te_blood); // #405 void(vector org, vector velocity, float howmany) te_blood (DP_TE_BLOOD)
	PR1VM_RegisterBuiltin (vm, 406, (builtin_t)csqc_te_bloodshower); // #406 void(vector mincorner, vector maxcorner, float explosionspeed, float howmany) te_bloodshower (DP_TE_BLOODSHOWER)
	PR1VM_RegisterBuiltin (vm, 407, (builtin_t)csqc_te_explosionrgb); // #407 void(vector org, vector color) te_explosionrgb (DP_TE_EXPLOSIONRGB)
	PR1VM_RegisterBuiltin (vm, 408, (builtin_t)csqc_te_particlecube); // #408 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color, float gravityflag, float randomveljitter) te_particlecube (DP_TE_PARTICLECUBE)
	PR1VM_RegisterBuiltin (vm, 409, (builtin_t)csqc_te_rain); // #409 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlerain (DP_TE_PARTICLERAIN)
	PR1VM_RegisterBuiltin (vm, 410, (builtin_t)csqc_te_rain); // #410 void(vector mincorner, vector maxcorner, vector vel, float howmany, float color) te_particlesnow (DP_TE_PARTICLESNOW)
	PR1VM_RegisterBuiltin (vm, 411, (builtin_t)csqc_te_spark); // #411 void(vector org, vector vel, float howmany) te_spark (DP_TE_SPARK)
	PR1VM_RegisterBuiltin (vm, 412, (builtin_t)csqc_te_quad); // #412 void(vector org) te_gunshotquad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 413, (builtin_t)csqc_te_quad); // #413 void(vector org) te_spikequad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 414, (builtin_t)csqc_te_quad); // #414 void(vector org) te_superspikequad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 415, (builtin_t)csqc_te_quad); // #415 void(vector org) te_explosionquad (DP_TE_QUADEFFECTS1)
	PR1VM_RegisterBuiltin (vm, 416, (builtin_t)csqc_te_smallflash); // #416 void(vector org) te_smallflash (DP_TE_SMALLFLASH)
	PR1VM_RegisterBuiltin (vm, 417, (builtin_t)csqc_te_customflash); // #417 void(vector org, float radius, float lifetime, vector color) te_customflash (DP_TE_CUSTOMFLASH)
	PR1VM_RegisterBuiltin (vm, 418, (builtin_t)csqc_te_gunshot); // #418 void(vector org) te_gunshot (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 419, (builtin_t)csqc_te_spike); // #419 void(vector org) te_spike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 420, (builtin_t)csqc_te_superspike); // #420 void(vector org) te_superspike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 421, (builtin_t)csqc_te_explosion); // #421 void(vector org) te_explosion (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 422, (builtin_t)csqc_te_tarexplosion); // #422 void(vector org) te_tarexplosion (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 423, (builtin_t)csqc_te_wizspike); // #423 void(vector org) te_wizspike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 424, (builtin_t)csqc_te_knightspike); // #424 void(vector org) te_knightspike (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 425, (builtin_t)csqc_te_lavasplash); // #425 void(vector org) te_lavasplash  (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 427, (builtin_t)csqc_te_explosion2); // #427 void(vector org, float color, float colorlength) te_explosion2 (DP_TE_STANDARDEFFECTBUILTINS)
	// C3.3b — beams #428-431.
	PR1VM_RegisterBuiltin (vm, 428, (builtin_t)csqc_te_lightning1); // #428 void(entity own, vector start, vector end) te_lightning1 (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 429, (builtin_t)csqc_te_lightning2); // #429 void(entity own, vector start, vector end) te_lightning2 (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 430, (builtin_t)csqc_te_lightning3); // #430 void(entity own, vector start, vector end) te_lightning3 (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 431, (builtin_t)csqc_te_beam); // #431 void(entity own, vector start, vector end) te_beam (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 115, (builtin_t)csqc_strcat); // #115 string(string str1, string str2, ...) strcat (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 221, (builtin_t)csqc_strstrofs); // #221 float(string s1, string sub) strstrofs (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 352, (builtin_t)csqc_registercommand); // #352 void(string cmdname) registercommand (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 441, (builtin_t)csqc_tokenize); // #441 float(string s) tokenize (KRIMZON_SV_PARSECLIENTCOMMAND)
	PR1VM_RegisterBuiltin (vm, 442, (builtin_t)csqc_argv); // #442 string(float n) argv (KRIMZON_SV_PARSECLIENTCOMMAND)

	// L2-тривиалы T1 — математика (2026-09-07): чистая математика/C, без состояния.
	PR1VM_RegisterBuiltin (vm, 471, (builtin_t)csqc_asin); // #471 float(float s) asin
	PR1VM_RegisterBuiltin (vm, 472, (builtin_t)csqc_acos); // #472 float(float c) acos
	PR1VM_RegisterBuiltin (vm, 473, (builtin_t)csqc_atan); // #473 float(float t) atan
	PR1VM_RegisterBuiltin (vm, 474, (builtin_t)csqc_atan2); // #474 float(float c, float s) atan2
	PR1VM_RegisterBuiltin (vm, 475, (builtin_t)csqc_tan); // #475 float(float a) tan
	PR1VM_RegisterBuiltin (vm, 532, (builtin_t)csqc_log); // #532 float(float value, optional float base) log
	PR1VM_RegisterBuiltin (vm, 102, (builtin_t)csqc_anglemod); // #102 float(float value) anglemod
	PR1VM_RegisterBuiltin (vm, 245, (builtin_t)csqc_mod); // #245 float(float a, float b) mod
	PR1VM_RegisterBuiltin (vm, 218, (builtin_t)csqc_bitshift); // #218 bitshift (EXT_DIMENSION_PLANES)
	PR1VM_RegisterBuiltin (vm, 494, (builtin_t)csqc_crc16); // #494 float(float caseinsensitive, string s, ...) crc16
	PR1VM_RegisterBuiltin (vm, 519, (builtin_t)csqc_gettimef); // #519 float(optional float timetype) gettime

	// L2-тривиалы T2 — int/hex конверсии (2026-09-07): #259-262.
	PR1VM_RegisterBuiltin (vm, 259, (builtin_t)csqc_stoi); // #259 int(string) stoi
	PR1VM_RegisterBuiltin (vm, 260, (builtin_t)csqc_itos); // #260 string(int) itos
	PR1VM_RegisterBuiltin (vm, 261, (builtin_t)csqc_stoh); // #261 int(string) stoh
	PR1VM_RegisterBuiltin (vm, 262, (builtin_t)csqc_htos); // #262 string(int) htos

	// L2-тривиалы T3 — cvar-метаданные (2026-09-07): #482/#495/#518.
	PR1VM_RegisterBuiltin (vm, 482, (builtin_t)csqc_cvar_defstring); // #482 string(string s) cvar_defstring
	PR1VM_RegisterBuiltin (vm, 495, (builtin_t)csqc_cvar_type); // #495 float(string name) cvar_type
	PR1VM_RegisterBuiltin (vm, 518, (builtin_t)csqc_cvar_description); // #518 string(string cvarname) cvar_description

	// L2-тривиалы T4 — строки простые (2026-09-07): #222/223/225/226/227/228/229/230/
	// 480/481/484/485.
	PR1VM_RegisterBuiltin (vm, 222, (builtin_t)csqc_str2chr); // #222 float(string str, float index) str2chr (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 223, (builtin_t)csqc_chr2str); // #223 string(float chr, ...) chr2str (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 225, (builtin_t)csqc_strpad); // #225 string strpad(float pad, string str1, ...) strpad (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 226, (builtin_t)csqc_infoadd); // #226 string(string old, string key, string value) infoadd
	PR1VM_RegisterBuiltin (vm, 227, (builtin_t)csqc_infoget); // #227 string(string info, string key) infoget
	PR1VM_RegisterBuiltin (vm, 228, (builtin_t)csqc_strncmp); // #228 float(string s1, string s2) strcmp (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 229, (builtin_t)csqc_strncasecmp); // #229 float(string s1, string s2) strcasecmp (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 230, (builtin_t)csqc_strncasecmp); // #230 float(string s1, string s2, float len) strncasecmp (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 480, (builtin_t)csqc_strtolower); // #480 string(string s) strtolower
	PR1VM_RegisterBuiltin (vm, 481, (builtin_t)csqc_strtoupper); // #481 string(string s) strlennocol
	PR1VM_RegisterBuiltin (vm, 484, (builtin_t)csqc_strreplace); // #484 string(string search, string replace, string subject) strreplace
	PR1VM_RegisterBuiltin (vm, 485, (builtin_t)csqc_strireplace); // #485 string(string search, string replace, string subject) strireplace

	// L2 — «Система/VM простые» (2026-09-07): #65/#338/#339/#350/#353/#512 +
	// no-op #63/#355.
	PR1VM_RegisterBuiltin (vm, 65,  (builtin_t)csqc_etos); // #65 string(entity ent) etos (DP_QC_ETOS)
	PR1VM_RegisterBuiltin (vm, 338, (builtin_t)csqc_cprint); // #338 void(string s) cprint (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 339, (builtin_t)csqc_print); // #339 void(string s) print (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 350, (builtin_t)csqc_isserver); // #350 float() isserver (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 353, (builtin_t)csqc_wasfreed); // #353 float(entity ent) wasfreed (EXT_CSQC) (should be availabe on server too)
	PR1VM_RegisterBuiltin (vm, 512, (builtin_t)csqc_num_for_edict); // #512 float(entity ent) num_for_edict
	PR1VM_RegisterBuiltin (vm, 63,  (builtin_t)csqc_changepitch); // #63 void(entity ent) changepitch (DP_QC_CHANGEPITCH)
	PR1VM_RegisterBuiltin (vm, 355, (builtin_t)csqc_nop_str); // #355 string() getentitytoken;

	// L2 ST — строки/токенизация (2026-09-07): #478/#514/#479/#515/#516.
	PR1VM_RegisterBuiltin (vm, 478, (builtin_t)csqc_strftime); // #478 string(float uselocaltime, string format, ...) strftime
	PR1VM_RegisterBuiltin (vm, 514, (builtin_t)csqc_tokenize_console); // #514 float(string str) tokenize_console
	PR1VM_RegisterBuiltin (vm, 479, (builtin_t)csqc_tokenizebyseparator); // #479 float(string s, string separator1, ...) tokenizebyseparator
	PR1VM_RegisterBuiltin (vm, 515, (builtin_t)csqc_argv_start_index); // #515 float(float idx) argv_start_index
	PR1VM_RegisterBuiltin (vm, 516, (builtin_t)csqc_argv_end_index); // #516 float(float idx) argv_end_index

	// L2 — «Ввод/клавиатура/меню» (2026-09-07): #342/#520/#521/#603/#604/#608/#609/#610/#614/#630/#631/#632.
	PR1VM_RegisterBuiltin (vm, 342, (builtin_t)csqc_getkeybind); // #342 string(float keynum) getkeybind (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 520, (builtin_t)csqc_keynumtostring_menu); // #520 string(float keynum) keynumtostring_omgwtf
	PR1VM_RegisterBuiltin (vm, 521, (builtin_t)csqc_findkeysforcommand); // #521 string(string command, optional float bindmap) findkeysforcommand
	PR1VM_RegisterBuiltin (vm, 603, (builtin_t)csqc_setmousetarget); // #603 void(float trg) setmousetarget
	PR1VM_RegisterBuiltin (vm, 604, (builtin_t)csqc_getmousetarget); // #604 float() getmousetarget
	PR1VM_RegisterBuiltin (vm, 608, (builtin_t)csqc_getresolution); // #608 vector(float vidmode, optional float forfullscreen) getresolution
	PR1VM_RegisterBuiltin (vm, 609, (builtin_t)csqc_keynumtostring_menu); // #609 string(float keynum) keynumtostring_menu
	PR1VM_RegisterBuiltin (vm, 610, (builtin_t)csqc_findkeysforcommand); // #610 string(string command, optional float bindmap) findkeysforcommand_dp
	PR1VM_RegisterBuiltin (vm, 614, (builtin_t)csqc_stringtokeynum_menu); // #614 float(string key) stringtokeynum_menu
	PR1VM_RegisterBuiltin (vm, 630, (builtin_t)csqc_setkeybind); // #630 void(float keynum, string binding, optional float bindmap) setkeybind
	PR1VM_RegisterBuiltin (vm, 631, (builtin_t)csqc_getbindmaps); // #631 vector() getbindmaps
	PR1VM_RegisterBuiltin (vm, 632, (builtin_t)csqc_setbindmaps); // #632 float(vector bindmaps) setbindmaps

	// L2 — «Система/VM остаток» (2026-09-07, минимум): #98 + #92-аппрокс + no-op
	// #64/#240/#278/#279 (рефлексия #496-500/#206 — отдельным шагом).
	PR1VM_RegisterBuiltin (vm, 98,  (builtin_t)csqc_findfloat); // #98 entity(entity start, .float fld, float match) findfloat (DP_QC_FINDFLOAT)
	PR1VM_RegisterBuiltin (vm, 92,  (builtin_t)csqc_getlight_approx); // #92 vector(vector org) getlight (DP_QC_GETLIGHT)
	PR1VM_RegisterBuiltin (vm, 64,  (builtin_t)csqc_vmrest_nop); // #64 void(entity ent, entity ignore) tracetoss (DP_QC_TRACETOSS)
	PR1VM_RegisterBuiltin (vm, 240, (builtin_t)csqc_vmrest_nop); // #240 float(vector viewpos, entity entity) checkpvs
	PR1VM_RegisterBuiltin (vm, 278, (builtin_t)csqc_vmrest_nop); // #278 void(float action, vector pos, float radius, float quant) terrain_edit
	PR1VM_RegisterBuiltin (vm, 279, (builtin_t)csqc_vmrest_nop); // #279 void() touchtriggers
	PR1VM_RegisterBuiltin (vm, 504, (builtin_t)csqc_getentity); // #504 __variant(float entnum, float fieldnum) getentity (C5-E Ф2)

	// L2 — «Звук» (2026-09-07): #483 + no-op #351/#371/#533/#534.
	PR1VM_RegisterBuiltin (vm, 483, (builtin_t)csqc_pointsound); // #483 void(vector origin, string sample, float volume, float attenuation) pointsound
	PR1VM_RegisterBuiltin (vm, 351, (builtin_t)csqc_setlistener); // #351 void(vector origin, vector forward, vector right, vector up) SetListener (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 371, (builtin_t)csqc_deltalisten); // #371 float(string modelname, float flags) deltalisten  (EXT_CSQC_1)
	PR1VM_RegisterBuiltin (vm, 533, (builtin_t)csqc_getsoundtime); // #533 float(entity e, float channel) getsoundtime
	PR1VM_RegisterBuiltin (vm, 534, (builtin_t)csqc_soundlength); // #534 float(string sample) soundlength

	// L2 — «Свет/decals/скины» (2026-09-07): все no-op (нет аналогов в ezq).
	PR1VM_RegisterBuiltin (vm, 372, (builtin_t)csqc_light_nop_ret0); // #372 __variant(float lno, float fld) dynamiclight_get
	PR1VM_RegisterBuiltin (vm, 373, (builtin_t)csqc_light_nop_ret0); // #373 void(float lno, float fld, __variant value) dynamiclight_set
	PR1VM_RegisterBuiltin (vm, 375, (builtin_t)csqc_light_nop_ret0); // #375 void(string shadername, vector origin, vector up, vector side, vector rgb, float alpha) adddecal
	PR1VM_RegisterBuiltin (vm, 376, (builtin_t)csqc_light_nop_ret0); // #376 void(entity e, string skinfilename, optional string skindata) setcustomskin
	PR1VM_RegisterBuiltin (vm, 377, (builtin_t)csqc_light_nop_ret0); // #377 ?
	PR1VM_RegisterBuiltin (vm, 378, (builtin_t)csqc_light_nop_ret0); // #378 ?
	PR1VM_RegisterBuiltin (vm, 379, (builtin_t)csqc_light_nop_ret0); // #379 ?
	PR1VM_RegisterBuiltin (vm, 501, (builtin_t)csqc_light_nop_ret0); // #501 void(float to, string s, float sz) WritePicture

	// L2 — «Интроспекция/кон» (2026-09-07): #294/#295/#607 + no-op #391-394/#605.
	PR1VM_RegisterBuiltin (vm, 294, (builtin_t)csqc_checkcommand); // #294 float(string name) checkcommand
	PR1VM_RegisterBuiltin (vm, 295, (builtin_t)csqc_argescape); // #295 string(string s) argescape
	PR1VM_RegisterBuiltin (vm, 607, (builtin_t)csqc_isfunction); // #607 float(string s) isfunction
	PR1VM_RegisterBuiltin (vm, 391, (builtin_t)csqc_nop_str); // #391 string(string conname, string field, optional string newvalue) con_getset
	PR1VM_RegisterBuiltin (vm, 392, (builtin_t)csqc_vmrest_nop); // #392 void(string conname, string messagefmt, ...) con_printf
	PR1VM_RegisterBuiltin (vm, 393, (builtin_t)csqc_vmrest_nop); // #393 void(string conname, vector pos, vector size, float fontsize) con_draw
	PR1VM_RegisterBuiltin (vm, 394, (builtin_t)csqc_light_nop_ret0); // #394 float(string conname, float inevtype, float parama, float paramb, float paramc) con_input
	PR1VM_RegisterBuiltin (vm, 605, (builtin_t)csqc_vmrest_nop); // #605 void(.../*, string funcname*/) callfunction

	// L2 — «BSP-поверхности» (2026-09-07): все no-op (нет geometry-интерфейса).
	PR1VM_RegisterBuiltin (vm, 434, (builtin_t)csqc_light_nop_ret0); // #434 float(entity e, float s) getsurfacenumpoints (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 435, (builtin_t)csqc_bsp_nop_vec); // #435 vector(entity e, float s, float n) getsurfacepoint (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 436, (builtin_t)csqc_bsp_nop_vec); // #436 vector(entity e, float s) getsurfacenormal (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 437, (builtin_t)csqc_nop_str); // #437 string(entity e, float s) getsurfacetexture (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 438, (builtin_t)csqc_light_nop_ret0); // #438 float(entity e, vector p) getsurfacenearpoint (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 439, (builtin_t)csqc_bsp_nop_vec); // #439 vector(entity e, float s, vector p) getsurfaceclippedpoint (DP_QC_GETSURFACE)
	PR1VM_RegisterBuiltin (vm, 486, (builtin_t)csqc_bsp_nop_vec); // #486 vector(entity e, float s, float n, float a) getsurfacepointattribute
	PR1VM_RegisterBuiltin (vm, 628, (builtin_t)csqc_light_nop_ret0); // #628 float(entity e, float s) getsurfacenumtriangles
	PR1VM_RegisterBuiltin (vm, 629, (builtin_t)csqc_bsp_nop_vec); // #629 vector(entity e, float s, float n) getsurfacetriangle

	// L2 — «Entity-рефлексия» (2026-09-07): #496-500 по fielddefs модуля.
	PR1VM_RegisterBuiltin (vm, 496, (builtin_t)csqc_numentityfields); // #496 float() numentityfields
	PR1VM_RegisterBuiltin (vm, 497, (builtin_t)csqc_entityfieldname); // #497 string(float fieldnum) entityfieldname
	PR1VM_RegisterBuiltin (vm, 498, (builtin_t)csqc_entityfieldtype); // #498 float(float fieldnum) entityfieldtype
	PR1VM_RegisterBuiltin (vm, 499, (builtin_t)csqc_getentityfieldstring); // #499 string(float fieldnum, entity ent) getentityfieldstring
	PR1VM_RegisterBuiltin (vm, 500, (builtin_t)csqc_putentityfieldstring); // #500 float(float fieldnum, entity ent, string s) putentityfieldstring

	// P2.3 — визуальный слой B (2D-оверлей; сетевая часть B — позже).
	PR1VM_RegisterBuiltin (vm, 300, (builtin_t)csqc_clearscene); // #300 void() clearscene (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 301, (builtin_t)csqc_addentities); // #301 void(float mask) addentities (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 303, (builtin_t)csqc_setproperty); // #303 float(float property, ...) setproperty (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 304, (builtin_t)csqc_renderscene); // #304 void() renderscene (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 309, (builtin_t)csqc_getproperty); // #309 vector/float(float property) getproperty (EXT_CSQC_1)
	PR1VM_RegisterBuiltin (vm, 326, (builtin_t)csqc_drawstring); // #326 float(vector position, string text, vector size, vector rgb, float alpha, float drawflag) drawstring
	PR1VM_RegisterBuiltin (vm, 315, (builtin_t)csqc_drawline); // #315 void(float width, vector pos1, vector pos2) drawline (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 317, (builtin_t)csqc_precache_pic); // #317 string(string name, float trywad) precache_pic (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 320, (builtin_t)csqc_drawcharacter); // #320 float(vector position, float character, vector scale, vector rgb, float alpha [, float flag]) drawcharacter (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 322, (builtin_t)csqc_drawpic); // #322 float(vector position, string pic, vector size, vector rgb, float alpha [, float flag]) drawpic (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 323, (builtin_t)csqc_drawfill); // #323 float(vector position, vector size, vector rgb, float alpha [, float flag]) drawfill (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 327, (builtin_t)csqc_stringwidth); // #327 float(string text, float usecolours, optional vector fontsize) stringwidth
	PR1VM_RegisterBuiltin (vm, 328, (builtin_t)csqc_drawsubpic); // #328 void(vector pos, vector sz, string pic, vector srcpos, vector srcsz, vector rgb, float alpha, optional float drawflag) drawsubpic
	// L2 — «2D-графика доп» (2026-09-07): #316/#318/#319/#321/#324/#325 + no-op #329.
	PR1VM_RegisterBuiltin (vm, 316, (builtin_t)csqc_iscachedpic); // #316 float(string name) iscachedpic (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 318, (builtin_t)csqc_drawgetimagesize); // #318 vector(string picname) draw_getimagesize (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 319, (builtin_t)csqc_freepic); // #319 void(string name) freepic (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 321, (builtin_t)csqc_drawrawstring); // #321 float(vector position, string text, vector scale, vector rgb, float alpha [, float flag]) drawstring (EXT_CSQC, [EXT_CSQC_???])
	PR1VM_RegisterBuiltin (vm, 324, (builtin_t)csqc_drawsetcliparea); // #324 void(float x, float y, float width, float height) drawsetcliparea (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 325, (builtin_t)csqc_drawresetcliparea); // #325 void(void) drawresetcliparea (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 329, (builtin_t)csqc_drawrotpic_dp); // #329 void(vector pivot, string picname, vector size, vector mins, float angle, vector rgb, float alpha, optional float drawflag) drawrotpic_dp
	PR1VM_RegisterBuiltin (vm, 330, (builtin_t)csqc_getstati); // #330 int(float stnum) getstati (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 331, (builtin_t)csqc_getstatf); // #331 float(float stnum) getstatf (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 332, (builtin_t)csqc_getstats); // #332 string(float firststnum) getstats (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 359, (builtin_t)csqc_sendevent); // #359 void(string evname, string evargs, ...) (EXT_CSQC_1)
	PR1VM_RegisterBuiltin (vm, 627, (builtin_t)csqc_sprintf); // #627 string(string fmt, ...) sprintf

	// S1 read*-минимум (полный набор #360–368 — S2).
	PR1VM_RegisterBuiltin (vm, 360, (builtin_t)csqc_readbyte); // #360 float() readbyte (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 361, (builtin_t)csqc_readchar); // #361 float() readchar (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 362, (builtin_t)csqc_readshort); // #362 float() readshort (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 363, (builtin_t)csqc_readlong); // #363 float() readlong (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 364, (builtin_t)csqc_readcoord); // #364 float() readcoord (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 365, (builtin_t)csqc_readangle); // #365 float() readangle (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 366, (builtin_t)csqc_readstring); // #366 string() readstring (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 367, (builtin_t)csqc_readfloat); // #367 float() readfloat (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 368, (builtin_t)csqc_readentitynum); // #368 float() readentitynum (EXT_CSQC)

	// L2 заглушки: VOID (67) — тип-correct no-op.
	PR1VM_RegisterBuiltin (vm, 111, (builtin_t)csqc_vmrest_nop); // #111 void(float fnum) fclose (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 113, (builtin_t)csqc_vmrest_nop); // #113 void(float fnum, string str) fputs (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 204, (builtin_t)csqc_vmrest_nop); // #204 void(float prnum, __variant newval, string varname) externset — no-op v6 (ADR 0020): variant-запись в другую прогу (типов нет в v6)
	PR1VM_RegisterBuiltin (vm, 207, (builtin_t)csqc_vmrest_nop); // #207 void(entity portal, float state) openportal
	PR1VM_RegisterBuiltin (vm, 210, (builtin_t)csqc_vmrest_nop); // #210 void() fork
	PR1VM_RegisterBuiltin (vm, 211, (builtin_t)csqc_vmrest_nop); // #211 void() abort (FTE_MULTITHREADED)
	PR1VM_RegisterBuiltin (vm, 212, (builtin_t)csqc_vmrest_nop); // #212 void() sleep
	PR1VM_RegisterBuiltin (vm, 215, (builtin_t)csqc_vmrest_nop); // #215 215 (FTE_PEXT_HEXEN2)
	PR1VM_RegisterBuiltin (vm, 216, (builtin_t)csqc_vmrest_nop); // #216 216 (FTE_PEXT_HEXEN2)
	PR1VM_RegisterBuiltin (vm, 217, (builtin_t)csqc_vmrest_nop); // #217 217 (FTE_PEXT_HEXEN2)
	PR1VM_RegisterBuiltin (vm, 219, (builtin_t)csqc_vmrest_nop); // #219 te_lightningblood void(vector org) (FTE_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 234, (builtin_t)csqc_light_nop_ret0); // #234 float(entity ent) isbackbuffered
	PR1VM_RegisterBuiltin (vm, 235, (builtin_t)csqc_vmrest_nop); // #235 void(vector angle) rotatevectorsbyangle
	PR1VM_RegisterBuiltin (vm, 236, (builtin_t)csqc_vmrest_nop); // #236 void(vector fwd, vector right, vector up) rotatevectorsbyvectors
	PR1VM_RegisterBuiltin (vm, 239, (builtin_t)csqc_vmrest_nop); // #239 void te_bloodqw(vector org[, float count]) (FTE_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 271, (builtin_t)csqc_vmrest_nop); // #271 void(float skel, float bonenum, vector org) skel_set_bone
	PR1VM_RegisterBuiltin (vm, 272, (builtin_t)csqc_vmrest_nop); // #272 void(float skel, float bonenum, vector org) skel_mul_bone
	PR1VM_RegisterBuiltin (vm, 273, (builtin_t)csqc_vmrest_nop); // #273 void(float skel, float startbone, float endbone, vector org) skel_mul_bone
	PR1VM_RegisterBuiltin (vm, 274, (builtin_t)csqc_vmrest_nop); // #274 void(float skeldst, float skelsrc, float startbone, float entbone) skel_copybones
	PR1VM_RegisterBuiltin (vm, 275, (builtin_t)csqc_vmrest_nop); // #275 void(float skel) skel_delete
	PR1VM_RegisterBuiltin (vm, 283, (builtin_t)csqc_vmrest_nop); // #283 void(entity ent, float bonenum, vector org, optional vector angorfwd, optional vector right, optional vector up) skel_set_bone_world
	PR1VM_RegisterBuiltin (vm, 288, (builtin_t)csqc_vmrest_nop); // #288 void(hashtable table) hash_destroytab
	PR1VM_RegisterBuiltin (vm, 289, (builtin_t)csqc_vmrest_nop); // #289 void(hashtable table, string name, __variant value, optional float typeandflags) hash_add
	PR1VM_RegisterBuiltin (vm, 293, (builtin_t)csqc_vmrest_nop); // #293 void() hash_getcb
	PR1VM_RegisterBuiltin (vm, 302, (builtin_t)csqc_addentity); // #302 void(entity ent) addentity (EXT_CSQC; Ф3 takeover)
	PR1VM_RegisterBuiltin (vm, 306, (builtin_t)csqc_vmrest_nop); // #306 void(string texturename) R_BeginPolygon (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 307, (builtin_t)csqc_vmrest_nop); // #307 void(vector org, vector texcoords, vector rgb, float alpha) R_PolygonVertex (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 308, (builtin_t)csqc_vmrest_nop); // #308 void() R_EndPolygon (EXT_CSQC_???)
	PR1VM_RegisterBuiltin (vm, 333, (builtin_t)csqc_setmodelindex); // #333 void(entity e, float mdlindex) setmodelindex (EXT_CSQC; Ф3)
	PR1VM_RegisterBuiltin (vm, 385, (builtin_t)csqc_vmrest_nop); // #385 void(__variant *ptr) memfree — no-op v6 (ADR 0020): типизированный указатель (нет pointer-модели в v6)
	PR1VM_RegisterBuiltin (vm, 386, (builtin_t)csqc_vmrest_nop); // #386 void(__variant *dst, __variant *src, int size) memcpy — no-op v6 (ADR 0020): типизированный указатель (нет pointer-модели в v6)
	PR1VM_RegisterBuiltin (vm, 387, (builtin_t)csqc_vmrest_nop); // #387 void(__variant *dst, int val, int size) memfill8 — no-op v6 (ADR 0020): типизированный указатель (нет pointer-модели в v6)
	PR1VM_RegisterBuiltin (vm, 389, (builtin_t)csqc_vmrest_nop); // #389 void(__variant *dst, float ofs, __variant val) memsetval — no-op v6 (ADR 0020): типизированный указатель + runtime-тип значения
	PR1VM_RegisterBuiltin (vm, 400, (builtin_t)csqc_copyentity); // #400 void(entity from, entity to) copyentity (DP_QC_COPYENTITY)
	PR1VM_RegisterBuiltin (vm, 404, (builtin_t)csqc_vmrest_nop); // #404 void(vector org, string modelname, float startframe, float endframe, float framerate) effect (DP_SV_EFFECT)
	PR1VM_RegisterBuiltin (vm, 426, (builtin_t)csqc_vmrest_nop); // #426 void(vector org) te_teleport (DP_TE_STANDARDEFFECTBUILTINS)
	PR1VM_RegisterBuiltin (vm, 432, (builtin_t)csqc_vectorvectors); // #432 void(vector dir) vectorvectors (DP_QC_VECTORVECTORS)
	PR1VM_RegisterBuiltin (vm, 433, (builtin_t)csqc_vmrest_nop); // #433 void(vector org) te_plasmaburn (DP_TE_PLASMABURN)
	PR1VM_RegisterBuiltin (vm, 443, (builtin_t)csqc_vmrest_nop); // #443 void(entity e, entity tagentity, string tagname) setattachment (DP_GFX_QUAKE3MODELTAGS)
	PR1VM_RegisterBuiltin (vm, 445, (builtin_t)csqc_vmrest_nop); // #445 void	search_end(float handle) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 457, (builtin_t)csqc_vmrest_nop); // #457 void(vector org, vector vel, float howmany) te_flamejet
	PR1VM_RegisterBuiltin (vm, 488, (builtin_t)csqc_vmrest_nop); // #488 void(string name)
	PR1VM_RegisterBuiltin (vm, 489, (builtin_t)csqc_vmrest_nop); // #489 void(string name, string URI)
	PR1VM_RegisterBuiltin (vm, 491, (builtin_t)csqc_vmrest_nop); // #491 void(string name, float x, float y)
	PR1VM_RegisterBuiltin (vm, 492, (builtin_t)csqc_vmrest_nop); // #492 void(string name, float w, float h)
	PR1VM_RegisterBuiltin (vm, 502, (builtin_t)csqc_vmrest_nop); // #502 void(float effectindex, entity own, vector org_from, vector org_to, vector dir_from, vector dir_to, float countmultiplier, optional float flags) boxparticles
	PR1VM_RegisterBuiltin (vm, 517, (builtin_t)csqc_vmrest_nop); // #517 void(strbuf strbuf) buf_cvarlist
	PR1VM_RegisterBuiltin (vm, 529, (builtin_t)csqc_vmrest_nop); // #529 void(string s) loadfromdata
	PR1VM_RegisterBuiltin (vm, 530, (builtin_t)csqc_vmrest_nop); // #530 void(string s) loadfromfile
	PR1VM_RegisterBuiltin (vm, 540, (builtin_t)csqc_vmrest_nop); // #540 void(entity e, float physics_enabled) physics_enable
	PR1VM_RegisterBuiltin (vm, 541, (builtin_t)csqc_vmrest_nop); // #541 void(entity e, vector force, vector relative_ofs) physics_addforce
	PR1VM_RegisterBuiltin (vm, 542, (builtin_t)csqc_vmrest_nop); // #542 void(entity e, vector torque) physics_addtorque
	PR1VM_RegisterBuiltin (vm, 606, (builtin_t)csqc_vmrest_nop); // #606 void(filestream fh, entity e) writetofile
	PR1VM_RegisterBuiltin (vm, 613, (builtin_t)csqc_vmrest_nop); // #613 void(entity e, string s) parseentitydata
	PR1VM_RegisterBuiltin (vm, 615, (builtin_t)csqc_vmrest_nop); // #615 void() resethostcachemasks
	PR1VM_RegisterBuiltin (vm, 616, (builtin_t)csqc_vmrest_nop); // #616 void(float mask, float fld, string str, float op) sethostcachemaskstring
	PR1VM_RegisterBuiltin (vm, 617, (builtin_t)csqc_vmrest_nop); // #617 void(float mask, float fld, float num, float op) sethostcachemasknumber
	PR1VM_RegisterBuiltin (vm, 618, (builtin_t)csqc_vmrest_nop); // #618 void() resorthostcache
	PR1VM_RegisterBuiltin (vm, 619, (builtin_t)csqc_vmrest_nop); // #619 void(float fld, float descending) sethostcachesort
	PR1VM_RegisterBuiltin (vm, 620, (builtin_t)csqc_vmrest_nop); // #620 void() refreshhostcache
	PR1VM_RegisterBuiltin (vm, 623, (builtin_t)csqc_vmrest_nop); // #623 void(string key) addwantedhostcachekey
	PR1VM_RegisterBuiltin (vm, 650, (builtin_t)csqc_vmrest_nop); // #650 void() fcopy
	PR1VM_RegisterBuiltin (vm, 651, (builtin_t)csqc_vmrest_nop); // #651 void() frename
	PR1VM_RegisterBuiltin (vm, 652, (builtin_t)csqc_vmrest_nop); // #652 void() fremove
	PR1VM_RegisterBuiltin (vm, 654, (builtin_t)csqc_vmrest_nop); // #654 void() rmtree
	PR1VM_RegisterBuiltin (vm, 741, (builtin_t)csqc_vmrest_nop); // #741 void() controller_rumble
	PR1VM_RegisterBuiltin (vm, 742, (builtin_t)csqc_vmrest_nop); // #742 void() controller_rumbletriggers
	// L2 заглушки: FLOAT0 (49) — тип-correct no-op.
	PR1VM_RegisterBuiltin (vm, 110, (builtin_t)csqc_light_nop_ret0); // #110 float(string strname, float accessmode) fopen (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 200, (builtin_t)csqc_getmodelindex); // #200 float(string modelname, optional float queryonly) getmodelindex (Ф3)
	PR1VM_RegisterBuiltin (vm, 201, (builtin_t)csqc_light_nop_ret0); // #201 __variant(float prnum, string funcname, ...) externcall — no-op v6 (ADR 0020): variant-обмен между VM (типов нет в v6)
	PR1VM_RegisterBuiltin (vm, 202, (builtin_t)csqc_light_nop_ret0); // #202 float(string progsname) addprogs
	PR1VM_RegisterBuiltin (vm, 203, (builtin_t)csqc_light_nop_ret0); // #203 __variant(float prnum, string varname) externvalue — no-op v6 (ADR 0020): variant-чтение другой проги (типов нет в v6)
	PR1VM_RegisterBuiltin (vm, 205, (builtin_t)csqc_light_nop_ret0); // #205 float() externrefcall
	PR1VM_RegisterBuiltin (vm, 206, (builtin_t)csqc_instr); // #206 string(string input, string token) instr (T3 Э4)
	PR1VM_RegisterBuiltin (vm, 237, (builtin_t)csqc_light_nop_ret0); // #237 float(float mdlindex, string skinname) skinforname
	PR1VM_RegisterBuiltin (vm, 238, (builtin_t)csqc_light_nop_ret0); // #238 float(string shadername, optional string defaultshader, ...) shaderforname
	PR1VM_RegisterBuiltin (vm, 242, (builtin_t)csqc_light_nop_ret0); // #242 void(string dest, string content) sendpacket
	PR1VM_RegisterBuiltin (vm, 263, (builtin_t)csqc_light_nop_ret0); // #263 float(float modlindex) skel_create
	PR1VM_RegisterBuiltin (vm, 264, (builtin_t)csqc_light_nop_ret0); // #264 float(float skel, entity ent, float modelindex, float retainfrac, float firstbone, float lastbone, optional float addition) skel_build
	PR1VM_RegisterBuiltin (vm, 265, (builtin_t)csqc_light_nop_ret0); // #265 float(float skel) skel_get_numbones
	PR1VM_RegisterBuiltin (vm, 267, (builtin_t)csqc_light_nop_ret0); // #267 float(float skel, float bonenum) skel_get_boneparent
	PR1VM_RegisterBuiltin (vm, 268, (builtin_t)csqc_light_nop_ret0); // #268 float(float skel, string tagname) skel_get_boneidx
	PR1VM_RegisterBuiltin (vm, 276, (builtin_t)csqc_light_nop_ret0); // #276 float(float modidx, string framename) frameforname
	PR1VM_RegisterBuiltin (vm, 277, (builtin_t)csqc_light_nop_ret0); // #277 float(float modidx, float framenum) frameduration
	PR1VM_RegisterBuiltin (vm, 281, (builtin_t)csqc_light_nop_ret0); // #281 (FTE_QC_RAGDOLL)
	PR1VM_RegisterBuiltin (vm, 282, (builtin_t)csqc_light_nop_ret0); // #282 (FTE_QC_RAGDOLL) skel_mmap — no-op v6 (ADR 0020): нативный указатель на данные скелета (нет pointer в v6)
	PR1VM_RegisterBuiltin (vm, 286, (builtin_t)csqc_light_nop_ret0); // #286 float(float resourcetype, float tryload, string resourcename) resourcestatus
	PR1VM_RegisterBuiltin (vm, 287, (builtin_t)csqc_light_nop_ret0); // #287 hashtable(float tabsize, optional float defaulttype) hash_createtab
	PR1VM_RegisterBuiltin (vm, 290, (builtin_t)csqc_light_nop_ret0); // #290 __variant(hashtable table, string name, optional __variant deflt, optional float requiretype, optional float index) hash_get
	PR1VM_RegisterBuiltin (vm, 291, (builtin_t)csqc_light_nop_ret0); // #291 __variant(hashtable table, string name) hash_delete
	PR1VM_RegisterBuiltin (vm, 356, (builtin_t)csqc_light_nop_ret0); // #356 float(string s) findfont
	PR1VM_RegisterBuiltin (vm, 357, (builtin_t)csqc_light_nop_ret0); // #357 float(string fontname, string fontmaps, string sizes, float slot, optional float fix_scale, optional float fix_voffset) loadfont
	PR1VM_RegisterBuiltin (vm, 384, (builtin_t)csqc_light_nop_ret0); // #384 __variant*(int size) memalloc — no-op v6 (ADR 0020): типизированный указатель (нет pointer-модели в v6)
	PR1VM_RegisterBuiltin (vm, 388, (builtin_t)csqc_light_nop_ret0); // #388 __variant(__variant *dst, float ofs) memgetval — no-op v6 (ADR 0020): типизированный указатель + runtime-тип значения
	PR1VM_RegisterBuiltin (vm, 390, (builtin_t)csqc_light_nop_ret0); // #390 __variant*(__variant *base, float ofs) memptradd — no-op v6 (ADR 0020): арифметика нативных указателей (нет pointer в v6)
	PR1VM_RegisterBuiltin (vm, 402, (builtin_t)csqc_findchain); // #402 entity(.string field, string match, .entity chainfield) findchain (DP_QC_FINDCHAIN)
	PR1VM_RegisterBuiltin (vm, 403, (builtin_t)csqc_findchainfloat); // #403 entity(.float fld, float match, .entity chainfield) findchainfloat (DP_QC_FINDCHAINFLOAT)
	PR1VM_RegisterBuiltin (vm, 444, (builtin_t)csqc_light_nop_ret0); // #444 float	search_begin(string pattern, float caseinsensitive, float quiet) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 446, (builtin_t)csqc_light_nop_ret0); // #446 float	search_getsize(float handle) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 449, (builtin_t)csqc_findflags); // #449 entity(entity start, .float fld, float match) findflags (DP_QC_FINDFLAGS)
	PR1VM_RegisterBuiltin (vm, 450, (builtin_t)csqc_findchainflags); // #450 entity(.float fld, float match, .entity chainfield) findchainflags (DP_QC_FINDCHAINFLAGS)
	PR1VM_RegisterBuiltin (vm, 451, (builtin_t)csqc_light_nop_ret0); // #451 float(entity ent, string tagname) gettagindex (DP_MD3_TAGSINFO)
	PR1VM_RegisterBuiltin (vm, 476, (builtin_t)csqc_strlennocol); // #476 float(string s) strlennocol
	PR1VM_RegisterBuiltin (vm, 487, (builtin_t)csqc_light_nop_ret0); // #487 float(string name)
	PR1VM_RegisterBuiltin (vm, 490, (builtin_t)csqc_light_nop_ret0); // #490 float(string name, float key, float eventtype)
	PR1VM_RegisterBuiltin (vm, 513, (builtin_t)csqc_light_nop_ret0); // #513 float(string uril, float id) uri_get
	PR1VM_RegisterBuiltin (vm, 535, (builtin_t)csqc_light_nop_ret0); // #535 float(string filename, strbuf bufhandle) buf_loadfile
	PR1VM_RegisterBuiltin (vm, 536, (builtin_t)csqc_light_nop_ret0); // #536 float(filestream filehandle, strbuf bufhandle, optional float startpos, optional float numstrings) buf_writefile
	PR1VM_RegisterBuiltin (vm, 537, (builtin_t)csqc_light_nop_ret0); // #537 float() bufstr_find
	PR1VM_RegisterBuiltin (vm, 611, (builtin_t)csqc_light_nop_ret0); // #611 float(float type) gethostcachevalue
	PR1VM_RegisterBuiltin (vm, 621, (builtin_t)csqc_light_nop_ret0); // #621 float(float fld, float hostnr) gethostcachenumber
	PR1VM_RegisterBuiltin (vm, 622, (builtin_t)csqc_light_nop_ret0); // #622 float(string key) gethostcacheindexforkey
	PR1VM_RegisterBuiltin (vm, 638, (builtin_t)csqc_light_nop_ret0); // #638 float() CL_RotateMoves
	PR1VM_RegisterBuiltin (vm, 640, (builtin_t)csqc_light_nop_ret0); // #640 float() V_CalcRefdef
	PR1VM_RegisterBuiltin (vm, 653, (builtin_t)csqc_light_nop_ret0); // #653 float() fexists
	PR1VM_RegisterBuiltin (vm, 740, (builtin_t)csqc_light_nop_ret0); // #740 float() controller_query
	// L2 заглушки: STRING (18) — тип-correct no-op.
	PR1VM_RegisterBuiltin (vm, 112, (builtin_t)csqc_nop_str); // #112 string(float fnum) fgets (FRIK_FILE)
	PR1VM_RegisterBuiltin (vm, 224, (builtin_t)csqc_strconv); // #224 string(float ccase, float redalpha, float redchars, string str, ...) strconv (FTE_STRINGS)
	PR1VM_RegisterBuiltin (vm, 266, (builtin_t)csqc_nop_str); // #266 string(float skel, float bonenum) skel_get_bonename
	PR1VM_RegisterBuiltin (vm, 284, (builtin_t)csqc_nop_str); // #284 string(float modidx, float framenum) frametoname
	PR1VM_RegisterBuiltin (vm, 285, (builtin_t)csqc_nop_str); // #285 string(float modidx, float skin) skintoname
	PR1VM_RegisterBuiltin (vm, 292, (builtin_t)csqc_nop_str); // #292 string(hashtable table, float idx) hash_getkey
	PR1VM_RegisterBuiltin (vm, 334, (builtin_t)csqc_modelnameforindex); // #334 string(float mdlindex) modelnameforindex (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 374, (builtin_t)csqc_nop_str); // #374 string(float efnum, float body) particleeffectquery
	PR1VM_RegisterBuiltin (vm, 447, (builtin_t)csqc_nop_str); // #447 string	search_getfilename(float handle, float num) (DP_QC_FS_SEARCH)
	PR1VM_RegisterBuiltin (vm, 477, (builtin_t)csqc_strdecolorize); // #477 string(string s) strdecolorize
	PR1VM_RegisterBuiltin (vm, 503, (builtin_t)csqc_nop_str); // #503 string(string filename) whichpack
	PR1VM_RegisterBuiltin (vm, 510, (builtin_t)csqc_nop_str); // #510 string(string in) uri_escape
	PR1VM_RegisterBuiltin (vm, 511, (builtin_t)csqc_nop_str); // #511 string(string in) uri_unescape
	PR1VM_RegisterBuiltin (vm, 612, (builtin_t)csqc_nop_str); // #612 string(float type, float hostnr) gethostcachestring
	PR1VM_RegisterBuiltin (vm, 624, (builtin_t)csqc_nop_str); // #624 string() getextresponse
	PR1VM_RegisterBuiltin (vm, 625, (builtin_t)csqc_nop_str); // #625 string(string dnsname, optional float defport) netaddress_resolve
	PR1VM_RegisterBuiltin (vm, 626, (builtin_t)csqc_nop_str); // #626 string() getgamedirinfo
	PR1VM_RegisterBuiltin (vm, 639, (builtin_t)csqc_nop_str); // #639 string(string digest, string data, ...) digest_hex
	// L2 заглушки: VECTOR (7) — тип-correct no-op.
	PR1VM_RegisterBuiltin (vm, 244, (builtin_t)csqc_bsp_nop_vec); // #244 vector(entity ent, float tagnum) rotatevectorsbytag
	PR1VM_RegisterBuiltin (vm, 269, (builtin_t)csqc_bsp_nop_vec); // #269 vector(float skel, float bonenum) skel_get_bonerel
	PR1VM_RegisterBuiltin (vm, 270, (builtin_t)csqc_bsp_nop_vec); // #270 vector(float skel, float bonenum) skel_get_boneabs
	PR1VM_RegisterBuiltin (vm, 310, (builtin_t)csqc_unproject); // #310 vector (vector v) unproject (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 311, (builtin_t)csqc_project); // #311 vector (vector v) project (EXT_CSQC)
	PR1VM_RegisterBuiltin (vm, 452, (builtin_t)csqc_bsp_nop_vec); // #452 vector(entity ent, float tagindex) gettaginfo (DP_MD3_TAGSINFO)
	PR1VM_RegisterBuiltin (vm, 493, (builtin_t)csqc_bsp_nop_vec); // #493 vector(string name)
}

#endif // !CLIENTONLY
