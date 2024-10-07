#include "pch.h"

window_data* to_window_data(lua_State* thread, int arg) {
	auto data_opt = get_window_data(luaL_checkbuffer(thread, 1, NULL));
	if (!data_opt.has_value()) {
		lua_pushstring(thread, "Invalid frame buffer");
		lua_error(thread);
		return 0;
	}
	return data_opt.value();
}
extern "C" __declspec(dllexport) void add_window_event(HWND hwnd, std::optional<window_event*> event) {
	if (event.has_value()) {
		expect_window_data(get_window_data(hwnd))->events->push_back(event.value());
	}
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	window_data* data = (window_data*)GetWindowLongPtr(hwnd, GWLP_USERDATA);

	if (uMsg == WM_NCCREATE) {
		CREATESTRUCT* cs = (CREATESTRUCT*)lParam;
		data = (window_data*)(cs->lpCreateParams);
		SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)data);
	}

	if (!data) {
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
	switch (uMsg) {
	case WM_CREATE:
		return 0;
	case WM_PAINT:
	{
		PAINTSTRUCT ps;
		HDC hdc = BeginPaint(data->hwnd, &ps);
		
		if (!data->hdc || !data->hglrc) {
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
			data->hdc = GetDC(hwnd);
			SetPixelFormat(data->hdc, ChoosePixelFormat(data->hdc, &pfd), &pfd);
			data->hglrc = wglCreateContext(data->hdc);
		}
		if (!wglMakeCurrent(data->hdc, data->hglrc)) {
			int err = GetLastError();
			printf("Failed to `wglMakeCurrent`: %d\n", err);
			exit(err);
		}

		if (!data->frame_buffer) {
			wglMakeCurrent(NULL, NULL);
			EndPaint(data->hwnd, &ps);
			return 0;
		}

		RECT win_rect;
		GetClientRect(data->hwnd, &win_rect);
		GLsizei win_width = win_rect.right - win_rect.left;
		GLsizei win_height = win_rect.bottom - win_rect.top;
		GLfloat scale;
		if ((float)data->width / data->height > (float)win_width / win_height) {
			scale = (GLfloat)win_width / data->width;
		} else {
			scale = (GLfloat)win_height / data->height;
		}
		GLsizei scaled_width = static_cast<GLsizei>(data->width * scale);
		GLsizei scaled_height = static_cast<GLsizei>(data->height * scale);

		glMatrixMode(GL_PROJECTION);
		glLoadIdentity();
		glOrtho(0.0, win_width, 0.0, win_height, -1.0, 1.0);

		glMatrixMode(GL_MODELVIEW);
		glLoadIdentity();

		glViewport(0, 0, win_width, win_height);

		GLint raster_x = (win_width - scaled_width) / 2;
		GLint raster_y = (win_height - scaled_height) / 2;

		glRasterPos2i(raster_x, raster_y);

		glClear(GL_COLOR_BUFFER_BIT);

		glPixelZoom(scale, scale);

		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		glDrawPixels(data->width, data->height, GL_RGBA, GL_UNSIGNED_BYTE, (void*)data->frame_buffer);

		SwapBuffers(data->hdc);

		wglMakeCurrent(NULL, NULL);
		EndPaint(data->hwnd, &ps);

		return 0;

	}
	case WM_KEYDOWN:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(wParam, true));
		return 0;
	}
	case WM_KEYUP:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(wParam, false));
		return 0;
	}
	case WM_LBUTTONDOWN:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(VK_LBUTTON, true));
		return 0;
	}
	case WM_LBUTTONUP:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(VK_LBUTTON, false));
		return 0;
	}
	case WM_RBUTTONDOWN:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(VK_RBUTTON, true));
		return 0;
	}
	case WM_RBUTTONUP:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(VK_RBUTTON, false));
		return 0;
	}
	case WM_MBUTTONDOWN:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(VK_MBUTTON, true));
		return 0;
	}
	case WM_MBUTTONUP:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		add_window_event(hwnd, new_WE_INPUT(VK_MBUTTON, false));
		return 0;
	}
	case WM_XBUTTONDOWN:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
			add_window_event(hwnd, new_WE_INPUT(VK_XBUTTON1, true));
		} else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) {
			add_window_event(hwnd, new_WE_INPUT(VK_XBUTTON2, true));
		}
		return 0;
	}
	case WM_XBUTTONUP:
	{
		std::lock_guard<std::recursive_mutex> lock(luau::luau_operation_mutex);
		if (GET_XBUTTON_WPARAM(wParam) == XBUTTON1) {
			add_window_event(hwnd, new_WE_INPUT(VK_XBUTTON1, false));
		} else if (GET_XBUTTON_WPARAM(wParam) == XBUTTON2) {
			add_window_event(hwnd, new_WE_INPUT(VK_XBUTTON2, false));
		}
		return 0;
	}
	case WM_DESTROY:
		remove_window(data);
		PostQuitMessage(0);
		return 0;
	case WM_SIZE:
		InvalidateRect(hwnd, NULL, TRUE);
		return DefWindowProc(hwnd, uMsg, wParam, lParam);

	default:
		return DefWindowProc(hwnd, uMsg, wParam, lParam);
	}
}
void create_window_thread(lua_State* thread, yield_ready_event_t yield_ready_event, void* ud) {
	int width = luaL_checkinteger(thread, 1);
	int height = luaL_checkinteger(thread, 2);
	const char* title = luaL_optstring(thread, 3, "Window");
	double pixel_size = luaL_optnumber(thread, 4, 1);

	int win_width = width * pixel_size;
	int win_height = height * pixel_size;

	WNDCLASSW wc{};
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = GetModuleHandle(NULL);
	wc.lpszClassName = L"runluauwindow";
	wc.style = CS_OWNDC;

	RegisterClassW(&wc);

	RECT win_rect = {0, 0, win_width, win_height};
	AdjustWindowRectEx(&win_rect, WS_OVERLAPPEDWINDOW, FALSE, 0);
	const wchar_t* title_w = str_to_wstr(title);
	window_data* data = add_window(thread, width, height);
	HWND hwnd = CreateWindowExW(0, wc.lpszClassName, title_w, WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, win_rect.right - win_rect.left, win_rect.bottom - win_rect.top, NULL, NULL, wc.hInstance, data);
	delete[] title_w;

	if (hwnd == NULL) {
		int err = GetLastError();
		printf("Failed to `CreateWindowExW`: %d\n", err);
		exit(err);
	}

	data->hwnd = hwnd;
	bool window_setup = false;

	const size_t buffer_size = (size_t)width * (size_t)height * sizeof(uint32_t);
	signal_yield_ready(yield_ready_event);
	luau::add_thread_to_resume_queue(thread, nullptr, 1, [thread, buffer_size, hwnd, &data, &window_setup]() {
		lua_newbuffer(thread, buffer_size);
		lua_ref(thread, -1);
		size_t pushed_buffer_size;
		void* buffer = luaL_checkbuffer(thread, -1, &pushed_buffer_size);
		if (buffer_size != pushed_buffer_size) {
			printf("Catastrophically failed to push frame buffer\n");
			exit(ERROR_INTERNAL_ERROR);
		}
		add_window_frame_buffer(data, buffer);
		window_setup = true;
	});

	while (!window_setup);

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
	lua_pushboolean(thread, get_window_data(luaL_checkbuffer(thread, 1, nullptr)).has_value());
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
			switch (event->type) {
			case WE_INPUT:
			{
				auto data = event->data.WE_INPUT;
				{
					lua_pushboolean(thread, data.down);
					lua_setfield(thread, -2, "down");
				}
				{
					lua_pushstring(thread, data.input);
					lua_setfield(thread, -2, "input");
				}
				break;
			}
			default: break;
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