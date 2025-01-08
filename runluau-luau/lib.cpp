#include "pch.h"

int compile(lua_State* thread) {
	wanted_arg_count(1);
	stack_slots_needed(1);
	std::string source = luau::checkstring(thread, 1);
	Luau::CompileOptions options;
	options.optimizationLevel = 1;
	options.debugLevel = 1;
	if (lua_gettop(thread) >= 2) {
		luaL_checktype(thread, 2, LUA_TTABLE);
		if (lua_getfield(thread, 2, "optimizationLevel")) {
			unsigned int value = luaL_checkunsigned(thread, -1);
			lua_pop(thread, 1);
			if (value > 2) {
				lua_pushstring(thread, "When compiling, optimizationLevel must be an integer from 0 to 2");
				lua_error(thread);
			}
			options.optimizationLevel = value;
		}
		if (lua_getfield(thread, 2, "debugLevel")) {
			unsigned int value = luaL_checkunsigned(thread, -1);
			lua_pop(thread, 1);
			if (value > 2) {
				lua_pushstring(thread, "When compiling, debugLevel must be an integer from 0 to 2");
				lua_error(thread);
			}
			options.debugLevel = value;
		}
	}
	std::string bytecode = Luau::compile(source, options, {});
	if (bytecode.data()[0] == '\0') {
		lua_pushlstring(thread, bytecode.data() + 1, bytecode.size() - 1);
		lua_error(thread);
		return 0;
	}
	lua_pushlstring(thread, bytecode.data(), bytecode.size());
	return 1;
}

// `set` functions expect a table at top of stack
inline void set_boolean(lua_State* thread, bool value, const char* field) {
	lua_pushboolean(thread, value);
	lua_setfield(thread, -2, field);
}
inline void set_unsigned(lua_State* thread, unsigned int value, const char* field) {
	lua_pushunsigned(thread, value);
	lua_setfield(thread, -2, field);
}
inline void push_size_t(lua_State* thread, size_t value) {
	lua_pushnumber(thread, static_cast<lua_Number>(value));
}
inline void set_number(lua_State* thread, lua_Number value, const char* field) {
	lua_pushnumber(thread, value);
	lua_setfield(thread, -2, field);
}
inline void set_number(lua_State* thread, size_t value, const char* field) {
	push_size_t(thread, value);
	lua_setfield(thread, -2, field);
}
inline void set_string(lua_State* thread, std::string value, const char* field) {
	luau::pushstring(thread, value);
	lua_setfield(thread, -2, field);
}
inline std::string position_to_string(Luau::Position pos) {
	return std::to_string(pos.line) + ',' + std::to_string(pos.column);
}
inline void set_location(lua_State* thread, Luau::Location location, const char* field = "location") {
	set_string(thread, position_to_string(location.begin) + " - " + position_to_string(location.end), field);
}
void push_ast(lua_State* thread, const simdjson::dom::element& element) {
	stack_slots_needed(1);
	switch (element.type()) {
	case simdjson::dom::element_type::OBJECT:
		lua_newtable(thread);
		for (auto [key, value] : element.get_object()) {
			lua_pushlstring(thread, key.data(), key.size());
			push_ast(thread, value);
			lua_settable(thread, -3);
		}
		break;
	case simdjson::dom::element_type::ARRAY:
	{
		lua_newtable(thread);
		const auto& arr = element.get_array();
		for (size_t i = 0; i < arr.size(); ++i) {
			push_size_t(thread, i + 1);
			push_ast(thread, arr.at(i));
			lua_settable(thread, -3);
		}
		break;
	}
	case simdjson::dom::element_type::BOOL:
		lua_pushboolean(thread, element.get_bool().value_unsafe());
		break;
	case simdjson::dom::element_type::STRING:
	{
		const auto& string = element.get_string().value_unsafe();
		lua_pushlstring(thread, string.data(), string.size());
		break;
	}
	case simdjson::dom::element_type::DOUBLE:
		lua_pushnumber(thread, element.get_double().value_unsafe());
		break;
	case simdjson::dom::element_type::INT64:
	{
		const auto& string = std::to_string(element.get_int64().value_unsafe());
		lua_pushlstring(thread, string.data(), string.size());
		break;
	}
	case simdjson::dom::element_type::UINT64:
	{
		const auto& string = std::to_string(element.get_uint64().value_unsafe());
		lua_pushlstring(thread, string.data(), string.size());
		break;
	}
	case simdjson::dom::element_type::NULL_VALUE:
		lua_pushnil(thread);
		break;
	default:
		[[unlikely]]
		lua_pushfstring(thread, "<UNKNOWN TYPE %d>", element.type());
		break;
	}
}
// This is what ChatGPT is good for
std::string replace_outside_quotes(
	const std::string& input_str,
	const std::string& search_str,
	const std::string& replace_str
) {
	std::string result;
	result.reserve(input_str.size());

	bool in_quotes = false;
	int backslash_count = 0;
	size_t i = 0;

	while (i < input_str.size()) {
		char c = input_str[i];

		if (in_quotes) {
			result.push_back(c);

			if (c == '\\') {
				backslash_count++;
			} else {
				if (c == '"' && (backslash_count % 2 == 0)) {
					in_quotes = false;
				}
				backslash_count = 0;
			}
			++i;
		} else {
			if (c == '"') {
				in_quotes = true;
				result.push_back(c);
				++i;
				backslash_count = 0;
			} else {
				if (!search_str.empty()
					&& i + search_str.size() <= input_str.size()
					&& input_str.compare(i, search_str.size(), search_str) == 0) {
					result += replace_str;
					i += search_str.size();
				} else {
					result.push_back(c);
					++i;
				}
			}
		}
	}

	return result;
}
bool push_parseresult(lua_State* thread, Luau::ParseResult parsed) {
	stack_slots_needed(7);
	lua_newtable(thread);

	set_number(thread, parsed.lines, "lines");
	lua_newtable(thread);
	for (size_t i = 0; i < parsed.hotcomments.size(); ++i) {
		const auto& hotcomment = parsed.hotcomments[i];
		push_size_t(thread, i + 1);
		lua_newtable(thread);
		set_location(thread, hotcomment.location);
		set_string(thread, hotcomment.content, "content");
		set_boolean(thread, hotcomment.header, "is_header");
		lua_settable(thread, -3);
	}
	lua_setfield(thread, -2, "hotcomments");
	lua_newtable(thread);
	for (size_t i = 0; i < parsed.errors.size(); ++i) {
		const auto& error = parsed.errors[i];
		push_size_t(thread, i + 1);
		lua_newtable(thread);
		set_location(thread, error.getLocation());
		set_string(thread, error.getMessage(), "message");
		lua_settable(thread, -3);
	}
	lua_setfield(thread, -2, "errors");
	std::string ast_json = Luau::toJson(parsed.root, parsed.commentLocations);
	ast_json = replace_outside_quotes(ast_json, "-Infinity", "\"-Infinity\"");
	ast_json = replace_outside_quotes(ast_json, "Infinity", "\"Infinity\"");
	ast_json = replace_outside_quotes(ast_json, "NaN", "\"NaN\"");
	set_string(thread, ast_json, "ast_json");
	simdjson::dom::parser parser;
	simdjson::dom::element ast;
	simdjson::error_code error = parser.parse(ast_json).get(ast);
	if (error) {
		lua_pushfstring(thread, "Failed to parse Luau's JSON: %s", simdjson::error_message(error));
		lua_error(thread);
		return false;
	}
	push_ast(thread, ast);
	lua_setfield(thread, -2, "ast");
	return true;
}
int parse(lua_State* thread) {
	wanted_arg_count(1);
	stack_slots_needed(1);
	size_t len;
	const char* source = luaL_checklstring(thread, 1, &len);
	Luau::ParseOptions options;
	if (lua_gettop(thread) >= 2) {
		luaL_checktype(thread, 2, LUA_TTABLE);
		if (lua_getfield(thread, 2, "allowDeclarationSyntax")) {
			unsigned int value = luaL_checkboolean(thread, -1);
			lua_pop(thread, 1);
			options.allowDeclarationSyntax = value;
		}
		if (lua_getfield(thread, 2, "captureComments")) {
			unsigned int value = luaL_checkboolean(thread, -1);
			lua_pop(thread, 1);
			options.captureComments = value;
		}
	}
	Luau::Allocator allocator;
	Luau::AstNameTable names(allocator);
	Luau::ParseResult parsed = Luau::Parser::parse(source, len, names, allocator, options);
	if (push_parseresult(thread, parsed)) {
		return 1;
	} else {
		return 0; // errored and already called lua_error
	}
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(compile),
	reg(parse),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "luau", library);
}