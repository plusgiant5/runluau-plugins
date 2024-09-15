#include "pch.h"

int read_file(lua_State* thread) {
	wanted_arg_count(1);
	size_t length;
	const char* path_c_str = luaL_checklstring(thread, 1, &length);
	fs::path path(std::string(path_c_str, length));
	std::ifstream file(path, std::ios::binary);
	if (!file) [[unlikely]] {
		lua_pushfstring(thread, "Failed to read file: %s", std::strerror(errno));
		lua_error(thread);
		return 0;
	}
	std::vector<char> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	file.close();
	std::string content(buffer.begin(), buffer.end());
	lua_pushlstring(thread, content.data(), content.size());
	return 1;
}

int write_file(lua_State* thread) {
	wanted_arg_count(2);
	size_t path_length;
	const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
	fs::path path(std::string(path_c_str, path_length));
	size_t content_length;
	const char* content = luaL_checklstring(thread, 2, &content_length);
	std::ofstream file(path, std::ios::binary);
	if (!file) [[unlikely]] {
		lua_pushfstring(thread, "Failed to write file: %s", std::strerror(errno));
		lua_error(thread);
		return 0;
	}
	file.write(content, content_length);
	file.close();
	return 0;
}

int append_file(lua_State* thread) {
	wanted_arg_count(2);
	size_t path_length;
	const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
	fs::path path(std::string(path_c_str, path_length));
	size_t content_length;
	const char* content = luaL_checklstring(thread, 2, &content_length);
	std::ofstream file(path, std::ios::binary | std::ios::app);
	if (!file) [[unlikely]] {
		lua_pushfstring(thread, "Failed to append file: %s", std::strerror(errno));
		lua_error(thread);
		return 0;
	}
	file.write(content, content_length);
	file.close();
	return 0;
}

int exists(lua_State* thread) {
	wanted_arg_count(1);
	lua_pushboolean(thread, fs::exists(fs::path(luaL_checkstring(thread, 1))));
	return 1;
}

int is_file(lua_State* thread) {
	wanted_arg_count(1);
	lua_pushboolean(thread, fs::is_regular_file(fs::path(luaL_checkstring(thread, 1))));
	return 1;
}

int is_folder(lua_State* thread) {
	wanted_arg_count(1);
	lua_pushboolean(thread, fs::is_directory(fs::path(luaL_checkstring(thread, 1))));
	return 1;
}

int list(lua_State* thread) {
	wanted_arg_count(1);
	fs::path path(luaL_checkstring(thread, 1));
	if (!fs::is_directory(path)) [[unlikely]] {
		if (fs::exists(path)) [[unlikely]] {
			lua_pushstring(thread, "Failed to list files: Expected folder, found file");
		} else {
			lua_pushstring(thread, "Failed to list files: Nothing found at path");
		}
		lua_error(thread);
		return 0;
	}
	lua_newtable(thread);
	int current = 0;
	for (const auto& file : fs::directory_iterator(path)) {
		std::string filename = file.path().filename().string();
		lua_pushinteger(thread, ++current);
		lua_pushlstring(thread, filename.data(), filename.size());
		lua_settable(thread, -3);
	}
	return 1;
}

int new_folder(lua_State* thread) {
	wanted_arg_count(1);
	size_t path_length;
	const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
	fs::path path(std::string(path_c_str, path_length));
	if (!fs::create_directory(path)) {
		lua_pushfstring(thread, "Failed to make folder: Folder already exists");
		lua_error(thread);
		return 0;
	}
	return 0;
}

int delete_file(lua_State* thread) {
	wanted_arg_count(1);
	size_t path_length;
	const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
	fs::path path(std::string(path_c_str, path_length));
	if (!fs::remove(path)) {
		lua_pushfstring(thread, "Failed to delete file: File not found");
		lua_error(thread);
		return 0;
	}
	return 0;
}

int delete_folder(lua_State* thread) {
	wanted_arg_count(1);
	size_t path_length;
	const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
	fs::path path(std::string(path_c_str, path_length));
	if (!fs::remove_all(path)) {
		lua_pushfstring(thread, "Failed to delete folder: Folder not found");
		lua_error(thread);
		return 0;
	}
	return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(read_file),
	reg(write_file),
	reg(exists),
	reg(is_file),
	reg(is_folder),
	reg(list),
	reg(new_folder),
	reg(delete_file),
	reg(delete_folder),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "fs", library);
}