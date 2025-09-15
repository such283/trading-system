//
// Created by Supradeep Chitumalla on 13/09/25.
//

#ifndef DERIBIT_CLIENT_H
#define DERIBIT_CLIENT_H

#include "config.hpp"
#include "market_data.hpp"
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <json/json.h>
#include <thread>
#include <mutex>
#include <atomic>

namespace deribit {

    class DeribitClient {
    public:
        using client = websocketpp::client<websocketpp::config::asio_tls_client>;
        using connection_hdl = websocketpp::connection_hdl;
        using message_ptr = client::message_ptr;

        explicit DeribitClient(Config &config, MarketData* market_manager);
        ~DeribitClient();

        void connect();
        void disconnect();
        void subscribe(const std::string& symbol);
        bool is_connected() const;

    private:
        void on_message(connection_hdl hdl, message_ptr msg);

        Config& config_;
        MarketData* market_manager_;
        client ws_client_;
        client::connection_ptr connection_;
        connection_hdl connection_hdl_;
        std::thread client_thread_;

        mutable std::mutex connection_mutex_;
        std::atomic<bool> is_connected_;
        std::atomic<int> subscription_id_{1};
    };
}

#endif //DERIBIT_CLIENT_H