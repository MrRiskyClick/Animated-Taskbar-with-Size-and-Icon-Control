// CI-only declarations for syntax-checking the read-only Windhawk source.
// This is NOT a substitute for compiling with real Windhawk's toolchain.
#pragma once
#include <windows.h>
extern "C" {
PCWSTR Wh_GetStringSetting(PCWSTR name);
void Wh_FreeStringSetting(PCWSTR value);
int Wh_GetIntSetting(PCWSTR name);
void Wh_Log(PCWSTR format, ...);
}
