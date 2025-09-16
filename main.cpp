#include "market_data.hpp"
#include "config.hpp"
#include <iostream>
#include <thread>

#include "deribit_client.hpp"

int main() {
    deribit::Config config(
            "",
            "",
            8080,
            "BTC",
            "BTC-PERPETUAL",
            {"BTC-PERPETUAL"}
        );
    deribit::MarketData market_data;
    deribit::DeribitClient deribit_client(config, &market_data);


    // Start Deribit connection
    deribit_client.connect();

    // Wait for connection
    std::this_thread::sleep_for(std::chrono::seconds(2));

    // Subscribe to instruments
    deribit_client.subscribe("ETH-PERPETUAL");
    deribit_client.subscribe("BTC-PERPETUAL");


    std::this_thread::sleep_for(std::chrono::hours(1));

    return 0;
}
