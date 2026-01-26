//
// Created by Supradeep Chitumalla
//

#ifndef LATENCY_METRICS_H
#define LATENCY_METRICS_H

#include <chrono>
#include <atomic>
#include <vector>
#include <algorithm>
#include <cmath>

namespace deribit {

using Clock = std::chrono::high_resolution_clock;
using TimePoint = std::chrono::time_point<Clock>;
using Nanoseconds = std::chrono::nanoseconds;

struct LatencyMeasurement {
    uint64_t websocket_to_queue_ns = 0;
    uint64_t queue_to_process_ns = 0;
    uint64_t process_to_update_ns = 0;
    uint64_t total_latency_ns = 0;

    TimePoint ws_receive_time;
    TimePoint queue_push_time;
    TimePoint worker_pop_time;
    TimePoint update_complete_time;
};

// Latency statistics
struct LatencyStats {
    uint64_t min_ns = UINT64_MAX;
    uint64_t max_ns = 0;
    uint64_t avg_ns = 0;
    uint64_t p50_ns = 0;  // Median
    uint64_t p95_ns = 0;
    uint64_t p99_ns = 0;
    size_t sample_count = 0;
};

class LatencyTracker {
public:
    LatencyTracker(size_t max_samples = 10000)
        : max_samples_(max_samples), measurement_count_(0) {
        latencies_.reserve(max_samples);
    }

    // Record timestamp at each stage
    void record_websocket_receive() {
        current_measurement_.ws_receive_time = Clock::now();
    }

    void record_queue_push() {
        current_measurement_.queue_push_time = Clock::now();

        auto ws_to_queue = current_measurement_.queue_push_time - current_measurement_.ws_receive_time;
        current_measurement_.websocket_to_queue_ns =
            std::chrono::duration_cast<Nanoseconds>(ws_to_queue).count();
    }

    void record_worker_pop() {
        current_measurement_.worker_pop_time = Clock::now();

        auto queue_to_process = current_measurement_.worker_pop_time - current_measurement_.queue_push_time;
        current_measurement_.queue_to_process_ns =
            std::chrono::duration_cast<Nanoseconds>(queue_to_process).count();
    }

    void record_update_complete() {
        current_measurement_.update_complete_time = Clock::now();

        auto process_to_update = current_measurement_.update_complete_time - current_measurement_.worker_pop_time;
        current_measurement_.process_to_update_ns =
            std::chrono::duration_cast<Nanoseconds>(process_to_update).count();

        auto total = current_measurement_.update_complete_time - current_measurement_.ws_receive_time;
        current_measurement_.total_latency_ns =
            std::chrono::duration_cast<Nanoseconds>(total).count();

        // Store measurement
        add_measurement(current_measurement_);
    }

    // Get statistics
    LatencyStats get_websocket_to_queue_stats() const {
        return calculate_stats([](const LatencyMeasurement& m) { return m.websocket_to_queue_ns; });
    }

    LatencyStats get_queue_to_process_stats() const {
        return calculate_stats([](const LatencyMeasurement& m) { return m.queue_to_process_ns; });
    }

    LatencyStats get_process_to_update_stats() const {
        return calculate_stats([](const LatencyMeasurement& m) { return m.process_to_update_ns; });
    }

    LatencyStats get_total_latency_stats() const {
        return calculate_stats([](const LatencyMeasurement& m) { return m.total_latency_ns; });
    }

    void print_summary() const {
        auto ws_to_queue = get_websocket_to_queue_stats();
        auto queue_to_process = get_queue_to_process_stats();
        auto process_to_update = get_process_to_update_stats();
        auto total = get_total_latency_stats();

        std::cout << "\n" << std::string(70, '=') << std::endl;
        std::cout << "LATENCY METRICS SUMMARY" << std::endl;
        std::cout << std::string(70, '=') << std::endl;
        std::cout << "Samples: " << measurement_count_.load() << std::endl << std::endl;

        print_stage_stats("WebSocket → Queue", ws_to_queue);
        print_stage_stats("Queue → Worker", queue_to_process);
        print_stage_stats("Worker → Update", process_to_update);
        print_stage_stats("Total (End-to-End)", total);

        std::cout << std::string(70, '=') << std::endl;
    }

    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        latencies_.clear();
        measurement_count_ = 0;
    }

    size_t get_sample_count() const {
        return measurement_count_.load();
    }

private:
    void add_measurement(const LatencyMeasurement& m) {
        std::lock_guard<std::mutex> lock(mutex_);

        if (latencies_.size() < max_samples_) {
            latencies_.push_back(m);
        } else {
            // Ring buffer behavior - overwrite oldest
            latencies_[measurement_count_ % max_samples_] = m;
        }

        measurement_count_++;
    }

    template<typename Func>
    LatencyStats calculate_stats(Func extractor) const {
        std::lock_guard<std::mutex> lock(mutex_);

        if (latencies_.empty()) {
            return LatencyStats{};
        }

        std::vector<uint64_t> values;
        values.reserve(latencies_.size());

        for (const auto& m : latencies_) {
            values.push_back(extractor(m));
        }

        std::sort(values.begin(), values.end());

        LatencyStats stats;
        stats.sample_count = values.size();
        stats.min_ns = values.front();
        stats.max_ns = values.back();

        uint64_t sum = 0;
        for (auto v : values) sum += v;
        stats.avg_ns = sum / values.size();

        stats.p50_ns = values[values.size() * 50 / 100];
        stats.p95_ns = values[values.size() * 95 / 100];
        stats.p99_ns = values[values.size() * 99 / 100];

        return stats;
    }

    void print_stage_stats(const std::string& name, const LatencyStats& stats) const {
        std::cout << name << ":" << std::endl;
        std::cout << "  Min:    " << format_latency(stats.min_ns) << std::endl;
        std::cout << "  Avg:    " << format_latency(stats.avg_ns) << std::endl;
        std::cout << "  Median: " << format_latency(stats.p50_ns) << std::endl;
        std::cout << "  p95:    " << format_latency(stats.p95_ns) << std::endl;
        std::cout << "  p99:    " << format_latency(stats.p99_ns) << std::endl;
        std::cout << "  Max:    " << format_latency(stats.max_ns) << std::endl;
        std::cout << std::endl;
    }

    std::string format_latency(uint64_t ns) const {
        if (ns < 1000) {
            return std::to_string(ns) + " ns";
        } else if (ns < 1000000) {
            return std::to_string(ns / 1000) + "." + std::to_string((ns % 1000) / 100) + " μs";
        } else {
            return std::to_string(ns / 1000000) + "." + std::to_string((ns % 1000000) / 100000) + " ms";
        }
    }

    size_t max_samples_;
    std::vector<LatencyMeasurement> latencies_;
    mutable std::mutex mutex_;
    std::atomic<size_t> measurement_count_;

    // Thread-local current measurement
    thread_local static LatencyMeasurement current_measurement_;
};

// Thread-local storage for current measurement
thread_local LatencyMeasurement LatencyTracker::current_measurement_;

} // namespace deribit

#endif // LATENCY_METRICS_H