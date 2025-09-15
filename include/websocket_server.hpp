#ifndef WEBSOCKET_SERVER_H
#define WEBSOCKET_SERVER_H

#include "config.hpp"
#include "market_data.hpp"  // Add this include
#include <websocketpp/server.hpp>
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>
#include <websocketpp/config/asio_client.hpp>
#include <json/json.h>  // Add this include
#include <map>
#include <set>
#include <memory>
#include <thread>

namespace deribit {
    class WebsocketServer {
    public:
        using server = websocketpp::server<websocketpp::config::asio>;
        using client = websocketpp::client<websocketpp::config::asio_tls_client>;
        using connection_hdl = websocketpp::connection_hdl;
        using message_ptr = server::message_ptr;

        // Fixed constructor signature to match implementation
        explicit WebsocketServer(Config &config, MarketData* mdm);
        ~WebsocketServer();

        void run(uint16_t port);
        void stop();

    private:
        void on_message(connection_hdl hdl, message_ptr msg);
        void on_open(connection_hdl hdl);
        void on_close(connection_hdl hdl);
        void subscribe_to_orderbook(const std::string symbol);
        void handle_orderbook_update(const std::string& symbol, const std::string& data);
        void init_deribit_connection();

        Config& config_;
        MarketData* market_manager_;  // Add this member variable
        server ws_server_;
        std::map<connection_hdl, std::set<std::string>, std::owner_less<connection_hdl>> subscriptions_;
        websocketpp::lib::shared_ptr<websocketpp::lib::thread> server_thread_;
        client deribit_client_;
        client::connection_ptr deribit_client_connection_;
        websocketpp::lib::shared_ptr<websocketpp::lib::thread> deribit_thread_;
    };
}

#endif //WEBSOCKET_SERVER_H
