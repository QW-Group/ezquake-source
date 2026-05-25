#ifndef NICK_OVERRIDE_H
#define NICK_OVERRIDE_H

#include <stdio.h>

struct player_info_s;

void Nick_Init(void);
void Nick_Shutdown(void);
void Nick_ClearOverrides(void);
void Nick_RefreshShortnames(void);
const char *Nick_PlayerDisplayName(const struct player_info_s *player);
const char *Nick_OverrideForPlayer(const struct player_info_s *player);
qbool Nick_HasOverrideForPlayer(const struct player_info_s *player);
void Nick_WriteOverrides(FILE *f);

#endif // NICK_OVERRIDE_H
