//
// Created by Supradeep Chitumalla on 17/09/25.
//

#ifndef ORDER_H
#define ORDER_H
#include "config.hpp"
#include "buffer.hpp"
#include <string>
#include <cpprest/http_client.h>
#include <thread>
#include <vector>
#include <atomic>
#include <future>
#include <functional>

namespace deribit {

struct OrderParams {
    std::string instrument_name;
    double amount;
    double price;
    std::string type;
    std::string side;

    std::function<void(const std::string& order_id, bool success)> callback;

    OrderParams() = default;

    OrderParams(const std::string& instrument, double amt, double pr, const std::string& order_type)
        : instrument_name(instrument), amount(amt), price(pr), type(order_type) {}
};

class OrderManager {
public:
    explicit OrderManager(Config& config, size_t thread_pool_size = 4, size_t buffer_capacity = 1024);
    ~OrderManager();

    std::string place_buy_order(const OrderParams& params);
    std::string place_sell_order(const OrderParams& params);
    bool cancel_order(const std::string& order_id);
    bool modify_order(const std::string& order_id, double new_amount, double new_price);
    web::json::value get_positions(const std::string& currency, const std::string& kind);

    bool submit_order_async(OrderParams&& order);
    bool submit_order_async(const OrderParams& order);

    std::future<std::string> submit_order_future(OrderParams order);

    void start_async_processing();
    void stop_async_processing();

    size_t pending_orders() const;
    bool is_async_running() const;

private:
    Config& config_;
    web::http::client::http_client client_;

    std::unique_ptr<Buffer<OrderParams>> order_buffer_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_;
    bool async_enabled_;

    web::http::http_request create_authenticated_request(
        web::http::method method,
        const std::string& path
    );

    void worker_thread();
    void process_order(const OrderParams& params);

    std::string place_buy_order_internal(const OrderParams& params);
    std::string place_sell_order_internal(const OrderParams& params);
};

} // namespace deribit
#endif //ORDER_H
