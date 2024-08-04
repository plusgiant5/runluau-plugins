#include <Windows.h>

#include "lib.h"

extern "C" __declspec(dllexport) void __fastcall runluau_load(lua_State* state) {
	register_library(state);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved) {
	return TRUE;
}