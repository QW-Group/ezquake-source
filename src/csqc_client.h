/*
csqc_client.h -- клиентская обвязка PR1VM (наш csprogs.dat), Фаза 5 (мини-каркас).

Точки вызова из клиентского кода (cl_parse.c / cl_screen.c / cl_main.c)
и accessor'ы, которыми пользуются builtins из csqc_builtins.c. Заголовок
намеренно без зависимостей (только примитивы), чтобы его можно было
подключать и из server-, и из client-TU.
*/

#ifndef CSQC_CLIENT_H
#define CSQC_CLIENT_H

#include <stddef.h>	// size_t (buf API)
struct usercmd_s;	// ezquake usercmd_t (common.h -> protocol.h); без зависимостей в шапке
struct pr1vm_s;		// PR1 инстанс (pr1vm.h); здесь — только opaque-указатель

// Extended CSQC stats 32..255 live in a dedicated client store (stat index on the
// wire is always a byte, 0..255; FTE-parity MAX_CL_STATS=256). Standard 0..31 stay
// in cl.stats[].
#define MAX_EXTENDED_CL_STATS 256

// Доступ к клиентскому состоянию/выводу (реализация в csqc_client.c):
float CSQC_Client_GetStat (int idx);				// 0..31 -> cl.stats, 32..255 -> ext-статы (int)
void CSQC_Client_SetStat (int idx, int value);		// приём ext-статов 32..255 (CL_SetStat)
// Stat wire 78/79 (float/string CSQC-статы 32..255): приём из svc_fte_updatestatfloat/string
// и выдача через #331 getstatf / #332 getstats. В FTE — per-player statsf[]/statsstr[]
// (pr_csqc.c CL_SetStatNumeric/CL_SetStatString); здесь — единое CSQC-хранилище модуля.
// GetStatInt — точное int-значение для бит-выборки #331 (getstatbits): float-путь теряет
// младшие биты больших int (паритет FTE pr_csqc.c:2826 читает stats[] как int).
int CSQC_Client_GetStatInt (int idx);			// 0..31 -> cl.stats, 32..255 -> ext (int)
float CSQC_Client_GetStatFloat (int idx);			// 0..31 -> cl.stats, 32..255 -> statsf
const char *CSQC_Client_GetStatString (int idx);	// 32..255 -> statss, иначе ""
void CSQC_Client_SetStatFloat (int idx, float value);		// svc_fte_updatestatfloat (79)
void CSQC_Client_SetStatString (int idx, const char *s);	// svc_fte_updatestatstring (78)
void CSQC_Client_GetScreenSize (int *w, int *h);	// vid.width/height (VF_SCREENVSIZE)
void CSQC_Client_DrawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale);
void CSQC_Client_RegisterCommand (const char *cmd);	// привязка registercommand -> консоль
void CSQC_Client_Abort (const char *msg);			// фатально: дисконнект клиента (паритет FTE CSQC_Abort)
// A3/option-2: ограниченное чтение строк клиентской VM (недоверенный csprogs):
// положительный offset за numstrings -> NULL. Ставится как vm->get_string.
char *CSQC_Client_GetString (struct pr1vm_s *vm, int num);

// FTE-пул (слот ≠ серверный номер). entnum-функции работают со слотами пула;
// сетевые номера держатся картой номер→слот (svc 76/92). slot 0 = world.
int CSQC_Client_EntAlloc (struct pr1vm_s *vm);			// первый свободный слот пула (свой) / 0
int CSQC_Client_EntNum (struct pr1vm_s *vm, int raw);		// raw (N*edict_size) -> слот пула; OOB -> world(0)
void CSQC_Client_EntFree (struct pr1vm_s *vm, int slot);	// освободить свою сущность (сеть не трогаем)
int CSQC_Client_NetAllocSlot (struct pr1vm_s *vm);		// слот без s_own (сетевой приём; vm — обнуление слота, R2)
void CSQC_Client_NetFreeSlot (int slot, int number);		// освободить слот + numslot
int CSQC_Client_NumToSlot (int number);					// карта номер→слот / 0
int CSQC_Client_MapNumber (int number, int slot);		// запись карты (возврат slot)
// P1d C1 — обход/диагностика пула и полей модуля.
qbool CSQC_Client_EntUsed (int slot);			// слот занят (сеть или spawn)
int CSQC_Client_EntSpawnBase (void);			// первый используемый слот (1)
int CSQC_Client_EntUsedCount (void);			// число занятых слотов пула
int CSQC_Client_FindField (struct pr1vm_s *vm, const char *name);	// offset поля в float-словах / -1
// C2 (Wave C): кэш офсетов «горячего пути» (резолв один раз при загрузке модуля;
// builtins читают массив, а не сканируют globaldefs/fielddefs по имени каждый кадр).
// Аналог FTE csqcg (pr_common.h:1109-1118) / overlay csqcentvars_t (pr_csqc.c:353-395).
typedef enum
{
	CSQC_TRACEG_FRACTION = 0,
	CSQC_TRACEG_ALLSOLID,
	CSQC_TRACEG_STARTSOLID,
	CSQC_TRACEG_INOPEN,
	CSQC_TRACEG_INWATER,
	CSQC_TRACEG_PLANE_DIST,
	CSQC_TRACEG_ENDPOS,
	CSQC_TRACEG_PLANE_NORMAL,
	CSQC_TRACEG_ENT,
	CSQC_TRACEG_COUNT
} csqc_traceglobal_id_t;

typedef enum
{
	CSQC_FLD_PREDRAW = 0,
	CSQC_FLD_MODELINDEX,
	CSQC_FLD_MODEL,
	CSQC_FLD_COLORMAP,
	CSQC_FLD_ORIGIN,
	CSQC_FLD_ANGLES,
	CSQC_FLD_FRAME,
	CSQC_FLD_SKIN,
	CSQC_FLD_EFFECTS,
	CSQC_FLD_ALPHA,
	CSQC_FLD_SCALE,
	CSQC_FLD_RENDERFLAGS,
	CSQC_FLD_SIZE,
	CSQC_FLD_MINS,
	CSQC_FLD_MAXS,
	CSQC_FLD_MODELFLAGS,
	CSQC_FLD_CHAIN,
	CSQC_FLD_SOLID,
	CSQC_FLD_FLAGS,
	CSQC_FLD_OWNER,
	CSQC_FLD_DRAWMASK,
	CSQC_FLD_COUNT
} csqc_field_id_t;

int CSQC_Client_TraceGlobal (struct pr1vm_s *vm, int id);	// офсет глобала (или -1)
int CSQC_Client_FieldOfs (struct pr1vm_s *vm, int id);		// офсет поля (или -1)
// Окружение builtins «как в FTE»: публикация player_localentnum каждый 2D-кадр
// до CSQC_UpdateView. Сущности игроков НЕ фабрикуются (см. csqc_client.c).
void CSQC_Client_UpdateLocalEntnum (void);

// E1a #371 deltalisten: реестр callback'ов на обновление сущностей по модели
// (FTE PF_DeltaListen). Callback модуля вызывается как CSQC_Ent_Update: `self`
// и `.entnum` выставлены движком, PARM0 = isnew. name=="*" — все модели;
// func<=0 — снятие регистрации. Слушатель игроков получает авторитетное
// (no-lerp) состояние player_state.
void CSQC_Client_DeltaListen (const char *model, int func, int flags);
// C4 Э3 (MASK_DELTA): delta-callback вернул !=0 → движок не рисует сущность
// (рисует модуль через #301); геттеры для cl_ents.c.
qbool CSQC_Client_DeltaPlayerOwned (int pnum);
qbool CSQC_Client_DeltaEntityOwned (int number);

// C5-E Ф1 (no-op revision): view/listener/view_angles + project/unproject.
// `#351 setlistener` — аудио-листенер модуля (используется в cl_main.c S_Update).
void CSQC_Client_SetListener (const float *origin, const float *forward, const float *right, const float *up);
qbool CSQC_Client_ListenerActive (void);
void CSQC_Client_GetListener (float *origin, float *forward, float *right, float *up);
// `#303 setproperty` (VF_*-подмножество): применяется немедленно (same-frame, FTE-паритет)
// + повторно в V_CalcRefdef (cl_view.c). Возврат — распознано ли свойство (FTE 1/0).
qbool CSQC_Client_SetViewProperty (int prop, int argc, const float *args);
void CSQC_Client_ApplyViewProps (void);
void CSQC_Client_ResetViewProps (void);	// #300 clearscene: сброс view-свойств (FTE)
// B22: гейт движкового sbar/HUD (#303 VF_DRAWENGINESBAR) и прицела (VF_DRAWCROSSHAIR)
// под takeover; дефолты clearscene — оба false (FTE pr_csqc.c:2078-2079).
qbool CSQC_Client_DrawEngineSbar (void);
qbool CSQC_Client_DrawCrosshairFlag (void);
// `#310 project` / `#311 unproject`.
qbool CSQC_Client_Project (const float *world, float *sx, float *sy, float *sz);
qbool CSQC_Client_Unproject (float sx, float sy, float sz, float *world);
// `#504 getentity` (C5-E Ф2): интерп. состояние engine-сущности по серверному номеру
// (cl_entities/lerp_origin + player-state). out[3] заполняется (float -> out[0],
// vector -> все три); поля без ezq-источника -> FTE-дефолт (отклонение, parity).
void CSQC_Client_GetEntity (int entnum, int fldnum, float out[3]);

// Ф3: CSQC-реестр моделей (имя → model_t*, индекс 1-based для #200/#333 и поля
// .modelindex). IndexKnown — только поиск (queryonly); Index — поиск или загрузка.
int CSQC_Client_ModelIndexKnown (const char *name);	// 0 если не зарегистрирована
int CSQC_Client_ModelIndex (const char *name);		// 0 если не загрузилась
struct model_s *CSQC_Client_ModelForIndex (int idx);	// NULL если нет
const char *CSQC_Client_ModelNameForIndex (int idx);	// #334: обратный резолв (NULL если нет)
void CSQC_Client_ModelReset (void);
// T4 reload-on-download: перезагрузить CSQC-модель(и), чьё имя совпало со скачанным
// файлом (downloadname = cls.downloadname "<gamedir>/<file>"). Вызывается из CL_FinishDownload.
void CSQC_Client_ModelDownloadFinished (const char *downloadname);

// Точки вызова клиентского жизненного цикла CSQC-VM:
int CSQC_Client_Active (void);			// модуль загружен и не в ошибке
void CSQC_Client_ConnectCheck (void);	// после полного serverinfo: load + CSQC_Init
void CSQC_Client_Disconnect (void);		// CSQC_Shutdown + выгрузка + снятие команд
void CSQC_Client_Update (void);			// каждый 2D-кадр: WorldLoaded-once + UpdateView

// Ф3 (renderscene takeover): когда CSQC-модуль активен, модуль владеет
// 3D-сценой как в FTE — CSQC_UpdateView вызывается в 3D-фазе, #300/#301 строят
// cl_visents, #304 renderscene вызывает R_RenderView(). Иначе — движковый путь.
qbool CSQC_Client_SceneActive (void);	// модуль активен && есть CSQC_UpdateView
void CSQC_Client_BeginScene (void);		// сброс флага «renderscene вызван в этом кадре»
void CSQC_Client_RenderScene (void);	// #304 renderscene -> R_RenderView()
qbool CSQC_Client_SceneRendered (void);	// вызывался ли renderscene в текущем кадре
// C4 Э2 (#301 mask&2): модуль запросил движковую вьюмодель в CSQC-сцене (FTE CL_LinkViewModel).
void CSQC_Client_LinkViewModel (void);
qbool CSQC_Client_SceneViewModel (void);	// запрошена ли вьюмодель в текущем кадре
// C4 #301/#302: вызов .predraw эдикта арены (self=slot; FTE pr_csqc.c:1450-1457).
// Возврат = OFS_RETURN (PREDRAW_AUTOADD=0 -> добавлять, !=0 -> пропустить); *removed —
// эдикт удалён/ошибка исполнения внутри predraw. Контекст self восстанавливается.
float CSQC_Client_CallPredraw (int slot, int fidx, qbool *removed);

// CSQC_Input_Frame: перед отправкой каждого usercmd (CL_SendCmd, cl_input.c).
// Движок заполняет input_* глобалы из cmd, исполняет модуль и пишет обратно
// изменения (см. csqc_client.c; механика FTE pr_csqc.c:9418).
void CSQC_Client_InputFrame (struct usercmd_s *cmd);

// C5-A #345: история отправленных usercmd. seq = зеркало cls.netchan.outgoing_
// sequence (номер клиентского сообщения на момент записи); servercommandframe =
// cl.parsecount (incoming_acknowledged) — окно предикции (servercommandframe,
// clientcommandframe] в одной нумерации (QW эхает подтверждение на netchan/кадре).
void CSQC_Client_RecordInput (struct usercmd_s *cmd);	// запись из CL_SendCmd
int CSQC_Client_ApplyInput (unsigned int seq);			// заполнить input_* по seq; 0/1
// #638 CL_RotateMoves: поворот углов неподтверждённых usercmd (seq > servercommandframe).
int CSQC_Client_RotateMoves (float *anglechange, int seat);
// FTE VectorAngles (up→roll, meshpitch=false): общий хелпер #51 и #638.
void CSQC_VectorAngles (const float *forward, const float *up, float *result);
void CSQC_Client_RunPlayerPhysics (int entnum);			// #347 runstandardplayerphysics (C1.4)
// #1 makevectors (C6.1): по vector-углам пишет v_forward/v_right/v_up модуля.
void CSQC_Client_MakeVectors (float *ang);
// #432 vectorvectors (T3 Э3): по направлению пишет нормализованный v_forward и
// ортогональные v_right/v_up модуля (FTE PF_vectorvectors).
void CSQC_Client_VectorVectors (float *dir);

// C2.2 #460-469 — string-buffers (DP). handle = 1-based; строки deep-copy.
int CSQC_Client_BufCreate (void);
void CSQC_Client_BufDel (int handle);
int CSQC_Client_BufGetSize (int handle);
int CSQC_Client_BufAdd (int handle, const char *s, int order);
int CSQC_Client_BufGet (int handle, int idx, char *out, size_t max);
int CSQC_Client_BufSet (int handle, int idx, const char *s);
int CSQC_Client_BufFree (int handle, int idx);
int CSQC_Client_BufCopy (int from, int to);
int CSQC_Client_BufSort (int handle, int prefixlen, int backward);
int CSQC_Client_BufImplode (int handle, const char *glue, char *out, size_t max);
void CSQC_Client_BufReset (void);

// Слой D, шаг 1 — 2D-графика (draw.h/r_draw*; координаты — сырые пиксели видео,
// как DrawText). Помощники для csqc_builtins.c (см. docs/archive/ezquake_csqc_client_layerd_2d_plan.md).
void CSQC_Client_DrawFill (float x, float y, float w, float h, int r, int g, int b, float alpha);
qbool CSQC_Client_DrawPic (float x, float y, float w, float h, const char *name, int r, int g, int b, float alpha);	// возврат: pic найден (FTE-паритет #322)
void CSQC_Client_DrawSubPic (float x, float y, float w, float h, const char *name, float srcx, float srcy, float srcw, float srch, int r, int g, int b, float alpha);
void CSQC_Client_DrawCharacter (float x, float y, int ch, int r, int g, int b, float alpha, float scale);
void CSQC_Client_DrawLine (float x1, float y1, float x2, float y2, float width, int r, int g, int b, float alpha);
void CSQC_Client_DrawRawText (float x, float y, const char *text, int r, int g, int b, float alpha, float scale);	// без &c-парсинга (#321)
qbool CSQC_Client_IsCachedPic (const char *name);		// #316 iscachedpic
qbool CSQC_Client_PicSize (const char *name, float *w, float *h);	// #318 drawgetimagesize
void CSQC_Client_SetClipArea (float x, float y, float w, float h);	// #324
void CSQC_Client_ResetClipArea (void);				// #325
float CSQC_Client_StringWidth (const char *text, qbool usecolours, float fontsize_x);
qbool CSQC_Client_PrecachePic (const char *name);

// Слой D, шаг 3 — #343 setcursormode (полная реализация, A3.1). Парсинг ABI в
// csqc_builtins.c; здесь состояние курсора модуля и его отрисовка. Пока активен
// CSQC-курсор, mouse-механика ezquake учитывает CSQC_Client_CSQCCursor() (vid_sdl2.c),
// а SCR_DrawCursor рисует курсор модуля (image/hotspot/scale). Клики/InputEvent в
// модуль — C1; здесь — release/grab мыши + собственный курсор + позиция (A3.2 #344).
void CSQC_Client_SetCursorMode (qbool usecursor, const char *image,
	float hotspot_x, float hotspot_y, float scale);
qbool CSQC_Client_CSQCCursor (void);		// usecursor=1 && модуль загружен && в игре
void CSQC_Client_DrawCursor (void);			// отрисовка курсора модуля (SCR_DrawCursor)
void CSQC_Client_GetCursorPos (float *x, float *y);	// позиция указателя (#344, A3.2)
void CSQC_Client_ScaleCursorDelta (float *x, float *y);	// B14: render-2D -> vid.conwidth (дельта мыши)
void CSQC_Client_SetSensitivityScale (float scale);	// #346 setsensitivityscaler (C1.1)
float CSQC_Client_SensitivityScale (void);	// множитель чувствительности (неактивен → 1)

// C1.2 — события ввода модулю (CSQC_InputEvent): клавиши/мышь/колесо.
qbool CSQC_Client_HasInputEvent (void);		// модуль определил CSQC_InputEvent
int CSQC_Client_InputEvent (int evtype, float a, float b, float c);	// возврат handled

// B1/B6 (FTE-parity): notmenu для CSQC_UpdateView (#300) и трансляция
// «внутренний keynum ezq <-> QC/DP-код» (эталон — fteqw pr_csqc.c:8889,
// pr_clcmd.c:14/:218). Применяется к CSQC_InputEvent и keynum-builtins.
qbool CSQC_Client_NotMenu (void);
int CSQC_Client_KeynumToQC (int keynum);
int CSQC_Client_QCToKeynum (int code);

// Типы событий (паритет csdefs.qc IE_*, FTE CSIE_*).
#ifndef IE_KEYDOWN
#define IE_KEYDOWN	0
#define IE_KEYUP	1
#define IE_MOUSEDELTA	2
#define IE_MOUSEABS	3
#define IE_ACCELEROMETER 4
#define IE_FOCUS		5
#define IE_JOYAXIS		6
#endif

// CSQC wire-номера (svc_fte_updatestatstring/float 78/79, svc_fte_cgamepacket 83,
// svc_fte_cgamepacket_sized 90, svc_fte_csqcentities_sized 92, clcfte_qcrequest 81)
// приходят из qwprot src/protocol.h под #ifdef FTE_PEXT_CSQC (upstream master d29fbd4+).
// Формат 78/79 — FTE protocol.h:351-352, fteqw/engine/client/cl_parse.c:8031-8040.

// Runtime-гейт CSQC-парсеров (R5): договорён FTE_PEXT_CSQC и включён cl_pext_csqc
// (как в cl_parse.c case 83/90). Без него 76/92 не должны трактоваться как CSQC.
qbool CSQC_Client_ParseAllowed (void);
// R7/T1.4a: read*-гейт модуля — true только внутри parse-callback'ов
// (CSQC_Ent_Update/CSQC_Parse_Event); иначе read* фатальны (FTE csqc_mayread).
qbool CSQC_Client_MayRead (void);
// Парсинг svc_fte_csqcentities(76) (S1; sized-92 — E3).
void CSQC_Client_ParseEntities (qbool sized);
// Парсинг svc_fte_cgamepacket(83)/sized(90) (E1): модуль сам читает имя + payload.
// sized=true — caller (cl_parse.c case 90) сам дрейнит остаток по длине; sized=false
// (case 83) длины нет — без живого модуля/колбэка это протокольная ошибка (B5,
// FTE Host_EndGame pr_csqc.c:9240-9247).
void CSQC_Client_ParseEvent (qbool sized);

// Э1: сетевые печатные колбэки. ParsePrint — true, если модуль обработал (движок
// подавляет свой print, FTE pr_csqc.c:9306); ParseCenterPrint — true, если модуль
// вернул != 0 (подавить centerprint, FTE pr_csqc.c:9385).
qbool CSQC_Client_ParsePrint (const char *msg, int level);
qbool CSQC_Client_ParseCenterPrint (const char *msg);

// Э2: сетевой колбэк урона. true, если модуль вернул != 0 (подавить цветосдвиг/
// view-kick движка, FTE pr_csqc.c:9287, view.c:513).
qbool CSQC_Client_ParseDamage (float save, float take, const vec3_t source);

// Э3: сетевой колбэк звука. true, если модуль вернул != 0 (движок звук не играет,
// FTE pr_csqc.c:9453, cl_parse.c:5336/5543). self = arena-энтити по entnum или world.
qbool CSQC_Client_EventSound (int entnum, int channel, const char *name, float vol,
							  float atten, const vec3_t pos, float pitchmod, float flags);

// Э4: сетевой колбэк принудительных углов. true, если модуль вернул != 0 (движок свой
// угол не применяет, FTE pr_csqc.c:9400, cl_parse.c:7527/7857/9816).
qbool CSQC_Client_ParseSetAngles (const float *angles, float isdelta);

// Э5: движковый колбэк CSQC_RendererRestarted(string rendererdescription) — вызывается
// при переинициализации рендерера (vid_restart/vid_reload) и при загрузке модуля.
// FTE pr_csqc.c:8314, pr_common.h:1096: return не читается (suppress нет).
void CSQC_Client_RendererRestarted (const char *desc);

// Register the builtin table of the client instance (implemented in csqc_builtins.c).
void CSQCVM_RegisterBuiltins (struct pr1vm_s *vm);

// Клиентская обёртка над единым PR1VM_SetString (core, pr_edict.c): temp-строки
// deep-copy в клиентское кольцо инстанса (стабильный буфер) и регистрируются
// через vm->strtbl. Строки из области модуля передаются в core без копии.
// Работает с PR1-VM (в отличие от PR2). address — string_t* (int*).
void PR1VM_ClientSetString (struct pr1vm_s *vm, int *address, char *s);

// Register client debug commands for PR1VM (csqc_smoke, etc.; csqc_client.c) —
// called from CL_InitLocal (cl_main.c).
void CSQC_Client_RegisterCommands (void);

#endif /* CSQC_CLIENT_H */
