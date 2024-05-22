#include<iostream>
#include<string>
#include"protobuf/user.pb.h"
#include"rpc/rpcServer.h"
#include"rpc/rpcController.h"
#include"log/log.h"
 
/*
UserService原来是一个本地服务，提供了两个进程内的本地方法
*/

class UserService:public fixbug::UserServiceRpc //使用rpc服务发布端(rpc服务提供者)
{
public:
    bool Login(std::string name,std::string pwd)
    {
        std::cout<<"doing local services: Login"<<std::endl;
        std::cout<<"name: "<<name<<"  pwd: "<<pwd<<std::endl;
        return true;
    }

    //重写基类UserServiceRpc的虚函数，下面这些方法都是框架直接调用的 
    void Login(::google::protobuf::RpcController *controller,
               const ::fixbug::LoginRequest *request,
               ::fixbug::LoginResponse *response,
               ::google::protobuf::Closure *done)
    {
        // 框架给业务上报请求参数LoginRequest,应用获取相应数据做本地业务
        std::string name = request->name();
        std::string pwd = request->pwd();

        // 做本地业务
        bool login_result = Login(name,pwd);

        // 把响应写入 包括错误码、错误消息、返回值
        fixbug::ResultCode *code = response->mutable_result();
        code->set_errcode(0);
        code->set_errmsg("");
        response->set_sucess(login_result);

        if(done) done->Run();
    }
};

int main(int argc, char **argv)
{

    //provide是一个rpc网络服务对象，把UserService对象发布到rpc节点上
    RpcServer server;
    server.NotifyService(new UserService());

    //启动一个rpc服务发布节点  Run以后，进程进入阻塞状态，等待远程调用rpc调用请求
    server.Run();

    return 0;
}
