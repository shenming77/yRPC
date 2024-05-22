//一个安全队列

#ifndef ATOMICQUEUE_H
#define ATOMICQUEUE_H

#include <memory>
#include <mutex>
#include <queue>
#include <condition_variable>

#include "threadpool/ThreadPoolDefine.h"
#include "threadpool/Queue/QueueObject.h"

namespace YThreadPool{

template<typename T>
class AtomicQueue : public QueueObject {
public:
    AtomicQueue() = default;

    /**
     * 等待弹出
     * @param value
     */
    void waitPop(T& value) {
        std::unique_lock<std::mutex> lk(mutex_);
        cv_.wait(lk, [this] { return !queue_.empty(); });
        value = std::move(*queue_.front());
        queue_.pop();
    }


    /**
     * 尝试弹出
     * @param value
     * @return
     */
    bool tryPop(T& value) {
        bool result = false;
        if (!queue_.empty() && mutex_.try_lock()) {
            if (!queue_.empty()) {
                value = std::move(*queue_.front());
                queue_.pop();
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
        if (!queue_.empty() && mutex_.try_lock()) {
            while (!queue_.empty() && maxPoolBatchSize-- > 0) {
                values.emplace_back(std::move(*queue_.front()));
                queue_.pop();
                result = true;
            }
            mutex_.unlock();
        }

        return result;
    }


    /**
     * 阻塞式等待弹出
     * @return
     */
    std::unique_ptr<T> popWithTimeout(long ms) {
        std::unique_lock<std::mutex> lk(mutex_);
        if (!cv_.wait_for(lk, std::chrono::milliseconds(ms), [this] { return !queue_.empty(); })) {//超时唤醒，尝试重新获取互斥锁
            return nullptr;
        }

        std::unique_ptr<T> result = std::move(queue_.front());
        queue_.pop();    // 如果等成功了，则弹出一个信息
        return result;
    }


    /**
     * 非阻塞式等待弹出
     * @return
     */
    std::unique_ptr<T> tryPop() {
        std::lock_guard<std::mutex> lk(mutex_);
        if (queue_.empty()) { return std::unique_ptr<T>(); }
        std::unique_ptr<T> ptr = std::move(queue_.front());
        queue_.pop();
        return ptr;
    }


    /**
     * 传入数据
     * @param value
     */
    void push(T&& value) {
        std::unique_ptr<typename std::remove_reference<T>::type>     \
            task(std::make_unique<typename std::remove_reference<T>::type>(std::forward<T>(value)));
        while (true) {
            if (mutex_.try_lock()) {
                queue_.push(std::move(task));
                mutex_.unlock();
                break;
            }
            else {
                std::this_thread::yield();
            }
        }
        cv_.notify_one();
    }


    /**
     * 判定队列是否为空
     * @return
     */
    bool empty() {
        std::lock_guard<std::mutex> lk(mutex_);
        return queue_.empty();
    }

    AtomicQueue (const AtomicQueue &) = delete;
    const AtomicQueue & operator=(const AtomicQueue &) = delete;

private:
    std::queue<std::unique_ptr<T>> queue_;
};


}

#endif