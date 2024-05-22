
#include "threadpool/ThreadPool.h"
#include "log/log.h"
namespace YThreadPool{

ThreadPool::ThreadPool(bool autoInit, const ThreadPoolConfig& config) noexcept {
    cur_index_ = 0;
    is_init_ = false;
    this->setConfig(config);    // setConfig 函数，用在 is_init_ 设定之后
    if (autoInit) {
        this->init();
    }
}


ThreadPool::~ThreadPool() {
    this->config_.monitor_enable_ = false;    // 在析构的时候，才释放监控线程。先释放监控线程，再释放其他的线程
    if (monitor_thread_.joinable()) { //检测线程是否相关联
        monitor_thread_.join();
    }

    destroy();
}


std::string ThreadPool::setConfig(const ThreadPoolConfig& config) {
    std::string res;
    this->config_ = config;
    return res;
}


std::string ThreadPool::init() {
    std::string res;
        if (is_init_) {
            return res;
        }

    if (config_.monitor_enable_) {
        // 默认不开启监控线程
        monitor_thread_ = std::move(std::thread(&ThreadPool::monitor, this));
    }
    thread_record_map_.clear();
    thread_record_map_[(size_t)std::hash<std::thread::id>{}(std::this_thread::get_id())] = MAIN_THREAD_ID;//当前线程的ID的哈希值，并将主线程标记
    primary_threads_.reserve(config_.default_thread_size_);
    for (int i = 0; i < config_.default_thread_size_; i++) {
        ThreadPrimary* pt = nullptr;
        while (!pt) {
            pt = new(std::nothrow) ThreadPrimary();// 创建核心线程数
        }
        pt->setThreadPoolInfo(i, &task_queue_, &primary_threads_, &config_);
        // 记录线程和匹配id信息
        primary_threads_.emplace_back(pt);
    }

    /**
     * 等待所有thread 设置完毕之后，再进行 init()，
     * 避免在个别的平台上，可能出现 thread竞争访问其他线程、并且导致异常的情况
     * 参考： https://github.com/ChunelFeng/CGraph/issues/309
     */
    for (int i = 0; i < config_.default_thread_size_; i++) {
        res += primary_threads_[i]->init();
        thread_record_map_[(size_t)std::hash<std::thread::id>{}(primary_threads_[i]->thread_.get_id())] = i;
    }
    if (!res.empty())//std::cout << res << std::endl;
    LOG_INFO("error : %s", res.c_str());
        /**
         * 策略更新：
         * 初始化的时候，也可以创建n个辅助线程。目的是为了配合仅使用 pool中 priority_queue 的场景
         * 一般情况下，建议为0。
         */
    res = createSecondaryThread(config_.secondary_thread_size_);
    if (!res.empty())//std::cout << res << std::endl;
    LOG_INFO("error : %s", res.c_str());

    is_init_ = true;
    return res;
}


std::string ThreadPool::submit(const TaskGroup& taskGroup, long ttl) {
    std::string res;
     
    std::vector<std::future<void>> futures;
    for (const auto& task : taskGroup.task_arr_) {
        futures.emplace_back(commit(task));
    }

    // 计算最终运行时间信息
    auto deadline = std::chrono::steady_clock::now()
        + std::chrono::milliseconds(std::min(taskGroup.getTtl(), ttl));

    for (auto& fut : futures) {
        const auto& futStatus = fut.wait_until(deadline);//阻塞当前线程到指定的截止时间，等待任务结束(结束就返回) 
        switch (futStatus) {
        case std::future_status::ready: break;    // 正常情况，直接返回了
        case std::future_status::timeout: res += "thread status timeout"; break;
        case std::future_status::deferred: res += "thread status deferred"; break;
        default: res += "thread status unknown";
        }
    }

    if (taskGroup.on_finished_) {
        taskGroup.on_finished_(res);
    }

    return res;
}


std::string ThreadPool::submit(const std::function<void()>& func, long ttl,
    const std::function<void(std::string)>& onFinished) {
    return submit(TaskGroup(func, ttl, onFinished));
}


int ThreadPool::getThreadIndex(size_t tid) {
    int index = SECONDARY_THREAD_COMMON_ID;
    auto result = thread_record_map_.find(tid);
    if (result != thread_record_map_.end()) {
        index = result->second;
    }

    return index;
}


std::string ThreadPool::destroy() {
    std::string res;
    if (!is_init_) {
        return res;
    }

    // primary 线程是普通指针，需要delete
    for (auto& pt : primary_threads_) {
        res += pt->destroy();
    }
    if (!res.empty())//std::cout << res << std::endl;
    LOG_INFO("error : %s", res.c_str());

        /**
         * 这里之所以 destroy和 delete分开两个循环执行，
         * 是因为当前线程被delete后，还可能存在未被delete的主线程，来steal当前线程的任务
         * 在windows环境下，可能出现问题。
         * destroy 和 delete 分开之后，不会出现此问题。
         * 感谢 Ryan大佬(https://github.com/ryanhuang) 提供的帮助
         */
        for (auto& pt : primary_threads_) {
            delete pt;  
            pt = nullptr;
        }
    primary_threads_.clear();
    // secondary 线程是智能指针，不需要delete
    for (auto& st : secondary_threads_) {
        res += st->destroy();
    }
    
    secondary_threads_.clear();
    thread_record_map_.clear();
    is_init_ = false;

    return res;
}


bool ThreadPool::isInit() const {
    return is_init_;
}


std::string ThreadPool::releaseSecondaryThread(int size) {
    std::string res;

    // 先将所有已经结束的，给删掉
    std::lock_guard<std::mutex> lock(st_mutex_);
    for (auto iter = secondary_threads_.begin(); iter != secondary_threads_.end(); ) {
        !(*iter)->done_ ? secondary_threads_.erase(iter++) : iter++;
    }


    if (unlikely(size > secondary_threads_.size())) {
        res += "cannot release [" + std::to_string(size) + "] secondary thread," \
            + "only [" + std::to_string(secondary_threads_.size()) + "] left.";
    }

        // 再标记几个需要删除的信息
    for (auto iter = secondary_threads_.begin();iter != secondary_threads_.end() && size-- > 0; ) {
        (*iter)->done_ = false;
        iter++;
    }

    return res;
}


int ThreadPool::dispatch(int origIndex) { //简单的负载均衡机制
    int realIndex = 0;
    if (DEFAULT_TASK_STRATEGY == origIndex) {
        /**
         * 如果是默认策略信息，在[0, default_thread_size_) 之间的，通过 thread 中queue来调度
         * 在[default_thread_size_, max_thread_size_) 之间的，通过 pool 中的queue来调度
         */
        realIndex = cur_index_++;
        if (cur_index_ >= config_.max_thread_size_ || cur_index_ < 0) {
            cur_index_ = 0;
        }
    }
    else {
        realIndex = origIndex;
    }

    return realIndex;    // 交到上游去判断，走哪个线程
}


std::string ThreadPool::createSecondaryThread(int size) {
    std::string res;

    int leftSize = (int)(config_.max_thread_size_ - config_.default_thread_size_ - secondary_threads_.size());
    int realSize = std::min(size, leftSize);    // 使用 realSize 来确保所有的线程数量之和，不会超过设定max值

    std::lock_guard<std::mutex> lock(st_mutex_);
    for (int i = 0; i < realSize; i++) {
        auto ptr = std::make_unique<ThreadSecondary>();
        ptr->setThreadPoolInfo(&task_queue_, &priority_task_queue_, &config_);
        res += ptr->init();
        secondary_threads_.emplace_back(std::move(ptr));
    }

    return res;
}


void ThreadPool::monitor() {
    while (config_.monitor_enable_) {
        while (config_.monitor_enable_ && !is_init_) {
            // 如果没有init，则一直处于空跑状态
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        auto span = config_.monitor_span_;
        while (config_.monitor_enable_ && is_init_ && span--) {
            std::this_thread::sleep_for(std::chrono::seconds(1));    // 保证可以快速退出
        }

        // 如果 primary线程都在执行，则表示忙碌
        bool busy = !primary_threads_.empty() && std::all_of(primary_threads_.begin(), primary_threads_.end(),
            [](ThreadPrimaryPtr ptr) { return nullptr != ptr && ptr->is_running_; });

        std::lock_guard<std::mutex> lock(st_mutex_);
        // 如果忙碌或者priority_task_queue_中有任务，则需要添加 secondary线程
        if (busy || !priority_task_queue_.empty()) {
            createSecondaryThread(1);
        }

        // 判断 secondary 线程是否需要退出
        for (auto iter = secondary_threads_.begin(); iter != secondary_threads_.end(); ) {
            (*iter)->freeze() ? secondary_threads_.erase(iter++) : iter++;
        }
    }
}

}