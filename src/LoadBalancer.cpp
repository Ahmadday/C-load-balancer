#include "LoadBalancer.hpp"

LoadBalancer::LoadBalancer(std::vector<std::shared_ptr<Server>> servers, std::unique_ptr<IStrategy> strategy)
    : servers_(std::move(servers)), strategy_(std::move(strategy)) {}

void LoadBalancer::dispatch(Request req) {
    auto server = strategy_->selectServer(servers_, req);
    server->enqueue(std::move(req));
}
