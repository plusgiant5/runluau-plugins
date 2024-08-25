#include "pch.h"
#include "taskscheduler.hpp"

#define wanted_arg_count(n) \
if (n > 0 && lua_gettop(thread) < n) [[unlikely]] { \
    lua_pushfstring(thread, __FUNCTION__" expects at least " #n " arguments, received %d", lua_gettop(thread)); \
    lua_error(thread); \
    return 0; \
}

static TaskScheduler scheduler;

static int schedule_task(lua_State* thread) {
    wanted_arg_count(2);
    
    if (!lua_isfunction(thread, 1)) {
        luaL_error(thread, "First argument must be a function");
    }
    
    double delay = luaL_checknumber(thread, 2);
    
    lua_pushvalue(thread, 1);  
    int ref = luaL_ref(thread, LUA_REGISTRYINDEX); 
    
    scheduler.schedule_task(ref, delay);
    
    return 0;
}

static int run_tasks(lua_State* thread) {
    scheduler.run_tasks(thread);
    return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
    reg(schedule_task),
    reg(run_tasks),
    {NULL, NULL}
};

extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
    luaL_register(thread, "taskscheduler", library);
}
