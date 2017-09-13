#ifndef __MEMCACHE_CLIENT_H__
#define __MEMCACHE_CLIENT_H__

#include <map>
#include <string>
using namespace std;

#include <libmemcached/memcached.h>
#include "template_singleton.h"
#include "mutex.h"
#include "logger.h"

namespace DCS
{

//memcache客户端自动恢复时间(秒)
#define MEM_CLIENT_RECOVERY_TIME 5

//同一server有4个连接实例
#define MEM_CLIENT_INSTANCE_NUM 4


class MemcacheClient;

/////////////////////////////////////////////////////////////////////
//memcache客户端管理者
//缓存所有客户端
class MemcacheClientManager:
    INHERIT_TSINGLETON( MemcacheClientManager )
public:
    ~MemcacheClientManager();

    //-------------------------------------------------------------------------------
    // 函数名称: MemcacheClientManager::GetMemcacheClient
    // 函数功能: 获取memcache客户端
    // 输入参数: const string &server--服务器地址端口格式:host:port
    //           unsigned int hashNum = 0 同一server有4个实例，hashNum决定返回哪个实例
    // 输出参数: 无
    // 返回值  : 成功返回MemcacheClient地址，否则返回NULL
    //-------------------------------------------------------------------------------
    MemcacheClient * GetMemcacheClient(const string &server, unsigned int hashNum = 0 );



private:
    Mutex m_MemCliMapLock;
    vector < map < string, MemcacheClient * > * > m_MemCliMaps;

    //累积失效memcache客户端超时删除
    //暂不提供
};

///////////////////////////////////////////////////////////////////
//memcache客户端
class MemcacheClient
{
public:
    //server=host:port
    MemcacheClient(const string &server);
    ~MemcacheClient();

    //set
    int CacheSet(const char *key, const size_t key_len, const void *val, const size_t bytes, const time_t expire=0);

    //get
    int CacheGet(const char *key, const size_t len, void **value, size_t *retlen);

    //del
    int CacheDel(const char *key, const size_t len);


private:
    //线程安全?，经测试不能大并发访问 奇怪??
    Mutex m_SafeLock;

    //失效测试
    int testNum;

    //ip:port
    string m_MemServer;

    //上下文
    memcached_st *memc;


    //初始化Mem对象
    int MemInit();
    //销毁Mem对象
    int MemDestroy();

    //bad or recovery
    int BadOrRecovery(memcached_return_t &rc);

};

}

#endif
