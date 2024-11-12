#include "pch.h"

int test(lua_State* thread) {
    printf("Working\n");
    return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
    reg(test),
    {NULL, NULL}
};

extern "C"
#ifdef _WIN32
__declspec(dllexport)
#else
__attribute__((visibility("default")))
#endif
void register_library(lua_State* thread) {
    luaL_register(thread, "template", library);
}
