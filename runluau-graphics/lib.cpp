#include "pch.h"

const char* window_event_type_names[] = {
	"render",
};

const wchar_t* str_to_wstr(const char* str) {
	size_t str_len = std::strlen(str) + 1;
	wchar_t* str_w = new wchar_t[str_len];
	std::mbstowcs(str_w, str, str_len);
	return str_w;
}

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
std::unordered_map<HWND, window_data*> window_hwnd_to_data;
std::unordered_map<int, window_data*> window_id_to_data;
std::unordered_map<void*, int> window_frame_buffer_to_id;
int current_window_id = 0;
std::mutex globals_mutex;
window_data* add_window(lua_State* thread, HWND hwnd, GLsizei width, GLsizei height) {
	std::lock_guard<std::mutex> lock(globals_mutex);
	current_window_id++;
	PIXELFORMATDESCRIPTOR pfd{
		.nSize = sizeof(PIXELFORMATDESCRIPTOR),
		.nVersion = 1,
		.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
		.iPixelType = PFD_TYPE_RGBA,
		.cColorBits = 32,
		.cDepthBits = 24,
		.cStencilBits = 8,
		.cAuxBuffers = 0,
		.iLayerType = PFD_MAIN_PLANE,
	};

	auto data = new window_data({
		.id = current_window_id,
		.hwnd = hwnd,
		.thread = thread,
		.events = new std::list<window_event*>,

		.hdc = GetDC(hwnd), .hglrc = nullptr,

		.width = width, .height = height, .frame_buffer = nullptr, .render_ready = false,
	});
	window_hwnd_to_data.insert({hwnd, data});
	window_id_to_data.insert({current_window_id, data});
	int pfIndex = ChoosePixelFormat(data->hdc, &pfd);
	SetPixelFormat(data->hdc, pfIndex, &pfd);
	data->hglrc = wglCreateContext(data->hdc);
	wglMakeCurrent(data->hdc, data->hglrc);
	return data;
}
void remove_window(window_data* data) {
	std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
	window_frame_buffer_to_id.erase(data->frame_buffer);
	window_hwnd_to_data.erase(data->hwnd);
	window_id_to_data.erase(data->id);
	wglMakeCurrent(NULL, NULL);
	wglDeleteContext(data->hglrc);
	delete data->events;
	delete data;
}

window_data* get_window_data(HWND hwnd) {
	if (window_hwnd_to_data.find(hwnd) == window_hwnd_to_data.end()) {
		printf("window data not found\n");
		exit(ERROR_INTERNAL_ERROR);
	}
	return window_hwnd_to_data.at(hwnd);
}
window_data* to_window_data(lua_State* thread, int arg) {
	void* buffer = luaL_checkbuffer(thread, 1, NULL);
	if (window_frame_buffer_to_id.find(buffer) == window_frame_buffer_to_id.end()) {
		lua_pushstring(thread, "Invalid frame buffer");
		lua_error(thread);
		return 0;
	}
	unsigned int id = window_frame_buffer_to_id.at(buffer);
	if (window_id_to_data.find(id) == window_id_to_data.end(id)) {
		lua_pushfstring(thread, "Invalid window id %d", id);
		lua_error(thread);
		return 0;
	}
	return window_id_to_data.at(id);
}
extern "C" __declspec(dllexport) void add_window_event(HWND hwnd, window_event* event) {
	get_window_data(hwnd)->events->push_back(event);
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (window_hwnd_to_data.find(hwnd) == window_hwnd_to_data.end()) {
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	window_data* data = window_hwnd_to_data.at(hwnd);
	switch (uMsg) {
	case WM_CREATE:
		return 0;
	case WM_PAINT:
	{
		static bool already_painting = false;
		if (already_painting)
			return 0;
		already_painting = true;

		const auto& data = get_window_data(hwnd);
		do {} while (!data->render_ready);
		data->render_ready = false;

		RECT win_rect;
		GetClientRect(data->hwnd, &win_rect);
		GLsizei win_width = win_rect.right - win_rect.left;
		GLsizei win_height = win_rect.bottom - win_rect.top;
		GLfloat scale;
		if (data->width / data->height > win_width / win_height) {
			scale = (GLfloat)win_width / data->width;
		} else {
			scale = (GLfloat)win_height / data->height;
		}
		GLsizei scaled_width = static_cast<GLsizei>(data->width * scale);
		GLsizei scaled_height = static_cast<GLsizei>(data->height * scale);
		glViewport((win_width - scaled_width) / 2, (win_height - scaled_height) / 2, scaled_width, scaled_height);
		glPixelZoom(scale, -scale);

		glClear(GL_COLOR_BUFFER_BIT);
		glRasterPos2f(-1.0f, 1.0f);

		glDrawPixels(data->width, data->height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)data->frame_buffer);
		SwapBuffers(data->hdc);

		already_painting = false;
		return 0;
	}
	case WM_DESTROY:
		remove_window(data);
		PostQuitMessage(0);
		return 0;

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}
void create_window_thread(lua_State* thread, yield_ready_event_t yield_ready_event, void* ud) {
	int width = luaL_checkinteger(thread, 1);
	int height = luaL_checkinteger(thread, 2);
	const char* title = luaL_optstring(thread, 3, "Window");

	WNDCLASSW wc{};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"runluauwindow";
	wc.style = CS_OWNDC;

	RegisterClassW(&wc);

	RECT win_rect = {0, 0, width, height};
	AdjustWindowRectEx(&win_rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
	const wchar_t* title_w = str_to_wstr(title);
	HWND hwnd = CreateWindowExW(0, wc.lpszClassName, title_w, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, win_rect.right - win_rect.left, win_rect.bottom - win_rect.top, NULL, NULL, wc.hInstance, nullptr);
	delete[] title_w;

	if (hwnd == NULL) {
		return;
	}

	window_data* data = add_window(thread, hwnd, width, height);

	const size_t buffer_size = (size_t)width * (size_t)height * sizeof(uint32_t);
	signal_yield_ready(yield_ready_event);
	luau::add_thread_to_resume_queue(thread, nullptr, 1, [thread, buffer_size, data]() {
		std::lock_guard<std::mutex> lock(globals_mutex);
		lua_newbuffer(thread, buffer_size);
		lua_ref(thread, -1);
		size_t pushed_buffer_size;
		void* buffer = luaL_checkbuffer(thread, -1, &pushed_buffer_size);
		if (buffer_size != pushed_buffer_size) {
			printf("Catastrophically failed to push frame buffer\n");
			exit(ERROR_INTERNAL_ERROR);
		}
		data->frame_buffer = (uint32_t*)buffer;
		window_frame_buffer_to_id.insert({buffer, data->id});
	});

	ShowWindow(hwnd, SW_SHOW);
	UpdateWindow(hwnd);

	MSG msg{};
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}
int create_window(lua_State* thread) {
	wanted_arg_count(2);
	create_windows_thread_for_luau(thread, create_window_thread);
	return lua_yield(thread, 1);
}

int window_exists(lua_State* thread) {
	wanted_arg_count(1);
	lua_pushboolean(thread, window_frame_buffer_to_id.find(luaL_checkbuffer(thread, 1, nullptr)) != window_frame_buffer_to_id.end() ? true : false);
	return 1;
}

int get_window_events(lua_State* thread) {
	wanted_arg_count(1);
	window_data* data = to_window_data(thread, 1);
	auto& events = data->events;
	lua_createtable(thread, (int)events->size(), 0);
	if (events->size() > 0) {
		int current = 0;
		for (const auto& event : *events) {
			lua_pushinteger(thread, ++current);
			lua_newtable(thread);
			{
				lua_pushstring(thread, window_event_type_names[event->type]);
				lua_setfield(thread, -2, "event_type");
			}
			lua_settable(thread, -3);
		}
		events->clear();
	}
	return 1;
}

int render(lua_State* thread) {
	wanted_arg_count(1);
	window_data* data = to_window_data(thread, 1);
	data->render_ready = true;
	return 0;
}

void SetWindowSize(HWND hwnd, int width, int height) {
	DWORD style = GetWindowLong(hwnd, GWL_STYLE);
	DWORD style_ex = GetWindowLong(hwnd, GWL_EXSTYLE);
	RECT win_rect;
	GetWindowRect(hwnd, &win_rect);

	RECT client_rect = {0, 0, width, height};
	AdjustWindowRectEx(&client_rect, style, FALSE, style_ex);
	SetWindowPos(hwnd, HWND_TOP, win_rect.left, win_rect.top, client_rect.right - client_rect.left, client_rect.bottom - client_rect.top, SWP_NOZORDER);
}
int set_window_size(lua_State* thread) {
	wanted_arg_count(3);
	window_data* data = to_window_data(thread, 1);
	int width = luaL_checkinteger(thread, 2);
	int height = luaL_checkinteger(thread, 3);
	WINDOWINFO existing_info{};
	if (!GetWindowInfo(data->hwnd, &existing_info)) {
		lua_pushfstring(thread, "WinAPI error on GetWindowInfo `%d`", GetLastError());
		lua_error(thread);
		return 0;
	}
	RECT existing_win_rect = existing_info.rcWindow;
	std::thread([hwnd = data->hwnd, width, height] {
		SetWindowSize(hwnd, width, height);
		}).detach();
	return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(create_window),
	reg(window_exists),
	reg(render),
	reg(get_window_events),
	reg(set_window_size),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "gfx", library);
}

extern "C" __declspec(dllexport) HWND get_window_hwnd_from_id(int id) {
	return NULL;
}