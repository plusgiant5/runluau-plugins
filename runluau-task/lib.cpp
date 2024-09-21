#include "pch.h"

void precise_sleep(double time_in_seconds) {
	HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
	if (!timer) {
		DWORD error = GetLastError();
		printf("Failed to CreateWaitableTimer: %d\n", error);
		exit(error);
	}
	LARGE_INTEGER due_time = { .QuadPart = -static_cast<LONGLONG>(time_in_seconds * 10000000.0) };
	if (!SetWaitableTimer(timer, &due_time, 0, nullptr, nullptr, FALSE)) {
		DWORD error = GetLastError();
		printf("Failed to SetWaitableTimer: %d\n", error);
		CloseHandle(timer);
		exit(error);
	}
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}

std::unordered_map<lua_State*, int> ref_for_thread;
std::mutex ref_for_thread_mutex;

void wait_thread(lua_State* thread, yield_ready_event_t yield_ready_event, void* ud) {
	double time = max(luaL_optnumber(thread, 1, MIN_WAIT), MIN_WAIT);
	signal_yield_ready(yield_ready_event);
	LARGE_INTEGER start;
	QueryPerformanceCounter(&start);
	precise_sleep(time);
	int ref;
	{
		std::lock_guard<std::mutex> lock(ref_for_thread_mutex);
		if (ref_for_thread.find(thread) == ref_for_thread.end()) {
			printf("Failed to find ref for thread in `wait`\n");
			exit(ERROR_INTERNAL_ERROR);
		}
		ref = ref_for_thread.at(thread);
	}
	luau::add_thread_to_resume_queue(thread, nullptr, 1, [ud, ref, thread, start]() {
		LARGE_INTEGER frequency, end;
		QueryPerformanceFrequency(&frequency);
		QueryPerformanceCounter(&end);
		double delta_time = static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart;
		lua_pushnumber(thread, delta_time);
		lua_unref((lua_State*)ud, ref);
	});
}
int wait(lua_State* thread) {
	lua_pushthread(thread);
	int ref = lua_ref(thread, -1);
	std::lock_guard<std::mutex> lock(ref_for_thread_mutex);
	ref_for_thread.insert({thread, ref});
	lua_pop(thread, 1);
	create_windows_thread_for_luau(thread, wait_thread, thread);
	return lua_yield(thread, 1);
}

int spawn(lua_State* thread) {
	wanted_arg_count(1);
	luaL_checktype(thread, 1, LUA_TFUNCTION);
	int arg_count = lua_gettop(thread) - 1;
	lua_State* new_thread = luau::create_thread(thread);
	lua_pushvalue(thread, 1);
	lua_xmove(thread, new_thread, 1);
	for (int i = 2; i <= arg_count + 1; i++) {
		lua_pushvalue(thread, i);
	}
	lua_xmove(thread, new_thread, arg_count);
	lua_pop(thread, arg_count + 1);
	luau::resume_and_handle_status(new_thread, nullptr, arg_count);
	lua_pushthread(new_thread);
	lua_xmove(new_thread, thread, 1);
	return 1;
}

int defer(lua_State* thread) {
	wanted_arg_count(1);
	luaL_checktype(thread, 1, LUA_TFUNCTION);
	int arg_count = lua_gettop(thread) - 1;
	lua_State* new_thread = luau::create_thread(thread);
	lua_pushvalue(thread, 1);
	lua_xmove(thread, new_thread, 1);
	for (int i = 2; i <= arg_count + 1; i++) {
		lua_pushvalue(thread, i);
	}
	lua_xmove(thread, new_thread, arg_count);
	lua_pop(thread, arg_count + 1);
	luau::add_thread_to_resume_queue(new_thread, nullptr, arg_count);
	return 1;
}

void delay_thread(lua_State* thread, yield_ready_event_t yield_ready_event, void* ud) {
	lua_State* from = (lua_State*)ud;
	double time = max(luaL_optnumber(thread, 1, MIN_WAIT), MIN_WAIT);
	signal_yield_ready(yield_ready_event);
	precise_sleep(time);
	std::lock_guard<std::mutex> lock(ref_for_thread_mutex);
	if (ref_for_thread.find(thread) == ref_for_thread.end()) {
		printf("Failed to find ref for thread in `delay`\n");
		exit(ERROR_INTERNAL_ERROR);
	}
	int ref = ref_for_thread.at(thread);
	luau::add_thread_to_resume_queue(thread, (lua_State*)ud, lua_gettop(thread) - 2, [ud, ref]{
		lua_unref((lua_State*)ud, ref);
	});
}
int delay(lua_State* thread) {
	wanted_arg_count(2);
	luaL_checktype(thread, 2, LUA_TFUNCTION);
	int arg_count = lua_gettop(thread);
	lua_State* new_thread = luau::create_thread(thread);
	for (int i = 1; i <= arg_count; i++) {
		lua_pushvalue(thread, i);
	}
	lua_xmove(thread, new_thread, arg_count);
	int ref = lua_ref(thread, -1);
	std::lock_guard<std::mutex> lock(ref_for_thread_mutex);
	ref_for_thread.insert({new_thread, ref});
	create_windows_thread_for_luau(new_thread, delay_thread, thread);
	return 1;
}

int cancel(lua_State* thread) {
	wanted_arg_count(1);
	lua_State* target = lua_tothread(thread, 1);
	luaL_argexpected(thread, target, 1, "thread");
	lua_resetthread(target);
	return 0;
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
	reg(wait),
	reg(spawn),
	reg(defer),
	reg(delay),
	reg(cancel),
	{NULL, NULL}
};
extern "C" __declspec(dllexport) void register_library(lua_State* thread) {
	luaL_register(thread, "task", library);
}