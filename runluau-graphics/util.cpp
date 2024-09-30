#include "pch.h"

#include "util.h"

const wchar_t* str_to_wstr(const char* str) {
	size_t str_len = std::strlen(str) + 1;
	wchar_t* str_w = new wchar_t[str_len];
	std::mbstowcs(str_w, str, str_len);
	return str_w;
}