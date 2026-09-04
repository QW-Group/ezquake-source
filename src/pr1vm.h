/*
pr1vm.h -- PR1 engine instance (ezquake PR1 refactor, S1)

PR1-интерпретатор переводится с глобального состояния на per-instance:
состояние исполнения (стеки вызовов/локальных переменных, xfunction/
xstatement) живёт в pr1vm_t. «Модульные» глобалы (progs/pr_functions/...)
в ezquake общие для PR1 и PR2 и читаются внешним кодом (sv_*.c) — они остаются
как есть; в pr1vm_t они зеркалятся через PR1VM_BindServer() и полноценно
переводятся на инстанс в слайсах S2-S4.

Пока PR1 исполняется под CLIENTONLY-гейтом (серверный билд). API для
клиентского инстанса добавится вместе с loader/обвязкой (S3/S5).
*/

#ifndef PR1VM_H
#define PR1VM_H

#include "progs.h"

#define PR1VM_MAX_STACK	32
#define PR1VM_LOCALSTACK	2048

typedef struct pr1vm_s pr1vm_t;

typedef struct
{
	int			s;			// statement (return address)
	dfunction_t	*f;
} pr1vm_stack_t;

struct pr1vm_s
{
	// Зеркала «модульных» глобалов (сервер = общие символы через BindServer;
	// клиентский инстанс получит собственные буферы в S3).
	dprograms_t		*progs;
	dfunction_t		*functions;
	ddef_t			*fielddefs;
	ddef_t			*globaldefs;
	dstatement_t	*statements;
	char			*strings;
	globalvars_t	*global_struct;
	float			*globals;
	int				edict_size;		// bytes per entvars block

	// Edict-модель (сервер: sv.edicts / sv.game_edicts). S5b: state зеркалит sv.state.
	edict_t			*edicts;
	int				num_edicts;
	int				max_edicts;
	int				state;
	void			*game_edicts;	// entvars-база для STOREP_*/EDICT_TO_PROG

	// Состояние исполнения (перенесено из глобалов pr_exec.c).
	pr1vm_stack_t	stack[PR1VM_MAX_STACK];
	int				depth;
	int				localstack[PR1VM_LOCALSTACK];
	int				localstack_used;
	dfunction_t		*xfunction;
	int				xstatement;

	// Builtin-контейнер (S5): диспетчер по номеру -first_statement.
	int				numbuiltins;
	builtin_t		*builtins;
	// Текущий вызов (S5): число аргументов + флаг трассы.
	int				argc;
	qbool			trace;

	// Клиентские (per-instance) динамические строки. Для серверного инстанса
	// не используются: он делегирует глобальным таблицам (PR1_GetString/...),
	// т.к. их читают PR2 и sv_*.
	char			*strtbl[MAX_PRSTR];
	char			*newstrtbl[MAX_PRSTR];
	int				numstr;

	// Host-интерфейс (S4): колбэки получают готовую строку.
	void (*host_error)(pr1vm_t *vm, const char *msg);
	void (*host_print)(pr1vm_t *vm, const char *msg);
	void *host_udata;
};

// Активный инстанс (тот, внутри которого сейчас исполняется PR1; NULL вне вызова).
pr1vm_t *PR1VM_Active(void);

// Серверный инстанс (sv_pr1vm) — «default» для PR_* обёрток.
pr1vm_t *PR1VM_Server(void);

// Обнулить инстанс и (для сервера) заполнить зеркала из общих глобалов и sv.*.
void PR1VM_Reset(pr1vm_t *vm);
// Fill the server mirrors from shared globals and sv.*. Exec state is not touched:
// BindServer may run on a nested (recursive) PR_ExecuteProgram.
void PR1VM_BindServer(pr1vm_t *vm);
// S6: освободить инстанс от модуля — очистить зеркала lumps и exec-состояние,
// сохранив host-колбэки (пере)задаются BindServer/клиентом.
void PR1VM_UnLoad(pr1vm_t *vm);

// Загрузка: байтсвоп заголовка+lumps и заполнение зеркал инстанса (без
// валидации версии/CRC — их делает серверная обёртка).
void PR1VM_LoadData(pr1vm_t *vm, dprograms_t *hdr);
// Сервер: зеркала инстанса -> общие «модульные» глобалы (PR2/sv_*.c читают их).
void PR1VM_CommitServer(pr1vm_t *vm);

// Клиентский v7-secondary16 loader («пустой» extended). Возвращает false и
// печатает причину через Con_Printf (без SV_Error).
qbool PR1VM_LoadClientV7(pr1vm_t *vm, const byte *data, int filesize);

// Резолв по имени на инстансе (в отличие от ED_Find* — по зеркалам vm).
dfunction_t *PR1VM_FindFunction(pr1vm_t *vm, const char *name);
int PR1VM_FindGlobal(pr1vm_t *vm, const char *name);
char *PR1VM_GetString(pr1vm_t *vm, int num);
void PR1VM_SetString(pr1vm_t *vm, string_t *address, char *s);

// S3 debug: консольная команда csqc_smoke (регистрируется в PR2_Init).
void PR1VM_CSQCSmoke_f(void);
// S4 debug: провокация PR_RunError на серверном инстансе (pr1vm_test_error).
void PR1VM_TestError_f(void);

int  PR1VM_EnterFunction(pr1vm_t *vm, dfunction_t *f);
int  PR1VM_LeaveFunction(pr1vm_t *vm);
void PR1VM_ExecuteProgram(pr1vm_t *vm, func_t fnum);

#endif /* PR1VM_H */
