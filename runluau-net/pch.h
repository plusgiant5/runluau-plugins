#pragma once

#include <stdio.h>
#include <Windows.h>
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <future>

#define ASIO_STANDALONE
#include <websocketpp/config/asio_client.hpp>
#include <websocketpp/client.hpp>

#include <boost/beast.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl/stream.hpp>

extern "C" {
    #include "lua.h"
    #include "lauxlib.h"
}
