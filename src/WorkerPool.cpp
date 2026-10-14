#include "gz/WorkerPool.hpp"
#include <algorithm>
#include <utility>

namespace gz {
WorkerPool::WorkerPool(unsigned requested): lanes_(std::clamp(requested, 1u, 4u)) {
    workers_.reserve(lanes_ - 1);
    for (unsigned lane=1; lane<lanes_; ++lane)
        workers_.emplace_back([this,lane]{workerLoop(lane);});
}
WorkerPool::~WorkerPool() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        stopping_ = true;
        ++generation_;
    }
    go_.notify_all();
    for (auto& worker: workers_) worker.join();
}
void WorkerPool::workerLoop(unsigned lane) {
    std::uint64_t observed = 0;
    while (true) {
        std::unique_lock<std::mutex> lock(mutex_);
        go_.wait(lock, [&]{return stopping_ || generation_ != observed;});
        if (stopping_) return;
        observed = generation_;
        auto count = count_;
        // The shared task stays unchanged until ALL workers finish.
        auto work = task_;
        lock.unlock();
        const auto begin = count * lane / lanes_;
        const auto end = count * (lane+1) / lanes_;
        if (begin < end) work(begin,end);
        lock.lock();
        if (--remaining_ == 0) done_.notify_one();
    }
}
void WorkerPool::forRanges(std::size_t count,
                           std::function<void(std::size_t,std::size_t)> work) {
    if (count == 0) return;
    if (lanes_ == 1 || count < 128) {
        work(0,count);
        return;
    }
    {
        std::lock_guard<std::mutex> lock(mutex_);
        task_ = std::move(work);
        count_ = count;
        remaining_ = static_cast<unsigned>(workers_.size());
        ++generation_;
    }
    go_.notify_all();
    task_(0, count / lanes_); // main thread is lane 0
    std::unique_lock<std::mutex> lock(mutex_);
    done_.wait(lock, [&]{return remaining_ == 0;});
    task_ = nullptr;
}
}
