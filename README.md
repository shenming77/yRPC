

# YRPC

基于C++14实现的RPC分布式系统，引入了zookeeper服务注册中心功能，实现了服务的动态注册和访问；使用Protobuf对数据序列化与反序列化；使用Epoll边沿触发的IO多路复用技术，搭建Reactor高并发模型；并实现了异步日志，记录服务器运行状态。  

## 功能
* 服务端与客户端都利用Epoll边沿触发的IO多路复用技术，非阻塞IO，搭建Reactor高并发模型；
* 使用Protobuf 数据序列化与反序列化、zookeeper服务注册中心功能；
* 利用**[YThreadPool](https://github.com/shenming77/Y_ThreadPool)**线程池，实现**任务窃取机制**、**无锁机制**等方式来提升并发能力；
* 利用标准库list与unordered_map封装了两种缓冲区，在便于查找的同时，减少了调度过程中的各种细节损耗（比如copy构造等）；
* 基于Reactor高并发模型下，客户端采用异步访问，并对各个IO连接的数据缓冲区进行区分，减少多线程下互斥对cpu资源的浪费；
* 利用单例模式与阻塞队列实现异步的日志系统，记录服务器运行状态；
* 为减少内存泄漏的可能，使用智能指针等RAII机制；也使用了YAML库加载程序配置；

## 环境要求
* Linux
* C++14
* cmake
* Protobuf 3.19.4
* Zookeepeer 3.4.10
* YAML-CPP

## 目录树
```
.
├── app    
│   ├── userclient.cpp            用户客户端    
│   └── userservice.cpp           用户服务端
├── build
├── CMakeLists.txt
├── config
│   └── parameter.yml             程序配置文件
├── include                       头文件
│   ├── buffer
│   │   └── buffer.h
│   ├── log
│   │   ├── blockqueue.h
│   │   └── log.h                 日志库
│   ├── rpc
│   │   ├── epoller.h             
│   │   ├── rpcChannel.h          
│   │   ├── rpcClient.h           客户端程序 
│   │   ├── rpcConfig.h
│   │   ├── rpcController.h
│   │   ├── rpcServer.h           服务端程序
│   │   ├── rpcSessiondata.h      服务端缓冲区
│   │   └── rpcSession.h          客户端缓冲区
│   ├── threadpool                线程池
│   │   ├── Metrics.h
│   │   ├── Queue
│   │   │   ├── AtomicPriorityQueue.h
│   │   │   ├── AtomicQueue.h
│   │   │   ├── AtomicRingBufferQueue.h
│   │   │   ├── LockFreeRingBufferQueue.h
│   │   │   ├── QueueDefine.h
│   │   │   ├── QueueObject.h
│   │   │   ├── WorkStealing.h
│   │   │   ├── WorkStealingLockFreeQueue.h
│   │   │   └── WorkStealingQueue.h
│   │   ├── Task
│   │   │   ├── TaskGroup.h
│   │   │   └── Task.h
│   │   ├── Thread
│   │   │   ├── ThreadBase.h
│   │   │   ├── ThreadPrimary.h
│   │   │   └── ThreadSecondary.h
│   │   ├── ThreadPoolConfig.h
│   │   ├── ThreadPoolDefine.h
│   │   ├── ThreadPool.h
│   │   └── ThreadPool.inl
│   └── zookeeper                zookeeper服务程序
│       └── zookeeperUtil.h
├── log
│   ├── client
│   └── service
├── protobuf                     Protobuf程序
│   ├── rpc.pb.cc
│   ├── rpc.pb.h
│   ├── rpc.proto
│   ├── user.pb.cc
│   ├── user.pb.h
│   └── user.proto
├── readme.md
└── src
    ├── buffer
    │   └── buffer.cpp
    ├── log
    │   └── log.cpp
    ├── rpc
    │   ├── epoller.cpp
    │   ├── rpcChannel.cpp
    │   ├── rpcClient.cpp
    │   ├── rpcConfig.cpp
    │   ├── rpcContorller.cpp
    │   ├── rpcServer.cpp
    │   ├── rpcSession.cpp
    │   └── rpcSessiondata.cpp
    ├── threadpool
    │   └── ThreadPool.cpp
    └── zookeeper
        └── zookeeperUtil.cpp

```


## 项目启动
需要先配置好项目环境，[install.md](./pic/install.md)

启动Zookeeper服务

```bash
# 进入zookeeper文件路径
cd bin
./zkServer.sh start
```

项目编译运行

```bash
cd yRPC
cmake -B build        # 生成构建目录，-B 指定生成的构建系统代码放在 build 目录
cmake --build build   # 执行构建
./build/service       # 运行用户服务端
./build/client        # 运行用户客户端
```



## TOTG
* 服务端没有多台部署，来验证客户端的并发；
* 服务端与客户端需添加定时器，关闭长期非活动的连接；
