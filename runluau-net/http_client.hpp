#pragma once
#include "pch.h"

namespace HttpClient {
    std::future<std::string> get_future;
    std::future<std::string> post_future;

    void get_async(const std::string& url) {
        get_future = std::async(std::launch::async, [url]() {
            namespace beast = boost::beast;
            namespace http = beast::http;
            namespace net = boost::asio;
            using tcp = net::ip::tcp;

            net::io_context ioc;
            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);

            auto const results = resolver.resolve(url, "80");
            stream.connect(results);

            http::request<http::string_body> req{http::verb::get, "/", 11};
            req.set(http::field::host, url);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);

            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(stream, buffer, res);

            std::string response = beast::buffers_to_string(res.body().data());

            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            if (ec && ec != beast::errc::not_connected)
                throw beast::system_error{ec};

            return response;
        });
    }

    std::string get_response() {
        return get_future.get();
    }

    void post_async(const std::string& url, const std::string& data) {
        post_future = std::async(std::launch::async, [url, data]() {
            namespace beast = boost::beast;
            namespace http = beast::http;
            namespace net = boost::asio;
            using tcp = net::ip::tcp;

            net::io_context ioc;
            tcp::resolver resolver(ioc);
            beast::tcp_stream stream(ioc);

            auto const results = resolver.resolve(url, "80");
            stream.connect(results);

            http::request<http::string_body> req{http::verb::post, "/", 11};
            req.set(http::field::host, url);
            req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
            req.set(http::field::content_type, "application/x-www-form-urlencoded");
            req.body() = data;
            req.prepare_payload();

            http::write(stream, req);

            beast::flat_buffer buffer;
            http::response<http::dynamic_body> res;
            http::read(stream, buffer, res);

            std::string response = beast::buffers_to_string(res.body().data());

            beast::error_code ec;
            stream.socket().shutdown(tcp::socket::shutdown_both, ec);

            if (ec && ec != beast::errc::not_connected)
                throw beast::system_error{ec};

            return response;
        });
    }

    std::string post_response() {
        return post_future.get();
    }
}
