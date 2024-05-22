
// 一个包含盗取功能的安全队列


#ifndef WORKSTEALINGQUEUE_H
#define WORKSTEALINGQUEUE_H

#include <deque>

#include "threadpool/Queue/WorkStealing.h"

namespace YThreadPool{

template<typename T>
class WorkStealingQueue : public WorkStealing<T>{
public:

    WorkStealingQueue() = default;
    WorkStealingQueue(const WorkStealingQueue&) = delete;
    const WorkStealingQueue& operator=(const WorkStealingQueue&) = delete;
    ~WorkStealingQueue() = default;

    /**
     * 向队列中写入信息
     * @param value
     */
    void push(T&& value) override {
        while (true) {
            if (this->mutex_.try_lock()) {
                deque_.emplace_back(std::forward<T>(value));
                this->mutex_.unlock();
                break;
            }
            else {
                std::this_thread::yield();
            }
        }
    }


    /**
     * 有条件的写入数据信息
     * @param value
     * @param enable
     * @param state
     * @return
     */
    void push(T&& value, bool enable, bool lockable) override {
        if (enable && lockable) {
            this->mutex_.lock();
        }
        deque_.emplace_back(std::forward<T>(value));
        if (enable && lockable) {
            this->mutex_.unlock();
        }
    }


    /**
     * 尝试往队列里写入信息
     * @param value
     * @return
     */
    bool tryPush(T&& value) override {
        bool result = false;
        if (this->mutex_.try_lock()) {
            deque_.emplace_back(std::forward<T>(value));
            this->mutex_.unlock();
            result = true;
        }
        return result;
    }


    /**
     * 批量向队列中写入信息
     * @param values
     */
    void push(const std::vector<T>& values) override {
        while (true) {
            if (this->mutex_.try_lock()) {
                for (auto& value : values) {
                    deque_.emplace_back(std::move(value));
                }
                this->mutex_.unlock();
                break;
            }
            else {
                std::this_thread::yield();
            }
        }
    }


    /**
     * 弹出节点，从头部进行
     * @param value
     * @return
     */
    bool tryPop(T& value) override {
        // 这里不使用raii锁，主要是考虑到多线程的情况下，可能会重复进入
        bool result = false;
        if (!deque_.empty() && this->mutex_.try_lock()) {
            if (!deque_.empty()) {
                value = std::forward<T>(deque_.front());    // 从前方弹出
                deque_.pop_front();
                result = true;
            }
            this->mutex_.unlock();
        }

        return result;
    }


    /**
     * 从头部开始批量获取可执行任务信息
     * @param values
     * @param maxLocalBatchSize
     * @return
     */
    bool tryPop(std::vector<T>& values, int maxLocalBatchSize) override {
        bool result = false;
        if (!deque_.empty() && this->mutex_.try_lock()) {
            while (!deque_.empty() && maxLocalBatchSize--) {
                values.emplace_back(std::forward<T>(deque_.front()));
                deque_.pop_front();
                result = true;
            }
            this->mutex_.unlock();
        }

        return result;
    }


    /**
     * 窃取节点，从尾部进行
     * @param task
     * @return
     */
    bool trySteal(T& value) override {
        bool result = false;
        if (!deque_.empty() && this->mutex_.try_lock()) {
            if (!deque_.empty()) {
                value = std::forward<T>(deque_.back());    // 从后方窃取
                deque_.pop_back();
                result = true;
            }
            this->mutex_.unlock();
        }

        return result;
    }


    /**
     * 批量窃取节点，从尾部进行
     * @param values
     * @return
     */
    bool trySteal(std::vector<T>& values, int maxStealBatchSize) override {
        bool result = false;
        if (!deque_.empty() && this->mutex_.try_lock()) {
            while (!deque_.empty() && maxStealBatchSize--) {
                values.emplace_back(std::forward<T>(deque_.back()));
                deque_.pop_back();
                result = true;
            }
            this->mutex_.unlock();
        }

        return result;    // 如果非空，表示盗取成功
    }


private:
    std::deque<T> deque_;            // 存放任务的双向队列
};

}

#endif 
