#include "lib.h"

#include <iostream>
#include <stdio.h>

#include "print.hpp"

#define wanted_arg_count(n) \
if (n > 0 && lua_gettop(thread) < n) [[unlikely]] { \
	lua_pushfstring(thread, __FUNCTION__" expects at least " #n " arguments, received %d", lua_gettop(thread)); \
	lua_error(thread); \
	return 0; \
}

int print(lua_State* thread) {
	int arg_count = lua_gettop(thread);
	for (int arg = 1; arg <= arg_count; arg++) {
		std::string str = bettertostring(thread, arg);
		if (arg == arg_count) [[likely]] {
			std::cout << str << std::endl;
		} else {
			std::cout << str << ' ';
		}
	}
	return 0;
}

#define r(name) {#name, name}
constexpr luaL_Reg library[] = {
	r(print),
	{NULL, NULL}
};
void register_library(lua_State* state) {
	luaL_register(state, "_G", library);
}