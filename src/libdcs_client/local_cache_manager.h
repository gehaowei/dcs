#ifndef __LOCAL_CACHE_MANAGER_H__
#define __LOCAL_CACHE_MANAGER_H__

#include "template_singleton.h"
#include "memcache_client.h"
#include "file_conf.h"
#include "zookeeper_client.h"

#include "mutex.h"
#include "cond.h"

#include <queue>
#include <map>
#include <set>
using namespace std;

namespace DCS {

//本地内存结构体节点
typedef struct stMemProcNode
{
    char *m_Key;
    char *m_Data;
    int m_Len;

    stMemProcNode *m_Next;
    stMemProcNode *m_Pre;

    stMemProcNode()
    {
        m_Key=0;
        m_Data=NULL;
        m_Len=0;
        m_Next=this;
        m_Pre=this;
    }
} stMemProcNode;

//本地cache管理，2种方式
//内存方式:最近最久未使用置换算法(LRU, Least Recently Used)
//memcached服务方式，接口参数控制超时
class LocalCacheManager:
        INHERIT_TSINGLETON( LocalCacheManager )
public:

    //-------------------------------------------------------------------------------
    // 函数名称: Init
    // 函数功能: 初始化并启动
    // 输入参数: conf配置文件指针
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int Init(FileConfIni *conf);

    //set
    int CacheSet(const char *key, const size_t key_len, const void *val, const size_t bytes, const time_t expire=0, bool ifSync=true);

    //get
    int CacheGet(const char *key, const size_t len, void **value, size_t *retlen);

    //del
    int CacheDel(const char *key, const size_t len, bool ifSync=true);

public:
        ~LocalCacheManager();

        string GetICEPort();

private:
    //同步队列相关变量
    Mutex m_SyncQueueMutex;
    queue< string > m_SyncQueue;
    Cond * m_SyncCond;

    //本模块内其他节点信息
    set< string > m_ICERemotes;
    Mutex m_ICEMutex;

    ZookeeperClient *m_ZookeeperClient;

    //初始化zookeeper
    int ConnectZookeeper();

    //初始化zookeeper目录
    int InitZookeeperDir();

    //注册本机同步通道
    int RegSyncICE();

    //获取本模块内其他节点同步通道
    int GetICERemotes();

    //zookeeper 回调
    static void RemotesWatcher(int type,  const char *path);


    //同步通道创建并监听
    static void * ThreadSyncICE( void *p_void );


    // 函数功能: cache同步任务处理线程
    static void * ThreadCacheSync( void *p_void );

    //刷新同步信息
    static void *ThreadSyncZookeeper( void *pVoid );

    //维护memcached服务的健康
    static void *ThreadMemProbe( void *pVoid );

    //远程业务节点的本地缓存删除
    int RemoteLocCacheDel(const string &ip, const string &port, const string &key);

private:
    //zookeeper服务列表以逗号分隔，格式:192.168.188.51:2181,192.168.188.52:2181
    string m_ZkServers;
    string m_ZkHome;
    //local cache open flag, 0:close,1:open
    int m_local_cache_open;
    //module inner sync flag, 0:close,1:open
    //passive 被动   active 主动
    int m_local_cache_active_sync;
    int m_local_cache_passive_sync;

    //module inner syn icnternal communication engine
    string m_local_cache_module_name;
    string m_local_cache_ice; //localip:26303
    string m_ICEIp;
    string m_ICEPort;

    //local cache type : 0:mem of process, 1:mem of memcached server(127.0.0.1)
    int m_local_cache_type;
    //when mem of process, mem max size can use (M)
    int m_local_mem_max;
    //local memcached server port
    int m_local_memcached_port;

    //local memcached expire time
    int m_local_memcached_expire;

    //本地内存相关变量及接口
    Mutex m_MemMutex;
    long long m_MemProcSize;
    stMemProcNode m_MemProHead;
    map< string, stMemProcNode * > m_MemProcNodeMap;

    stMemProcNode * GetTail()
    {
        if( m_MemProHead.m_Pre == &m_MemProHead )
        {
            return NULL;
        }
        return m_MemProHead.m_Pre;
    }

    void AddHead( stMemProcNode * node )
    {
        node->m_Next = m_MemProHead.m_Next;
        node->m_Pre  = &m_MemProHead;
        node->m_Pre->m_Next = node;
        node->m_Next->m_Pre = node;
    }

    //从链表中删除node
    void Remove( stMemProcNode * node )
    {
    	node->m_Next->m_Pre = node->m_Pre;
    	node->m_Pre->m_Next = node->m_Next;
    }
};

}



#endif
