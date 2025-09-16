//
// Created by Supradeep Chitumalla on 15/09/25.
//

#ifndef MARKET_DATA_H
#define MARKET_DATA_H

#include <string>
#include <map>
#include <set>
#include <mutex>
#include <functional>
#include <json/json.h>
#include <unordered_map>
#include <vector>
#include <thread>
#include <atomic>
#include <optional>

#include "buffer.hpp"

namespace deribit {

    struct Orderbook {
        std::string instrument_name;
        int64_t timestamp = 0;
        int64_t change_id = 0;
        double best_bid_price = 0.0;
        double best_bid_amount = 0.0;
        double best_ask_price = 0.0;
        double best_ask_amount = 0.0;
        std::map<double,double> bids;
        std::map<double,double> asks;
    };

    using OrderBookUpdateCallback = std::function<void(const std::string&, const Orderbook&)>;

    class MarketData {
    public:
        MarketData(size_t num_workers = 4, size_t queue_size = 65536)
            : queue_(queue_size), running_(true)
        {
            for (size_t i = 0; i < num_workers; ++i) {
                workers_.emplace_back([this] { this->worker_loop(); });
            }
        }

        ~MarketData() {
            running_ = false;
            for (auto& t : workers_) {
                if (t.joinable()) t.join();
            }
        }

        void enqueue_orderbook_update(const std::string& symbol, const Json::Value& payload) {
            while (!queue_.push({symbol, payload})) {
                std::this_thread::yield();
            }
        }

        void register_orderbook_callback(OrderBookUpdateCallback callback);
        Orderbook get_orderbook(const std::string &symbol);

    private:
        void worker_loop() {
            while (running_) {
                auto task = queue_.pop();
                if (task) {
                    this->on_orderbook_update(task->first, task->second);
                }
            }
        }

        // --- Core update logic ---
        void on_orderbook_update(const std::string &symbol, const Json::Value &payload);
        std::mutex& get_mutex_for_symbol(const std::string& symbol);
        void parse_orderbook_update(const std::string& symbol, const Json::Value& json_data);
        void apply_incremental_update(Orderbook& ob, const Json::Value& update_data);

        std::map<std::string, Orderbook> orderbooks_;
        std::unordered_map<std::string, std::unique_ptr<std::mutex>> orderbook_mutexes_;
        std::mutex mutexes_map_mutex_;

        std::vector<OrderBookUpdateCallback> orderbook_callbacks_;
        std::mutex callbacks_mutex_;

        Buffer<std::pair<std::string, Json::Value>> queue_;
        std::vector<std::thread> workers_;
        std::atomic<bool> running_;
    };

}

#endif //MARKET_DATA_H
