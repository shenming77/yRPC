#ifndef ZOOKEEPERUTIL_H
#define ZOOKEEPERUTIL_H

#include<zookeeper/zookeeper.h>
#include<semaphore.h>
#include<string>

// 封装zookeeper客户端
class ZkClient
{ 
public:
    ZkClient();
    ~ZkClient();
    void start();
    void create(const char* path,const char *data, int datalen, int state = 0);
    std::string getData(const char *path);

private:
    // zk的客户端句柄
    zhandle_t * m_zhandle;
};


#endif