#pragma once

#include <stdio.h>
#include <Windows.h>
#include <mutex>

#include "luau.h"

#define MIN_WAIT (1 / SCHEDULER_RATE)