#ifndef QUEUEDEFINE_H
#define QUEUEDEFINE_H

namespace YThreadPool{

/** 当环形队列满的时候，写入信息时候的策略 */
enum class RingBufferPushStrategy {
    WAIT = 1,                 // 等待有数据被消费后，再写入
    REPLACE = 2,              // 替换未被消费的最早进入的内容
    DROP = 3,                 // 丢弃当前信息
};

}

#endif 
