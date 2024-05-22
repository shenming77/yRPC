#include<iostream>
#include<google/protobuf/service.h>
#include<google/protobuf/descriptor.h>
#include"protobuf/user.pb.h"
#include"rpc/rpcChannel.h"
#include"rpc/rpcClient.h"
#include"rpc/rpcController.h"

void FooDone(fixbug::LoginResponse *response, RpcController *controller) {
	if (controller->Failed()) {
		std::cout<<"test  Rpc Failed :"<<controller->ErrorText().c_str()<<std::endl;;
	} else {
		std::cout<<"++++++ test  Rpc Response is "<< (response->sucess()?"sucess":"fail")<<std::endl;
	}
}


int main(int argc,char** argv)
{
    RpcClient client;
    MyRpcChannel channel(&client);
    fixbug::UserServiceRpc_Stub echo_clt(&channel);
    
    fixbug::LoginRequest request1;
    fixbug::LoginResponse response1;
    RpcController controller1;
    request1.set_name("zhang san");
    request1.set_pwd("12345687");
    // 发起rpc方法调用
    echo_clt.Login(&controller1,&request1,&response1,google::protobuf::NewCallback(&FooDone, &response1, &controller1));

    fixbug::LoginRequest request2;
    RpcController controller2;
    request2.set_name("li shi");
    request2.set_pwd("12345");
    fixbug::LoginResponse response2;
    echo_clt.Login(&controller2,&request2,&response2,google::protobuf::NewCallback(&FooDone, &response2, &controller2));

    fixbug::LoginRequest request3;
    RpcController controller3;
    request3.set_name("yang er");
    request3.set_pwd("123456789");
    fixbug::LoginResponse response3;
    echo_clt.Login(&controller3,&request3,&response3,google::protobuf::NewCallback(&FooDone, &response3, &controller3));

    sleep(30);
    return 0;
}






