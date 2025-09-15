//
// Created by Supradeep Chitumalla on 13/09/25.
//
#include "websocket_server.hpp"
#include <iostream>
#include <cpprest/json.h>

#include "market_data.hpp"

namespace deribit {

    WebsocketServer::WebsocketServer(Config &config, MarketData* mdm)
        : config_(config), market_manager_(mdm) {
        ws_server_.init_asio();

        ws_server_.set_open_handler(std::bind(&WebsocketServer::on_open, this, std::placeholders::_1));
        ws_server_.set_close_handler(std::bind(&WebsocketServer::on_close, this, std::placeholders::_1));
        ws_server_.set_message_handler(std::bind(&WebsocketServer::on_message, this, std::placeholders::_1, std::placeholders::_2));

        // Register callback from MarketManager
        market_manager_->register_orderbook_callback(
            [this](const std::string& symbol, const Orderbook& ob){
                Json::Value payload;
                payload["symbol"] = symbol;
                payload["best_bid"] = ob.best_bid_price;
                payload["best_ask"] = ob.best_ask_price;
                std::string msg = Json::FastWriter().write(payload);
                handle_orderbook_update(symbol, msg);
            }
        );
    }

    WebsocketServer::~WebsocketServer() {
        if (server_thread_ && server_thread_->joinable()) server_thread_->join();
    }

    void WebsocketServer::run(uint16_t port) {
        ws_server_.listen(port);
        ws_server_.start_accept();
        server_thread_ = std::make_shared<std::thread>([this](){ ws_server_.run(); });
    }

    void WebsocketServer::handle_orderbook_update(const std::string& symbol, const std::string& data) {
        for (auto& s : subscriptions_) {
            if (s.second.find(symbol) != s.second.end()) {
                ws_server_.send(s.first, data, websocketpp::frame::opcode::text);
            }
        }
    }

    void WebsocketServer::on_open(connection_hdl hdl) { subscriptions_[hdl] = {}; }
    void WebsocketServer::on_close(connection_hdl hdl) { subscriptions_.erase(hdl); }

    void WebsocketServer::on_message(connection_hdl hdl, server::message_ptr msg) {
        Json::Value json;
        Json::Reader reader;
        if (!reader.parse(msg->get_payload(), json)) return;

        std::string op = json["operation"].asString();
        std::string sym = json["symbol"].asString();

        if (op == "subscribe") subscriptions_[hdl].insert(sym);
        else if (op == "unsubscribe") subscriptions_[hdl].erase(sym);
    }

}
