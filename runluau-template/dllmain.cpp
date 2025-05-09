#include "pch.h"

#ifdef _WIN32
// Windows entry point
BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
	return TRUE;
}

#else
// Unix-like systems entry point
__attribute__((constructor)) void on_library_load() {
    // Code to run when the shared library is loaded
}

__attribute__((destructor)) void on_library_unload() {
    // Code to run when the shared library is unloaded
}
#endif
