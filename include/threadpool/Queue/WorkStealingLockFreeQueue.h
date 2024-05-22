
// 一个包含盗取功能的无锁队列


#ifndef WORKSTEALINGLOCKFREEQUEUE_H
#define WORKSTEALINGLOCKFREEQUEUE_H

#include <atomic>
#include <memory>
#include <iostream>
#include "threadpool/Queue/WorkStealing.h"

namespace YThreadPool{

template<typename T, int CAPACITY = DEFAULT_RINGBUFFER_SIZE>
class WorkStealingLockFreeQueue : public WorkStealing<T> {
public:
    explicit WorkStealingLockFreeQueue() {
        head_ = 0;
        tail_ = 0;
        head_update = 0;
        tail_update = 0;
        ring_buffer_.resize(CAPACITY);
    }
    
    /**
     * 向队列中写入信息
     * @param value
     */
    void push(T&& value) override {
        int curTail, nextTail;
        do {
            curTail = tail_.load(std::memory_order_acquire);
            nextTail = (curTail + 1) % CAPACITY;
            if (nextTail == head_.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
            if (nextTail == head_update.load(std::memory_order_acquire)) {
                std::this_thread::yield();
            }
        } while (!tail_.compare_exchange_weak(curTail, nextTail, std::memory_order_release, std::memory_order_relaxed));
        
        ring_buffer_[curTail] = std::move(value);
        tail_update.store(nextTail, std::memory_order_release); // 更新尾部更新标记
    }


    /**
     * 有条件的写入数据信息
     * @param value
     * @param enable
     * @param state
     * @return
     */
    void push(T&& value, bool enable, bool lockable) override {
        int curTail, nextTail;
        do {
            curTail = tail_.load(std::memory_order_acquire);
            nextTail = (curTail + 1) % CAPACITY;
            if ((enable && lockable) && (nextTail == head_.load(std::memory_order_acquire))) {
                std::this_thread::yield();
            }
            if ((enable && lockable) && (nextTail == head_update.load(std::memory_order_acquire))) {
                std::this_thread::yield();
            }
        } while ((enable && lockable) && !tail_.compare_exchange_weak(curTail, nextTail, std::memory_order_release, std::memory_order_relaxed));
        
        ring_buffer_[curTail] = std::move(value);
        if (enable && lockable) tail_update.store(nextTail, std::memory_order_release); // 更新尾部更新标记
    }


    /**
     * 尝试往队列里写入信息
     * @param value
     * @return
     */
    bool tryPush(T&& value) override {
        int curTail, nextTail;
        do {
            curTail = tail_.load(std::memory_order_acquire);
            nextTail = (curTail + 1) % CAPACITY;
            if (nextTail == head_.load(std::memory_order_acquire)) {
                return false; // 队列已满
            }
            if (nextTail == head_update.load(std::memory_order_acquire)) {
                return false; // 队列已满
            }
        } while (!tail_.compare_exchange_weak(curTail, nextTail, std::memory_order_release, std::memory_order_relaxed));
        
        ring_buffer_[curTail] = std::move(value);
        tail_update.store(nextTail, std::memory_order_release); // 更新尾部更新标记
        return true;
    }




    /**
     * 批量向队列中写入信息
     * @param values
     */
    void push(const std::vector<T>& values) override {
        int curTail, nextTail;
        for (auto& value : values) {
            do {
                curTail = tail_.load(std::memory_order_acquire);
                nextTail = (curTail + 1) % CAPACITY;
                if (nextTail == head_.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                if (nextTail == head_update.load(std::memory_order_acquire)) {
                    std::this_thread::yield();
                }
                } while (!tail_.compare_exchange_weak(curTail, nextTail, std::memory_order_release, std::memory_order_relaxed));
        
            ring_buffer_[curTail] = std::move(const_cast<T&>(value));
            tail_update.store(nextTail, std::memory_order_release); // 更新尾部更新标记
        }
    }
    


    /**
     * 弹出节点，从头部进行
     * @param value
     * @return
     */

    bool tryPop(T& value) override {
        int curHead, nextHead;
        do {
            curHead = head_.load(std::memory_order_acquire);
            nextHead = (curHead + 1) % CAPACITY;
            if (curHead == tail_.load(std::memory_order_acquire)) {
                return false; // 队列为空
            }
            if (curHead == tail_update.load(std::memory_order_acquire)) {
                return false; // 队列为空
            }
        } while (!head_.compare_exchange_weak(curHead, nextHead, std::memory_order_release, std::memory_order_relaxed));
        
        value = std::move(ring_buffer_[curHead]);
        head_update.store(nextHead, std::memory_order_release); // 更新头部更新标记
        return true;
    }



    /**
     * 从头部开始批量获取可执行任务信息
     * @param values
     * @param maxLocalBatchSize
     * @return
     */
    bool tryPop(std::vector<T>& values, int maxLocalBatchSize) override {
        bool result = false;
        int curHead, nextHead;
        while (maxLocalBatchSize--) {
            do {
                curHead = head_.load(std::memory_order_acquire);
                nextHead = (curHead + 1) % CAPACITY;
                if (curHead == tail_.load(std::memory_order_acquire)) {
                    return result; // 队列为空
                }
                if (curHead == tail_update.load(std::memory_order_acquire)) {
                    return result; // 队列为空
                }
            } while (!head_.compare_exchange_weak(curHead, nextHead, std::memory_order_release, std::memory_order_relaxed));
        
            values.emplace_back(std::move(ring_buffer_[curHead]));
            head_update.store(nextHead, std::memory_order_release); // 更新头部更新标记
            result = true;
        }
        return result;
    }


    /**
     * 窃取节点，从尾部进行
     * @param task
     * @return
     */
    bool trySteal(T& value) override {
        int curTail, prevTail;
        do {
            curTail = tail_.load(std::memory_order_acquire);
            if (curTail == head_.load(std::memory_order_acquire)) {
                return false; // 队列为空
            }
            prevTail = (curTail - 1 + CAPACITY) % CAPACITY;
            if (prevTail == head_update.load(std::memory_order_acquire)) {
                return false; // 新数据可能正在被推入
            }
        } while (!tail_.compare_exchange_weak(curTail, prevTail, std::memory_order_release, std::memory_order_relaxed));
        
        value = std::move(ring_buffer_[prevTail]);//curTail并不可用
        tail_update.store(prevTail, std::memory_order_release); // 更新尾部更新标记
        return true;
    }



    /**
     * 批量窃取节点，从尾部进行
     * @param values
     * @return
     */
    bool trySteal(std::vector<T>& values, int maxStealBatchSize) override{
        bool result = false;
        int curTail, prevTail;
        while (maxStealBatchSize--) {
            do {
                curTail = tail_.load(std::memory_order_acquire);
                if (curTail == head_.load(std::memory_order_acquire)) {
                    return result; // 队列为空
                }
                prevTail = (curTail - 1 + CAPACITY) % CAPACITY;
                if (prevTail == head_update.load(std::memory_order_acquire)) {
                    return result; // 新数据可能正在被推入
                }
            } while (!tail_.compare_exchange_weak(curTail, prevTail, std::memory_order_release, std::memory_order_relaxed));
        
            values.emplace_back(std::move(ring_buffer_[prevTail]));
            tail_update.store(prevTail, std::memory_order_release); // 更新尾部更新标记
            result = true;
        }
        return result;
    }

    //std::memory_order_relaxed最宽松的内存顺序约束规则,同一个线程具有happens-before关系
    //memory_order_acquire获取,保证在它之后的访问永远在它之后
    //memory_order_release释放,保证它之前的操作永远在它之前
    
    //compare_exchange_strong 试图将原子对象的值与提供的预期值 (expected value) 比较，如果它们相等，
    //则将该原子对象设置为新的值 (desired value)。如果比较成功，操作返回 true；
    //如果失败，原子对象的当前值会被存储到预期值变量中，并返回 false。
    
    //利用了 compare_exchange_strong 来确保 head_ 和 tail_ 的更新是原子性的，
    //从而避免并发环境下的竞争问题。这些修改有助于确保队列的线程安全和数据的一致性。

    //通过额外的 head_update 和 tail_update 试图同步。虽然这样做可以减少直接的数据竞争，但没有明确解决 ABA 问题

    WorkStealingLockFreeQueue(const WorkStealingLockFreeQueue&) = delete;
    const WorkStealingLockFreeQueue& operator=(const WorkStealingLockFreeQueue&) = delete;
    ~WorkStealingLockFreeQueue() = default;
    
private:
    std::atomic<int> head_;                                // 开始元素（较早写入的）的位置
    std::atomic<int> tail_;                                // 尾部的位置
    std::atomic<int> head_update;
    std::atomic<int> tail_update; 
    std::vector<T> ring_buffer_;          // 环形队列
};

}

#endif 




