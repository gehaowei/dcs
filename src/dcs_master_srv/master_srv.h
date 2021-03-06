#ifndef __CACHE_SRV_H__
#define __CACHE_SRV_H__

#include "file_conf.h"
#include <sys/types.h>
#include <signal.h>
#include <string>
using namespace std;

#include "template_singleton.h"
#include "zookeeper_client.h"


namespace DCS {

class MasterServer:
        INHERIT_TSINGLETON( MasterServer )
public:

    //-------------------------------------------------------------------------------
    // 函数名称: DoWork
    // 函数功能: 模块初始化并启动工作
    // 输入参数: 透传main参数
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int DoWork(int& argc,	char *argv[]);

    //获取配置文件对象指针
    FileConfIni *GetCfg()
    {
        return m_Conf;
    }

public:
    ~MasterServer();

private:
    //初始化化zookeeper目录
    int InitZookeeperDir();

    //争抢master锁
    int AcquireMasterLock();

    //zookeeper回调监控cache server 列表变化
    static void CacheServerWatcher(int type, const char *path);

    //设置变化标志
    void SetChanged()
    {
        m_ChangedMutex.lock();
        m_ChangedFlag=1;
        m_ChangedMutex.unlock();
    }

    //获取变化
    bool IfChanged(){
        bool ret=false;

        m_ChangedMutex.lock();
        ret = m_ChangedFlag?true:false;
        m_ChangedMutex.unlock();
        return ret;
    }

    //监控cacheserver节点，当发生变化则做相关处理
    static void * ThreadMonitor( void * p_void );

    //主逻辑
    //1.获取${zk_home}/cacheserver/all
    int GetZkCacheAll();

    //2.获取${zk_home}/cacheserver/目录下子节点
    //3.处理上述数据:分离机群/故障探测
    //4.回写${zk_home}/cacheserver/all和本地
    int MonitorCacheNodes();

    //自增获取下一个MG编号
    void IncMgNum(const string& lastMg, string &newMg);

    //检查是否符合加入机群的条件,并加入欠祷�0，否则返回-1
    int CheckMGAndAppend(string &orgMg, const string &cacheIp, const string &cachePort, const string &cacheSize);


private:
    //配置文件
    FileConfIni *m_Conf;

    //cache server列表变化标志
    int m_ChangedFlag;
    Mutex m_ChangedMutex;

    //zookeeper客户端
    ZookeeperClient *m_ZookeeperClient;

    //用于存放zk获取的所有机群原始数据
    char *m_ZKALLDataBuf;


    //all cache机群信息mg0000=123.123.123.123:12345:2000,123.123.123.123:12345:2000|mg0001=123.123.123.123:12345:1500,123.123.123.123:12345:1500|
    Mutex m_CacheMGMutex;

    map < string, string > m_CacheMGAll;
    set < string > m_CacheMGName;

    //故障cache列表< 123.123.123.123:12345:2000, time >
    //配置项:failure_period
    map < string, time_t > m_FailedCacheList;

    int m_WriteBackFlag;


    //远程修复节点
    int RemoteRepairNode(const string &ip, const string &port, const string &cacheNode);


};

}


#endif
