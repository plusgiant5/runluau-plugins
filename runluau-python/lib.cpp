#include "pch.h"

static bool python_initialized = false;
static std::mutex gil_mutex;
static std::unordered_map<std::string, PyObject*> module_cache;

static PyObject* luau_to_python(lua_State* L, int index);
static int python_to_luau(lua_State* L, PyObject* obj);
static bool is_python_object(lua_State* L, int index);
static PyObject* check_python_object(lua_State* L, int index);
static int push_python_object(lua_State* L, PyObject* obj);

static int py_import(lua_State* L) {
    const char* module_name = luaL_checkstring(L, 1);

    auto it = module_cache.find(module_name);
    if (it != module_cache.end()) {
        return push_python_object(L, it->second);
    }

    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject* module = PyImport_ImportModule(module_name);

    if (!module) {
        PyObject* type = nullptr, * value = nullptr, * traceback = nullptr;
        PyErr_Fetch(&type, &value, &traceback);

        const char* error_msg = "Unknown error";
        if (value != nullptr) {
            PyObject* str_value = PyObject_Str(value);
            if (str_value != nullptr) {
                error_msg = PyUnicode_AsUTF8(str_value);
                Py_DECREF(str_value);
            }
        }

        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);
        PyGILState_Release(gstate);

        luaL_error(L, "Failed to import Python module '%s': %s", module_name, error_msg);
        return 0;
    }

    module_cache[module_name] = module;
    PyGILState_Release(gstate);
    return push_python_object(L, module);
}

static int py_eval(lua_State* L) {
    const char* code = luaL_checkstring(L, 1);

    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject* globals = PyDict_New();
    if (!globals) {
        PyGILState_Release(gstate);
        luaL_error(L, "Failed to create Python globals dictionary");
        return 0;
    }

    PyObject* locals = PyDict_New();
    if (!locals) {
        Py_DECREF(globals);
        PyGILState_Release(gstate);
        luaL_error(L, "Failed to create Python locals dictionary");
        return 0;
    }

    PyDict_SetItemString(globals, "__builtins__", PyEval_GetBuiltins());

    PyObject* result = PyRun_String(code, Py_eval_input, globals, locals);

    Py_DECREF(globals);
    Py_DECREF(locals);

    if (!result) {
        PyObject* type = nullptr, * value = nullptr, * traceback = nullptr;
        PyErr_Fetch(&type, &value, &traceback);

        const char* error_msg = "Unknown error";
        if (value != nullptr) {
            PyObject* str_value = PyObject_Str(value);
            if (str_value != nullptr) {
                error_msg = PyUnicode_AsUTF8(str_value);
                Py_DECREF(str_value);
            }
        }

        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);
        PyGILState_Release(gstate);

        luaL_error(L, "Python eval error: %s", error_msg);
        return 0;
    }

    int status = python_to_luau(L, result);
    Py_DECREF(result);
    PyGILState_Release(gstate);
    return status;
}

static int py_call(lua_State* L) {
    PyObject* callable = check_python_object(L, 1);

    if (!PyCallable_Check(callable)) {
        luaL_error(L, "Object is not callable");
        return 0;
    }

    int nargs = lua_gettop(L) - 1;
    PyObject* args = PyTuple_New(nargs);

    for (int i = 0; i < nargs; i++) {
        PyObject* arg = luau_to_python(L, i + 2);
        if (!arg) {
            Py_DECREF(args);
            luaL_error(L, "Failed to convert argument %d to Python", i + 1);
            return 0;
        }
        PyTuple_SET_ITEM(args, i, arg); // PyTuple_SET_ITEM steals reference
    }

    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject* result = PyObject_CallObject(callable, args);
    Py_DECREF(args);

    if (!result) {
        PyObject* type, * value, * traceback;
        PyErr_Fetch(&type, &value, &traceback);

        const char* error_msg = "Unknown error";
        if (value != nullptr) {
            PyObject* str_value = PyObject_Str(value);
            if (str_value != nullptr) {
                error_msg = PyUnicode_AsUTF8(str_value);
                Py_DECREF(str_value);
            }
        }

        Py_XDECREF(type);
        Py_XDECREF(value);
        Py_XDECREF(traceback);

        PyGILState_Release(gstate);
        luaL_error(L, "Python call error: %s", error_msg);
        return 0;
    }

    int status = python_to_luau(L, result);
    Py_DECREF(result);
    PyGILState_Release(gstate);
    return status;
}

static int py_get_attr(lua_State* L) {
    PyObject* obj = check_python_object(L, 1);
    const char* attr_name = luaL_checkstring(L, 2);

    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject* attr = PyObject_GetAttrString(obj, attr_name);

    if (!attr) {
        PyErr_Clear();
        lua_pushnil(L);
        PyGILState_Release(gstate);
        return 1;
    }

    int status = python_to_luau(L, attr);
    Py_DECREF(attr);
    PyGILState_Release(gstate);
    return status;
}

static int py_set_attr(lua_State* L) {
    PyObject* obj = check_python_object(L, 1);
    const char* attr_name = luaL_checkstring(L, 2);
    PyObject* value = luau_to_python(L, 3);

    if (!value) {
        luaL_error(L, "Failed to convert value to Python");
        return 0;
    }

    PyGILState_STATE gstate = PyGILState_Ensure();
    int result = PyObject_SetAttrString(obj, attr_name, value);
    Py_DECREF(value);

    if (result == -1) {
        PyObject* type, * py_value, * traceback;
        PyErr_Fetch(&type, &py_value, &traceback);

        const char* error_msg = "Unknown error";
        if (py_value != nullptr) {
            PyObject* str_value = PyObject_Str(py_value);
            if (str_value != nullptr) {
                error_msg = PyUnicode_AsUTF8(str_value);
                Py_DECREF(str_value);
            }
        }

        Py_XDECREF(type);
        Py_XDECREF(py_value);
        Py_XDECREF(traceback);

        PyGILState_Release(gstate);
        luaL_error(L, "Failed to set attribute: %s", error_msg);
        return 0;
    }

    PyGILState_Release(gstate);
    lua_pushboolean(L, 1);
    return 1;
}

static int py_to_string(lua_State* L) {
    PyObject* obj = check_python_object(L, 1);

    PyGILState_STATE gstate = PyGILState_Ensure();
    PyObject* str_obj = PyObject_Str(obj);

    if (!str_obj) {
        PyErr_Clear();
        PyGILState_Release(gstate);
        lua_pushstring(L, "[Python object]");
        return 1;
    }

    const char* str = PyUnicode_AsUTF8(str_obj);
    lua_pushstring(L, str);
    Py_DECREF(str_obj);

    PyGILState_Release(gstate);
    return 1;
}

static int py_is_callable(lua_State* L) {
    PyObject* obj = check_python_object(L, 1);

    PyGILState_STATE gstate = PyGILState_Ensure();
    lua_pushboolean(L, PyCallable_Check(obj));
    PyGILState_Release(gstate);

    return 1;
}

// Python garbage collection
static int py_gc(lua_State* L) {
    if (!is_python_object(L, 1)) {
        return 0;
    }

    PyObject** obj_ptr = (PyObject**)lua_touserdata(L, 1);
    if (obj_ptr && *obj_ptr) {
        PyGILState_STATE gstate = PyGILState_Ensure();
        Py_DECREF(*obj_ptr);
        *obj_ptr = nullptr;
        PyGILState_Release(gstate);
    }

    return 0;
}

static int py_tostring(lua_State* L) {
    return py_to_string(L);
}

static int py_index(lua_State* L) {
    if (lua_isstring(L, 2)) {
        return py_get_attr(L);
    }

    luaL_error(L, "Invalid index for Python object");
    return 0;
}

static int py_newindex(lua_State* L) {
    if (lua_isstring(L, 2)) {
        return py_set_attr(L);
    }

    luaL_error(L, "Invalid index for Python object");
    return 0;
}

// Convert Luau value to Python object
static PyObject* luau_to_python(lua_State* L, int index) {
    std::lock_guard<std::mutex> lock(gil_mutex);

    int type = lua_type(L, index);

    switch (type) {
    case LUA_TNIL:
        Py_RETURN_NONE;

    case LUA_TBOOLEAN:
        return PyBool_FromLong(lua_toboolean(L, index));

    case LUA_TNUMBER:
        return PyFloat_FromDouble(lua_tonumber(L, index));

    case LUA_TSTRING: {
        size_t len;
        const char* str = lua_tolstring(L, index, &len);
        return PyUnicode_FromStringAndSize(str, len);
    }

    case LUA_TTABLE: {
        size_t len = lua_objlen(L, index);

        bool is_array = true;
        lua_pushnil(L);
        while (lua_next(L, index < 0 ? index - 1 : index) != 0) {
            if (!lua_isnumber(L, -2) || lua_tonumber(L, -2) > len) {
                is_array = false;
            }
            lua_pop(L, 1);

            if (!is_array) break;
        }

        if (is_array) {
            PyObject* list = PyList_New(len);
            for (size_t i = 1; i <= len; i++) {
                lua_rawgeti(L, index, i);
                PyObject* item = luau_to_python(L, -1);
                PyList_SetItem(list, i - 1, item); // PyList_SetItem steals reference
                lua_pop(L, 1);
            }
            return list;
        }
        else {
            PyObject* dict = PyDict_New();
            lua_pushnil(L);
            while (lua_next(L, index < 0 ? index - 1 : index) != 0) {
                PyObject* key = luau_to_python(L, -2);
                PyObject* value = luau_to_python(L, -1);
                PyDict_SetItem(dict, key, value);
                Py_DECREF(key);
                Py_DECREF(value);
                lua_pop(L, 1);
            }
            return dict;
        }
    }

    case LUA_TUSERDATA: {
        if (is_python_object(L, index)) {
            PyObject* obj = check_python_object(L, index);
            Py_INCREF(obj);
            return obj;
        }
        PyErr_SetString(PyExc_TypeError, "Cannot convert userdata to Python");
        return nullptr;
    }

    default: {
        PyErr_Format(PyExc_TypeError, "Cannot convert Luau type %s to Python", lua_typename(L, type));
        return nullptr;
    }
    }
}

static int python_to_luau(lua_State* L, PyObject* obj) {
    std::lock_guard<std::mutex> lock(gil_mutex);

    if (obj == nullptr) {
        lua_pushnil(L);
        return 1;
    }

    if (obj == Py_None) {
        lua_pushnil(L);
        return 1;
    }

    if (PyBool_Check(obj)) {
        lua_pushboolean(L, obj == Py_True);
        return 1;
    }

    if (PyLong_Check(obj)) {
        lua_pushnumber(L, PyLong_AsDouble(obj));
        return 1;
    }

    if (PyFloat_Check(obj)) {
        lua_pushnumber(L, PyFloat_AsDouble(obj));
        return 1;
    }

    if (PyUnicode_Check(obj)) {
        Py_ssize_t size;
        const char* str = PyUnicode_AsUTF8AndSize(obj, &size);
        lua_pushlstring(L, str, size);
        return 1;
    }

    if (PyBytes_Check(obj)) {
        char* str;
        Py_ssize_t size;
        PyBytes_AsStringAndSize(obj, &str, &size);
        lua_pushlstring(L, str, size);
        return 1;
    }

    if (PyList_Check(obj) || PyTuple_Check(obj)) {
        Py_ssize_t size = PyList_Check(obj) ? PyList_Size(obj) : PyTuple_Size(obj);
        lua_createtable(L, size, 0);

        for (Py_ssize_t i = 0; i < size; i++) {
            PyObject* item = PyList_Check(obj) ? PyList_GetItem(obj, i) : PyTuple_GetItem(obj, i);
            python_to_luau(L, item);
            lua_rawseti(L, -2, i + 1); // 1-indexed
        }
        return 1;
    }

    if (PyDict_Check(obj)) {
        lua_createtable(L, 0, PyDict_Size(obj));

        PyObject* key, * value;
        Py_ssize_t pos = 0;

        while (PyDict_Next(obj, &pos, &key, &value)) {
            python_to_luau(L, key);
            python_to_luau(L, value);
            lua_settable(L, -3);
        }
        return 1;
    }

    Py_INCREF(obj);
    return push_python_object(L, obj);
}

static bool is_python_object(lua_State* L, int index) {
    if (!lua_isuserdata(L, index)) return false;

    lua_getmetatable(L, index);
    luaL_getmetatable(L, "PythonObject");
    bool result = lua_rawequal(L, -1, -2);
    lua_pop(L, 2);

    return result;
}

static PyObject* check_python_object(lua_State* L, int index) {
    if (!is_python_object(L, index)) {
        luaL_error(L, "Expected Python object at argument %d", index);
        return nullptr;
    }

    PyObject** obj = (PyObject**)lua_touserdata(L, index);
    if (!obj || !*obj) {
        luaL_error(L, "Invalid Python object at argument %d", index);
        return nullptr;
    }

    return *obj;
}

static int push_python_object(lua_State* L, PyObject* obj) {
    if (!obj) {
        lua_pushnil(L);
        return 1;
    }

    PyObject** udata = (PyObject**)lua_newuserdata(L, sizeof(PyObject*));
    *udata = obj;

    luaL_getmetatable(L, "PythonObject");
    lua_setmetatable(L, -2);

    return 1;
}

static void init_python() {
    if (!python_initialized) {
        Py_Initialize();
        python_initialized = true;
        PyEval_InitThreads();
        PyEval_SaveThread();
    }
}

static void cleanup_python() {
    if (python_initialized) {
        PyGILState_STATE gstate = PyGILState_Ensure();
        for (auto& pair : module_cache) {
            Py_DECREF(pair.second);
        }
        module_cache.clear();
        Py_Finalize();
        python_initialized = false;
    }
}

static const struct luaL_Reg python_object_mt[] = {
    {"__gc", py_gc},
    {"__tostring", py_tostring},
    {"__index", py_index},
    {"__newindex", py_newindex},
    {NULL, NULL}
};

static const struct luaL_Reg python_funcs[] = {
    {"import", py_import},
    {"eval", py_eval},
    {"call", py_call},
    {"get_attr", py_get_attr},
    {"set_attr", py_set_attr},
    {"to_string", py_to_string},
    {"is_callable", py_is_callable},
    {NULL, NULL}
};

extern "C" __declspec(dllexport) void register_library(lua_State* L) {
    init_python();
    luaL_newmetatable(L, "PythonObject");
    luaL_register(L, NULL, python_object_mt);
    lua_pop(L, 1);

    luaL_register(L, "python", python_funcs);
}

extern "C" __declspec(dllexport) const char** get_dependencies() {
    static const char* deps[] = {
        NULL
    };
    return deps;
}
