# Low-Latency Trading System

A C++ trading system that connects to Deribit exchange via WebSocket and processes market data with ~107μs average latency.

Built to learn about lock-free data structures, concurrent programming, and low-latency systems.

## What It Does

- Streams real-time orderbook data from Deribit (cryptocurrency exchange)
- Processes updates using lock-free queue and 4 worker threads
- Places/cancels/modifies orders via REST API
- Tracks positions and measures processing latency

## Architecture

```
WebSocket → Lock-Free Queue (65k capacity) → 4 Workers → Per-Symbol Orderbooks
                                                              ↓
                                                        Order Manager
```

**Key Design Choices:**
- Lock-free circular buffer with cache-line alignment (prevents false sharing)
- Move semantics to avoid JSON copy overhead 
- Per-symbol mutexes (BTC and ETH can update in parallel)
- Atomic counters for dropped messages and latency tracking

**Measured Performance:**
- Average latency: ~107 microseconds (orderbook update)
- Breakdown: JSON parsing (60-80μs) + map update (20-30μs) + mutex (5-10μs)

## Project Structure

```
trading-system/
├── CMakeLists.txt
├── config.json              # replace your API credentials create a file named config.json
├── main.cpp                 
├── README.md
├── include/
│   ├── authentication.hpp
│   ├── buffer.hpp           # Lock-free circular buffer
│   ├── config.hpp
│   ├── config_loader.hpp
│   ├── deribit_client.hpp   # WebSocket client
│   ├── market_data.hpp      # Orderbook manager + latency tracking
│   └── order.hpp            # REST API for orders
├── src/
│   ├── Authentication.cpp
│   ├── deribit_client.cpp
│   ├── market_data.cpp
│   └── order.cpp
```

## Build & Run

### Prerequisites

**macOS:**
```bash
brew install cmake openssl cpprestsdk jsoncpp websocketpp
```

**Ubuntu/Debian:**
```bash
sudo apt-get install cmake build-essential libssl-dev libcpprest-dev \
    libjsoncpp-dev libwebsocketpp-dev
```

### Get Deribit API Credentials

1. Go to https://test.deribit.com (testnet!)
2. Create account
3. Account → API → Create new key
4. Copy `client_id` and `client_secret`

### Build

```bash
git clone https://github.com/such283/trading-system
cd trading-system

# Create config file
cat > config.json << EOF
{
  "client_id": "YOUR_CLIENT_ID",
  "client_secret": "YOUR_CLIENT_SECRET"
}
EOF

# Build 
mkdir build && cd build
cmake ..
make

# Run
./trading
```

## Usage

```
==================================================
DERIBIT TRADING INTERFACE
==================================================
Available commands:
1. Place buy order
2. Place sell order
3. Cancel order
4. Modify order
5. Get positions
6. Get orderbook
7. View latency metrics
8. Subscribe to symbol
9. Exit
==================================================
```

**Typical workflow:**

1. Subscribe to market data: `8 → BTC-PERPETUAL`
2. View orderbook: `6 → BTC-PERPETUAL`
3. Check latency: `7` (shows avg processing time)
4. Place order: `1 → BTC-PERPETUAL → 10 → limit → 87000 → async`
5. Check positions: `5 → BTC → future`

## Known Issues & Limitations
### 1. Performance Bottlenecks
- **JSON parsing (60-80μs)**: Using jsoncpp which is slow. Could use simdjson (10x faster)
- **std::map for orderbook**: O(log n) operations. Could use fixed arrays for better cache locality
- **Copies on get_orderbook()**: Returns by value. Could use shared_ptr with RCU

### 2. Things I'd Fix for Production
- Replace jsoncpp with simdjson
- Lock-free orderbook (no mutexes at all)
- CPU pinning for worker threads
- Pre-allocated memory pools
- Latency histograms (not just average)

## What I Learned

**Memory Ordering:**
Used `memory_order_relaxed` for counters (don't need synchronization) vs `memory_order_acquire/release` for queue operations (need ordering guarantees).

**Cache-Line Alignment:**
```cpp
alignas(64) std::atomic<size_t> write_pos_;  // Separate cache line
alignas(64) std::atomic<size_t> read_pos_;   // Prevents false sharing
```
Without this, producer and consumer thrash the same cache line. ~2x performance difference.

**Move Semantics:**
Changed from copying Json::Value (5-20μs) to moving (0.1μs). Simple optimization, huge impact.

**Concurrency is Hard:**
Found race condition only after careful code review. ThreadSanitizer helps but doesn't catch everything.

## Technical Details

### Lock-Free Queue
```cpp
template<typename T>
class Buffer {
    alignas(64) std::atomic<size_t> write_pos_;
    alignas(64) std::atomic<size_t> read_pos_;
    std::vector<T> buffer_;
    
    bool push(T&& item) {
        size_t current_write = write_pos_.load(std::memory_order_relaxed);
        // ... check if full ...
        buffer_[current_write] = std::move(item);
        write_pos_.store(next_write, std::memory_order_release);
    }
};
```

### Latency Tracking
```cpp
auto start = std::chrono::high_resolution_clock::now();
this->on_orderbook_update(symbol, json);
auto end = std::chrono::high_resolution_clock::now();
auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
```

## Why This Project?

I wanted to understand:
- How lock-free data structures actually work in practice
- When to use atomics vs mutexes
- What "low-latency" really means (turns out 100μs is fast for this use case)
- How to measure and optimize performance

## Author

Supradeep Chitumalla  
LinkedIn: https://www.linkedin.com/in/supradeep-c/