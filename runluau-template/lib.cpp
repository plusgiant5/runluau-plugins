#include "pch.h"

#define wanted_arg_count(n) \
if (n > 0 && lua_gettop(thread) < n) [[unlikely]] { \
	lua_pushfstring(thread, __FUNCTION__" expects at least " #n " arguments, received %d", lua_gettop(thread)); \
	lua_error(thread); \
	return 0; \
}

static int test(lua_State* L) {
	printf("Working\n");
	return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(test),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "template", library);
}