#pragma once

#include <list>

#include <Windows.h>
#include <gl/GL.h>

#include "luau.h"

enum window_event_type {
	WE_INPUT,
};
extern const char* window_event_type_names[];

struct window_event {
	window_event_type type;
	union {
		struct {
			const char* input;
			bool down;
		} WE_INPUT;
	} data;
};

struct window_data {
	int id;
	HWND hwnd;
	lua_State* thread;
	std::list<window_event*>* events;

	HDC hdc;
	HGLRC hglrc;

	GLsizei width;
	GLsizei height;
	uint32_t* frame_buffer;
	bool render_ready;
};

window_data* add_window(lua_State* thread, GLsizei width, GLsizei height);
void remove_window(window_data* data);
void add_window_frame_buffer(window_data* data, void* frame_buffer);

std::optional<window_data*> get_window_data(HWND hwnd);
std::optional<window_data*> get_window_data(int id);
std::optional<window_data*> get_window_data(void* frame_buffer);
window_data* expect_window_data(std::optional<window_data*> data);

std::optional<window_event*> new_WE_INPUT(WPARAM input, bool down);