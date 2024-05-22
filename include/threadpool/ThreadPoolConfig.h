
#ifndef THREADPOOLCONFIG_H
#define THREADPOOLCONFIG_H

#include "threadpool/ThreadPoolDefine.h"
#include "string"

namespace YThreadPool{

struct ThreadPoolConfig {
    /** 具体值含义，ThreadPoolDefinene.h文件 */
    int default_thread_size_ = DEFAULT_THREAD_SIZE;
    int secondary_thread_size_ = SECONDARY_THREAD_SIZE;
    int max_thread_size_ = MAX_THREAD_SIZE;
    int max_task_steal_range_ = MAX_TASK_STEAL_RANGE;
    int max_local_batch_size_ = MAX_LOCAL_BATCH_SIZE;
    int max_pool_batch_size_ = MAX_POOL_BATCH_SIZE;
    int max_steal_batch_size_ = MAX_STEAL_BATCH_SIZE;
    int primary_thread_busy_epoch_ = PRIMARY_THREAD_BUSY_EPOCH;
    long primary_thread_empty_interval_ = PRIMARY_THREAD_EMPTY_INTERVAL;
    long secondary_thread_ttl_ = SECONDARY_THREAD_TTL;
    long monitor_span_ = MONITOR_SPAN;
    long queue_emtpy_interval_ = QUEUE_EMPTY_INTERVAL;
    int primary_thread_policy_ = PRIMARY_THREAD_POLICY;
    int secondary_thread_policy_ = SECONDARY_THREAD_POLICY;
    int primary_thread_priority_ = PRIMARY_THREAD_PRIORITY;
    int secondary_thread_priority_ = SECONDARY_THREAD_PRIORITY;
    bool bind_cpu_enable_ = BIND_CPU_ENABLE;
    bool batch_task_enable_ = BATCH_TASK_ENABLE;
    bool monitor_enable_ = MONITOR_ENABLE;

    std::string check() const {
        std::string res;
            if (default_thread_size_ < 0 || secondary_thread_size_ < 0) {
                res += "thread size cannot less than 0";
            }

        if (default_thread_size_ + secondary_thread_size_ > max_thread_size_) {
            res+="max thread size is less than default + secondary thread. ["     \
                + std::to_string(max_thread_size_) + "<" + std::to_string(default_thread_size_) + "+"    \
                + std::to_string(secondary_thread_size_) + "]";
        }

        if (monitor_enable_ && monitor_span_ <= 0) {
            res += "monitor span cannot less than 0";
        }
        return res;
    }

protected:
    /**
     * 计算可盗取的范围，盗取范围不能超过默认线程数-1
     * @return
     */
    int calcStealRange() const {
        int range = std::min(this->max_task_steal_range_, this->default_thread_size_ - 1);//盗取机制相邻范围
        return range;
    }

    friend class ThreadPrimary;
    friend class ThreadSecondary;
};

using ThreadPoolConfigPtr = ThreadPoolConfig*;

}

#endif 
