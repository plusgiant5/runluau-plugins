#pragma once

enum window_event_type {
	WE_RENDER,
};

struct window_event {
	window_event_type type;
	void* data;
};