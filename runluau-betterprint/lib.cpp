#include "pch.h"

#include "print.hpp"

int print(lua_State* thread) {
	int arg_count = lua_gettop(thread);
	for (int arg = 1; arg <= arg_count; arg++) {
		std::string str = bettertostring(thread, arg);
		if (arg == arg_count) {
			std::cout << str << std::endl;
			} else {
			std::cout << str << ' ';
		}
	}
	return 0;
}

int warn(lua_State* thread) {
	int arg_count = lua_gettop(thread);
	std::cout << YELLOW;
	for (int arg = 1; arg <= arg_count; arg++) {
		std::string str = bettertostring<false, false>(thread, arg);
		if (arg == arg_count) {
			std::cout << str << std::endl;
			} else {
			std::cout << str << ' ';
		}
	}
	std::cout << RESET;
	return 0;
}

int better_tostring(lua_State* thread) {
	if (lua_gettop(thread) == 0) {
		lua_pushstring(thread, "nil");
		return 1;
	} else {
		std::string str = bettertostring<false, false>(thread, 1);
		lua_pushlstring(thread, str.data(), str.size());
		return 1;
	}
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(print),
	reg(warn),
	reg(better_tostring),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "_G", library);
}