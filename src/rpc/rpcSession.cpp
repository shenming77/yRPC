#include"rpc/rpcSession.h"

void RpcSession::init(int _fd) {
    std::unique_lock<std::shared_mutex> lock_r(mutex_);
    fd = _fd;
    fd_state = STATE::ST_HEAD;
    read_session.clear();
    write_session.clear();
    find_read_session.clear();
    find_write_session.clear();
}

int RpcSession::GetFd() {
    std::shared_lock<std::shared_mutex> lock_r(mutex_);
    return fd;
}

void RpcSession::read_begin_emplace(google::protobuf::RpcController *controller,
                     const google::protobuf::Message *request,
                     google::protobuf::Message *response,
                     google::protobuf::Closure *c,
                     int fd,const std::string& methodPath) {
    std::unique_lock<std::shared_mutex> lock_r(mutex_);
    find_read_session[controller] = read_session.emplace(read_session.begin(),controller,request,response,c,fd,methodPath);
}


void RpcSession::read_end_splice(std::list<Session>& sessions,std::list<Session>::iterator it) {
    std::unique_lock<std::shared_mutex> lock_r(mutex_);
    read_session.splice(read_session.end(),sessions,it);
    find_read_session[it->_controller] = it;
}

void RpcSession::read_data_out(std::list<Session>& sessions) {
    std::unique_lock<std::shared_mutex> lock_r(mutex_);
    sessions.splice(sessions.end(),read_session);
    find_read_session.clear();
}

bool RpcSession::read_count(google::protobuf::RpcController* controller) {
    std::shared_lock<std::shared_mutex> lock_r(mutex_);
    return find_read_session.count(controller);
}

bool RpcSession::read_empty(){
    std::shared_lock<std::shared_mutex> lock_r(mutex_);
    return read_session.empty();
}


void RpcSession::write_end_splice(std::list<Session>& sessions,std::list<Session>::iterator it) {
    std::unique_lock<std::shared_mutex> lock_r(mutex_);
    write_session.splice(write_session.end(),sessions,it);
    find_write_session[it->_controller] = it;
}

void RpcSession::write_erase(google::protobuf::RpcController* controller) {
    std::unique_lock<std::shared_mutex> lock_r(mutex_);
    write_session.erase(find_write_session[controller]);
    find_write_session.erase(controller);
}

bool RpcSession::write_count(google::protobuf::RpcController* controller){
    std::shared_lock<std::shared_mutex> lock_r(mutex_);
    return find_write_session.count(controller);
}

std::list<Session>::iterator RpcSession::write_find(google::protobuf::RpcController* controller) {
    std::shared_lock<std::shared_mutex> lock_r(mutex_);
    return find_write_session[controller];
}



















