

#ifndef THREADPOOL_INL
#define THREADPOOL_INL

#include "threadpool/ThreadPool.h"

namespace YThreadPool{

template<typename FunctionType>
auto ThreadPool::commit(const FunctionType& task, int index)
-> std::future<decltype(std::declval<FunctionType>()())> {
    using ResultType = decltype(std::declval<FunctionType>()());//在编译时，不真正创建对象(构造函数)，进行类型推导

    std::packaged_task<ResultType()> runnableTask(std::move(task));//模板类,包装成一个异步任务
    std::future<ResultType> result(runnableTask.get_future()); //异步地运行代码并在未来某个时刻获取它的输出

    int realIndex = dispatch(index);
    if (realIndex >= 0 && realIndex < config_.default_thread_size_) {
        // 如果返回的结果，在主线程数量之间，则放到主线程的queue中执行
        primary_threads_[realIndex]->pushTask(std::move(runnableTask));
    }
    else if (LONG_TIME_TASK_STRATEGY == realIndex) {
        /**
         * 如果是长时间任务，则交给特定的任务队列，仅由辅助线程处理
         * 目的是防止有很多长时间任务，将所有运行的线程均阻塞
         * 长任务程序，默认优先级较低
         **/
        priority_task_queue_.push(std::move(runnableTask), LONG_TIME_TASK_STRATEGY);
    }
    else {
        // 返回其他结果，放到pool的queue中执行
        task_queue_.push(std::move(runnableTask));
    }
    return result;
}


template<typename FunctionType>
auto ThreadPool::commitWithTid(const FunctionType& task, int tid, bool enable, bool lockable)
-> std::future<decltype(std::declval<FunctionType>()())> {
    using ResultType = decltype(std::declval<FunctionType>()());
    std::packaged_task<ResultType()> runnableTask(std::move(task));
    std::future<ResultType> result(runnableTask.get_future());

    if (tid >= 0 && tid < config_.default_thread_size_) {
        primary_threads_[tid]->pushTask(std::move(runnableTask), enable, lockable);
    }
    else {
        // 如果超出主线程的范围，则默认写入 pool 通用的任务队列中
        task_queue_.push(task);
    }
    return result;
}


template<typename FunctionType>
auto ThreadPool::commitWithPriority(const FunctionType& task, int priority)
-> std::future<decltype(std::declval<FunctionType>()())> {
    using ResultType = decltype(std::declval<FunctionType>()());

    std::packaged_task<ResultType()> runnableTask(task);
    std::future<ResultType> result(runnableTask.get_future());

    if (secondary_threads_.empty()) {
        createSecondaryThread(1);    // 如果没有开启辅助线程，则直接开启一个
    }

    priority_task_queue_.push(std::move(runnableTask), priority);
    return result;
}

}
#endif   
