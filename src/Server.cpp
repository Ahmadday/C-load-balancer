#include "Server.hpp"
#include <algorithm>

Server::Server(int id, double speedMultiplier, Metrics& metrics)
    : id_(id), speedMultiplier_(speedMultiplier), metrics_(metrics) {}

Server::~Server() {
    stop();
}

void Server::start() {
    running_ = true;
    thread_ = std::thread(&Server::run, this);
}

void Server::stop() {
    running_ = false;
    cv_.notify_all();
    if (thread_.joinable()) thread_.join();
}

void Server::enqueue(Request req) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push(std::move(req));
    }
    cv_.notify_one();
}

void Server::run() {
    while (true) {
        Request req;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [&] { return !queue_.empty() || !running_; });
            if (!running_ && queue_.empty()) return;
            req = std::move(queue_.front());
            queue_.pop();
        }

        activeConnections_.fetch_add(1);
        req.dispatchTime = Clock::now();

        // Scale the base work cost by this server's speed. Faster servers
        // (multiplier > 1) finish the same request quicker.
        double scaledMs = std::max(1.0, req.workMs / speedMultiplier_);
        std::this_thread::sleep_for(std::chrono::duration<double, std::milli>(scaledMs));

        req.completionTime = Clock::now();

        double serviceMs = std::chrono::duration<double, std::milli>(req.completionTime - req.dispatchTime).count();
        double prevEma = emaResponseMs_.load();
        double newEma = (prevEma == 0.0) ? serviceMs : (EMA_ALPHA * serviceMs + (1 - EMA_ALPHA) * prevEma);
        emaResponseMs_.store(newEma);

        metrics_.recordCompletion(req, id_);

        totalProcessed_.fetch_add(1);
        activeConnections_.fetch_sub(1);
    }
}
