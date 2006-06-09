//
// common HUD elements
// like clock etc..
//

#ifndef __HUD_COMMON__H__
#define __HUD_COMMON__H__

extern hud_t *hud_netgraph;

void SCR_HUD_Netgraph(hud_t *hud);
void SCR_HUD_DrawFPS(hud_t *hud);
void SCR_HUD_DrawNetStats(hud_t *hud);

void SCR_HUD_DrawGun2 (hud_t *hud);
void SCR_HUD_DrawGun3 (hud_t *hud);
void SCR_HUD_DrawGun4 (hud_t *hud);
void SCR_HUD_DrawGun5 (hud_t *hud);
void SCR_HUD_DrawGun6 (hud_t *hud);
void SCR_HUD_DrawGun7 (hud_t *hud);
void SCR_HUD_DrawGun8 (hud_t *hud);
void SCR_HUD_DrawGunCurrent (hud_t *hud);

void HUD_NewMap();
void SCR_HUD_DrawMVDRadar(hud_t *hud);

void HudCommon_Init(void);
void SCR_HUD_DrawNum(hud_t *hud, int num, qbool low,
                     float scale, int style, int digits, char *s_align);
#endif // __HUD_COMMON__H__
