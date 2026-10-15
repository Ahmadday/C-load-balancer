#include "LiveDashboard.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <string>

LiveDashboard::LiveDashboard(const std::vector<std::shared_ptr<Server>>& servers,
                              Metrics& metrics,
                              int totalRequests,
                              std::string strategyName)
    : servers_(servers), metrics_(metrics), totalRequests_(totalRequests),
      strategyName_(std::move(strategyName)) {}

static std::string makeBar(double fraction, int width) {
    fraction = std::max(0.0, std::min(1.0, fraction));
    const int filled = static_cast<int>(fraction * width);
    const int empty = std::max(0, width - filled);
    std::string s = "[";
    s += std::string(static_cast<std::size_t>(filled), '#');
    s += std::string(static_cast<std::size_t>(empty), '.');
    s += "]";
    return s;
}

void LiveDashboard::start() {
    startTime_ = std::chrono::steady_clock::now();
    running_ = true;
    thread_ = std::thread(&LiveDashboard::renderLoop, this);
}

void LiveDashboard::stop() {
    running_ = false;
    if (thread_.joinable()) thread_.join();
    renderFrame(true);  // one final 100%-accurate frame
    std::cout << "\n";
}

void LiveDashboard::renderLoop() {
    while (running_) {
        renderFrame(false);
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
    }
}

void LiveDashboard::renderFrame(bool final) {
    std::ostringstream out;

    // Move cursor back up to the top of the dashboard region before
    // redrawing, so the whole thing updates in place instead of scrolling.
    if (linesPrintedLastFrame_ > 0) {
        out << "\033[" << linesPrintedLastFrame_ << "A";
    }

    long completed = metrics_.getCompletedCount();
    double elapsed = std::chrono::duration<double>(std::chrono::steady_clock::now() - startTime_).count();
    double pct = totalRequests_ > 0 ? (100.0 * completed / totalRequests_) : 0.0;
    double rps = elapsed > 0 ? completed / elapsed : 0.0;

    int lineCount = 0;
    auto line = [&](const std::string& s) {
        // \033[K clears any leftover characters from a longer previous frame.
        out << s << "\033[K\n";
        lineCount++;
    };

    line(std::string(final ? "Live [DONE]: " : "Live: ") + strategyName_);

    {
        std::ostringstream l;
        l << "Progress " << makeBar(pct / 100.0, 30) << "  "
          << completed << "/" << totalRequests_
          << " (" << std::fixed << std::setprecision(1) << pct << "%)   "
          << std::setprecision(1) << elapsed << "s   "
          << std::setprecision(1) << rps << " req/s";
        line(l.str());
    }

    line("");

    {
        std::ostringstream l;
        l << std::left << std::setw(5) << "ID" << std::setw(7) << "Speed"
          << std::setw(8) << "Active" << std::setw(7) << "Queue" << std::setw(8) << "Done"
          << "Load (relative to busiest server)";
        line(l.str());
    }

    long maxDone = 1;
    if (!servers_.empty()) {
        for (const auto& s : servers_) {
            maxDone = std::max(maxDone, s->getTotalProcessed());
        }
    }

    for (const auto& s : servers_) {
        long done = s->getTotalProcessed();
        double frac = static_cast<double>(done) / static_cast<double>(maxDone);

        std::ostringstream weightStr;
        weightStr << std::fixed << std::setprecision(1) << s->getWeight() << "x";

        std::ostringstream l;
        l << std::left << std::setw(5) << ("S" + std::to_string(s->getId()))
          << std::setw(7) << weightStr.str()
          << std::setw(8) << s->getActiveConnections()
          << std::setw(7) << s->getQueueLength()
          << std::setw(8) << done
          << makeBar(frac, 25);
        line(l.str());
    }

    line("");

    linesPrintedLastFrame_ = lineCount;
    std::cout << out.str() << std::flush;
}
