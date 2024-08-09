#include "pch.h"

#define wanted_arg_count(n) \
if (n > 0 && lua_gettop(thread) < n) [[unlikely]] { \
	lua_pushfstring(thread, __FUNCTION__" expects at least " #n " arguments, received %d", lua_gettop(thread)); \
	lua_error(thread); \
	return 0; \
}

int compile(lua_State* thread) {
	wanted_arg_count(1);
	size_t source_length;
	const char* source_c_str = luaL_checklstring(thread, 1, &source_length);
	Luau::CompileOptions options;
	options.optimizationLevel = 1;
	options.debugLevel = 1;
	if (lua_gettop(thread) >= 2) {
		luaL_checktype(thread, 2, LUA_TTABLE);
		if (lua_getfield(thread, 2, "optimizationLevel")) {
			unsigned int optimization_level = luaL_checkunsigned(thread, -1);
			lua_pop(thread, 1);
			if (optimization_level > 2) {
				lua_pushstring(thread, "When compiling, optimizationLevel must be an integer from 0 to 2");
				lua_error(thread);
			}
			options.optimizationLevel = optimization_level;
		}
		if (lua_getfield(thread, 2, "debugLevel")) {
			unsigned int debug_level = luaL_checkunsigned(thread, -1);
			lua_pop(thread, 1);
			if (debug_level > 2) {
				lua_pushstring(thread, "When compiling, debugLevel must be an integer from 0 to 2");
				lua_error(thread);
			}
			options.debugLevel = debug_level;
		}
	}
	std::string bytecode = Luau::compile(std::string(source_c_str, source_length), options, {});
	if (bytecode.data()[0] == '\0') {
		lua_pushlstring(thread, bytecode.data() + 1, bytecode.size() - 1);
		lua_error(thread);
		return 0;
	}
	lua_pushlstring(thread, bytecode.data(), bytecode.size());
	return 1;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(compile),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "luau", library);
}