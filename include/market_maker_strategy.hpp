//
// Created by Supradeep Chitumalla
//

#ifndef MARKET_MAKER_STRATEGY_H
#define MARKET_MAKER_STRATEGY_H

#include "market_data.hpp"
#include "order.hpp"
#include <string>
#include <atomic>
#include <mutex>
#include <vector>
#include <iostream>
#include <iomanip>

namespace deribit {

struct Position {
    std::string instrument;
    double size = 0.0;              // positive = long, negative = short
    double avg_entry_price = 0.0;   // average price of position
    double realized_pnl = 0.0;      // closed position PnL
    mutable double unrealized_pnl = 0.0;    // current position PnL (mutable for const methods)

    void update_unrealized_pnl(double current_price) const {
        if (size != 0) {
            unrealized_pnl = size * (current_price - avg_entry_price);
        } else {
            unrealized_pnl = 0.0;
        }
    }

    void add_trade(double trade_size, double trade_price) {
        if (size == 0) {
            // Opening new position
            size = trade_size;
            avg_entry_price = trade_price;
        } else if ((size > 0 && trade_size > 0) || (size < 0 && trade_size < 0)) {
            // Adding to position
            double total_value = size * avg_entry_price + trade_size * trade_price;
            size += trade_size;
            avg_entry_price = total_value / size;
        } else {
            // Reducing or flipping position
            double min_size = std::min(std::abs(size), std::abs(trade_size));
            realized_pnl += min_size * (trade_price - avg_entry_price) * (size > 0 ? 1 : -1);

            size += trade_size;
            if (size * trade_size > 0) {
                // Flipped position
                avg_entry_price = trade_price;
            }
        }
    }

    double get_total_pnl(double current_price) const {
        update_unrealized_pnl(current_price);
        return realized_pnl + unrealized_pnl;
    }
};

struct MarketMakerConfig {
    std::string instrument = "BTC-PERPETUAL";
    double order_size = 10.0;           // Size of each order (in USD for perpetuals)
    double spread_bps = 10.0;           // Spread in basis points (10 bps = 0.1%)
    double max_position = 1000.0;       // Maximum position size
    double stop_loss_usd = 500.0;       // Stop loss in USD
    double take_profit_usd = 1000.0;    // Take profit in USD
    bool enabled = false;
};

class SimpleMarketMaker {
public:
    SimpleMarketMaker(OrderManager& order_mgr, const MarketMakerConfig& config = MarketMakerConfig())
        : order_manager_(order_mgr)
        , config_(config)
        , running_(false)
        , total_orders_placed_(0)
        , total_orders_filled_(0) {}

    void start() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = true;
        config_.enabled = true;
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "MARKET MAKER STARTED" << std::endl;
        std::cout << "Instrument: " << config_.instrument << std::endl;
        std::cout << "Order Size: " << config_.order_size << " USD" << std::endl;
        std::cout << "Spread: " << config_.spread_bps << " bps" << std::endl;
        std::cout << "Max Position: ±" << config_.max_position << " USD" << std::endl;
        std::cout << std::string(60, '=') << std::endl << std::endl;
    }

    void stop() {
        std::lock_guard<std::mutex> lock(mutex_);
        running_ = false;
        config_.enabled = false;
        cancel_all_orders();
        std::cout << "\nMarket maker stopped." << std::endl;
    }

    bool is_running() const {
        return running_.load();
    }

    void on_orderbook_update(const std::string& symbol, const Orderbook& ob) {
        if (!running_ || symbol != config_.instrument) {
            return;
        }

        if (ob.best_bid_price == 0 || ob.best_ask_price == 0) {
            return;  // Invalid orderbook
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Calculate mid price
        double mid_price = (ob.best_bid_price + ob.best_ask_price) / 2.0;

        // Update unrealized PnL
        position_.update_unrealized_pnl(mid_price);

        // Check risk limits
        if (should_stop_trading(mid_price)) {
            std::cout << "⚠️  Risk limit hit! Stopping strategy..." << std::endl;
            print_pnl(mid_price);
            stop();
            return;
        }

        // Calculate our quote prices
        double spread_multiplier = config_.spread_bps / 10000.0;  // bps to decimal
        double our_bid = mid_price * (1.0 - spread_multiplier);
        double our_ask = mid_price * (1.0 + spread_multiplier);

        // Round to nearest 0.50 (Deribit tick size for BTC)
        our_bid = std::floor(our_bid * 2.0) / 2.0;
        our_ask = std::ceil(our_ask * 2.0) / 2.0;

        // Check if we should place orders (position limits)
        bool can_buy = position_.size < config_.max_position;
        bool can_sell = position_.size > -config_.max_position;

        // Simple strategy: Place limit orders if we don't have active orders
        if (active_buy_order_.empty() && can_buy) {
            place_buy_order(our_bid);
        }

        if (active_sell_order_.empty() && can_sell) {
            place_sell_order(our_ask);
        }
    }

    void print_status(double current_price) const {
        std::lock_guard<std::mutex> lock(mutex_);

        std::cout << "\n" << std::string(60, '-') << std::endl;
        std::cout << "MARKET MAKER STATUS" << std::endl;
        std::cout << std::string(60, '-') << std::endl;
        std::cout << "Running: " << (running_ ? "Yes" : "No") << std::endl;
        std::cout << "Orders Placed: " << total_orders_placed_ << std::endl;
        std::cout << "Orders Filled: " << total_orders_filled_ << std::endl;
        std::cout << std::endl;

        std::cout << "Position: " << position_.size << " USD" << std::endl;
        std::cout << "Avg Entry: $" << std::fixed << std::setprecision(2) << position_.avg_entry_price << std::endl;
        std::cout << "Current Price: $" << current_price << std::endl;
        std::cout << std::endl;

        std::cout << "Realized PnL: $" << std::setprecision(2) << position_.realized_pnl << std::endl;
        std::cout << "Unrealized PnL: $" << position_.unrealized_pnl << std::endl;
        std::cout << "Total PnL: $" << position_.get_total_pnl(current_price) << std::endl;
        std::cout << std::string(60, '-') << std::endl << std::endl;
    }

    Position get_position() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return position_;
    }

private:
    void place_buy_order(double price) {
        OrderParams params;
        params.instrument_name = config_.instrument;
        params.amount = config_.order_size;
        params.price = price;
        params.type = "limit";
        params.side = "buy";

        params.callback = [this](const std::string& order_id, bool success) {
            if (success) {
                std::lock_guard<std::mutex> lock(mutex_);
                active_buy_order_ = order_id;
                total_orders_placed_++;
                std::cout << "✓ Buy order placed at $" << std::fixed << std::setprecision(2)
                          << "..." << std::endl;
            }
        };

        order_manager_.submit_order_async(std::move(params));
    }

    void place_sell_order(double price) {
        OrderParams params;
        params.instrument_name = config_.instrument;
        params.amount = config_.order_size;
        params.price = price;
        params.type = "limit";
        params.side = "sell";

        params.callback = [this](const std::string& order_id, bool success) {
            if (success) {
                std::lock_guard<std::mutex> lock(mutex_);
                active_sell_order_ = order_id;
                total_orders_placed_++;
                std::cout << "✓ Sell order placed at $" << std::fixed << std::setprecision(2)
                          << "..." << std::endl;
            }
        };

        order_manager_.submit_order_async(std::move(params));
    }

    void cancel_all_orders() {
        if (!active_buy_order_.empty()) {
            order_manager_.cancel_order(active_buy_order_);
            active_buy_order_.clear();
        }
        if (!active_sell_order_.empty()) {
            order_manager_.cancel_order(active_sell_order_);
            active_sell_order_.clear();
        }
    }

    bool should_stop_trading(double current_price) const {
        double total_pnl = position_.get_total_pnl(current_price);

        // Stop loss
        if (total_pnl < -config_.stop_loss_usd) {
            std::cout << " Stop loss triggered: $" << total_pnl << std::endl;
            return true;
        }

        // Take profit
        if (total_pnl > config_.take_profit_usd) {
            std::cout << " Take profit triggered: $" << total_pnl << std::endl;
            return true;
        }

        return false;
    }

    void print_pnl(double current_price) const {
        std::cout << "\n" << std::string(60, '=') << std::endl;
        std::cout << "FINAL PNL REPORT" << std::endl;
        std::cout << std::string(60, '=') << std::endl;
        std::cout << "Position Size: " << position_.size << " USD" << std::endl;
        std::cout << "Realized PnL: $" << std::fixed << std::setprecision(2) << position_.realized_pnl << std::endl;
        std::cout << "Unrealized PnL: $" << position_.unrealized_pnl << std::endl;
        std::cout << "Total PnL: $" << position_.get_total_pnl(current_price) << std::endl;
        std::cout << std::string(60, '=') << std::endl << std::endl;
    }

    OrderManager& order_manager_;
    MarketMakerConfig config_;
    Position position_;

    std::string active_buy_order_;
    std::string active_sell_order_;

    std::atomic<bool> running_;
    mutable std::mutex mutex_;

    // Statistics
    uint64_t total_orders_placed_;
    uint64_t total_orders_filled_;
};

} // namespace deribit

#endif // MARKET_MAKER_STRATEGY_H