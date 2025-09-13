//
// Created by Supradeep Chitumalla on 13/09/25.
//
#include "websocket_server.hpp"
#include <json/json.h>
#include <iostream>
#include <boost/beast/websocket.hpp>
#include <boost/asio/io_context.hpp>


namespace deribit {
    WebsocketServer::WebsocketServer(Config &config) : config_(config) {
        ws_server_.clear_access_channels(websocketpp::log::alevel::all);
        ws_server_.set_access_channels(websocketpp::log::alevel::connect);
        ws_server_.set_access_channels(websocketpp::log::alevel::disconnect);
        ws_server_.set_access_channels(websocketpp::log::alevel::app);
        ws_server_.init_asio();
        ws_server_.set_message_handler(
        std::bind(&WebsocketServer::on_message,this,std::placeholders::_1,std::placeholders::_2)
            );
        ws_server_.set_open_handler(
            std::bind(&WebsocketServer::on_open,this,std::placeholders::_1)
            );
        ws_server_.set_close_handler(
            std::bind(&WebsocketServer::on_close,this,std::placeholders::_1)
            );
        init_deribit_connection();
    }
    WebsocketServer::~WebsocketServer() {
        stop();
    }

    void WebsocketServer::stop() {
        try {
            std::cout<<"stopping web socket server"<< std::endl;
            ws_server_.stop();
            if (server_thread_ && server_thread_->joinable()) {
                server_thread_->join();
            }
            std::cout << "stopping deribit client" << std::endl;
            deribit_client_.stop();
            if (deribit_thread_ && deribit_thread_->joinable()) {
                deribit_thread_->join();
            }
            std::cout << "stopped websocket server and deribit client." << std::endl;

        } catch (std::exception e) {

        }
    }

    void WebsocketServer::on_message(connection_hdl hdl, message_ptr msg) {
        try {
            std::cout<<"recieved client messgae"<< std::endl;
            Json::Value json;
            Json::Reader reader;
            reader.parse(msg->get_payload(),json);
            std::string operation = json["operation"].asString();
            std::string symbol = json["symbol"].asString();
            if (operation=="subscribe") {
                std::cout<<"subscribing to "<<symbol<< std::endl;
                subscriptions_[hdl].insert(symbol);
                subscribe_to_orderbook(symbol);
            }
        } catch (std::exception e) {
            std::cout<<"unable to process message"<< std::endl;
        }
    }

    void WebsocketServer::on_open(connection_hdl hdl) {
        try {
            std::cout<<"new client connection"<< std::endl;
            subscriptions_[hdl] = std::set<std::string>();
        } catch (std::exception e) {
            std::cout<<"unable to create new client connection"<< std::endl;
        }
    }

    void WebsocketServer::on_close(connection_hdl hdl) {
        try {
            std::cout << "Client disconnected" << std::endl;
            subscriptions_.erase(hdl);
        } catch (std::exception e) {
            std::cout<<"client connection closed"<< std::endl;
        }
    }

    void WebsocketServer::subscribe_to_orderbook(const std::string symbol) {
        try {
            if (!deribit_client_connection_) {
                std::cout<<"deribit client has not started yet.please try again"<< std::endl;
                return;
            }
            Json::Value subscription;
            subscription["jsonrpc"] = 2.0;
            subscription["id"] = 27;
            subscription["method"] = "public/subscribe";
            subscription["params"]["channels"]=Json::arrayValue;
            subscription["params"]["channels"].append("book."+symbol+".100ms");
            std::string message = Json::FastWriter().write(subscription);
            deribit_client_.send(deribit_client_connection_,message,websocketpp::frame::opcode::text);
            std::cout<<"subscribed to order book of "<<symbol<< std::endl;
        } catch (std::exception e) {
            std::cout<<"unable to subscribe to the order book"<< std::endl;
        }
    }
    void WebsocketServer::handle_orderbook_update(const std::string &symbol, const std::string &data) {

        for (const auto& subscription : subscriptions_) {
            if (subscription.second.find(symbol)!=subscription.second.end()) {
                try {
                    ws_server_.send(subscription.first,data,websocketpp::frame::opcode::text);
                }
                catch (std::exception e) {
                    std::cerr<<"error sending the update to client"<< std::endl;
                }
            }
        }
    }

    void WebsocketServer::init_deribit_connection() {
    try {
        deribit_client_.clear_access_channels(websocketpp::log::alevel::all);
        deribit_client_.set_access_channels(websocketpp::log::alevel::connect);
        deribit_client_.set_access_channels(websocketpp::log::alevel::disconnect);
        deribit_client_.set_access_channels(websocketpp::log::alevel::app);

        deribit_client_.init_asio();
        deribit_client_.set_message_handler([this](websocketpp::connection_hdl hdl, client::message_ptr msg) {
            std::cout << "📨 Received from Deribit: " << msg->get_payload() << std::endl;

            try {
                Json::Value json;
                Json::Reader reader;
                if (reader.parse(msg->get_payload(), json)) {
                    // Handle subscription data
                    if (json.isMember("params") && json["params"].isMember("channel")) {
                        std::string channel = json["params"]["channel"].asString();
                        if (channel.find("book.") == 0) {
                            // Extract symbol from channel (e.g., "book.BTC-PERPETUAL.100ms")
                            size_t start = channel.find('.') + 1;
                            size_t end = channel.find('.', start);
                            std::string symbol = channel.substr(start, end - start);
                            handle_orderbook_update(symbol, msg->get_payload());
                        }
                    }
                }
            } catch (const std::exception& e) {
                std::cout << "Error parsing Deribit message: " << e.what() << std::endl;
            }
        });

        // Add connection open handler
        deribit_client_.set_open_handler([this](websocketpp::connection_hdl hdl) {
            std::cout << "Successfully connected to Deribit WebSocket!" << std::endl;
        });

        // Add connection close handler
        deribit_client_.set_close_handler([this](websocketpp::connection_hdl hdl) {
            std::cout << " Deribit connection closed" << std::endl;
        });

        // Add error handler
        deribit_client_.set_fail_handler([this](websocketpp::connection_hdl hdl) {
            std::cout << " Deribit connection failed" << std::endl;
        });

        // Proper TLS/SSL setup
        deribit_client_.set_tls_init_handler([](websocketpp::connection_hdl) {
            auto ctx = websocketpp::lib::make_shared<asio::ssl::context>(asio::ssl::context::tlsv12_client);
            ctx->set_options(
                asio::ssl::context::default_workarounds |
                asio::ssl::context::no_sslv2 |
                asio::ssl::context::no_sslv3 |
                asio::ssl::context::single_dh_use
            );
            return ctx;
        });

        // Create connection
        websocketpp::lib::error_code ec;
        deribit_client_connection_ = deribit_client_.get_connection(config_.WS_URL, ec);

        if (ec) {
            std::cout << " Cannot create Deribit connection: " << ec.message() << std::endl;
            return;
        }

        deribit_client_.connect(deribit_client_connection_);

        // Start the client thread
        deribit_thread_ = websocketpp::lib::make_shared<websocketpp::lib::thread>([this]() {
            try {
                std::cout << " Starting Deribit client thread" << std::endl;
                deribit_client_.run();
            } catch (const std::exception& e) {
                std::cout << " Deribit WebSocket error " << e.what() << std::endl;
            }
        });

        std::cout << " Deribit connection initialized" << std::endl;

        } catch (const std::exception& e) {
            std::cout << " Error initializing Deribit connection " << e.what() << std::endl;
        }
    }

    void WebsocketServer::run(uint16_t port) {
        try {
            std::cout<<"webserver starting on "<<port<< std::endl;
            ws_server_.listen(port);
            ws_server_.start_accept();
            server_thread_ = websocketpp::lib::make_shared<websocketpp::lib::thread>([this]() {
                try {
                    ws_server_.run();
                } catch (const std::exception& e) {
                    std::cout << "WebSocket server error: " << e.what() << std::endl;
                }
            });
            std::cout << "WebSocket server running" << std::endl;

        } catch (std::exception e) {
            std::cout << "Failed to start WebSocket server: " << e.what() << std::endl;
            throw;
        }
    }
}
