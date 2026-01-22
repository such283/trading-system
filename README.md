# Trading System

A low-latency C++ trading system for processing real-time market data from Deribit cryptocurrency exchange.

## Architecture

**Core Design Principles:**
- Lock-free data structures to minimize contention
- Multi-threaded worker pool for parallel order book processing
- Separation of I/O (WebSocket) and compute (order book updates)
- Cache-aware memory layout to avoid false sharing

### System Components

```
WebSocket Client → Lock-Free Buffer → Worker Pool → Order Book Manager
                                   ↓
                            Order Manager (HTTP)
```

**Message Flow:**
1. WebSocket receives market data (50K+ messages/second)
2. Messages enqueued to lock-free circular buffer
3. Worker threads process updates in parallel
4. Order book maintains bid/ask state per instrument

## Performance Characteristics

**Throughput:** 50,000+ messages/second across Spot, Futures, Options  
**Latency:** Sub-millisecond order book updates (when not I/O bound)  
**Concurrency:** 4 worker threads (configurable)  
**Buffer Capacity:** 65,536 messages (lock-free queue)

## Technical Highlights

### 1. Lock-Free Circular Buffer

```cpp
template<typename T>
class Buffer {
private:
    alignas(64) std::atomic<size_t> head_;  // Cache line alignment
    alignas(64) std::atomic<size_t> tail_;  // Prevents false sharing
    // ...
};
```

**Key Features:**
- **Acquire-release memory ordering** for correctness without sequential consistency overhead
- **Cache line alignment** (`alignas(64)`) to prevent false sharing between producer/consumer
- **Lock-free** single-producer, multi-consumer design

**Design Choice:** Used `acquire-release` instead of `seq_cst` because we only need ordering guarantees between producer and consumer threads, not global ordering across all threads.

### 2. Multi-Threaded Order Book Processing

**Architecture:**
- Per-symbol mutexes to allow parallel updates to different instruments
- Worker pool pattern: dequeue → parse → update order book
- Incremental updates (deltas) + periodic snapshots for bandwidth efficiency

**Handles:**
- Snapshot messages (full order book state)
- Incremental updates (bid/ask changes)
- Out-of-order message detection via timestamps

### 3. WebSocket Integration

- Asynchronous WebSocket client using `websocketpp`
- JSON parsing with `jsoncpp`
- Subscription management for multiple instruments
- Automatic reconnection handling

## Known Limitations & Future Work

### Current Issues

**1. Race Condition in Mutex Management** (Documented in `market_data.cpp:13-15`)
```cpp
// Bug: accessing orderbook_mutexes_ without holding mutexes_map_mutex_
// This can cause crashes under high contention
```
**Fix:** Acquire map mutex before accessing symbol mutex, or replace with lock-free hash map.

**2. Order Book Copy Overhead**
- `get_orderbook()` returns by value, copying entire `std::map<double, double>`
- For deep order books (100+ levels), this is expensive
- **Solution:** Use `shared_ptr` with RCU pattern or return const reference

**3. std::map for Price Levels**
- Red-black tree has O(log n) insert/lookup
- Better: `std::vector` with binary search (cache-friendly) or custom array-based levels

### Production TODOs

- [ ] Lock-free hash map for per-symbol mutexes
- [ ] Zero-copy order book access (shared_ptr + RCU)
- [ ] Replace `std::map` with array-based price levels
- [ ] Add latency monitoring (RDTSC timestamps, p99 metrics)
- [ ] Backpressure handling when buffer is full


## What I Learned

**Memory Ordering:**  
Understanding when `acquire-release` is sufficient vs. when you need `seq_cst`. The performance difference matters at high message rates.

**False Sharing:**  
Cache line alignment (`alignas(64)`) on atomic variables prevents CPU cache thrashing when producer/consumer run on different cores. Measured ~2x throughput improvement.

**Latency vs. Throughput Trade-offs:**
- Lock-free structures reduce tail latency but complicate the code
- Worker pool increases throughput but adds queue latency
- Copying order books is safe but slow; zero-copy is fast but needs careful lifetime management

**Debugging Concurrent Systems:**
- ThreadSanitizer caught the mutex race condition
- Understanding happens-before relationships is critical
- Reproducing race conditions requires stress testing under load

## Build & Run

**Dependencies:**
```bash
# Ubuntu/Debian
sudo apt-get install libwebsocketpp-dev libjsoncpp-dev libcpprest-dev libssl-dev

# macOS
brew install websocketpp jsoncpp cpprestsdk openssl
```

**Build:**
```bash
mkdir build && cd build
cmake ..
make -j4
```

**Run:**
```bash
./deribit_trader
```

**Configuration:**  
Edit `config.json` to set API credentials and subscriptions.


## Why I did this ?

I wanted to understand low-latency systems at a fundamental level:
- How does memory ordering affect correctness in lock-free code?
- What are the real bottlenecks in concurrent data structures?
- When should you sacrifice simplicity for performance?

This isn't production-ready HFT code (see limitations above), but it taught me how to think about:
- Cache behavior and memory layout
- Lock-free algorithm correctness
- Performance profiling under load
- Debugging race conditions



**Author:** Supradeep Chitumalla  
**Linkedin:** https://www.linkedin.com/in/supradeep-c/
