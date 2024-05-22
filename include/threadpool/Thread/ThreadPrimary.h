// 核心线程，处理任务中


#ifndef THREADPRIMARY_H
#define THREADPRIMARY_H

#include <vector>
#include <mutex>

#include "threadpool/Thread/ThreadBase.h"

namespace YThreadPool{

class ThreadPrimary : public ThreadBase {
protected:
    explicit ThreadPrimary(bool lockfree_open = false) {
        index_ = SECONDARY_THREAD_COMMON_ID;
        pool_threads_ = nullptr;
        type_ = THREAD_TYPE_PRIMARY;
        if (lockfree_open) {
            primary_queue_ = std::make_unique<WorkStealingLockFreeQueue<Task>>();
            secondary_queue_ = std::make_unique<WorkStealingLockFreeQueue<Task>>();
        }else {
            primary_queue_ = std::make_unique<WorkStealingQueue<Task>>();
            secondary_queue_ = std::make_unique<WorkStealingQueue<Task>>();
        }
    }


    std::string init() override {
        std::string res;
        assert(config_ != nullptr);

        is_init_ = true;
        metrics_.reset();
        buildStealTargets();
        thread_ = std::move(std::thread(&ThreadPrimary::run, this));
        setSchedParam();
        setAffinity(index_);
        return res;
    }


    /**
     * 注册线程池相关内容，需要在init之前使用
     * @param index
     * @param poolTaskQueue
     * @param poolThreads
     * @param config
     */
    std::string setThreadPoolInfo(int index,
        AtomicQueue<Task>* poolTaskQueue,
        std::vector<ThreadPrimary*>* poolThreads,
        ThreadPoolConfigPtr config) {
        std::string res;
        
        assert(poolTaskQueue != nullptr);
        assert(poolThreads != nullptr);
        assert(config != nullptr);

        this->index_ = index;
        this->pool_task_queue_ = poolTaskQueue;
        this->pool_threads_ = poolThreads;
        this->config_ = config;
        
        return res;
    }


    /**
     * 线程执行函数
     * @return
     */
    std::string run() final {
        std::string res;

        assert(pool_threads_ != nullptr);

       /**
        * 线程池中任何一个primary线程为null都不可以执行
        * 防止线程初始化失败的情况，导致的崩溃
        * 理论不会走到这个判断逻辑里面
        */
       if (std::any_of(pool_threads_->begin(), pool_threads_->end(),
           [](ThreadPrimary* thd) {
               return nullptr == thd;
           })) {
           res += "primary thread is null";
       }

        res += loopProcess();
        return res;
    }


    void processTask() override {
        Task task;
        if (popTask(task) || popPoolTask(task) || stealTask(task)) {
            runTask(task);
        }
        else {
            fatWait();
        }
    }


    void processTasks() override {
        TaskArr tasks;
        if (popTask(tasks) || popPoolTask(tasks) || stealTask(tasks)) {
            // 尝试从主线程中获取/盗取批量task，如果成功，则依次执行
            runTasks(tasks);
        }
        else {
            fatWait();
        }
    }


    /**
     * 如果总是进入无task的状态，则开始休眠
     * 休眠一定时间后，然后恢复执行状态，避免出现异常情况导致无法唤醒
     */
    void fatWait() {
        cur_empty_epoch_++;
        metrics_.fleet_wait_times_++;
        std::this_thread::yield();
        if (cur_empty_epoch_ >= config_->primary_thread_busy_epoch_) {
            std::unique_lock<std::mutex> lk(mutex_);
            cv_.wait_for(lk, std::chrono::milliseconds(config_->primary_thread_empty_interval_));
            metrics_.deep_wait_times_++;
            cur_empty_epoch_ = 0;
        }
    }


    /**
     * 依次push到任一队列里。如果都失败，则yield，然后重新push
     * @param task
     * @return
     */
    void pushTask(Task&& task) {
        while (!(primary_queue_->tryPush(std::move(task))
            || secondary_queue_->tryPush(std::move(task)))) {
            metrics_.local_push_yield_times_++;
            std::this_thread::yield();
        }
        cur_empty_epoch_ = 0;
        metrics_.local_push_real_num_++;
        cv_.notify_one();
    }


    /**
     * 写入 task信息，是否上锁由
     * @param task
     * @param enable 确认是否有
     * @param lockable true 的时候需要上锁，false 的时候会解锁
     * @return
     */
    void pushTask(Task&& task, bool enable, bool lockable) {
        secondary_queue_->push(std::move(task), enable, lockable);    // 通过 second 写入，主要是方便其他的thread 进行steal操作
        if (enable && !lockable) {
            cur_empty_epoch_ = 0;
            cv_.notify_one();
        }
        metrics_.local_push_yield_times_++;
        metrics_.local_push_real_num_++;
    }


    /**
     * 从本地弹出一个任务
     * @param task
     * @return
     */
    bool popTask(TaskRef task) {
        auto result = primary_queue_->tryPop(task) || secondary_queue_->tryPop(task);
        metrics_.calcLocal(result, 1);
        return result;
    }


    /**
     * 从本地弹出一批任务
     * @param tasks
     * @return
     */
    bool popTask(TaskArrRef tasks) {
        bool result = primary_queue_->tryPop(tasks, config_->max_local_batch_size_);
        auto leftSize = config_->max_local_batch_size_ - tasks.size();
        if (leftSize > 0) {
            // 如果凑齐了，就不需要了。没凑齐的话，就继续
            result |= (secondary_queue_->tryPop(tasks, leftSize));
        }
        metrics_.calcLocal(result, tasks.size());
        return result;
    }


    /**
     * 从其他线程窃取一个任务
     * @param task
     * @return
     */
    bool stealTask(TaskRef task) {
        if (unlikely(pool_threads_->size() < config_->default_thread_size_)) {
            /**
             * 线程池还未初始化完毕的时候，无法进行steal。
             * 确保程序安全运行。
             */
            return false;
        }

        /**
         * 窃取的时候，仅从相邻的primary线程中窃取
         * 待窃取相邻的数量，不能超过默认primary线程数
         */
        bool result = false;
        for (auto& target : steal_targets_) {
            /**
            * 从线程中周围的thread中，窃取任务。
            * 如果成功，则返回true，并且执行任务。
             * steal 的时候，先从第二个队列里偷，从而降低触碰锁的概率
            */
            if (likely((*pool_threads_)[target])
                && (((*pool_threads_)[target])->secondary_queue_->trySteal(task))
                || ((*pool_threads_)[target])->primary_queue_->trySteal(task)) {
                result = true;
                break;
            }
        }

        metrics_.calcSteal(result, 1);
        return result;
    }


    /**
     * 从其他线程盗取一批任务
     * @param tasks
     * @return
     */
    bool stealTask(TaskArrRef tasks) {
        if (unlikely(pool_threads_->size() != config_->default_thread_size_)) {
            return false;
        }

        bool result = false;
        for (auto& target : steal_targets_) {
            if (likely((*pool_threads_)[target])) {
                result = ((*pool_threads_)[target])->secondary_queue_->trySteal(tasks, config_->max_steal_batch_size_);
                auto leftSize = config_->max_steal_batch_size_ - tasks.size();
                if (leftSize > 0) {
                    result |= ((*pool_threads_)[target])->primary_queue_->trySteal(tasks, leftSize);
                }

                if (result) {
                    /**
                     * 在这里，我们对模型进行了简化。实现的思路是：
                     * 尝试从邻居主线程(先secondary，再primary)中，获取 x(=max_steal_batch_size_) 个task，
                     * 如果从某一个邻居中，获取了 y(<=x) 个task，则也终止steal的流程
                     * 且如果如果有一次批量steal成功，就认定成功
                     */
                    break;
                }
            }
        }

        metrics_.calcSteal(result, tasks.size());
        return result;
    }


    /**
     * 构造 steal 范围的 target，避免每次盗取的时候，重复计算
     * @return
     */
    void buildStealTargets() {
        steal_targets_.clear();
        for (int i = 0; i < config_->calcStealRange(); i++) {
            auto target = (index_ + i + 1) % config_->default_thread_size_;
            steal_targets_.push_back(target);
        }
        steal_targets_.shrink_to_fit();//请求减少std::vector的容量以适应其大小
    }


    ~ThreadPrimary()  {
        /**
         * 在开启展示宏的时候，会在主线程退出的时候，打印相关内容
         * 默认情况下，不会开启
         */
        metrics_.show("thread" + std::to_string(index_));
    }

private:
    int index_;                                                    // 线程index
    int cur_empty_epoch_ = 0;                                      // 当前空转的轮数信息
    WorkStealingPtr<Task> primary_queue_;                      // 内部队列信息
    WorkStealingPtr<Task> secondary_queue_;                    // 第二个队列，用于减少触锁概率，提升性能
    std::vector<ThreadPrimary*>* pool_threads_;                  // 用于存放线程池中的线程信息
    std::vector<int> steal_targets_;                               // 被偷的目标信息

    friend class ThreadPool;
};

using ThreadPrimaryPtr = ThreadPrimary*;

}

#endif 
