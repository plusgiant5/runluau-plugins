#pragma once

#include <stdio.h>

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h> // For Unix-like systems
#endif

#include "luau.h"
