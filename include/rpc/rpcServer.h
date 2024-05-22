# ifndef RPCSERVER_H
# define RPCSERVER_H
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
#include"rpc/rpcSessiondata.h"
#include"rpc/epoller.h"
#include"log/log.h"

#define LENGTH_TYPE unsigned int
#define HEAD_LEN sizeof(LENGTH_TYPE)
 
//服务类型信息
struct ServiceInfo{
    google::protobuf::Service* m_service;//保存服务对象
    std::unordered_map<std::string,const google::protobuf::MethodDescriptor*> m_methodMap;//保存服务方法
};


class RpcServer {
public:
    explicit RpcServer(const std::string& config_file = "",const std::string& log_file = "");
    ~RpcServer();
    RpcServer(const RpcServer&) = delete;
    const RpcServer& operator=(const RpcServer&) = delete;
    void NotifyService(google::protobuf::Service* service);
    void Registerzk();
    void Run();
    void DealListen_();
    void CloseConn_(int fd);
    void DealRead_(RpcSessiondata& sessions);
    void DealWrite_(RpcSessiondata& sessions);
    void Read_(RpcSessiondata& sessions);
    void Write_(RpcSessiondata& sessions);
    void read_session(rpc::RpcRequestData& rpc_data,RpcSessiondata& sessions);
    ssize_t write_session(int fd,iovec iov_[2]);
    bool InitSocket_(); 
    int SetFdNonblock(int fd);

private:
    bool isClose_;
    int listenFd_;
    uint32_t connEvent_,listenEvent_;
    std::string host_;
    int port_;

    char* srcDir_;
    std::string config_file_,log_file_;
    std::unique_ptr<ZkClient> zkclient;
    std::unique_ptr<YThreadPool::ThreadPool> threadpool_;
    std::unique_ptr<Epoller> epoller_;
    std::unordered_map<std::string,ServiceInfo> m_servicMap;//存储注册成功的服务对象和其服务方法的信息
    
    std::unordered_map<int, RpcSessiondata> fd_session; // 文件描述符发送的队列

};


#endif