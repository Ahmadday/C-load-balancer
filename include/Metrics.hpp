#pragma once
#include "Request.hpp"
#include <mutex>
#include <condition_variable>
#include <vector>
#include <string>
#include <map>

struct CompletedRequestRecord {
    int id;
    int serverId;
    double queueDelayMs;    // time spent waiting before a server picked it up
    double serviceTimeMs;   // time the server spent actually processing it
    double totalLatencyMs;  // end-to-end: arrival -> completion
};

// Thread-safe sink for completed-request records. Servers report into this
// from their own threads, so every method here must be safe to call
// concurrently from multiple threads.
class Metrics {
public:
    void recordCompletion(const Request& req, int serverId);
    void reset();

    // Blocks until at least `expectedCount` requests have been recorded.
    void waitForCompletion(long expectedCount);

    // Non-blocking snapshot of how many requests have completed so far.
    // Used by LiveDashboard to show live progress.
    long getCompletedCount() {
        std::lock_guard<std::mutex> lock(mutex_);
        return static_cast<long>(records_.size());
    }

    struct Summary {
        std::string strategyName;
        long totalRequests = 0;
        double avgQueueDelayMs = 0;
        double avgServiceTimeMs = 0;
        double avgLatencyMs = 0;
        double p95LatencyMs = 0;
        double p99LatencyMs = 0;
        double throughputRps = 0;
        double fairnessIndex = 0; // Jain's fairness index across servers, in [0,1], 1 = perfectly even
        std::map<int, long> perServerCounts;
    };

    // wallClockSeconds is the measured duration of the run, used for throughput.
    Summary computeSummary(const std::string& strategyName, double wallClockSeconds);

private:
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<CompletedRequestRecord> records_;
};
