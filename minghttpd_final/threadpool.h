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
    // �̳߳ر���
    std::vector< std::thread > workers;
    // �������
    std::queue< std::function<void()> > tasks;
    
    // synchronization
    std::mutex queue_mutex; //�����������ߺ�������ģʽ
    std::condition_variable condition;//���������������������������֪ͨ
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
                            [this]{ return this->stop || !this->tasks.empty(); });//����ǰ���е��߳�����
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
    using return_type = typename std::result_of<F(Args...)>::type;//�������͵��Ƶ�

    auto task = std::make_shared< std::packaged_task<return_type()> >(//pack�൱�ڰ󶨵���һ��future
            std::bind(std::forward<F>(f), std::forward<Args>(args)...)
        );
        
    std::future<return_type> res = task->get_future();
    {
        std::unique_lock<std::mutex> lock(queue_mutex);

        // don't allow enqueueing after stopping the pool
        if(stop)
            throw std::runtime_error("enqueue on stopped threadpool");

        tasks.emplace([task](){ (*task)(); });//����lambda������Ϊ����ָ��
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
