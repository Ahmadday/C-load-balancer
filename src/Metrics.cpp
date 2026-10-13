#include "Metrics.hpp"
#include <algorithm>
#include <cmath>

void Metrics::recordCompletion(const Request& req, int serverId) {
    double queueDelayMs = std::chrono::duration<double, std::milli>(req.dispatchTime - req.arrivalTime).count();
    double serviceTimeMs = std::chrono::duration<double, std::milli>(req.completionTime - req.dispatchTime).count();
    double totalLatencyMs = std::chrono::duration<double, std::milli>(req.completionTime - req.arrivalTime).count();

    std::lock_guard<std::mutex> lock(mutex_);
    records_.push_back({req.id, serverId, queueDelayMs, serviceTimeMs, totalLatencyMs});
    cv_.notify_all();
}

void Metrics::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    records_.clear();
}

void Metrics::waitForCompletion(long expectedCount) {
    std::unique_lock<std::mutex> lock(mutex_);
    cv_.wait(lock, [&] { return static_cast<long>(records_.size()) >= expectedCount; });
}

static double percentile(std::vector<double>& sorted, double p) {
    if (sorted.empty()) return 0.0;
    double idx = p * (sorted.size() - 1);
    size_t lo = static_cast<size_t>(std::floor(idx));
    size_t hi = static_cast<size_t>(std::ceil(idx));
    if (lo == hi) return sorted[lo];
    double frac = idx - lo;
    return sorted[lo] * (1 - frac) + sorted[hi] * frac;
}

Metrics::Summary Metrics::computeSummary(const std::string& strategyName, double wallClockSeconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    Summary s;
    s.strategyName = strategyName;
    s.totalRequests = static_cast<long>(records_.size());
    if (s.totalRequests == 0) return s;

    double sumQueue = 0, sumService = 0, sumLatency = 0;
    std::vector<double> latencies;
    latencies.reserve(records_.size());

    for (const auto& r : records_) {
        sumQueue += r.queueDelayMs;
        sumService += r.serviceTimeMs;
        sumLatency += r.totalLatencyMs;
        latencies.push_back(r.totalLatencyMs);
        s.perServerCounts[r.serverId]++;
    }

    s.avgQueueDelayMs = sumQueue / s.totalRequests;
    s.avgServiceTimeMs = sumService / s.totalRequests;
    s.avgLatencyMs = sumLatency / s.totalRequests;

    std::sort(latencies.begin(), latencies.end());
    s.p95LatencyMs = percentile(latencies, 0.95);
    s.p99LatencyMs = percentile(latencies, 0.99);

    s.throughputRps = wallClockSeconds > 0 ? s.totalRequests / wallClockSeconds : 0.0;

    // Jain's fairness index over per-server request counts:
    // (sum xi)^2 / (n * sum xi^2). 1.0 = perfectly even distribution.
    double sumX = 0, sumX2 = 0;
    for (const auto& [serverId, count] : s.perServerCounts) {
        sumX += count;
        sumX2 += static_cast<double>(count) * count;
    }
    size_t n = s.perServerCounts.size();
    s.fairnessIndex = (n > 0 && sumX2 > 0) ? (sumX * sumX) / (n * sumX2) : 0.0;

    return s;
}
