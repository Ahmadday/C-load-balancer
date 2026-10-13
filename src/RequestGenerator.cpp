#include "RequestGenerator.hpp"
#include <random>
#include <thread>
#include <algorithm>

RequestGenerator::RequestGenerator(LoadBalancer& lb, GeneratorConfig config)
    : lb_(lb), config_(config) {}

void RequestGenerator::run() {
    std::mt19937 rng(config_.randomSeed);
    std::exponential_distribution<double> interArrival(config_.arrivalRatePerSec);
    std::normal_distribution<double> workDist(config_.meanWorkMs, config_.workStdDevMs);
    std::uniform_int_distribution<int> clientDist(0, config_.numClients - 1);

    for (int i = 0; i < config_.totalRequests; ++i) {
        double waitSec = interArrival(rng);
        std::this_thread::sleep_for(std::chrono::duration<double>(waitSec));

        Request req;
        req.id = i;
        req.clientId = clientDist(rng);
        req.workMs = std::max(1, static_cast<int>(workDist(rng)));
        req.arrivalTime = Clock::now();

        lb_.dispatch(std::move(req));
    }
}
