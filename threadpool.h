#ifndef threadpool_H
#define threadpool_H

#include <vector>
#include <queue>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <stdexcept>

class threadpool {
public:
    threadpool(size_t);
    template<class F, class... Args>
    auto enqueue(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    ~threadpool();
private:
    // 线程池本体
    std::vector< std::thread > workers;
    // 任务队列
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex; //类似于生产者和消费者模式
    std::condition_variable condition;//条件变量，当互斥锁解锁后可以通知
    bool stop;
};
 
// the constructor just launches some amount of workers
inline threadpool::threadpool(size_t threads) : stop(false)
{
    for(size_t i = 0;i<threads;++i)
        workers.emplace_back(
            [this]
            {
                for(;;)
                {
                    std::function<void()> task;

                    {
                        std::unique_lock<std::mutex> lock(this->queue_mutex);
                        this->condition.wait(lock,
                            [this]{ return this->stop || !this->tasks.empty(); });//将当前所有的线程阻塞
                        if(this->stop && this->tasks.empty())
                            return;
                        task = std::move(this->tasks.front());
                        this->tasks.pop();
                    }

                    task();
                }
            }
        );
}

// queue< std::function<void()> > tasks
template<class F, class... Args>
auto threadpool::enqueue(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type>
{
    using return_type = typename std::result_of<F(Args...)>::type;//返回类型的推导

    auto task = std::make_shared< std::packaged_task<return_type()> >(//pack相当于绑定到了一个future
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped threadpool");

        tasks.emplace([task](){ (*task)(); });//传递lambda函数作为函数指针
    }
    condition.notify_one();
    return res;
}

// the destructor joins all threads
inline threadpool::~threadpool()
{
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for(std::thread &worker: workers)
        worker.join();
}

#endif
