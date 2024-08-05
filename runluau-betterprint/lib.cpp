#include "lib.h"

#include <stdio.h>

#define wanted_arg_count(n) \
if (n > 0 && lua_gettop(thread) < n) [[unlikely]] { \
	lua_pushfstring(thread, __FUNCTION__" expects at least " #n " arguments, received %d", lua_gettop(thread)); \
	lua_error(thread); \
	return 0; \
}

int test(lua_State* thread) {
	wanted_arg_count(0);
	printf("Working\n");
	return 0;
}

#define r(name) {#name, name}
constexpr luaL_Reg fs_library[] = {
	r(test),
	{NULL, NULL}
};
void register_library(lua_State* state) {
	luaL_register(state, "template", fs_library);
}