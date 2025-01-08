#include "pch.h"

int execute(lua_State* thread) {
	wanted_arg_count(1);
	stack_slots_needed(1);
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
	stack_slots_needed(1);
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
	stack_slots_needed(1);
	const char* name = luaL_checkstring(thread, 1);
	const char* value = std::getenv(name);
	if (value) {
		lua_pushstring(thread, value);
	} else {
		lua_pushnil(thread);
	}
	return 1;
}
// Yea I copied it because precompiled headers
void SetPermanentEnvironmentVariable(const char* name, const char* value, bool system) {
	HKEY key;
	LSTATUS open_status;
	if (system) {
		open_status = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "System\\CurrentControlSet\\Control\\Session Manager\\Environment", 0, KEY_SET_VALUE, &key);
	} else {
		open_status = RegOpenKeyExA(HKEY_CURRENT_USER, "Environment", 0, KEY_SET_VALUE, &key);
	}
	if (open_status != ERROR_SUCCESS) {
		throw std::runtime_error(std::string("RegOpenKeyExA inside SetPermanentEnvironmentVariable failed with ") + std::to_string(open_status));
	}
	LSTATUS set_status = RegSetValueExA(key, name, 0, REG_SZ, (LPBYTE)value, (DWORD)(strlen(value) + 1));
	RegCloseKey(key);
	if (set_status != ERROR_SUCCESS) {
		throw std::runtime_error(std::string("RegSetValueExA inside SetPermanentEnvironmentVariable failed with ") + std::to_string(set_status));
	}
	SendMessageTimeoutA(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)"Environment", SMTO_BLOCK, 100, NULL);
}
int setenv(lua_State* thread) {
	wanted_arg_count(2);
	stack_slots_needed(1);
	const char* name = luaL_checkstring(thread, 1);
	const char* value = luaL_checkstring(thread, 2);
	if (lua_gettop(thread) >= 3) {
		std::string global_mode = luaL_checkstring(thread, 3);
		bool system;
		if (global_mode == "user") {
			system = false;
		} else if (global_mode == "system") {
			system = true;
		} else {
			lua_pushstring(thread, "Expected `global_mode` to be \"user\" or \"system\". If you don't want it to be global, don't specify the third argument.");
			lua_error(thread);
			return 0;
		}
		try {
			SetPermanentEnvironmentVariable(name, value, system);
		} catch (std::runtime_error error) {
			lua_pushstring(thread, error.what());
			lua_error(thread);
			return 0;
		}
	}
	if (!SetEnvironmentVariableA(name, value)) {
		lua_pushfstring(thread, "Failed to SetEnvironmentVariableA: %d", GetLastError());
		lua_error(thread);
		return 0;
	}
	return 0;
}

int getclipboard(lua_State* thread) {
	wanted_arg_count(1);
	stack_slots_needed(1);

	if (!OpenClipboard(nullptr)) {
		lua_pushfstring(thread, "Failed to open clipboard: %d", GetLastError());
		lua_error(thread);
		return 0;
	}

	HANDLE clipboard_data = GetClipboardData(CF_TEXT);
	if (!clipboard_data) {
		lua_pushfstring(thread, "Failed to get clipboard data: %d", GetLastError());
		lua_error(thread);
		CloseClipboard();
		return 0;
	}

	char* locked_text = static_cast<char*>(GlobalLock(clipboard_data));
	if (!locked_text) {
		lua_pushfstring(thread, "Failed to lock global memory: %d", GetLastError());
		lua_error(thread);
		CloseClipboard();
		return 0;
	}

	lua_pushstring(thread, locked_text);
	GlobalUnlock(clipboard_data);
	CloseClipboard();
	return 1;
}
int setclipboard(lua_State* thread) {
	wanted_arg_count(1);
	stack_slots_needed(1);

	const char* content = luaL_checkstring(thread, 1);

	if (!OpenClipboard(nullptr)) {
		lua_pushfstring(thread, "Failed to open clipboard: %d", GetLastError());
		lua_error(thread);
		return 0;
	}

	if (!EmptyClipboard()) {
		lua_pushfstring(thread, "Failed to empty clipboard: %d", GetLastError());
		lua_error(thread);
		CloseClipboard();
		return 0;
	}

	size_t length = strlen(content) + 1;
	HGLOBAL memory_handle = GlobalAlloc(GMEM_MOVEABLE, length);
	if (!memory_handle) {
		lua_pushfstring(thread, "Failed to allocate global memory: %d", GetLastError());
		lua_error(thread);
		CloseClipboard();
		return 0;
	}

	void* memory_ptr = GlobalLock(memory_handle);
	if (!memory_ptr) {
		lua_pushfstring(thread, "Failed to lock global memory: %d", GetLastError());
		lua_error(thread);
		CloseClipboard();
		return 0;
	}

	memcpy(memory_ptr, content, length);
	GlobalUnlock(memory_handle);

	if (!SetClipboardData(CF_TEXT, memory_handle)) {
		lua_pushfstring(thread, "Failed to set clipboard data: %d", GetLastError());
		lua_error(thread);
		CloseClipboard();
		return 0;
	}

	CloseClipboard();
	return 0;
}

#define reg(name) lua_pushcfunction(thread, name, #name); lua_setfield(thread, -2, #name)
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	lua_getglobal(thread, "os");
	reg(execute);
	reg(capture);
	reg(getenv);
	reg(setenv);
	reg(getclipboard);
	reg(setclipboard);
	lua_setglobal(thread, "os");
}