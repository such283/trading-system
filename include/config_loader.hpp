//
// Created by Supradeep Chitumalla
//

#ifndef CONFIG_LOADER_H
#define CONFIG_LOADER_H

#include "config.hpp"
#include <string>
#include <fstream>
#include <stdexcept>
#include <json/json.h>

namespace deribit {

    class ConfigLoader {
    public:
        static Config load_from_file(const std::string& filepath) {
            std::ifstream config_file(filepath);
            if (!config_file.is_open()) {
                throw std::runtime_error("Failed to open config file: " + filepath);
            }

            Json::Value root;
            Json::CharReaderBuilder reader_builder;
            std::string errs;

            if (!Json::parseFromStream(reader_builder, config_file, &root, &errs)) {
                throw std::runtime_error("Failed to parse config file: " + errs);
            }

            // Extract credentials
            std::string client_id = root["client_id"].asString();
            std::string client_secret = root["client_secret"].asString();

            // Validate credentials
            if (client_id.empty() || client_secret.empty() ||
                client_id == "YOUR_CLIENT_ID_HERE" ||
                client_secret == "YOUR_CLIENT_SECRET_HERE") {
                throw std::runtime_error(
                    "Invalid credentials in config file. Please update config.json with your actual Deribit API keys."
                );
                }

            // Create config with credentials and hardcoded defaults
            Config config(
                client_id,
                client_secret,
                8080,              // websocket_port
                "BTC",             // default_currency
                "BTC-PERPETUAL"    // default_instrument
            );

            return config;
        }
    };

} // namespace deribit

#endif // CONFIG_LOADER_H