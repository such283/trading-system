//
// Created by Supradeep Chitumalla on 15/09/25.
//

#include "deribit_client.hpp"
#include <websocketpp/common/thread.hpp>
#include <asio/ssl/context.hpp>

namespace deribit {

    DeribitClient::DeribitClient(Config &cfg, MarketData* mdm)
        : config_(cfg), market_manager_(mdm), is_connected_(false) {

        // Initialize WebSocket client
        ws_client_.clear_access_channels(websocketpp::log::alevel::all);
        ws_client_.clear_error_channels(websocketpp::log::elevel::all);
        ws_client_.init_asio();

        // Set up handlers
        ws_client_.set_message_handler([this](connection_hdl hdl, client::message_ptr msg){
            on_message(hdl, msg);
        });

        ws_client_.set_open_handler([this](connection_hdl hdl){
            std::lock_guard<std::mutex> lock(connection_mutex_);
            is_connected_ = true;
            connection_hdl_ = hdl;
            std::cout << "Connected to Deribit WebSocket!" << std::endl;
        });

        ws_client_.set_close_handler([this](connection_hdl){
            std::lock_guard<std::mutex> lock(connection_mutex_);
            is_connected_ = false;
            std::cout << "Disconnected from Deribit WebSocket!" << std::endl;
        });

        ws_client_.set_fail_handler([this](connection_hdl){
            std::lock_guard<std::mutex> lock(connection_mutex_);
            is_connected_ = false;
            std::cout << "Failed to connect to Deribit WebSocket!" << std::endl;
        });

        // TLS initialization for secure connection
        ws_client_.set_tls_init_handler([](connection_hdl) -> websocketpp::lib::shared_ptr<asio::ssl::context> {
            auto ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);

            try {
                ctx->set_options(asio::ssl::context::default_workarounds |
                               asio::ssl::context::no_sslv2 |
                               asio::ssl::context::no_sslv3 |
                               asio::ssl::context::single_dh_use);

                // For production, you might want to verify certificates
                ctx->set_verify_mode(asio::ssl::verify_none);

            } catch (std::exception& e) {
                std::cout << "TLS initialization error: " << e.what() << std::endl;
            }

            return ctx;
        });
    }

    DeribitClient::~DeribitClient() {
        disconnect();
    }

    void DeribitClient::connect() {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (is_connected_) {
            std::cout << "Already connected to Deribit" << std::endl;
            return;
        }

        try {
            websocketpp::lib::error_code ec;
            connection_ = ws_client_.get_connection(config_.WS_URL, ec);

            if (ec) {
                std::cout << "Error creating connection: " << ec.message() << std::endl;
                return;
            }

            ws_client_.connect(connection_);

            // Start client thread
            client_thread_ = std::thread([this]() {
                try {
                    ws_client_.run();
                } catch (std::exception& e) {
                    std::cout << "WebSocket client thread error: " << e.what() << std::endl;
                }
            });

            std::cout << "Connecting to Deribit..." << std::endl;

        } catch (std::exception& e) {
            std::cout << "Exception in connect(): " << e.what() << std::endl;
        }
    }

    void DeribitClient::disconnect() {
        {
            std::lock_guard<std::mutex> lock(connection_mutex_);
            is_connected_ = false;
        }

        try {
            ws_client_.stop();
        } catch (std::exception& e) {
            std::cout << "Error stopping WebSocket client: " << e.what() << std::endl;
        }

        if (client_thread_.joinable()) {
            client_thread_.join();
        }
    }

    void DeribitClient::subscribe(const std::string& symbol) {
        std::lock_guard<std::mutex> lock(connection_mutex_);

        if (!is_connected_) {
            std::cout << "Not connected to Deribit. Cannot subscribe to " << symbol << std::endl;
            return;
        }

        try {
            Json::Value sub;
            sub["jsonrpc"] = "2.0";
            sub["id"] = subscription_id_++;
            sub["method"] = "public/subscribe";
            sub["params"]["channels"].append("book." + symbol + ".100ms");

            Json::FastWriter writer;
            std::string message = writer.write(sub);

            websocketpp::lib::error_code ec;
            ws_client_.send(connection_hdl_, message, websocketpp::frame::opcode::text, ec);

            if (ec) {
                std::cout << "Error sending subscription: " << ec.message() << std::endl;
            } else {
                std::cout << "Subscribed to " << symbol << " 100ms book" << std::endl;
            }

        } catch (std::exception& e) {
            std::cout << "Exception in subscribe(): " << e.what() << std::endl;
        }
    }

    void DeribitClient::on_message(connection_hdl, client::message_ptr msg) {
        try {
            const std::string& payload = msg->get_payload();

            Json::Value json;
            Json::Reader reader;
            if (!reader.parse(payload, json)) {
                std::cout << "Failed to parse JSON: " << payload.substr(0, 100) << "..." << std::endl;
                return;
            }

            // Handle subscription confirmations
            if (json.isMember("result") && json.isMember("id")) {
                std::cout << "Subscription confirmed for ID: " << json["id"].asInt() << std::endl;
                return;
            }

            if (json.isMember("params") && json["params"].isMember("channel")) {
                const std::string& channel = json["params"]["channel"].asString();

                if (channel.find("book.") == 0) {
                    // Extract symbol from channel name: "book.BTC-PERPETUAL.100ms"
                    size_t first_dot = channel.find('.');
                    size_t second_dot = channel.find('.', first_dot + 1);

                    if (first_dot != std::string::npos && second_dot != std::string::npos) {
                        std::string symbol = channel.substr(first_dot + 1, second_dot - first_dot - 1);

                        if (market_manager_) {
                            market_manager_->on_orderbook_update(symbol, json);
                        }
                    }
                }
            }

            if (json.isMember("error")) {
                std::cout << "Deribit error: " << json["error"] << std::endl;
            }

        } catch (std::exception& e) {
            std::cout << "Error parsing Deribit message: " << e.what() << std::endl;
        }
    }

    bool DeribitClient::is_connected() const {
        std::lock_guard<std::mutex> lock(connection_mutex_);
        return is_connected_;
    }
}