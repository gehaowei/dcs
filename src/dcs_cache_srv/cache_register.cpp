#include "cache_register.h"
#include "zookeeper_client.h"
#include "logger.h"
#include "common.h"

#include "cache_srv.h"

#include "memcache_client.h"
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

namespace DCS {

CacheRegister::CacheRegister()
{
    m_ZookeeperClient = NULL;

    pthread_t id;

    //启动接收master指令线程
    int retCode=pthread_create(&id, NULL, ThreadUdpRecv, NULL);
    if(0 != retCode)
    {
        DCS_LOG(WARN, "pthread_create ThreadUdpRecv.retCode=" << retCode);
    }

    //启动健康维护线程
    retCode=pthread_create(&id, NULL, ThreadMemProbe, NULL);
    if(0 != retCode)
    {
        DCS_LOG(WARN, "pthread_create ThreadMemProbe.retCode=" << retCode);
    }
}

CacheRegister::~CacheRegister()
{

}

//-------------------------------------------------------------------------------
// 函数名称: RegistAll
// 函数功能: 向zookeeper注册所有，通过读取配置文件获取zookeeper和mem信息
// 输入参数: const string& confFile 配置文件全路径名
// 输出参数: 无
// 返回值  : 成功返回0;配置文件异常返回-1;zookeeper异常返回-2
//-------------------------------------------------------------------------------
int CacheRegister::RegistAll(const string& confFile)
{
    DCS_LOG(TRACE, "CacheRegister::RegistAll(confFile:" << confFile << ") begin.");

    int retCode=0;

    //加锁
    m_MemMutex.lock();

    //配置信息
    FileConfIni *conf=CacheServer::GetInstance()->GetCfg();
    if( confFile.length() )
    {
        conf = new FileConfIni(confFile);
    }

    //zookeeper初始化
    ZookeeperInit(conf);

    string zk_home = conf->getData("SYSTEM", "zk_home");

    //获取memcachelist
    int total = conf->getIntData("MEMLIST","total");
    m_MemInfos.clear();
    string sKey;
    string zkNode, nodeData;

    for(int i=0; i<total; i++)
    {
        string sKey = "mem" + IntToString(i);
        nodeData = conf->getData("MEMLIST",sKey);
        zkNode = zk_home + "/cacheserver/" + nodeData;

        m_MemInfos.insert( nodeData );

        //向zookeeper写临时文件
        int ret = m_ZookeeperClient->SetNode(zkNode.c_str(), nodeData.c_str(), nodeData.length(), 1);
        if(ret != 0 )
        {
            DCS_LOG(WARN, "cacheNode regist failed:" << zkNode );
            exit(1);
        }
    }

    //解锁
    m_MemMutex.unlock();

    if( confFile.length() )
    {
        delete conf;
    }

    DCS_LOG(TRACE, "CacheRegister::RegistAll() end.");

    return retCode;
}

//-------------------------------------------------------------------------------
// 函数名称: RepairMemRegist
// 函数功能: 修复单个mem注册信息
// 输入参数: const string& cacheNode mem注册文件名 格式ip:port
// 输出参数: 无
// 返回值  : 成功返回0;不在维护队列返回-1;zookeeper异常返回-2
//-------------------------------------------------------------------------------
int CacheRegister::RepairMemRegist(const string& cacheNode)
{
    DCS_LOG(TRACE, "CacheRegister::RepairMemRegist() begin.cacheNode:" << cacheNode);

    int retCode=0;

    //加锁
    m_MemMutex.lock();

    //zookeeper初始化
    ZookeeperInit(CacheServer::GetInstance()->GetCfg());

    string zk_home = CacheServer::GetInstance()->GetCfg()->getData("SYSTEM", "zk_home");


    //检查是否属于维护列表
    set< string >::iterator it = m_MemInfos.find(cacheNode);

    //属于维护列表则重新向zookeeper写文件
    if( it != m_MemInfos.end() )
    {
        string nodeData = it->c_str();

        string zkNode = zk_home + "/cacheserver/" + nodeData;

        //向zookeeper写临时文件
        int ret = m_ZookeeperClient->SetNode(zkNode.c_str(), nodeData.c_str(), nodeData.length(), 1);
        if(ret != 0 )
        {
            retCode=-2;
            DCS_LOG(WARN, "cacheNode regist failed:" << zkNode );
            exit(2);
        }
    }
    else
    {
        retCode=-1;
    }

    //解锁
    m_MemMutex.unlock();

    DCS_LOG(TRACE, "CacheRegister::RepairMemRegist() end.");

    return retCode;
}

int CacheRegister::ZookeeperInit(FileConfIni *conf)
{
    DCS_LOG(TRACE, "CacheRegister::ZookeeperInit() begin.");

    string zk_home = conf->getData("SYSTEM", "zk_home");

    if(!m_ZookeeperClient)
    {
        //zookeeper检查
        string zk_server = conf->getData("SYSTEM", "zk_server");

        //连接zookeeper
        m_ZookeeperClient = new ZookeeperClient(zk_server,MyWatcher);
    }

    //${zk_home}/cacheserver
    string cacheNode = zk_home + "/cacheserver";
    size_t idx=0;
    string dir;

    while(idx+1 < cacheNode.length())
    {
        dir = cacheNode.substr(0, cacheNode.find("/",idx+1));
        idx = dir.length();
        m_ZookeeperClient->SetNode(dir.c_str(), NULL, 0, 0);
    }

    DCS_LOG(TRACE, "CacheRegister::ZookeeperInit() end.");

    return 0;

}


//zookeeper回调
void CacheRegister::MyWatcher(int type, const char *path)
{
    DCS_LOG(TRACE, "CacheRegister::MyWatcher() begin.");

    DCS_LOG(DEBUG, "CacheRegister::MyWatcher()type:" << type);
    CacheRegister::GetInstance()->RegistAll();

    DCS_LOG(TRACE, "CacheRegister::MyWatcher() end.");
}

//接收master指令，以恢复异常节点数据
void *CacheRegister::ThreadUdpRecv( void *pVoid )
{
    DCS_LOG(TRACE, "CacheRegister::ThreadUdpRecv() begin");
	std::string cfgPort = CacheServer::GetInstance()->GetCfg()->getData( "SYSTEM", "cache_ice_port" );
    int listenPort = stringToInt(cfgPort);
    int ListenFd, ConnFd;
    struct sockaddr_in	cliaddr, servaddr;
    socklen_t   clilen;
    char sBuf[1024];

    ListenFd = ::socket(AF_INET, SOCK_DGRAM, 0);

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family      = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port        = htons(listenPort);

    int option = 1;
    ::setsockopt( ListenFd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option) );

    assert( ::bind(ListenFd, (struct sockaddr *) &servaddr, sizeof(servaddr)) != -1 );

    DCS_LOG(INFO, "int  CacheRegister::ThreadUdpRecv()-listenPort:" << listenPort);
    size_t len = 0;

    while(1)
    {
        bzero(&sBuf, sizeof(sBuf));
        if( (len = ::recvfrom( ListenFd, sBuf, sizeof(sBuf), 0, (struct sockaddr *) &cliaddr, &clilen) ) > 0 )
        {
            if(0 == ::strncmp(sBuf, "REPAIRNODE|", 11))
            {
                std::string cacheNode = sBuf+11;
                int nRet = CacheRegister::GetInstance()->RepairMemRegist(cacheNode);
                if(0 == nRet)
                {
                    sendto( ListenFd, "+OK", 3, 0, ( struct sockaddr * )&cliaddr, ( unsigned int )clilen );
                }

                DCS_LOG(DEBUG, "CacheRegister::ThreadUdpRecv()-sBuf:" << sBuf
                << ",client:" << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port)
                << ",nRet:" << nRet );
            }
        }
        else
        {
            DCS_LOG(DEBUG, "CacheRegister::ThreadUdpRecv()-recvfrom len:" << len
                << ",errno:" << errno );
        }
    }


    DCS_LOG(TRACE, "CacheRegister::ThreadUdpRecv() end");

    return NULL;
}


//维护memcached服务的健康
void *CacheRegister::ThreadMemProbe( void *pVoid )
{
    DCS_LOG(TRACE, "CacheRegister::ThreadMemProbe() begin");
    int lifeCycle=0;

    while(1)
    {
        //轮休机制,600分钟后退出
        if( lifeCycle++ > 300)
        {
            exit(1);
        }

        //2分钟
        TimeWait( 120*1000 );

        CacheRegister::GetInstance()->RegistAll();

        vector< string > memVecs;

        //取列表
        CacheRegister::GetInstance()->m_MemMutex.lock();

        set< string >::iterator itMem = CacheRegister::GetInstance()->m_MemInfos.begin();
        for(; itMem != CacheRegister::GetInstance()->m_MemInfos.end(); itMem++)
        {
            memVecs.push_back(*itMem);
        }

        CacheRegister::GetInstance()->m_MemMutex.unlock();

        //遍历探测健康状况
        int maxFailed=0;
        vector< string >::iterator itServer=memVecs.begin();
        for(; itServer != memVecs.end(); )
        {
            string memIp,memPort,memSize,cacheServer;
            cacheServer = *itServer;
            CutStringHead(cacheServer, ":", memIp, cacheServer);
            CutStringHead(cacheServer, ":", memPort, cacheServer);

            //异常情况
            if(memIp.length()<=0 || memPort.length() <= 0 )
            {
                DCS_LOG(WARN, "memVecs abnormal.itServer:"<< *itServer );
                itServer++;
                maxFailed=0;
                continue;
            }

            cacheServer = memIp + ":" + memPort;
            MemcacheClient *PMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(cacheServer);
            if(PMemCli)
            {
                int retCode=0;
                char *value=NULL;
                size_t retlen=0;

                retCode += PMemCli->CacheSet( "MemcacheClient", 14, "MemcacheClient", 14, 10);
                retCode += PMemCli->CacheGet( "MemcacheClient", 14, (void **) &value, &retlen);
                retCode += PMemCli->CacheDel( "MemcacheClient", 14 );
                DCS_LOG(DEBUG, "probe memcached server:"<< cacheServer << ",retCode:" << retCode << ",retlen:" << retlen );
                if( 0 == retCode && value && 14 == retlen )
                {
                    //释放内存
                    FREE(value);

                    //探测下一条
                    itServer++;
                    maxFailed=0;
                    continue;
                }

                //释放内存
                FREE(value);
            }


            //失败等待计数
            maxFailed++;
            TimeWait(1000);


            //达到最大失败次数则重启
            if(maxFailed>=10)
            {
                char sCmd[512]={0};
                snprintf(sCmd, sizeof( sCmd ) - 1,
                    "kill -9 `ps -u %s -o ppid,pid,cmd|grep memcached|grep -v grep|grep %s|grep %s|awk -F' ' '{if( $1==1) print $2}'`",
                    getenv("USER"), memIp.c_str(), memPort.c_str() );
                system( sCmd );

                DCS_LOG(INFO, "system sCmd:"<< sCmd );

                //探测下一条
                itServer++;
                maxFailed=0;
                continue;
            }
        }

    }

    DCS_LOG(TRACE, "CacheRegister::ThreadMemProbe() end");

    return NULL;
}


}


