#pragma once
#include <chrono>

using Clock = std::chrono::steady_clock;
using TimePoint = Clock::time_point;

// A single unit of "traffic" flowing through the simulated load balancer.
struct Request {
    int id = 0;
    int clientId = 0;     // simulated client/session id (used by IP-hash affinity)
    int workMs = 0;       // simulated base processing cost, in milliseconds

    TimePoint arrivalTime;     // when the generator created the request
    TimePoint dispatchTime;    // when a server actually began processing it
    TimePoint completionTime;  // when the server finished it
};
