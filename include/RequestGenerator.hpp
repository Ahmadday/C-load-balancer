#pragma once
#include "LoadBalancer.hpp"

struct GeneratorConfig {
    int totalRequests = 2000;
    double arrivalRatePerSec = 150.0; // lambda for the Poisson arrival process
    double meanWorkMs = 40.0;         // mean base service cost, before per-server speed scaling
    double workStdDevMs = 12.0;
    int numClients = 50;              // distinct simulated client/session ids, for IP-hash affinity
    unsigned int randomSeed = 42;     // fixed seed so every strategy sees the *same* traffic pattern
};

// Produces requests on a Poisson process (exponentially distributed
// inter-arrival times, the standard model for "independent random arrivals"
// like real user traffic) and dispatches them to the load balancer.
class RequestGenerator {
public:
    RequestGenerator(LoadBalancer& lb, GeneratorConfig config);

    // Blocking: generates and dispatches totalRequests, then returns.
    void run();

private:
    LoadBalancer& lb_;
    GeneratorConfig config_;
};
