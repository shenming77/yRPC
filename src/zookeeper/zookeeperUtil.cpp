#include"zookeeper/zookeeperUtil.h"
#include"rpc/rpcConfig.h"
#include"log/log.h"

void global_watcher(zhandle_t *zh, int type,
                        int state,const char* path,void *watherCtx) {
    if(type == ZOO_SESSION_EVENT)
    {
        if(state == ZOO_CONNECTED_STATE)
        {
            sem_t *sem = (sem_t*)zoo_get_context(zh);
            sem_post(sem);
        }
    }
}


ZkClient::ZkClient():m_zhandle(nullptr) {}

ZkClient::~ZkClient() {
    if(m_zhandle != nullptr)
    {
        // 关闭句柄，释放资源 
        zookeeper_close(m_zhandle);
    }
}

void ZkClient::start() {
    std::string host = RpcConfig::Instance()->Load("zookeeperip");
    std::string port = RpcConfig::Instance()->Load("zookeeperport");
    std::string connstr = host + ":" + port;
    std::cout<<"connect zookeeper, zookeeper"<<host<<":"<<port<<std::endl;
    LOG_INFO("connect zookeeper, zookeeper- %s:%s",host.c_str(),port.c_str());

    m_zhandle = zookeeper_init(connstr.c_str(),global_watcher,30000,nullptr,nullptr,0);
    if(m_zhandle == nullptr)
    {
        std::cout<<"zookeeper_init error!"<<std::endl;
        LOG_ERROR("zookeeper_init error!");
        exit(EXIT_FAILURE);
    }

    sem_t sem;
    sem_init(&sem,0,0);
    zoo_set_context(m_zhandle,&sem);

    sem_wait(&sem);
    //std::cout<<"zookeeper init success!"<<std::endl;
    LOG_INFO("zookeeper init success!");

}

void ZkClient::create(const char *path, const char *data, int datalen, int state) {
    char path_buffer[128];
    int bufferlen = sizeof(path_buffer);
    int flag;
    flag = zoo_exists(m_zhandle,path,0,nullptr);
    if(ZNONODE == flag)
    {
        flag = zoo_create(m_zhandle,path,data,datalen,
            &ZOO_OPEN_ACL_UNSAFE,state,path_buffer,bufferlen);
        if(flag == ZOK)
        {
            //std::cout<<"znode create success... path:"<<path<<std::endl;
            LOG_INFO("znode create success... path:%s",path);
        }
        else
        {
            //std::cout<<"flag:"<<flag<<std::endl;
            //std::cout<<"znode create error... path:"<<path<<std::endl;
            LOG_ERROR("znode create success... path:%s",path);
            exit(EXIT_FAILURE);
        }

    }
}

std::string ZkClient::getData(const char *path) {
    char buffer[64];
    int bufferlen = sizeof(buffer);
    int flag = zoo_get(m_zhandle,path,0,buffer,&bufferlen,nullptr);
    if(flag != ZOK)
    {
        //std::cout<<"get znode error...path:"<<path<<std::endl;
        LOG_ERROR("get znode error... path:%s",path);
        return "";
    }
    else
    {
        return buffer; 
    }
}
