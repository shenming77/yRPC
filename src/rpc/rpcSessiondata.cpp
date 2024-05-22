#include"rpc/rpcSessiondata.h"

void RpcSessiondata::init(int _fd){
    fd = _fd;
    fd_state = STATE::ST_HEAD;
    session.clear();
    find_session.clear();
}

int RpcSessiondata::GetFd(){
    return fd;
}

void RpcSessiondata::begin_emplace(google::protobuf::RpcController *controller,
                  const std::string& sessiondata){
    find_session[controller] = session.emplace(session.begin(),controller,sessiondata);
}

void RpcSessiondata::data_out(std::list<Sessiondata>& sessions){
    sessions.splice(sessions.end(),session);
    find_session.clear();
}

bool RpcSessiondata::empty(){
    return session.empty();
}

void RpcSessiondata::end_splice(std::list<Sessiondata>& sessions,std::list<Sessiondata>::iterator it){
    session.splice(session.end(),sessions,it);
    find_session[it->_controller] = it;
}

void RpcSessiondata::erase(google::protobuf::RpcController* controller){
    session.erase(find_session[controller]);
    find_session.erase(controller);
}






