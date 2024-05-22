#ifndef RPCCLIENT_H
#define RPCCLIENT_H
#include <fcntl.h>       // fcntl()
#include <unistd.h>      // close()
#include <assert.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<shared_mutex>
#include<google/protobuf/service.h>
#include<iostream>
#include<string>
#include<unordered_map>
#include<list>
#include"protobuf/rpc.pb.h"
#include"zookeeper/zookeeperUtil.h"
#include"threadpool/ThreadPool.h"
#include"rpc/epoller.h"
#include"rpc/rpcSession.h"
#include"log/log.h"

#define LENGTH_TYPE unsigned int
#define HEAD_LEN sizeof(LENGTH_TYPE)


class RpcClient {
public:
    explicit RpcClient(const std::string& config_file = "",const std::string& log_file = "");
    ~RpcClient();
    RpcClient(const RpcClient&) = delete;
    const RpcClient& operator=(const RpcClient&) = delete;

    bool is_connect(const std::string& method_path);
    void connect_(const std::string&method_path,google::protobuf::RpcController *controller);
    std::pair<std::string,uint16_t> getServiceipport(const std::string&method_path,google::protobuf::RpcController *controller);
    void run();
    bool CallSession(google::protobuf::RpcController *controller,
                const google::protobuf::Message *request,
                google::protobuf::Message *response,
                google::protobuf::Closure *c,
                const std::string& method_path);
    void CloseConn_(int fd);
    void DealRead_(RpcSession& sessions);
    void DealWrite_(RpcSession& sessions);
    void Read_(RpcSession& sessions);
    void Write_(RpcSession& sessions);
    ssize_t write_session(int fd,iovec iov_[2]);
    bool InitSocket_(); 
    int SetFdNonblock(int fd);


private: 
    bool isClose_;
    uint32_t connEvent_;
    // int timeoutMS_;  /* 毫秒MS */

    char* srcDir_;
    std::string config_file_,log_file_;
    std::unique_ptr<ZkClient> zkclient;
    std::unique_ptr<YThreadPool::ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;

    using List = std::list<std::pair<int,std::string>>;
    std::unordered_map<std::string,List> methodPath2fd;  //方法路径(服务器路径)to文件描述符
    std::unordered_map<int,List::iterator> fd_find;  //查找文件描述符的地址，便于删除
    std::unordered_map<std::string,int>  methodPath_curfd;    //方法路径当前使用的文件描述符

    std::unordered_map<int,RpcSession> fd_sessions;

    std::shared_mutex mutex_;
    std::thread thread_;
};



#endif
