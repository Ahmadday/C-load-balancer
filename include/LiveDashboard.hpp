#pragma once
#include "Server.hpp"
#include "Metrics.hpp"
#include <vector>
#include <memory>
#include <string>
#include <thread>
#include <atomic>
#include <chrono>

// Renders a self-updating terminal view of a running simulation: per-server
// active connections, queue depth, and a live bar chart of completed
// requests per server (so load skew toward one server, e.g. the
// LeastResponseTime herd effect, is visible as it happens rather than only
// in the final summary table).
//
// Runs its own thread that redraws in place using ANSI cursor-movement
// codes - it does not touch stdout from any other thread.
class LiveDashboard {
public:
    LiveDashboard(const std::vector<std::shared_ptr<Server>>& servers,
                  Metrics& metrics,
                  int totalRequests,
                  std::string strategyName);

    void start(); // spawns the render thread
    void stop();  // signals stop, joins, and draws one final 100% frame

private:
    void renderLoop();
    void renderFrame(bool final);

    const std::vector<std::shared_ptr<Server>>& servers_;
    Metrics& metrics_;
    int totalRequests_;
    std::string strategyName_;

    std::atomic<bool> running_{false};
    std::thread thread_;
    std::chrono::steady_clock::time_point startTime_;
    int linesPrintedLastFrame_ = 0;
};
