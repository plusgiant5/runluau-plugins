#include "pch.h"

#include "window.h"

const char* window_event_type_names[] = {
	"input",
	"scroll",
};

std::unordered_map<int, window_data*> window_id_to_data;
std::unordered_map<void*, window_data*> window_frame_buffer_to_data;
int current_window_id = 0;
std::mutex globals_mutex;
window_data* add_window(lua_State* thread, GLsizei width, GLsizei height) {
	std::lock_guard<std::mutex> lock(globals_mutex);
	current_window_id++;

	auto data = new window_data({
		.id = current_window_id,
		.thread = thread,
		.events = new std::list<window_event*>,

		.hdc = nullptr, .hglrc = nullptr,

		.width = width, .height = height, .frame_buffer = nullptr, .render_ready = false,

		.last_cursor_position = POINT(0, 0),
	});

	window_id_to_data.insert({current_window_id, data});
	return data;
}
void remove_window(window_data* data) {
	std::lock_guard<std::mutex> lock(globals_mutex);
	window_id_to_data.erase(data->id);
	window_frame_buffer_to_data.erase(data->frame_buffer);
	wglDeleteContext(data->hglrc);
	delete data->events;
	delete data;
}
void add_window_frame_buffer(window_data* data, void* frame_buffer) {
	std::lock_guard<std::mutex> lock(globals_mutex);
	data->frame_buffer = (uint32_t*)frame_buffer;
	window_frame_buffer_to_data.insert({frame_buffer, data});
}

std::optional<window_data*> get_window_data(HWND hwnd) {
	window_data* data = (window_data*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
	if (!data) {
		return std::nullopt;
	}
	return data;
}
std::optional<window_data*> get_window_data(int id) {
	if (window_id_to_data.find(id) == window_id_to_data.end()) {
		return std::nullopt;
	}
	return window_id_to_data.at(id);
}
std::optional<window_data*> get_window_data(void* frame_buffer) {
	if (window_frame_buffer_to_data.find(frame_buffer) == window_frame_buffer_to_data.end()) {
		return std::nullopt;
	}
	return window_frame_buffer_to_data.at(frame_buffer);
}
window_data* expect_window_data(std::optional<window_data*> data) {
	if (!data.has_value()) [[unlikely]] {
		throw "Window data not found!";
	}
	return data.value();
}

std::unordered_map<WPARAM, const char*> input_names = {
#define a(c) {*#c, #c}
	a(0), a(1), a(2), a(3), a(4), a(5), a(6), a(7), a(8), a(9),
	a(A), a(B), a(C), a(D), a(E), a(F), a(G), a(H), a(I), a(J), a(K), a(L), a(M), a(N), a(O), a(P), a(Q), a(R), a(S), a(T), a(U), a(V), a(W), a(X), a(Y), a(Z),
#undef a

	{VK_F1, "F1"},
	{VK_F2, "F2"},
	{VK_F3, "F3"},
	{VK_F4, "F4"},
	{VK_F5, "F5"},
	{VK_F6, "F6"},
	{VK_F7, "F7"},
	{VK_F8, "F8"},
	{VK_F9, "F9"},
	{VK_F10, "F10"},
	{VK_F11, "F11"},
	{VK_F12, "F12"},

	{VK_NUMPAD0, "NumPad0"},
	{VK_NUMPAD1, "NumPad1"},
	{VK_NUMPAD2, "NumPad2"},
	{VK_NUMPAD3, "NumPad3"},
	{VK_NUMPAD4, "NumPad4"},
	{VK_NUMPAD5, "NumPad5"},
	{VK_NUMPAD6, "NumPad6"},
	{VK_NUMPAD7, "NumPad7"},
	{VK_NUMPAD8, "NumPad8"},
	{VK_NUMPAD9, "NumPad9"},
	{VK_MULTIPLY, "NumPadMultiply"},
	{VK_ADD, "NumPadAdd"},
	{VK_SEPARATOR, "NumPadSeparator"},
	{VK_SUBTRACT, "NumPadSubtract"},
	{VK_DECIMAL, "NumPadDecimal"},
	{VK_DIVIDE, "NumPadDivide"},

	{VK_SHIFT, "Shift"},
	{VK_LSHIFT, "LeftShift"},
	{VK_RSHIFT, "RightShift"},
	{VK_CONTROL, "Control"},
	{VK_LCONTROL, "LeftControl"},
	{VK_RCONTROL, "RightControl"},
	{VK_MENU, "Alt"},
	{VK_LMENU, "LeftAlt"},
	{VK_RMENU, "RightAlt"},

	{VK_LEFT, "LeftArrow"},
	{VK_UP, "UpArrow"},
	{VK_RIGHT, "RightArrow"},
	{VK_DOWN, "DownArrow"},
	{VK_HOME, "Home"},
	{VK_END, "End"},
	{VK_PRIOR, "PageUp"},
	{VK_NEXT, "PageDown"},
	{VK_INSERT, "Insert"},
	{VK_DELETE, "Delete"},

	{VK_ESCAPE, "Escape"},
	{VK_SPACE, "Space"},
	{VK_TAB, "Tab"},
	{VK_CAPITAL, "CapsLock"},
	{VK_NUMLOCK, "NumLock"},
	{VK_SCROLL, "ScrollLock"},
	{VK_PAUSE, "Pause"},
	{VK_SNAPSHOT, "PrintScreen"},
	{VK_LWIN, "LeftWindows"},
	{VK_RWIN, "RightWindows"},
	{VK_APPS, "Applications"},
	{VK_SLEEP, "Sleep"},

	{VK_VOLUME_MUTE, "VolumeMute"},
	{VK_VOLUME_DOWN, "VolumeDown"},
	{VK_VOLUME_UP, "VolumeUp"},
	{VK_MEDIA_NEXT_TRACK, "MediaNextTrack"},
	{VK_MEDIA_PREV_TRACK, "MediaPrevTrack"},
	{VK_MEDIA_STOP, "MediaStop"},
	{VK_MEDIA_PLAY_PAUSE, "MediaPlayPause"},

	{VK_BACK, "Backspace"},
	{VK_RETURN, "Enter"},
	{VK_CLEAR, "Clear"},
	{VK_SELECT, "Select"},
	{VK_PRINT, "Print"},
	{VK_EXECUTE, "Execute"},
	{VK_HELP, "Help"},
	{VK_OEM_1, "Semicolon"},
	{VK_OEM_PLUS, "Plus"},
	{VK_OEM_COMMA, "Comma"},
	{VK_OEM_MINUS, "Minus"},
	{VK_OEM_PERIOD, "Period"},
	{VK_OEM_2, "Slash"},
	{VK_OEM_3, "Tilde"},
	{VK_OEM_4, "OpenBracket"},
	{VK_OEM_5, "Backslash"},
	{VK_OEM_6, "CloseBracket"},
	{VK_OEM_7, "Quote"},

	{VK_LBUTTON, "LeftMouseButton"},
	{VK_RBUTTON, "RightMouseButton"},
	{VK_MBUTTON, "MiddleMouseButton"},
	{VK_XBUTTON1, "XButton1"},
	{VK_XBUTTON2, "XButton2"}
};
std::optional<window_event*> new_WE_INPUT(WPARAM input, bool down) {
	if (input_names.find(input) == input_names.end()) {
		return std::nullopt;
	}
	window_event* event = new window_event{WE_INPUT};
	event->data.WE_INPUT.down = down;
	event->data.WE_INPUT.input = input_names.at(input);
	return event;
}
std::optional<window_event*> new_WE_SCROLL(int delta) {
	window_event* event = new window_event{WE_SCROLL};
	event->data.WE_SCROLL.delta = delta;
	return event;
}