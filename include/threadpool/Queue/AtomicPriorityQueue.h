//线程安全的优先队列

#ifndef ATOMICPRIORITYQUEUE_H
#define ATOMICPRIORITYQUEUE_H

#include <queue>

#include "threadpool/Queue/QueueObject.h"


namespace YThreadPool{

template<typename T>
class AtomicPriorityQueue : public QueueObject {
public:
    AtomicPriorityQueue() = default;

    /**
     * 尝试弹出
     * @param value
     * @return
     */
    bool tryPop(T& value) {
        bool result = false;
        if (mutex_.try_lock()) {
            if (!priority_queue_.empty()) {
                value = std::move(*priority_queue_.top());
                priority_queue_.pop();
                result = true;
            }
            mutex_.unlock();
        }

        return result;
    }


    /**
     * 尝试弹出多个任务
     * @param values
     * @param maxPoolBatchSize
     * @return
     */
    bool tryPop(std::vector<T>& values, int maxPoolBatchSize) {
        bool result = false;
        if (mutex_.try_lock()) {
            while (!priority_queue_.empty() && maxPoolBatchSize-- > 0) {
                values.emplace_back(std::move(*priority_queue_.top()));
                priority_queue_.pop();
                result = true;
            }
            mutex_.unlock();
        }

        return result;
    }


    /**
     * 传入数据
     * @param value
     * @param priority 任务优先级，数字排序
     * @return
     */
    void push(T&& value, int priority) {
        std::unique_ptr<T> task(std::make_unique<T>(std::move(value), priority));
        std::lock_guard<std::mutex> lk(mutex_);
        priority_queue_.push(std::move(task));
    }


    /**
     * 判定队列是否为空
     * @return
     */
    bool empty() {
        std::lock_guard<std::mutex> lk(mutex_);
        return priority_queue_.empty();
    }

    AtomicPriorityQueue(const AtomicPriorityQueue &) = delete;
    const AtomicPriorityQueue &operator=(const AtomicPriorityQueue &) = delete;

private:
    std::priority_queue<std::unique_ptr<T>> priority_queue_;    // 优先队列信息，根据重要级别决定先后执行顺序
};

}

#endif 
