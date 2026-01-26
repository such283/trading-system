//
// Created by Supradeep Chitumalla on 15/09/25.
//

#ifndef MARKET_DATA_H
#define MARKET_DATA_H

#include <string>
#include <map>
#include <iostream>
#include <set>
#include <mutex>
#include <functional>
#include <json/json.h>
#include <unordered_map>
#include <vector>
#include <thread>
#include <atomic>
#include <optional>
#include <chrono>

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
            : queue_(queue_size), running_(true), dropped_messages_(0),
              total_updates_(0), total_latency_ns_(0)
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

        // Print latency statistics
        void print_latency_stats() const {
            uint64_t total = total_updates_.load(std::memory_order_relaxed);
            uint64_t total_lat = total_latency_ns_.load(std::memory_order_relaxed);

            if (total == 0) {
                std::cout << "No latency data collected yet." << std::endl;
                std::cout << "Subscribe to a symbol to start collecting data." << std::endl;
                return;
            }

            uint64_t avg_ns = total_lat / total;

            std::cout << "\n" << std::string(60, '=') << std::endl;
            std::cout << "LATENCY STATISTICS" << std::endl;
            std::cout << std::string(60, '=') << std::endl;
            std::cout << "Total updates processed: " << total << std::endl;
            std::cout << "Average processing time: " << format_latency(avg_ns) << std::endl;
            std::cout << std::string(60, '=') << std::endl << std::endl;
        }

    private:
        void worker_loop() {
            while (running_) {
                auto task = queue_.pop();
                if (task) {
                    // Measure latency
                    auto start = std::chrono::high_resolution_clock::now();
                    this->on_orderbook_update(task->first, task->second);
                    auto end = std::chrono::high_resolution_clock::now();

                    // Track latency
                    auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                    total_latency_ns_.fetch_add(duration_ns, std::memory_order_relaxed);
                    total_updates_.fetch_add(1, std::memory_order_relaxed);
                }
            }
        }

        void on_orderbook_update(const std::string &symbol, const Json::Value &payload);
        std::mutex& get_mutex_for_symbol(const std::string& symbol);
        void parse_orderbook_update(const std::string& symbol, const Json::Value& json_data);
        void apply_incremental_update(Orderbook& ob, const Json::Value& update_data);

        // Format latency for display
        std::string format_latency(uint64_t ns) const {
            if (ns < 1000) {
                return std::to_string(ns) + " ns";
            } else if (ns < 1000000) {
                return std::to_string(ns / 1000) + "." + std::to_string((ns % 1000) / 100) + " μs";
            } else {
                return std::to_string(ns / 1000000) + "." + std::to_string((ns % 1000000) / 100000) + " ms";
            }
        }

        std::mutex orderbooks_mutex_;
        std::map<std::string, Orderbook> orderbooks_;
        std::unordered_map<std::string, std::unique_ptr<std::mutex>> orderbook_mutexes_;
        std::mutex mutexes_map_mutex_;

        Buffer<std::pair<std::string, Json::Value>> queue_;
        std::vector<std::thread> workers_;
        std::atomic<bool> running_;
        std::atomic<size_t> dropped_messages_;  // Track dropped messages

        // Simple latency tracking
        std::atomic<uint64_t> total_updates_;
        std::atomic<uint64_t> total_latency_ns_;
    };

}

#endif //MARKET_DATA_H