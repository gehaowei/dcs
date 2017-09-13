#ifndef __DCS_CACHE_MANAGER_H__
#define __DCS_CACHE_MANAGER_H__

#include "template_singleton.h"
#include "zookeeper_client.h"
#include "file_conf.h"
#include <queue>
#include "cond.h"

typedef int (*compfn)( const void*, const void* );

namespace DCS {

//一致性hash环上的机群(machine group)点
typedef struct
{
    unsigned int m_Point;  // point on circle
    char * m_Servers;    //机群cache server列表以逗号分隔,数据格式:1.2.3.4:13000,1.2.3.5:13000
} stContinuumMG;

//机群(machine group)信息
typedef struct
{
    string m_MGName;  //机群名称
    unsigned long m_CacheSize;  //机群能力
    string m_Servers;  //机群cache server列表
} stMGInfo;

//cache操作任务单元
typedef struct stCacheOpTask
{
    int    m_OpType; //1:set,2:del
    char * m_CacheServers; //服务列表
    size_t m_ServersLen;
    char * m_Key;
    size_t m_KeyLen;
    char * m_Val;
    size_t m_ValLen;
    time_t m_Expire;
    stCacheOpTask(int OpType, const char * CacheServers, size_t ServersLen,
        const char * Key, size_t KeyLen, const char * Val, size_t ValLen, time_t Expire)
    {
        m_OpType       = OpType      ;

        m_CacheServers = NULL;
        if(CacheServers && ServersLen)
        {
            m_CacheServers = new char[ServersLen+1];
            memcpy(m_CacheServers, CacheServers, ServersLen);
            m_CacheServers[ServersLen] = '\0';
        }

        m_ServersLen   = ServersLen  ;

        m_Key = NULL;
        if(Key && KeyLen)
        {
            m_Key = new char[KeyLen+1];
            memcpy(m_Key, Key, KeyLen);
            m_Key[KeyLen] = '\0';
        }

        m_KeyLen       = KeyLen      ;

        m_Val = NULL;
        if(Val && ValLen)
        {
            m_Val = new char[ValLen+1];
            memcpy(m_Val, Val, ValLen);
            m_Val[ValLen] = '\0';
        }

        m_ValLen       = ValLen      ;
        m_Expire       = Expire      ;
    }


    stCacheOpTask()
    {
        m_OpType       = 0;
        m_CacheServers = NULL;
        m_ServersLen   = 0;
        m_Key          = NULL;
        m_KeyLen       = 0;
        m_Val          = NULL;
        m_ValLen       = 0;
        m_Expire       = 0;

    }
} stCacheOpTask;


//缓存节点管理类:一致性hash存储cache节点
class DCSCacheManager:
        INHERIT_TSINGLETON( DCSCacheManager )
public:

    //-------------------------------------------------------------------------------
    // 函数名称: Init
    // 函数功能: 初始化并启动
    // 输入参数: conf配置文件指针
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int Init(FileConfIni *conf);

    //-------------------------------------------------------------------------------
    // 函数名称: GetCacheMG
    // 函数功能: 获取cache机群信息
    // 输入参数: key键
    // 输出参数: cacheMg 机群cache server列表以逗号分隔，格式:1.2.3.4:13000,1.2.3.5:13001
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int GetCacheMG(const unsigned int key, string &cacheMg);

    //-------------------------------------------------------------------------------
    // 函数名称: PutCacheOpTask
    // 函数功能: 添加cache操作任务到队列异步执行
    // 输入参数: cacheOp结构，外部创建内存填充。本类处理完毕后会释放。
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int PutCacheOpTask(const stCacheOpTask &cacheOp );

    //-------------------------------------------------------------------------------
    // 函数名称: DoCacheOpTask
    // 函数功能: 直接执行cache操作任务
    // 输入参数: oneTask结构，外部创建内存填充。本类处理完毕后会释放。
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    static int DoCacheOpTask(const stCacheOpTask &oneTask );


    //字符串转换成一致性hash键值
    unsigned int Str2Hash( const char* inString );

public:
    ~DCSCacheManager();

private:
    //构建一致性hash环
    int CreateContinuum();

    //重连zookeeper
    int ConnectZookeeper();

    //读取zookeeper的cache all信息
    int GetZkCacheAll(vector< stMGInfo * > &m_MGVector, unsigned long &m_TotalSize);

    //zookeeper回调监控${zk_home}/cacheserver/all节点的变化
    static void CacheAllWatcher(int type, const char *path);

    //cache操作任务处理线程
    static void *ThreadCacheOp( void *pVoid );

private:
    //打印一致性hash环
    void PrintContinuum();

    //md5加密 16位
    void Md5Digest16( const char* inString, unsigned char md5pword[16] );

private:
    //zookeeper服务列表以逗号分隔，格式:192.168.188.51:2181,192.168.188.52:2181
    string m_ZkServers;

    ZookeeperClient *m_ZookeeperClient;

    //用于存放zk获取的所有机群原始数据
    char *m_ZKALLDataBuf;

    //${zk_home}/cacheserver/all
    string m_ZkCacheAll;

    //从zookeeper获取到的机群数据
    //vector< stMGInfo * > m_MGVector;
    //unsigned long m_TotalSize;
    Mutex m_ZookeeperMutex;

    //重建一致性hash环的开关
    bool m_ReBuildFlag;

    //需要重新创建一致性hash数据
    bool m_NeedReBuild;
    time_t m_Expire;


    //一致性hash环数据
    stContinuumMG *m_ContinuumMG;
    //一致性hash环数量
    int m_NumPoints;

    //一致性hash数据锁
    Mutex m_ContHashMutex;


    //cache操作队列
    queue< stCacheOpTask > m_CacheOpTaskQueue;
    Mutex m_CacheOpTaskMutex;
    Cond *m_CacheOpTaskCond;
    //保护大小
    int m_OpTaskProtectSize;

};

}



#endif
