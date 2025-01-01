#include "pch.h"

int execute(lua_State* thread) {
	wanted_arg_count(1);
	const char* command = luaL_checkstring(thread, 1);
	int code = system(command);
	if (code == ERROR_SUCCESS) {
		return 0;
	} else {
		code = errno;
		char* error_message = new char[256];
		strerror_s(error_message, 256, code);
		lua_pushfstring(thread, "Command failed with error code %d: %s", code, error_message);
		lua_error(thread);
		delete[] error_message;
		return 0;
	}
}

int capture(lua_State* thread) {
	wanted_arg_count(1);
	const char* command = luaL_checkstring(thread, 1);

	FILE* pipe = _popen(command, "r");
	if (!pipe) {
		int code = errno;
		char error_message[256];
		strerror_s(error_message, sizeof(error_message), code);
		lua_pushfstring(thread, "Command failed with error code %d: %s. Unable to capture.", code, error_message);
		lua_error(thread);
		return 0;
	}

	std::string output;
	char buffer[256];
	while (fgets(buffer, sizeof(buffer), pipe)) {
		output += buffer;
	}

	int status = _pclose(pipe);
	if (status != 0) {
		int code = errno;
		char error_message[256];
		strerror_s(error_message, sizeof(error_message), code); 
		lua_pushfstring(thread, "Command failed with error code %d: %s. Captured:\n%s", code, error_message, output.data());
		lua_error(thread);
		return 0;
	}

	lua_pushlstring(thread, output.c_str(), output.size());
	return 1;
}

int getenv(lua_State* thread) {
	wanted_arg_count(1);
	const char* name = luaL_checkstring(thread, 1);
	const char* value = std::getenv(name);
	if (value) {
		lua_pushstring(thread, value);
	} else {
		lua_pushnil(thread);
	}
	return 1;
}
int setenv(lua_State* thread) {
	wanted_arg_count(2);
	const char* name = luaL_checkstring(thread, 1);
	const char* value = luaL_checkstring(thread, 2);
	if (!SetEnvironmentVariableA(name, value)) {
		lua_pushfstring(thread, "Failed to SetEnvironmentVariableA: %d", GetLastError());
		lua_error(thread);
		return 0;
	}
	return 0;
}

#define reg(name) lua_pushcfunction(thread, name, #name); lua_setfield(thread, -2, #name)
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	lua_newtable(thread);
	reg(execute);
	reg(capture);
	reg(getenv);
	reg(setenv);
	lua_setglobal(thread, "os");
}