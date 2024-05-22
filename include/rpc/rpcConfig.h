#ifndef RPCCONFIG_H
#define RPCCONFIG_H

#include<unordered_map>
#include<string>
#include<yaml-cpp/yaml.h>

//框架读取配置文件
//rpcserverip rpcserverport zookeeperip zookeeperport
class RpcConfig
{
public:
    RpcConfig() = default;
    RpcConfig(const RpcConfig&) = delete;
    const RpcConfig& operator=(const RpcConfig&) = delete;
    ~RpcConfig() = default;

    //负责解析加载配置文件
    void LoadConfigFile(const std::string& config_file);

    //查询配置项信息
    std::string Load(const std::string& key);

    static RpcConfig* Instance();


private:
    //配置项映射表
    std::unordered_map<std::string,std::string>m_configMap;
};

# endif