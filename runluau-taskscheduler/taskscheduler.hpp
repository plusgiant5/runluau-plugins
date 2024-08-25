#pragma once

#include "pch.h"

struct Task {
    int ref;
    std::chrono::steady_clock::time_point execution_time;
    
    Task(int r, double delay) : ref(r) {
        execution_time = std::chrono::steady_clock::now() + std::chrono::duration<double>(delay);
    }
};

class TaskScheduler {
private:
    std::vector<Task> tasks;

public:
    void schedule_task(int ref, double delay) {
        tasks.emplace_back(ref, delay);
    }

    void run_tasks(lua_State* thread) {
        auto now = std::chrono::steady_clock::now();
        
        auto it = std::remove_if(tasks.begin(), tasks.end(), [&](const Task& task) {
            if (task.execution_time <= now) {
                lua_rawgeti(thread, LUA_REGISTRYINDEX, task.ref);
                if (lua_pcall(thread, 0, 0, 0) != 0) {
                    const char* error_msg = lua_tostring(thread, -1);
                    fprintf(stderr, "Error running scheduled task: %s\n", error_msg);
                    lua_pop(thread, 1);
                }
                luaL_unref(thread, LUA_REGISTRYINDEX, task.ref);
                return true;
            }
            return false;
        });
        
        tasks.erase(it, tasks.end());
    }
};
