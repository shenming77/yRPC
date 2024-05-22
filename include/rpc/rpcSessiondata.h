#ifndef RPCSESSIONDATA_H
#define RPCSESSIONDATA_H
#include<shared_mutex>
#include<google/protobuf/service.h>
#include<iostream>
#include<string>
#include<unordered_map>
#include<list>
#include"log/log.h"

struct Sessiondata {
    explicit Sessiondata(google::protobuf::RpcController *controller,
                        const std::string& sessiondata)
                        :_controller(controller),_sessiondata(std::move(sessiondata)){}
    ~Sessiondata() = default;
    Sessiondata(const Sessiondata&) = delete;
    const Sessiondata& operator=(const Sessiondata&) = delete;

    google::protobuf::RpcController* _controller;
    std::string _sessiondata;
};

enum class STATE {ST_HEAD, ST_DATA};

class RpcSessiondata {
public:
    explicit RpcSessiondata() = default;
    ~RpcSessiondata() = default;
    RpcSessiondata(const RpcSessiondata&) = delete;
    const RpcSessiondata& operator=(const RpcSessiondata&) = delete;
    
    void init(int _fd);
    int GetFd();
    void begin_emplace(google::protobuf::RpcController *controller,
                      const std::string& sessiondata);
    void data_out(std::list<Sessiondata>& sessions);
    void end_splice(std::list<Sessiondata>& sessions,std::list<Sessiondata>::iterator it);
    bool empty();
    void erase(google::protobuf::RpcController* controller);

    STATE fd_state;
private:
    int fd = -1;
    std::list<Sessiondata>  session; //读写队列
    std::unordered_map<google::protobuf::RpcController*,std::list<Sessiondata>::iterator> find_session;
};



#endif