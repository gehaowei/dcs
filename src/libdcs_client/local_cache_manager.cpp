#include "local_cache_manager.h"
#include "logger.h"
#include "memcache_client.h"
//#include "ice_implement.h"
#include "common.h"
#include "dcs_cache_manager.h"
#include <sys/wait.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

namespace DCS{

LocalCacheManager::LocalCacheManager()
{
    m_MemProcSize=0;
    m_SyncCond = new Cond(&m_SyncQueueMutex);
    m_ZookeeperClient = NULL;
}

LocalCacheManager::~LocalCacheManager()
{
}

//-------------------------------------------------------------------------------
// 函数名称: Init
// 函数功能: 初始化并启动
// 输入参数: conf配置文件指针
// 输出参数: 无
// 返回值  : 成功返回0;异常返回负数
//-------------------------------------------------------------------------------
int LocalCacheManager::Init(FileConfIni *conf)
{
    DCS_LOG(TRACE, "LocalCacheManager::Init() begin.");
    int retCode=0;

    //获取配置项
    m_ZkServers = conf->getData("DCS_CLIENT", "zk_server");
    m_ZkHome = conf->getData("DCS_CLIENT", "zk_home");

    //local cache open flag, 0:close,1:open
    m_local_cache_open = conf->getIntData("DCS_CLIENT", "local_cache_open");
    //module inner sync flag, 0:close,1:open
    //passive 被动   active 主动
    m_local_cache_active_sync = conf->getIntData("DCS_CLIENT", "local_cache_active_sync");
    m_local_cache_passive_sync = conf->getIntData("DCS_CLIENT", "local_cache_passive_sync");

    //module inner syn icnternal communication engine
    m_local_cache_module_name = conf->getData("DCS_CLIENT", "module_name");
    m_local_cache_ice = conf->getData("DCS_CLIENT", "local_cache_ice");; //localip:26303
    CutStringHead(m_local_cache_ice, ":", m_ICEIp, m_ICEPort);
    //local cache type : 0:mem of process, 1:mem of memcached server(127.0.0.1)
    m_local_cache_type = conf->getIntData("DCS_CLIENT", "local_cache_type");
    //when mem of process, mem max size can use (M)
    m_local_mem_max = conf->getIntData("DCS_CLIENT", "local_mem_max");

    //local memcached server port
    m_local_memcached_port = conf->getIntData("DCS_CLIENT", "local_memcached_port");

    //local memcached expire time
    m_local_memcached_expire = conf->getIntData("DCS_CLIENT", "local_memcached_expire");


    DCS_LOG(DEBUG, "LocalCacheManager::Init()m_local_cache_active_sync:" << m_local_cache_active_sync
        << ",m_local_cache_passive_sync:" << m_local_cache_passive_sync
        << ",m_local_memcached_expire:" << m_local_memcached_expire);


    //如果未开通则直接返回
    if(0 == m_local_cache_open)
    {
        return 0;
    }

    //如果开通模块间主动同步
    if(1 == m_local_cache_active_sync )
    {
        pthread_t id;

        //启动sync线程
        retCode=pthread_create(&id, NULL, ThreadCacheSync, NULL);
        if(0 != retCode)
        {
            DCS_LOG(WARN, "pthread_create ThreadCacheSync.retCode=" << retCode);
            return retCode;
        }
    }

    //如果开通模块间被动同步
    if(1 == m_local_cache_passive_sync && m_ICEPort.length() > 0 )
    {
        pthread_t id;

        //启动同步通道
        retCode=pthread_create(&id, NULL, ThreadSyncICE, NULL);
        if(0 != retCode)
        {
            DCS_LOG(WARN, "pthread_create ThreadSyncICE.retCode=" << retCode);
            return retCode;
        }

    }

    if(1 == m_local_cache_passive_sync || 1 == m_local_cache_active_sync )
    {
        //zookeeper注册同步通道
        retCode=ConnectZookeeper();
        if(0 != retCode)
        {
            DCS_LOG(WARN, "ConnectZookeeper().retCode=" << retCode);
            return retCode;
        }

        pthread_t id;

        //启动ThreadSyncZookeeper
        retCode=pthread_create(&id, NULL, ThreadSyncZookeeper, NULL);
        if(0 != retCode)
        {
            DCS_LOG(WARN, "pthread_create ThreadSyncZookeeper.retCode=" << retCode);
            return retCode;
        }
    }

    //如果开通本地memcached服务
    if(  1 == m_local_cache_type && m_local_memcached_port > 0 )
    {
        pthread_t id;

        //启动memcached服务探测
        retCode=pthread_create(&id, NULL, ThreadMemProbe, NULL);
        if(0 != retCode)
        {
            DCS_LOG(WARN, "pthread_create ThreadMemProbe.retCode=" << retCode);
            return retCode;
        }
    }

    DCS_LOG(TRACE, "LocalCacheManager::Init() end.");

    return retCode;
}

//set
int LocalCacheManager::CacheSet(const char *key, const size_t key_len, const void *val, const size_t bytes, const time_t expire, bool ifSync)
{
    DCS_LOG(TRACE, "LocalCacheManager::CacheSet() begin.");
    int retCode=0;
    if(0 == m_local_cache_open)
    {
        return retCode;
    }

    //本进程or服务
    if( 0 == m_local_cache_type )
    {
        m_MemMutex.lock();
        map< string, stMemProcNode *>::iterator it = m_MemProcNodeMap.find(key);
        if( it != m_MemProcNodeMap.end() )
        {
            stMemProcNode *pNode = it->second;

            if(pNode)
            {
                Remove(pNode);

                if(pNode->m_Data)
                {
                    m_MemProcSize -= pNode->m_Len;

                    //DCS_LOG(DEBUG,"delete [] pNode->m_Data len:" << pNode->m_Len );

                    delete [] pNode->m_Data;
                }

                if( pNode->m_Key )
                {
                    m_MemProcSize -= ( strlen(pNode->m_Key) + 1 + sizeof(string) + sizeof(stMemProcNode *) );

                    //DCS_LOG(DEBUG,"delete [] pNode->m_Key len:" << strlen(pNode->m_Key) );

                    m_MemProcNodeMap.erase(pNode->m_Key);

                    delete [] pNode->m_Key;

                }

                m_MemProcSize -= sizeof(stMemProcNode);
                delete pNode;
            }
        }

        //内存过载则删除
        stMemProcNode *pNode = NULL;
        while( m_MemProcSize > (m_local_mem_max*1024*1024 )
            && NULL != (pNode=GetTail()) )
        {
            Remove(pNode);

            if(pNode->m_Data)
            {
                m_MemProcSize -= pNode->m_Len;

                //DCS_LOG(DEBUG,"delete [] pNode->m_Data len:" << pNode->m_Len );

                delete [] pNode->m_Data;
            }

            if( pNode->m_Key )
            {
                m_MemProcSize -= ( strlen(pNode->m_Key) + 1 + sizeof(string) + sizeof(stMemProcNode *) );

                //DCS_LOG(DEBUG,"delete [] pNode->m_Key len:" << strlen(pNode->m_Key) );

                m_MemProcNodeMap.erase(pNode->m_Key);
                delete [] pNode->m_Key;
            }

            m_MemProcSize -= sizeof(stMemProcNode);
            delete pNode;
        }

        //添加内存
        pNode = new stMemProcNode();
        pNode->m_Key = new char[key_len+1];
        memcpy(pNode->m_Key, key, key_len);
        pNode->m_Key[key_len] = '\0';
        pNode->m_Data = new char[bytes];
        memcpy(pNode->m_Data, val, bytes);
        pNode->m_Len  = bytes;

        m_MemProcSize += ( sizeof(stMemProcNode) + (key_len+1) + pNode->m_Len + sizeof(string) + sizeof(stMemProcNode *) );

        AddHead( pNode );
        m_MemProcNodeMap.insert( map< string, stMemProcNode * >::value_type( key, pNode )  );

        m_MemMutex.unlock();

        retCode = 0;
    }
    else if(  1 == m_local_cache_type && m_local_memcached_port > 0 )
    {
        string memcachedServer = "127.0.0.1:" + IntToString(m_local_memcached_port);

        unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( key );

        MemcacheClient *pMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(memcachedServer,curHashValue);
        if(pMemCli)
        {
            time_t locExpire=m_local_memcached_expire;
            if(locExpire < 0
                || ( expire > 0 && expire < locExpire ) )
            {
                locExpire = expire;
            }
            retCode = pMemCli->CacheSet( key,  key_len,  val,  bytes,  locExpire);
        }
    }

    //模块间同步
    if(1 == m_local_cache_active_sync && ifSync )
    {
        m_SyncQueueMutex.lock();
        m_SyncQueue.push(key);
        m_SyncCond->signal();
        m_SyncQueueMutex.unlock();
    }

    DCS_LOG(TRACE, "LocalCacheManager::CacheSet() end.");

    return retCode;
}


//get
int LocalCacheManager::CacheGet(const char *key, const size_t len, void **value, size_t *retlen)
{
    DCS_LOG(TRACE, "LocalCacheManager::CacheGet() begin.");
    int retCode=-1;
    if(0 == m_local_cache_open)
    {
        return retCode;
    }

    //本进程or服务
    if( 0 == m_local_cache_type )
    {
        stMemProcNode *pNode=NULL;
        m_MemMutex.lock();
        map< string, stMemProcNode *>::iterator it = m_MemProcNodeMap.find(key);
        if( it != m_MemProcNodeMap.end() )
        {
            pNode = it->second;

            if(pNode)
            {
                //数据拷贝
                *retlen = pNode->m_Len;
                MALLOC(*value, *retlen, char);
                memcpy(*value, pNode->m_Data, *retlen);

                //LRU挪动
                Remove(pNode);
                AddHead(pNode);

                retCode = 0;
            }



        }
        m_MemMutex.unlock();

    }
    else if(  1 == m_local_cache_type && m_local_memcached_port > 0 )
    {
        string memcachedServer = "127.0.0.1:" + IntToString(m_local_memcached_port);

        unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( key );

        MemcacheClient *pMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(memcachedServer,curHashValue);
        if(pMemCli)
        {
            retCode = pMemCli->CacheGet(key, len, value, retlen);
        }
    }


    DCS_LOG(TRACE, "LocalCacheManager::CacheGet() end.");

    return retCode;
}


//del
int LocalCacheManager::CacheDel(const char *key, const size_t len, bool ifSync)
{
    DCS_LOG(TRACE, "LocalCacheManager::CacheDel() begin.");
    int retCode=0;
    if(0 == m_local_cache_open)
    {
        return retCode;
    }

    //本进程or服务
    if( 0 == m_local_cache_type )
    {

        m_MemMutex.lock();
        DCS_LOG(DEBUG, "m_MemProcNodeMap.size():" << m_MemProcNodeMap.size() << ",m_MemProcSize:" << m_MemProcSize );

        map< string, stMemProcNode *>::iterator it = m_MemProcNodeMap.find(key);
        if( it != m_MemProcNodeMap.end() )
        {
            stMemProcNode *pNode = it->second;

            if(pNode)
            {
                Remove(pNode);

                if(pNode->m_Data)
                {
                    m_MemProcSize -= pNode->m_Len;

                    delete [] pNode->m_Data;
                }

                if( pNode->m_Key )
                {
                    m_MemProcSize -= ( strlen(pNode->m_Key) + 1 + sizeof(string) + sizeof(stMemProcNode *) );

                    m_MemProcNodeMap.erase(pNode->m_Key);
                    delete [] pNode->m_Key;
                }

                m_MemProcSize -= sizeof(stMemProcNode);
                delete pNode;
            }
        }
        m_MemMutex.unlock();

        retCode = 0;

    }
    else if(  1 == m_local_cache_type && m_local_memcached_port > 0 )
    {
        string memcachedServer = "127.0.0.1:" + IntToString(m_local_memcached_port);

        unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( key );

        MemcacheClient *pMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(memcachedServer,curHashValue);
        if(pMemCli)
        {
            retCode = pMemCli->CacheDel(key, len);
        }
    }

    //模块间同步
    if( 1 == m_local_cache_active_sync && ifSync )
    {
        m_SyncQueueMutex.lock();
        m_SyncQueue.push(key);
        m_SyncCond->signal();
        m_SyncQueueMutex.unlock();
    }

    DCS_LOG(TRACE, "LocalCacheManager::CacheDel() end.");

    return retCode;
}

//重连zookeeper
int LocalCacheManager::ConnectZookeeper()
{
    DCS_LOG(TRACE, "LocalCacheManager::ConnectZookeeper() begin.");

    int retCode=0;
    if(m_ZookeeperClient)
    {
        return retCode;
    }

    //连接zookeeper
    m_ZookeeperClient = new ZookeeperClient(m_ZkServers, RemotesWatcher);

    //${zk_home}/module/模块目录初始化
    string cacheNode = m_ZkHome + "/module/" + m_local_cache_module_name;
    size_t idx=0;
    string dir;

    while(idx+1 < cacheNode.length())
    {
        dir = cacheNode.substr(0, cacheNode.find("/",idx+1));
        idx = dir.length();
        m_ZookeeperClient->SetNode(dir.c_str(), NULL, 0, 0);
    }

    DCS_LOG(TRACE, "LocalCacheManager::ConnectZookeeper() end.");
    return retCode;
}

//注册本机同步通道
int LocalCacheManager::RegSyncICE()
{
    int retCode=0;

    DCS_LOG(TRACE, "LocalCacheManager::RegSyncICE() begin.");

    if(m_ZookeeperClient)
    {
        string zkNode = m_ZkHome + "/module/" + m_local_cache_module_name + "/" + m_local_cache_ice;
        retCode = m_ZookeeperClient->SetNode(zkNode.c_str(), NULL, 0, 1);
        DCS_LOG(DEBUG, "LocalCacheManager::RegSyncICE()SetNode:" << zkNode << ",retCode:" << retCode );
    }

    DCS_LOG(TRACE, "LocalCacheManager::RegSyncICE() end.");

    return retCode;
}

//获取本模块内其他节点同步通道
int LocalCacheManager::GetICERemotes()
{
    int retCode=0;

    DCS_LOG(TRACE, "LocalCacheManager::GetICERemotes() begin.");

    if(m_ZookeeperClient)
    {
        m_ICEMutex.lock();

        string zkNode = m_ZkHome + "/module/" + m_local_cache_module_name;
        retCode = m_ZookeeperClient->GetNodeChildren(zkNode.c_str(), RemotesWatcher, m_ICERemotes);

        //debug
        set< string >::iterator itChild;
        string remotes;
        for( itChild=m_ICERemotes.begin(); itChild!=m_ICERemotes.end(); itChild++  )
        {
            remotes += *itChild + "|";
        }

        DCS_LOG(DEBUG, "GetNodeChildren()-zkNode:" << zkNode << ",retCode:" << retCode << ",m_ICERemotes:" << remotes);

        m_ICEMutex.unlock();
    }

    DCS_LOG(TRACE, "LocalCacheManager::GetICERemotes() end.");

    return retCode;
}

//zookeeper 回调
void LocalCacheManager::RemotesWatcher(int type,  const char *path)
{
    DCS_LOG(TRACE, "LocalCacheManager::RemotesWatcher() begin.");

    DCS_LOG(DEBUG, "LocalCacheManager::RemotesWatcher(type:" << type);

    //注册本机
    //LocalCacheManager::GetInstance()->RegSyncICE();

    //获取远程机器
    //LocalCacheManager::GetInstance()->GetICERemotes();

    DCS_LOG(TRACE, "LocalCacheManager::RemotesWatcher() end.");

    return;
}

string LocalCacheManager::GetICEPort()
{
    return m_ICEPort;
}


//同步通道创建并监听
void * LocalCacheManager::ThreadSyncICE( void *p_void )
{
    DCS_LOG(TRACE, "LocalCacheManager::ThreadSyncICE() begin.");

	std::string cfgPort = LocalCacheManager::GetInstance()->GetICEPort();
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

    DCS_LOG(INFO, "int  LocalCacheManager::ThreadSyncICE()-listenPort:" << listenPort);
    size_t len = 0;

    while(1)
    {
        bzero(&sBuf, sizeof(sBuf));
        if( (len = ::recvfrom( ListenFd, sBuf, sizeof(sBuf), 0, (struct sockaddr *) &cliaddr, &clilen) ) > 0 )
        {
            if(0 == ::strncmp(sBuf, "REMOLOCDEL|", 11))
            {
                std::string key = sBuf+11;
                int nRet = LocalCacheManager::GetInstance()->CacheDel(key.c_str(), key.length(), false);;
                //int nRet=0;
                if(0 == nRet)
                {
                    sendto( ListenFd, "+OK", 3, 0, ( struct sockaddr * )&cliaddr, ( unsigned int )clilen );
                }

                DCS_LOG(DEBUG, "LocalCacheManager::ThreadSyncICE(passive)-sBuf:" << sBuf
                << ",client:" << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port)
                << ",nRet:" << nRet );
            }
        }
        else
        {
            DCS_LOG(DEBUG, "LocalCacheManager::ThreadSyncICE()-recvfrom len:" << len
                << ",errno:" << errno );
        }
    }


    //int retCode=0;

    //int argc=0;
    //char sTmp[]="LocalCacheManager::ThreadSyncICE";
    //char **argv=(char **) &sTmp;

    //自动阻塞
    //DCSClientICEImpl::GetInstance()->ICEInit( argc, argv );

    DCS_LOG(TRACE, "LocalCacheManager::ThreadSyncICE() end.");

    return NULL;
}

// 函数功能: cache同步任务处理线程
void * LocalCacheManager::ThreadCacheSync( void *p_void )
{
    DCS_LOG(TRACE, "LocalCacheManager::ThreadCacheSync() begin.");
    int retCode=0;

    //等待通道就绪
    TimeWait( 500 );

    while(1)
    {
        string oneKey;

        //锁队列
        LocalCacheManager::GetInstance()->m_SyncQueueMutex.lock();
        while(1)
        {
            //提取
            if(!LocalCacheManager::GetInstance()->m_SyncQueue.empty())
            {
                oneKey = LocalCacheManager::GetInstance()->m_SyncQueue.front();
                LocalCacheManager::GetInstance()->m_SyncQueue.pop();
                break;
            }

            LocalCacheManager::GetInstance()->m_SyncCond->wait();
        }
        LocalCacheManager::GetInstance()->m_SyncQueueMutex.unlock();


        //向同模块机群同步
        std::vector< std::string > noticeList;
        LocalCacheManager::GetInstance()->m_ICEMutex.lock();

        set< string >::iterator it =  LocalCacheManager::GetInstance()->m_ICERemotes.begin();
        for(; it != LocalCacheManager::GetInstance()->m_ICERemotes.end(); it++)
        {
            noticeList.push_back( *it );
        }

        LocalCacheManager::GetInstance()->m_ICEMutex.unlock();

        std::vector< std::string >::iterator itNotice = noticeList.begin();
        for( ; itNotice != noticeList.end(); itNotice++ )
        {
            string zkICE = *itNotice;
            //非本机
            if( LocalCacheManager::GetInstance()->m_local_cache_ice != zkICE  )
            {
                string ip, port;
                CutStringHead(zkICE, ":", ip, port);
                if(ip.length() > 0 && port.length() > 0)
                {
                    int ret = LocalCacheManager::GetInstance()->RemoteLocCacheDel(ip, port, oneKey);

                    DCS_LOG(DEBUG, "LocalCacheManager::ThreadCacheSync(active),ip:" << ip
                                        << ",port:" << port  << ",key:" << oneKey << ",ret:" << ret );
                }
            }
        }
    }


    DCS_LOG(TRACE, "LocalCacheManager::CacheSync() end.");

    return NULL;
}

//维护memcached服务的健康
void *LocalCacheManager::ThreadSyncZookeeper( void *pVoid )
{
    DCS_LOG(TRACE, "LocalCacheManager::ThreadSyncZookeeper() begin");


    while(1)
    {

        if( 1 == LocalCacheManager::GetInstance()->m_local_cache_passive_sync )
        {
            //注册本机
            LocalCacheManager::GetInstance()->RegSyncICE();
        }

        if( 1 == LocalCacheManager::GetInstance()->m_local_cache_active_sync )
        {
            //获取远程机器
            LocalCacheManager::GetInstance()->GetICERemotes();
        }

        DCS_LOG(INFO, "LocalCacheManager::ThreadSyncZookeeper() m_local_cache_passive_sync:"
        << LocalCacheManager::GetInstance()->m_local_cache_passive_sync
        << ",m_local_cache_active_sync:"
        << LocalCacheManager::GetInstance()->m_local_cache_active_sync );

        //1小时
        int iCnt=60;
        while(iCnt--)
        {
            TimeWait( 60*1000 );
        }
    }

    return NULL;

}

//维护memcached服务的健康
void *LocalCacheManager::ThreadMemProbe( void *pVoid )
{
    DCS_LOG(TRACE, "LocalCacheManager::ThreadMemProbe() begin");

    while(1)
    {
        //2分钟
        TimeWait( 120*1000 );

        vector< string > memVecs;
        string memcachedServer = "127.0.0.1:" + IntToString(LocalCacheManager::GetInstance()->m_local_memcached_port);
        memcachedServer += ":1000";
        //取列表
        memVecs.push_back(memcachedServer);

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

    DCS_LOG(TRACE, "LocalCacheManager::ThreadMemProbe() end");

    return NULL;
}


//远程业务节点的本地缓存删除
int LocalCacheManager::RemoteLocCacheDel(const string &ip, const string &port, const string &key)
{
    DCS_LOG(TRACE, "LocalCacheManager::RemoteLocCacheDel() begin." );

    //DCS_LOG(DEBUG, "LocalCacheManager::RemoteLocCacheDel() ip:" << ip
    // << ",port:" << port
    // << ",key:" << key );

    int sockfd;

    struct sockaddr_in servaddr;

    ::bzero( &servaddr, sizeof(servaddr) );

    servaddr.sin_family = AF_INET;

    servaddr.sin_port = htons( stringToInt(port) );

    if( ::inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0 )
    {
        return -1;
    }
    //DCS_LOG(DEBUG, "MasterServer::RemoteRepairNode()1111111111111");

    sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);

    struct timeval timeout={3,0};//3s
    int ret=setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    ret=setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));

    if(::connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) == -1)
    {
        ::close(sockfd);
        return -2;
    }
    //DCS_LOG(DEBUG, "MasterServer::RemoteRepairNode()222222222222");

    //构造数据
    std::string sendData;
    sendData = "REMOLOCDEL|" + key;
    ::write(sockfd, sendData.c_str(), sendData.length());

    char sBuf[100];
    ::read(sockfd, sBuf, 100);
    if(0 != ::strncmp(sBuf, "+OK", 3))
    {
        ::close(sockfd);
        return -3;
    }
    //DCS_LOG(DEBUG, "MasterServer::RemoteRepairNode()333333333333");

    ::close(sockfd);


    DCS_LOG(TRACE, "LocalCacheManager::RemoteLocCacheDel() end." );

    return 0;
}



}


