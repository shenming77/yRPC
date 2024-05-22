

#ifndef LOCKFREERINGBUFFERQUEUE_H
#define LOCKFREERINGBUFFERQUEUE_H

#include <atomic>
#include <memory>

#include "threadpool/Queue/QueueObject.h"

namespace YThreadPool{

template<typename T, int CAPACITY = DEFAULT_RINGBUFFER_SIZE>
class LockFreeRingBufferQueue : public QueueObject {
public:
    explicit LockFreeRingBufferQueue() {
        head_ = 0;
        tail_ = 0;
        ring_buffer_.resize(CAPACITY);
    }

    ~LockFreeRingBufferQueue() override {
        ring_buffer_.clear();
    }

    /**
     * 写入一个任务
     * @param value
     */
    void push(T&& value) {
        int curTail = tail_.load(std::memory_order_relaxed);//最宽松的内存顺序约束规则,同一个线程具有happens-before关系
        int nextTail = (curTail + 1) % CAPACITY;

        while (nextTail == head_.load(std::memory_order_acquire)) {//memory_order_acquire获取,保证在它之后的访问永远在它之后
            // 队列已满，等待其他线程出队
            std::this_thread::yield();//让出CPU,但线程仍然处于“就绪”状态
        }

        ring_buffer_[curTail] = std::move(value);
        tail_.store(nextTail, std::memory_order_release);//memory_order_release释放,保证它之前的操作永远在它之前
    }

    /**
     * 尝试弹出一个任务
     * @param value
     * @return
     */
    bool tryPop(T& value) {
        int curHead = head_.load(std::memory_order_relaxed);
        if (curHead == tail_.load(std::memory_order_acquire)) {//保证消费者线程可以看到生产者线程对 tail_ 所做的最新修改
            // 队列已空，直接返回false
            return false;
        }

        value = std::move(ring_buffer_[curHead]);

        int nextHead = (curHead + 1) % CAPACITY;
        head_.store(nextHead, std::memory_order_release);
        return true;
    }
    //memory_order_release 和 memory_order_acquire 我们可以避免成本较高的锁操作，同时还能保证数据的一致性和正确性。
    //release 和 acquire 之间形成了所谓的释放-获取配对，它确保内存操作的可见性和顺序性


private:
    std::atomic<int> head_;                                // 开始元素（较早写入的）的位置
    std::atomic<int> tail_;                                // 尾部的位置
    std::vector<std::unique_ptr<T>> ring_buffer_;          // 环形队列
};

}

#endif 
