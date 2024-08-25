#pragma once
#include "pch.h"

class WebSocketClient {
public:
    WebSocketClient(const std::string& uri) : m_uri(uri) {
        m_client.init_asio();
        m_client.set_open_handler([this](websocketpp::connection_hdl) {
            m_connected = true;
            m_connection_promise.set_value();
        });
        m_client.set_message_handler([this](websocketpp::connection_hdl, websocketpp::client<websocketpp::config::asio_client>::message_ptr msg) {
            m_message_promise.set_value(msg->get_payload());
        });
    }

    void connect_async() {
        websocketpp::lib::error_code ec;
        auto conn = m_client.get_connection(m_uri, ec);
        if (ec) {
            throw std::runtime_error("Could not create connection: " + ec.message());
        }

        m_client.connect(conn);

        m_client_thread = std::thread([this]() {
            m_client.run();
        });

        m_connection_future = m_connection_promise.get_future();
    }

    bool is_connected() {
        return m_connection_future.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
    }

    void send(const std::string& message) {
        m_client.send(m_hdl, message, websocketpp::frame::opcode::text);
    }

    void receive_async() {
        m_message_promise = std::promise<std::string>();
        m_message_future = m_message_promise.get_future();
    }

    std::string get_received_message() {
        return m_message_future.get();
    }

    void close() {
        m_client.close(m_hdl, websocketpp::close::status::normal, "Closing connection");
        if (m_client_thread.joinable()) {
            m_client_thread.join();
        }
    }

private:
    websocketpp::client<websocketpp::config::asio_client> m_client;
    websocketpp::connection_hdl m_hdl;
    std::string m_uri;
    std::thread m_client_thread;
    bool m_connected = false;
    std::promise<void> m_connection_promise;
    std::future<void> m_connection_future;
    std::promise<std::string> m_message_promise;
    std::future<std::string> m_message_future;
};
