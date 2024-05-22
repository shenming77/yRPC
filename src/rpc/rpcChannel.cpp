#include"rpc/rpcChannel.h"
#include"rpc/rpcController.h"
#include<string>
#include<sys/types.h>
#include<sys/socket.h>
#include<arpa/inet.h>
#include<unistd.h>
#include"rpc/rpcConfig.h"
#include"zookeeper/zookeeperUtil.h"
#include"log/log.h"

// 所有通过stub代理对象调用rpc方法，最终都是通过这个接口函数统一做数据的序列化和网络发送
void MyRpcChannel::CallMethod(const google::protobuf::MethodDescriptor *method,
                google::protobuf::RpcController *controller,
                const google::protobuf::Message *request,
                google::protobuf::Message *response,
                google::protobuf::Closure *done){
    const google::protobuf::ServiceDescriptor *sd = method->service();
    std::string service_name = sd->name();
    std::string method_name = method->name(); 
    std::string method_path = "/" + service_name + "/" + method_name;
    if(!_client->is_connect(method_path)){
        std::cout<<"connecting"<<std::endl;
        _client->connect_(method_path,controller);
        if(controller->Failed()) return;
    }
    if(!_client->CallSession(controller,request,response,done,method_path)){
        if(controller->Failed()) return;
    }

}






