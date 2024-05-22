

#ifndef UTHREADSECONDARY_H
#define UTHREADSECONDARY_H

#include "threadpool/Thread/ThreadBase.h"

namespace YThreadPool{

class ThreadSecondary : public ThreadBase {
public:
    explicit ThreadSecondary() {
        cur_ttl_ = 0;
        type_ = THREAD_TYPE_SECONDARY;
    }


protected:
    std::string init() override {
        std::string res;

        assert(config_ != nullptr);

        cur_ttl_ = config_->secondary_thread_ttl_;
        is_init_ = true;
        thread_ = std::move(std::thread(&ThreadSecondary::run, this));
        setSchedParam();

        return res;
    }


    /**
     * 设置pool的信息
     * @param poolTaskQueue
     * @param poolPriorityTaskQueue
     * @param config
     * @return
     */
    std::string setThreadPoolInfo(AtomicQueue<Task>* poolTaskQueue,
        AtomicPriorityQueue<Task>* poolPriorityTaskQueue,
        ThreadPoolConfigPtr config) {
        std::string res;

        assert(poolTaskQueue != nullptr);
        assert(poolPriorityTaskQueue != nullptr);
        assert(config != nullptr);

        this->pool_task_queue_ = poolTaskQueue;
        this->pool_priority_task_queue_ = poolPriorityTaskQueue;
        this->config_ = config;

        return res;
    }


    std::string run() final {
        std::string res;
        
        res += loopProcess();
        
        return res;
    }


    void processTask() override {
        Task task;
        if (popPoolTask(task)) {
            runTask(task);
        }
        else {
            // 如果单次无法获取，则稍加等待
            waitRunTask(config_->queue_emtpy_interval_);
        }
    }


    void processTasks() override {
        TaskArr tasks;
        if (popPoolTask(tasks)) {
            runTasks(tasks);
        }
        else {
            waitRunTask(config_->queue_emtpy_interval_);
        }
    }


    /**
     * 有等待的执行任务
     * @param ms
     * @return
     * @notice 目的是降低cpu的占用率
     */
    void waitRunTask(long ms) {
        auto task = this->pool_task_queue_->popWithTimeout(ms);
        if (nullptr != task) {
            (*task)();
        }
    }


    /**
     * 判断本线程是否需要被自动释放
     * @return
     */
    bool freeze() {
        if (likely(is_running_)) {
            cur_ttl_++;
            cur_ttl_ = std::min(cur_ttl_, config_->secondary_thread_ttl_);
        }
        else {
            cur_ttl_--;    // 如果当前线程没有在执行，则ttl-1
        }

        return cur_ttl_ <= 0 && done_;    // 必须是正在执行的线程，才可以被回收
    }

private:
    long cur_ttl_ = 0;                                                      // 当前最大生存周期

    friend class ThreadPool;
};

using ThreadSecondaryPtr = ThreadSecondary*;

}

#endif
