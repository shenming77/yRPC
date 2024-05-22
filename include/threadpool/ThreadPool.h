

#ifndef YTHREADPOOL_H
#define YTHREADPOOL_H

#include <vector>
#include <list>
#include <map>
#include <future>
#include <thread>
#include <algorithm>
#include <memory>
#include <functional>

#include "threadpool/Queue/AtomicQueue.h"
#include "threadpool/Queue/WorkStealingQueue.h"
#include "threadpool/Queue/AtomicPriorityQueue.h"
#include "threadpool/Queue/AtomicRingBufferQueue.h"
#include "threadpool/Queue/LockFreeRingBufferQueue.h"
#include "threadpool/Task/Task.h"
#include "threadpool/Task/TaskGroup.h"
#include "threadpool/ThreadPoolConfig.h"
#include "threadpool/Thread/ThreadPrimary.h"
#include "threadpool/Thread/ThreadSecondary.h"

namespace YThreadPool{

class ThreadPool {
public:
    /**
     * 通过默认设置参数，来创建线程池
     * @param autoInit 是否自动开启线程池功能
     * @param config
     */
    explicit ThreadPool(bool autoInit = true,
        const ThreadPoolConfig& config = ThreadPoolConfig()) noexcept;

    /**
     * 析构函数
     */
    ~ThreadPool();

    /**
     * 设置线程池相关配置信息，需要在init()函数调用前，完成设置
     * @param config
     * @return
     * @notice 通过单例类(ThreadPoolSingleton)开启线程池，则线程池默认init。需要 destroy 后才可以设置参数
     */
    std::string setConfig(const ThreadPoolConfig& config);

    /**
     * 开启所有的线程信息
     * @return
     */
    std::string init();

    /**
     * 提交任务信息
     * @tparam FunctionType
     * @param task
     * @param index
     * @return
     */
    template<typename FunctionType>
    auto commit(const FunctionType& task,
        int index = DEFAULT_TASK_STRATEGY)
        -> std::future<decltype(std::declval<FunctionType>()())>;

    /**
     * 向特定的线程id中，提交任务信息
     * @tparam FunctionType
     * @param task
     * @param tid 线程id。如果超出主线程个数范围，则默认写入pool的通用队列中
     * @param enable 是否启用上锁/解锁功能
     * @param lockable 上锁(true) / 解锁(false)
     * @return
     */
    template<typename FunctionType>
    auto commitWithTid(const FunctionType& task, int tid, bool enable, bool lockable)
        -> std::future<decltype(std::declval<FunctionType>()())>;

    /**
     * 根据优先级，执行任务
     * @tparam FunctionType
     * @param task
     * @param priority 优先级别。自然序从大到小依次执行
     * @return
     * @notice 建议，priority 范围在 [-100, 100] 之间
     */
    template<typename FunctionType>
    auto commitWithPriority(const FunctionType& task,
        int priority)
        -> std::future<decltype(std::declval<FunctionType>()())>;

    /**
     * 执行任务组信息
     * 取taskGroup内部ttl和入参ttl的最小值，为计算ttl标准
     * @param taskGroup
     * @param ttl
     * @return
     */
    std::string submit(const TaskGroup& taskGroup,
        long ttl = MAX_BLOCK_TTL);

    /**
     * 针对单个任务的情况，复用任务组信息，实现单个任务直接执行
     * @param task
     * @param ttl
     * @param onFinished
     * @return
     */
    std::string submit(const std::function<void()>& func,
        long ttl = MAX_BLOCK_TTL,
        const std::function<void(std::string)>& onFinished = nullptr);

    /**
     * 获取根据线程id信息，获取线程index信息
     * @param tid
     * @return
     * @notice 辅助线程返回-1
     */
    int getThreadIndex(size_t tid);

    /**
     * 释放所有的线程信息
     * @return
     */
    std::string destroy();

    /**
     * 判断线程池是否已经初始化了
     * @return
     */
    bool isInit() const;

    /**
     * 生成辅助线程。内部确保辅助线程数量不超过设定参数
     * @param size
     * @return
     */
    std::string createSecondaryThread(int size);

    /**
     * 删除辅助线程
     * @param size
     * @return
     */
    std::string releaseSecondaryThread(int size);

protected:
    /**
     * 根据传入的策略信息，确定最终执行方式
     * @param origIndex
     * @return
     */
    virtual int dispatch(int origIndex);

    /**
     * 监控线程执行函数，主要是判断是否需要增加线程，或销毁线程
     * 增/删 操作，仅针对secondary类型线程生效
     */
    void monitor();

    ThreadPool(const ThreadPool&) = delete;
    const ThreadPool& operator=(const ThreadPool&) = delete;

private:
    bool is_init_{ false };                                                       // 是否初始化
    int cur_index_ = 0;                                                            // 记录放入的线程数
    AtomicQueue<Task> task_queue_;                                                // 用于存放普通任务
    AtomicPriorityQueue<Task> priority_task_queue_;                               // 运行时间较长的任务队列，仅在辅助线程中执行
    std::vector<ThreadPrimaryPtr> primary_threads_;                                // 记录所有的主线程
    std::list<std::unique_ptr<ThreadSecondary>> secondary_threads_;                // 用于记录所有的辅助线程
    ThreadPoolConfig config_;                                                      // 线程池设置值
    std::thread monitor_thread_;                                                    // 监控线程
    std::map<size_t, int> thread_record_map_;                                        // 线程记录的信息
    std::mutex st_mutex_;                                                           // 辅助线程发生变动的时候，加的mutex信息
};

using ThreadPoolPtr = std::unique_ptr<ThreadPool>;
using ThreadPoolPtrs = std::shared_ptr<ThreadPool>;

}

#include "ThreadPool.inl"


#endif 
