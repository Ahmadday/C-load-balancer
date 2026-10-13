#pragma once
#include "Strategies.hpp"
#include "Server.hpp"
#include <vector>
#include <memory>
#include <string>

// Ties a set of servers to a chosen strategy. Dispatching just delegates
// server selection to the strategy and hands the request off to that
// server's own queue/thread.
class LoadBalancer {
public:
    LoadBalancer(std::vector<std::shared_ptr<Server>> servers, std::unique_ptr<IStrategy> strategy);

    void dispatch(Request req);

    std::string strategyName() const { return strategy_->name(); }
    const std::vector<std::shared_ptr<Server>>& getServers() const { return servers_; }

private:
    std::vector<std::shared_ptr<Server>> servers_;
    std::unique_ptr<IStrategy> strategy_;
};
