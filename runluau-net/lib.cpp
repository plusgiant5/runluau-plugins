#include "pch.h"

int hello(lua_State* thread) {
	printf("Working\n");
	return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(hello),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "net", library);
}