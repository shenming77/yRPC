#include<iostream>
#include<fstream>
#include"rpc/rpcConfig.h"



// 负责解析加载配置文件
void RpcConfig::LoadConfigFile(const std::string& config_file)
{
    YAML::Node config = YAML::LoadFile(config_file);
    
    // 遍历YAML节点
    for (YAML::const_iterator it = config.begin(); it != config.end(); ++it) {
        std::string key = it->first.as<std::string>();
        std::string value = it->second.as<std::string>();
        m_configMap[key] = value;
    }

}

// 查询配置项信息
std::string RpcConfig::Load(const std::string& key)
{
    auto it = m_configMap.find(key);
    if(it == m_configMap.end())
    {
        return "no find";
    }
    return it->second;
}

RpcConfig* RpcConfig::Instance() {
    static RpcConfig inst;
    return &inst;
}
