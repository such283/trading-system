//
// Created by Supradeep Chitumalla on 15/09/25.
//

#include "market_data.hpp"
#include <iostream>
namespace deribit {

    std::mutex &MarketData::get_mutex_for_symbol(const std::string &symbol) {
        std::lock_guard<std::mutex>mtx(mutexes_map_mutex_);
        if (orderbook_mutexes_.find(symbol)==orderbook_mutexes_.end()) {
            orderbook_mutexes_[symbol] = std::make_unique<std::mutex>();
        }
        return *orderbook_mutexes_[symbol];
    }


    void MarketData::register_orderbook_callback(OrderBookUpdateCallback callback) {
        std::lock_guard<std::mutex>lock(callbacks_mutex_);
        orderbook_callbacks_.push_back(callback);
    }
    Orderbook MarketData::get_orderbook(const std::string &symbol) {
        std::lock_guard<std::mutex>lock(get_mutex_for_symbol(symbol));
        auto it = orderbooks_.find(symbol);
        return (it != orderbooks_.end()) ? it->second : Orderbook();
    }
    void MarketData::on_orderbook_update(const std::string& symbol, const Json::Value& payload) {
        std::lock_guard<std::mutex>lock(callbacks_mutex_);
        if (!payload.isMember("params") || !payload["params"].isMember("data")) return;
        const Json::Value& data = payload["params"]["data"];
        int64_t update_ts = data.isMember("timestamp") ? data["timestamp"].asInt64() : 0;
        auto& ob = orderbooks_[symbol];
        if (update_ts <= ob.timestamp) {
            // Ignore out-of-order update
            return;
        }
        if (data.isMember("type")) {
            std::string type = data["type"].asString();
            if (type == "snapshot") {
                parse_orderbook_update(symbol, data);
            } else if (type == "change") {
                apply_incremental_update(ob, data);
            }
        }
        for (auto& cb : orderbook_callbacks_) {
            cb(symbol, orderbooks_[symbol]);
        }
    }
    void MarketData::parse_orderbook_update(const std::string &symbol, const Json::Value &json_data) {
        const Json::Value* data_ptr;
        if (json_data.isMember("params")) {
            data_ptr = &json_data["params"]["data"];
        } else {
            data_ptr = &json_data;
        }
        const Json::Value& data = *data_ptr;
        Orderbook ob;
        ob.instrument_name = symbol;
        ob.timestamp = data.get("timestamp", ob.timestamp).asInt64();
        ob.change_id = data.get("change_id", ob.change_id).asInt64();

        if (const Json::Value& val = data["best_bid_price"]; !val.isNull()) {
            ob.best_bid_price = val.asDouble();
        }
        if (const Json::Value& val = data["best_bid_amount"]; !val.isNull()) {
            ob.best_bid_amount = val.asDouble();
        }
        if (const Json::Value& val = data["best_ask_price"]; !val.isNull()) {
            ob.best_ask_price = val.asDouble();
        }
        if (const Json::Value& val = data["best_ask_amount"]; !val.isNull()) {
            ob.best_ask_amount = val.asDouble();
        }
        if (const Json::Value& bids = data["bids"]; bids.isArray()) {
            for (Json::ArrayIndex i = 0; i < bids.size(); ++i) {
                const Json::Value& bid = bids[i];
                if (bid.size() >= 2) {
                    if (bid.size() >= 3 && bid[0].isString()) {
                        if (bid[0].asCString()[0] == 'd') {
                            ob.bids.erase(bid[1].asDouble());
                        } else {
                            double amount = bid[2].asDouble();
                            if (amount == 0.0) {
                                ob.bids.erase(bid[1].asDouble());
                            } else {
                                ob.bids[bid[1].asDouble()] = amount;
                            }
                        }
                    } else {
                        double amount = bid[1].asDouble();
                        if (amount == 0.0) {
                            ob.bids.erase(bid[0].asDouble());
                        } else {
                            ob.bids[bid[0].asDouble()] = amount;
                        }
                    }
                }
            }
        }
        if (const Json::Value& asks = data["asks"]; asks.isArray()) {
            for (Json::ArrayIndex i = 0; i < asks.size(); ++i) {
                const Json::Value& ask = asks[i];
                if (ask.size() >= 2) {
                    if (ask.size() >= 3 && ask[0].isString()) {
                        if (ask[0].asCString()[0] == 'd') { // "delete"
                            ob.asks.erase(ask[1].asDouble());
                        } else {
                            double amount = ask[2].asDouble();
                            if (amount == 0.0) {
                                ob.asks.erase(ask[1].asDouble());
                            } else {
                                ob.asks[ask[1].asDouble()] = amount;
                            }
                        }
                    } else {
                        double amount = ask[1].asDouble();
                        if (amount == 0.0) {
                            ob.asks.erase(ask[0].asDouble());
                        } else {
                            ob.asks[ask[0].asDouble()] = amount;
                        }
                    }
                }
            }
        }
        if (!ob.bids.empty()) {
            ob.best_bid_price = ob.bids.rbegin()->first;
            ob.best_bid_amount = ob.bids.rbegin()->second;
        }
        if (!ob.asks.empty()) {
            ob.best_ask_price = ob.asks.begin()->first;
            ob.best_ask_amount = ob.asks.begin()->second;
        }
    }
    void MarketData::apply_incremental_update(Orderbook& ob, const Json::Value& update_data) {
        ob.timestamp = update_data.get("timestamp", ob.timestamp).asInt64();
        ob.change_id = update_data.get("change_id", ob.change_id).asInt64();
        if (const Json::Value& bids = update_data["bids"]; bids.isArray()) {
            for (Json::ArrayIndex i = 0; i < bids.size(); ++i) {
                const Json::Value& b = bids[i];
                if (b.size() >= 2) {
                    if (b.size() >= 3 && b[0].isString()) {
                        if (b[0].asCString()[0] == 'd') {
                            ob.bids.erase(b[1].asDouble());
                        } else {
                            double q = b[2].asDouble();
                            if (q == 0.0) {
                                ob.bids.erase(b[1].asDouble());
                            } else {
                                ob.bids[b[1].asDouble()] = q;
                            }
                        }
                    } else {
                        double p = b[0].asDouble();
                        double q = b[1].asDouble();
                        if (q == 0.0) {
                            ob.bids.erase(p);
                        } else {
                            ob.bids[p] = q;
                        }
                    }
                }
            }
        }
        if (const Json::Value& asks = update_data["asks"]; asks.isArray()) {
            for (Json::ArrayIndex i = 0; i < asks.size(); ++i) {
                const Json::Value& a = asks[i];
                if (a.size() >= 2) {
                    if (a.size() >= 3 && a[0].isString()) {
                        if (a[0].asCString()[0] == 'd') {
                            ob.asks.erase(a[1].asDouble());
                        } else {
                            double q = a[2].asDouble();
                            if (q == 0.0) {
                                ob.asks.erase(a[1].asDouble());
                            } else {
                                ob.asks[a[1].asDouble()] = q;
                            }
                        }
                    } else {
                        double p = a[0].asDouble();
                        double q = a[1].asDouble();
                        if (q == 0.0) {
                            ob.asks.erase(p);
                        } else {
                            ob.asks[p] = q;
                        }
                    }
                }
            }
        }
        if (const Json::Value& val = update_data["best_bid_price"]; !val.isNull()) {
            ob.best_bid_price = val.asDouble();
        }
        if (const Json::Value& val = update_data["best_ask_price"]; !val.isNull()) {
            ob.best_ask_price = val.asDouble();
        }
        if (const Json::Value& val = update_data["best_bid_amount"]; !val.isNull()) {
            ob.best_bid_amount = val.asDouble();
        }
        if (const Json::Value& val = update_data["best_ask_amount"]; !val.isNull()) {
            ob.best_ask_amount = val.asDouble();
        }
        if (!ob.bids.empty()) {
            ob.best_bid_price = ob.bids.rbegin()->first;
            ob.best_bid_amount = ob.bids.rbegin()->second;
        }
        if (!ob.asks.empty()) {
            ob.best_ask_price = ob.asks.begin()->first;
            ob.best_ask_amount = ob.asks.begin()->second;
        }
    }
}