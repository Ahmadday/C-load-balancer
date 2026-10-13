#include "LoadBalancer.hpp"
#include "RequestGenerator.hpp"
#include "Metrics.hpp"
#include "Server.hpp"
#include "LiveDashboard.hpp"

#include <iostream>
#include <iomanip>
#include <fstream>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include <string>

// Heterogeneous fleet: some servers are faster than others, so strategies
// that account for load (least-connections, least-response-time, weighted
// round robin) have a genuine advantage over plain round robin.
static std::vector<double> serverSpeedMultipliers() {
    return {1.0, 1.0, 2.0, 0.5, 1.5};
}

static Metrics::Summary runSimulation(std::unique_ptr<IStrategy> strategy, const GeneratorConfig& genConfig,
                                       bool live) {
    Metrics metrics;
    std::vector<std::shared_ptr<Server>> servers;
    auto speeds = serverSpeedMultipliers();
    for (size_t i = 0; i < speeds.size(); ++i) {
        auto server = std::make_shared<Server>(static_cast<int>(i), speeds[i], metrics);
        server->start();
        servers.push_back(server);
    }

    LoadBalancer lb(servers, std::move(strategy));
    std::string name = lb.strategyName();

    RequestGenerator generator(lb, genConfig);

    std::unique_ptr<LiveDashboard> dashboard;
    if (live) {
        dashboard = std::make_unique<LiveDashboard>(servers, metrics, genConfig.totalRequests, name);
        dashboard->start();
    }

    auto startTime = std::chrono::steady_clock::now();
    generator.run();                                   // blocks until all requests dispatched
    metrics.waitForCompletion(genConfig.totalRequests); // blocks until all requests finish processing
    auto endTime = std::chrono::steady_clock::now();

    if (dashboard) dashboard->stop(); // draws one final 100% frame

    for (auto& s : servers) s->stop();

    double wallClockSeconds = std::chrono::duration<double>(endTime - startTime).count();
    return metrics.computeSummary(name, wallClockSeconds);
}

static void printSummaryTable(const std::vector<Metrics::Summary>& summaries) {
    std::cout << "\n=== Strategy Comparison ===\n\n";
    std::cout << std::left
               << std::setw(24) << "Strategy"
               << std::right
               << std::setw(10) << "Avg Lat"
               << std::setw(10) << "P95 Lat"
               << std::setw(10) << "P99 Lat"
               << std::setw(10) << "AvgQueue"
               << std::setw(10) << "Tput/s"
               << std::setw(12) << "Fairness"
               << "\n";
    std::cout << std::string(86, '-') << "\n";

    for (const auto& s : summaries) {
        std::cout << std::left << std::setw(24) << s.strategyName
                   << std::right << std::fixed << std::setprecision(2)
                   << std::setw(10) << s.avgLatencyMs
                   << std::setw(10) << s.p95LatencyMs
                   << std::setw(10) << s.p99LatencyMs
                   << std::setw(10) << s.avgQueueDelayMs
                   << std::setw(10) << s.throughputRps
                   << std::setw(12) << s.fairnessIndex
                   << "\n";
    }

    std::cout << "\nPer-server request distribution:\n";
    for (const auto& s : summaries) {
        std::cout << "  " << std::left << std::setw(28) << s.strategyName;
        for (const auto& [serverId, count] : s.perServerCounts) {
            std::cout << "S" << serverId << "=" << count << "  ";
        }
        std::cout << "\n";
    }
}

static void writeCsv(const std::vector<Metrics::Summary>& summaries, const std::string& path) {
    std::ofstream out(path);
    out << "strategy,total_requests,avg_queue_delay_ms,avg_service_time_ms,avg_latency_ms,"
           "p95_latency_ms,p99_latency_ms,throughput_rps,fairness_index\n";
    for (const auto& s : summaries) {
        out << s.strategyName << "," << s.totalRequests << ","
            << s.avgQueueDelayMs << "," << s.avgServiceTimeMs << "," << s.avgLatencyMs << ","
            << s.p95LatencyMs << "," << s.p99LatencyMs << "," << s.throughputRps << ","
            << s.fairnessIndex << "\n";
    }
    std::cout << "\nWrote " << path << "\n";
}

int main(int argc, char** argv) {
    GeneratorConfig genConfig;
    bool live = false;

    // Simple manual arg parsing: --live can appear anywhere; the first two
    // remaining args are totalRequests and arrivalRatePerSec.
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--live") live = true;
        else positional.push_back(arg);
    }
    if (positional.size() > 0) genConfig.totalRequests = std::atoi(positional[0].c_str());
    if (positional.size() > 1) genConfig.arrivalRatePerSec = std::atof(positional[1].c_str());

    std::cout << "Load Balancer Simulation\n";
    std::cout << "  requests/strategy: " << genConfig.totalRequests << "\n";
    std::cout << "  arrival rate:      " << genConfig.arrivalRatePerSec << " req/s (Poisson)\n";
    std::cout << "  server speeds:     [1.0, 1.0, 2.0, 0.5, 1.5]\n";
    if (live) std::cout << "  mode:              live dashboard\n";

    std::vector<Metrics::Summary> summaries;

    auto runNamed = [&](const std::string& label, std::unique_ptr<IStrategy> strategy) {
        if (!live) std::cout << "\nRunning " << label << "...\n";
        else std::cout << "\n";
        summaries.push_back(runSimulation(std::move(strategy), genConfig, live));
        if (live) std::this_thread::sleep_for(std::chrono::milliseconds(700)); // pause to let you read the final frame
    };

    runNamed("Round Robin", std::make_unique<RoundRobinStrategy>());
    runNamed("Weighted Round Robin", std::make_unique<WeightedRoundRobinStrategy>(serverSpeedMultipliers()));
    runNamed("Least Connections", std::make_unique<LeastConnectionsStrategy>());
    runNamed("Least Response Time", std::make_unique<LeastResponseTimeStrategy>());
    runNamed("IP Hash", std::make_unique<IPHashStrategy>());

    printSummaryTable(summaries);
    writeCsv(summaries, "results.csv");

    return 0;
}
