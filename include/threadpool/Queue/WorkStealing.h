
// 一个包含盗取功能队列的基类


#ifndef WORKSTEALINGL_H
#define WORKSTEALINGL_H
#include <memory>
#include "threadpool/Queue/QueueObject.h"

namespace YThreadPool{

template<typename T>
class WorkStealing: public QueueObject{
public:
    WorkStealing() = default;
    WorkStealing(const WorkStealing&) = delete;
    const WorkStealing& operator=(const WorkStealing&) = delete;
    virtual ~WorkStealing() = default;

    /**
     * 向队列中写入信息
     * @param value
     */
    virtual void push(T&& value) = 0;


    /**
     * 有条件的写入数据信息
     * @param value
     * @param enable
     * @param state
     * @return
     */
    virtual void push(T&& value, bool enable, bool lockable) = 0;

    /**
     * 尝试往队列里写入信息
     * @param value
     * @return
     */
    virtual bool tryPush(T&& value) = 0;

    /**
     * 批量向队列中写入信息
     * @param values
     */
    virtual void push(const std::vector<T>& values) = 0;

    /**
     * 弹出节点，从头部进行
     * @param value
     * @return
     */
    virtual bool tryPop(T& value) = 0;

    /**
     * 从头部开始批量获取可执行任务信息
     * @param values
     * @param maxLocalBatchSize
     * @return
     */
    virtual bool tryPop(std::vector<T>& values, int maxLocalBatchSize) = 0;

    /**
     * 窃取节点，从尾部进行
     * @param task
     * @return
     */
    virtual bool trySteal(T& value) = 0;

    /**
     * 批量窃取节点，从尾部进行
     * @param values
     * @return
     */
    virtual bool trySteal(std::vector<T>& values, int maxStealBatchSize) = 0;

};

template<typename T>
using WorkStealingPtr = std::unique_ptr<WorkStealing<T>>;

}

#endif 
