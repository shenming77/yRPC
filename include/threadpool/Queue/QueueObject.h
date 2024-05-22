

#ifndef QUEUEOBJECT_H
#define QUEUEOBJECT_H

#include <mutex>

#include "threadpool/Queue/QueueDefine.h"

namespace YThreadPool{

class QueueObject {
protected:
    std::mutex mutex_;
    std::condition_variable cv_;
};

}

#endif