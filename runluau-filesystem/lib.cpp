#include "pch.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>
#include <iostream>
#include <system_error>

namespace fs = std::filesystem;

bool is_unsafe;

static bool get_safe_path(lua_State* thread, const fs::path& user_path, fs::path& out_path) {
    fs::path sandbox = fs::current_path() / "runluau-filesystem";

    std::error_code ec;
    if (!fs::exists(sandbox, ec)) {
        if (!fs::create_directory(sandbox, ec)) {
            lua_pushstring(thread, "Filesystem safe mode: Failed to create sandbox directory");
            lua_error(thread);
            return false;
        }
    }

    fs::path relative = user_path;
    if (relative.is_absolute()) {
        relative = relative.relative_path();
    }

    relative = relative.lexically_normal();

    for (const auto& part : relative) {
        if (part == "..") {
            lua_pushstring(thread, "Filesystem safe mode: Path traversal detected");
            lua_error(thread);
            return false;
        }
    }

    fs::path safe_path = sandbox / relative;
    safe_path = safe_path.lexically_normal();

    std::string sandbox_str = sandbox.string();
    std::string safe_path_str = safe_path.string();
    if (safe_path_str.compare(0, sandbox_str.size(), sandbox_str) != 0) {
        lua_pushstring(thread, "Filesystem safe mode: Path escapes sandbox");
        lua_error(thread);
        return false;
    }

    out_path = safe_path;
    return true;
}

int read_file(lua_State* thread) {
    wanted_arg_count(1);
    stack_slots_needed(1);

    size_t length;
    const char* path_c_str = luaL_checklstring(thread, 1, &length);
    fs::path path(std::string(path_c_str, length));

    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    if (fs::is_directory(path)) {
        lua_pushstring(thread, "Path is a directory, cannot open as a file");
        lua_error(thread);
        return 0;
    }

    std::ifstream file(path, std::ios::binary);
    if (!file) {
        lua_pushfstring(thread, "Failed to read file: %s", std::strerror(errno));
        lua_error(thread);
        return 0;
    }

    std::vector<char> buffer((std::istreambuf_iterator<char>(file)),
                              std::istreambuf_iterator<char>());
    file.close();
    std::string content(buffer.begin(), buffer.end());
    lua_pushlstring(thread, content.data(), content.size());
    return 1;
}

int write_file(lua_State* thread) {
    wanted_arg_count(2);
    stack_slots_needed(1);

    size_t path_length;
    const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
    fs::path path(std::string(path_c_str, path_length));

    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    size_t content_length;
    const char* content = luaL_checklstring(thread, 2, &content_length);

	fs::create_directories(path.parent_path());

    if (fs::is_directory(path)) {
        lua_pushstring(thread, "Path is a directory, cannot open as a file");
        lua_error(thread);
        return 0;
    }

    std::ofstream file(path, std::ios::binary);
    if (!file) {
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
    stack_slots_needed(1);

    size_t path_length;
    const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
    fs::path path(std::string(path_c_str, path_length));

    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    size_t content_length;
    const char* content = luaL_checklstring(thread, 2, &content_length);

    if (fs::is_directory(path)) {
        lua_pushstring(thread, "Path is a directory, cannot open as a file");
        lua_error(thread);
        return 0;
    }

    std::ofstream file(path, std::ios::binary | std::ios::app);
    if (!file) {
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
    stack_slots_needed(1);

    const char* path_c_str = luaL_checkstring(thread, 1);
    fs::path path(path_c_str);

    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    lua_pushboolean(thread, fs::exists(path));
    return 1;
}

int is_file(lua_State* thread) {
    wanted_arg_count(1);
    stack_slots_needed(1);

    fs::path path(luaL_checkstring(thread, 1));
    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    lua_pushboolean(thread, fs::is_regular_file(path));
    return 1;
}

int is_folder(lua_State* thread) {
    wanted_arg_count(1);
    stack_slots_needed(1);

    fs::path path(luaL_checkstring(thread, 1));
    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    lua_pushboolean(thread, fs::is_directory(path));
    return 1;
}

int list(lua_State* thread) {
    wanted_arg_count(1);
    stack_slots_needed(3);

    fs::path path(luaL_checkstring(thread, 1));
    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    if (!fs::is_directory(path)) {
        if (fs::exists(path)) {
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
    stack_slots_needed(1);

    size_t path_length;
    const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
    fs::path path(std::string(path_c_str, path_length));

    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    if (!fs::create_directory(path)) {
        lua_pushstring(thread, "Failed to make folder: Folder already exists");
        lua_error(thread);
        return 0;
    }
    return 0;
}

int delete_file(lua_State* thread) {
    wanted_arg_count(1);
    stack_slots_needed(1);

    size_t path_length;
    const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
    fs::path path(std::string(path_c_str, path_length));

    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    if (!fs::remove(path)) {
        lua_pushstring(thread, "Failed to delete file: File not found");
        lua_error(thread);
        return 0;
    }
    return 0;
}

int delete_folder(lua_State* thread) {
    wanted_arg_count(1);
    stack_slots_needed(1);

    size_t path_length;
    const char* path_c_str = luaL_checklstring(thread, 1, &path_length);
    fs::path path(std::string(path_c_str, path_length));

    if (!is_unsafe) {
        if (!get_safe_path(thread, path, path)) {
            return 0;
        }
    }

    if (fs::remove_all(path) == 0) {
        lua_pushstring(thread, "Failed to delete folder: Folder not found");
        lua_error(thread);
        return 0;
    }
    return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
    reg(read_file),
    reg(write_file),
    reg(append_file),
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
    is_unsafe = luau::is_plugin_loaded("runluau-osunsafe.dll");
}
