#include<iostream>
#include <cpprest/http_client.h>
#include <cpprest/json.h>
#include "./include/authentication.hpp"

using namespace web;
using namespace web::http;
using namespace web::http::client;

int main() {
    // Step 1: Authentication
    deribit::Config config(
        "####",
        "########",
        8080,
        "BTC",
        "BTC-PERPETUAL",
        {"BTC-PERPETUAL", "ETH-PERPETUAL"}
    );

    derbit::Authentication auth(config);

    std::cout << "🔐 Attempting to authenticate..." << std::endl;
    if (!auth.authenticate()) {
        std::cout << "❌ Authentication failed!" << std::endl;
        return 1;
    }
    std::cout << "✅ Authentication successful!" << std::endl;
    std::cout << "Access Token: " << auth.get_access_token().substr(0, 20) << "..." << std::endl;

    // Create HTTP client for API calls
    http_client client(config.BASE_URL);

    // Step 2: Get Account Information
    std::cout << "\n💰 Getting account information..." << std::endl;
    try {
        uri_builder account_uri("/private/get_account_summary");
        account_uri.append_query("currency", "BTC");
        account_uri.append_query("access_token", auth.get_access_token());

        auto account_response = client.request(methods::GET, account_uri.to_string()).get();
        if (account_response.status_code() == status_codes::OK) {
            auto account_json = account_response.extract_json().get();
            auto result = account_json["result"];

            std::cout << "📊 Account Summary:" << std::endl;
            std::cout << "   Balance: " << result["balance"].as_double() << " BTC" << std::endl;
            std::cout << "   Equity: " << result["equity"].as_double() << " BTC" << std::endl;
            std::cout << "   Available: " << result["available_funds"].as_double() << " BTC" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Account info failed: " << e.what() << std::endl;
    }

    // Step 3: Get Market Data
    std::cout << "\n📈 Getting BTC-PERPETUAL market data..." << std::endl;
    try {
        uri_builder ticker_uri("/public/ticker");
        ticker_uri.append_query("instrument_name", "BTC-PERPETUAL");

        auto ticker_response = client.request(methods::GET, ticker_uri.to_string()).get();
        if (ticker_response.status_code() == status_codes::OK) {
            auto ticker_json = ticker_response.extract_json().get();
            auto result = ticker_json["result"];

            std::cout << "🎯 BTC-PERPETUAL Ticker:" << std::endl;
            std::cout << "   Last Price: $" << result["last_price"].as_double() << std::endl;
            std::cout << "   Bid: $" << result["best_bid_price"].as_double() << std::endl;
            std::cout << "   Ask: $" << result["best_ask_price"].as_double() << std::endl;
            std::cout << "   24h Volume: " << result["stats"]["volume"].as_double() << " BTC" << std::endl;
            std::cout << "   24h Change: " << result["stats"]["price_change"].as_double() << "%" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Market data failed: " << e.what() << std::endl;
    }

    // Step 4: Get Order Book
    std::cout << "\n📚 Getting order book..." << std::endl;
    try {
        uri_builder book_uri("/public/get_order_book");
        book_uri.append_query("instrument_name", "BTC-PERPETUAL");
        book_uri.append_query("depth", "5");

        auto book_response = client.request(methods::GET, book_uri.to_string()).get();
        if (book_response.status_code() == status_codes::OK) {
            auto book_json = book_response.extract_json().get();
            auto result = book_json["result"];

            std::cout << "📖 Order Book (Top 5):" << std::endl;
            std::cout << "   Asks (Sell Orders):" << std::endl;
            auto asks = result["asks"].as_array();
            for (int i = 0; i < std::min(5, (int)asks.size()); i++) {
                auto ask = asks[i].as_array();
                std::cout << "     $" << ask[0].as_double() << " x " << ask[1].as_double() << std::endl;
            }

            std::cout << "   Bids (Buy Orders):" << std::endl;
            auto bids = result["bids"].as_array();
            for (int i = 0; i < std::min(5, (int)bids.size()); i++) {
                auto bid = bids[i].as_array();
                std::cout << "     $" << bid[0].as_double() << " x " << bid[1].as_double() << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Order book failed: " << e.what() << std::endl;
    }

    // Step 5: Place a Test Order (VERY SMALL AMOUNT!)
    std::cout << "\n📝 Placing a small test order..." << std::endl;
    try {
        uri_builder order_uri("/private/buy");
        order_uri.append_query("instrument_name", "BTC-PERPETUAL")
                 .append_query("amount", "10")  // Very small amount
                 .append_query("type", "limit")
                 .append_query("price", "50000")  // Way below market (won't fill)
                 .append_query("access_token", auth.get_access_token());

        auto order_response = client.request(methods::GET, order_uri.to_string()).get();
        if (order_response.status_code() == status_codes::OK) {
            auto order_json = order_response.extract_json().get();
            auto result = order_json["result"];
            auto order = result["order"];

            std::cout << "✅ Test order placed successfully!" << std::endl;
            std::cout << "   Order ID: " << order["order_id"].as_string() << std::endl;
            std::cout << "   State: " << order["order_state"].as_string() << std::endl;
            std::cout << "   Amount: " << order["amount"].as_double() << " USD" << std::endl;
            std::cout << "   Price: $" << order["price"].as_double() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Order placement failed: " << e.what() << std::endl;
    }

    // Step 6: Get Open Orders
    std::cout << "\n📋 Getting open orders..." << std::endl;
    try {
        uri_builder orders_uri("/private/get_open_orders_by_currency");
        orders_uri.append_query("currency", "BTC")
                  .append_query("access_token", auth.get_access_token());

        auto orders_response = client.request(methods::GET, orders_uri.to_string()).get();
        if (orders_response.status_code() == status_codes::OK) {
            auto orders_json = orders_response.extract_json().get();
            auto orders = orders_json["result"].as_array();

            std::cout << "📋 Open Orders (" << orders.size() << " total):" << std::endl;
            for (auto& order : orders) {  // Remove const
                std::cout << "   Order ID: " << order["order_id"].as_string() << std::endl;
                std::cout << "   Instrument: " << order["instrument_name"].as_string() << std::endl;
                std::cout << "   Side: " << order["direction"].as_string() << std::endl;
                std::cout << "   Amount: " << order["amount"].as_double() << std::endl;
                std::cout << "   Price: $" << order["price"].as_double() << std::endl;
                std::cout << "   ---" << std::endl;
            }
        }
    } catch (const std::exception& e) {
        std::cout << "❌ Open orders failed: " << e.what() << std::endl;
    }

    std::cout << "\n🎉 Trading system demo completed!" << std::endl;
    std::cout << "💡 This was on testnet with fake money - perfect for learning!" << std::endl;

    return 0;
}