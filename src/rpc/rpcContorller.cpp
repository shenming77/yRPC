#include "rpc/rpcController.h"

RpcController::RpcController():m_false(false),m_errorText("") {
}

RpcController::~RpcController() {
    Reset();
}

void RpcController::Reset() {
    m_false = false;
    m_errorText = "";
}

bool RpcController::Failed() const {     
    return m_false;
}

std::string RpcController::ErrorText() const {
    return m_errorText;
}

void RpcController::SetFailed(const std::string &reason) {
    m_false = true;
    m_errorText = reason;
}

void RpcController::StartCancel() {
}


bool RpcController::IsCanceled() const {
    return false;
}

void RpcController::NotifyOnCancel(google::protobuf::Closure *callback) {
}
