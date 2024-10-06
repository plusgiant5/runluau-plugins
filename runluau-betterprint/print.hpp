#pragma once

#include "pch.h"

bool is_valid_name(std::string name) {
	size_t length = name.size();
	if (length == 0) {
		return false;
	}
	const char* data = name.data();
	switch (data[0]) {
	// Thanks for not having `...`
	case '_':
	case 'q': case 'w': case 'e': case 'r': case 't': case 'y': case 'u': case 'i': case 'o': case 'p':
	case 'a': case 's': case 'd': case 'f': case 'g': case 'h': case 'j': case 'k': case 'l':
	case 'z': case 'x': case 'c': case 'v': case 'b': case 'n': case 'm':
	case 'Q': case 'W': case 'E': case 'R': case 'T': case 'Y': case 'U': case 'I': case 'O': case 'P':
	case 'A': case 'S': case 'D': case 'F': case 'G': case 'H': case 'J': case 'K': case 'L':
	case 'Z': case 'X': case 'C': case 'V': case 'B': case 'N': case 'M':
		for (size_t i = 1; i < length; i++) {
			switch (data[i]) {
			case '_':
			case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': case '0':
			case 'q': case 'w': case 'e': case 'r': case 't': case 'y': case 'u': case 'i': case 'o': case 'p':
			case 'a': case 's': case 'd': case 'f': case 'g': case 'h': case 'j': case 'k': case 'l':
			case 'z': case 'x': case 'c': case 'v': case 'b': case 'n': case 'm':
			case 'Q': case 'W': case 'E': case 'R': case 'T': case 'Y': case 'U': case 'I': case 'O': case 'P':
			case 'A': case 'S': case 'D': case 'F': case 'G': case 'H': case 'J': case 'K': case 'L':
			case 'Z': case 'X': case 'C': case 'V': case 'B': case 'N': case 'M':
				[[likely]]
				break;
			default:
				return false;
			}
		}
		return true;
	default:
		return false;
	}
}

constexpr size_t MAX_DEPTH = 4;
template<bool quotes_around_strings = false, bool colors = true> std::string bettertostring(lua_State* thread, int object_location, std::string indent = "", std::unordered_set<const void*> encountered = {}, size_t depth = 0) {
	int raw_type = lua_type(thread, object_location);
	if (raw_type == -1) {
		return "NULL";
	}
	lua_Type type = (lua_Type)raw_type;
	switch (type) {
	case LUA_TNIL:
		if (colors) {
			return NIL_COLOR + "nil" + RESET;
		} else {
			return "nil";
		}
	case LUA_TBOOLEAN:
	{
		int value = lua_toboolean(thread, object_location);
		if (colors) {
			return BOOLEAN_COLOR + (value ? "true" : "false") + RESET;
		} else {
			return value ? "true" : "false";
		}
	}
	case LUA_TTABLE:
	{
		if (depth > MAX_DEPTH) {
			if (colors) {
				return TABLE_COLOR + "<max depth>" + RESET;
			} else {
				return "<max depth>";
			}
		}
		std::string new_indent = indent;
		const void* table = lua_topointer(thread, object_location);
		if (encountered.find(table) == encountered.end()) {
			encountered.insert(table);
		} else {
			if (colors) {
				return TABLE_COLOR + "<cyclic reference>" + RESET;
			} else {
				return "<cyclic reference>";
			}
		}
		std::string output;
		if (colors)
			output += TABLE_COLOR;
		output += '{';
		if (colors)
			output += RESET;
		int array_length = lua_objlen(thread, object_location);
		bool multi_line = false;
		if (array_length == 0) [[unlikely]] {
			multi_line = true;
		}
		bool has_hash = false;
		if (array_length > 0) [[unlikely]] {
			if (lua_rawiter(thread, object_location, array_length) >= 0) [[unlikely]] {
				lua_pop(thread, 2);
				multi_line = true;
			}
		}
		if (multi_line) [[likely]] {
			new_indent += '\t';
			if (array_length > 0) [[unlikely]] {
				output += '\n';
				output += new_indent;
			}
		}
		bool empty = true;
		bool last_was_array = false;
		for (int index = 0; index = lua_rawiter(thread, object_location, index), index >= 0;) {
			empty = false;
			if (index <= array_length) [[unlikely]] {
				output += bettertostring<true, colors>(thread, -1, new_indent, encountered, depth + 1);
				if (index == array_length) [[unlikely]] {
					if (multi_line) {
						if (colors)
							output += TABLE_COLOR;
						output += ", ";
						if (colors)
							output += RESET;
					}
				} else {
					if (colors)
						output += TABLE_COLOR;
					output += ", ";
					if (colors)
						output += RESET;
				}
			} else {
				output += '\n';
				output += new_indent;
				bool did_name_key = false;
				if (lua_type(thread, -2) == LUA_TSTRING) [[likely]] {
					size_t key_length;
					const char* key_content = luaL_tolstring(thread, -2, &key_length);
					lua_pop(thread, 1);
					std::string key = std::string(key_content, key_length);
					if (is_valid_name(key)) [[likely]] {
						output += key;
						if (colors)
							output += TABLE_COLOR;
						output += " = ";
						if (colors)
							output += RESET;
						did_name_key = true;
					}
				}
				if (!did_name_key) [[unlikely]] {
					if (colors)
						output += TABLE_COLOR;
					output += '[';
					if (colors)
						output += RESET;
					output += bettertostring<true, colors>(thread, -2, new_indent, encountered, depth + 1);
					if (colors)
						output += TABLE_COLOR;
					output += "] = ";
					if (colors)
						output += RESET;
				}
				output += bettertostring<true, colors>(thread, -1, new_indent, encountered, depth + 1);
				if (colors)
					output += TABLE_COLOR;
				output += ";";
				if (colors)
					output += RESET;
			}
			lua_pop(thread, 2);
		}
		if (empty || !multi_line) [[unlikely]] {
			if (colors)
				output += TABLE_COLOR;
			output += '}';
			if (colors)
				output += RESET;
			return output;
		} else {
			if (colors)
				output += TABLE_COLOR;
			output += "\n" + indent + "}";
			if (colors)
				output += RESET;
			return output;
		}
	}
	case LUA_TFUNCTION:
		lua_Debug info;
		if (lua_getinfo(thread, object_location, "sln", &info)) [[likely]] {
			std::string func_name;
			if (info.name) [[unlikely]] {
				func_name = info.name;
			} else {
				std::stringstream address;
				address << std::hex << lua_encodepointer(thread, (uintptr_t)lua_topointer(thread, object_location));
				func_name = "unnamed_" + address.str();
			}
			std::string formatted;
			if (strcmp(info.what, "Lua") == 0) {
				formatted = "function[Lua]:" + (info.linedefined >= 0 ? std::to_string(info.linedefined) : std::string("?")) + ':' + func_name;
			} else {
				formatted = "function[C]:" + func_name;
			}
			if (colors) {
				return FUNCTION_COLOR + formatted + RESET;
			} else {
				return formatted;
			}
		}
		goto _default;
	case LUA_TSTRING:
		if (quotes_around_strings) {
			size_t length;
			const char* content = luaL_tolstring(thread, object_location, &length);
			lua_pop(thread, 1);
			if (colors) {
				return STRING_COLOR + '"' + std::string(content, length) + '"' + RESET;
			} else {
				return '"' + std::string(content, length) + '"';
			}
		}
		__fallthrough;
	default:
	_default:
		{
			size_t length;
			const char* content = luaL_tolstring(thread, object_location, &length);
			lua_pop(thread, 1);
			if (colors && type == LUA_TNUMBER) {
				return NUMBER_COLOR + std::string(content, length) + RESET;
			}
			return std::string(content, length);
		}
	}
}