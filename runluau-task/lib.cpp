#include "pch.h"

void precise_sleep(double time_in_seconds) {
	HANDLE timer = CreateWaitableTimer(NULL, TRUE, NULL);
	if (!timer) {
		DWORD error = GetLastError();
		printf("Failed to CreateWaitableTimer: %d\n", error);
		exit(error);
	}
	LARGE_INTEGER due_time;
	due_time.QuadPart = -static_cast<LONGLONG>(time_in_seconds * 10000000.0);
	if (!SetWaitableTimer(timer, &due_time, 0, nullptr, nullptr, FALSE)) {
		DWORD error = GetLastError();
		printf("Failed to SetWaitableTimer: %d\n", error);
		CloseHandle(timer);
		exit(error);
	}
	WaitForSingleObject(timer, INFINITE);
	CloseHandle(timer);
}

void wait_thread(lua_State* thread, HANDLE yield_ready_event, void* ud) {
	double time = max(luaL_optnumber(thread, 1, 1/1000), 1/1000);
	signal_yield_ready(yield_ready_event);
	LARGE_INTEGER frequency, start, end;
	QueryPerformanceFrequency(&frequency);
	QueryPerformanceCounter(&start);
	precise_sleep(time);
	QueryPerformanceCounter(&end);
	lua_pushnumber(thread, static_cast<double>(end.QuadPart - start.QuadPart) / frequency.QuadPart);
	luau::add_thread_to_resume_queue(thread, nullptr, 1);
}
int wait(lua_State* thread) {
	create_windows_thread_for_luau(thread, wait_thread);
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
	luau::resume_and_handle_status(new_thread, thread, arg_count);
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
	luau::add_thread_to_resume_queue(new_thread, thread, arg_count);
	lua_pushthread(new_thread);
	lua_xmove(new_thread, thread, 1);
	return 1;
}

void delay_thread(lua_State* thread, HANDLE yield_ready_event, void* ud) {
	lua_State* from = (lua_State*)ud;
	double time = max(luaL_optnumber(thread, 1, 1/1000), 1/1000);
	signal_yield_ready(yield_ready_event);
	precise_sleep(time);
	luau::add_thread_to_resume_queue(thread, from, lua_gettop(thread) - 2);
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
	create_windows_thread_for_luau(new_thread, delay_thread, thread);
	printf("T:%d\n", lua_gettop(new_thread));
	lua_pushthread(new_thread);
	printf("T:%d\n", lua_gettop(new_thread));
	lua_xmove(new_thread, thread, 1);
	printf("T:%d\n", lua_gettop(new_thread));
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