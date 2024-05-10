// ThreadPool.hpp
#ifndef THREAD_POOL_HPP
#define THREAD_POOL_HPP

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>

class ThreadPool {
public:
    ThreadPool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~ThreadPool();

private:
    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks; 
    std::mutex queue_mutex;
    std::condition_variable condition;
    std::atomic<size_t> active_threads; // 使用原子操作来更新活跃线程数
    std::atomic<size_t> task_semaphore;  // 信号量，用来控制并发任务的数量
    bool stop;
    void work(std::size_t thread_id);
};




ThreadPool::ThreadPool(size_t threads)
    : stop(false), active_threads(0), task_semaphore(threads)
{
    for(size_t i = 0; i < threads; ++i) {
        workers.emplace_back([this, thread_id=i]() {
            for(;;) {
                std::function<void()> task;

                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this]{ return this->stop || !this->tasks.empty(); });
                    if(this->stop && this->tasks.empty())
                        return;
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                    ++active_threads;
                }

                // 等待获取信号量
                while (task_semaphore.load(std::memory_order_acquire) == 0) {
                    std::this_thread::yield(); // 如果没有可用的信号量，则让出CPU
                }
                --task_semaphore; // 获取一个信号量

                task();

                // 任务完成后，增加信号量
                ++task_semaphore;
                --active_threads;
            }
        });
    }
}

template<class F, class... Args>
auto ThreadPool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;

    auto task = std::make_shared<std::packaged_task<return_type()>>(
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        if(stop)
            throw std::runtime_error("enqueue on stopped ThreadPool");

        tasks.emplace([task](){ (*task)(); });
    }
    condition.notify_one();
    return res;
}

ThreadPool::~ThreadPool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

#endif // THREAD_POOL_HPP