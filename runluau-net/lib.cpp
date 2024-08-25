#include "pch.h"
#include "websocket_client.hpp"
#include "http_client.hpp"

#define wanted_arg_count(n) \
if (n > 0 && lua_gettop(L) < n) [[unlikely]] { \
    lua_pushfstring(L, __FUNCTION__" expects at least " #n " arguments, received %d", lua_gettop(L)); \
    lua_error(L); \
    return 0; \
}


static int ws_connect(lua_State* L) {
    wanted_arg_count(1);
    const char* uri = luaL_checkstring(L, 1);
    
    WebSocketClient* client = new WebSocketClient(uri);
    lua_pushlightuserdata(L, client);
    
    client->connect_async();
    
    return lua_yield(L, 1);
}

static int ws_send(lua_State* L) {
    wanted_arg_count(2);
    WebSocketClient* client = (WebSocketClient*)lua_touserdata(L, 1);
    const char* message = luaL_checkstring(L, 2);
    
    client->send(message);
    return 0;
}

static int ws_receive(lua_State* L) {
    wanted_arg_count(1);
    WebSocketClient* client = (WebSocketClient*)lua_touserdata(L, 1);
    
    client->receive_async();
    
    return lua_yield(L, 1);
}

static int ws_close(lua_State* L) {
    wanted_arg_count(1);
    WebSocketClient* client = (WebSocketClient*)lua_touserdata(L, 1);
    
    client->close();
    delete client;
    return 0;
}


static int http_get(lua_State* L) {
    wanted_arg_count(1);
    const char* url = luaL_checkstring(L, 1);
    
    HttpClient::get_async(url);
    
    return lua_yield(L, 1);
}

static int http_post(lua_State* L) {
    wanted_arg_count(2);
    const char* url = luaL_checkstring(L, 1);
    const char* data = luaL_checkstring(L, 2);
    
    HttpClient::post_async(url, data);
    
    return lua_yield(L, 1);
}

#define reg(name) {#name, name}
constexpr luaL_Reg library[] = {
    reg(ws_connect),
    reg(ws_send),
    reg(ws_receive),
    reg(ws_close),
    reg(http_get),
    reg(http_post),
    {NULL, NULL}
};

extern "C" __declspec(dllexport) void register_library(lua_State* L) {
    luaL_register(L, "net", library);
}
