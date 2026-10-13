#pragma once
#include "Server.hpp"
#include "Request.hpp"
#include <vector>
#include <memory>
#include <string>
#include <atomic>
#include <mutex>

class IStrategy {
public:
    virtual ~IStrategy() = default;
    virtual std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers, const Request& req) = 0;
    virtual std::string name() const = 0;
};

// Cycles through servers in fixed order, ignoring load or capacity.
class RoundRobinStrategy : public IStrategy {
public:
    std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers, const Request& req) override;
    std::string name() const override { return "Round Robin"; }

private:
    std::atomic<size_t> index_{0};
};

// Smooth weighted round robin (the algorithm nginx uses): servers with
// higher weight get picked proportionally more often, but selections are
// spread out evenly over time rather than bursting.
class WeightedRoundRobinStrategy : public IStrategy {
public:
    explicit WeightedRoundRobinStrategy(std::vector<double> weights);
    std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers, const Request& req) override;
    std::string name() const override { return "Weighted Round Robin"; }

private:
    std::mutex mutex_;
    std::vector<double> weights_;
    std::vector<double> currentWeights_;
};

// Always sends the next request to whichever server currently has the
// fewest in-flight requests.
class LeastConnectionsStrategy : public IStrategy {
public:
    std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers, const Request& req) override;
    std::string name() const override { return "Least Connections"; }
};

// Sends the next request to whichever server has the lowest recent average
// response time (exponential moving average tracked on the Server itself).
class LeastResponseTimeStrategy : public IStrategy {
public:
    std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers, const Request& req) override;
    std::string name() const override { return "Least Response Time"; }
};

// Hashes the client/session id to consistently route the same client to the
// same server (session affinity / "sticky sessions").
class IPHashStrategy : public IStrategy {
public:
    std::shared_ptr<Server> selectServer(
        const std::vector<std::shared_ptr<Server>>& servers, const Request& req) override;
    std::string name() const override { return "IP Hash (Session Affinity)"; }
};
