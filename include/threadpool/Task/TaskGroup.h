//任务组，用于批量提交

#ifndef TASKGROUP_H
#define TASKGROUP_H

#include <utility>
#include <functional>
#include <string>
#include <vector>


#include "threadpool/ThreadPoolDefine.h"

namespace YThreadPool{

class TaskGroup {
public:
    explicit TaskGroup() = default;

    TaskGroup(const TaskGroup&) = delete; 
    const TaskGroup& operator=(const TaskGroup&) = delete;
        /**
         * 直接通过函数来申明taskGroup
         * @param task
         * @param ttl
         * @param onFinished
         */
    explicit TaskGroup(const std::function<void()>& task,
            long ttl = MAX_BLOCK_TTL,
            const std::function<void(std::string)>& onFinished = nullptr) noexcept {
            this->addTask(task)
            ->setTtl(ttl)
            ->setOnFinished(onFinished);
    }

    /**
     * 添加一个任务
     * @param task
     */
    TaskGroup* addTask(const std::function<void()>& task) {
        task_arr_.emplace_back(task);
        return this;
    }

    /**
     * 设置任务最大超时时间
     * @param ttl
     */
    TaskGroup* setTtl(long ttl) {
        this->ttl_ = ttl;
        return this;
    }

    /**
     * 设置执行完成后的回调函数
     * @param onFinished
     * @return
     */
    TaskGroup* setOnFinished(const std::function<void(std::string)>& onFinished) {
        this->on_finished_ = onFinished;
        return this;
    }

    /**
     * 获取最大超时时间信息
     * @return
     */
    long getTtl() const {
        return this->ttl_;
    }

    /**
     * 清空任务组
     */
    void clear() {
        task_arr_.clear();
    }

    /**
     * 获取任务组大小
     * @return
     */
    size_t getSize() const {
        auto size = task_arr_.size();
        return size;
    }

private:
    std::vector<std::function<void()>> task_arr_;         // 任务消息
    long ttl_ = MAX_BLOCK_TTL;                      // 任务组最大执行耗时(如果是0的话，则表示不阻塞)
    std::function<void(std::string)> on_finished_ = nullptr;        // 执行函数任务结束

    friend class ThreadPool;
};

using TaskGroupPtr = TaskGroup*;
using TaskGroupRef = TaskGroup&;

}

#endif 
