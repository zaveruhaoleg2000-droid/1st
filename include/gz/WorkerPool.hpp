#pragma once
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

namespace gz {
// 4 execution lanes TOTAL: calling/main thread + 3 persistent workers.
class WorkerPool final {
public:
    explicit WorkerPool(unsigned totalLanes = 4);
    ~WorkerPool();
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;
    void forRanges(std::size_t count, std::function<void(std::size_t,std::size_t)> work);
    unsigned totalLanes() const { return lanes_; }
private:
    void workerLoop(unsigned lane);
    unsigned lanes_ = 1;
    std::vector<std::thread> workers_;
    std::mutex mutex_;
    std::condition_variable go_, done_;
    std::function<void(std::size_t,std::size_t)> task_;
    std::size_t count_ = 0;
    std::uint64_t generation_ = 0;
    unsigned remaining_ = 0;
    bool stopping_ = false;
};
}
