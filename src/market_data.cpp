//
// Created by Supradeep Chitumalla on 15/09/25.
//

#include "market_data.hpp"
#include <iostream>
namespace deribit {

    std::mutex &MarketData::get_mutex_for_symbol(const std::string &symbol) {
        std::lock_guard<std::mutex> mtx(mutexes_map_mutex_);
        if (orderbook_mutexes_.find(symbol) == orderbook_mutexes_.end()) {
            orderbook_mutexes_[symbol] = std::make_unique<std::mutex>();
        }
        return *orderbook_mutexes_[symbol];
    }

    Orderbook MarketData::get_orderbook(const std::string &symbol) {
        //bug here need to acquire the map mutex before accessing symbol mutex
        //will try to implement lock free on sunday for market data
        // if unable to do will fix this bug
        std::lock_guard<std::mutex> lock(get_mutex_for_symbol(symbol));
        auto it = orderbooks_.find(symbol);
        return (it != orderbooks_.end()) ? it->second : Orderbook();
    }

    void MarketData::on_orderbook_update(const std::string& symbol, const Json::Value& payload) {
        if (!payload.isMember("params") || !payload["params"].isMember("data")) return;

        const Json::Value& data = payload["params"]["data"];
        int64_t update_ts = data.isMember("timestamp") ? data["timestamp"].asInt64() : 0;

        std::lock_guard<std::mutex> lock(get_mutex_for_symbol(symbol));

        auto& ob = orderbooks_[symbol];

        if (update_ts <= ob.timestamp) {
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
    }

    void MarketData::parse_orderbook_update(const std::string &symbol, const Json::Value &json_data) {
        const Json::Value* data_ptr;
        if (json_data.isMember("params")) {
            data_ptr = &json_data["params"]["data"];
        } else {
            data_ptr = &json_data;
        }
        const Json::Value& data = *data_ptr;

        auto& ob = orderbooks_[symbol];

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
                    try {
                        if (bid.size() >= 3 && bid[0].isString()) {
                            if (bid[0].asString() == "delete") {
                                double price = bid[1].asDouble();
                                ob.bids.erase(price);
                            } else {
                                double price = bid[1].asDouble();
                                double amount = bid[2].asDouble();
                                if (amount == 0.0) {
                                    ob.bids.erase(price);
                                } else {
                                    ob.bids[price] = amount;
                                }
                            }
                        } else {
                            double price = bid[0].asDouble();
                            double amount = bid[1].asDouble();
                            if (amount == 0.0) {
                                ob.bids.erase(price);
                            } else {
                                ob.bids[price] = amount;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error processing bid: " << e.what() << std::endl;
                        continue;
                    }
                }
            }
        }
        if (const Json::Value& asks = data["asks"]; asks.isArray()) {
            for (Json::ArrayIndex i = 0; i < asks.size(); ++i) {
                const Json::Value& ask = asks[i];
                if (ask.size() >= 2) {
                    try {
                        if (ask.size() >= 3 && ask[0].isString()) {
                            if (ask[0].asString() == "delete") {
                                double price = ask[1].asDouble();
                                ob.asks.erase(price);
                            } else {
                                double price = ask[1].asDouble();
                                double amount = ask[2].asDouble();
                                if (amount == 0.0) {
                                    ob.asks.erase(price);
                                } else {
                                    ob.asks[price] = amount;
                                }
                            }
                        } else {
                            double price = ask[0].asDouble();
                            double amount = ask[1].asDouble();
                            if (amount == 0.0) {
                                ob.asks.erase(price);
                            } else {
                                ob.asks[price] = amount;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error processing ask: " << e.what() << std::endl;
                        continue;
                    }
                }
            }
        }

        if (!ob.bids.empty()) {
            ob.best_bid_price = ob.bids.rbegin()->first;
            ob.best_bid_amount = ob.bids.rbegin()->second;
        } else {
            ob.best_bid_price = 0.0;
            ob.best_bid_amount = 0.0;
        }

        if (!ob.asks.empty()) {
            ob.best_ask_price = ob.asks.begin()->first;
            ob.best_ask_amount = ob.asks.begin()->second;
        } else {
            ob.best_ask_price = 0.0;
            ob.best_ask_amount = 0.0;
        }

        std::cout << "Snapshot processed for " << symbol << std::endl;
    }

    void MarketData::apply_incremental_update(Orderbook& ob, const Json::Value& update_data) {
        ob.timestamp = update_data.get("timestamp", ob.timestamp).asInt64();
        ob.change_id = update_data.get("change_id", ob.change_id).asInt64();

        if (const Json::Value& bids = update_data["bids"]; bids.isArray()) {
            for (Json::ArrayIndex i = 0; i < bids.size(); ++i) {
                const Json::Value& b = bids[i];
                if (b.size() >= 2) {
                    try {
                        if (b.size() >= 3 && b[0].isString()) {
                            if (b[0].asString() == "delete") {
                                double price = b[1].asDouble();
                                ob.bids.erase(price);
                            } else {
                                double price = b[1].asDouble();
                                double amount = b[2].asDouble();
                                if (amount == 0.0) {
                                    ob.bids.erase(price);
                                } else {
                                    ob.bids[price] = amount;
                                }
                            }
                        } else {
                            double price = b[0].asDouble();
                            double amount = b[1].asDouble();
                            if (amount == 0.0) {
                                ob.bids.erase(price);
                            } else {
                                ob.bids[price] = amount;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error processing bid update: " << e.what() << std::endl;
                        continue;
                    }
                }
            }
        }
        if (const Json::Value& asks = update_data["asks"]; asks.isArray()) {
            for (Json::ArrayIndex i = 0; i < asks.size(); ++i) {
                const Json::Value& a = asks[i];
                if (a.size() >= 2) {
                    try {
                        if (a.size() >= 3 && a[0].isString()) {
                            if (a[0].asString() == "delete") {
                                double price = a[1].asDouble();
                                ob.asks.erase(price);
                            } else {
                                double price = a[1].asDouble();
                                double amount = a[2].asDouble();
                                if (amount == 0.0) {
                                    ob.asks.erase(price);
                                } else {
                                    ob.asks[price] = amount;
                                }
                            }
                        } else {
                            double price = a[0].asDouble();
                            double amount = a[1].asDouble();
                            if (amount == 0.0) {
                                ob.asks.erase(price);
                            } else {
                                ob.asks[price] = amount;
                            }
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "Error processing ask update: " << e.what() << std::endl;
                        continue;
                    }
                }
            }
        }
        bool has_json_best_bid = false, has_json_best_ask = false;

        if (const Json::Value& val = update_data["best_bid_price"]; !val.isNull()) {
            ob.best_bid_price = val.asDouble();
            has_json_best_bid = true;
        }
        if (const Json::Value& val = update_data["best_ask_price"]; !val.isNull()) {
            ob.best_ask_price = val.asDouble();
            has_json_best_ask = true;
        }
        if (const Json::Value& val = update_data["best_bid_amount"]; !val.isNull()) {
            ob.best_bid_amount = val.asDouble();
        }
        if (const Json::Value& val = update_data["best_ask_amount"]; !val.isNull()) {
            ob.best_ask_amount = val.asDouble();
        }

        if (!has_json_best_bid) {
            if (!ob.bids.empty()) {
                ob.best_bid_price = ob.bids.rbegin()->first;
                ob.best_bid_amount = ob.bids.rbegin()->second;
            } else {
                ob.best_bid_price = 0.0;
                ob.best_bid_amount = 0.0;
            }
        }

        if (!has_json_best_ask) {
            if (!ob.asks.empty()) {
                ob.best_ask_price = ob.asks.begin()->first;
                ob.best_ask_amount = ob.asks.begin()->second;
            } else {
                ob.best_ask_price = 0.0;
                ob.best_ask_amount = 0.0;
            }
        }


    }
}