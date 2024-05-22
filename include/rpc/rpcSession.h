#ifndef RPCSESSION_H
#define RPCSESSION_H
#include<shared_mutex>
#include<google/protobuf/service.h>
#include<iostream>
#include<string>
#include<unordered_map>
#include<list>
#include"log/log.h"

struct Session {
    explicit Session(google::protobuf::RpcController *controller,
                     const google::protobuf::Message *request,
                     google::protobuf::Message *response,
                     google::protobuf::Closure *c,
                     int fd,const std::string& methodPath)
                     :_controller(controller),_request(request),
                     _response(response),_c(c),_fd(fd),_methodPath(methodPath){}
    ~Session() = default;
    Session(const Session&) = delete;
    const Session& operator=(const Session&) = delete;

    google::protobuf::RpcController* _controller;
    const google::protobuf::Message* _request;
	google::protobuf::Message* _response;
	google::protobuf::Closure* _c;
    int _fd;
    std::string _methodPath;
};

enum class STATE {ST_HEAD, ST_DATA};

class RpcSession {
public:
    explicit RpcSession() = default;
    ~RpcSession() = default;
    RpcSession(const RpcSession&) = delete;
    const RpcSession& operator=(const RpcSession&) = delete;

    void init(int _fd);
    int GetFd();
    void read_begin_emplace(google::protobuf::RpcController *controller,
                     const google::protobuf::Message *request,
                     google::protobuf::Message *response,
                     google::protobuf::Closure *c,
                     int fd,const std::string& methodPath);
    void read_end_splice(std::list<Session>& sessions,std::list<Session>::iterator it);
    void read_data_out(std::list<Session>& sessions);
    bool read_count(google::protobuf::RpcController* controller);
    bool read_empty();

    void write_end_splice(std::list<Session>& sessions,std::list<Session>::iterator it) ;
    void write_erase(google::protobuf::RpcController* controller);
    bool write_count(google::protobuf::RpcController* controller);
    std::list<Session>::iterator write_find(google::protobuf::RpcController* controller);

    STATE fd_state;

private:
   int fd = -1;
   std::list<Session>  read_session, write_session; //读写队列
   std::unordered_map<google::protobuf::RpcController*,std::list<Session>::iterator> find_read_session, find_write_session;

   std::shared_mutex mutex_;
};



#endif