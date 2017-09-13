#ifndef __CACHE_REGISTER_H__
#define __CACHE_REGISTER_H__

#include <string>
using namespace std;

#include "file_conf.h"
#include "zookeeper_client.h"
#include "template_singleton.h"

namespace DCS {

class CacheRegister:
        INHERIT_TSINGLETON( CacheRegister )
public:

    //-------------------------------------------------------------------------------
    // 函数名称: RegistAll
    // 函数功能: 向zookeeper注册所有，通过读取配置文件获取zookeeper和mem信息
    // 输入参数: const string& confFile 配置文件全路径名，如果为空则使用全局配置g_conf
    // 输出参数: 无
    // 返回值  : 成功返回0;配置文件异常返回-1;zookeeper异常返回-2
    //-------------------------------------------------------------------------------
    int RegistAll(const string& confFile="");

    //
    //-------------------------------------------------------------------------------
    // 函数名称: RepairMemRegist
    // 函数功能: 修复单个mem注册信息
    // 输入参数: const string& cacheNode mem注册文件名 格式ip:port
    // 输出参数: 无
    // 返回值  : 成功返回0;不在维护队列返回-1;zookeeper异常返回-2
    //-------------------------------------------------------------------------------
    int RepairMemRegist(const string& cacheNode);

public:
    ~CacheRegister();

private:

    Mutex m_MemMutex;

    //ip:port:memsize
    set< string > m_MemInfos;

    ZookeeperClient *m_ZookeeperClient;

    int ZookeeperInit(FileConfIni *conf=NULL);

    //zookeeper回调
    static void MyWatcher(int type, const char *path);

    //接收master指令，以恢复异常节点数据
    static void *ThreadUdpRecv( void *pVoid );

    //维护memcached服务的健康
    static void *ThreadMemProbe( void *pVoid );

};

}

#endif

