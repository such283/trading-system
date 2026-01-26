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
            : queue_(queue_size), running_(true), dropped_messages_(0)
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

        // NEW: Accept Json::Value by rvalue reference (move semantics)
        void enqueue_orderbook_update(const std::string& symbol, Json::Value&& payload) {
            // Try to push, if queue is full, drop the message instead of blocking
            if (!queue_.push({symbol, std::move(payload)})) {
                // Queue is full - drop message and track it
                dropped_messages_.fetch_add(1, std::memory_order_relaxed);
                // Optionally log (but don't spam console in production)
                // std::cerr << "WARNING: Dropped message for " << symbol << std::endl;
            }
        }

        // Keep backward compatibility with const& (will make a copy, but at least no busy-wait)
        void enqueue_orderbook_update(const std::string& symbol, const Json::Value& payload) {
            if (!queue_.push({symbol, payload})) {
                dropped_messages_.fetch_add(1, std::memory_order_relaxed);
            }
        }

        Orderbook get_orderbook(const std::string &symbol);

        // Get stats
        size_t get_dropped_message_count() const {
            return dropped_messages_.load(std::memory_order_relaxed);
        }

    private:
        void worker_loop() {
            while (running_) {
                auto task = queue_.pop();
                if (task) {
                    this->on_orderbook_update(task->first, task->second);
                }
            }
        }

        void on_orderbook_update(const std::string &symbol, const Json::Value &payload);
        std::mutex& get_mutex_for_symbol(const std::string& symbol);
        void parse_orderbook_update(const std::string& symbol, const Json::Value& json_data);
        void apply_incremental_update(Orderbook& ob, const Json::Value& update_data);

        std::mutex orderbooks_mutex_;
        std::map<std::string, Orderbook> orderbooks_;
        std::unordered_map<std::string, std::unique_ptr<std::mutex>> orderbook_mutexes_;
        std::mutex mutexes_map_mutex_;

        Buffer<std::pair<std::string, Json::Value>> queue_;
        std::vector<std::thread> workers_;
        std::atomic<bool> running_;
        std::atomic<size_t> dropped_messages_;  // Track dropped messages
    };

}

#endif //MARKET_DATA_H