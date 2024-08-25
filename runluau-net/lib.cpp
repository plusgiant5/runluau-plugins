#include "pch.h"

#define wanted_arg_count(n) \
if (n > 0 && lua_gettop(L) < n) [[unlikely]] { \
    lua_pushfstring(L, __FUNCTION__" expects at least " #n " arguments, received %d", lua_gettop(L)); \
    lua_error(L); \
    return 0; \
}

static int test(lua_State* L) {
    printf("Working\n");
    return 0;
}

static int wait(lua_State* L) {
    wanted_arg_count(1);
    
    double seconds = luaL_checknumber(L, 1);
    if (seconds < 0) {
        return luaL_error(L, "wait time must be non-negative");
    }

    std::this_thread::sleep_for(std::chrono::duration<double>(seconds));
    
    return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
    reg(test),
    reg(wait),  
    {NULL, NULL}
};

extern "C" __declspec(dllexport) void register_library(lua_State* L) {
    luaL_register(L, "template", library);
}
