#include"rpc/rpcServer.h"
#include"rpc/rpcConfig.h"

RpcServer::RpcServer(const std::string& config_file ,const std::string& log_file): 
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
    Log::Instance()->init(1, "../log/service", ".log", 1024); //日志等级:1 日志异步队列容量:1024
    RpcConfig::Instance()->LoadConfigFile(config_file_);
    
    zkclient->start();
    connEvent_ = EPOLLONESHOT | EPOLLRDHUP | EPOLLET;
    listenEvent_ = EPOLLRDHUP | EPOLLET;
    if(!InitSocket_()) isClose_ = true;

}

RpcServer::~RpcServer(){
    close(listenFd_);
    isClose_ = true;
    free(srcDir_);
}

//主线程
void RpcServer::NotifyService(google::protobuf::Service *service) {
    ServiceInfo service_info;//服务类型信息
    // 获取了服务对象的描述信息
    const google::protobuf::ServiceDescriptor* serviceDesc = service->GetDescriptor();
    std::string service_name = serviceDesc->name();// 获取服务的名字
    int methodCnt = serviceDesc->method_count();// 获取服务对象service的方法数量

    std::cout<<"service_name:"<<service_name<<std::endl;
    LOG_INFO("service_name: %s",service_name.c_str());

    for(int i = 0; i < methodCnt; ++i)
    {
        // 获取了服务对象指定下标的服务方法的描述(抽象描述)
        const google::protobuf::MethodDescriptor* methodDesc = serviceDesc->method(i);
        std::string method_name = methodDesc->name();
        service_info.m_methodMap[method_name] = methodDesc;

        std::cout<<"service_method:"<<method_name<<std::endl;
        LOG_INFO("service_method: %s",method_name.c_str());
    }
    service_info.m_service = service;
    m_servicMap[service_name] = service_info;
}

void RpcServer::Registerzk() {
       //service_name为永久性节点  method_name为临时性节点
    for(auto &sp : m_servicMap)
    {
        std::string service_path = "/" + sp.first;
        zkclient->create(service_path.c_str(),nullptr,0);
        for(auto &mp :sp.second.m_methodMap)
        {
            // 存储当前这个rpc服务节点主机的ip和port
            std::string method_path = service_path + "/" +mp.first;
            char method_path_data[128] = {0};
            sprintf(method_path_data,"%s:%d",host_.c_str(),port_);
            // ZOO_EPHEMERAL 表示一个临时节点
            zkclient->create(method_path.c_str(),method_path_data,strlen(method_path_data),ZOO_EPHEMERAL);
        }
    }
}


//主线程
void RpcServer::Run() {
    Registerzk();//注册节点

    int timeMS = -1;  /* epoll wait timeout == -1 无事件将阻塞 */
    if(!isClose_) { LOG_INFO("========== server start =========="); }
    while(!isClose_) {
        // if(timeoutMS_ > 0) {
        //     timeMS = timer_->GetNextTick();
        // }
        int eventCnt = epoller_->Wait(timeMS);
        for(int i = 0; i < eventCnt; i++) {
            /* 处理事件 */
            int fd = epoller_->GetEventFd(i);
            uint32_t events = epoller_->GetEvents(i);
            if(fd == listenFd_) {
                DealListen_();
            }
            if(events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) {
                CloseConn_(fd);
            }
            else if(events & EPOLLIN) {
                //std::cout<<"RpcServer::DealRead_"<<std::endl;
                DealRead_(fd_session[fd]);
            }
            else if(events & EPOLLOUT) {
                //std::cout<<"RpcServer::DealRead_"<<std::endl;
                DealWrite_(fd_session[fd]);
            } else {
                LOG_ERROR("Unexpected event");
            }
        }
    }
}

void RpcServer::DealListen_(){
    struct sockaddr_in addr;
    socklen_t len = sizeof(addr);
    do {
        int fd = accept(listenFd_, (struct sockaddr *)&addr, &len);
        if(fd <= 0) { 
            return;
        }
        
        fd_session[fd].init(fd);
        epoller_->AddFd(fd, EPOLLIN | connEvent_);
        SetFdNonblock(fd);
        LOG_INFO("Client[%d] in!", fd);
    } while(listenEvent_ & EPOLLET);
}

void RpcServer::CloseConn_(int fd) {

    LOG_INFO("Client[%d] quit!", fd);
    epoller_->DelFd(fd);
    close(fd);
}

//线程池访问
void RpcServer::DealRead_(RpcSessiondata& sessions) {
    threadpool_->commit(std::bind(&RpcServer::Read_, this, std::ref(sessions)));
}

//线程池访问
void RpcServer::DealWrite_(RpcSessiondata& sessions) {
    threadpool_->commit(std::bind(&RpcServer::Write_, this, std::ref(sessions)));
}

void RpcServer::Read_(RpcSessiondata& sessions) {
    char buffer[4096];  // 接收缓冲区
    size_t _data_length = 0;
    ssize_t bytes_received;
    rpc::RpcRequestData rpc_data;
    google::protobuf::RpcController* controller;
    std::list<Sessiondata>::iterator session;

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
                break;  // 退出switch，处理错误或者不完整数据
            }
			case STATE::ST_DATA: {
                bytes_received = recv(sessions.GetFd(), buffer, _data_length, 0);
                if (bytes_received > 0) {
                    if (bytes_received == _data_length) {
                        // 处理接收到的数据
                        rpc_data.ParseFromArray(buffer,_data_length);
                        //解析并回调
                        read_session(rpc_data,sessions);

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
				return;
			}
		}

        epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLOUT);
        return;  // 退出函数，等待下一次事件通知
	}
}

void RpcServer::read_session(rpc::RpcRequestData& rpc_data,RpcSessiondata& sessions){
    std::string method_path = rpc_data.method_path();
    google::protobuf::RpcController* controller = reinterpret_cast<google::protobuf::RpcController*>(rpc_data.call_address());
    
    size_t start = 1; // 跳过开头的 '/'
    size_t _pos = method_path.find('/', start);
    std::string service_name, method_name;
    if (_pos != std::string::npos) {
        service_name = method_path.substr(start, _pos - start);// 提取 service_name
        method_name = method_path.substr(_pos + 1);  // 提取 method_name
        LOG_INFO("Service[%d] Service Name: %s, Method Name: %s", sessions.GetFd(), service_name.c_str(), method_name.c_str());
    } else {
        LOG_ERROR("Error STATE state , %d", sessions.GetFd());
    }
    //获取service对象和method对象
    if(!m_servicMap.count(service_name)){
        LOG_ERROR("%s is not exit!",service_name.c_str());
        return;
    }
    if(!m_servicMap[service_name].m_methodMap.count(method_name)){
        LOG_ERROR("%s:%s is not exit!",service_name,method_name);
        return;
    }
    // 获取service对象
    google::protobuf::Service* service = m_servicMap[service_name].m_service;
    const google::protobuf::MethodDescriptor* method = m_servicMap[service_name].m_methodMap[method_name];
    
    google::protobuf::Message* request = service->GetRequestPrototype(method).New();
    if(!request->ParseFromString(rpc_data.content())){
        LOG_ERROR("request parse error,content:%s",rpc_data.content());
        return;
    }
    google::protobuf::Message* response = service->GetResponsePrototype(method).New();
    service->CallMethod(method,nullptr,request,response,nullptr);

	std::string content,ret_data;
    response->SerializeToString(&content);
    rpc::RpcResponseData response_data;
    response_data.set_call_address(rpc_data.call_address());
    response_data.set_content(content);
    response_data.SerializeToString(&ret_data);
    delete request;
	delete response;
    
    sessions.begin_emplace(controller,ret_data);
}

void RpcServer::Write_(RpcSessiondata& sessions) {
    //取出发送数据
    std::list<Sessiondata> session_cursead;

    std::cout<<"RpcServer::Write_"<<std::endl;
    if(sessions.empty()){
        epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLIN);
        return ;
    }
    sessions.data_out(session_cursead);
    std::string data_buf,_constent;
    LENGTH_TYPE header;
    struct iovec iov_[2];

    LOG_INFO("Server[%d] send size: %d", sessions.GetFd(),session_cursead.size());
    for(auto session = session_cursead.begin(); session != session_cursead.end();){
        header = session->_sessiondata.size();
        iov_[0].iov_base = &header;
        iov_[0].iov_len = HEAD_LEN;
        iov_[1].iov_base = const_cast<char*>(session->_sessiondata.c_str());
        iov_[1].iov_len = session->_sessiondata.size();

        if (-1 == write_session(sessions.GetFd(),iov_)) {
			//session._controller->SetFailed("bufferevent_write Failed");
            auto cur_session = session++; 
            //没发送的添加到发送组
            sessions.end_splice(session_cursead,cur_session);
		}else {
            ++session;
        }
    }
    session_cursead.clear();

    epoller_->ModFd(sessions.GetFd(), connEvent_ | EPOLLIN);
}

ssize_t RpcServer::write_session(int fd,iovec iov_[2]){
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


bool RpcServer::InitSocket_() {
    bool openLinger_ = false; 
    int ret;
    host_ = RpcConfig::Instance()->Load("rpcserverip");
    port_ = stoi(RpcConfig::Instance()->Load("rpcserverport"));

    struct sockaddr_in addr;
    if(port_ > 65535 || port_ < 1024) {
        LOG_ERROR("Port:%d error!",  port_);
        return false;
    }
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = inet_addr(host_.c_str());
    addr.sin_port = htons(port_);
    struct linger optLinger = { 0 };
    if(openLinger_) {
        /* 优雅关闭: 直到所剩数据发送完毕或超时 */
        optLinger.l_onoff = 1;
        optLinger.l_linger = 1;
    }

    listenFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if(listenFd_ < 0) {
        LOG_ERROR("Create socket %d error!", port_);
        return false;
    }
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_LINGER, &optLinger, sizeof(optLinger));
    if(ret < 0) {
        close(listenFd_);
        LOG_ERROR("Init linger error!", port_);
        return false;
    }
    int optval = 1;
    /* 端口复用 */
    /* 只有最后一个套接字会正常接收数据。 */
    ret = setsockopt(listenFd_, SOL_SOCKET, SO_REUSEADDR, (const void*)&optval, sizeof(int));
    if(ret == -1) {
        LOG_ERROR("set socket setsockopt error !");
        close(listenFd_);
        return false;
    }
    ret = bind(listenFd_, (struct sockaddr *)&addr, sizeof(addr));
    if(ret < 0) {
        LOG_ERROR("Bind Port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = listen(listenFd_, 6);
    if(ret < 0) {
        LOG_ERROR("Listen port:%d error!", port_);
        close(listenFd_);
        return false;
    }
    ret = epoller_->AddFd(listenFd_,  listenEvent_ | EPOLLIN);
    if(ret == 0) {
        LOG_ERROR("Add listen error!");
        close(listenFd_);
        return false;
    }
    SetFdNonblock(listenFd_);
    LOG_INFO("Server port:%d", port_);
    return true;
}

int RpcServer::SetFdNonblock(int fd) {
    assert(fd > 0);
    return fcntl(fd, F_SETFL, fcntl(fd, F_GETFD, 0) | O_NONBLOCK);
}

