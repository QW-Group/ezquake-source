/*
csqc_client.c -- клиентская обвязка PR1VM (наш csprogs.dat), Фаза 5 (мини-каркас).

Что делает (спайк, «оверлей + статы 0-31»):
  1. При получении полного serverinfo с *csprogs / *csprogssize и битом
     FTE_PEXT_CSQC — грузит локальный csprogs.dat в статический клиентский
     инстанс PR1VM (v7-secondary16), регистрирует клиентские builtins и
     вызывает CSQC_Init.
  2. В первом 2D-кадре (ca_active) — CSQC_WorldLoaded, каждый кадр —
     CSQC_UpdateView(w,h,menushown); перед вызовом обновляет глобал time.
  3. registercommand -> Cmd_AddCommand; выполнение команды -> CSQC_ConsoleCommand.
  4. При разрыве — CSQC_Shutdown, PR1VM_UnLoad, снятие команд.

Вне скоупа мини-каркаса (следующие подшаги): парсинг 76/83/90/92, статы
32-127, реальный sendevent (clcfte_qcrequest), read*-builtins, скачивание
csprogs.dat.
*/

#ifndef CLIENTONLY
#include "quakedef.h"	// client.h (cl.stats), draw.h, vid.h, common.h (Cmd_*)
#include "keys.h"		// key_dest / key_menu
#include "pr1vm.h"
#include "csqc_client.h"
#include "pmove.h"		// playermove_t/pmove/movevars/PM_PlayerMove (C1.4 #347)
#include "common_draw.h"	// CachePic_Find/Remove, Draw_EnableScissorRectangle/DisableScissor
#include "r_texture.h"		// R_LoadPicImage/TEX_ALPHA (#318)
#include "r_matrix.h"		// R_Project3DCoordinates/R_Get*Matrix (#310/#311)
#include "gl_model.h"		// model_t mins/maxs (#504 getentity)
#include "r_renderer.h"		// R_RendererDescription (Э5 CSQC_RendererRestarted)
#include "input.h"		// CL_SendClientCommand (enablecsqc/disablecsqc, T1.6a)
#include "version.h"		// VERSION_NUM (CSQC_Init enginever, T1.6a)

// CSQC API level, который движок сообщает модулю в CSQC_Init (FTE-паритет:
// pr_common.h CSQC_API_VERSION 1.0, pr_csqc.c:8285).
#ifndef CSQC_API_VERSION
#define CSQC_API_VERSION	1.0f
#endif

// FTE-пул (слот ≠ серверный номер; план docs/archive/ezquake_csqc_client_corebuiltins_plan.md):
// CSQC_MAX_NUM — верх серверных номеров (карта номер→слот), CSQC_MAX_EDICTS — размер пула
// edict-слотов арены (слот 0 = world, не управляется). .entnum (поле модуля) = серверный
// номер; модульные spawn-сущности номера не имеют (.entnum=0).
#define CSQC_MAX_NUM	4096
#define CSQC_MAX_EDICTS	4096

// Клиентские строковые таблицы + temp-кольцо инстанса клиентской VM. Держатся
// вне shared pr1vm_t (ядро хранит только указатели на них в vm->), чтобы в
// shared-ядре не было клиентских данных/логики (mvdsv копирует ядро дословно).
// Размер кольца = числу уникальных temp-строк, живущих до перезаписи слота.
#define CSQC_TEMP_STRINGS		64
#define CSQC_TEMP_STRING_SIZE	2048
typedef struct csqc_strpool_s
{
	char	*strtbl[MAX_PRSTR];
	char	*newstrtbl[MAX_PRSTR];
	int		numstr;
	// Temp-строки deep-copy в следующий слот кольца: каждый вызов получает
	// собственный стабильный буфер (результат builtin не алиасит ни источник,
	// ни прошлые результаты; слот перезаписывается последующими вызовами).
	char	tmpstr[CSQC_TEMP_STRINGS][CSQC_TEMP_STRING_SIZE];
	int		tmpstr_cur;
} csqc_strpool_t;

typedef struct csqc_client_state_s
{
	pr1vm_t		vm;
	qbool		loaded;		// модуль загружен в инстанс
	qbool		inited;		// CSQC_Init вызван
	// C3 (Wave C): кэш cvar csqc_dbg (модуль регистрирует его в CSQC_Init через
	// registercvar; резолвим после init, чтобы не звать Cvar_Find на каждую сущность).
	cvar_t		*csqc_dbg_cvar;
	qbool		errored;	// PR_RunError на клиентском инстансе (кадры отключены)
	qbool		mayread;	// модуль вправе читать net-message — только parse-callback'и
						// (CSQC_Ent_Update/CSQC_Parse_Event; R7/T1.4a, FTE csqc_mayread)
	qbool		world_done;	// CSQC_WorldLoaded вызван
	qbool		enable_sent;	// enablecsqc/disablecsqc уже отправлен серверу
	qbool		enable_value;	// последнее отправленное состояние (true=enablecsqc, T1.6a)
	qbool		seen[CSQC_MAX_NUM];	// известные CSQC-сущности (isnew для Ent_Update)
	int			func_init, func_world, func_update, func_console, func_shutdown;
	int			func_entupdate, func_entremove, func_parseevent;
	int			func_parseprint, func_parsecp;	// Э1: CSQC_Parse_Print / CSQC_Parse_CenterPrint
	int			func_parsedamage;	// Э2: CSQC_Parse_Damage (или -1)
	int			func_eventsound;	// Э3: CSQC_Event_Sound (или -1)
	int			func_parsesetangles;	// Э4: CSQC_Parse_SetAngles (или -1)
	int			func_rr;	// Э5: CSQC_RendererRestarted (или -1)
	int			func_entspawn;	// CSQC_Ent_Spawn (или -1; R7/T1.3a, FTE-паритет)
	int			func_input;		// CSQC_Input_Frame (или -1)
	int			func_inputevent;	// CSQC_InputEvent (или -1; C1.2)
	int			func_startframe;	// CSQC StartFrame (или -1; T2.7)
	int			func_endframe;		// CSQC EndFrame (или -1; T2.7)
	int			global_time;	// смещение глобала time (или -1)
	int			global_gamespeed;	// смещение глобала gamespeed (или -1; T2.1)
	int			global_self;	// смещение глобала self (или -1; ADR 0017 P2/D3)
	int			global_other;	// смещение глобала other (или -1; T2.7 think-loop)
	int			global_physics_mode;	// смещение глобала physics_mode (или -1; T2.7)
	int			field_entnum;	// float-слово поля .entnum в entvars (или -1)
	// C1.4/C5-B #347: field-offset'ы стандартной физики (или -1).
	int			f_origin, f_velocity, f_angles, f_mins, f_maxs;
	int			f_movetype, f_flags, f_gravity, f_pmove_flags;
	int			f_modelindex, f_skin;	// #371 player/delta bridge (raw state fields)
	int			f_frame, f_effects, f_colormap, f_drawmask;	// #371 bridge (raw state fields)
	int			f_think, f_nextthink;	// T2.7 think-loop: поля .think/.nextthink (или -1)
	// FTE-пул Шаг 7 (часть 2): поля классификации трасс и зеркала игроков —
	// удалены вместе с зеркалом (окружение = FTE: без серверной эмиссии игроков
	// ezquake сущности игроков не фабрикует). Публикация player_localentnum (FTE).
	int			g_localentnum;	// глобал модуля player_localentnum (или -1)
	// input_* глобалы для CSQC_Input_Frame (или -1, если модуль их не объявил).
	int			in_timelength, in_angles, in_movevalues, in_buttons, in_impulse;
	int			in_sequence;	// input_sequence (C1.3 #345) или -1
	// C5-A: глобалы окна предикции (csdefs.qc:50-51) или -1.
	int			g_ccframe;		// clientcommandframe
	int			g_scframe;		// servercommandframe
	// C5-A/B: deprec-глобалы pmove_org/pmove_vel/pmove_onground (или -1;
	// пишет #347 в B; в A только резолв).
	int			p_org, p_vel, p_onground;
	// #1 makevectors (C6.1): глобалы v_forward/v_right/v_up модуля (или -1).
	int			g_vfwd, g_vright, g_vup;
	int			g_view_angles;	// C5-E: глобал view_angles (или -1)
	// B4: симулированные глобалы уровня FTE (или -1): frametime/cltime/maxclients/
	// player_localnum/intermission (pr_csqc.c:8818-8838).
	int			g_frametime, g_cltime, g_maxclients, g_player_localnum, g_intermission;
	// Скачивание csprogs (локально нет валидного файла): качаем *csprogsname с
	// сервера и сохраняем в csprogsvers/<crc>.dat (как FTE); загружаем после
	// появления валидного файла (см. CSQC_Client_Update).
	qbool		csprogs_dl_pending;
	// B17: таймаут — по отсутствию прогресса, а не плоские 20 c от старта.
	double		csprogs_dl_lastprogress;	// время последнего роста downloadpercent
	int			csprogs_dl_percent;			// последний виденный cls.downloadpercent
	qbool		csprogs_dl_started;			// наше скачивание уже открывалось (cls.download)
	char		csprogs_dl_localname[MAX_OSPATH];	// cls.downloadname нашего файла (гейт)
	unsigned	csprogs_crc;	// *csprogs (md4 Com_BlockChecksum) / 0 если нет
	int			csprogs_size;	// *csprogssize
	char		csprogs_dl_path[MAX_QPATH];	// локальный файл после скачивания
	int			numcmds;	// число зарегистрированных команд модуля
	int			maxcmds;	// ёмкость cmds (B18, динамическая)
	char		**cmds;		// Q_malloc: имена команд модуля (снятие при выгрузке)
	// Арена edicts клиентского инстанса (ADR 0017 P1/D2). Q_malloc, free в
	// Disconnect/Load-start; bind в vm->edicts/game_edicts (entity-опкоды).
	edict_t		*edicts;
	byte		*game_edicts;
	// Клиентские строковые таблицы инстанса (см. csqc_strpool_t): при загрузке
	// vm->strtbl/newstrtbl/numstr указывают сюда.
	csqc_strpool_t strpool;
} csqc_client_state_t;

static csqc_client_state_t s_csqc;

// Client PR1VM helpers (rule "client parts live outside shared core files"):
// LoadClientV6 + CSQCSmoke are implemented here (used to be in pr_edict.c/pr1vm.h).
static qbool PR1VM_LoadClientV6 (pr1vm_t *vm, const byte *data, int filesize);
static void PR1VM_CSQCSmoke_f (void);
static void CSQC_Client_ProgsCheck_f (void);

/*
=================
PR1VM_ClientSetString

Клиентская обёртка над единым PR1VM_SetString (core): temp-строки deep-copy в
per-instance кольцо (стабильный буфер), затем core регистрирует указатель в
vm->strtbl. Переполнение — клиентская политика (silent bail). Строки из области
строк модуля передаются в core без копии (offset). Имя с PR1VM- — работа с PR1-VM
(в отличие от PR2).
=================
*/
void PR1VM_ClientSetString (pr1vm_t *vm, int *address, char *s)
{
	csqc_strpool_t *pool;
	char *dst;

	if (!address)
		return;

	if (!s || !s[0])
	{
		*address = 0;
		return;
	}

	pool = (csqc_strpool_t *)vm->host_udata;
	if (!pool || !vm->strings || !vm->strtbl || !vm->numstr)
		return;

	// Уже область строк модуля — core запишет offset сам.
	if (s >= vm->strings && s < vm->strings + vm->progs->numstrings)
	{
		PR1VM_SetString (vm, (string_t *)address, s);
		return;
	}

	// Temp-строка: deep-copy в следующий слот кольца (буфер стабилен для
	// инстанса; слот перезаписывается последующими вызовами).
	dst = pool->tmpstr[pool->tmpstr_cur];
	pool->tmpstr_cur = (pool->tmpstr_cur + 1) % CSQC_TEMP_STRINGS;
	strlcpy (dst, s, CSQC_TEMP_STRING_SIZE);

	if (*vm->numstr + 1 >= MAX_PRSTR)
		return;	// клиент: без fatal

	PR1VM_SetString (vm, (string_t *)address, dst);
}

// C5-A #345: кольцевой буфер отправленных usercmd (запись из CL_SendCmd).
// seq = зеркало cls.netchan.outgoing_sequence (номер клиентского сообщения на
// момент записи; Netchan_Transmit инкрементирует ПОСЛЕ записи заголовка —
// net_chan.c:316-319, поэтому во время CL_SendCmd outgoing_sequence ещё равен
// номеру текущего cmd). C1.3 ввёл локальный счётчик — заменён зеркалом (C5-A).
// Размер 64 = UPDATE_BACKUP (окно предикции).
#define CSQC_INHIST	64
typedef struct { unsigned int seq; usercmd_t cmd; } csqc_inrec_t;
static csqc_inrec_t s_inhist[CSQC_INHIST];
static unsigned int s_last_seq;	// seq последней записи (0 — записей нет)
// T2.2: «живой» clientcommandframe = seq последнего собранного cmd (аналог FTE
// cl.movesequence; cl_input.c ставит его на сборке). НЕ следующая outgoing_sequence:
// Netchan_Transmit инкрементирует после отправки (net_chan.c:319), поэтому в
// render-фазе outgoing_sequence уже N+1, а FTE ccframe остаётся N (client.h:869
// «movesequence+1 … still pending»). Обновляется в CSQC_Client_InputFrame.
static unsigned int s_ccframe;	// 0 — cmd ещё не собирался

// C2.2 #460-469: пул string-buffers (DP). Строки deep-copy (переживают кадры).
#define CSQC_MAX_BUFS	64
typedef struct
{
	qbool	inuse;
	int		num;
	int		cap;
	char	**str;
} csqc_buf_t;
static csqc_buf_t s_bufs[CSQC_MAX_BUFS];

// C1.1 — #346 setsensitivityscaler: временный множитель чувствительности мыши
// (зум-аналог FTE in_sensitivityscale). Хранит модуль; применяет in_sdl2.c.
static float s_sens_scale = 1;

// Слой D шаг 3 — #343 setcursormode (A3.1): состояние курсора модуля. Пока
// usecursor=1 и модуль активен в игре (CSQC_Client_CSQCCursor), мышь свободна
// (vid_sdl2 не отдаёт её OS-курсору), а SCR_DrawCursor рисует курсор модуля.
typedef struct
{
	qbool	usecursor;
	char	cursorimage[MAX_QPATH];
	float	hotspot[2];
	float	scale;
} csqc_cursormode_t;
static csqc_cursormode_t s_cursormode;

// Клиентская арена edicts (ADR 0017 P1/D2 + FTE-пул): пул слотов произвольный,
// серверный номер хранится в .entnum (карта s_numslot: номер→слот). Слот 0 — world.
// s_own — сущность создана модулем (spawn); remove разрешён только для своих.
static qbool s_used[CSQC_MAX_EDICTS];
static qbool s_own[CSQC_MAX_EDICTS];
static int s_numslot[CSQC_MAX_NUM];
// B16: обратная карта slot→номер. Invariant: s_numslot[N] не должен переживать
// освобождение слота — иначе движок возьмёт stale-слот (review add #9).
static int s_slotnum[CSQC_MAX_EDICTS];

// Extended CSQC-статы 32..255 (clientstat/pointerstat от mvdsv). Стандартные
// 0..31 живут в cl.stats[] (клиентская структура); расширенные хранятся здесь
// (см. CSQC_Client_GetStat/SetStat). Stat wire 78/79 кладёт float/string-статы:
// statsf — точное значение (приём и из 79, и из int-пути svc_updatestat), statss —
// строка (Q_strdup, освобождается в CSQC_Client_Disconnect).
static int s_csqc_stat[MAX_EXTENDED_CL_STATS];
static float s_csqc_statsf[MAX_EXTENDED_CL_STATS];
static char *s_csqc_statss[MAX_EXTENDED_CL_STATS];

/*
=================
CSQC_Client_GetStat / SetStat / GetScreenSize / DrawText / RegisterCommand
Accessor'ы для csqc_builtins.c и cl_parse.c (см. csqc_client.h).
=================
*/
float CSQC_Client_GetStat (int idx)
{
	if (idx >= 0 && idx < 32)
		return (float)cl.stats[idx];
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
		return (float)s_csqc_stat[idx];
	return 0;
}

int CSQC_Client_GetStatInt (int idx)
{
	if (idx >= 0 && idx < 32)
		return cl.stats[idx];
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
		return s_csqc_stat[idx];
	return 0;
}

float CSQC_Client_GetStatFloat (int idx)
{
	if (idx >= 0 && idx < 32)
		return (float)cl.stats[idx];
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
		return s_csqc_statsf[idx];
	return 0;
}

const char *CSQC_Client_GetStatString (int idx)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS && s_csqc_statss[idx])
		return s_csqc_statss[idx];
	return "";
}

void CSQC_Client_SetStat (int idx, int value)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
	{
		s_csqc_stat[idx] = value;
		// Сервер при int-эмиссии float-стата держит int-кэш в синхроне
		// (sv_send.c:1199 client->stats[i]=iv) — getstatf должен видеть то же.
		s_csqc_statsf[idx] = (float)value;
	}
}

void CSQC_Client_SetStatFloat (int idx, float value)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
	{
		// Паритет FTE CL_SetStatNumeric (cl_parse.c:6110): int=(int)fvalue.
		s_csqc_statsf[idx] = value;
		s_csqc_stat[idx] = (int)value;
	}
}

void CSQC_Client_SetStatString (int idx, const char *s)
{
	if (idx >= 32 && idx < MAX_EXTENDED_CL_STATS)
	{
		Q_free (s_csqc_statss[idx]);
		s_csqc_statss[idx] = Q_strdup (s ? s : "");
	}
}

void CSQC_Client_GetScreenSize (int *w, int *h)
{
	if (w)
		*w = vid.width;
	if (h)
		*h = vid.height;
}

// T4 ^-разметка FTE -> &cRGB-раны. Контракт docs/adr/0030-csqc-strcolor-markup.md,
// ход — .opencode/plans/csqc-pr1vm-engine-strcolor-render.md (Этапы 2/3).
// Реализовано: ^0-9 (q3), ^xRGB (3 hex), ^&XY extended FG (палитра consolecolours[16];
// BG не выразим в draw-пути — doc), ^d (reset), ^s/^r (стек цвета, глубина 4 как FTE
// extstack), consume ^b/^h/^m/^a (флаги не рисуются — doc), ^^ (литерал); неизвестный/
// висячий ^ — литерал (FTE messedup, common.c:4523); &c/&r копируются.
// Вне scope (accept+doc, ADR 0030): links ^[..^], charset `u8:`/`k8:`, ^Uxxxx/^{xxxx},
// ezquakemess, визуальные эффекты blink/halfalpha/2nd charset и BG.
// Палитры — FTE consolecolours (fteqw/engine/common/common.c:3621), квантование 16
// уровней/канал (&c-ниббл); q3codemasks :3642; ^8 (half-alpha white) рисуется белым —
// alpha-отклонение. out==NULL — только подсчёт длины.
static const char *csqc_q3_nibbles[10] = {
	"000", "F55", "5F5", "FF5", "55F", "5FF", "F5F", "FFF", "FFF", "BBB"
};

// ^&XY extended FG: X/Y — индекс consolecolours[16] (fteqw/engine/common/console.h:66-81),
// квантование 4 бит/канал (как q3-таблица выше).
static const char *csqc_console_nibbles[16] = {
	"000", "00B", "0B0", "0BB", "B00", "B0B", "B50", "BBB",
	"555", "55F", "5F5", "5FF", "F55", "F5F", "FF5", "FFF"
};

static int csqc_hexval (int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	return -1;
}

// ^&XY код-символ: 0-9, A-F (FTE isextendedcode; зеркало CSQCVM_IsExtCode,
// csqc_builtins.c:3000). Возврат — индекс consolecolours 0-15 или -1 ('-'/невалид).
static int csqc_ext_index (int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static int CSQC_Client_TranslateMarkup (const char *in, char *out, size_t outsize)
{
	size_t n = 0;
	char curfg[3];		// текущий FG (&c-нибблы)
	int have_col = 0;	// 0 = default/white (=&r), 1 = цветной
	char stackfg[4][3];	// стек ^s/^r (FTE extstack, глубина 4)
	int stackcol[4];
	int sp = 0;

	if (!in)
		in = "";
	// PUT: копирует байт, безопасно для out==NULL (только длина) и переполнения.
#define PUT(ch) do { if (out && outsize && n + 1 < outsize) out[n] = (char)(ch); n++; } while (0)
	// SETFG: запомнить текущий FG (для ^s/^r).
#define SETFG(p) do { curfg[0] = (p)[0]; curfg[1] = (p)[1]; curfg[2] = (p)[2]; have_col = 1; } while (0)
	for (; *in; in++)
	{
		if (in[0] == '^' && in[1] >= '0' && in[1] <= '9')
		{
			const char *p = csqc_q3_nibbles[in[1] - '0'];
			PUT ('&'); PUT ('c'); PUT (p[0]); PUT (p[1]); PUT (p[2]);
			SETFG (p);
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == 'x')
		{
			if (csqc_hexval (in[2]) >= 0 && csqc_hexval (in[3]) >= 0 && csqc_hexval (in[4]) >= 0)
			{
				PUT ('&'); PUT ('c'); PUT (in[2]); PUT (in[3]); PUT (in[4]);
				SETFG (in + 2);
				in += 4;
			}
			else
			{
				// невалидный hex: скипаем "^x" целиком (FTE str+=2, common.c:4413;
				// паритет со стрипом #476/#477, ADR 0030) — 'x' не рисуется.
				in += 1;
			}
			continue;
		}
		if (in[0] == '^' && in[1] == '&')
		{
			// ^&XY extended FG/BG (FTE common.c:4177-4207): реализуем FG (BG не выразим
			// в draw-пути — отклонение, ADR 0030); Y игнорируется.
			if ((csqc_ext_index (in[2]) >= 0 || in[2] == '-') &&
				(csqc_ext_index (in[3]) >= 0 || in[3] == '-'))
			{
				if (in[2] == '-')
				{
					// default FG = white (FTE COLOR_WHITE, common.c:4186)
					PUT ('&'); PUT ('r');
					have_col = 0;
				}
				else
				{
					const char *p = csqc_console_nibbles[csqc_ext_index (in[2])];
					PUT ('&'); PUT ('c'); PUT (p[0]); PUT (p[1]); PUT (p[2]);
					SETFG (p);
				}
				in += 3;
			}
			else
			{
				// невалид: '^' литерал, '&' на след. итерации (FTE messedup)
				PUT (*in);
			}
			continue;
		}
		if (in[0] == '^' && in[1] == 'd')
		{
			PUT ('&'); PUT ('r');
			have_col = 0;
			in += 1;
			continue;
		}
		if (in[0] == '^' && (in[1] == 'b' || in[1] == 'h' || in[1] == 'm' || in[1] == 'a'))
		{
			// FTE toggle blink/halfalpha/2nd charset (common.c:4290-4304); флаги не
			// выразимы в draw-пути — код потребляем (паритет ширины), эффект — doc.
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == 's')
		{
			// push стека (FTE extstack, common.c:4305-4312); храним только цвет.
			if (sp < (int)(sizeof (stackcol) / sizeof (stackcol[0])))
			{
				stackfg[sp][0] = curfg[0];
				stackfg[sp][1] = curfg[1];
				stackfg[sp][2] = curfg[2];
				stackcol[sp] = have_col;
				sp++;
			}
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == 'r')
		{
			// pop стека (FTE common.c:4313-4320): восстановить цвет.
			if (sp > 0)
			{
				sp--;
				if (stackcol[sp])
				{
					const char *p = stackfg[sp];
					PUT ('&'); PUT ('c'); PUT (p[0]); PUT (p[1]); PUT (p[2]);
					SETFG (p);
				}
				else
				{
					PUT ('&'); PUT ('r');
					have_col = 0;
				}
			}
			in += 1;
			continue;
		}
		if (in[0] == '^' && in[1] == '^')
		{
			PUT ('^');
			in += 1;
			continue;
		}
		// неизвестный/висячий '^' — литерал (FTE messedup, common.c:4523): '^' в out,
		// следующий символ обрабатывается на след. итерации.
		PUT (*in);
	}
	if (out && outsize)
		out[(n < outsize) ? n : outsize - 1] = 0;
	return (int)n;
#undef PUT
#undef SETFG
}

void CSQC_Client_DrawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale)
{
	extern cvar_t scr_coloredText;
	static char buf[16384];
	int prefix;
	float saved;
	if (!text)
		return;
	// Слой D шаг 2: масштаб шрифта из size.x (scale=size.x/8; 0 => 1). Цвет
	// модуля передаём &cRRGGBB-кодом движка. Чтобы он не зависел от
	// scr_coloredText пользователя, временно включаем его на время отрисовки.
	saved = scr_coloredText.value;
	Cvar_SetValue (&scr_coloredText, 1);
	// Цвет &cRGB — 3 hex-разряда (канал×16), а не &cRRGGBB.
	prefix = snprintf (buf, sizeof (buf), "&c%X%X%X",
		(bound (0, r, 255)) / 16, (bound (0, g, 255)) / 16, (bound (0, b, 255)) / 16);
	if (prefix < 0)
		prefix = 0;
	// T4: ^-разметка модуля -> &cRGB-раны после базового цвета (см. транслятор выше).
	CSQC_Client_TranslateMarkup (text, buf + prefix, sizeof (buf) - prefix);
	// B15 (FTE-паритет drawcolouredstring, pr_menu.c:565): alpha применяется
	// (R2D_ImageColours(...,alpha)); color=NULL -> цвет берётся из &c-кодов.
	Draw_SColoredAlphaString (x, y, buf, NULL, 0, 0, (scale > 0) ? scale : 1,
		bound (0, alpha, 1), true);
	Cvar_SetValue (&scr_coloredText, saved);
}

// Цвет для draw-помощников Слоя D (rgb 0..255 байты, alpha 0..1).
static color_t CSQC_Client_Color (int r, int g, int b, float alpha)
{
	return RGBA_TO_COLOR ((byte)bound (0, r, 255), (byte)bound (0, g, 255),
		(byte)bound (0, b, 255), (byte)bound (0, (int)(alpha * 255.0f + 0.5f), 255));
}

/*
#324 drawsetcliparea / #325 drawresetcliparea — геометрическое отсечение (решение
2026-09-07; аппаратный GL-scissor на отложенном 2D-пайплайне ezq не применим).
Состояние clip-прямоугольника в координатах CSQC-рисования; прямоугольные
примитивы (pic/subpic/fill) пересекаются с ним, текст/линии только не рисуются,
если целиком вне (строки внутри не режутся) — отклонение в parity.
*/
static qbool s_clip_on = false;
static float s_clip_x, s_clip_y, s_clip_w, s_clip_h;

// Пересекает dest-rect (x,y,w,h) с активным clip. Возврат false = пусто/вне.
static qbool CSQC_Client_ClipDest (float *x, float *y, float *w, float *h)
{
	float x0, y0, x1, y1;
	if (!s_clip_on)
		return true;
	x0 = *x; y0 = *y;
	x1 = *x + *w; y1 = *y + *h;
	if (x1 <= s_clip_x || x0 >= s_clip_x + s_clip_w ||
		y1 <= s_clip_y || y0 >= s_clip_y + s_clip_h)
		return false;
	x0 = (x0 > s_clip_x) ? x0 : s_clip_x;
	y0 = (y0 > s_clip_y) ? y0 : s_clip_y;
	x1 = (x1 < s_clip_x + s_clip_w) ? x1 : s_clip_x + s_clip_w;
	y1 = (y1 < s_clip_y + s_clip_h) ? y1 : s_clip_y + s_clip_h;
	*x = x0; *y = y0; *w = x1 - x0; *h = y1 - y0;
	return true;
}

/*
=================
Слой D, шаг 1 — 2D-графика (docs/archive/ezquake_csqc_client_layerd_2d_plan.md).
Координаты/размеры — сырые пиксели видео (как DrawText). drawpic: rgb-tint
игнорируется (только alpha; решение R2), масштаб = size / нативный размер.
=================
*/
void CSQC_Client_DrawFill (float x, float y, float w, float h, int r, int g, int b, float alpha)
{
	if (w <= 0 || h <= 0)
		return;
	if (!CSQC_Client_ClipDest (&x, &y, &w, &h))
		return;
	Draw_AlphaRectangleRGB (x, y, w, h, 1, true, CSQC_Client_Color (r, g, b, alpha));
}

qbool CSQC_Client_DrawPic (float x, float y, float w, float h, const char *name, int r, int g, int b, float alpha)
{
	mpic_t *pic;
	float sx, sy, a = bound (0, alpha, 1);
	float dx, dy, dw, dh, srcx, srcy, srcw, srch;
	if (!name || !name[0])
		return false;
	pic = Draw_CachePicSafe (name, false, false);
	if (!pic)
		return false;
	// FTE-паритет #322: отрицательный размер — зеркалирование. Знаковый scale_x/y
	// разворачивает текстуру (texcoord привязан к вершине в R_DrawImage); клип не
	// применяем (отрицательный dest ломает пересечение).
	if (w < 0 || h < 0)
	{
		sx = w / (float)pic->width;
		sy = h / (float)pic->height;
		if (r == 255 && g == 255 && b == 255)
			Draw_SAlphaSubPic2 (x, y, pic, 0, 0, pic->width, pic->height, sx, sy, a);
		else
			Draw_SColoredSubPic2 (x, y, pic, 0, 0, pic->width, pic->height, sx, sy,
				bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
		return true;
	}
	if (w == 0 || h == 0)
		return true;
	// Клип: пересечение dest с активной областью, источник пересчитывается
	// (свойство «весь pic → dest» сохраняется).
	dx = x; dy = y; dw = w; dh = h;
	if (!CSQC_Client_ClipDest (&dx, &dy, &dw, &dh))
		return true;
	sx = w / (float)pic->width;
	sy = h / (float)pic->height;
	srcx = (dx - x) / sx;
	srcy = (dy - y) / sy;
	srcw = dw / sx;
	srch = dh / sy;
	if (r == 255 && g == 255 && b == 255)
		Draw_SAlphaSubPic2 (dx, dy, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, sx, sy, a);
	else
		Draw_SColoredSubPic2 (dx, dy, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, sx, sy,
			bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
	return true;
}

void CSQC_Client_DrawSubPic (float x, float y, float w, float h, const char *name, float srcx, float srcy, float srcw, float srch, int r, int g, int b, float alpha)
{
	mpic_t *pic;
	float a = bound (0, alpha, 1);
	float dx, dy, dw, dh, nsx, nsy, nsw, nsh, ssx, ssy;
	if (!name || !name[0] || srcw <= 0 || srch <= 0)
		return;
	pic = Draw_CachePicSafe (name, false, false);
	if (!pic)
		return;
	// FTE-паритет #328: отрицательный размер одной оси — зеркалирование (знаковый scale).
	if (w < 0 || h < 0)
	{
		ssx = w / srcw;
		ssy = h / srch;
		if (r == 255 && g == 255 && b == 255)
			Draw_SAlphaSubPic2 (x, y, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, ssx, ssy, a);
		else
			Draw_SColoredSubPic2 (x, y, pic, (int)srcx, (int)srcy, (int)srcw, (int)srch, ssx, ssy,
				bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
		return;
	}
	if (w == 0 || h == 0)
		return;
	// Клип как в DrawPic: dest пересекается, источник — по аффинному маппингу.
	dx = x; dy = y; dw = w; dh = h;
	if (!CSQC_Client_ClipDest (&dx, &dy, &dw, &dh))
		return;
	ssx = w / srcw;
	ssy = h / srch;
	nsx = srcx + ((dx - x) / w) * srcw;
	nsy = srcy + ((dy - y) / h) * srch;
	nsw = (dw / w) * srcw;
	nsh = (dh / h) * srch;
	if (r == 255 && g == 255 && b == 255)
		Draw_SAlphaSubPic2 (dx, dy, pic, (int)nsx, (int)nsy, (int)nsw, (int)nsh, ssx, ssy, a);
	else
		Draw_SColoredSubPic2 (dx, dy, pic, (int)nsx, (int)nsy, (int)nsw, (int)nsh, ssx, ssy,
			bound (0, r, 255), bound (0, g, 255), bound (0, b, 255), a);
}

void CSQC_Client_DrawCharacter (float x, float y, int ch, int r, int g, int b, float alpha, float scale)
{
	extern cvar_t scr_coloredText;
	static char buf[8];
	float saved;
	int c = ch & 0xff;
	if (c <= 0)
		return;
	// Один символ default-шрифта с цветом &cRGB (как DrawText).
	saved = scr_coloredText.value;
	Cvar_SetValue (&scr_coloredText, 1);
	snprintf (buf, sizeof (buf), "&c%X%X%X%c",
		(bound (0, r, 255)) / 16, (bound (0, g, 255)) / 16, (bound (0, b, 255)) / 16, c);
	// B15: alpha как FTE drawcharacter (pr_menu.c:1009).
	Draw_SColoredAlphaString (x, y, buf, NULL, 0, 0, (scale > 0) ? scale : 1,
		bound (0, alpha, 1), true);
	Cvar_SetValue (&scr_coloredText, saved);
}

void CSQC_Client_DrawLine (float x1, float y1, float x2, float y2, float width, int r, int g, int b, float alpha)
{
	if (width <= 0)
		return;
	Draw_AlphaLineRGB (x1, y1, x2, y2, width, CSQC_Client_Color (r, g, b, alpha));
}

float CSQC_Client_StringWidth (const char *text, qbool usecolours, float fontsize_x)
{
	static char wbuf[16384];
	float scale;
	if (!text)
		return 0;
	// Масштаб из size.x (как DrawText; 0 => 1) — та же метрика, что рисует
	// drawstring: r_draw_charset.c Draw_StringLength/Colors.
	scale = (fontsize_x > 0) ? fontsize_x / 8.0f : 1;
	if (usecolours)
	{
		// T4: ^-коды не считаются символами ширины (FTE #327 со снятым markup) —
		// та же трансляция, что в DrawText, затем Draw_StringLengthColors пропускает &c/&r.
		CSQC_Client_TranslateMarkup (text, wbuf, sizeof (wbuf));
		return Draw_StringLengthColors (wbuf, -1, scale, true);
	}
	// usecolours=0: FTE #327 держит markup (keepmarkup) и считает его символы — оригинал.
	return Draw_StringLength (text, -1, scale, true);
}

qbool CSQC_Client_PrecachePic (const char *name)
{
	if (!name || !name[0])
		return false;
	return Draw_CachePicSafe (name, false, false) != NULL;
}

// Слой L2 — «2D-графика доп» (2026-09-07; #316/#318/#319/#321/#324/#325/#329).

qbool CSQC_Client_IsCachedPic (const char *name)
{
	if (!name || !name[0])
		return false;
	return CachePic_Find (name, false) != NULL;
}

qbool CSQC_Client_PicSize (const char *name, float *w, float *h)
{
	mpic_t *pic;
	const char *ext;
	char path[MAX_QPATH];
	if (!name || !name[0])
		return false;
	// #318 FTE-паритет (PF_CL_drawgetimagesize, pr_menu.c:1093): R2D_SafeCachePic +
	// R_GetShaderSizes. FTE резолвит имя через Image_GetTexture extension-fallback
	// (r_imageextensions + COM_DefaultExtension(".lmp")) — bare-имя тоже резолвится
	// (parity-audit 2026-09-22, задача A). ".lmp" читаем из заголовка напрямую
	// (ezq Draw_CachePicSafe на .lmp-пути отдаёт чужой размер).
	ext = COM_FileExtension (name);
	if (!ext || !ext[0])
	{
		strlcpy (path, name, sizeof (path));
		COM_DefaultExtension (path, ".lmp", sizeof (path));
		name = path;
		ext = COM_FileExtension (name);
	}
	if (!strcasecmp (ext, "lmp"))
	{
		// Заголовок .lmp (qpic_t): два int LE (см. SwapPic/LittleLong). wad.h не
		// тянем (требует texture_t) — читаем заголовок напрямую.
		byte *data = FS_LoadTempFile ((char *)name, NULL);
		int iw, ih;
		if (!data)
			return false;
		memcpy (&iw, data, sizeof (iw));
		memcpy (&ih, data + sizeof (iw), sizeof (ih));
		if (w)
			*w = (float)LittleLong (iw);
		if (h)
			*h = (float)LittleLong (ih);
		return true;
	}
	pic = R_LoadPicImage (name, NULL, 0, 0, TEX_ALPHA);
	if (!pic)
		return false;
	if (w)
		*w = (float)pic->width;
	if (h)
		*h = (float)pic->height;
	return true;
}

void CSQC_Client_DrawRawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale)
{
	char one[2];
	const char *p;
	float xx;
	if (!text)
		return;
	// «Сырой» вывод: каждый символ рисуется одиночным цветным глифом — внутри
	// одной строки нет места для сборки &cRGB, поэтому & в тексте модуля
	// выводится литерально (как FTE drawrawstring). Цвет и alpha применяются
	// (FTE drawrawstring, pr_menu.c:1039).
	xx = x;
	for (p = text; *p; p++)
	{
		one[0] = *p;
		one[1] = 0;
		CSQC_Client_DrawCharacter (xx, y, (int)(unsigned char)*p, r, g, b, alpha, scale);
		xx += Draw_StringLength (one, 1, (scale > 0) ? scale : 1, true);
	}
}

/*
#324 drawsetcliparea / #325 drawresetcliparea — геометрический clip (состояние),
без аппаратного scissor/flush (см. комментарий к CSQC_Client_ClipDest).
*/
void CSQC_Client_SetClipArea (float x, float y, float w, float h)
{
	s_clip_x = x;
	s_clip_y = y;
	s_clip_w = (w > 0) ? w : 0;
	s_clip_h = (h > 0) ? h : 0;
	s_clip_on = true;
}

void CSQC_Client_ResetClipArea (void)
{
	s_clip_on = false;
}

void CSQC_Client_SetCursorMode (qbool usecursor, const char *image,
	float hotspot_x, float hotspot_y, float scale)
{
	// Полная реализация (roadmap A3.1): запоминаем параметры; эффект включается
	// самим состоянием CSQC_Client_CSQCCursor() — пока usecursor=1 и модуль активен
	// в игре, mouse-механика ezquake не отдаёт мышь OS-курсору (vid_sdl2.c), а
	// SCR_DrawCursor рисует курсор модуля. Клики/InputEvent-канал — C1.
	s_cursormode.usecursor = usecursor;
	s_cursormode.cursorimage[0] = 0;
	if (image)
		strlcpy (s_cursormode.cursorimage, image, sizeof (s_cursormode.cursorimage));
	s_cursormode.hotspot[0] = hotspot_x;
	s_cursormode.hotspot[1] = hotspot_y;
	s_cursormode.scale = scale;
}

qbool CSQC_Client_CSQCCursor (void)
{
	// Курсор модуля действует только в игровом кадре (key_game): при открытом
	// консоль/меню движка их собственный курсор/мышь имеют приоритет.
	return s_cursormode.usecursor && s_csqc.loaded && !s_csqc.errored
		&& key_dest == key_game;
}

// B14 (FTE-паритет): позиция/дельта мыши выдаются в vid.conwidth-единицах (как
// Draw_* и контракт IE_MOUSEABS), тогда как cursor_x/y и mx/my — в render-2D
// (VID_RenderWidth2D). FTE масштабирует *vid.width/vid.pixelwidth
// (pr_csqc.c:9053 MOUSEABS, :9070 MOUSEDELTA). Эталон конверсии — SCR_UpdateCursor
// (cl_screen.c:668-673).
static float CSQC_Client_CursorScaleX (void)
{
	int rw = VID_RenderWidth2D ();
	return (rw > 0) ? (float)vid.conwidth / (float)rw : 1.0f;
}

static float CSQC_Client_CursorScaleY (void)
{
	int rh = VID_RenderHeight2D ();
	return (rh > 0) ? (float)vid.conheight / (float)rh : 1.0f;
}

void CSQC_Client_ScaleCursorDelta (float *x, float *y)
{
	if (x)
		*x *= CSQC_Client_CursorScaleX ();
	if (y)
		*y *= CSQC_Client_CursorScaleY ();
}

void CSQC_Client_GetCursorPos (float *x, float *y)
{
	extern double cursor_x, cursor_y;	// cl_screen.c:164 (render-2D координаты указателя)
	if (x)
		*x = (float)cursor_x * CSQC_Client_CursorScaleX ();
	if (y)
		*y = (float)cursor_y * CSQC_Client_CursorScaleY ();
}

void CSQC_Client_SetSensitivityScale (float scale)
{
	// C1.1 #346: множитель чувствительности (может быть 0); дефолт 1.
	s_sens_scale = scale;
}

float CSQC_Client_SensitivityScale (void)
{
	// Неактивный модуль — без влияния (default 1).
	if (!s_csqc.loaded || s_csqc.errored)
		return 1;
	return s_sens_scale;
}

void CSQC_Client_DrawCursor (void)
{
	extern double cursor_x, cursor_y;
	mpic_t *pic;
	float scale, x, y;

	if (!CSQC_Client_CSQCCursor ())
		return;
	// FTE: scale <= 0 -> 1; hotspot — «остриё» курсора в пикселях картинки
	// (умножается на масштаб), т.е. позиция указывает на точку клика.
	// B14: позиция курсора — в vid.conwidth-единицах (как Draw_*), а cursor_x/y —
	// в render-2D; hotspot остаётся пиксельным (FTE in_generic.c).
	scale = (s_cursormode.scale > 0) ? s_cursormode.scale : 1;
	x = (float)cursor_x * CSQC_Client_CursorScaleX () - s_cursormode.hotspot[0] * scale;
	y = (float)cursor_y * CSQC_Client_CursorScaleY () - s_cursormode.hotspot[1] * scale;

	if (s_cursormode.cursorimage[0])
	{
		pic = Draw_CachePicSafe (s_cursormode.cursorimage, false, false);
		if (!pic)
			pic = Draw_CachePicSafe (s_cursormode.cursorimage, false, true);	// tga/png
		if (pic)
		{
			Draw_SColoredSubPic2 (x, y, pic, 0, 0, pic->width, pic->height,
				scale, scale, 255, 255, 255, 1);
			return;
		}
	}
	// Без картинки — дефолтное перекрестие (визуально как ezquake-курсор).
	{
		color_t c = RGBA_TO_COLOR (0, 255, 0, 255);
		float s = scale;
		Draw_AlphaLineRGB (x + 4 * s, y + 4 * s, x + 16 * s, y + 16 * s, 2 * s, c);
		Draw_AlphaLineRGB (x, y, x + 8 * s, y, 2 * s, c);
		Draw_AlphaLineRGB (x, y, x, y + 8 * s, 2 * s, c);
		Draw_AlphaLineRGB (x + 8 * s, y, x, y + 8 * s, 2 * s, c);
	}
}

static void CSQC_Client_ConsoleCommand_f (void);

void CSQC_Client_RegisterCommand (const char *cmd)
{
	int i;
	char *name;

	if (!cmd || !cmd[0])
		return;
	for (i = 0; i < s_csqc.numcmds; i++)
		if (!strcmp (s_csqc.cmds[i], cmd))
			return;					// уже зарегистрирована

	// B18 (FTE-parity, unlimited): динамический список вместо прежнего cap [16][64]
	// (fteqw PF_cs_registercommand, pr_csqc.c:5426 -> Cmd_AddCommandD без лимита).
	if (s_csqc.numcmds >= s_csqc.maxcmds)
	{
		int newmax = s_csqc.maxcmds ? s_csqc.maxcmds * 2 : 16;
		s_csqc.cmds = (char **)Q_realloc (s_csqc.cmds, newmax * sizeof (s_csqc.cmds[0]));
		s_csqc.maxcmds = newmax;
	}

	// Cmd_AddRemCommand копирует имя в Q_malloc-блок (в отличие от
	// Cmd_AddCommand, который держит указатель на имя и аллоцит узел в hunk).
	// Узел/имя переживают Host_ClearMemory и корректно удаляются RemoveCommand.
	// Своя копия нужна для снятия команды при выгрузке модуля.
	name = Q_strdup (cmd);
	if (!Cmd_AddRemCommand (name, CSQC_Client_ConsoleCommand_f))
	{
		Q_free (name);
		return;
	}
	s_csqc.cmds[s_csqc.numcmds] = name;
	s_csqc.numcmds++;
}

/*
=================
CSQC_Client_RegisterCommands

Registers client debug commands for PR1VM (csqc_smoke, etc.). Called from
CL_InitLocal (cl_main.c) — commands available in the client console. csqc_smoke
used to be registered in PR2_Init (server); moved here per the rule "client parts
live outside shared core files" (docs/archive/ezquake_csqc_client_pr1vm_plan.md).
=================
*/
void CSQC_Client_RegisterCommands (void)
{
	Cmd_AddCommand ("csqc_smoke", PR1VM_CSQCSmoke_f);	// PR1VM S3 debug
	Cmd_AddCommand ("csqc_progscheck", CSQC_Client_ProgsCheck_f);	// A3 debug canary
}

/*
=================
host-колбэки клиентского инстанса
=================
*/
static void CSQC_Client_HostPrint (pr1vm_t *vm, const char *msg)
{
	(void)vm;
	Con_Printf ("%s\n", msg);
}

static void CSQC_Client_HostError (pr1vm_t *vm, const char *msg)
{
	Con_Printf ("CSQC (PR1VM) program error: %s\n", msg);
	s_csqc.errored = true;
	// A1 (abort-stack): do not return into the interpreter — unwind back to the
	// outermost active setjmp in PR1VM_ExecuteProgram (the client VM always has
	// abortbuf_valid set). Frames stay disabled (errored); reload on the next
	// ConnectCheck.
	if (vm && vm->abortbuf_valid && vm->abortbuf)
		longjmp (*vm->abortbuf, 1);
	// No abort-buffer: fall back to the previous behavior (return; the caller
	// PR_RunError then takes the fatal path).
}

/*
=================
CSQC_Client_Abort

Фатальная ошибка модуля (паритет FTE CSQC_Abort → Host_EndGame): печатаем
причину и отключаем клиента от сервера (дисконнект, возврат в меню), затем
Host_Abort (longjmp в Host_Frame) — не возвращаемся в исполняемую VM.
errored ставим ДО CL_Disconnect, чтобы CSQC_Client_Disconnect не звал
func_shutdown реентерабельно (мы сами внутри исполняемой VM).
=================
*/
void CSQC_Client_Abort (const char *msg)
{
	Con_Printf ("CSQC (PR1VM) fatal: %s\n", msg ? msg : "fatal");
	s_csqc.errored = true;
	CL_Disconnect ();
	Host_Abort ();
}

/*
=================
CSQC_Client_GetString

Ограниченное чтение строк клиентской VM (недоверенный скачанный csprogs.dat):
положительный offset обязан лежать в блоке строк модуля; всё, что дальше —
подделанное значение и даёт NULL (вызывающие мапят в ""/пропуск). Общий
PR1VM_GetString остаётся без границы (серверные/карты-строки живут за
numstrings), поэтому граница клиента живёт здесь и ставится как vm->get_string
(ADR 0019, option 2).
=================
*/
char *CSQC_Client_GetString (pr1vm_t *vm, int num)
{
	if (num >= 0 && (!vm || !vm->progs || num >= vm->progs->numstrings))
		return NULL;
	return PR1VM_GetString (vm, num);
}

/*
=================
Внутренние помощники
=================
*/
// B4: map-uptime клиента и предыдущий cl.time для frametime (FTE
// pr_csqc.c:8818-8838: frametime = bound(0, cl.time - cl.lasttime, 0.1),
// cltime = realtime - cl.mapstarttime).
static double s_mapstarttime;
static double s_prev_cltime;

// B4: симулированное серверное время модуля. FTE: *csqcg.time = cl.servertime
// (pr_csqc.c:8839-8840); если сервер не шлёт STAT_TIME/svc_time — клиентский
// map-uptime (тот же часовой домен, что cltime).
static double CSQC_Client_TimeNow (void)
{
	if (cl.servertime_works)
		return cl.servertime;
	return cls.realtime - s_mapstarttime;
}

static void CSQC_Client_SetTime (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (s_csqc.global_time >= 0)
		vm->globals[s_csqc.global_time] = (float)CSQC_Client_TimeNow ();
}

// B21 (FTE CSQC_StateOp pr_csqc.c:7868-7875): OP_STATE клиентского модуля — по
// его собственным field/global-офсетам (не по фикс. entvars_t/globalvars_t).
// self — арена-слот (движок пишет raw = slot*edict_size); поля — через резолв
// модуля (f_nextthink/f_frame/f_think), time — через global_time.
static void CSQC_Client_StateOp (pr1vm_t *vm, float frame, func_t func)
{
	float *v;
	int slot = 0;

	if (s_csqc.global_self >= 0)
		slot = CSQC_Client_EntNum (vm, *(int *)&vm->globals[s_csqc.global_self]);
	if (slot < 0 || slot >= vm->max_edicts)
		return;
	v = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);

	if (s_csqc.f_nextthink >= 0)
		v[s_csqc.f_nextthink] = ((s_csqc.global_time >= 0)
			? vm->globals[s_csqc.global_time] : 0.0f) + 0.1f;
	if (s_csqc.f_frame >= 0 && frame != v[s_csqc.f_frame])
		v[s_csqc.f_frame] = frame;
	if (s_csqc.f_think >= 0)
		*(int *)&v[s_csqc.f_think] = (int)func;
}

static qbool CSQC_Client_Exec (int fidx)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (fidx <= 0 || fidx >= vm->progs->numfunctions)
		return false;
	CSQC_Client_SetTime ();
	PR1VM_ExecuteProgram (vm, (func_t)fidx);
	return !s_csqc.errored;
}

/* C4 Э3: как CSQC_Client_Exec, но возвращает G_FLOAT(OFS_RETURN) модуля
 * (для delta-callback: возврат != 0 = «движок не рисует сущность»). */
static qbool CSQC_Client_ExecRet (int fidx, float *ret)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (ret)
		*ret = 0;
	if (fidx <= 0 || fidx >= vm->progs->numfunctions)
		return false;
	CSQC_Client_SetTime ();
	PR1VM_ExecuteProgram (vm, (func_t)fidx);
	if (ret)
		*ret = vm->globals[OFS_RETURN];
	return !s_csqc.errored;
}

static void CSQC_Client_ClearCommands (void)
{
	int i;
	for (i = 0; i < s_csqc.numcmds; i++)
	{
		Cmd_RemoveCommand (s_csqc.cmds[i]);
		Q_free (s_csqc.cmds[i]);
	}
	s_csqc.numcmds = 0;
	if (s_csqc.cmds)
	{
		Q_free (s_csqc.cmds);
		s_csqc.cmds = NULL;
	}
	s_csqc.maxcmds = 0;
}

/*
=================
CSQC_Client_ConsoleCommand_f

Команда, зарегистрированная модулем через registercommand. Восстанавливаем
полную строку («name arg1 arg2 …») и зовём CSQC_ConsoleCommand(string cmd).
=================
*/
static void CSQC_Client_ConsoleCommand_f (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	const char *line;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.func_console <= 0)
		return;

	if (Cmd_Argc () > 1)
		line = va ("%s %s", Cmd_Argv (0), Cmd_Args ());
	else
		line = Cmd_Argv (0);

	CSQC_Client_SetTime ();
	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM0], (char *)line);
	vm->globals[OFS_RETURN] = 0;
	PR1VM_ExecuteProgram (vm, (func_t)s_csqc.func_console);
}

/*
=================
CSQC_Client_Active
=================
*/
int CSQC_Client_Active (void)
{
	return (s_csqc.loaded && !s_csqc.errored) ? 1 : 0;
}

/*
=================
Ф3 (renderscene takeover)

Модуль владеет 3D-сценой как в FTE: когда модуль активен, CSQC_UpdateView
вызывается в 3D-фазе (SCR_UpdateScreenPlayerView) вместо R_RenderView();
#300 clearscene / #301 addentities наполняют cl_visents; #304 renderscene
выполняет R_RenderView(). Флаг s_scene_rendered — защита от чёрного экрана:
если модуль не позвал renderscene, движок рисует кадр сам (fallback).
=================
*/
static qbool s_scene_rendered = false;
static qbool s_scene_viewmodel = false;	// C4 Э2: #301 mask&MASK_STDVIEWMODEL запрошен

qbool CSQC_Client_SceneActive (void)
{
	return s_csqc.loaded && s_csqc.inited && !s_csqc.errored && s_csqc.func_update > 0;
}

void CSQC_Client_BeginScene (void)
{
	s_scene_rendered = false;
	s_scene_viewmodel = false;
}

// C4 Э2 (#301 mask&2): модуль запросил движковую вьюмодель в сцене (FTE CL_LinkViewModel).
void CSQC_Client_LinkViewModel (void)
{
	s_scene_viewmodel = true;
}

qbool CSQC_Client_SceneViewModel (void)
{
	return s_scene_viewmodel;
}

void CSQC_Client_RenderScene (void)
{
	s_scene_rendered = true;
	R_RenderView ();
}

qbool CSQC_Client_SceneRendered (void)
{
	return s_scene_rendered;
}

/*
=================
C4 Этап 1: CSQC_Client_CallPredraw

Вызов .predraw эдикта арены при #301/#302 (FTE PF_R_AddEntityMask, pr_csqc.c:1450-1457):
self = slot*edict_size, исполнение, возврат G_FLOAT(OFS_RETURN). Модуль через возврат решает
авто-добавление (PREDRAW_AUTOADD=0) или пропуск (!=0). Если predraw удалил эдикт или исполнение
упало — *removed=1 (не добавлять). self восстанавливается (как FTE `*csqcg.self = oldself`).
.entnum не трогаем (в отличие от SetContextSlot — FTE тоже не переписывает его в addentities).
=================
*/
float CSQC_Client_CallPredraw (int slot, int fidx, qbool *removed)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;
	int oldself = 0;

	if (removed)
		*removed = false;
	if (!vm || !vm->game_edicts || fidx <= 0)
		return 0;
	if (!CSQC_Client_EntUsed (slot))
	{
		if (removed)
			*removed = true;
		return 0;
	}

	if (s_csqc.global_self >= 0)
	{
		oldself = *(int *)&vm->globals[s_csqc.global_self];
		*(int *)&vm->globals[s_csqc.global_self] = (int)slot * vm->edict_size;
	}

	if (CSQC_Client_Exec (fidx))
		ret = vm->globals[OFS_RETURN];
	else if (removed)
		*removed = true;

	if (s_csqc.global_self >= 0)
		*(int *)&vm->globals[s_csqc.global_self] = oldself;

	if (removed && !CSQC_Client_EntUsed (slot))
		*removed = true;
	return ret;
}

/*
=================
Ф3: CSQC-реестр моделей

#20/#75 precache_model регистрирует модель (имя→model_t*, индекс 1-based); #200
getmodelindex / #333 setmodelindex и поле `.modelindex` работают с этим индексом
(отклонение от FTE: у FTE отдельное пространство индексов для csqc-only моделей;
у нас — единый реестр поверх Mod_ForName). Индекс module-opaque.

T4 precache_model re-trigger: имя регистрируется даже при отсутствующем файле
(Mod_ForName возвращает NULL) — слот хранит NULL-заглушку, но индекс стабилен
(FTE pr_csqc.c:3215). После успешного скачивания заглушка заполняется в
CSQC_Client_ModelDownloadFinished (drop-in без повторного precache моделью).
=================
*/
#define CSQC_MAX_MODELS 512
static struct model_s *s_models[CSQC_MAX_MODELS];
static char s_modelnames[CSQC_MAX_MODELS][MAX_QPATH];
static int s_nmodels;

int CSQC_Client_ModelIndexKnown (const char *name)
{
	int i;
	if (!name || !name[0])
		return 0;
	for (i = 0; i < s_nmodels; i++)
		if (!strcmp (s_modelnames[i], name))
			return i + 1;
	return 0;
}

int CSQC_Client_ModelIndex (const char *name)
{
	struct model_s *m;
	int idx = CSQC_Client_ModelIndexKnown (name);
	if (idx || !name || !name[0])
		return idx;
	if (s_nmodels >= CSQC_MAX_MODELS)
		return 0;
	// T4 precache_model re-trigger (FTE pr_csqc.c:3215): register the name even if the
	// file is missing (Mod_ForName == NULL) so the returned index stays stable; the slot
	// then holds a NULL placeholder until the model is loaded after a successful download
	// (CSQC_Client_ModelDownloadFinished). "!= 0" therefore means "registered", not "loaded".
	m = Mod_ForName (name, false);
	strlcpy (s_modelnames[s_nmodels], name, MAX_QPATH);
	s_models[s_nmodels] = m;
	return ++s_nmodels;
}

struct model_s *CSQC_Client_ModelForIndex (int idx)
{
	return (idx >= 1 && idx <= s_nmodels) ? s_models[idx - 1] : NULL;
}

/* #334 modelnameforindex: обратный резолв индекса CSQC-реестра (T3 Э3). */
const char *CSQC_Client_ModelNameForIndex (int idx)
{
	return (idx >= 1 && idx <= s_nmodels) ? s_modelnames[idx - 1] : NULL;
}

void CSQC_Client_ModelReset (void)
{
	memset (s_modelnames, 0, sizeof (s_modelnames));
	memset (s_models, 0, sizeof (s_models));
	s_nmodels = 0;
}

/*
=================
T4 precache_model re-trigger: reload-on-download (FTE CL_DownloadFinished, cl_parse.c:858-868).

Called from CL_FinishDownload after a successful download. `downloadname` is
cls.downloadname (="<gamedir>/<file>", cl_parse.c:510), so the gamedir prefix is
stripped and the rest matched against the CSQC model registry. A matching slot is
(re)loaded via Mod_ForName: a NULL placeholder becomes the loaded model, so its stable
index turns render-usable without the module re-calling precache_model. A no-op when
the registry is empty (no CSQC module / nothing precached).
=================
*/
void CSQC_Client_ModelDownloadFinished (const char *downloadname)
{
	const char *raw;
	int i, prefix;

	if (!downloadname || !downloadname[0] || s_nmodels <= 0)
		return;

	prefix = (int)strlen (cls.gamedir);
	if (prefix <= 0 || strncmp (downloadname, cls.gamedir, prefix) || downloadname[prefix] != '/')
		return;
	raw = downloadname + prefix + 1;

	for (i = 0; i < s_nmodels; i++)
	{
		if (strcmp (s_modelnames[i], raw))
			continue;
		s_models[i] = Mod_ForName (s_modelnames[i], false);
	}
}

/*
=================
CSQC_Client_ValidateFile

Проверяет локальный файл csprogs по серверным ключам: размер == *csprogssize
и (если задан *csprogs) Com_BlockChecksum == crc (тот же md4, что у mvdsv
Com_BlockChecksum, md4.c). Аналог FTE CSQC_ValidateMainCSProgs (pr_csqc.c).
=================
*/
static qbool CSQC_Client_ValidateData (byte *data, int filesize, int size, unsigned crc)
{
	if (!data)
		return false;
	if (size > 0 && filesize != size)
		return false;
	if (crc && Com_BlockChecksum (data, filesize) != crc)
		return false;
	return true;
}

static qbool CSQC_Client_ValidateFile (const char *path, int size, unsigned crc)
{
	byte *data;
	int filesize;
	qbool ok;

	if (!path || !path[0])
		return false;
	// C1 (Wave C): heap-буфер + Q_free, не низкий hunk. ValidateFile зовётся в т.ч.
	// каждый кадр в CSQC_Client_Update, пока идёт скачивание; Hunk_AllocName копил бы
	// копии csprogs до смены карты.
	data = (byte *)FS_LoadHeapFile (path, &filesize);
	ok = CSQC_Client_ValidateData (data, filesize, size, crc);
	Q_free (data);
	return ok;
}

/*
=================
CSQC_Client_FindMainProgs

Поиск валидного локального csprogs по FTE-семантике (CSQC_FindMainProgs,
fteqw/engine/client/pr_csqc.c): 1) кэш csprogsvers/<crc>.dat, 2) *csprogsname
(+ фолбэк на csprogs.dat). При валидном name-файле и заданном crc пишем копию
в кэш csprogsvers/<crc>.dat (write-back, как FTE COM_WriteFile в pr_csqc.c) —
следующие коннекты берут кэш, а не перекачивают. Возвращает true и заполняет
pathbuf путём для CSQC_Client_Load.

anycsqc (T1.6b, FTE-паритет pr_csqc.c:7779): promiscuous-режим — не сверять
size/crc локального кандидата (сервер с anycsqc/битым *csprogs либо demoplayback,
pr_csqc.c:7777); write-back в crc-кэш при этом не делается.
=================
*/
static qbool CSQC_Client_FindMainProgs (char *pathbuf, size_t bufsz,
	const char *name, int sizep, unsigned crc, qbool anycsqc)
{
	extern void Sys_mkdir (const char *path);
	char buf[MAX_QPATH];
	const char *cands[3];
	int nc = 0;
	int i;

	if (crc)
	{
		snprintf (buf, sizeof (buf), "csprogsvers/%x.dat", crc);
		if (CSQC_Client_ValidateFile (buf, sizep, crc))
		{
			strlcpy (pathbuf, buf, bufsz);
			return true;
		}
	}

	if (name && name[0])
		cands[nc++] = name;
	if (!name || !name[0] || strcmp (name, "csprogs.dat"))
		cands[nc++] = "csprogs.dat";

	for (i = 0; i < nc; i++)
	{
		byte *data;
		int len;

		// C1 (Wave C): грузим кандидата один раз (heap) — валидация и write-back
		// из одного буфера; раньше было две HunkFile-загрузки на кандидата.
		data = (byte *)FS_LoadHeapFile (cands[i], &len);
		if (!CSQC_Client_ValidateData (data, len, anycsqc ? 0 : sizep, anycsqc ? 0 : crc))
		{
			Q_free (data);
			continue;
		}

		strlcpy (pathbuf, cands[i], bufsz);
		// FTE write-back: валидный name-файл копируем в кэш на будущее.
		// При anycsqc/demo crc не подтверждён — в кэш не пишем.
		if (crc && !anycsqc && !cls.demoplayback)
		{
			char dest[MAX_OSPATH], dir[MAX_OSPATH];
			char *slash;
			FILE *f;
			snprintf (dest, sizeof (dest), "%s/csprogsvers/%x.dat",
				cls.gamedir, crc);
			strlcpy (dir, dest, sizeof (dir));
			slash = strrchr (dir, '/');
			if (slash && slash != dir)
			{
				*slash = 0;
				Sys_mkdir (dir);
			}
			f = fopen (dest, "wb");
			if (f)
			{
				fwrite (data, 1, len, f);
				fclose (f);
				Con_Printf ("CSQC: cached csprogsvers/%x.dat\n", crc);
			}
		}
		Q_free (data);
		return true;
	}
	return false;
}

/*
=================
CSQC_Client_StartDownload

Запрашивает у сервера скачивание csprogs. Сервер отдаёт файл под *csprogsname
(mvdsv SV_LoadCSQC), но мы сохраняем его в отдельную папку-кэш
csprogsvers/<crc>.dat (как ftew, cl_parse.c:1640-1641), чтобы разные серверы не
перезатирали друг друга. ezquake CL_CheckOrDownloadFile не умеет разделять
remote/local имя — повторяем его стартовые шаги с другим локальным путём.
=================
*/
static void CSQC_Client_StartDownload (const char *remote, const char *localrel)
{
	extern void Sys_mkdir (const char *path);
	char dir[MAX_OSPATH];
	char *slash;

	if (cls.state < ca_connected || cls.demoplayback)
		return;

	snprintf (cls.downloadname, sizeof (cls.downloadname), "%s/%s", cls.gamedir, localrel);
	cls.downloadmethod = DL_QW;
	cls.downloadstarttime = Sys_DoubleTime ();
	COM_StripExtension (cls.downloadname, cls.downloadtempname, sizeof (cls.downloadtempname));
	strlcat (cls.downloadtempname, ".tmp", sizeof (cls.downloadtempname));

	// каталог назначения (напр. csprogsvers/) должен существовать
	strlcpy (dir, cls.downloadname, sizeof (dir));
	slash = strrchr (dir, '/');
	if (slash && slash != dir)
	{
		*slash = 0;
		Sys_mkdir (dir);
	}

	Com_Printf ("CSQC: downloading %s -> %s\n", remote, localrel);
	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va ("download \"%s\"", remote));
	cls.downloadnumber++;
	// B17: старт окна отсутствия прогресса + запоминаем имя нашего downloadname
	// (cls.download общий для всех загрузок — прогресс считаем только по своему файлу).
	s_csqc.csprogs_dl_lastprogress = Sys_DoubleTime ();
	s_csqc.csprogs_dl_percent = 0;
	s_csqc.csprogs_dl_started = false;
	strlcpy (s_csqc.csprogs_dl_localname, cls.downloadname, sizeof (s_csqc.csprogs_dl_localname));
}

/*
=================
CSQC_Client_FreeArena / AllocArena

Клиентская арена edicts (ADR 0017 P1/D2): прямая карта entnum -> слот
(entity-значение PR1 = N*edict_size). Q_malloc (не hunk — урок Bug1);
free в Disconnect и в начале Load (защита от повторного вызова).
=================
*/
static void CSQC_Client_FreeArena (void)
{
	if (s_csqc.edicts)
	{
		Q_free (s_csqc.edicts);
		s_csqc.edicts = NULL;
	}
	if (s_csqc.game_edicts)
	{
		Q_free (s_csqc.game_edicts);
		s_csqc.game_edicts = NULL;
	}
}

static void CSQC_Client_AllocArena (pr1vm_t *vm)
{
	int i;

	CSQC_Client_FreeArena ();
	if (!vm || vm->edict_size <= 0)
		return;

	// FTE-пул: сброс занятости/номера-карты при (пере)выделении арены.
	memset (s_used, 0, sizeof (s_used));
	memset (s_own, 0, sizeof (s_own));
	memset (s_numslot, 0, sizeof (s_numslot));
	memset (s_slotnum, 0, sizeof (s_slotnum));

	s_csqc.game_edicts = (byte *)Q_malloc ((size_t)CSQC_MAX_EDICTS * vm->edict_size);
	s_csqc.edicts = (edict_t *)Q_malloc (sizeof (edict_t) * CSQC_MAX_EDICTS);
	memset (s_csqc.game_edicts, 0, (size_t)CSQC_MAX_EDICTS * vm->edict_size);
	memset (s_csqc.edicts, 0, sizeof (edict_t) * CSQC_MAX_EDICTS);
	for (i = 0; i < CSQC_MAX_EDICTS; i++)
		s_csqc.edicts[i].v = (entvars_t *)(s_csqc.game_edicts + (size_t)i * vm->edict_size);

	vm->edicts = s_csqc.edicts;
	vm->game_edicts = s_csqc.game_edicts;
	vm->num_edicts = CSQC_MAX_EDICTS;
	vm->max_edicts = CSQC_MAX_EDICTS;
	vm->state = 0;	// клиентский инстанс; OP_ADDRESS-гард «world» не активен (world не пишем)
	vm->fieldofs_patch = NULL;	// FTE csprogs: raw field-оффсеты (ADR 0017 P2)
}

/*
=================
CSQC_Client_FindField

Ищет поле модуля по имени в fielddefs (см. PR1VM_FindFunction). Возвращает
смещение поля в float-словах от начала entvars (ddef_t.ofs) или -1.
=================
*/
int CSQC_Client_FindField (pr1vm_t *vm, const char *name)
{
	int i;

	if (!vm || !vm->fielddefs || !name)
		return -1;
	for (i = 0; i < vm->progs->numfielddefs; i++)
	{
		const char *s = CSQC_Client_GetString (vm, vm->fielddefs[i].s_name);
		if (s && s[0] && !strcmp (s, name))
			return vm->fielddefs[i].ofs;
	}
	return -1;
}

/*
=================
C2 (Wave C): кэш офсетов горячего пути

Резолв traced-глобалов и entity-полей, читаемых каждый кадр (csqc_store_trace,
csqc_add_one_entity, csqc_addentities), выполняется один раз при загрузке модуля
(CSQC_Client_OffsetCacheResolve). Раньше каждый вызов звал PR1VM_FindGlobal /
CSQC_Client_FindField — линейный скан globaldefs/fielddefs по strcmp.

FTE-эталон: глобалы — csqcg (pr_common.h:1109-1118, CSQC_FindGlobals pr_csqc.c:290-296,
store :2917-2923); entity-поля — overlay csqcentvars_t (pr_csqc.c:353-395).
=================
*/
static const char *s_traceg_names[CSQC_TRACEG_COUNT] =
{
	"trace_fraction", "trace_allsolid", "trace_startsolid", "trace_inopen",
	"trace_inwater", "trace_plane_dist", "trace_endpos", "trace_plane_normal",
	"trace_ent"
};

static const char *s_fieldcache_names[CSQC_FLD_COUNT] =
{
	"predraw", "modelindex", "model", "colormap", "origin", "angles",
	"frame", "skin", "effects", "alpha", "scale", "renderflags",
	"size", "mins", "maxs", "modelflags", "chain", "solid", "flags",
	"owner", "drawmask"
};

static int s_traceg[CSQC_TRACEG_COUNT];
static int s_fieldcache[CSQC_FLD_COUNT];

static void CSQC_Client_OffsetCacheReset (void)
{
	int i;

	for (i = 0; i < CSQC_TRACEG_COUNT; i++)
		s_traceg[i] = -1;
	for (i = 0; i < CSQC_FLD_COUNT; i++)
		s_fieldcache[i] = -1;
}

static void CSQC_Client_OffsetCacheResolve (pr1vm_t *vm)
{
	int i;

	for (i = 0; i < CSQC_TRACEG_COUNT; i++)
		s_traceg[i] = PR1VM_FindGlobal (vm, s_traceg_names[i]);
	for (i = 0; i < CSQC_FLD_COUNT; i++)
		s_fieldcache[i] = CSQC_Client_FindField (vm, s_fieldcache_names[i]);
}

int CSQC_Client_TraceGlobal (pr1vm_t *vm, int id)
{
	(void)vm;
	return (id >= 0 && id < CSQC_TRACEG_COUNT) ? s_traceg[id] : -1;
}

int CSQC_Client_FieldOfs (pr1vm_t *vm, int id)
{
	(void)vm;
	return (id >= 0 && id < CSQC_FLD_COUNT) ? s_fieldcache[id] : -1;
}

/*
=================
CSQC_Client_SetEntityContext

Ставит контекст сущности для CSQC_Ent_Update/Remove (ADR 0017 P2/D3):
self = entnum*edict_size (entity-значение PR1) и пишет float entnum в поле
.entnum (слот 7) арены. Модуль дальше читает self.entnum.
=================
*/
static void CSQC_Client_SetContextSlot (pr1vm_t *vm, unsigned slot, unsigned number)
{
	float *s;

	if (!vm || !vm->game_edicts)
		return;
	// self = slot*edict_size (entity-значение PR1, int-биты); .entnum (поле модуля)
	// = серверный номер (у своих spawn-сущностей номер не пишется — остаётся 0).
	if (s_csqc.global_self >= 0)
		*(int *)&vm->globals[s_csqc.global_self] = (int)slot * vm->edict_size;
	if (s_csqc.field_entnum >= 0 && slot < CSQC_MAX_EDICTS)
	{
		s = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size + s_csqc.field_entnum * 4);
		s[0] = (float)number;
	}
}

/*
=================
CSQC_Client_RunEntSpawn

R7/T1.3a: хук новой CSQC-сущности (FTE-паритет, pr_csqc.c:9650-9664). Движок
обнуляет self (self=0=мир), кладёт серверный номер в PARM0, вызывает
CSQC_Ent_Spawn; модуль создаёт/настраивает сущность (обычно spawn();
self.entnum = entnum) и возвращает её в self. Читаем self → слот арены
(self/edict_size). Возврат: валидный занятый слот или 0 (мир/невалиден; Q-D —
без фолбэка, как FTE ent=NULL).
=================
*/
static int CSQC_Client_RunEntSpawn (pr1vm_t *vm, unsigned int entnum)
{
	int selfval, slot;

	if (!vm || s_csqc.func_entspawn <= 0 || s_csqc.global_self < 0 || vm->edict_size <= 0)
		return 0;
	*(int *)&vm->globals[s_csqc.global_self] = 0;
	vm->globals[OFS_PARM0] = (float)entnum;
	if (!CSQC_Client_Exec (s_csqc.func_entspawn))
		return 0;
	selfval = *(int *)&vm->globals[s_csqc.global_self];
	slot = CSQC_Client_EntNum (vm, selfval);
	return (slot > 0 && slot < CSQC_MAX_EDICTS && s_used[slot]) ? slot : 0;
}

/* Q-E (FTE pr_csqc.c:9693-9694): после CSQC_Ent_Update модуль может сменить self;
   номер→слот переносим на новый валидный слот (0 = мир/снят). */
static void CSQC_Client_RemapAfterUpdate (pr1vm_t *vm, unsigned int entnum)
{
	int selfval, slot;

	if (!vm || s_csqc.func_entspawn <= 0 || s_csqc.global_self < 0 || vm->edict_size <= 0)
		return;
	selfval = *(int *)&vm->globals[s_csqc.global_self];
	slot = CSQC_Client_EntNum (vm, selfval);
	if (slot < 0 || slot >= CSQC_MAX_EDICTS || !s_used[slot])
		slot = 0;
	CSQC_Client_MapNumber ((int)entnum, slot);
}

/*
=================
CSQC_Client_EntAlloc / EntFree (FTE-пул)

Модульные сущности (builtin spawn) берут произвольный свободный слот пула
(первый свободный от 1) и помечаются s_own (remove разрешён только своим).
Сетевые слоты выделяются тем же пулом (без s_own) и держатся картой
номер→слот в ParseEntities. entity-значение PR1 = slot*edict_size.
=================
*/
static int CSQC_Client_AllocSlot (pr1vm_t *vm)
{
	int i;
	for (i = 1; i < CSQC_MAX_EDICTS; i++)
		if (!s_used[i])
		{
			s_used[i] = true;
			s_own[i] = false;
			// R2/D-A (FTE-паритет): обнулять поля слота при (пере)использовании —
			// иначе модуль видит остатки прошлой сущности. FTE: QC_ClearEdict /
			// ED_AllocIndex (pr_edict.c:30,85).
			if (vm && vm->game_edicts && vm->edict_size > 0)
				memset ((byte *)vm->game_edicts + (size_t)i * vm->edict_size, 0, vm->edict_size);
			return i;
		}
	Con_Printf ("CSQC_Client_AllocSlot: pool full (%d)\n", CSQC_MAX_EDICTS - 1);
	return 0;
}

int CSQC_Client_EntAlloc (struct pr1vm_s *v)
{
	pr1vm_t *vm = (pr1vm_t *)v;
	int slot;
	slot = CSQC_Client_AllocSlot (vm);
	if (slot)
		s_own[slot] = true;	// spawn-сущность: .entnum не пишем (0)
	return slot;
}

void CSQC_Client_EntFree (struct pr1vm_s *v, int entnum)
{
	pr1vm_t *vm = (pr1vm_t *)v;
	float *s;

	if (!vm || !vm->game_edicts)
		return;
	if (entnum <= 0 || entnum >= CSQC_MAX_EDICTS || !s_used[entnum])
		return;
	if (!s_own[entnum])
		return;	// сетевая сущность — не трогаем (ADR 0017)
	// B16: снять обратную карту slot→N, иначе s_numslot[N] остаётся валидным на
	// освобождённый слот, который может быть переиспользован (review add #9).
	if (s_slotnum[entnum] > 0 && s_slotnum[entnum] < CSQC_MAX_NUM
		&& s_numslot[s_slotnum[entnum]] == entnum)
		s_numslot[s_slotnum[entnum]] = 0;
	s_slotnum[entnum] = 0;
	s_used[entnum] = false;
	s_own[entnum] = false;
	s = (float *)((byte *)vm->game_edicts + (size_t)entnum * vm->edict_size);
	memset (s, 0, vm->edict_size);
}

/* внутренний сетевой путь (ParseEntities): слот без s_own.
   vm нужен для обнуления полей слота (R2/D-A). */
int CSQC_Client_NetAllocSlot (struct pr1vm_s *v)
{
	return CSQC_Client_AllocSlot ((pr1vm_t *)v);
}

void CSQC_Client_NetFreeSlot (int slot, int number)
{
	if (slot > 0 && slot < CSQC_MAX_EDICTS && s_used[slot])
	{
		s_used[slot] = false;
		s_own[slot] = false;
	}
	if (slot > 0 && slot < CSQC_MAX_EDICTS)
		s_slotnum[slot] = 0;	// B16: обратная карта не переживает фриз
	if (number > 0 && number < CSQC_MAX_NUM && s_numslot[number] == slot)
		s_numslot[number] = 0;
}

/* доступ/диагностика (P1d C1): обход пула и полей */
qbool CSQC_Client_EntUsed (int entnum)
{
	return (entnum > 0 && entnum < CSQC_MAX_EDICTS) ? s_used[entnum] : false;
}

int CSQC_Client_EntSpawnBase (void)
{
	return 1;	// первый используемый слот пула (0 — world)
}

int CSQC_Client_EntUsedCount (void)
{
	int i, n = 0;
	for (i = 1; i < CSQC_MAX_EDICTS; i++)
		if (s_used[i])
			n++;
	return n;
}

/*
=================
CSQC_Client_EntNum

Convert a raw PR1 entity value (slot*edict_size) into a pool slot. The client
VM executes untrusted csprogs and a builtin entity argument is not dereferenced
by the VM (unlike an opcode pointer), so an out-of-range value is clamped to
world (0) instead of faulting. FTE parity: PF_etos/PF_wasfreed go through
ProgsToEdict (fteqw/engine/qclib/initlib.c:960-974), which reports "Bad entity
index" and falls back to edict 0. Same bound as the opcode predicate
PR1VM_ClientBadEdict (pr_exec.c:428-436); on the client instance
num_edicts == max_edicts == CSQC_MAX_EDICTS (see CSQC_Client_AllocArena).
=================
*/
int CSQC_Client_EntNum (struct pr1vm_s *v, int raw)
{
	pr1vm_t *vm = (pr1vm_t *)v;
	int idx;

	if (!vm || vm->edict_size <= 0)
		return 0;
	idx = raw / vm->edict_size;
	if (raw < 0 || idx >= vm->max_edicts)
	{
		Con_DPrintf ("CSQC: bad entity value %d\n", raw);
		return 0;	// world
	}
	return idx;
}

int CSQC_Client_NumToSlot (int number)
{
	return (number > 0 && number < CSQC_MAX_NUM) ? s_numslot[number] : 0;
}

int CSQC_Client_MapNumber (int number, int slot)
{
	if (number > 0 && number < CSQC_MAX_NUM)
		s_numslot[number] = slot;
	if (slot > 0 && slot < CSQC_MAX_EDICTS)
		s_slotnum[slot] = (number > 0 && number < CSQC_MAX_NUM) ? number : 0;
	return slot;
}

/*
=================
E1a #371 deltalisten: движковый мост player_state → arena-edict (FTE-путь,
pr_csqc.c `CSQC_DeltaPlayer`/`CSQC_PlayerStateToCSQC`). Мост отдаёт модулю
авторитетное (no-lerp) состояние игроков: `self`/`.entnum` = pnum+1, поля
origin/velocity/angles (+modelindex/skin). Модуль-калбэк зовётся как
CSQC_Ent_Update (PARM0 = isnew) раз на новый acked-кадр (cl.parsecount).
=================
*/
static int s_delta_func[MAX_MODELS];
static int s_delta_flags[MAX_MODELS];
// Личный маппинг player-bridge (pnum → arena slot), чтобы отличать владение от
// svc76 (CSQC_Client_NumToSlot). num = pnum+1 (серверный entnum игрока).
static int s_player_slot[MAX_CLIENTS];
// E1b: delta-entity мост — номер пакетной сущности → arena slot + «виден в кадре».
static int s_delta_slot[CSQC_MAX_NUM];
static byte s_delta_seen[CSQC_MAX_NUM];
// C4 Э3 (MASK_DELTA): callback вернул !=0 → движок не рисует сущность (рисует модуль).
static byte s_delta_player_owned[MAX_CLIENTS];
static byte s_delta_ent_owned[CSQC_MAX_NUM];
// C4 (Wave C): есть ли хоть один зарегистрированный deltalisten (func>0). Если нет —
// Delta* не memset'ит 4096-массивы и не сканирует пакетные сущности каждый кадр
// (FTE: deltafunction[] пуст -> CSQC_DeltaUpdate не работает, pr_csqc.c:7691/:5733).
static qbool s_delta_any;

static void CSQC_Client_DeltaReset (void)
{
	s_delta_any = false;
	memset (s_delta_func, 0, sizeof (s_delta_func));
	memset (s_delta_flags, 0, sizeof (s_delta_flags));
	memset (s_player_slot, 0, sizeof (s_player_slot));
	memset (s_delta_slot, 0, sizeof (s_delta_slot));
	memset (s_delta_seen, 0, sizeof (s_delta_seen));
	memset (s_delta_player_owned, 0, sizeof (s_delta_player_owned));
	memset (s_delta_ent_owned, 0, sizeof (s_delta_ent_owned));
}

// C4 Э3: геттеры для CL_LinkPlayers/CL_LinkPacketEntities (cl_ents.c).
qbool CSQC_Client_DeltaPlayerOwned (int pnum)
{
	return (pnum >= 0 && pnum < MAX_CLIENTS && s_delta_player_owned[pnum]) ? true : false;
}

qbool CSQC_Client_DeltaEntityOwned (int number)
{
	return (number > 0 && number < CSQC_MAX_NUM && s_delta_ent_owned[number]) ? true : false;
}

void CSQC_Client_DeltaListen (const char *model, int func, int flags)
{
	int i;
	if (!model)
		return;
	if (!strcmp (model, "*"))
	{
		for (i = 0; i < MAX_MODELS; i++)
		{
			s_delta_func[i] = (func > 0) ? func : 0;
			s_delta_flags[i] = flags;
		}
		if (func > 0)
			s_delta_any = true;	// C4: хотя бы один слушатель — Delta* активны
		return;
	}
	for (i = 1; i < MAX_MODELS; i++)
	{
		if (!cl.model_name[i][0])
			break;
		if (!strcmp (cl.model_name[i], model))
		{
			s_delta_func[i] = (func > 0) ? func : 0;
			s_delta_flags[i] = flags;
			if (func > 0)
				s_delta_any = true;
			break;
		}
	}
}

static void CSQC_Client_DeltaPlayers (pr1vm_t *vm)
{
	int pnum;

	if (!vm || !vm->game_edicts || !vm->edict_size)
		return;
	if (!s_delta_any)
		return;		// C4: нет слушателей — работа не нужна
	if (cls.demoplayback || cls.mvdplayback)
		return;		// предикция — только живая игра (как C5-A)
	memset (s_delta_player_owned, 0, sizeof (s_delta_player_owned));
	for (pnum = 0; pnum < MAX_CLIENTS; pnum++)
	{
		player_state_t *st = &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[pnum];
		int num = pnum + 1;
		int slot = s_player_slot[pnum];
		int func = 0, isnew = 0;

		if (st->messagenum == cl.parsecount && st->modelindex > 0
			&& st->modelindex < MAX_MODELS)
			func = s_delta_func[st->modelindex];

		if (!func)
		{
			// сущность отсутствует/без слушателя — убрать, если она была
			if (slot)
			{
				if (s_csqc.func_entremove > 0)
				{
					CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);
					CSQC_Client_Exec (s_csqc.func_entremove);
				}
				CSQC_Client_NetFreeSlot (slot, num);
				s_player_slot[pnum] = 0;
			}
			continue;
		}

		// svc76 уже владеет номером — не перетираем (FTE csqcent[]-guard)
		if (!slot && CSQC_Client_NumToSlot (num))
			continue;

		if (!slot)
		{
			slot = CSQC_Client_NetAllocSlot (vm);
			if (!slot)
				continue;
			CSQC_Client_MapNumber (num, slot);
			s_player_slot[pnum] = slot;
			isnew = 1;
		}

		CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);

		// Поля player_state (no-lerp: сырые значения, как FTE RSES_NOLERP).
		{
			float *base = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);
			if (s_csqc.f_origin >= 0)
				VectorCopy (st->origin, base + s_csqc.f_origin);
			if (s_csqc.f_velocity >= 0)
				VectorCopy (st->velocity, base + s_csqc.f_velocity);
			if (s_csqc.f_angles >= 0)
			{
				// viewangles сервер шлёт только в демо; локальному игроку —
				// свежие cl.viewangles (обновляются из usercmd).
				const float *ang = (pnum == cl.playernum) ? cl.viewangles : st->viewangles;
				VectorCopy (ang, base + s_csqc.f_angles);
			}
			if (s_csqc.f_modelindex >= 0)
				base[s_csqc.f_modelindex] = (float)st->modelindex;
			if (s_csqc.f_skin >= 0)
				base[s_csqc.f_skin] = (float)st->skinnum;
			// Stage 4: player render fields. FTE CSQC_PlayerStateToCSQC fills
			// frame/skin/colormap (pr_csqc.c:5485,5609-5636); engine player render
			// uses frame/effects/translations (cl_ents.c:2222-2226).
			if (s_csqc.f_frame >= 0)
				base[s_csqc.f_frame] = (float)st->frame;
			if (s_csqc.f_effects >= 0)
				base[s_csqc.f_effects] = (float)st->effects;
			if (s_csqc.f_colormap >= 0)
				base[s_csqc.f_colormap] = (float)(pnum + 1);	// player index (FTE pr_csqc.c:5627)
			if (s_csqc.f_drawmask >= 0)
				base[s_csqc.f_drawmask] = 1;	// MASK_DELTA (FTE pr_csqc.c:5697)
		}

		vm->globals[OFS_PARM0] = isnew ? 1 : 0;
		{
			// C4 Э3: возврат callback !=0 → движок не рисует этого игрока (рисует модуль).
			float pret = 0;
			if (CSQC_Client_ExecRet (func, &pret) && pret != 0)
				s_delta_player_owned[pnum] = 1;
		}
		if (s_csqc.errored)
			return;
	}
}

/*
=================
E1b #371 delta-entity мост (FTE CSQC_DeltaStart/Update/End, pr_csqc.c:5719+):
пакетные сущности кадра (entity_state_t) с зарегистрированным по модели callback'ом
отдаются модулю как CSQC_Ent_Update (self/.entnum, PARM0 = isnew). Пропавшие в
кадре — remove-путь. RSES_NOLERP/NOROTATE: сырое состояние (интерполяции нет);
NOTRAILS/NOLIGHTS недействительны (в ezq CSQC нет трейлов/динамического света).
=================
*/
static void CSQC_Client_DeltaEntities (pr1vm_t *vm)
{
	packet_entities_t *pack;
	int i, num;

	if (!vm || !vm->game_edicts || !vm->edict_size)
		return;
	if (!s_delta_any)
		return;		// C4: нет слушателей — не memset'им/сканируем 4096 каждый кадр
	if (cls.demoplayback || cls.mvdplayback)
		return;
	if (!cl.validsequence)
		return;

	memset (s_delta_seen, 0, sizeof (s_delta_seen));
	memset (s_delta_ent_owned, 0, sizeof (s_delta_ent_owned));
	pack = &cl.frames[cl.validsequence & UPDATE_MASK].packet_entities;

	for (i = 0; i < pack->num_entities; i++)
	{
		entity_state_t *es = &pack->entities[i];
		int slot, func, isnew = 0;
		float *base;

		num = es->number;
		if (num <= 0 || num >= CSQC_MAX_NUM)
			continue;
		if (es->modelindex <= 0 || es->modelindex >= MAX_MODELS)
			continue;
		func = s_delta_func[es->modelindex];
		if (!func)
			continue;

		s_delta_seen[num] = 1;
		slot = s_delta_slot[num];
		if (!slot)
		{
			// svc76 уже владеет номером — не перетираем
			if (CSQC_Client_NumToSlot (num))
				continue;
			slot = CSQC_Client_NetAllocSlot (vm);
			if (!slot)
				continue;
			CSQC_Client_MapNumber (num, slot);
			s_delta_slot[num] = slot;
			isnew = 1;
		}

		CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);
		base = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);
		if (s_csqc.f_origin >= 0)
			VectorCopy (es->origin, base + s_csqc.f_origin);
		if (s_csqc.f_angles >= 0)
			VectorCopy (es->angles, base + s_csqc.f_angles);
		if (s_csqc.f_modelindex >= 0)
			base[s_csqc.f_modelindex] = (float)es->modelindex;
		if (s_csqc.f_frame >= 0)
			base[s_csqc.f_frame] = (float)es->frame;
		if (s_csqc.f_skin >= 0)
			base[s_csqc.f_skin] = (float)es->skinnum;
		if (s_csqc.f_effects >= 0)
			base[s_csqc.f_effects] = (float)es->effects;
		if (s_csqc.f_drawmask >= 0)
			base[s_csqc.f_drawmask] = 1;	// MASK_DELTA (FTE pr_common.h:901)

		vm->globals[OFS_PARM0] = isnew ? 1 : 0;
		{
			// C4 Э3: возврат callback !=0 → движок не рисует эту пакетную сущность.
			float pret = 0;
			if (CSQC_Client_ExecRet (func, &pret) && pret != 0)
				s_delta_ent_owned[num] = 1;
		}
		if (s_csqc.errored)
			return;
	}

	// пропавшие в этом кадре — remove-путь
	for (num = 1; num < CSQC_MAX_NUM; num++)
	{
		int slot = s_delta_slot[num];
		if (slot && !s_delta_seen[num])
		{
			if (s_csqc.func_entremove > 0)
			{
				CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)num);
				CSQC_Client_Exec (s_csqc.func_entremove);
			}
			CSQC_Client_NetFreeSlot (slot, num);
			s_delta_slot[num] = 0;
		}
	}
}

/*
=================
player_localentnum (FTE pr_csqc.c:136-145)
=================

Публикация глобала модуля player_localentnum (номер наблюдаемого игрока) каждый
кадр перед CSQC_UpdateView. Это часть окружения builtins «как в FTE»: FTE публикует
глобал всегда; НО сущности игроков ezquake НЕ фабрикует (окружение сущностей = то,
что прислал сервер svc76 + свои spawn, как у FTE в отсутствие серверной эмиссии
игроков / player-delta). Зеркало игроков (бывш. Шаг 7.2) удалено — C7 self/play
N/A до серверной эмиссии игроков модом.
*/
void CSQC_Client_UpdateLocalEntnum (void)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored || s_csqc.g_localentnum < 0)
		return;
	vm->globals[s_csqc.g_localentnum] = (cl.viewplayernum >= 0) ? cl.viewplayernum + 1 : 0;
}

/*
=================
C5-E Ф1 (no-op revision): view/listener/view_angles + project/unproject.

- `view_angles` — глобал модуля, публикуется каждый кадр (FTE); значение — углы вида
  движка (cl.viewangles).
- `#351 setlistener` — модуль задаёт аудио-листенер; cl_main.c использует его в S_Update,
  пока модуль активен (иначе — обычно).
- `#303 setproperty` (подмножество VF_*) — view-origin/angles/vrect/fov модуля; применяется
  к r_refdef после V_CalcRefdef (cl_view.c) при активном CSQC. Лаг 1 кадр: CSQC_UpdateView
  вызывается в HUD-фазе (после 3D-рендера) — отличие от FTE, документировано.
- `#310/#311 project/unproject` — экран↔мир через матрицы движка
  (R_GetModelviewMatrix/R_GetProjectionMatrix/R_GetViewport, r_matrix.c).
=================
*/
#define CSQC_VFP_MIN		1
#define CSQC_VFP_MIN_X		2
#define CSQC_VFP_MIN_Y		3
#define CSQC_VFP_SIZE		4
#define CSQC_VFP_SIZE_X		5
#define CSQC_VFP_SIZE_Y		6
#define CSQC_VFP_VIEWPORT	7
#define CSQC_VFP_FOV		8
#define CSQC_VFP_FOVX		9
#define CSQC_VFP_FOVY		10
#define CSQC_VFP_ORIGIN		11
#define CSQC_VFP_ORIGIN_X	12
#define CSQC_VFP_ORIGIN_Y	13
#define CSQC_VFP_ORIGIN_Z	14
#define CSQC_VFP_ANGLES		15
#define CSQC_VFP_ANGLES_X	16
#define CSQC_VFP_ANGLES_Y	17
#define CSQC_VFP_ANGLES_Z	18
// B22: set-флаги (FTE pr_common.h:809-824, csdefs.qc:395-402). Значения — как
// в FTE; VF_PERSPECTIVE распознаётся (return 1), но визуально не реализован
// (accept+doc) — изометрия в ezq-рендере отсутствует.
#define CSQC_VFP_DRAWWORLD	19
#define CSQC_VFP_DRAWENGINESBAR	20
#define CSQC_VFP_DRAWCROSSHAIR	21
#define CSQC_VFP_PERSPECTIVE	200

static qbool s_listener_on;
static vec3_t s_listener_org, s_listener_fwd, s_listener_rht, s_listener_up;

static qbool s_vp_on;
static qbool s_vp_origin_set, s_vp_angles_set, s_vp_vrect_set, s_vp_fovx_set, s_vp_fovy_set;
static vec3_t s_vp_origin, s_vp_angles;
static int s_vp_x, s_vp_y, s_vp_w, s_vp_h;
static float s_vp_fovx, s_vp_fovy;
// B22: set-флаги (FTE-дефолты clearscene, pr_csqc.c:2078-2079).
static qbool s_vp_drawsbar = false;
static qbool s_vp_drawcrosshair = false;

static void CSQC_Client_ViewPropsReset (void)
{
	s_vp_on = false;
	s_vp_origin_set = s_vp_angles_set = s_vp_vrect_set = false;
	s_vp_fovx_set = s_vp_fovy_set = false;
	// FTE clearscene: sbar/crosshair off (pr_csqc.c:2078-2079).
	s_vp_drawsbar = false;
	s_vp_drawcrosshair = false;
}

static void CSQC_Client_ViewReset (void)
{
	s_listener_on = false;
	VectorClear (s_listener_org);
	VectorClear (s_listener_fwd);
	VectorClear (s_listener_rht);
	VectorClear (s_listener_up);
	CSQC_Client_ViewPropsReset ();
}

// #300 clearscene: FTE сбрасывает view-свойства (модуль зовёт clearscene каждую
// CSQC_UpdateView; без сброса #303-override «залипал» бы между кадрами).
void CSQC_Client_ResetViewProps (void)
{
	CSQC_Client_ViewPropsReset ();
}

// #351 setlistener(origin, forward, right, up)
void CSQC_Client_SetListener (const float *origin, const float *forward, const float *right, const float *up)
{
	VectorCopy (origin, s_listener_org);
	VectorCopy (forward, s_listener_fwd);
	VectorCopy (right, s_listener_rht);
	VectorCopy (up, s_listener_up);
	s_listener_on = true;
}

qbool CSQC_Client_ListenerActive (void)
{
	return s_listener_on && s_csqc.loaded && !s_csqc.errored;
}

void CSQC_Client_GetListener (float *origin, float *forward, float *right, float *up)
{
	VectorCopy (s_listener_org, origin);
	VectorCopy (s_listener_fwd, forward);
	VectorCopy (s_listener_rht, right);
	VectorCopy (s_listener_up, up);
}

// #303 setproperty: VF_* подмножество (view). args — последовательные float-аргументы
// после property (вектор — 3 значения, скаляр — 1).
qbool CSQC_Client_SetViewProperty (int prop, int argc, const float *args)
{
	qbool handled = true;
	switch (prop)
	{
	case CSQC_VFP_ORIGIN:
		if (argc >= 3) { VectorCopy (args, s_vp_origin); s_vp_origin_set = true; }
		break;
	case CSQC_VFP_ORIGIN_X: s_vp_origin[0] = args[0]; s_vp_origin_set = true; break;
	case CSQC_VFP_ORIGIN_Y: s_vp_origin[1] = args[0]; s_vp_origin_set = true; break;
	case CSQC_VFP_ORIGIN_Z: s_vp_origin[2] = args[0]; s_vp_origin_set = true; break;
	case CSQC_VFP_ANGLES:
		if (argc >= 3) { VectorCopy (args, s_vp_angles); s_vp_angles_set = true; }
		break;
	case CSQC_VFP_ANGLES_X: s_vp_angles[0] = args[0]; s_vp_angles_set = true; break;
	case CSQC_VFP_ANGLES_Y: s_vp_angles[1] = args[0]; s_vp_angles_set = true; break;
	case CSQC_VFP_ANGLES_Z: s_vp_angles[2] = args[0]; s_vp_angles_set = true; break;
	case CSQC_VFP_VIEWPORT:
		// FTE pr_csqc.c:2542 — позиция-вектор (PARM1) + размер-вектор (PARM2),
		// т.е. 6 слов (csdefs VF_VIEWPORT = "vector+vector"). Раньше ezq читал
		// args[0..1] как размер, теряя позицию/size.
		if (argc >= 6)
		{
			s_vp_x = (int)args[0]; s_vp_y = (int)args[1];
			s_vp_w = (int)args[3]; s_vp_h = (int)args[4];
			s_vp_vrect_set = true;
		}
		break;
	case CSQC_VFP_MIN:
		if (argc >= 2) { s_vp_x = (int)args[0]; s_vp_y = (int)args[1]; s_vp_vrect_set = true; }
		break;
	case CSQC_VFP_MIN_X: s_vp_x = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_MIN_Y: s_vp_y = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_SIZE:
		if (argc >= 2) { s_vp_w = (int)args[0]; s_vp_h = (int)args[1]; s_vp_vrect_set = true; }
		break;
	case CSQC_VFP_SIZE_X: s_vp_w = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_SIZE_Y: s_vp_h = (int)args[0]; s_vp_vrect_set = true; break;
	case CSQC_VFP_FOV:
		if (argc >= 2) { s_vp_fovx = args[0]; s_vp_fovy = args[1]; s_vp_fovx_set = s_vp_fovy_set = true; }
		break;
	case CSQC_VFP_FOVX: s_vp_fovx = args[0]; s_vp_fovx_set = true; break;
	case CSQC_VFP_FOVY: s_vp_fovy = args[0]; s_vp_fovy_set = true; break;
	case CSQC_VFP_DRAWWORLD:
		// accept+doc: распознан (return 1); мир под takeover всегда рисует
		// R_RenderView (r_rmain.c:939), аналога RDF_NOWORLDMODEL нет.
		break;
	case CSQC_VFP_DRAWENGINESBAR:
		if (argc >= 1) s_vp_drawsbar = (args[0] != 0);
		break;
	case CSQC_VFP_DRAWCROSSHAIR:
		if (argc >= 1) s_vp_drawcrosshair = (args[0] != 0);
		break;
	case CSQC_VFP_PERSPECTIVE:
		// accept+doc: флаг распознан (return 1), изометрия в ezq-рендере не
		// реализована (нет аналога r_refdef.useperspective).
		break;
	default:
		handled = false;	// без аналога — FTE default возвращает 0
		break;
	}
	s_vp_on = s_vp_origin_set || s_vp_angles_set || s_vp_vrect_set || s_vp_fovx_set || s_vp_fovy_set;
	// FTE применяет view-флаги в том же кадре (setter пишет r_refdef сразу); ezq
	// раньше откладывал до V_CalcRefdef => лаг 1 кадр (parity-audit, было).
	CSQC_Client_ApplyViewProps ();
	return handled;
}

// Применяется после V_CalcRefdef (cl_view.c), только при активном CSQC-модуле.
void CSQC_Client_ApplyViewProps (void)
{
	if (!s_vp_on || !s_csqc.loaded || s_csqc.errored)
		return;
	if (s_vp_origin_set)
		VectorCopy (s_vp_origin, r_refdef.vieworg);
	if (s_vp_angles_set)
		VectorCopy (s_vp_angles, r_refdef.viewangles);
	if (s_vp_vrect_set)
	{
		r_refdef.vrect.x = s_vp_x;
		r_refdef.vrect.y = s_vp_y;
		r_refdef.vrect.width = s_vp_w;
		r_refdef.vrect.height = s_vp_h;
	}
	if (s_vp_fovx_set)
		r_refdef.fov_x = s_vp_fovx;
	if (s_vp_fovy_set)
		r_refdef.fov_y = s_vp_fovy;
}

/*
=================
B22 (FTE-паритет): гейт движкового sbar/HUD и crosshair. FTE clearscene ставит
r_refdef.drawsbar/drawcrosshair = false (pr_csqc.c:2078-2079), модуль возвращает их
через #303 setproperty(VF_DRAWENGINESBAR/VF_DRAWCROSSHAIR, 1). Под takeover
(CSQC_Client_SceneActive) cl_screen.c спрашивает эти аксессоры; вне takeover гейт
не применяется (движковый HUD/прицел — как раньше).
=================
*/
qbool CSQC_Client_DrawEngineSbar (void)
{
	return s_vp_drawsbar;
}

qbool CSQC_Client_DrawCrosshairFlag (void)
{
	return s_vp_drawcrosshair;
}

// C5-E: публикация глобала view_angles (перед CSQC_UpdateView).
void CSQC_Client_PublishViewAngles (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored || s_csqc.g_view_angles < 0)
		return;
	vm->globals[s_csqc.g_view_angles + 0] = cl.viewangles[0];
	vm->globals[s_csqc.g_view_angles + 1] = cl.viewangles[1];
	vm->globals[s_csqc.g_view_angles + 2] = cl.viewangles[2];
}

/*
C5-E Ф1: #311 project / #310 unproject — семантика FTE (pr_csqc.c:1966/2012):
clip = (model*proj) * v (наша композиция эквивалентна FTE proj*modelview),
NDC -> экран с Y-флипом и r_refdef.vrect, глубина FTE (знак при w<0).
Guard'ов нет (FTE-паритет); вырожденные случаи дают NaN/Inf — это диагностика.
*/
qbool CSQC_Client_Project (const float *world, float *sx, float *sy, float *sz)
{
	float model[16], proj[16], a[16], v[4], clip[4], sum;
	float rx, ry, rw, rh;
	int i, j, k;

	R_GetModelviewMatrix (model);
	R_GetProjectionMatrix (proj);

	// a = model * proj (row-вектор)
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
		{
			sum = 0;
			for (k = 0; k < 4; k++)
				sum += model[i * 4 + k] * proj[k * 4 + j];
			a[i * 4 + j] = sum;
		}
	v[0] = world[0]; v[1] = world[1]; v[2] = world[2]; v[3] = 1;
	for (j = 0; j < 4; j++)
	{
		sum = 0;
		for (k = 0; k < 4; k++)
			sum += v[k] * a[k * 4 + j];
		clip[j] = sum;
	}
	clip[0] /= clip[3];	// FTE: без guard (вырожденный w -> NaN/Inf)
	clip[1] /= clip[3];
	clip[2] /= clip[3];

	rx = r_refdef.vrect.x;
	ry = r_refdef.vrect.y;
	rw = r_refdef.vrect.width;
	rh = r_refdef.vrect.height;
	*sx = (1 + clip[0]) / 2 * rw + rx;
	*sy = (1 - (1 + clip[1]) / 2) * rh + ry;
	*sz = clip[2];
	if (clip[3] < 0)
		*sz = -*sz;
	return true;
}

// Обратная 4x4 (row-major) методом Гаусса-Жордана.
static qbool csqc_mat4_invert (const float *m, float *out)
{
	float a[4][8];
	int i, j, k;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			a[i][j] = m[i * 4 + j];
			a[i][j + 4] = (i == j) ? 1.0f : 0.0f;
		}
	}
	for (i = 0; i < 4; i++)
	{
		int piv = i;
		for (k = i + 1; k < 4; k++)
			if (fabs (a[k][i]) > fabs (a[piv][i]))
				piv = k;
		if (fabs (a[piv][i]) < 1e-12f)
			return false;
		if (piv != i)
			for (j = 0; j < 8; j++)
			{
				float t = a[i][j]; a[i][j] = a[piv][j]; a[piv][j] = t;
			}
		{
			float d = a[i][i];
			for (j = 0; j < 8; j++)
				a[i][j] /= d;
		}
		for (k = 0; k < 4; k++)
		{
			float f;
			if (k == i)
				continue;
			f = a[k][i];
			for (j = 0; j < 8; j++)
				a[k][j] -= f * a[i][j];
		}
	}
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
			out[i * 4 + j] = a[i][j + 4];
	return true;
}

// #310 unproject(screen x, y, depth) -> world (FTE-маппинг экран->NDC).
qbool CSQC_Client_Unproject (float sx, float sy, float sz, float *world)
{
	float model[16], proj[16], a[16], inv[16], v[4], res[4], sum, tx, ty;
	int i, j, k;

	R_GetModelviewMatrix (model);
	R_GetProjectionMatrix (proj);
	for (i = 0; i < 4; i++)
		for (j = 0; j < 4; j++)
		{
			sum = 0;
			for (k = 0; k < 4; k++)
				sum += model[i * 4 + k] * proj[k * 4 + j];
			a[i * 4 + j] = sum;
		}
	if (!csqc_mat4_invert (a, inv))
		return false;

	tx = (sx - r_refdef.vrect.x) / r_refdef.vrect.width;
	ty = (sy - r_refdef.vrect.y) / r_refdef.vrect.height;
	ty = 1 - ty;
	v[0] = tx * 2 - 1;
	v[1] = ty * 2 - 1;
	v[2] = sz * 2 - 1;
	if (v[2] >= 1)
		v[2] = 0.999999f;
	v[3] = 1;
	for (j = 0; j < 4; j++)
	{
		sum = 0;
		for (k = 0; k < 4; k++)
			sum += v[k] * inv[k * 4 + j];
		res[j] = sum;
	}
	// FTE: деление на res[3] без guard
	world[0] = res[0] / res[3];
	world[1] = res[1] / res[3];
	world[2] = res[2] / res[3];
	return true;
}

/*
=================
CSQC_Client_GetEntity

#504 getentity — FTE PF_getentity (pr_csqc.c:5862-6170): read interpolated state
of non-csqc (engine-networked) entities by server number. ezq has no
cl.lerpents/cl.lerpplayers; the source is cl_entities[] (current entity_state_t +
per-frame lerp data) and, for players, player_state_t / player bbox / player
colours. "Active" = present in the current packet (player: playerstate.messagenum
== cl.parsecount; map: cent->sequence == cl.validsequence), the analog of FTE
"le->sequence == cl.lerpentssequence".

Origin and angles are interpolated (T2.4) with the same lerp data the renderer
uses (cent->old_origin/current.origin, old_angles/current.angles, startlerp/
deltalerp), so even the local player and non-drawn entities (whose lerp_origin is
not written by CL_LinkPlayers) get the lerped values. Player-angle convention
follows FTE: the local player's pitch is model-space (-viewangles[0]/3), remote
players keep the raw packet angles. GE_MAXENTS is the runtime equivalent of FTE
cl.maxlerpents (highest packet entity number + headroom).

out[3] is always zeroed then filled (float fields use out[0]; vector fields use all
three). Fields with no ezq data source return the FTE default (0, or '1 1 1' for
GLOWMOD/RTCOLOUR) — documented deviation (parity audit).
=================
*/
#define CSQC_GE_MAXENTS		(-1)
#define CSQC_GE_ACTIVE		0
#define CSQC_GE_ORIGIN		1
#define CSQC_GE_FORWARD		2
#define CSQC_GE_RIGHT		3
#define CSQC_GE_UP		4
#define CSQC_GE_SCALE		5
#define CSQC_GE_ORIGINANDVECTORS 6
#define CSQC_GE_ALPHA		7
#define CSQC_GE_COLORMOD	8
#define CSQC_GE_PANTSCOLOR	9
#define CSQC_GE_SHIRTCOLOR	10
#define CSQC_GE_SKIN		11
#define CSQC_GE_MINS		12
#define CSQC_GE_MAXS		13
#define CSQC_GE_ABSMIN		14
#define CSQC_GE_ABSMAX		15
#define CSQC_GE_LIGHT		16
#define CSQC_GE_MODELINDEX	200
#define CSQC_GE_EFFECTS		202
#define CSQC_GE_FRAME		203
#define CSQC_GE_ANGLES		204
#define CSQC_GE_GLOWMOD		208
#define CSQC_GE_RTCOLOUR	213

// Lerp helpers for #504 (T2.4) — mirror the renderer's interpolation
// (cl_ents.c CL_LinkPacketEntities:1270-1295/1325-1333). The lerp data
// (old_origin/current.origin, old_angles/current.angles, startlerp/deltalerp) is
// filled by CL_SetupPacketEntity (map entities) and SetupPlayerEntity (players).
extern cvar_t cl_nolerp, cl_lerp_monsters;
extern qbool cl_nolerp_on_entity_flag;
extern qbool NewLerp_AbleModel (int idx);

static qbool CSQC_Client_EntityIsMonster (int modelindex)
{
	int i;
	if (!cl_lerp_monsters.value)
		return false;
	for (i = 1; i < 17; i++)
		if (modelindex == cl_modelindices[mi_monster1 + i - 1])
			return true;
	return false;
}

static void CSQC_Client_EntityLerp (const centity_t *cent, vec3_t org, vec3_t ang)
{
	double time = cls.mvdplayback ? cls.demotime : cl.time;
	float lerp;

	// Same gates as the renderer: no-lerp cvars/flag (not in demos, not for
	// monsters) and a non-positive lerp delta fall back to the packet state.
	if (((cl_nolerp.value || cl_nolerp_on_entity_flag) && !cls.mvdplayback &&
		 !CSQC_Client_EntityIsMonster (cent->current.modelindex)) ||
		cent->deltalerp <= 0)
	{
		VectorCopy (cent->current.origin, org);
		VectorCopy (cent->current.angles, ang);
		return;
	}

	lerp = min (max ((float)((time - cent->startlerp) / cent->deltalerp), 0.0f), 1.0f);

	if (NewLerp_AbleModel (cent->current.modelindex))
	{
		float d = time - cent->startlerp;

		if (d >= 2 * cent->deltalerp)	// entity looks stopped — stay at last lerp
			VectorCopy (cent->lerp_origin, org);
		else
			VectorMA (cent->old_origin, d, cent->velocity, org);
	}
	else
	{
		VectorInterpolate (cent->old_origin, lerp, cent->current.origin, org);
	}
	AngleInterpolate (cent->old_angles, lerp, cent->current.angles, ang);
}

void CSQC_Client_GetEntity (int entnum, int fldnum, float out[3])
{
	centity_t *cent;
	entity_state_t *es;
	player_state_t *ps;
	qbool is_player = false;
	qbool active;
	int pnum = -1, modelindex;
	const model_t *model;
	vec3_t org, ang;

	if (out)
		out[0] = out[1] = out[2] = 0;

	if (cls.state != ca_active)
		return;

	if (fldnum == CSQC_GE_MAXENTS)
	{
		// Runtime equivalent of FTE cl.maxlerpents: highest packet-entity number
		// seen this frame + headroom (FTE grows its lerp array in steps of 16).
		packet_entities_t *pack;
		int i, mx = 0;

		if (cl.validsequence)
		{
			pack = &cl.frames[cl.parsecount & UPDATE_MASK].packet_entities;
			for (i = 0; i < pack->num_entities; i++)
				mx = max (mx, pack->entities[i].number + 1);
		}
		mx = max (mx, MAX_CLIENTS);
		out[0] = (float)min (CL_MAX_EDICTS, mx + 16);
		return;
	}

	if (entnum < 0 || entnum >= CL_MAX_EDICTS)
		return;		// invalid entity -> 0 (FTE: "not valid")

	cent = &cl_entities[entnum];
	es = &cent->current;

	// Players are tracked through playerinfo (SetupPlayerEntity: cent->sequence =
	// state->messagenum) and are "present" when playerstate was updated this frame
	// (same test as CL_LinkPlayers); map entities use the packet-entity frame
	// sequence (CL_SetupPacketEntity: cent->sequence = cl.validsequence).
	ps = (entnum >= 1 && entnum <= MAX_CLIENTS)
		? &cl.frames[cl.parsecount & UPDATE_MASK].playerstate[entnum - 1] : NULL;
	if (ps && ps->messagenum == cl.parsecount)
		is_player = true, pnum = entnum - 1;

	active = is_player
		? true
		: (cent->sequence != 0 && cent->sequence == cl.validsequence);
	if (!active)
		return;

	modelindex = es->modelindex;

	// Interpolated origin + angles (T2.4): same lerp data as the renderer, applied
	// here so the local player / non-drawn entities are lerped too (their
	// cent->lerp_origin is not written by CL_LinkPlayers).
	CSQC_Client_EntityLerp (cent, org, ang);

	// Local player (T2.4, FTE parity): the server does not send the local player
	// its own viewangles (playerstate.viewangles is demo-only, client.h:128), so
	// use the client's own angles — the same source as the #371 bridge. FTE's
	// #504 for the local player returns the *model* pitch (le->angles[0] =
	// simangles[0]*0.333*r_meshpitch, cl_pred.c:1448-1451; r_meshpitch=-1 in QW),
	// i.e. exactly the renderer convention (cl_ents.c:2240: -viewangles[0]/3).
	// Remote players keep the raw packet angles (verified live: FTE and ezq
	// remote readings match), so only the local player gets the transform.
	if (is_player && pnum == cl.playernum)
	{
		VectorCopy (cl.viewangles, ang);
		ang[0] = -ang[0] / 3;
	}

	switch (fldnum)
	{
	case CSQC_GE_ACTIVE:
		out[0] = 1;
		break;
	case CSQC_GE_ORIGIN:
		VectorCopy (org, out);
		break;
	case CSQC_GE_ANGLES:
		VectorCopy (ang, out);
		break;
	case CSQC_GE_FORWARD:
	case CSQC_GE_RIGHT:
	case CSQC_GE_UP:
		AngleVectors (ang,
			(fldnum == CSQC_GE_FORWARD) ? out : NULL,
			(fldnum == CSQC_GE_RIGHT) ? out : NULL,
			(fldnum == CSQC_GE_UP) ? out : NULL);
		break;
	case CSQC_GE_ORIGINANDVECTORS:
		VectorCopy (org, out);
		CSQC_Client_MakeVectors (ang);	// module v_forward/v_right/v_up
		break;
	case CSQC_GE_MINS:
	case CSQC_GE_MAXS:
	case CSQC_GE_ABSMIN:
	case CSQC_GE_ABSMAX:
		{
			vec3_t mn, mx;
			if (is_player)
			{
				// FTE uses ps->szmins/szmaxs (hull); ezq keeps the prediction hull.
				extern vec3_t player_mins, player_maxs;
				VectorCopy (player_mins, mn);
				VectorCopy (player_maxs, mx);
			}
			else
			{
				// FTE decodes es->solidsize; ezq has none — approximate with the
				// model bounding box.
				model = (modelindex > 0 && modelindex < MAX_MODELS)
					? cl.model_precache[modelindex] : NULL;
				if (model)
				{
					VectorCopy (model->mins, mn);
					VectorCopy (model->maxs, mx);
				}
				else
					VectorClear (mn), VectorClear (mx);
			}
			if (fldnum == CSQC_GE_MINS)
				VectorCopy (mn, out);
			else if (fldnum == CSQC_GE_MAXS)
				VectorCopy (mx, out);
			else if (fldnum == CSQC_GE_ABSMIN)
				VectorAdd (org, mn, out);
			else
				VectorAdd (org, mx, out);
		}
		break;
	case CSQC_GE_SCALE:
		out[0] = 1;		// no scale in ezq state (FTE default 16/16) — deviation
		break;
	case CSQC_GE_ALPHA:
#ifdef FTE_PEXT_TRANS
		out[0] = es->trans / 255.0f;
#else
		out[0] = 1;
#endif
		break;
	case CSQC_GE_COLORMOD:
#ifdef FTE_PEXT_COLOURMOD
		out[0] = es->colourmod[0] / 8.0f;
		out[1] = es->colourmod[1] / 8.0f;
		out[2] = es->colourmod[2] / 8.0f;
#endif
		break;
	case CSQC_GE_PANTSCOLOR:
		out[0] = is_player ? (float)cl.players[pnum].bottomcolor
			: (float)(es->colormap & 15);
		break;
	case CSQC_GE_SHIRTCOLOR:
		out[0] = is_player ? (float)cl.players[pnum].topcolor
			: (float)((es->colormap >> 4) & 15);
		break;
	// Stage 4b: for players the source is player_state (FTE PF_getentity player
	// branch, pr_csqc.c:5909/5948/5954/5957) — SetupPlayerEntity does not copy
	// skinnum/effects into cent->current (cl_ents.c:1526-1531).
	case CSQC_GE_SKIN:
		out[0] = (float)(is_player ? ps->skinnum : es->skinnum);
		break;
	case CSQC_GE_LIGHT:
		out[0] = 0;
		break;
	case CSQC_GE_MODELINDEX:
		out[0] = (float)(is_player ? ps->modelindex : es->modelindex);
		break;
	case CSQC_GE_EFFECTS:
		out[0] = (float)(is_player ? ps->effects : es->effects);
		break;
	case CSQC_GE_FRAME:
		out[0] = (float)(is_player ? ps->frame : es->frame);
		break;
	default:
		// GE_MODELINDEX2/GE_FATNESS/GE_DRAWFLAGS/GE_ABSLIGHT/GE_GLOWSIZE/
		// GE_GLOWCOLOUR/GE_RTSTYLE/GE_RTPFLAGS/GE_RTRADIUS/GE_TAGENTITY/
		// GE_TAGINDEX/GE_GRAVITYDIR/GE_TRAILEFFECTNUM — no ezq data source
		// (documented deviation); FTE default (vec3 defaults to '1 1 1' for the
		// two glow/rt colour fields, 0 otherwise).
		if (fldnum == CSQC_GE_GLOWMOD || fldnum == CSQC_GE_RTCOLOUR)
			out[0] = out[1] = out[2] = 1;
		break;
	}
}

/*
=================
PR1VM_LumpFits / PR1VM_StmtWords / PR1VM_ValidateClientV6

A3: a downloaded csprogs.dat is server-supplied and must not be trusted. The
server PR1 core is only ever fed a locally-installed, CRC-checked progs, so its
loader trusts the header; the client loader must not. Validate the header lump
ranges against the file size and the operand/field ranges the interpreter
indexes (pr_exec.c:592-594 `vm->globals[st->a/b/c]`, `parm_start/locals`,
`first_statement`) before PR1VM_LoadData byte-swaps and walks the lumps.
=================
*/
static qbool PR1VM_LumpFits (int ofs, int num, int elemsize, int filesize, const char *name)
{
	if (ofs < (int)sizeof (dprograms_t) || num < 0 || elemsize <= 0 ||
		(size_t) ofs + (size_t) num * (size_t) elemsize > (size_t) filesize)
	{
		Con_Printf ("CSQC: csprogs.dat rejected: bad %s lump (ofs=%d num=%d)\n",
			name, ofs, num);
		return false;
	}
	return true;
}

// Width (in float words) of operand a/b/c (which 0/1/2) for the ops that read or
// write 3 words; all others are single-word. Branch ops (OP_GOTO/OP_IF/OP_IFNOT)
// store a *statement delta* in one operand (handled at the call site, not here).
static int PR1VM_StmtWords (int op, int which)
{
	switch (op)
	{
	case OP_DONE:
	case OP_RETURN:
		return which == 0 ? 3 : 1;
	case OP_MUL_V:
		return (which == 0 || which == 1) ? 3 : 1;
	case OP_ADD_V:
	case OP_SUB_V:
		return 3;
	case OP_MUL_FV:
		return (which == 1 || which == 2) ? 3 : 1;
	case OP_MUL_VF:
		return (which == 0 || which == 2) ? 3 : 1;
	case OP_EQ_V:
	case OP_NE_V:
		return (which == 0 || which == 1) ? 3 : 1;
	case OP_NOT_V:
		return which == 0 ? 3 : 1;
	case OP_STORE_V:
		return which == 1 ? 3 : 1;
	case OP_LOAD_V:
		return which == 2 ? 3 : 1;
	default:
		return 1;
	}
}

static qbool PR1VM_ValidateClientV6 (const byte *data, int filesize)
{
	dprograms_t h;
	const dstatement_t *st;
	const dfunction_t *fn;
	int i, j;

	if (!data || filesize < (int)sizeof (h))
	{
		Con_Printf ("PR1VM_LoadClientV6: file too small (%d bytes)\n", filesize);
		return false;
	}

	memcpy (&h, data, sizeof (h));
	for (i = 0; i < (int)(sizeof (h) / sizeof (int)); i++)
		((int *) &h)[i] = LittleLong (((int *) &h)[i]);

	if (h.version != PROG_VERSION)
	{
		Con_Printf ("PR1VM_LoadClientV6: not a QW v6 progs (version=%d)\n", h.version);
		return false;
	}

	if (!PR1VM_LumpFits (h.ofs_statements, h.numstatements, sizeof (dstatement_t), filesize, "statements") ||
		!PR1VM_LumpFits (h.ofs_globaldefs, h.numglobaldefs, sizeof (ddef_t), filesize, "globaldefs") ||
		!PR1VM_LumpFits (h.ofs_fielddefs, h.numfielddefs, sizeof (ddef_t), filesize, "fielddefs") ||
		!PR1VM_LumpFits (h.ofs_functions, h.numfunctions, sizeof (dfunction_t), filesize, "functions") ||
		!PR1VM_LumpFits (h.ofs_strings, h.numstrings, 1, filesize, "strings") ||
		!PR1VM_LumpFits (h.ofs_globals, h.numglobals, sizeof (float), filesize, "globals"))
		return false;

	if (h.numstatements < 1 || h.numfunctions < 1 || h.numstrings < 1 ||
		h.numglobals < 3 || h.entityfields <= 0)
	{
		Con_Printf ("CSQC: csprogs.dat rejected: bad counts (stmt=%d func=%d str=%d glob=%d ef=%d)\n",
			h.numstatements, h.numfunctions, h.numstrings, h.numglobals, h.entityfields);
		return false;
	}

	// statements: every operand indexes vm->globals[st->a/b/c]; vector ops use 3.
	// Branch deltas (OP_GOTO->a, OP_IF/OP_IFNOT->b) are targets, not globals —
	// bounded at run time in PR1VM_ExecuteProgram.
	st = (const dstatement_t *) ((const byte *) data + h.ofs_statements);
	for (i = 0; i < h.numstatements; i++)
	{
		int op = (int)(unsigned short) LittleShort ((short) st[i].op);
		int ops[3];

		if (op > OP_BITOR)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: bad opcode %d at statement %d\n", op, i);
			return false;
		}

		ops[0] = (int) LittleShort (st[i].a);
		ops[1] = (int) LittleShort (st[i].b);
		ops[2] = (int) LittleShort (st[i].c);
		for (j = 0; j < 3; j++)
		{
			if ((op == OP_GOTO && j == 0) || ((op == OP_IF || op == OP_IFNOT) && j == 1))
				continue;
			if (ops[j] < 0 || ops[j] > h.numglobals - PR1VM_StmtWords (op, j))
			{
				Con_Printf ("CSQC: csprogs.dat rejected: statement %d operand out of range\n", i);
				return false;
			}
		}
	}

	// functions: parm_start/locals within globals, first_statement within statements
	fn = (const dfunction_t *) ((const byte *) data + h.ofs_functions);
	for (i = 0; i < h.numfunctions; i++)
	{
		int first = LittleLong (fn[i].first_statement);
		int parm = LittleLong (fn[i].parm_start);
		int loc = LittleLong (fn[i].locals);
		int nparm = LittleLong (fn[i].numparms);

		if (first > 0 && first >= h.numstatements)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: function %d first_statement out of range\n", i);
			return false;
		}
		if (nparm < 0 || nparm > MAX_PARMS || parm < 0 || loc < 0 || parm + loc > h.numglobals)
		{
			Con_Printf ("CSQC: csprogs.dat rejected: function %d parms/locals out of range\n", i);
			return false;
		}
	}

	return true;
}

/*
=================
PR1VM_LoadClientV6

Client v6-loader (our csprogs.dat, classic QW version 6; v6 migration).
No CRC check; errors -> false + Con_Printf (no SV_Error). A3: structural
validation before executing server-supplied bytes. Implemented in the client
file (rule "client parts live outside shared core files").
=================
*/
static qbool PR1VM_LoadClientV6 (pr1vm_t *vm, const byte *data, int filesize)
{
	if (!PR1VM_ValidateClientV6 (data, filesize))
		return false;

	PR1VM_LoadData (vm, (dprograms_t *)data);
	return true;
}

/*
=================
PR1VM_CSQCSmoke_f

PR1VM (S3, debug): loads csprogs.dat (classic v6, migration P1) from the current
gamedir into a static client instance, resolves CSQC functions and runs
CSQC_WorldLoaded (empty body — client builtins not wired yet, S5).
Debug command lives in the client file (rule "client parts live outside shared");
registered from CSQC_Client_RegisterCommands (cl_main.c: CL_InitLocal).
=================
*/
static pr1vm_t csqc_smoke_vm;
// Отдельный строковый пул для debug-инстанса csqc_smoke (свой к vm).
static csqc_strpool_t csqc_smoke_strpool;

static void PR1VM_CSQCSmoke_f (void)
{
	byte *data;
	int filesize;
	pr1vm_t *vm = &csqc_smoke_vm;
	dfunction_t *f;
	func_t idx;

	data = (byte *)FS_LoadHunkFile ("csprogs.dat", &filesize);
	if (!data)
	{
		Con_Printf ("csqc_smoke: couldn't load csprogs.dat from gamedir\n");
		return;
	}

	// S6/P2.1: cleanup (incl. Q_free of builtin table), then reload
	PR1VM_UnLoad (vm);
	vm->get_string = CSQC_Client_GetString;	// option 2: bounded untrusted csprogs strings
	if (!PR1VM_LoadClientV6 (vm, data, filesize))
	{
		Con_Printf ("csqc_smoke: v6 load failed\n");
		return;
	}

	// Строковые таблицы debug-инстанса: свой пул (back-pointer в host_udata).
	memset (&csqc_smoke_strpool, 0, sizeof (csqc_smoke_strpool));
	vm->host_udata = &csqc_smoke_strpool;
	vm->strtbl = csqc_smoke_strpool.strtbl;
	vm->newstrtbl = csqc_smoke_strpool.newstrtbl;
	vm->numstr = &csqc_smoke_strpool.numstr;

	Con_Printf ("csqc_smoke: client (v6): statements=%d functions=%d globals=%d"
		" (server PR1: statements=%d functions=%d)\n",
		vm->progs->numstatements, vm->progs->numfunctions, vm->progs->numglobals,
		progs ? progs->numstatements : -1, progs ? progs->numfunctions : -1);

	// P2.1: client builtin table (layer C)
	CSQCVM_RegisterBuiltins (vm);

	f = PR1VM_FindFunction (vm, "CSQC_Init");
	Con_Printf ("csqc_smoke: CSQC_Init %s\n", f ? "found" : "MISSING");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		vm->globals[OFS_PARM0] = 0;
		vm->globals[OFS_PARM1] = 0;
		vm->globals[OFS_PARM2] = 0;
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: CSQC_Init executed ok (registercommand builtins)\n");
	}
	f = PR1VM_FindFunction (vm, "CSQC_WorldLoaded");
	Con_Printf ("csqc_smoke: CSQC_WorldLoaded %s\n", f ? "found" : "MISSING");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: CSQC_WorldLoaded executed ok (server PR1 still alive)\n");
	}
	f = PR1VM_FindFunction (vm, "CSQC_ConsoleCommand");
	Con_Printf ("csqc_smoke: CSQC_ConsoleCommand %s\n", f ? "found" : "MISSING");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		vm->globals[OFS_PARM0] = 0;	// empty command
		vm->globals[OFS_RETURN] = -1;
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: CSQC_ConsoleCommand ok (ret=%.0f, tokenize/argv builtins)\n",
			vm->globals[OFS_RETURN]);
	}
	// P2.2: weapon_name(0) -> ftos(0)="0" (builtin ftos + string return)
	f = PR1VM_FindFunction (vm, "weapon_name");
	if (f)
	{
		idx = (func_t)(f - vm->functions);
		vm->globals[OFS_PARM0] = 0;
		vm->globals[OFS_RETURN] = 0;
		PR1VM_ExecuteProgram (vm, idx);
		Con_Printf ("csqc_smoke: weapon_name(0) -> \"%s\" (ftos builtin)\n",
			CSQC_Client_GetString (vm, *(int *)&vm->globals[OFS_RETURN]));
	}
}

/*
=================
CSQC_Client_ProgsCheck_f

A3 debug canary (client console `csqc_progscheck`): verifies the load-time
validator (clean csprogs.dat -> accepted; synthetically corrupted copies ->
rejected) and runs the runtime-guard predicate unit tests
(PR1VM_TestGuards_f). Engine-side, FTE has no equivalent command — a recorded
deviation from the module-harness FTE-oracle rule (ADR 0023).
=================
*/
static void CSQC_Client_ProgsCheck_f (void)
{
	byte *data;
	byte *buf;
	dprograms_t *h;
	int filesize;
	int pass = 0, fail = 0;

	data = (byte *)FS_LoadHunkFile ("csprogs.dat", &filesize);
	if (!data || filesize < (int)sizeof (dprograms_t))
	{
		Con_Printf ("csqc_progscheck: no/short csprogs.dat in gamedir\n");
		return;
	}

#define PC_CHECK(name, cond) \
	do { if (cond) pass++; else { fail++; Con_Printf ("[CSQC-TEST] FAIL %s\n", name); } } while (0)

	// 1) the real file must pass validation
	PC_CHECK ("clean-accepted", PR1VM_ValidateClientV6 (data, filesize));

	// 2) corrupted copies must be rejected (scratch copy, LE-safe writes)
	buf = (byte *)Q_malloc (filesize);
	if (!buf)
	{
		Con_Printf ("csqc_progscheck: out of memory\n");
		return;
	}
	h = (dprograms_t *)buf;

	memcpy (buf, data, filesize);
	h->ofs_functions = LittleLong (filesize + 4096);
	PC_CHECK ("ofs-functions-oob", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	h->numstatements = LittleLong (0x7fffffff);
	PC_CHECK ("numstatements-huge", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	h->numglobals = LittleLong (0x7fffffff);
	PC_CHECK ("numglobals-huge", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	h->version = LittleLong (7);
	PC_CHECK ("bad-version", !PR1VM_ValidateClientV6 (buf, filesize));

	memcpy (buf, data, filesize);
	PC_CHECK ("tiny-size", !PR1VM_ValidateClientV6 (buf, 8));

	memset (buf, 0, filesize);
	PC_CHECK ("zeroed", !PR1VM_ValidateClientV6 (buf, filesize));

	Q_free (buf);

	// 3) client string accessor (option 2): a positive offset at/beyond the
	// module string block must be rejected; in-range stays readable. The bound
	// lives here (client layer), not in the shared PR1VM_GetString.
	{
		pr1vm_t tvm;
		dprograms_t xh;
		int ns = LittleLong (((dprograms_t *) data)->numstrings);

		memset (&tvm, 0, sizeof (tvm));
		memset (&xh, 0, sizeof (xh));
		xh.numstrings = ns;
		tvm.strings = (char *) data;
		tvm.progs = &xh;
		PC_CHECK ("clientstr-in-range", CSQC_Client_GetString (&tvm, 0) != NULL);
		PC_CHECK ("clientstr-oob", CSQC_Client_GetString (&tvm, ns) == NULL);
		PC_CHECK ("clientstr-nullvm", CSQC_Client_GetString (NULL, 0) == NULL);
	}

#undef PC_CHECK

	// 3) runtime-guard predicate unit tests (synthetic instance)
	PR1VM_TestGuards_f ();

	// 4) R3: entity-argument conversion bound (CSQC_Client_EntNum) — OOB must
	// clamp to world(0), never fault (FTE ProgsToEdict parity). Synthetic
	// instance: the helper only needs edict_size/max_edicts.
	{
		pr1vm_t tvm;
		int gpass = 0, gfail = 0;

		memset (&tvm, 0, sizeof (tvm));
		tvm.edict_size = 16;
		tvm.max_edicts = 4;

		PR1VM_GuardCheck ("ent_of-zero",     CSQC_Client_EntNum (&tvm, 0) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-3",        CSQC_Client_EntNum (&tvm, 3 * 16) == 3, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-over",     CSQC_Client_EntNum (&tvm, 4 * 16) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-negative", CSQC_Client_EntNum (&tvm, -1) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-huge",     CSQC_Client_EntNum (&tvm, 0x40000000) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-small",    CSQC_Client_EntNum (&tvm, 1) == 0, &gpass, &gfail);
		PR1VM_GuardCheck ("ent_of-nullvm",   CSQC_Client_EntNum (NULL, 3 * 16) == 0, &gpass, &gfail);

		Con_Printf ("[CSQC-TEST] SUMMARY group=ent_of pass=%d fail=%d\n", gpass, gfail);
	}

	Con_Printf ("[CSQC-TEST] SUMMARY group=progscheck pass=%d fail=%d\n", pass, fail);
}

/*
=================
CSQC_Client_Load

Загружает csprogs (path из gamedir; локальный файл или только что скачанный
csprogsvers/<crc>.dat) в клиентский инстанс и вызывает CSQC_Init. Возвращает
true при успехе. При неудаче печатает причину.
=================
*/
static qbool CSQC_Client_Load (const char *path)
{
	byte *data;
	int filesize;
	pr1vm_t *vm;
	dfunction_t *f;

	// C1 (Wave C): здесь намеренно FS_LoadHunkFile (низкий hunk), а не heap/temp —
	// PR1VM_LoadData не копирует буфер (vm->progs/... ссылаются в data), поэтому
	// данные должны жить до выгрузки модуля. Обоснование — ADR 0019/0031.
	data = (byte *)FS_LoadHunkFile ((char *)path, &filesize);
	if (!data)
	{
		Con_Printf ("CSQC: server offers csprogs but %s not found locally\n", path);
		return false;
	}

	// Защита от повторного Load (арена из прошлой загрузки) до memset.
	CSQC_Client_FreeArena ();
	// C2.2: string-buffers чистить при новой загрузке модуля.
	CSQC_Client_BufReset ();
	// E1a #371: снять регистрации deltalisten/карту player-моста.
	CSQC_Client_DeltaReset ();
	CSQC_Client_ViewReset ();
	CSQC_Client_ModelReset ();	// Ф3: CSQC-реестр моделей чистится при загрузке

	memset (&s_csqc, 0, sizeof (s_csqc));
	s_csqc.func_init = s_csqc.func_world = s_csqc.func_update =
		s_csqc.func_console = s_csqc.func_shutdown = -1;
	s_csqc.func_entupdate = s_csqc.func_entremove = s_csqc.func_parseevent = -1;
	s_csqc.func_parseprint = s_csqc.func_parsecp = -1;
	s_csqc.func_parsedamage = -1;
	s_csqc.func_eventsound = -1;
	s_csqc.func_parsesetangles = -1;
	s_csqc.func_rr = -1;
	s_csqc.func_entspawn = -1;
	s_csqc.mayread = false;
	s_csqc.func_input = -1;
	s_csqc.func_inputevent = -1;
	s_csqc.func_startframe = s_csqc.func_endframe = -1;
	s_csqc.global_time = -1;
	s_csqc.global_gamespeed = -1;
	s_csqc.global_self = -1;
	s_csqc.global_other = -1;
	s_csqc.global_physics_mode = -1;
	s_csqc.field_entnum = -1;
	s_csqc.f_origin = s_csqc.f_velocity = s_csqc.f_angles = s_csqc.f_mins = s_csqc.f_maxs = -1;
	s_csqc.f_movetype = s_csqc.f_flags = s_csqc.f_gravity = s_csqc.f_pmove_flags = -1;
	s_csqc.f_modelindex = s_csqc.f_skin = -1;
	s_csqc.f_frame = s_csqc.f_effects = s_csqc.f_colormap = s_csqc.f_drawmask = -1;
	s_csqc.f_think = s_csqc.f_nextthink = -1;
	s_csqc.g_localentnum = -1;
	s_csqc.in_timelength = s_csqc.in_angles = s_csqc.in_movevalues = -1;
	s_csqc.in_buttons = s_csqc.in_impulse = -1;
	s_csqc.in_sequence = -1;
	s_csqc.g_ccframe = s_csqc.g_scframe = -1;
	s_csqc.p_org = s_csqc.p_vel = s_csqc.p_onground = -1;
	s_csqc.g_vfwd = s_csqc.g_vright = s_csqc.g_vup = -1;
	s_csqc.g_view_angles = -1;
	s_csqc.g_frametime = s_csqc.g_cltime = s_csqc.g_maxclients = -1;
	s_csqc.g_player_localnum = s_csqc.g_intermission = -1;
	CSQC_Client_OffsetCacheReset ();	// C2 (Wave C): офсеты горячего пути — до резолва
	s_last_seq = 0;
	s_ccframe = 0;

	vm = &s_csqc.vm;
	vm->host_error = CSQC_Client_HostError;
	vm->host_print = CSQC_Client_HostPrint;
	vm->abortbuf_valid = true;	// A1: client VM unwinds via the abort-stack
	vm->get_string = CSQC_Client_GetString;	// option 2: bounded untrusted csprogs strings
	vm->stateop = CSQC_Client_StateOp;	// B21: OP_STATE по field/global модуля

	if (!PR1VM_LoadClientV6 (vm, data, filesize))
	{
		Con_Printf ("CSQC: %s load failed (v6)\n", path);
		return false;
	}

	// Строковые таблицы клиентского инстанса: vm->strtbl/newstrtbl/numstr ->
	// пул инстанса; host_udata — back-pointer для PR1VM_ClientSetString.
	vm->host_udata = &s_csqc.strpool;
	vm->strtbl = s_csqc.strpool.strtbl;
	vm->newstrtbl = s_csqc.strpool.newstrtbl;
	vm->numstr = &s_csqc.strpool.numstr;

	CSQCVM_RegisterBuiltins (vm);

	// P1/D2: арена edicts клиентского инстанса (edict_size известен после load).
	CSQC_Client_AllocArena (vm);

	f = PR1VM_FindFunction (vm, "CSQC_Init");
	if (f)
		s_csqc.func_init = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_WorldLoaded");
	if (f)
		s_csqc.func_world = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_UpdateView");
	if (f)
		s_csqc.func_update = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_ConsoleCommand");
	if (f)
		s_csqc.func_console = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Shutdown");
	if (f)
		s_csqc.func_shutdown = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Ent_Update");
	if (f)
		s_csqc.func_entupdate = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Ent_Remove");
	if (f)
		s_csqc.func_entremove = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Ent_Spawn");
	if (f)
		s_csqc.func_entspawn = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Parse_Event");
	if (f)
		s_csqc.func_parseevent = (int)(f - vm->functions);
	// Э1: сетевые печатные колбэки (FTE pr_common.h:1087-1088).
	f = PR1VM_FindFunction (vm, "CSQC_Parse_Print");
	if (f)
		s_csqc.func_parseprint = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Parse_CenterPrint");
	if (f)
		s_csqc.func_parsecp = (int)(f - vm->functions);
	// Э2: сетевой колбэк урона (FTE pr_common.h:1090).
	f = PR1VM_FindFunction (vm, "CSQC_Parse_Damage");
	if (f)
		s_csqc.func_parsedamage = (int)(f - vm->functions);
	// Э3: сетевой колбэк звука (FTE pr_common.h:1106, pr_csqc.c:9453).
	f = PR1VM_FindFunction (vm, "CSQC_Event_Sound");
	if (f)
		s_csqc.func_eventsound = (int)(f - vm->functions);
	// Э4: сетевой колбэк углов (FTE pr_common.h:1091, pr_csqc.c:9400).
	f = PR1VM_FindFunction (vm, "CSQC_Parse_SetAngles");
	if (f)
		s_csqc.func_parsesetangles = (int)(f - vm->functions);
	// Э5: движковый колбэк переинициализации рендерера (FTE pr_common.h:1096, pr_csqc.c:8314).
	f = PR1VM_FindFunction (vm, "CSQC_RendererRestarted");
	if (f)
		s_csqc.func_rr = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_Input_Frame");
	if (f)
		s_csqc.func_input = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "CSQC_InputEvent");
	if (f)
		s_csqc.func_inputevent = (int)(f - vm->functions);
	// T2.7 (R9): CSQC think-loop — StartFrame/EndFrame (FTE pr_common.h:1112-1113).
	f = PR1VM_FindFunction (vm, "StartFrame");
	if (f)
		s_csqc.func_startframe = (int)(f - vm->functions);
	f = PR1VM_FindFunction (vm, "EndFrame");
	if (f)
		s_csqc.func_endframe = (int)(f - vm->functions);

	s_csqc.global_time = PR1VM_FindGlobal (vm, "time");
	// T2.1: gamespeed (csdefs.qc:166; engine-set). QW/ezq не имеет cl.gamespeed,
	// поэтому публикуем 1 (0 при серверной паузе) — см. CSQC_Client_Update.
	s_csqc.global_gamespeed = PR1VM_FindGlobal (vm, "gamespeed");
	// P2/D3: self-глобал и поле .entnum (движок пишет их при entity-вызовах).
	s_csqc.global_self = PR1VM_FindGlobal (vm, "self");
	// T2.7: other (world для StartFrame/EndFrame/thinks; FTE CSQC_Event_Think) и
	// physics_mode (csdefs.qc:163, default 2).
	s_csqc.global_other = PR1VM_FindGlobal (vm, "other");
	s_csqc.global_physics_mode = PR1VM_FindGlobal (vm, "physics_mode");
	s_csqc.field_entnum = CSQC_Client_FindField (vm, "entnum");
	// C1.4 #347: поля стандартной физики (если есть в схеме модуля).
	s_csqc.f_origin = CSQC_Client_FindField (vm, "origin");
	s_csqc.f_velocity = CSQC_Client_FindField (vm, "velocity");
	s_csqc.f_angles = CSQC_Client_FindField (vm, "angles");
	s_csqc.f_mins = CSQC_Client_FindField (vm, "mins");
	s_csqc.f_maxs = CSQC_Client_FindField (vm, "maxs");
	s_csqc.f_movetype = CSQC_Client_FindField (vm, "movetype");
	s_csqc.f_flags = CSQC_Client_FindField (vm, "flags");
	s_csqc.f_gravity = CSQC_Client_FindField (vm, "gravity");
	s_csqc.f_pmove_flags = CSQC_Client_FindField (vm, "pmove_flags");
	s_csqc.f_modelindex = CSQC_Client_FindField (vm, "modelindex");
	s_csqc.f_skin = CSQC_Client_FindField (vm, "skin");
	s_csqc.f_frame = CSQC_Client_FindField (vm, "frame");
	s_csqc.f_effects = CSQC_Client_FindField (vm, "effects");
	s_csqc.f_colormap = CSQC_Client_FindField (vm, "colormap");
	s_csqc.f_drawmask = CSQC_Client_FindField (vm, "drawmask");
	// T2.7 think-loop: поля .think/.nextthink (csdefs.qc:90,92).
	s_csqc.f_think = CSQC_Client_FindField (vm, "think");
	s_csqc.f_nextthink = CSQC_Client_FindField (vm, "nextthink");
	s_csqc.g_localentnum = PR1VM_FindGlobal (vm, "player_localentnum");

	// input_* глобалы для CSQC_Input_Frame (csdefs.qc: input_timelength/angles/
	// movevalues/buttons/impulse). Резолвим только объявленные модулем.
	s_csqc.in_timelength = PR1VM_FindGlobal (vm, "input_timelength");
	s_csqc.in_angles = PR1VM_FindGlobal (vm, "input_angles");
	s_csqc.in_movevalues = PR1VM_FindGlobal (vm, "input_movevalues");
	s_csqc.in_buttons = PR1VM_FindGlobal (vm, "input_buttons");
	s_csqc.in_impulse = PR1VM_FindGlobal (vm, "input_impulse");
	s_csqc.in_sequence = PR1VM_FindGlobal (vm, "input_sequence");
	// C5-A: глобалы окна предикции + deprec pmove_* (csdefs.qc:50-51,69-71).
	s_csqc.g_ccframe = PR1VM_FindGlobal (vm, "clientcommandframe");
	s_csqc.g_scframe = PR1VM_FindGlobal (vm, "servercommandframe");
	s_csqc.p_org = PR1VM_FindGlobal (vm, "pmove_org");
	s_csqc.p_vel = PR1VM_FindGlobal (vm, "pmove_vel");
	s_csqc.p_onground = PR1VM_FindGlobal (vm, "pmove_onground");
	// #1 makevectors (C6.1): цели записи v_forward/v_right/v_up (FTE-паритет).
	s_csqc.g_vfwd = PR1VM_FindGlobal (vm, "v_forward");
	s_csqc.g_vright = PR1VM_FindGlobal (vm, "v_right");
	s_csqc.g_vup = PR1VM_FindGlobal (vm, "v_up");
	// C5-E Ф1: глобал view_angles (публикуется каждый кадр).
	s_csqc.g_view_angles = PR1VM_FindGlobal (vm, "view_angles");
	// B4: симулированные глобалы (FTE pr_csqc.c:8818-8838).
	s_csqc.g_frametime = PR1VM_FindGlobal (vm, "frametime");
	s_csqc.g_cltime = PR1VM_FindGlobal (vm, "cltime");
	s_csqc.g_maxclients = PR1VM_FindGlobal (vm, "maxclients");
	s_csqc.g_player_localnum = PR1VM_FindGlobal (vm, "player_localnum");
	s_csqc.g_intermission = PR1VM_FindGlobal (vm, "intermission");

	// C2 (Wave C): резолв кэша офсетов горячего пути (traceline/addentities).
	CSQC_Client_OffsetCacheResolve (vm);

	s_csqc.loaded = true;

	Con_Printf ("CSQC: loaded %s (%d statements, crc=0x%x), funcs i=%d w=%d u=%d "
		"c=%d s=%d eu=%d er=%d pe=%d if=%d ie=%d time=%d\n",
		path, vm->progs->numstatements, (unsigned int)vm->progs->crc, s_csqc.func_init,
		s_csqc.func_world, s_csqc.func_update, s_csqc.func_console, s_csqc.func_shutdown,
		s_csqc.func_entupdate, s_csqc.func_entremove, s_csqc.func_parseevent,
		s_csqc.func_input, s_csqc.func_inputevent, s_csqc.global_time);
	Con_Printf ("CSQC: T2.7 sf=%d ef=%d pm=%d think=%d nextthink=%d\n",
		s_csqc.func_startframe, s_csqc.func_endframe, s_csqc.global_physics_mode,
		s_csqc.f_think, s_csqc.f_nextthink);
	Con_Printf ("CSQC: P2 self=%d entnum_fld=%d edict_size=%d es=%d\n",
		s_csqc.global_self, s_csqc.field_entnum, vm->edict_size, s_csqc.func_entspawn);

	// CSQC_Init(apiver, enginename, enginever) — FTE-паритет (pr_csqc.c:8285-8287):
	// apiver = CSQC_API_VERSION, enginename = имя движка, enginever = номер версии.
	// Модуль аргументы использует только как хинты (TF2003/fo-qwprogs их игнорируют).
	if (s_csqc.func_init > 0)
	{
		vm->globals[OFS_PARM0] = CSQC_API_VERSION;
		PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM1], "ezQuake");
		vm->globals[OFS_PARM2] = VERSION_NUM;
		CSQC_Client_Exec (s_csqc.func_init);
		s_csqc.inited = !s_csqc.errored;
	}
	// Э5: сразу после CSQC_Init уведомить модуль о (пере)инициализации рендерера — FTE-паритет
	// (pr_csqc.c:8305 `CSQC_RendererRestarted(true)`), до первого CSQC_WorldLoaded.
	CSQC_Client_RendererRestarted (R_RendererDescription ());
	// C3 (Wave C): модуль зарегистрировал csqc_dbg через registercvar (#93) в
	// CSQC_Init — кэшируем указатель для горячего пути (CSQC_Client_ParseEntities).
	s_csqc.csqc_dbg_cvar = Cvar_Find ("csqc_dbg");
	return true;
}

/*
=================
CSQC_Client_NotifyCSQC

FTE-паритет (cl_parse.c:1526-1535): сообщить серверу, получает ли наш модуль
CSQC-поток. enablecsqc — модуль загружен и готов (после CSQC_WorldLoaded);
disablecsqc — сервер предложил CSQC, но модуль не запустился (T1.6a, D-J).
Идемпотентно (повторное состояние не отправляем); в демо/без коннекта — no-op
(CL_SendClientCommand).
=================
*/
static void CSQC_Client_NotifyCSQC (qbool enable)
{
#ifdef FTE_PEXT_CSQC
	if (!(cls.fteprotocolextensions & FTE_PEXT_CSQC))
		return;
#endif
	if (s_csqc.enable_sent && s_csqc.enable_value == enable)
		return;
	CL_SendClientCommand (true, enable ? "enablecsqc" : "disablecsqc");
	s_csqc.enable_sent = true;
	s_csqc.enable_value = enable;
	Con_Printf ("CSQC: %s sent\n", enable ? "enablecsqc" : "disablecsqc");
}

/*
=================
CSQC_Client_ConnectCheck

Вызывается при входе в мир (CL_MakeActive, до ca_active) — момент, когда весь
контент (включая csprogs.dat) уже доступен в FS (аналог преспауна FTE).
Если сервер предлагает CSQC (*csprogssize) и модуль ещё не загружен —
грузим и вызываем CSQC_Init.
=================
*/
void CSQC_Client_ConnectCheck (void)
{
	extern cvar_t cl_pext_csqc;
	extern cvar_t cl_download_csprogs;
	const char *name, *crcs;
	unsigned crc;
	int sizep;
	char path[MAX_QPATH];
	char *crcend;
	qbool anycsqc;

	// B4: клиентский map-uptime (FTE cltime = realtime-cl.mapstarttime) и база
	// frametime (cl.time - prev). Ставится на каждый вход в мир.
	s_mapstarttime = cls.realtime;
	s_prev_cltime = cl.time;

	// Мастер-выключатель (аналог FTE cl_nocsqc): 0 — весь CSQC отключён,
	// модуль не грузится, клиент ведёт себя как раньше.
	if (!cl_pext_csqc.value)
		return;

	sizep = (int)strtoul (Info_ValueForKey (cl.serverinfo, "*csprogssize"), NULL, 0);

	// anycsqc (T1.6b, FTE-паритет): сервер разрешает грузить локальный csprogs без
	// сверки crc (pr_csqc.c:7779); «битый» *csprogs (trailing-мусор) FTE тоже
	// трактует как anycsqc (cl_parse.c:1342-1347). В демо сверки нет (pr_csqc.c:7777).
	anycsqc = atoi (Info_ValueForKey (cl.serverinfo, "anycsqc")) != 0;
	crcs = Info_ValueForKey (cl.serverinfo, "*csprogs");
	crc = (unsigned)strtoul (crcs, &crcend, 0);
	if (crcs[0] && *crcend)
	{
		Con_Printf ("CSQC: corrupt *csprogs key in serverinfo\n");
		anycsqc = true;
		crc = 0;
	}
	if (cls.demoplayback)
		anycsqc = true;

	if (sizep <= 0 && !anycsqc)
		return;		// обычный сервер без CSQC (или PR1-гейт сервера)

	name = Info_ValueForKey (cl.serverinfo, "*csprogsname");
	if (!name || !name[0])
		name = "csprogs.dat";

	// Модуль загружается «с нуля» на КАЖДЫЙ вход в мир (первый коннект и каждая
	// смена карты): выгрузка происходит при выходе из мира (CL_ClearState, до
	// Host_ClearMemory), здесь — загрузка свежего csprogs. Защитный unload на
	// случай путей без CL_ClearState (двойной вызов безопасен — no-op).
	if (s_csqc.loaded)
		CSQC_Client_Disconnect ();

	// Локальные кандидаты по FTE-семантике (CSQC_FindMainProgs, pr_csqc.c):
	// 1) кэш csprogsvers/<crc>.dat, 2) *csprogsname (+ фолбэк csprogs.dat);
	// при валидном name-файле делается write-back копии в кэш. anycsqc/demo —
	// без сверки size/crc (T1.6b).
	if (CSQC_Client_FindMainProgs (path, sizeof (path), name, sizep, crc, anycsqc))
	{
		if (!CSQC_Client_Load (path))
		{
			CSQC_Client_NotifyCSQC (false);
			return;
		}
		if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		{
			CSQC_Client_NotifyCSQC (false);
			return;
		}
		// Вход в новую карту: per-карта состояние чистое (WorldLoaded/enablecsqc
		// будут этой карты; модуль уже новый).
		memset (s_csqc.seen, 0, sizeof (s_csqc.seen));
		s_csqc.world_done = false;
		s_csqc.enable_sent = false;
		return;
	}

	// Демо/MVD: локального csprogs нет, скачивание в демо недоступно
	// (StartDownload — no-op) — pending не ставим (иначе ложный таймаут, T1.6b).
	if (cls.demoplayback)
	{
		Con_Printf ("CSQC: no local csprogs for demo playback\n");
		return;
	}

	// Скачивание csprogs запрещено cvar'ом (FTE-паритет: cl_download_csprogs):
	// модуль не грузится, сообщаем серверу disablecsqc (D-J/T1.6a).
	if (!cl_download_csprogs.value)
	{
		Con_Printf ("CSQC: not downloading %s (cl_download_csprogs 0)\n", name);
		CSQC_Client_NotifyCSQC (false);
		return;
	}

	// Валидного локального нет — качаем с сервера: сервер отдаёт *csprogsname,
	// сохраняем в отдельную папку csprogsvers/<crc>.dat (не перезатираем чужие).
	// Загрузка модуля произойдёт в CSQC_Client_Update, когда файл появится.
	s_csqc.csprogs_crc = crc;
	s_csqc.csprogs_size = sizep;
	if (crc)
		snprintf (s_csqc.csprogs_dl_path, sizeof (s_csqc.csprogs_dl_path), "csprogsvers/%x.dat", crc);
	else
		snprintf (s_csqc.csprogs_dl_path, sizeof (s_csqc.csprogs_dl_path), "%s", name);
	CSQC_Client_StartDownload (name, s_csqc.csprogs_dl_path);
	s_csqc.csprogs_dl_pending = true;
}

/*
=================
C5-A: окно предикции EXT_CSQC_1 — значения глобалов модуля
clientcommandframe/servercommandframe (csdefs.qc:50-51).

- clientcommandframe = «живой» (последний собранный) клиентский кадр = s_ccframe
  (аналог FTE cl.movesequence, pr_csqc.c:8841/9430-9431). Ставится в
  CSQC_Client_InputFrame на сборке cmd и НЕ пересчитывается после отправки:
  Netchan_Transmit инкрементирует outgoing_sequence (net_chan.c:319), но
  clientcommandframe остаётся номером собранного cmd. Это то, что просит
  #345(clientcommandframe) — живой pending-кадр (T2.2); следующего ещё нет.
- servercommandframe = последний подтверждённый сервером клиентский кадр =
  cl.parsecount (CL_ParseClientdata ставит его в cls.netchan.incoming_acknowledged,
  cl_parse.c:2050-2054) — аналог FTE QW ackedmovesequence.
- Предикция недоступна (0): демо/MVD, не ca_active, до первого принятого
  серверного кадра (cl.validsequence == 0; client.h:647-650).
- Окно (servercommandframe, clientcommandframe] — контракт модуля (движок его
  не проверяет; спека ext_csqc_1.txt:262) — см. CSQC_Client_ApplyInput.
=================
*/
static float CSQC_Client_ClientCmdFrame (void)
{
	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (cls.state != ca_active || cls.demoplayback || cls.mvdplayback)
		return 0;
	// T2.2: последний собранный кадр (FTE cl.movesequence), не следующая
	// outgoing_sequence — иначе в render-фазе #345(clientcommandframe) просит ещё
	// не собранный seq и получает 0 (см. s_ccframe).
	return (float)s_ccframe;
}

static float CSQC_Client_ServerCmdFrame (void)
{
	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (cls.state != ca_active || cls.demoplayback || cls.mvdplayback)
		return 0;
	if (!cl.validsequence)
		return 0;	// ни одного принятого серверного кадра (преспаун)
	return (float)cl.parsecount;
}

static void CSQC_Client_PatchFrames (void)
{
	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.g_ccframe >= 0)
		s_csqc.vm.globals[s_csqc.g_ccframe] = CSQC_Client_ClientCmdFrame ();
	if (s_csqc.g_scframe >= 0)
		s_csqc.vm.globals[s_csqc.g_scframe] = CSQC_Client_ServerCmdFrame ();
}

/*
=================
T2.7 (R9): CSQC think-loop — per-frame жизненный цикл модуля в 3D-takeover.

FTE-паритет (CSQC_DrawView pr_csqc.c:8740-8811; CSQC_Event_Think pr_csqc.c:7576-7586):
StartFrame → thinks (.nextthink/.think, single-think NQ-стиль) → EndFrame. StartFrame/
EndFrame: self/other = world, time = кадровое. think: self = сущность, other = world,
time = кадровое (FTE перекрывает thinktime физикстаймом), nextthink обнуляется до вызова.
thinks — только при physics_mode != 0. Отклонения (accept+doc): World_Physics_Frame
mode 2 (movetypes), customphysics и PR_RunThreads (у PR1VM нет sleep/fork → no-op).
=================
*/
static void CSQC_Client_RunFrameThink (void)
{
	pr1vm_t *vm = &s_csqc.vm;
	int mode, slot;
	float t, frame;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;

	// База — модульный time (Q4; Sys_DoubleTime), окно — кадровый интервал (аналог
	// FTE host_frametime).
	CSQC_Client_SetTime ();
	t = (s_csqc.global_time >= 0) ? vm->globals[s_csqc.global_time] : 0;
	frame = (float)cls.frametime;
	if (frame < 0)
		frame = 0;

	// physics_mode: csdefs.qc:163 default 2; 0 = «original csqc» — физика не гоняется.
	mode = (s_csqc.global_physics_mode >= 0)
		? (int)vm->globals[s_csqc.global_physics_mode] : 2;

	// StartFrame: self/other = world (FTE pr_csqc.c:8746-8749).
	if (s_csqc.func_startframe > 0)
	{
		if (s_csqc.global_self >= 0)
			*(int *)&vm->globals[s_csqc.global_self] = 0;
		if (s_csqc.global_other >= 0)
			*(int *)&vm->globals[s_csqc.global_other] = 0;
		if (s_csqc.global_time >= 0)
			vm->globals[s_csqc.global_time] = t;
		PR1VM_ExecuteProgram (vm, (func_t)s_csqc.func_startframe);
		if (s_csqc.errored)
			return;
	}

	// PR_RunThreads: PR1VM не имеет sleep/fork → no-op (FTE-паритет без тредов).

	// thinks: mode1 (DP-compat) и mode2 (movetypes) — у нас только thinks (mode2
	// movetypes accept+doc). Слот 0 = world, free/незанятые пропускаем.
	if (mode != 0 && s_csqc.f_think >= 0 && s_csqc.f_nextthink >= 0)
	{
		for (slot = 1; slot < CSQC_MAX_EDICTS; slot++)
		{
			float *base, nt;
			int thinkfunc;

			if (!s_used[slot])
				continue;
			base = (float *)((byte *)vm->game_edicts + (size_t)slot * vm->edict_size);
			nt = base[s_csqc.f_nextthink];
			if (nt <= 0 || nt > t + frame)
				continue;
			thinkfunc = *(int *)&base[s_csqc.f_think];
			base[s_csqc.f_nextthink] = 0;
			if (thinkfunc <= 0)
				continue;	// nextthink без think — пропуск (FTE пишет варн)
			// FTE CSQC_Event_Think: self=сущность, other=world, time=кадровое.
			if (s_csqc.global_self >= 0)
				*(int *)&vm->globals[s_csqc.global_self] = (int)slot * vm->edict_size;
			if (s_csqc.global_other >= 0)
				*(int *)&vm->globals[s_csqc.global_other] = 0;
			if (s_csqc.global_time >= 0)
				vm->globals[s_csqc.global_time] = t;
			PR1VM_ExecuteProgram (vm, (func_t)thinkfunc);
			if (s_csqc.errored)
				return;	// edict мог self-удалиться — цикл по s_used безопасен
		}
	}

	// EndFrame: self/other = world (FTE pr_csqc.c:8752-8757).
	if (s_csqc.func_endframe > 0)
	{
		if (s_csqc.global_self >= 0)
			*(int *)&vm->globals[s_csqc.global_self] = 0;
		if (s_csqc.global_other >= 0)
			*(int *)&vm->globals[s_csqc.global_other] = 0;
		if (s_csqc.global_time >= 0)
			vm->globals[s_csqc.global_time] = t;
		PR1VM_ExecuteProgram (vm, (func_t)s_csqc.func_endframe);
	}
}

/*
=================
CSQC_Client_NotMenu / CSQC_Client_KeynumToQC / CSQC_Client_QCToKeynum

B1/B6 (FTE-parity). Эталон FTE: notmenu — pr_csqc.c:8889
(`!Key_Dest_Has(kdm_menu|kdm_cwindows)`); трансляция клавиш —
pr_clcmd.c:14 (`MP_TranslateFTEtoQCCodes`, FTE->QC) и :218 (`MP_TranslateQCtoFTECodes`,
QC->FTE). Внутренний домен ezq — keys.h:28-213 (K_*, K_MOUSE1=200, K_MWHEELUP=244);
QC/CSQC-контракт — DP-нумерация (csdefs.qc:1377-1449).
Модуль получает/отдаёт только QC-коды; неизвестные ключи уходят «нативными»
(отрицательное значение собственного keynum) — как FTE-дефолт, round-trip сохраняется.
=================
*/
qbool CSQC_Client_NotMenu (void)
{
	// Вариант (b): любое слоёное меню скрывает игру (FTE kdm_menu|kdm_cwindows);
	// консоль/чат/стартовое демо остаются notmenu=1 (FTE не исключает kdm_console).
	return !(key_dest == key_menu || key_dest == key_hudeditor
		|| key_dest == key_demo_controls || key_dest == key_startupdemo_menu);
}

int CSQC_Client_KeynumToQC (int keynum)
{
	switch (keynum)
	{
	case K_TAB:			return 9;
	case K_ENTER:		return 13;
	case K_ESCAPE:		return 27;
	case K_SPACE:		return 32;
	case K_BACKSPACE:	return 127;

	case K_UPARROW:		return 128;
	case K_DOWNARROW:	return 129;
	case K_LEFTARROW:	return 130;
	case K_RIGHTARROW:	return 131;

	case K_LALT:		return 132;
	case K_LCTRL:		return 133;
	case K_LSHIFT:		return 134;

	case K_F1:			return 135;
	case K_F2:			return 136;
	case K_F3:			return 137;
	case K_F4:			return 138;
	case K_F5:			return 139;
	case K_F6:			return 140;
	case K_F7:			return 141;
	case K_F8:			return 142;
	case K_F9:			return 143;
	case K_F10:			return 144;
	case K_F11:			return 145;
	case K_F12:			return 146;

	case K_INS:			return 147;
	case K_DEL:			return 148;
	case K_PGDN:		return 149;
	case K_PGUP:		return 150;
	case K_HOME:		return 151;
	case K_END:			return 152;
	case K_PAUSE:		return 153;

	case KP_NUMLOCK:	return 154;
	case K_CAPSLOCK:	return 155;
	case K_SCRLCK:		return 156;

	case KP_INS:		return 157;
	case KP_END:		return 158;
	case KP_DOWNARROW:	return 159;
	case KP_PGDN:		return 160;
	case KP_LEFTARROW:	return 161;
	case KP_5:			return 162;
	case KP_RIGHTARROW:	return 163;
	case KP_HOME:		return 164;
	case KP_UPARROW:	return 165;
	case KP_PGUP:		return 166;
	case KP_DEL:		return 167;
	case KP_SLASH:		return 168;
	case KP_STAR:		return 169;
	case KP_MINUS:		return 170;
	case KP_PLUS:		return 171;
	case KP_ENTER:		return 172;

	case K_PRINTSCR:	return 174;

	// mouse: DP интерливит колёса между MOUSE3 и MOUSE4 (pr_clcmd.c:74-83).
	case K_MOUSE1:		return 512;
	case K_MOUSE2:		return 513;
	case K_MOUSE3:		return 514;
	case K_MWHEELUP:	return 515;
	case K_MWHEELDOWN:	return 516;
	case K_MOUSE4:		return 517;
	case K_MOUSE5:		return 518;
	case K_MOUSE6:		return 519;
	case K_MOUSE7:		return 520;
	case K_MOUSE8:		return 521;

	case K_JOY1:		return 768;
	case K_JOY2:		return 769;
	case K_JOY3:		return 770;
	case K_JOY4:		return 771;

	// FTE K_AUX1..16 -> 800..815 (замечание: csdefs.qc даёт 784..799 —
	// известное расхождение констант модуля, вне Э5; см. ADR 0032).
	case K_AUX1:		return 800;
	case K_AUX2:		return 801;
	case K_AUX3:		return 802;
	case K_AUX4:		return 803;
	case K_AUX5:		return 804;
	case K_AUX6:		return 805;
	case K_AUX7:		return 806;
	case K_AUX8:		return 807;
	case K_AUX9:		return 808;
	case K_AUX10:		return 809;
	case K_AUX11:		return 810;
	case K_AUX12:		return 811;
	case K_AUX13:		return 812;
	case K_AUX14:		return 813;
	case K_AUX15:		return 814;
	case K_AUX16:		return 815;

	default:
		if (keynum == -1)			// модуль передал «нет клавиши»
			return keynum;
		if (keynum < 0)				// уже нативный отрицательный код
			return -keynum;
		if (keynum >= 0 && keynum < 128)	// printable/control ascii — identity
			return keynum;
		return -keynum;			// нет QC-эквивалента — нативный код
	}
}

int CSQC_Client_QCToKeynum (int code)
{
	switch (code)
	{
	case 9:			return K_TAB;
	case 13:		return K_ENTER;
	case 27:		return K_ESCAPE;
	case 32:		return K_SPACE;
	case 127:		return K_BACKSPACE;

	case 128:		return K_UPARROW;
	case 129:		return K_DOWNARROW;
	case 130:		return K_LEFTARROW;
	case 131:		return K_RIGHTARROW;

	case 132:		return K_LALT;
	case 133:		return K_LCTRL;
	case 134:		return K_LSHIFT;

	case 135:		return K_F1;
	case 136:		return K_F2;
	case 137:		return K_F3;
	case 138:		return K_F4;
	case 139:		return K_F5;
	case 140:		return K_F6;
	case 141:		return K_F7;
	case 142:		return K_F8;
	case 143:		return K_F9;
	case 144:		return K_F10;
	case 145:		return K_F11;
	case 146:		return K_F12;

	case 147:		return K_INS;
	case 148:		return K_DEL;
	case 149:		return K_PGDN;
	case 150:		return K_PGUP;
	case 151:		return K_HOME;
	case 152:		return K_END;
	case 153:		return K_PAUSE;

	case 154:		return KP_NUMLOCK;
	case 155:		return K_CAPSLOCK;
	case 156:		return K_SCRLCK;

	case 157:		return KP_INS;
	case 158:		return KP_END;
	case 159:		return KP_DOWNARROW;
	case 160:		return KP_PGDN;
	case 161:		return KP_LEFTARROW;
	case 162:		return KP_5;
	case 163:		return KP_RIGHTARROW;
	case 164:		return KP_HOME;
	case 165:		return KP_UPARROW;
	case 166:		return KP_PGUP;
	case 167:		return KP_DEL;
	case 168:		return KP_SLASH;
	case 169:		return KP_STAR;
	case 170:		return KP_MINUS;
	case 171:		return KP_PLUS;
	case 172:		return KP_ENTER;

	case 174:		return K_PRINTSCR;

	case 512:		return K_MOUSE1;
	case 513:		return K_MOUSE2;
	case 514:		return K_MOUSE3;
	case 515:		return K_MWHEELUP;
	case 516:		return K_MWHEELDOWN;
	case 517:		return K_MOUSE4;
	case 518:		return K_MOUSE5;
	case 519:		return K_MOUSE6;
	case 520:		return K_MOUSE7;
	case 521:		return K_MOUSE8;

	case 768:		return K_JOY1;
	case 769:		return K_JOY2;
	case 770:		return K_JOY3;
	case 771:		return K_JOY4;

	case 800:		return K_AUX1;
	case 801:		return K_AUX2;
	case 802:		return K_AUX3;
	case 803:		return K_AUX4;
	case 804:		return K_AUX5;
	case 805:		return K_AUX6;
	case 806:		return K_AUX7;
	case 807:		return K_AUX8;
	case 808:		return K_AUX9;
	case 809:		return K_AUX10;
	case 810:		return K_AUX11;
	case 811:		return K_AUX12;
	case 812:		return K_AUX13;
	case 813:		return K_AUX14;
	case 814:		return K_AUX15;
	case 815:		return K_AUX16;

	default:
		if (code == -1)				// модуль передал «нет клавиши»
			return code;
		if (code < 0)				// нативный код — обратно
			return -code;
		if (code >= 0 && code < 128)	// printable/control ascii — identity
			return code;
		return -code;				// нет ezq-эквивалента
	}
}

/*
=================
CSQC_Client_PublishSimGlobals

B4 (FTE pr_csqc.c:8818-8838): симулированные глобалы модуля — frametime, cltime,
maxclients, player_localnum, intermission. Публикуются каждый кадр до
CSQC_UpdateView. frametime — дельта клиентского времени (FTE cl.time-cl.lasttime);
cltime — клиентский map-uptime; maxclients — serverinfo (QW-ключ).
=================
*/
static void CSQC_Client_PublishSimGlobals (void)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (s_csqc.g_frametime >= 0)
		vm->globals[s_csqc.g_frametime] = cl.paused
			? 0 : (float)bound (0, cl.time - s_prev_cltime, 0.1);
	if (s_csqc.g_cltime >= 0)
		vm->globals[s_csqc.g_cltime] = (float)(cls.realtime - s_mapstarttime);
	if (s_csqc.g_maxclients >= 0)
	{
		const char *mc = Info_ValueForKey (cl.serverinfo, "maxclients");
		vm->globals[s_csqc.g_maxclients] = (float)((mc && mc[0]) ? atoi (mc) : 0);
	}
	if (s_csqc.g_player_localnum >= 0)
		vm->globals[s_csqc.g_player_localnum] = (cl.viewplayernum >= 0) ? cl.viewplayernum : 0;
	if (s_csqc.g_intermission >= 0)
		vm->globals[s_csqc.g_intermission] = cl.intermission ? 1 : 0;

	s_prev_cltime = cl.time;
}

/*
=================
CSQC_Client_Update

Вызывается каждый 2D-кадр (HUD-фаза, cl_screen.c). WorldLoaded — один раз
после входа в мир; далее CSQC_UpdateView(vid.width, vid.height, notmenu).
Если модуль ждёт скачивания csprogs — при появлении валидного файла грузит
его и продолжает как при входе в мир.
=================
*/
void CSQC_Client_Update (void)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (cls.state != ca_active)
		return;

	// Clip-состояние (#324/325) — пер-кадр (модуль ставит/снимает в своём кадре).
	s_clip_on = false;

	// Ожидание скачанного csprogs (валидный файл появился -> грузим).
	if (s_csqc.csprogs_dl_pending)
	{
		char path[MAX_QPATH];

		if (CSQC_Client_ValidateFile (s_csqc.csprogs_dl_path,
			s_csqc.csprogs_size, s_csqc.csprogs_crc))
		{
			strlcpy (path, s_csqc.csprogs_dl_path, sizeof (path));
			s_csqc.csprogs_dl_pending = false;
			if (!CSQC_Client_Load (path))
			{
				CSQC_Client_NotifyCSQC (false);
				return;
			}
			if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
			{
				CSQC_Client_NotifyCSQC (false);
				return;
			}
			// как при входе в мир: per-карта состояние чистое
			memset (s_csqc.seen, 0, sizeof (s_csqc.seen));
			s_csqc.world_done = false;
			s_csqc.enable_sent = false;
		}
		else
		{
			// B17: таймаут по отсутствию прогресса (FTE своего лимита не имеет —
			// опирается на общую download-машину; плоские 20 c от старта сдавались
			// на медленном линке, хотя загрузка шла).
			double now = Sys_DoubleTime ();
			qbool ours = cls.download
				&& !strcmp (cls.downloadname, s_csqc.csprogs_dl_localname);

			if (ours)
			{
				int pct = (int)cls.downloadpercent;

				if (pct > s_csqc.csprogs_dl_percent)
				{
					s_csqc.csprogs_dl_percent = pct;
					s_csqc.csprogs_dl_lastprogress = now;
				}
				s_csqc.csprogs_dl_started = true;
			}
			else if (s_csqc.csprogs_dl_started)
			{
				// наше скачивание завершилось без валидного файла (сбой/отмена)
				s_csqc.csprogs_dl_pending = false;
				Con_Printf ("CSQC: csprogs download failed\n");
				CSQC_Client_NotifyCSQC (false);
			}
			else if (now - s_csqc.csprogs_dl_lastprogress > 20)
			{
				// прогресса нет дольше окна (в т.ч. загрузка так и не началась)
				s_csqc.csprogs_dl_pending = false;
				Con_Printf ("CSQC: csprogs download timed out (no progress)\n");
				CSQC_Client_NotifyCSQC (false);
			}
			return;
		}
	}

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;

	if (!s_csqc.world_done)
	{
		s_csqc.world_done = true;
		if (!CSQC_Client_Exec (s_csqc.func_world))
			return;
		// FTE: enablecsqc — после CSQC_WorldLoaded каждой карты (module ready).
		CSQC_Client_NotifyCSQC (true);
	}

	// T2.7 (R9): per-frame CSQC think-loop (StartFrame/thinks/EndFrame) — FTE
	// CSQC_DrawView до CSQC_UpdateView (pr_csqc.c:8740-8811).
	CSQC_Client_RunFrameThink ();

	// player_localentnum — публикуем до модуля (окружение builtins как FTE;
	// сущности игроков не фабрикуем — см. CSQC_Client_UpdateLocalEntnum).
	CSQC_Client_UpdateLocalEntnum ();
	// C5-A: окно предикции модулю (перед CSQC_UpdateView; FTE pr_csqc.c:8837-8844).
	CSQC_Client_PatchFrames ();
	// C5-E Ф1: view_angles модулю (FTE).
	CSQC_Client_PublishViewAngles ();
	// T2.1: gamespeed модулю (FTE pr_csqc.c:8845-8851). QW/ezq не имеет
	// cl.gamespeed → 1; на серверной паузе 0 (как FTE).
	if (s_csqc.global_gamespeed >= 0)
		s_csqc.vm.globals[s_csqc.global_gamespeed] = (cl.paused & PAUSED_SERVER) ? 0 : 1;
	// B4: frametime/cltime/maxclients/player_localnum/intermission (FTE).
	CSQC_Client_PublishSimGlobals ();

	// E1a/E1b #371 deltalisten: мост player_state/entity_state → arena-edict
	// каждый кадр (FTE-модель: CL_LinkPlayers/CL_LinkPacketEntities per-frame).
	// Модуль получает авторитетное (no-lerp) состояние игроков и delta-сущностей.
	CSQC_Client_DeltaPlayers (vm);
	if (!s_csqc.errored)
		CSQC_Client_DeltaEntities (vm);

	if (s_csqc.func_update > 0)
	{
		// FTE-семантика #351: листенер действует только если модуль задал его
		// в этом кадре (иначе — движковый вид; сбрасываем перед UpdateView).
		s_listener_on = false;
		vm->globals[OFS_PARM0] = vid.width;
		vm->globals[OFS_PARM1] = vid.height;
		vm->globals[OFS_PARM2] = CSQC_Client_NotMenu () ? 1 : 0;	// B1 (FTE pr_csqc.c:8889)
		CSQC_Client_Exec (s_csqc.func_update);
	}

	// C1.2: при активном CSQC-курсоре — абсолютная позиция мыши модулю, только
	// когда она изменилась с прошлого кадра (как FTE: события на перемещение).
	{
		static float ie_abs_lastx = -1, ie_abs_lasty = -1;
		float cmx = 0, cmy = 0;

		CSQC_Client_GetCursorPos (&cmx, &cmy);
		if (CSQC_Client_CSQCCursor ())
		{
			if (cmx != ie_abs_lastx || cmy != ie_abs_lasty)
			{
				ie_abs_lastx = cmx;
				ie_abs_lasty = cmy;
				CSQC_Client_InputEvent (IE_MOUSEABS, cmx, cmy, 0);
			}
		}
		else
		{
			ie_abs_lastx = ie_abs_lasty = -1;	// курсор снят — сброс
		}
	}
}

/*
=================
CSQC_Client_ParseAllowed

Runtime-гейт CSQC-парсеров (R5): договорён FTE_PEXT_CSQC и включён cl_pext_csqc —
тот же критерий, что в cl_parse.c для case 83/90. Гейт в функции (а не только в
case) покрывает и «модуль не загружен/ошибся», см. ParseEntities.
=================
*/
qbool CSQC_Client_ParseAllowed (void)
{
#ifdef FTE_PEXT_CSQC
	extern cvar_t cl_pext_csqc;
	return cl_pext_csqc.value && (cls.fteprotocolextensions & FTE_PEXT_CSQC);
#else
	return false;
#endif
}

/*
=================
CSQC_Client_MayRead

R7/T1.4a: read*-builtins модуля допустимы только внутри parse-callback'ов
(CSQC_Ent_Update / CSQC_Parse_Event) — паритет FTE csqc_mayread
(pr_csqc.c:9690-9692, :9208, :9249). Вне их — CSQC_Client_Abort (паритет FTE
CSQC_Abort, pr_csqc.c:7489-7505).
=================
*/
qbool CSQC_Client_MayRead (void)
{
	return s_csqc.mayread;
}

/*
=================
CSQC_Client_ParsePrint

Э1: CSQC_Parse_Print(string, float) — перехват сетевого svc_print (chat и обычный).
FTE pr_csqc.c:9306-9362: наличие колбэка => движок свой print не печатает (модуль
сам решает, форвардить ли в #339 print). Возврат — был ли колбэк вызван.
=================
*/
qbool CSQC_Client_ParsePrint (const char *msg, int level)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parseprint <= 0)
		return false;

	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM0], (char *)(msg ? msg : ""));
	vm->globals[OFS_PARM1] = level;
	CSQC_Client_Exec (s_csqc.func_parseprint);
	return true;
}

/*
=================
CSQC_Client_ParseCenterPrint

Э1: CSQC_Parse_CenterPrint(string) — перехват svc_centerprint/svc_finale.
FTE pr_csqc.c:9385-9398: возврат модуля != 0 => движок centerprint игнорирует.
Возврат — подавлять ли движковый вывод.
=================
*/
qbool CSQC_Client_ParseCenterPrint (const char *msg)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parsecp <= 0)
		return false;

	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM0], (char *)(msg ? msg : ""));
	CSQC_Client_ExecRet (s_csqc.func_parsecp, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_ParseDamage

Э2: CSQC_Parse_Damage(float save, float take, vector inflictororg) — разбор svc_damage
(V_ParseDamage). FTE pr_csqc.c:9287-9304: PARM0=save(dmg_save), PARM1=take(dmg_take),
PARM2=вектор источника; return≠0 ⇒ полностью подавить цветосдвиг/view-kick (view.c:513).
Возврат — подавлять ли движковые эффекты урона.
=================
*/
qbool CSQC_Client_ParseDamage (float save, float take, const vec3_t source)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parsedamage <= 0)
		return false;

	vm->globals[OFS_PARM0] = save;
	vm->globals[OFS_PARM1] = take;
	vm->globals[OFS_PARM2 + 0] = source[0];
	vm->globals[OFS_PARM2 + 1] = source[1];
	vm->globals[OFS_PARM2 + 2] = source[2];
	CSQC_Client_ExecRet (s_csqc.func_parsedamage, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_EventSound

Э3: CSQC_Event_Sound(entnum, channel, soundname, vol, attenuation, pos, pitchmod, flags) —
разбор svc_sound (CL_ParseStartSoundPacket / NQD_ParseStartSoundPacket). FTE
pr_csqc.c:9453-9484: PARM0=entnum…PARM5=pos, PARM6=pitchmod*100, PARM7=flags; self =
csqc-энтити по номеру или world (pr_csqc.c:9464-9469). Возврат ≠0 ⇒ движок звук не играет
(FTE cl_parse.c:5336/5543). self выставляется и не восстанавливается (FTE-паритет).
=================
*/
qbool CSQC_Client_EventSound (int entnum, int channel, const char *name, float vol,
							  float atten, const vec3_t pos, float pitchmod, float flags)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;
	int slot;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_eventsound <= 0)
		return false;

	// FTE pr_csqc.c:9464-9469: self = arena-энтити по номеру или 0 (world).
	slot = CSQC_Client_NumToSlot (entnum);
	if (slot > 0 && slot < CSQC_MAX_EDICTS && CSQC_Client_EntUsed (slot))
		CSQC_Client_SetContextSlot (vm, (unsigned)slot, (unsigned)entnum);
	else if (s_csqc.global_self >= 0)
		*(int *)&vm->globals[s_csqc.global_self] = 0;

	vm->globals[OFS_PARM0] = (float)entnum;
	vm->globals[OFS_PARM1] = (float)channel;
	PR1VM_ClientSetString (vm, (string_t *)&vm->globals[OFS_PARM2], (char *)(name ? name : ""));
	vm->globals[OFS_PARM3] = vol;
	vm->globals[OFS_PARM4] = atten;
	vm->globals[OFS_PARM5 + 0] = pos[0];
	vm->globals[OFS_PARM5 + 1] = pos[1];
	vm->globals[OFS_PARM5 + 2] = pos[2];
	vm->globals[OFS_PARM6] = pitchmod * 100.0f;	// FTE pr_csqc.c:9477
	vm->globals[OFS_PARM7] = flags;

	CSQC_Client_ExecRet (s_csqc.func_eventsound, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_ParseSetAngles

Э4: CSQC_Parse_SetAngles(vector angles, float isdelta) — разбор svc_setangle
(cl_parse.c live/QW-demo, cl_nqdemo.c NQ-demo). FTE pr_csqc.c:9400-9416, pr_common.h:1091:
PARM0+0..2=вектор углов (3 слова), PARM1=isdelta; return≠0 ⇒ движок свой угол не применяет
(FTE cl_parse.c:7527/7857/9816; MVD DPB_MVD-ветка хук не вызывает). Возврат — подавлять ли
движковое применение угла.
=================
*/
qbool CSQC_Client_ParseSetAngles (const float *angles, float isdelta)
{
	pr1vm_t *vm = &s_csqc.vm;
	float ret = 0;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_parsesetangles <= 0)
		return false;

	vm->globals[OFS_PARM0 + 0] = angles[0];
	vm->globals[OFS_PARM0 + 1] = angles[1];
	vm->globals[OFS_PARM0 + 2] = angles[2];
	vm->globals[OFS_PARM1] = isdelta;
	CSQC_Client_ExecRet (s_csqc.func_parsesetangles, &ret);
	return ret != 0;
}

/*
=================
CSQC_Client_RendererRestarted

Э5: CSQC_RendererRestarted(string rendererdescription) — движковый колбэк при
переинициализации рендерера (vid_restart/vid_reload, VID_Startup) и при загрузке
модуля (CSQC_Client_Load, после CSQC_Init). FTE pr_csqc.c:8314-8363, pr_common.h:1096:
PARM0 = строка-описание рендерера; возврат движок не читает (suppress-семантики нет).

Строка persistent (в отличие от temp-колбэков Э1–Э4): модуль может сохранить её в
глобале (канарейка `g_rr_desc = rrdesc`), а кольцо PR1VM_ClientSetString перезаписывает
слоты. Поэтому держим собственную переиспользуемую копию и регистрируем её в strtbl
напрямую (PR1VM_SetString) — offset стабилен между вызовами, GL-указатель (glGetString)
после vid_restart не переиспользуется.
=================
*/
static char s_rr_desc[256];

void CSQC_Client_RendererRestarted (const char *desc)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!s_csqc.loaded || s_csqc.errored || s_csqc.func_rr <= 0)
		return;

	strlcpy (s_rr_desc, desc ? desc : "", sizeof (s_rr_desc));
	PR1VM_SetString (vm, (string_t *)&vm->globals[OFS_PARM0], s_rr_desc);
	CSQC_Client_Exec (s_csqc.func_rr);
}

/*
=================
CSQC_Client_ParseEntities

Парсинг svc_fte_csqcentities(76)/sized(92):
для каждой сущности — short entnum, бит 0x8000 = remove, 0 = конец.
Update: CSQC_Ent_Update(isnew) — модуль читает payload из текущего сообщения
(read*); контекст сущности (self/.entnum) движок ставит перед вызовом
(ADR 0017 P2/D3). Remove: CSQC_Ent_Remove с self/.entnum (без builtin-стрима).
Sized (92, только mvdsv под sv_csqcdebug): перед payload каждой update-сущности
идёт short-длина — skip-защита от рассинхрона (E3).
=================
*/
void CSQC_Client_ParseEntities (qbool sized)
{
	pr1vm_t *vm = &s_csqc.vm;
	unsigned int entnum;
	qbool removeflag;
	qbool ready;

	// R5: runtime-гейт (как cl_parse.c case 83/90) + живой модуль. Без гейта или
	// без модуля 76/92 не трактуются как CSQC. Sized-поток проходим и без модуля
	// (нужны только wire-поля entnum/len); non-sized пройти нельзя — длины payload
	// нет без исполнения модуля, поэтому это протокольная ошибка (как FTE
	// Host_EndGame, pr_csqc.c:9560), а не тихий рассинхрон.
	ready = CSQC_Client_ParseAllowed ()
		&& s_csqc.loaded && s_csqc.inited && !s_csqc.errored
		&& (s_csqc.func_entupdate > 0 || s_csqc.func_entremove > 0);
	if (!ready && !sized)
	{
		Host_Error ("CSQC_Client_ParseEntities: svc_fte_csqcentities without CSQC\n");
		return;
	}

	for (;;)
	{
		int payload_start = 0;
		int payload_len = -1;

		entnum = (unsigned short)MSG_ReadShort ();
		removeflag = !!(entnum & 0x8000);
		entnum &= ~0x8000u;
		if ((!entnum && !removeflag) || msg_badread)
			break;
		if (entnum >= (unsigned int)(sizeof (s_csqc.seen) / sizeof (s_csqc.seen[0])))
		{
			// R7/T1.3b (D4, accept+doc): dynamic growth НЕ реализован — карта
			// номер→слот фиксирована (CSQC_MAX_NUM/CSQC_MAX_EDICTS = 4096). Номер
			// ≥ 4096 недостижим с mvdsv (MAX_EDICTS=2048), путь чисто защитный;
			// остаток датаграма не дрейнится — задокументированное ограничение
			// (FTE `CSQC_EntityCheck` растит csqcent[], pr_csqc.c:9440-9451).
			break;
		}

		if (removeflag)
		{
			int slot;

			// D2/Q1: remove-0 (world) — фатально безусловно (FTE pr_csqc.c:9615-9616).
			// Host_Error (сообщение + Host_Abort); в ezq Host_EndGame — void() без abort.
			if (!entnum)
				Host_Error ("CSQC_Client_ParseEntities: cannot remove world\n");

			slot = CSQC_Client_NumToSlot ((int)entnum);
			if (slot)
			{
				if (ready && s_csqc.func_entremove > 0)
				{
					// P2/D3: контекст (self=slot, .entnum=номер), без builtin-стрима.
					CSQC_Client_SetContextSlot (vm, (unsigned)slot, entnum);
					CSQC_Client_Exec (s_csqc.func_entremove);
				}
				// D-B/R7: слот освобождает движок безусловно (колбэк опционален);
				// FTE pr_csqc.c:9625-9632, :5658-5671. Отклонение ezq от FTE: при
				// наличии колбэка FTE перекладывает фри на модуль — не меняем.
				CSQC_Client_NetFreeSlot (slot, (int)entnum);
			}
			s_csqc.seen[entnum] = false;
			continue;
		}

		// Update. Sized: [len short][payload]. R1: payload_start — ПОСЛЕ длины
		// (иначе used включает 2 байта длины и skip недосигает на 2; FTE
		// pr_csqc.c:9640-9642 берёт packetstart после ReadShort).
		if (sized)
		{
			payload_len = MSG_ReadShort ();
			payload_start = msg_readcount;
		}

		if (!ready || s_csqc.func_entupdate <= 0)
		{
			// Модуля/колбэка update нет: вычитать payload, чтобы не рассинхронить
			// поток. Non-sized длину не знает — фатально (см. выше).
			if (sized && payload_len > 0)
				MSG_ReadSkip (payload_len);
			else if (!sized)
				Host_Error ("CSQC_Client_ParseEntities: update without CSQC\n");
			continue;
		}

		vm->globals[OFS_PARM0] = s_csqc.seen[entnum] ? 0 : 1;
		s_csqc.seen[entnum] = true;

		// P2/D3 + FTE-пул: номер→слот; новый номер — слот пула либо CSQC_Ent_Spawn
		// (R7/T1.3a), контекст (self=slot, .entnum=номер).
		{
			int slot = CSQC_Client_NumToSlot ((int)entnum);
			if (!slot)
			{
				if (s_csqc.func_entspawn > 0)
				{
					// FTE pr_csqc.c:9650-9658: модуль сам создаёт/настраивает сущность;
					// невалидный self (0/мир) → без слота (Q-D, как FTE ent=NULL).
					slot = CSQC_Client_RunEntSpawn (vm, entnum);
					if (slot)
						CSQC_Client_MapNumber ((int)entnum, slot);
				}
				else
				{
					slot = CSQC_Client_NetAllocSlot (vm);
					if (!slot)
					{
						Con_Printf ("CSQC: pool full, entity %u dropped\n", entnum);
						break;	// патологично (пул 4095); рассинхрон невозможен при чтении
					}
					CSQC_Client_MapNumber ((int)entnum, slot);
					// FTE-пул Шаг 5 (диагностика): номер → слот пула; печать
					// ограничена, чтобы серверный churn remove/update не залил
					// консоль (≤32 строк на сессию csqc_dbg>=3).
					{
						static int s_dbg_lines = 0;
						cvar_t *dbg = s_csqc.csqc_dbg_cvar;	// C3: кэш (резолв после CSQC_Init)
						if (dbg && dbg->value >= 3)
						{
							if (s_dbg_lines < 32)
							{
								Con_Printf ("CSQC ent num %u -> slot %d\n", entnum, slot);
								s_dbg_lines++;
							}
						}
						else
							s_dbg_lines = 0;
					}
				}
			}
			if (slot)
				CSQC_Client_SetContextSlot (vm, (unsigned)slot, entnum);
			else if (s_csqc.global_self >= 0)
				*(int *)&vm->globals[s_csqc.global_self] = 0;	// FTE: self = NULL/world
		}

		s_csqc.mayread = true;	// R7/T1.4a: read*-контекст модуля (паритет FTE csqc_mayread)
		CSQC_Client_Exec (s_csqc.func_entupdate);
		s_csqc.mayread = false;

		if (s_csqc.errored)
		{
			// B5: модуль упал mid-message — нельзя выходить, оставив остаток списка
			// (он будет разобран как svc-опкоды). Non-sized (76) длины payload не
			// знает → ресинк невозможен, фатально (FTE Host_EndGame). Sized (92):
			// дочитываем текущий payload и далее идём в drain-режим по остатку.
			if (!sized)
				Host_Error ("CSQC_Client_ParseEntities: update module error\n");
			if (payload_len >= 0)
			{
				int used = msg_readcount - payload_start;
				if (used < payload_len)
					MSG_ReadSkip (payload_len - used);
			}
			ready = false;
			continue;
		}

		// Q-E (FTE pr_csqc.c:9693-9694): Spawn-модуль может сменить self в Update —
		// переносим номер→слот на новый валидный слот (0 = снят/мир).
		CSQC_Client_RemapAfterUpdate (vm, entnum);

		// Skip-защита: если модуль прочитал меньше payload_len — дочитать.
		if (payload_len >= 0)
		{
			int used = msg_readcount - payload_start;
			if (used < payload_len)
				MSG_ReadSkip (payload_len - used);
		}
	}
}

/*
=================
CSQC_Client_ParseEvent

Парсинг svc_fte_cgamepacket(83) (E1): имя события и payload читает сам модуль
(CSQC_Parse_Event) через read*-builtins из текущего сообщения. Guard как в
ParseEntities — без модуля чужой CSQC-multicast (echo) не роняет клиент.
=================
*/
void CSQC_Client_ParseEvent (qbool sized)
{
	qbool ready = CSQC_Client_ParseAllowed ()
		&& s_csqc.loaded && s_csqc.inited && !s_csqc.errored
		&& s_csqc.func_parseevent > 0;

	// B5 (FTE CSQC_ParseGamePacket pr_csqc.c:9212-9255): sized-поток caller
	// (cl_parse.c case 90) дрейнит по длине сам — здесь достаточно вернуться;
	// non-sized (case 83) длины не имеет, поэтому без модуля/колбэка это
	// протокольная ошибка (FTE Host_EndGame) — иначе остаток payload разберётся
	// как svc-опкоды и даст misparse.
	if (!ready)
	{
		if (!sized)
			Host_Error ("CSQC_Client_ParseEvent: cgamepacket without CSQC\n");
		return;
	}
	s_csqc.mayread = true;	// R7/T1.4a: read*-контекст модуля (паритет FTE csqc_mayread)
	CSQC_Client_Exec (s_csqc.func_parseevent);
	s_csqc.mayread = false;
	// B5: модуль мог упасть на середине payload. Sized — caller дрейнит по длине;
	// non-sized длину не знает → фатально (как FTE Host_EndGame).
	if (s_csqc.errored && !sized)
		Host_Error ("CSQC_Client_ParseEvent: cgamepacket module error\n");
}

/*
=================
CSQC_Client_InputFrame

CSQC_Input_Frame: вызывается перед отправкой каждого usercmd (CL_SendCmd,
cl_input.c). Механика FTE (pr_csqc.c:9418 CSQC_Input_Frame + cs_set/get_input_state,
:3875-4010) на QW-наборе input_*-глобалов, объявленных модулем (csdefs.qc:
input_sequence/timelength/angles/movevalues/buttons/impulse): движок заполняет их из
cmd, исполняет CSQC_Input_Frame, затем пишет изменения обратно в cmd.

Отличия от FTE:
- usercmd.angles в ezquake — float-градусы (не short), конвертацию делает
  MSG_WriteAngle16 в MSG_WriteDeltaUsercmd (com_msg.c:237) — здесь копируем напрямую;
- input_timelength множится на gamespeed (T2.1); у ezq нет cl.gamespeed → 1
  (0 на серверной паузе), т.е. в QW-поведении это no-op;
- FTE-глобалы lightlevel/weapon/servertime/clienttime/cursor/VR и InputEvent-типы
  joy/accel/focus (CSIE_*) в QW-модуле не объявлены — N/A.
=================
*/
void CSQC_Client_InputFrame (usercmd_t *cmd)
{
	pr1vm_t *vm = &s_csqc.vm;

	// T2.2: живой clientcommandframe = seq текущего собранного cmd (FTE
	// cl.movesequence, pr_csqc.c:9430-9431). Ставим до guard — трекаем и без
	// модуля; render-фаза (PatchFrames) затем отдаёт это же значение, а не
	// инкрементированную outgoing_sequence.
	s_ccframe = (unsigned int)cls.netchan.outgoing_sequence;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored || s_csqc.func_input <= 0)
		return;

	// C5-A: окно предикции до CSQC_Input_Frame (clientcommandframe = текущий cmd;
	// FTE pr_csqc.c:9430-9431).
	CSQC_Client_PatchFrames ();

	CSQC_Client_SetTime ();

	// T2.1: input_sequence = seq текущего cmd (FTE cs_set_input_state,
	// pr_csqc.c:3877-3878); та же нумерация, что clientcommandframe.
	if (s_csqc.in_sequence >= 0)
		vm->globals[s_csqc.in_sequence] = CSQC_Client_ClientCmdFrame ();

	// cmd -> input_* глобалы (только объявленные модулем).
	// input_timelength = msec/1000 * gamespeed (FTE pr_csqc.c:3880); gamespeed у ezq
	// 1 (0 на серверной паузе) — см. CSQC_Client_Update.
	if (s_csqc.in_timelength >= 0)
		vm->globals[s_csqc.in_timelength] = cmd->msec / 1000.0f
			* ((cl.paused & PAUSED_SERVER) ? 0.0f : 1.0f);
	if (s_csqc.in_angles >= 0)
	{
		vm->globals[s_csqc.in_angles + 0] = cmd->angles[0];
		vm->globals[s_csqc.in_angles + 1] = cmd->angles[1];
		vm->globals[s_csqc.in_angles + 2] = cmd->angles[2];
	}
	if (s_csqc.in_movevalues >= 0)
	{
		vm->globals[s_csqc.in_movevalues + 0] = cmd->forwardmove;
		vm->globals[s_csqc.in_movevalues + 1] = cmd->sidemove;
		vm->globals[s_csqc.in_movevalues + 2] = cmd->upmove;
	}
	if (s_csqc.in_buttons >= 0)
		vm->globals[s_csqc.in_buttons] = cmd->buttons;
	if (s_csqc.in_impulse >= 0)
		vm->globals[s_csqc.in_impulse] = cmd->impulse;

	if (!CSQC_Client_Exec (s_csqc.func_input))
		return;		// errored — кадры отключены, cmd не трогаем

	// input_* глобалы -> cmd (записываем только то, что изменил модуль).
	if (s_csqc.in_timelength >= 0)
	{
		int msec = (int)(vm->globals[s_csqc.in_timelength] * 1000.0f);
		if (msec < 1)
			msec = 1;
		else if (msec > 255)
			msec = 255;
		cmd->msec = (byte)msec;
	}
	if (s_csqc.in_angles >= 0)
	{
		cmd->angles[0] = vm->globals[s_csqc.in_angles + 0];
		cmd->angles[1] = vm->globals[s_csqc.in_angles + 1];
		cmd->angles[2] = vm->globals[s_csqc.in_angles + 2];
	}
	if (s_csqc.in_movevalues >= 0)
	{
		cmd->forwardmove = (short)vm->globals[s_csqc.in_movevalues + 0];
		cmd->sidemove = (short)vm->globals[s_csqc.in_movevalues + 1];
		cmd->upmove = (short)vm->globals[s_csqc.in_movevalues + 2];
	}
	if (s_csqc.in_buttons >= 0)
		cmd->buttons = (byte)vm->globals[s_csqc.in_buttons];
	if (s_csqc.in_impulse >= 0)
		cmd->impulse = (byte)vm->globals[s_csqc.in_impulse];
}

/*
=================
CSQC_Client_RecordInput / CSQC_Client_ApplyInput

C5-A #345: история отправленных usercmd. CL_SendCmd записывает каждый
отправленный cmd (CSQC_Client_RecordInput); builtin #345(seq) запрашивает его и
заполняет input_* глобалы (CSQC_Client_ApplyInput).

seq = зеркало cls.netchan.outgoing_sequence (номер клиентского сообщения на
момент записи; Netchan_Transmit инкрементирует после записи заголовка). Это и
есть тот номер, который подтверждает сервер (servercommandframe = incoming_
acknowledged = cl.parsecount, cl_parse.c:2050-2053) — окно (servercommandframe,
clientcommandframe] согласовано в одной нумерации. Отличие от FTE: у нас
ring-история (64) + запись только живого пути CL_SendCmd (демо/MVD не
записываются). T2.2: clientcommandframe = s_ccframe = последний записанный seq,
поэтому #345(clientcommandframe) попадает в ring (живой pending-кадр доступен вне
CSQC_Input_Frame, как FTE movesequence). NQ-механизм ackedmovesequence
(PEXT2_PREDINFO) недостижим (не для QW).
=================
*/
void CSQC_Client_RecordInput (usercmd_t *cmd)
{
	unsigned int seq;

	seq = (unsigned int)cls.netchan.outgoing_sequence;
	s_last_seq = seq;
	s_inhist[seq % CSQC_INHIST].seq = seq;
	s_inhist[seq % CSQC_INHIST].cmd = *cmd;
	if (s_csqc.loaded && !s_csqc.errored)
	{
		if (s_csqc.in_sequence >= 0)
			s_csqc.vm.globals[s_csqc.in_sequence] = seq;
		if (s_csqc.g_ccframe >= 0)
			s_csqc.vm.globals[s_csqc.g_ccframe] = seq;
	}
}

static void CSQC_Client_FillInputFromCmd (usercmd_t *cmd)
{
	pr1vm_t *vm = &s_csqc.vm;

	// T2.2: ×gamespeed как в Input_Frame/FTE cs_set_input_state (pr_csqc.c:3880);
	// у ezq gamespeed 1 (0 на серверной паузе) — в QW no-op.
	if (s_csqc.in_timelength >= 0)
		vm->globals[s_csqc.in_timelength] = cmd->msec / 1000.0f
			* ((cl.paused & PAUSED_SERVER) ? 0.0f : 1.0f);
	if (s_csqc.in_angles >= 0)
	{
		vm->globals[s_csqc.in_angles + 0] = cmd->angles[0];
		vm->globals[s_csqc.in_angles + 1] = cmd->angles[1];
		vm->globals[s_csqc.in_angles + 2] = cmd->angles[2];
	}
	if (s_csqc.in_movevalues >= 0)
	{
		vm->globals[s_csqc.in_movevalues + 0] = cmd->forwardmove;
		vm->globals[s_csqc.in_movevalues + 1] = cmd->sidemove;
		vm->globals[s_csqc.in_movevalues + 2] = cmd->upmove;
	}
	if (s_csqc.in_buttons >= 0)
		vm->globals[s_csqc.in_buttons] = cmd->buttons;
	if (s_csqc.in_impulse >= 0)
		vm->globals[s_csqc.in_impulse] = cmd->impulse;
}

int CSQC_Client_ApplyInput (unsigned int seq)
{
	unsigned int i;
	csqc_inrec_t *r;

	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (!seq)
		return 0;
	// C5-A: paused-guard как FTE (pr_csqc.c:4142) — на серверной паузе кадры
	// окна не применяются. Диапазон (servercommandframe, clientcommandframe]
	// движок не проверяет (контракт модуля; спека ext_csqc_1.txt:262) — здесь
	// только живучесть кольца.
	if ((cl.paused & PAUSED_SERVER) && seq >= (unsigned)CSQC_Client_ServerCmdFrame ())
		return 0;
	for (i = 0; i < CSQC_INHIST; i++)
	{
		r = &s_inhist[i];
		if (r->seq == seq)
		{
			CSQC_Client_FillInputFromCmd (&r->cmd);
			if (s_csqc.in_sequence >= 0)
				s_csqc.vm.globals[s_csqc.in_sequence] = seq;
			return 1;
		}
	}
	return 0;
}

/*
=================
CSQC_VectorAngles

Порт FTE VectorAngles (fteqw/engine/common/mathlib.c:294) с optional up→roll,
meshpitch=false (r_meshpitch/r_meshroll не применяются — то же отклонение, что и
у #51 vectoangles). forward — направление; up может быть NULL; result[3] =
(pitch, yaw, roll). Общий хелпер для #51 (csqc_builtins.c) и #638 CL_RotateMoves.
=================
*/
void CSQC_VectorAngles (const float *forward, const float *up, float *result)
{
	float yaw, pitch, roll;

	if (forward[1] == 0 && forward[0] == 0)
	{
		if (forward[2] > 0)
		{
			pitch = -M_PI * 0.5f;
			yaw = up ? (float)atan2 (-up[1], -up[0]) : 0;
		}
		else
		{
			pitch = M_PI * 0.5f;
			yaw = up ? (float)atan2 (up[1], up[0]) : 0;
		}
		roll = 0;
	}
	else
	{
		float cp, sp, cy, sy;
		yaw = (float)atan2 (forward[1], forward[0]);
		pitch = -(float)atan2 (forward[2], sqrt (forward[0] * forward[0] + forward[1] * forward[1]));
		if (up)
		{
			float tleft[3], tup[3];
			cp = (float)cos (pitch); sp = (float)sin (pitch);
			cy = (float)cos (yaw); sy = (float)sin (yaw);
			tleft[0] = -sy; tleft[1] = cy; tleft[2] = 0;
			tup[0] = sp * cy; tup[1] = sp * sy; tup[2] = cp;
			roll = -(float)atan2 (up[0] * tleft[0] + up[1] * tleft[1] + up[2] * tleft[2],
				up[0] * tup[0] + up[1] * tup[1] + up[2] * tup[2]);
		}
		else
			roll = 0;
	}
	pitch *= (float)(180 / M_PI);
	yaw *= (float)(180 / M_PI);
	roll *= (float)(180 / M_PI);
	if (pitch < 0) pitch += 360;
	if (yaw < 0) yaw += 360;
	if (roll < 0) roll += 360;
	result[0] = pitch; result[1] = yaw; result[2] = roll;
}

/*
CSQC_VectorTransform — порт FTE VectorTransform (fteqw/engine/common/mathlib.c:760)
для matrix3x4 без трансляции (в #638 4-й столбец матрицы = 0).
*/
static void CSQC_VectorTransform (const float *in, float mat[3][4], float *out)
{
	out[0] = DotProduct (in, mat[0]) + mat[0][3];
	out[1] = DotProduct (in, mat[1]) + mat[1][3];
	out[2] = DotProduct (in, mat[2]) + mat[2][3];
}

/*
=================
CSQC_Client_RotateMoves

#638 CL_RotateMoves (класс I; FTE PF_cl_RotateMoves pr_csqc.c:4094-4125):
поворот углов отправленных, но ещё не подтверждённых usercmd (seq >
servercommandframe) на дельту anglechange, порядок как FTE:
AngleVectorsFLU(anglechange) → forward/up кадра → VectorTransform → VectorAngles.
usercmd.angles в ezq — float-градусы (qwprot/src/protocol.h:539), поэтому без
SHORT2ANGLE/ANGLE2SHORT (в FTE cmd.angles — short). Возврат 0 при невалидном
seat (single-seat: валиден только 0; FTE pr_csqc.c:4102-4106).
=================
*/
int CSQC_Client_RotateMoves (float *anglechange, int seat)
{
	int i;
	float mat[3][4];
	vec3_t of, ou, nf, nu, a;
	unsigned int ack;

	if (!s_csqc.loaded || s_csqc.errored)
		return 0;
	if (seat != 0)
		return 0;

	AngleVectorsFLU (anglechange, mat[0], mat[1], mat[2]);
	mat[0][3] = mat[1][3] = mat[2][3] = 0;

	ack = (unsigned int)CSQC_Client_ServerCmdFrame ();
	for (i = 0; i < CSQC_INHIST; i++)
	{
		csqc_inrec_t *r = &s_inhist[i];
		if (!r->seq || r->seq <= ack)
			continue;			// пустые и подтверждённые слоты не трогаем

		VectorCopy (r->cmd.angles, a);
		AngleVectors (a, of, NULL, ou);
		CSQC_VectorTransform (of, mat, nf);
		CSQC_VectorTransform (ou, mat, nu);
		CSQC_VectorAngles (nf, nu, a);
		VectorCopy (a, r->cmd.angles);
	}
	return 1;
}

/*
=================
CSQC_Client_MakeVectors

#1 makevectors (C6.1; FTE-паритет PF_cs_makevectors, pr_csqc.c:669): по вектору
углов пишет v_forward/v_right/v_up модуля (глобалы, резолв в Load). Если модуль
их не объявил — no-op (offset -1).
=================
*/
void CSQC_Client_MakeVectors (float *ang)
{
	pr1vm_t *vm = &s_csqc.vm;
	float *f, *r, *u;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.g_vfwd < 0 || s_csqc.g_vright < 0 || s_csqc.g_vup < 0)
		return;		// модуль не объявил v_forward/v_right/v_up
	f = &vm->globals[s_csqc.g_vfwd];
	r = &vm->globals[s_csqc.g_vright];
	u = &vm->globals[s_csqc.g_vup];
	AngleVectors (ang, f, r, u);
}

/*
=================
CSQC_Client_VectorVectors

#432 vectorvectors (T3 Э3; FTE-паритет PF_vectorvectors, pr_bgcmd.c:6559): нормализует
заданное направление в v_forward модуля и строит ортогональные v_right/v_up через FTE
VVPerpendicularVector + CrossProduct (mathlib.c:270/288). Не используем ezq
PerpendicularVector — у неё другой edge-case для (0,0,z). Модуль без глобалов — no-op.
=================
*/
void CSQC_Client_VectorVectors (float *dir)
{
	pr1vm_t *vm = &s_csqc.vm;
	float *f, *r, *u;
	float right[3];

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (s_csqc.g_vfwd < 0 || s_csqc.g_vright < 0 || s_csqc.g_vup < 0)
		return;		// модуль не объявил v_forward/v_right/v_up
	f = &vm->globals[s_csqc.g_vfwd];
	r = &vm->globals[s_csqc.g_vright];
	u = &vm->globals[s_csqc.g_vup];

	VectorCopy (dir, f);
	VectorNormalize (f);

	if (!f[0] && !f[1])
	{
		right[0] = 0;
		right[1] = f[2] ? -1 : 0;
		right[2] = 0;
	}
	else
	{
		right[0] = f[1];
		right[1] = -f[0];
		right[2] = 0;
		VectorNormalize (right);
	}
	VectorCopy (right, r);
	CrossProduct (right, f, u);
}

/*
=================
CSQC_Client_HasInputEvent / CSQC_Client_InputEvent

C1.2: доставка событий ввода модулю (CSQC_InputEvent, csdefs.qc:159). Вызывается
из keys.c (клавиши/клики/колесо при key_dest == key_game) и in_sdl2.c (мышь:
MOUSEDELTA в обычном режиме; MOUSEABS — из CSQC_Client_Update при CSQCCursor).
Возврат модуля != 0 означает «событие обработано» (движок не применяет его).
=================
*/
qbool CSQC_Client_HasInputEvent (void)
{
	return s_csqc.loaded && s_csqc.inited && !s_csqc.errored
		&& s_csqc.func_inputevent > 0;
}

int CSQC_Client_InputEvent (int evtype, float a, float b, float c)
{
	pr1vm_t *vm = &s_csqc.vm;

	if (!CSQC_Client_HasInputEvent ())
		return 0;
	// B6: модуль работает в QC/DP-домене клавиш (csdefs.qc:1377-1449); переводим
	// внутренний keynum ezq -> QC для key-событий (мышь/дельты — без трансляции).
	if (evtype == IE_KEYDOWN || evtype == IE_KEYUP)
		a = CSQC_Client_KeynumToQC ((int)a);
	// Параметры модульной функции (4 float) — как CSQC_UpdateView.
	vm->globals[OFS_PARM0] = evtype;
	vm->globals[OFS_PARM1] = a;
	vm->globals[OFS_PARM2] = b;
	vm->globals[OFS_PARM3] = c;
	if (!CSQC_Client_Exec (s_csqc.func_inputevent))
		return 0;
	return (int)vm->globals[OFS_RETURN];
}

/*
=================
CSQC_Client_RunPlayerPhysics

C5-B #347 runstandardplayerphysics(ent): FTE-семантика (PF_cs_runplayerphysics,
pr_csqc.c:4185-4299) на клиентском PM-пути ezquake (PM_PlayerMove, как cl_pred.c):

- B1: команда движения — из input_*-глобалов (модуль зовёт getinputstate(seq)
  перед #347); fallback — последний записанный usercmd (если input_* не объявлены);
- B2: solid-набор пересобирается внутри вызова (CL_SetSolidEntities + Players);
  поля ent .mins/.maxs/.gravity/.pmove_flags/.flags; запись .origin/.velocity/
  .angles, .flags (FL_ONGROUND), .pmove_flags (PMF_JUMP_HELD);
- B3: чанки ≤50 мс (как cl_pred.c:76-88) + deprec pmove_org/vel/onground.

Отклонения от FTE — accept+doc (T2.3, ezq pmove без соответствующих полей):
- skipent: FTE-PM его не читает (только trace-хелперы, pmovetst.c:131), а
  CL_SetSolidPlayers локального игрока исключает (cl_ents.c:2532) → мотв;
- .gravitydir: FTE-дефолт -z (pr_csqc.c:4270, pmove.c:890-894); directional
  требует port PM-core (вне #347) — для QW-мода не используется;
- onladder → PMF_LADDER: FTE-детект только Q2/Q3 (pmove.c:1010-1052), в QW
  мёртв → недостижим и на FTE;
- .waterlevel: PM считает и использует внутри (PM_CategorizePosition/PM_Friction/
  PM_WaterMove), но в поле не отдаётся — ни ezq-, ни FTE-клиентский #347 его не
  пишут (SSQC runclientphys — pr_cmds.c:10286); .groundent в csdefs нет.
Box ent мапится на глобальные player_mins/maxs (box других игроков — тот же).
=================
*/
#define CSQC_MV_WALK	3	// csdefs.qc MOVETYPE_* (FTE-нумерация)
#define CSQC_MV_FLY		5
#define CSQC_MV_NOCLIP	8
#define CSQC_PMF_JUMP_HELD	1	// fteqw/engine/common/pmove.h:36
#define CSQC_FL_ONGROUND	512	// csdefs.qc:270

void CSQC_Client_RunPlayerPhysics (int entnum)
{
	extern vec3_t player_mins, player_maxs;
	pr1vm_t *vm = &s_csqc.vm;
	float *base, *o, *v;
	vec3_t saved_mins, saved_maxs;
	int msecs, mt, i;

	if (!s_csqc.loaded || !s_csqc.inited || s_csqc.errored)
		return;
	if (entnum <= 0 || entnum >= CSQC_MAX_EDICTS || cls.state != ca_active)
		return;		// slot 0 = world (FTE: readonly-guard, pr_csqc.c:4197)
	if (cls.demoplayback || cls.mvdplayback)
		return;		// только живая игра (physents из cl)

	base = (float *)((byte *)vm->game_edicts + (size_t)entnum * vm->edict_size);

	memset (&pmove, 0, sizeof (pmove));

	// --- B1: вход из input_* (fallback — последний записанный usercmd) ---
	if (s_csqc.in_timelength >= 0 || s_csqc.in_angles >= 0 || s_csqc.in_movevalues >= 0)
	{
		int m = (s_csqc.in_timelength >= 0)
			? (int)(vm->globals[s_csqc.in_timelength] * 1000.0f) : 1;
		if (m < 1)
			m = 1;
		else if (m > 255)
			m = 255;
		pmove.cmd.msec = (byte)m;
		if (s_csqc.in_angles >= 0)
		{
			VectorCopy (&vm->globals[s_csqc.in_angles], pmove.cmd.angles);
			VectorCopy (pmove.cmd.angles, pmove.angles);
		}
		if (s_csqc.in_movevalues >= 0)
		{
			pmove.cmd.forwardmove = (short)vm->globals[s_csqc.in_movevalues + 0];
			pmove.cmd.sidemove = (short)vm->globals[s_csqc.in_movevalues + 1];
			pmove.cmd.upmove = (short)vm->globals[s_csqc.in_movevalues + 2];
		}
		if (s_csqc.in_buttons >= 0)
			pmove.cmd.buttons = (byte)vm->globals[s_csqc.in_buttons];
		if (s_csqc.in_impulse >= 0)
			pmove.cmd.impulse = (byte)vm->globals[s_csqc.in_impulse];
	}
	else
	{
		if (!s_last_seq)
			return;
		pmove.cmd = s_inhist[s_last_seq % CSQC_INHIST].cmd;
		VectorCopy (pmove.cmd.angles, pmove.angles);
	}

	// --- B2: состояние ent ---
	if (s_csqc.f_origin >= 0)
		VectorCopy (base + s_csqc.f_origin, pmove.origin);
	else
		VectorClear (pmove.origin);
	if (s_csqc.f_velocity >= 0)
		VectorCopy (base + s_csqc.f_velocity, pmove.velocity);
	else
		VectorClear (pmove.velocity);
	if (s_csqc.f_flags >= 0)
		pmove.onground = (((int)base[s_csqc.f_flags]) & CSQC_FL_ONGROUND) != 0;

	// box ent -> глобальные player_mins/maxs (в ezq PM/SetSolidPlayers используют
	// глобальный бокс); сохраняем и восстанавливаем после вызова.
	VectorCopy (player_mins, saved_mins);
	VectorCopy (player_maxs, saved_maxs);
	if (s_csqc.f_mins >= 0 && s_csqc.f_maxs >= 0)
	{
		float *mn = base + s_csqc.f_mins, *mx = base + s_csqc.f_maxs;
		if (mn[0] || mn[1] || mn[2] || mx[0] || mx[1] || mx[2])
		{
			VectorCopy (mn, player_mins);
			VectorCopy (mx, player_maxs);
		}
	}

	mt = (s_csqc.f_movetype >= 0) ? (int)base[s_csqc.f_movetype] : CSQC_MV_WALK;
	switch (mt)
	{
	case CSQC_MV_FLY:
		pmove.pm_type = PM_FLY;
		break;
	case CSQC_MV_NOCLIP:
		pmove.pm_type = PM_SPECTATOR;
		break;
	default:
		pmove.pm_type = PM_NORMAL;
		break;
	}
	pmove.jump_held = (s_csqc.f_pmove_flags >= 0)
		? (((int)base[s_csqc.f_pmove_flags] & CSQC_PMF_JUMP_HELD) != 0) : false;
	pmove.jump_msec = 0;
	pmove.waterjumptime = 0;

	movevars.entgravity = cl.entgravity;
	movevars.maxspeed = cl.maxspeed;
	movevars.bunnyspeedcap = cl.bunnyspeedcap;
	if (s_csqc.f_gravity >= 0 && base[s_csqc.f_gravity] != 0)
		movevars.entgravity = base[s_csqc.f_gravity];

	// solid-набор: мир+BSP-энт (пересборка после memset) + игроки (cl_pred).
	CL_SetSolidEntities ();
	CL_SetSolidPlayers (cl.playernum);

	// --- B3: чанки ≤50 мс ---
	msecs = pmove.cmd.msec;
	if (msecs <= 0)
		msecs = 1;
	while (msecs > 0)
	{
		int step = (msecs > 50) ? 50 : msecs;
		pmove.cmd.msec = step;
		PM_PlayerMove ();
		msecs -= step;
	}

	// --- результат обратно в ent ---
	o = (s_csqc.f_origin >= 0) ? base + s_csqc.f_origin : NULL;
	v = (s_csqc.f_velocity >= 0) ? base + s_csqc.f_velocity : NULL;
	if (o)
		VectorCopy (pmove.origin, o);
	if (v)
		VectorCopy (pmove.velocity, v);
	if (s_csqc.f_angles >= 0)
		VectorCopy (pmove.angles, base + s_csqc.f_angles);
	if (s_csqc.f_flags >= 0)
	{
		float *fl = base + s_csqc.f_flags;
		*fl = (float)(pmove.onground
			? (((int)*fl) | CSQC_FL_ONGROUND)
			: (((int)*fl) & ~CSQC_FL_ONGROUND));
	}
	if (s_csqc.f_pmove_flags >= 0)
		base[s_csqc.f_pmove_flags] = (float)(pmove.jump_held ? CSQC_PMF_JUMP_HELD : 0);

	// deprec-глобалы (читает fo-модуль).
	if (s_csqc.p_org >= 0)
		for (i = 0; i < 3; i++)
			vm->globals[s_csqc.p_org + i] = pmove.origin[i];
	if (s_csqc.p_vel >= 0)
		for (i = 0; i < 3; i++)
			vm->globals[s_csqc.p_vel + i] = pmove.velocity[i];
	if (s_csqc.p_onground >= 0)
		vm->globals[s_csqc.p_onground] = pmove.onground ? 1 : 0;

	VectorCopy (saved_mins, player_mins);
	VectorCopy (saved_maxs, player_maxs);
}

/*
=================
C2.2 — string-buffers (#460-469). Хранилище — s_bufs (handle = idx+1).
=================
*/
static csqc_buf_t *CSQC_Client_BufAt (int handle)
{
	if (handle < 1 || handle > CSQC_MAX_BUFS || !s_bufs[handle - 1].inuse)
		return NULL;
	return &s_bufs[handle - 1];
}

static void CSQC_Client_BufClear (csqc_buf_t *b)
{
	int i;
	for (i = 0; i < b->num; i++)
	{
		if (b->str[i])
			Q_free (b->str[i]);
	}
	Q_free (b->str);
	b->str = NULL;
	b->num = b->cap = 0;
}

int CSQC_Client_BufCreate (void)
{
	int i;
	for (i = 0; i < CSQC_MAX_BUFS; i++)
	{
		if (!s_bufs[i].inuse)
		{
			memset (&s_bufs[i], 0, sizeof (s_bufs[i]));
			s_bufs[i].inuse = true;
			return i + 1;
		}
	}
	return 0;
}

void CSQC_Client_BufDel (int handle)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	if (b)
	{
		CSQC_Client_BufClear (b);
		b->inuse = false;
	}
}

int CSQC_Client_BufGetSize (int handle)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	return b ? b->num : 0;
}

static int CSQC_Client_BufPush (csqc_buf_t *b, const char *s)
{
	char **ns;
	int idx;
	if (b->num >= b->cap)
	{
		int ncap = b->cap ? b->cap * 2 : 8;
		ns = (char **)Q_realloc (b->str, sizeof (char *) * ncap);
		if (!ns)
			return -1;
		b->str = ns;
		b->cap = ncap;
	}
	idx = b->num;
	b->str[idx] = Q_strdup (s ? s : "");
	b->num++;
	return idx;
}

int CSQC_Client_BufAdd (int handle, const char *s, int order)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	int idx, i;
	if (!b)
		return -1;
	idx = CSQC_Client_BufPush (b, s);
	if (idx < 0)
		return -1;
	// order > 0 — вставка на позицию (не дальше конца списка).
	if (order > 0 && order < idx)
	{
		char *tmp = b->str[idx];
		for (i = idx; i > order; i--)
			b->str[i] = b->str[i - 1];
		b->str[order] = tmp;
	}
	return idx;
}

int CSQC_Client_BufGet (int handle, int idx, char *out, size_t max)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	if (!b || idx < 0 || idx >= b->num || !out || max < 1)
		return 0;
	strlcpy (out, b->str[idx], max);
	return 1;
}

int CSQC_Client_BufSet (int handle, int idx, const char *s)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	char *c;
	if (!b || idx < 0 || idx >= b->num)
		return 0;
	c = Q_strdup (s ? s : "");
	if (!c)
		return 0;
	Q_free (b->str[idx]);
	b->str[idx] = c;
	return 1;
}

int CSQC_Client_BufFree (int handle, int idx)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	int i;
	if (!b || idx < 0 || idx >= b->num)
		return 0;
	Q_free (b->str[idx]);
	for (i = idx; i < b->num - 1; i++)
		b->str[i] = b->str[i + 1];
	b->num--;
	return 1;
}

int CSQC_Client_BufCopy (int from, int to)
{
	csqc_buf_t *f = CSQC_Client_BufAt (from);
	csqc_buf_t *t = CSQC_Client_BufAt (to);
	int i;
	if (!f || !t)
		return 0;
	CSQC_Client_BufClear (t);
	for (i = 0; i < f->num; i++)
		CSQC_Client_BufPush (t, f->str[i]);
	return 1;
}

int CSQC_Client_BufSort (int handle, int prefixlen, int backward)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	int i, j;
	if (!b)
		return 0;
	(void)prefixlen;	// сортировка по всей строке (prefix-семантику не эмулируем)
	for (i = 0; i < b->num; i++)
	{
		for (j = i + 1; j < b->num; j++)
		{
			int cmp = strcmp (b->str[i], b->str[j]);
			if ((!backward && cmp > 0) || (backward && cmp < 0))
			{
				char *t = b->str[i];
				b->str[i] = b->str[j];
				b->str[j] = t;
			}
		}
	}
	return 1;
}

int CSQC_Client_BufImplode (int handle, const char *glue, char *out, size_t max)
{
	csqc_buf_t *b = CSQC_Client_BufAt (handle);
	size_t o = 0;
	int i;
	if (!b || !out || max < 1)
		return 0;
	out[0] = 0;
	for (i = 0; i < b->num && o + 1 < max; i++)
	{
		if (i && glue)
		{
			size_t gl = strlen (glue);
			if (o + gl < max - 1)
			{
				memcpy (out + o, glue, gl);
				o += gl;
				out[o] = 0;
			}
		}
		{
			size_t l = strlen (b->str[i]);
			if (o + l >= max)
				l = max - 1 - o;
			memcpy (out + o, b->str[i], l);
			o += l;
			out[o] = 0;
		}
	}
	return 1;
}

void CSQC_Client_BufReset (void)
{
	int i;
	for (i = 0; i < CSQC_MAX_BUFS; i++)
	{
		if (s_bufs[i].inuse)
		{
			CSQC_Client_BufClear (&s_bufs[i]);
			s_bufs[i].inuse = false;
		}
	}
}


/*
=================
CSQC_Client_Disconnect
=================
*/
void CSQC_Client_Disconnect (void)
{
	int i;

	if (s_csqc.loaded)
	{
		if (s_csqc.inited && !s_csqc.errored)
			CSQC_Client_Exec (s_csqc.func_shutdown);
		PR1VM_UnLoad (&s_csqc.vm);
	}
	CSQC_Client_ClearCommands ();
	// P1/D2: арена edicts до memset (указатели ещё на месте).
	CSQC_Client_FreeArena ();
	// Сброс курсора модуля (#343 A3.1): при новом коннекте состояние чистое.
	s_cursormode.usecursor = false;
	s_cursormode.cursorimage[0] = 0;
	s_cursormode.scale = 0;
	// C1.1 #346: чувствительность в дефолт.
	s_sens_scale = 1;
	// C2.2: string-buffers очистить (deep-copy строки).
	CSQC_Client_BufReset ();
	// E1a #371: снять регистрации deltalisten/карту player-моста.
	CSQC_Client_DeltaReset ();
	CSQC_Client_ViewReset ();
	s_scene_rendered = false;	// Ф3: takeover-сцена сброшена
	CSQC_Client_ModelReset ();	// Ф3: CSQC-реестр моделей
	memset (&s_csqc, 0, sizeof (s_csqc));
	memset (s_csqc_stat, 0, sizeof (s_csqc_stat));
	memset (s_csqc_statsf, 0, sizeof (s_csqc_statsf));
	// Stat wire 78/79: строковые статы — глубокие копии (Q_strdup).
	for (i = 0; i < MAX_EXTENDED_CL_STATS; i++)
	{
		Q_free (s_csqc_statss[i]);
		s_csqc_statss[i] = NULL;
	}
	s_csqc.func_init = s_csqc.func_world = s_csqc.func_update =
		s_csqc.func_console = s_csqc.func_shutdown = -1;
	s_csqc.func_entupdate = s_csqc.func_entremove = s_csqc.func_parseevent = -1;
	s_csqc.func_parseprint = s_csqc.func_parsecp = -1;
	s_csqc.func_parsedamage = -1;
	s_csqc.func_eventsound = -1;
	s_csqc.func_parsesetangles = -1;
	s_csqc.func_rr = -1;
	s_csqc.func_entspawn = -1;
	s_csqc.mayread = false;
	s_csqc.func_input = -1;
	s_csqc.func_inputevent = -1;
	s_csqc.func_startframe = s_csqc.func_endframe = -1;
	s_csqc.global_time = -1;
	s_csqc.global_gamespeed = -1;
	s_csqc.global_self = -1;
	s_csqc.global_other = -1;
	s_csqc.global_physics_mode = -1;
	s_csqc.field_entnum = -1;
	s_csqc.f_origin = s_csqc.f_velocity = s_csqc.f_angles = s_csqc.f_mins = s_csqc.f_maxs = -1;
	s_csqc.f_movetype = s_csqc.f_flags = s_csqc.f_gravity = s_csqc.f_pmove_flags = -1;
	s_csqc.f_modelindex = s_csqc.f_skin = -1;
	s_csqc.f_frame = s_csqc.f_effects = s_csqc.f_colormap = s_csqc.f_drawmask = -1;
	s_csqc.f_think = s_csqc.f_nextthink = -1;
	s_csqc.g_localentnum = -1;
	s_csqc.in_timelength = s_csqc.in_angles = s_csqc.in_movevalues = -1;
	s_csqc.in_buttons = s_csqc.in_impulse = -1;
	s_csqc.in_sequence = -1;
	s_csqc.g_ccframe = s_csqc.g_scframe = -1;
	s_csqc.p_org = s_csqc.p_vel = s_csqc.p_onground = -1;
	s_csqc.g_vfwd = s_csqc.g_vright = s_csqc.g_vup = -1;
	s_csqc.g_view_angles = -1;
	s_csqc.g_frametime = s_csqc.g_cltime = s_csqc.g_maxclients = -1;
	s_csqc.g_player_localnum = s_csqc.g_intermission = -1;
	CSQC_Client_OffsetCacheReset ();	// C2 (Wave C): офсеты горячего пути
	s_last_seq = 0;
	s_ccframe = 0;
}

#endif // !CLIENTONLY
