#include "market_data.hpp"
#include "config.hpp"
#include "config_loader.hpp"
#include "deribit_client.hpp"
#include "order.hpp"
#include "authentication.hpp"
#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <iomanip>
#include <chrono>

class TradingInterface {
private:
    deribit::Config& config_;
    deribit::OrderManager& order_manager_;
    deribit::MarketData& market_data_;
    deribit::DeribitClient* deribit_client_;
    std::vector<std::string> active_orders_;

public:
    TradingInterface(deribit::Config& config, deribit::OrderManager& om, deribit::MarketData& md, deribit::DeribitClient* client)
        : config_(config), order_manager_(om), market_data_(md), deribit_client_(client) {}

    void show_menu() {
        std::cout << "\n" << std::string(50, '=') << std::endl;
        std::cout << "DERIBIT TRADING INTERFACE" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "Available commands:" << std::endl;
        std::cout << "1. Place buy order" << std::endl;
        std::cout << "2. Place sell order" << std::endl;
        std::cout << "3. Cancel order" << std::endl;
        std::cout << "4. Modify order" << std::endl;
        std::cout << "5. Get positions" << std::endl;
        std::cout << "6. Get orderbook" << std::endl;
        std::cout << "7. Get ticker" << std::endl;
        std::cout << "8. Get instruments" << std::endl;
        std::cout << "9. Subscribe to symbol" << std::endl;
        std::cout << "10. Exit" << std::endl;
        std::cout << std::string(50, '=') << std::endl;
        std::cout << "Enter your choice (1-10): ";
    }

    void handle_buy_order() {
        std::cout << "\nPLACE BUY ORDER" << std::endl;

        std::string instrument, order_type;
        double amount, price = 0.0;

        std::cout << "Enter instrument name (e.g., BTC-PERPETUAL): ";
        std::cin >> instrument;

        std::cout << "Enter amount: ";
        std::cin >> amount;

        std::cout << "Enter order type (market/limit): ";
        std::cin >> order_type;

        if (order_type == "limit") {
            std::cout << "Enter price: ";
            std::cin >> price;
        }

        std::cout << "\nChoose execution method:" << std::endl;
        std::cout << "1. Synchronous (blocking)" << std::endl;
        std::cout << "2. Asynchronous (non-blocking)" << std::endl;
        std::cout << "Enter choice (1-2): ";

        int exec_choice;
        std::cin >> exec_choice;

        deribit::OrderParams order_params;
        order_params.instrument_name = instrument;
        order_params.amount = amount;
        order_params.price = price;
        order_params.type = order_type;
        order_params.side = "buy";

        if (exec_choice == 1) {
            std::cout << "Placing buy order (synchronous)..." << std::endl;
            auto start = std::chrono::high_resolution_clock::now();

            std::string order_id = order_manager_.place_buy_order(order_params);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (!order_id.empty()) {
                active_orders_.push_back(order_id);
                std::cout << "Buy order placed successfully!" << std::endl;
                std::cout << "Order ID: " << order_id << std::endl;
                std::cout << "Execution time: " << duration.count() << "ms" << std::endl;
            } else {
                std::cout << "Failed to place buy order" << std::endl;
            }
        } else {
            order_params.callback = [this](const std::string& order_id, bool success) {
                if (success) {
                    active_orders_.push_back(order_id);
                    std::cout << "\n[ASYNC] Buy order completed! Order ID: " << order_id << std::endl;
                } else {
                    std::cout << "\n[ASYNC] Buy order failed!" << std::endl;
                }
            };

            std::cout << "Submitting buy order (asynchronous)..." << std::endl;
            bool queued = order_manager_.submit_order_async(std::move(order_params));

            if (queued) {
                std::cout << "Order queued successfully! You'll be notified when it completes." << std::endl;
                std::cout << "Pending orders in queue: " << order_manager_.pending_orders() << std::endl;
            } else {
                std::cout << "Failed to queue order (buffer might be full)" << std::endl;
            }
        }
    }

    void handle_sell_order() {
        std::cout << "\nPLACE SELL ORDER" << std::endl;

        std::string instrument, order_type;
        double amount, price = 0.0;

        std::cout << "Enter instrument name (e.g., BTC-PERPETUAL): ";
        std::cin >> instrument;

        std::cout << "Enter amount: ";
        std::cin >> amount;

        std::cout << "Enter order type (market/limit): ";
        std::cin >> order_type;

        if (order_type == "limit") {
            std::cout << "Enter price: ";
            std::cin >> price;
        }

        std::cout << "\nChoose execution method:" << std::endl;
        std::cout << "1. Synchronous (blocking)" << std::endl;
        std::cout << "2. Asynchronous (non-blocking)" << std::endl;
        std::cout << "Enter choice (1-2): ";

        int exec_choice;
        std::cin >> exec_choice;

        deribit::OrderParams order_params;
        order_params.instrument_name = instrument;
        order_params.amount = amount;
        order_params.price = price;
        order_params.type = order_type;
        order_params.side = "sell";

        if (exec_choice == 1) {
            std::cout << "Placing sell order (synchronous)..." << std::endl;
            auto start = std::chrono::high_resolution_clock::now();

            std::string order_id = order_manager_.place_sell_order(order_params);

            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);

            if (!order_id.empty()) {
                active_orders_.push_back(order_id);
                std::cout << "Sell order placed successfully!" << std::endl;
                std::cout << "Order ID: " << order_id << std::endl;
                std::cout << "Execution time: " << duration.count() << "ms" << std::endl;
            } else {
                std::cout << "Failed to place sell order" << std::endl;
            }
        } else {
            order_params.callback = [this](const std::string& order_id, bool success) {
                if (success) {
                    active_orders_.push_back(order_id);
                    std::cout << "\n[ASYNC] Sell order completed! Order ID: " << order_id << std::endl;
                } else {
                    std::cout << "\n[ASYNC] Sell order failed!" << std::endl;
                }
            };

            std::cout << "Submitting sell order (asynchronous)..." << std::endl;
            bool queued = order_manager_.submit_order_async(std::move(order_params));

            if (queued) {
                std::cout << "Order queued successfully! You'll be notified when it completes." << std::endl;
                std::cout << "Pending orders in queue: " << order_manager_.pending_orders() << std::endl;
            } else {
                std::cout << "Failed to queue order (buffer might be full)" << std::endl;
            }
        }
    }

    void handle_cancel_order() {
        std::cout << "\nCANCEL ORDER" << std::endl;

        if (active_orders_.empty()) {
            std::cout << "No active orders to cancel." << std::endl;
            return;
        }

        std::cout << "Active orders:" << std::endl;
        for (size_t i = 0; i < active_orders_.size(); ++i) {
            std::cout << i + 1 << ". " << active_orders_[i] << std::endl;
        }

        std::cout << "Enter order number to cancel (0 for manual entry): ";
        int choice;
        std::cin >> choice;

        std::string order_id;
        if (choice == 0) {
            std::cout << "Enter order ID manually: ";
            std::cin >> order_id;
        } else if (choice > 0 && choice <= static_cast<int>(active_orders_.size())) {
            order_id = active_orders_[choice - 1];
        } else {
            std::cout << "Invalid choice!" << std::endl;
            return;
        }

        std::cout << "Cancelling order: " << order_id << std::endl;
        bool success = order_manager_.cancel_order(order_id);

        if (success) {
            std::cout << "Order cancelled successfully!" << std::endl;
            active_orders_.erase(
                std::remove(active_orders_.begin(), active_orders_.end(), order_id),
                active_orders_.end()
            );
        } else {
            std::cout << "Failed to cancel order" << std::endl;
        }
    }

    void handle_modify_order() {
        std::cout << "\nMODIFY ORDER" << std::endl;

        if (active_orders_.empty()) {
            std::cout << "No active orders to modify." << std::endl;
            return;
        }

        std::cout << "Active orders:" << std::endl;
        for (size_t i = 0; i < active_orders_.size(); ++i) {
            std::cout << i + 1 << ". " << active_orders_[i] << std::endl;
        }

        std::cout << "Enter order number to modify (0 for manual entry): ";
        int choice;
        std::cin >> choice;

        std::string order_id;
        if (choice == 0) {
            std::cout << "Enter order ID manually: ";
            std::cin >> order_id;
        } else if (choice > 0 && choice <= static_cast<int>(active_orders_.size())) {
            order_id = active_orders_[choice - 1];
        } else {
            std::cout << "Invalid choice!" << std::endl;
            return;
        }

        double new_amount, new_price;
        std::cout << "Enter new amount: ";
        std::cin >> new_amount;
        std::cout << "Enter new price: ";
        std::cin >> new_price;

        std::cout << "Modifying order: " << order_id << std::endl;
        bool success = order_manager_.modify_order(order_id, new_amount, new_price);

        if (success) {
            std::cout << "Order modified successfully!" << std::endl;
        } else {
            std::cout << "Failed to modify order" << std::endl;
        }
    }

    void handle_get_positions() {
        std::cout << "\nGET POSITIONS" << std::endl;

        std::string currency, kind;
        std::cout << "Enter currency (BTC/ETH/USD): ";
        std::cin >> currency;
        std::cout << "Enter kind (future/option): ";
        std::cin >> kind;

        std::cout << "Fetching positions..." << std::endl;
        auto positions = order_manager_.get_positions(currency, kind);

        if (!positions.is_null() && positions.has_field("result")) {
            std::cout << "Positions retrieved:" << std::endl;
            std::cout << positions.at("result").serialize() << std::endl;
        } else {
            std::cout << "Failed to get positions or no positions found" << std::endl;
        }
    }

    void handle_get_orderbook() {
        std::cout << "\nGET ORDERBOOK" << std::endl;

        std::string symbol;
        std::cout << "Enter symbol (e.g., BTC-PERPETUAL): ";
        std::cin >> symbol;

        auto orderbook = market_data_.get_orderbook(symbol);

        if (orderbook.instrument_name.empty()) {
            std::cout << "No orderbook data available for " << symbol << std::endl;
            std::cout << "Make sure you're subscribed to this symbol's market data." << std::endl;
            return;
        }

        std::cout << "Orderbook for " << symbol << ":" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Timestamp: " << orderbook.timestamp << std::endl;
        std::cout << "Best Bid: " << std::fixed << std::setprecision(2)
                  << orderbook.best_bid_price << " (" << orderbook.best_bid_amount << ")" << std::endl;
        std::cout << "Best Ask: " << orderbook.best_ask_price << " (" << orderbook.best_ask_amount << ")" << std::endl;

        if (orderbook.best_ask_price > 0 && orderbook.best_bid_price > 0) {
            double spread = orderbook.best_ask_price - orderbook.best_bid_price;
            double spread_pct = (spread / orderbook.best_bid_price) * 100;
            std::cout << "Spread: " << spread << " (" << std::setprecision(4) << spread_pct << "%)" << std::endl;
        }
    }

    void handle_get_ticker() {
        std::cout << "\nGET TICKER" << std::endl;
        std::cout << "This would fetch ticker data from Deribit API." << std::endl;
        std::cout << "(Not implemented in current order manager)" << std::endl;
    }

    void handle_get_instruments() {
        std::cout << "\nGET INSTRUMENTS" << std::endl;
        std::cout << "This would fetch available instruments from Deribit API." << std::endl;
        std::cout << "(Not implemented in current order manager)" << std::endl;
    }

    void handle_coin_subscribe() {
        std::cout << "Enter the symbol to subscribe (e.g., BTC-PERPETUAL): ";
        std::string symbol;
        std::cin >> symbol;
        if (deribit_client_) {
            deribit_client_->subscribe(symbol);
        } else {
            std::cout << "Deribit client is not available!" << std::endl;
        }
    }

    void run() {
        int choice = 0;

        while (choice != 10) {
            show_menu();
            std::cin >> choice;

            switch (choice) {
                case 1:
                    handle_buy_order();
                    break;
                case 2:
                    handle_sell_order();
                    break;
                case 3:
                    handle_cancel_order();
                    break;
                case 4:
                    handle_modify_order();
                    break;
                case 5:
                    handle_get_positions();
                    break;
                case 6:
                    handle_get_orderbook();
                    break;
                case 7:
                    handle_get_ticker();
                    break;
                case 8:
                    handle_get_instruments();
                    break;
                case 9:
                    handle_coin_subscribe();
                    break;
                case 10:
                    std::cout << "Exiting trading interface..." << std::endl;
                    break;
                default:
                    std::cout << "Invalid choice! Please enter 1-10." << std::endl;
                    break;
            }

            if (choice != 10) {
                std::cout << "\nPress Enter to continue...";
                std::cin.ignore();
                std::cin.get();
            }
        }
    }
};

int main(int argc, char* argv[]) {
    // Determine config file path
    std::string config_path = "config.json";
    if (argc > 1) {
        config_path = argv[1];
    }

    std::cout << "Starting Deribit Trading System..." << std::endl;
    std::cout << "Loading configuration from: " << config_path << std::endl;

    // Load configuration from file
    deribit::Config config;
    try {
        config = deribit::ConfigLoader::load_from_file(config_path);
        std::cout << "Configuration loaded successfully!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Failed to load configuration: " << e.what() << std::endl;
        std::cerr << "\nPlease ensure config.json exists and contains valid credentials." << std::endl;
        std::cerr << "You can create it from the template or pass a custom path: ./trading <config_path>" << std::endl;
        return -1;
    }

    // Authenticate
    derbit::Authentication auth(config);
    std::cout << "Authenticating..." << std::endl;
    if (!auth.authenticate()) {
        std::cerr << "Authentication failed! Please check your credentials in config.json" << std::endl;
        return -1;
    }
    std::cout << "Authentication successful!" << std::endl;

    // Initialize market data and WebSocket client
    deribit::MarketData market_data;
    deribit::DeribitClient deribit_client(config, &market_data);

    std::cout << "Connecting to Deribit WebSocket..." << std::endl;
    deribit_client.connect();
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (!deribit_client.is_connected()) {
        std::cerr << "Failed to connect to Deribit WebSocket!" << std::endl;
        return -1;
    }
    std::cout << "WebSocket connected!" << std::endl;

    // Initialize order manager
    deribit::OrderManager order_manager(config, 4, 1024);

    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "SYSTEM READY FOR TRADING!" << std::endl;
    std::cout << "Async Order Manager: 4 worker threads" << std::endl;
    std::cout << "Market Data: Connected and streaming" << std::endl;
    std::cout << "Authentication: Active" << std::endl;
    std::cout << std::string(60, '=') << std::endl;

    // Run trading interface
    TradingInterface interface(config, order_manager, market_data, &deribit_client);
    interface.run();

    // Shutdown
    std::cout << "\nShutting down trading system..." << std::endl;
    deribit_client.disconnect();
    std::cout << "System shutdown complete. Goodbye!" << std::endl;

    return 0;
}