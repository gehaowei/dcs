#ifndef _ZOOKEEPER_CLIENT_H_
#define _ZOOKEEPER_CLIENT_H_

#include <unistd.h>
#include <list>
#include <zookeeper.h>
#include <time.h>
#include <string>
#include <vector>
#include <set>
#include <map>
#include "mutex.h"

using namespace std;

namespace DCS
{


class ZookeeperClient;
/* Struct of watcherCtx used in function:
 * void (*watcher_fn)(
 *      zhandle_t *zh,
 *      int type,
 *      int state,
 *      const char *path,
 *      void *watcherCtx
 * );
 */
typedef struct watchCtx {

public:
    bool connected;
    string hostPorts;
    zhandle_t *zh;
    bool bevent;
    ZookeeperClient *zk_cli;

    watchCtx() {
        connected = false;
        zh = 0;
	    bevent = false;
    }

    ~watchCtx() {
        if (zh) {
            zookeeper_close(zh);
            zh = 0;
        }
    }

    bool waitForConnected(int timeOut) {
        time_t expires = time(0) + timeOut;
        while(!connected && time(0) < expires) {
            usleep(500);
        }
        return connected;
    }

} watchctx_t;

//用户可选的自实现的watcher
//type传值范围如下:
//1.node发生变化
//2.连接异常重连

//node为事件触发的节点名，当为异常重连情形node为NULL
typedef void (*SubscriberWatcher)(int type, const char *node);


/* Class of zookeeper client 不支持多线程访问 */
class ZookeeperClient
{
public:
    //构造即连接 serverList=1.2.3.4:12000,1.2.3.5:12000
    ZookeeperClient(const string &serverList, SubscriberWatcher watcher=NULL);

    //-------------------------------------------------------------------------------
    // 函数名称: SetNode
    // 函数功能: 设置节点，不存在则自动创建
    // 输入参数:
    //const char *nodeName 节点名称(全路径名称),
    //const char *nodeData(节点数据),
    //const int dataLen(节点数据长度),
    //int flags(节点类型:0永久;1临时.)
    // 输出参数: 无
    // 返回值  : 成功返回0;参数错误返回-1;父目录不存在返回-2;zookeeper异常返回-3
    //-------------------------------------------------------------------------------
    int SetNode(const char *nodeName, const char *nodeData, const int dataLen, int flags);

    //-------------------------------------------------------------------------------
    // 函数名称: GetNode
    // 函数功能: 获取节点
    // 输入参数:
    //const char *nodeName 节点名称(全路径名称),
    //SubscriberWatcher watcher(用户自实行的watcher函数可以为NULL),
    // 输出参数: char *dataBuf (函数调用者持有的内存，zookeeper服务器返回的节点数据将回填)
    // int &dataLen (dataBuf的长度，函数返回时将回写成节点数据的长度，空则填入-1)
    // 返回值  : 成功返回0;参数错误返回-1;节点不存在返回-2;zookeeper异常返回-3
    //-------------------------------------------------------------------------------
    int GetNode(const char *nodeName, SubscriberWatcher watcher, char *dataBuf, int &dataLen);


    //-------------------------------------------------------------------------------
    // 函数名称: GetNodeChildren
    // 函数功能: 获取节点的子节点列表
    // 输入参数:
    //const char *nodeName 节点名称(全路径名称),
    //SubscriberWatcher watcher(用户自实行的watcher函数可以为NULL),
    // 输出参数: set< string >& childs 子节点列表
    // 返回值  : 成功返回0;参数错误返回-1;不存在返回-2;zookeeper异常返回-3
    //-------------------------------------------------------------------------------
    int GetNodeChildren(const char *nodeName, SubscriberWatcher watcher, set< string >& childs );

    //-------------------------------------------------------------------------------
    // 函数名称: DelNode
    // 函数功能: 删除节点
    // 输入参数:
    //const char *nodeName 节点名称(全路径名称),
    // 输出参数: 无
    // 返回值  : 成功返回0;参数错误返回-1;zookeeper异常返回-2
    //-------------------------------------------------------------------------------
    int DelNode(const char *nodeName);

    //-------------------------------------------------------------------------------
    // 函数名称: AcquireMutualLock
    // 函数功能: 获得互斥锁
    // 输入参数:
    //const char *nodeName 节点名称(全路径名称),
    // 输出参数: 无
    // 返回值  : 成功返回0;参数错误返回-1;节点已存在返回-2;zookeeper异常返回-3
    //-------------------------------------------------------------------------------
    int AcquireMutualLock(const char *nodeName);

    //-------------------------------------------------------------------------------
    // 函数名称: ReleaseMutualLock
    // 函数功能: 释放已经获得的锁
    // 输入参数: 无
    // 输出参数: 无
    // 返回值  : 成功返回0;zookeeper异常返回-1
    //-------------------------------------------------------------------------------
    int ReleaseMutualLock();

public:
   ~ZookeeperClient();
   SubscriberWatcher GetWatcher(const char *nodeName);
   void ReConnect(const bool ifWait=true);

private:
	watchctx_t m_ctx;
    SubscriberWatcher m_watcher;

    /* Watch function
     * callback function
     * used by zookeeper
     */
	static void Watcher(zhandle_t *, int type, int state, const char *path,void*v);

    /* Lock names */
    string m_strMutualLockName;

    //观察者函数列表(一个对象暂时只支持一次watch)
    map < string, SubscriberWatcher > m_SubWathers;


    //线程安全
    Mutex m_SafeLock;

    //维护连接列表
    vector < string > m_ServerList;

};

}

#endif
