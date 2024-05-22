#ifndef RPCCHANNEL_H
#define RPCCHANNEL_H

#include<google/protobuf/service.h>
#include"rpc/rpcClient.h"


class MyRpcChannel:public google::protobuf::RpcChannel
{ 
public:
    MyRpcChannel(RpcClient* client) : _client(client) {};
    ~MyRpcChannel() = default;

    //所有通过stub代理对象调用rpc方法，最终都是通过这个接口函数统一做数据的序列化和网络发送
    void CallMethod(const google::protobuf::MethodDescriptor* method,
                          google::protobuf::RpcController* controller, 
                          const google::protobuf::Message* request,
                          google::protobuf::Message* response, 
                          google::protobuf::Closure* done) override;

    //void connect(google::protobuf::RpcController *controller);



private:
    RpcClient* _client;

};
 
#endif