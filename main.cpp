#include <iostream>
#include <thread>
#include <chrono>
#include "./include/websocket_server.hpp"
#include "./include/config.hpp"

int main() {
    std::cout << "🚀 Starting WebSocket Server Test..." << std::endl;

    try {
        // Create configuration
        deribit::Config config(
            "",  // No API key needed for this test
            "",  // No API secret needed for this test
            8080,
            "BTC",
            "BTC-PERPETUAL",
            {"BTC-PERPETUAL"}
        );

        // Create WebSocket server
        std::cout << "📡 Creating WebSocket server..." << std::endl;
        deribit::WebsocketServer server(config);

        // Start the server on port 8080
        std::cout << "🌐 Starting server on port 8080..." << std::endl;
        server.run(8080);

        std::cout << "✅ WebSocket server is running!" << std::endl;
        std::cout << "📋 Test Instructions:" << std::endl;
        std::cout << "   1. Open a WebSocket client (like wscat or browser dev tools)" << std::endl;
        std::cout << "   2. Connect to: ws://localhost:8080" << std::endl;
        std::cout << "   3. Send a test message like:" << std::endl;
        std::cout << "      {\"operation\":\"subscribe\",\"symbol\":\"BTC-PERPETUAL\"}" << std::endl;
        std::cout << "   4. Check console for connection/message logs" << std::endl;
        std::cout << "\n⏳ Server will run for 60 seconds for testing..." << std::endl;

        // Let the server run for 60 seconds for testing
        std::this_thread::sleep_for(std::chrono::seconds(60));

        std::cout << "\n🛑 Stopping server..." << std::endl;
        server.stop();

        std::cout << "✅ Test completed successfully!" << std::endl;

    } catch (const std::exception& e) {
        std::cout << "❌ Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}