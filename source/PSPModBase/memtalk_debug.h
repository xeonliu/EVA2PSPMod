#pragma once

#include <stdint.h>

void MemTalkDebug_SetGameBase(uintptr_t gameBase);
void MemTalkDebug_InstallHook(void);
void *MemTalkDebug_ShowMemorySentence(void *ctx, const void *rec, const char *verbSjis);

