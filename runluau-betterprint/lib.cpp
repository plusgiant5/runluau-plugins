#include "pch.h"

#include "print.hpp"

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

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(print),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "_G", library);
}