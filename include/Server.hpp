#pragma once
#include "Request.hpp"
#include "Metrics.hpp"
#include <thread>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <atomic>

// Simulates a single backend server. Runs its own worker thread that pulls
// requests off its queue and "processes" them by sleeping for a scaled
// duration, then reports the completion to the shared Metrics collector.
class Server {
public:
    Server(int id, double speedMultiplier, Metrics& metrics);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();
    void stop(); // signals the worker to finish draining and joins the thread

    void enqueue(Request req);

    int getId() const { return id_; }
    int getActiveConnections() const { return activeConnections_.load(); }
    long getTotalProcessed() const { return totalProcessed_.load(); }
    double getWeight() const { return speedMultiplier_; }

    // Snapshot of how many requests are currently sitting in this server's
    // queue (not counting the one actively being processed). Used only for
    // display purposes (LiveDashboard) - takes the queue lock briefly.
    size_t getQueueLength() {
        std::lock_guard<std::mutex> lock(mutex_);
        return queue_.size();
    }

    // Exponential moving average of recent service times, in ms.
    // Used by the LeastResponseTime strategy. Returns 0 until the first
    // request completes (treated as "unknown, try me" by that strategy).
    double getAvgResponseTimeMs() const { return emaResponseMs_.load(); }

private:
    void run();

    int id_;
    double speedMultiplier_; // >1 = faster (less time per unit of work)
    Metrics& metrics_;

    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<Request> queue_;
    std::atomic<bool> running_{false};
    std::atomic<bool> draining_{false}; // stop accepting new work, finish what's queued
    std::atomic<int> activeConnections_{0};
    std::atomic<long> totalProcessed_{0};
    std::atomic<double> emaResponseMs_{0.0};

    static constexpr double EMA_ALPHA = 0.3;
};
