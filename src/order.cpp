//
// Created by Supradeep Chitumalla on 17/09/25.
//

#include "order.hpp"
#include <cpprest/json.h>
#include <iostream>
#include <chrono>

namespace deribit {

OrderManager::OrderManager(Config& config, size_t thread_pool_size, size_t buffer_capacity)
    : config_(config)
    , client_(config.BASE_URL)
    , running_(false)
    , async_enabled_(false) {
    if (thread_pool_size > 0) {
        order_buffer_ = std::make_unique<Buffer<OrderParams>>(buffer_capacity);
        async_enabled_ = true;
        running_.store(true);
        for (size_t i = 0; i < thread_pool_size; ++i) {
            workers_.emplace_back(&OrderManager::worker_thread, this);
        }
    }
}

OrderManager::~OrderManager() {
    stop_async_processing();
}

web::http::http_request OrderManager::create_authenticated_request(
    web::http::method method,
    const std::string& path) {
    web::http::http_request request(method);
    request.headers().add("Authorization", "Bearer " + config_.access_token);
    request.set_request_uri(path);
    return request;
}

std::string OrderManager::place_buy_order(const OrderParams& params) {
    return place_buy_order_internal(params);
}

std::string OrderManager::place_sell_order(const OrderParams& params) {
    return place_sell_order_internal(params);
}

bool OrderManager::cancel_order(const std::string& order_id) {
    web::uri_builder builder("/private/cancel");
    builder.append_query("order_id", order_id);
    auto request = create_authenticated_request(web::http::methods::GET, builder.to_string());
    try {
        auto response = client_.request(request).get();
        return response.status_code() == web::http::status_codes::OK;
    } catch (const std::exception& e) {
        std::cout << "Cancel order error: " << e.what() << std::endl;
    }
    return false;
}

bool OrderManager::modify_order(const std::string& order_id, double new_amount, double new_price) {
    web::uri_builder builder("/private/edit");
    builder.append_query("order_id", order_id)
        .append_query("amount", new_amount)
        .append_query("price", new_price);
    auto request = create_authenticated_request(web::http::methods::GET, builder.to_string());
    try {
        auto response = client_.request(request).get();
        return response.status_code() == web::http::status_codes::OK;
    } catch (const std::exception& e) {
        std::cout << "Modify order error: " << e.what() << std::endl;
    }
    return false;
}

web::json::value OrderManager::get_positions(const std::string& currency, const std::string& kind) {
    web::uri_builder builder("/private/get_positions");
    builder.append_query("currency", currency)
        .append_query("kind", kind);
    auto request = create_authenticated_request(web::http::methods::GET, builder.to_string());
    try {
        auto response = client_.request(request).get();
        if (response.status_code() == web::http::status_codes::OK) {
            return response.extract_json().get();
        }
    } catch (const std::exception& e) {
        std::cout << "Get positions error: " << e.what() << std::endl;
    }
    return web::json::value::null();
}

std::string OrderManager::place_buy_order_internal(const OrderParams& params) {
    web::uri_builder builder("/private/buy");
    builder.append_query("amount", params.amount)
        .append_query("instrument_name", params.instrument_name)
        .append_query("type", params.type);
    if (params.type == "limit") {
        builder.append_query("price", params.price);
    }
    auto request = create_authenticated_request(web::http::methods::GET, builder.to_string());
    try {
        auto response = client_.request(request).get();
        if (response.status_code() == web::http::status_codes::OK) {
            auto json = response.extract_json().get();
            return json["result"]["order"]["order_id"].as_string();
        }
    } catch (const std::exception& e) {
        std::cout << "Buy order error: " << e.what() << std::endl;
    }
    return "";
}

std::string OrderManager::place_sell_order_internal(const OrderParams& params) {
    web::uri_builder builder("/private/sell");
    builder.append_query("advanced", "usd")
        .append_query("amount", params.amount)
        .append_query("instrument_name", params.instrument_name);
    if (params.type == "limit") {
        builder.append_query("price", params.price);
    }
    builder.append_query("type", params.type);
    auto request = create_authenticated_request(web::http::methods::GET, builder.to_string());
    try {
        auto response = client_.request(request).get();
        if (response.status_code() == web::http::status_codes::OK) {
            auto json = response.extract_json().get();
            return json["result"]["order"]["order_id"].as_string();
        }
    } catch (const std::exception& e) {
        std::cout << "Sell order error: " << e.what() << std::endl;
    }
    return "";
}

bool OrderManager::submit_order_async(OrderParams&& order) {
    if (!async_enabled_ || !order_buffer_) {
        return false;
    }
    return order_buffer_->push(order);
}

bool OrderManager::submit_order_async(const OrderParams& order) {
    if (!async_enabled_ || !order_buffer_) {
        return false;
    }
    return order_buffer_->push(order);
}

std::future<std::string> OrderManager::submit_order_future(OrderParams order) {
    auto promise = std::make_shared<std::promise<std::string>>();
    auto future = promise->get_future();
    order.callback = [promise](const std::string& order_id, bool success) {
        promise->set_value(success ? order_id : "");
    };
    if (!submit_order_async(std::move(order))) {
        promise->set_value("");
    }
    return future;
}

void OrderManager::start_async_processing() {
    if (!async_enabled_ || running_.load()) {
        return;
    }
    running_.store(true);
    size_t thread_count = 4;
    for (size_t i = 0; i < thread_count; ++i) {
        workers_.emplace_back(&OrderManager::worker_thread, this);
    }
}

void OrderManager::stop_async_processing() {
    if (!running_.load()) {
        return;
    }
    running_.store(false);
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
}

void OrderManager::worker_thread() {
    while (running_.load()) {
        if (!order_buffer_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
            continue;
        }
        auto order_opt = order_buffer_->pop();
        if (order_opt) {
            process_order(*order_opt);
        } else {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
}

void OrderManager::process_order(const OrderParams& params) {
    std::string order_id;
    bool success = false;
    try {
        if (params.side == "buy") {
            order_id = place_buy_order_internal(params);
        } else if (params.side == "sell") {
            order_id = place_sell_order_internal(params);
        } else {
            order_id = place_buy_order_internal(params);
        }
        success = !order_id.empty();
    } catch (const std::exception& e) {
        std::cout << "Async order processing error: " << e.what() << std::endl;
        success = false;
    }
    if (params.callback) {
        params.callback(order_id, success);
    }
}

size_t OrderManager::pending_orders() const {
    return 0;
}

bool OrderManager::is_async_running() const {
    return running_.load();
}

} // namespace deribit
