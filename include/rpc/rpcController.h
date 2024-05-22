# ifndef RPCCONTROLLER_H
#define RPCCONTROLLER_H

#include<string>
#include<google/protobuf/service.h>

class RpcController:public google::protobuf::RpcController
{
public:
    RpcController();

    ~RpcController();

    //重置控制器
    void Reset();

    //获得RPC方法执行状态
    bool Failed() const;

    //获得控制器错误信息
    std::string ErrorText() const;

    //设置控制器状态和错误信息
    void SetFailed(const std::string &reason);

    void StartCancel();

    bool IsCanceled() const;

    void NotifyOnCancel(google::protobuf::Closure *callback);

private:
    // RPC方法执行状态
    bool m_false;
    // RPC执行错误信息
    std::string m_errorText;
};

#endif