#include"rpc/rpcClient.h"
#include"rpc/rpcConfig.h"


RpcClient::RpcClient(const std::string& config_file ,const std::string& log_file): 
        config_file_(std::move(config_file)), log_file_(std::move(log_file)),isClose_(false),
        zkclient(new ZkClient()),threadpool_(new YThreadPool::ThreadPool(true)), epoller_(new Epoller(128)){
    srcDir_ = getcwd(nullptr, 256);
    assert(srcDir_);
    char* buildPos = strstr(srcDir_, "/build");
    if (buildPos != nullptr) *buildPos = '\0';  // 终止原字符串于 "/build" 起始位置

    if(config_file_.empty()||log_file_.empty()){
        log_file_ = std::string(srcDir_) + "/log/client";
        config_file_ = std::string(srcDir_) + "/config/parameter.yml";
    }
    Log::Instance()->init(1, "../log/client", ".log", 1024); //日志等级:1 日志异步队列容量:1024
    RpcConfig::Instance()->LoadConfigFile(config_file_);
    
    zkclient->start();
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP | EPOLLET;
    if(!InitSocket_()) isClose_ = true;

    thread_ = std::move(std::thread(&RpcClient::run, this));
}

RpcClient::~RpcClient(){

    isClose_ = true;
    free(srcDir_);
}

//主访问
bool RpcClient::is_connect(const std::string& method_path){ 
    std::shared_lock<std::shared_mutex> locker(mutex_);
    return (!methodPath2fd[method_path].empty())&&methodPath_curfd.count(method_path);
}

//主访问
void RpcClient::connect_(const std::string&method_path,google::protobuf::RpcController *controller){
    std::string ip;
    uint16_t port;
    std::tie(ip, port) = getServiceipport(method_path,controller);

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        controller->SetFailed("create socket error!");
        LOG_ERROR("create socket error!");
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    if(-1 == connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr))){
        LOG_ERROR("connect error! errno: %d",errno);
        std::string false_msg = "connect error! errno: ";
        false_msg += errno;
        controller->SetFailed(false_msg);
        close(sockfd);
    }
    SetFdNonblock(sockfd);
    epoller_->AddFd(sockfd, EPOLLOUT | connEvent_);
    LOG_INFO("connect  %s : %d", ip.c_str(),port);
    
    std::unique_lock<std::shared_mutex> locker(mutex_);
    fd_find[sockfd] = methodPath2fd[method_path].emplace(methodPath2fd[method_path].end(),sockfd,method_path);//添加文件描述符
    methodPath_curfd[method_path] = sockfd;
    auto& sessions = fd_sessions[sockfd];
    locker.unlock();
    sessions.init(sockfd);
}


std::pair<std::string,uint16_t> RpcClient::getServiceipport(const std::string&method_path,google::protobuf::RpcController *controller){
    std::string host_data = zkclient->getData(method_path.c_str());
    if(host_data == ""){
        std::string false_msg = method_path;
        false_msg += "connect error! errno: ";
        controller->SetFailed(false_msg);
        LOG_ERROR("%s is not exit!",method_path);
        return {};
    }
    int idx = host_data.find(":");
    if(idx == std::string::npos) {
        std::string false_msg = method_path;
        false_msg += " address is invilid";
        controller->SetFailed(false_msg);
        LOG_ERROR("%s address is invilid",method_path);
        return {};
    }
    std::string ip = host_data.substr(0,idx);
    uint16_t port = atoi(host_data.substr(idx+1,host_data.size()-idx).c_str());
    return std::make_pair(ip,port);
}


//副访问
void RpcClient::run() {
    std::shared_lock<std::shared_mutex> locker(mutex_,std::defer_lock);

    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== client start =========="); }
    while(!isClose_) {
        // if(timeoutMS_ > 0) {
        //     timeMS = timer_->GetNextTick();
        // }
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                CloseConn_(fd);
            }
            else if(events & EPOLLIN) {
                locker.lock();
                DealRead_(fd_sessions[fd]);
                //std::cout<<"RpcClient::DealRead_"<<std::endl;
                locker.unlock();
            }
            else if(events & EPOLLOUT) {
                locker.lock();
                DealWrite_(fd_sessions[fd]);
                //std::cout<<"RpcClient::DealWrite_"<<std::endl;
                locker.unlock();
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

//主访问
bool RpcClient::CallSession(google::protobuf::RpcController *controller,
                const google::protobuf::Message *request,
                google::protobuf::Message *response,
                google::protobuf::Closure *c,
                const std::string& method_path){
    std::shared_lock<std::shared_mutex> locker(mutex_);
    int fd = methodPath_curfd[method_path];
    RpcSession& sessions = fd_sessions[fd];
    locker.unlock();
    
    if(!(sessions.read_count(controller)||sessions.write_count(controller))){
        sessions.read_begin_emplace(controller,request,response,c,fd,method_path);
    }else{
        controller->SetFailed("controller is reused");
        LOG_ERROR("controller is reused");
        return false;
    }
    return true;
}

//副访问
void RpcClient::CloseConn_(int fd) {
    std::unique_lock<std::shared_mutex> locker(mutex_);
    auto&& method = std::move(*fd_find[fd]);
    methodPath2fd[method.second].erase(fd_find[fd]);
    if(methodPath2fd[method.second].empty())methodPath_curfd.erase(method.second);
    locker.unlock();

    LOG_INFO("Client[%d] quit!", fd);
    epoller_->DelFd(fd);
    close(fd);
}

//线程池访问
void RpcClient::DealRead_(RpcSession& sessions) {
    threadpool_->commit(std::bind(&RpcClient::Read_, this, std::ref(sessions)));
}

//线程池访问
void RpcClient::DealWrite_(RpcSession& sessions) {
    threadpool_->commit(std::bind(&RpcClient::Write_, this, std::ref(sessions)));
}

//线程池访问
void RpcClient::Read_(RpcSession& sessions) {
    char buffer[4096];  // 接收缓冲区
    size_t _data_length = 0;
    ssize_t bytes_received;
    rpc::RpcResponseData rpc_data;
    google::protobuf::RpcController* controller;
    std::list<Session>::iterator session;
    std::cout<<"RpcClient::Read_"<<std::endl;

	for (;;) {
		switch (sessions.fd_state) {
			case STATE::ST_HEAD: {
                bytes_received = recv(sessions.GetFd(), buffer, HEAD_LEN, 0);
                if (bytes_received > 0) {
                    if (bytes_received == HEAD_LEN) {
                        memcpy(&_data_length, buffer, HEAD_LEN);
                        sessions.fd_state = STATE::ST_DATA;
                        continue;  // 直接继续处理数据部分
                    }
                } else if (bytes_received == 0 || (bytes_received < 0 )) {
                    // 连接关闭或发生错误
                    epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLOUT);
                    LOG_ERROR("Connection closed or error receiving data 1");
                    return;  // 退出函数
                }
                break;
			}
			case STATE::ST_DATA: {
                bytes_received = recv(sessions.GetFd(), buffer, _data_length, 0);
                if (bytes_received > 0) {
                    if (bytes_received == _data_length) {
                        // 处理接收到的数据
                        rpc_data.ParseFromArray(buffer,_data_length);
                        controller = reinterpret_cast<google::protobuf::RpcController*>(rpc_data.call_address());
                        if(!sessions.write_count(controller)){
                            LOG_ERROR("Parsing RpcResponseData failed");
                            epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLOUT);
                            return ;
                        }
                        session = sessions.write_find(controller);
                        //if(sessions.write_count(controller))std::cout<<"controller :"<<controller<<std::endl;

                        //解析并回调
                        session->_response->ParseFromString(rpc_data.content());
                        if(session->_c)session->_c->Run();
                        //删除session
                        sessions.write_erase(controller);
                        sessions.fd_state = STATE::ST_HEAD;
                        continue ;
                    }
                }else if (bytes_received == 0 || (bytes_received < 0 )) {
                    // 连接关闭或发生错误
                    epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLOUT);
                    LOG_ERROR("Connection closed or error receiving data 2");
                    return;  // 退出函数
                }
                break;
			} 
            default: {
				LOG_ERROR("Error STATE state");
				break;
			}
        }
        epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLOUT);
        return;  // 退出函数，等待下一次事件通知
	}
}

//线程池访问
void RpcClient::Write_(RpcSession& sessions) {
    //取出发送数据
    std::list<Session> session_cursead;

    if(sessions.read_empty()){
        epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLIN);
        return ;
    }
    sessions.read_data_out(session_cursead);

    rpc::RpcRequestData rpc_data;
    std::string data_buf,_constent;
    LENGTH_TYPE header;
    struct iovec iov_[2];
    //全部发送
    for(auto session = session_cursead.begin(); session != session_cursead.end();){
        session->_request->SerializeToString(&_constent);
        rpc_data.set_method_path(session->_methodPath);
        rpc_data.set_call_address(reinterpret_cast<uintptr_t>(session->_controller));
        rpc_data.set_content(_constent);
        rpc_data.SerializeToString(&data_buf);
        header = data_buf.size();
        iov_[0].iov_base = &header;
        iov_[0].iov_len = HEAD_LEN;
        iov_[1].iov_base = const_cast<char*>(data_buf.c_str());
        iov_[1].iov_len = data_buf.size();

        if (-1 == write_session(sessions.GetFd(),iov_)) {
			//session._controller->SetFailed("bufferevent_write Failed");
            auto cur_session = session++; 
            //没发送的添加到发送组
            sessions.read_end_splice(session_cursead,cur_session);
		}else {
            ++session;
        }
    }
    //添加到接受队列
    for(auto session = session_cursead.begin(); session != session_cursead.end();){
        auto cur_session = session++; 
        sessions.write_end_splice(session_cursead,cur_session);
    }
    
    epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLIN);
}

ssize_t RpcClient::write_session(int fd,iovec iov_[2]){
    ssize_t len = -1;
    do {
        len = writev(fd, iov_, 2);
        if(len <= 0) {
            break;
        }
        if(iov_[0].iov_len + iov_[1].iov_len  == 0) { break; } /* 传输结束 */
        else if(static_cast<size_t>(len) > iov_[0].iov_len) {
            iov_[1].iov_base = (uint8_t*) iov_[1].iov_base + (len - iov_[0].iov_len);
            iov_[1].iov_len -= (len - iov_[0].iov_len);
            if(iov_[0].iov_len) {
                iov_[0].iov_len = 0;
            }
        }
        else {
            iov_[0].iov_base = (uint8_t*)iov_[0].iov_base + len; 
            iov_[0].iov_len -= len; 
        }
    } while(true);
    return len;
}

bool RpcClient::InitSocket_() {
     return true;
}

int RpcClient::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}
