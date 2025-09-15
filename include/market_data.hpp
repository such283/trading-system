//
// Created by Supradeep Chitumalla on 15/09/25.
//

#ifndef MARKET_DATA_H
#define MARKET_DATA_H

#include<string>
#include <string>
#include <map>
#include <set>
#include <mutex>
#include <functional>
#include <json/json.h>

namespace deribit {
    struct Orderbook {
        std::string instrument_name;
        int64_t timestamp = 0;
        int64_t change_id = 0;
        double best_bid_price = 0.0;
        double best_bid_amount = 0.0;
        double best_ask_price = 0.0;
        double best_ask_amount = 0.0;
        std::map<double,double>bids;
        std::map<double,double>asks;
    };
    using OrderBookUpdateCallback = std::function<void(const std::string&, const Orderbook&)>;

    class MarketData {
    public:
        void register_orderbook_callback(OrderBookUpdateCallback callback);
        void on_orderbook_update(const std::string &symbol, const Json::Value &payload);
        Orderbook get_orderbook(const std::string &symbol);
    private:
        void parse_orderbook_update(const std::string& symbol, const Json::Value& json_data);
        void apply_incremental_update(Orderbook& ob, const Json::Value& update_data);
        std::map<std::string, Orderbook> orderbooks_;
        std::mutex orderbooks_mutex_;
        std::vector<OrderBookUpdateCallback> orderbook_callbacks_;
        std::mutex callbacks_mutex_;
    };

}

#endif //MARKET_DATA_H
