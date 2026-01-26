//
// Created by Supradeep Chitumalla on 06/09/25.
//
#ifndef CONFIG_H
#define CONFIG_H
#include <string>
#include <vector>
namespace deribit {

    struct Config {
        static constexpr const char* BASE_URL = "https://test.deribit.com/api/v2";
        static constexpr const char* WS_URL = "wss://test.deribit.com/ws/api/v2";

        std::string client_id;
        std::string client_secret;
        std::string access_token;

        struct Server {
            int websocket_port;
        } server;

        struct Trading {
            std::string default_currency;
            std::string default_instrument;
        } trading;

        // Default constructor
        Config() : server{8080}, trading{"BTC", "BTC-PERPETUAL"} {}

        // Parameterized constructor
        Config(const std::string& id, const std::string& secret, int port, const std::string& currency, const std::string& instrument)
            : client_id(id), client_secret(secret), server{port}, trading{currency, instrument} {}
    };
}
#endif