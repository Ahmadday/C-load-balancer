#include "Strategies.hpp"
#include <limits>
#include <functional>

// ---------------- Round Robin ----------------

std::shared_ptr<Server> RoundRobinStrategy::selectServer(
    const std::vector<std::shared_ptr<Server>>& servers, const Request&) {
    size_t idx = index_.fetch_add(1) % servers.size();
    return servers[idx];
}

// ---------------- Weighted Round Robin ----------------

WeightedRoundRobinStrategy::WeightedRoundRobinStrategy(std::vector<double> weights)
    : weights_(std::move(weights)), currentWeights_(weights_.size(), 0.0) {}

std::shared_ptr<Server> WeightedRoundRobinStrategy::selectServer(
    const std::vector<std::shared_ptr<Server>>& servers, const Request&) {
    std::lock_guard<std::mutex> lock(mutex_);

    double total = 0.0;
    int bestIdx = 0;
    for (size_t i = 0; i < servers.size(); ++i) {
        currentWeights_[i] += weights_[i];
        total += weights_[i];
        if (currentWeights_[i] > currentWeights_[bestIdx]) bestIdx = static_cast<int>(i);
    }
    currentWeights_[bestIdx] -= total;
    return servers[bestIdx];
}

// ---------------- Least Connections ----------------

std::shared_ptr<Server> LeastConnectionsStrategy::selectServer(
    const std::vector<std::shared_ptr<Server>>& servers, const Request&) {
    std::shared_ptr<Server> best = servers[0];
    int bestCount = best->getActiveConnections();
    for (const auto& s : servers) {
        int c = s->getActiveConnections();
        if (c < bestCount) {
            bestCount = c;
            best = s;
        }
    }
    return best;
}

// ---------------- Least Response Time ----------------

std::shared_ptr<Server> LeastResponseTimeStrategy::selectServer(
    const std::vector<std::shared_ptr<Server>>& servers, const Request&) {
    std::shared_ptr<Server> best = servers[0];
    double bestTime = best->getAvgResponseTimeMs();
    for (const auto& s : servers) {
        double t = s->getAvgResponseTimeMs();
        // 0 means "no data yet" - treat as most attractive so every server
        // gets tried at least once early on.
        if (t == 0.0) return s;
        if (t < bestTime) {
            bestTime = t;
            best = s;
        }
    }
    return best;
}

// ---------------- IP Hash ----------------

std::shared_ptr<Server> IPHashStrategy::selectServer(
    const std::vector<std::shared_ptr<Server>>& servers, const Request& req) {
    size_t idx = std::hash<int>{}(req.clientId) % servers.size();
    return servers[idx];
}
