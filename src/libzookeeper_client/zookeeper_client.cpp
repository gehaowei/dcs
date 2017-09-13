#include "zookeeper_client.h"
#include "logger.h"
#include <errno.h>
#include <string>
#include <iostream>
#include "common.h"

using namespace std;

namespace DCS
{


ZookeeperClient::ZookeeperClient(const string &serverList, SubscriberWatcher watcher)
{
    if(serverList.length()<=0)
    {
        DCS_LOG(WARN, "ZookeeperClient::ZookeeperClient() serverList is NULL.");
        return;
    }

    DCS_LOG(DEBUG, "ZookeeperClient::ZookeeperClient()serverList:" << serverList);

    //服务列表
    StringSplit(serverList, ",", m_ServerList);

    //zokeeper和回调
    m_ctx.zk_cli = this;
    zoo_set_debug_level(ZOO_LOG_LEVEL_WARN);
    m_watcher = watcher;

    /* Used for reconnect */
    ReConnect(false);

    return;
}

ZookeeperClient::~ZookeeperClient()
{
    if(m_ctx.zh)
    {
        ReleaseMutualLock();
        zookeeper_close(m_ctx.zh);
    }

}

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
int ZookeeperClient::SetNode(const char *nodeName, const char *nodeData, const int dataLen, int flags)
{

    if(nodeName == NULL)
    {
        DCS_LOG(WARN, "ZookeeperClient::SetNode() nodeName == NULL.");
        return -1;
    }
    /* Used for reconnect */
    ReConnect();

    int rtn = 0;
    int nodeType=0;

    if(1 == flags)
    {
       nodeType |=  ZOO_EPHEMERAL;
    }

    rtn = zoo_create(m_ctx.zh, nodeName, nodeData, dataLen, &ZOO_OPEN_ACL_UNSAFE, nodeType, NULL, 0);

    if(rtn != ZOK)
    {
        if(rtn != ZNODEEXISTS)
        {
            return -2;
        }
        /* if node exist */
        else
        {
            /* set data on old node */
            rtn = zoo_set(m_ctx.zh, nodeName, nodeData, dataLen, -1);

            if(rtn != ZOK)
            {
                return -3;
            }
        }
    }

    return 0;
}

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
int ZookeeperClient::GetNode(const char *nodeName, SubscriberWatcher watcher, char *dataBuf, int &dataLen)
{
	int rtn = 0;

	if(nodeName == NULL)
	{
        DCS_LOG(WARN, "ZookeeperClient::GetNode() nodeName == NULL.");
		return -1;
	}
    /* Used for reconnect */
    ReConnect();

	int ifWatch= (watcher)?1:0;

	if(ifWatch)
	{
	    m_SubWathers.insert( map < string, SubscriberWatcher >::value_type(nodeName, watcher) );
	}

	char pathKey[256] = {0};

	if((rtn =zoo_get(m_ctx.zh, nodeName, ifWatch, dataBuf, &dataLen, NULL)) != ZOK)
	{
		return -2;
	}

	return 0;
}

//-------------------------------------------------------------------------------
// 函数名称: GetNodeChildren
// 函数功能: 获取节点的子节点列表
// 输入参数:
//const char *nodeName 节点名称(全路径名称),
//SubscriberWatcher watcher(用户自实行的watcher函数可以为NULL),
// 输出参数: set< string >& childs 子节点列表
// 返回值  : 成功返回0;参数错误返回-1;不存在返回-2;zookeeper异常返回-3
//-------------------------------------------------------------------------------
int ZookeeperClient::GetNodeChildren(const char *nodeName, SubscriberWatcher watcher, set<string> &childs )
{
    childs.clear();

    int rtn = 0;
	int ifWatch= (watcher)?1:0;
    struct String_vector vec_children;

	if(nodeName == NULL)
	{
        DCS_LOG(WARN, "ZookeeperClient::GetNodeChildren() nodeName == NULL.");
		return -1;
	}
    /* Used for reconnect */
    ReConnect();

	if(ifWatch)
	{
	    m_SubWathers.insert( map < string, SubscriberWatcher >::value_type(nodeName, watcher) );
	}

    if((rtn = zoo_get_children(m_ctx.zh, nodeName, ifWatch, &vec_children)) != ZOK)
    {
        return -2;
    }

    for(int i=0; i < vec_children.count; i++)
    {
        childs.insert(vec_children.data[i]);
    }

    deallocate_String_vector(&vec_children);

    return 0;
}

//-------------------------------------------------------------------------------
// 函数名称: DelNode
// 函数功能: 删除节点
// 输入参数:
//const char *nodeName 节点名称(全路径名称),
// 输出参数: 无
// 返回值  : 成功返回0;参数错误返回-1;zookeeper异常返回-2
//-------------------------------------------------------------------------------
int ZookeeperClient::DelNode(const char *nodeName)
{
    int rtn = 0;

    if( nodeName == NULL )
    {
        DCS_LOG(WARN, "ZookeeperClient::DelNode() nodeName == NULL.");
        return -1;
    }
    /* Used for reconnect */
    ReConnect();

    rtn = zoo_delete(m_ctx.zh, nodeName, -1);

    if(rtn != ZOK)
    {
        return -2;
    }

    return 0;
}

//-------------------------------------------------------------------------------
// 函数名称: AcquireMutualLock
// 函数功能: 获得互斥锁
// 输入参数:
//const char *nodeName 节点名称(全路径名称),
// 输出参数: 无
// 返回值  : 成功返回0;参数错误返回-1;节点已存在返回-2;zookeeper异常返回-3
//-------------------------------------------------------------------------------
int ZookeeperClient::AcquireMutualLock(const char *nodeName)
{
	if(nodeName == NULL)
	{
        DCS_LOG(WARN, "ZookeeperClient::AcquireMutualLock() nodeName == NULL.");
		return -1;
	}
    /* Used for reconnect */
    ReConnect();

    //以免死锁
	if(m_strMutualLockName.length())
	{
	    ReleaseMutualLock();
	}

	int flags = 0;

	flags |= ZOO_EPHEMERAL;

    int rtn = zoo_create(m_ctx.zh, nodeName, NULL, -1, &ZOO_OPEN_ACL_UNSAFE, flags,NULL, 0);

	if(rtn != ZOK)
    {
		if(rtn == ZNODEEXISTS)
		{
		    //节点已存在
            return -2;
		}
		else
		{
		    return -3;
		}
	}

	m_strMutualLockName = nodeName;

	return 0;
}

//-------------------------------------------------------------------------------
// 函数名称: ReleaseMutualLock
// 函数功能: 释放已经获得的锁
// 输入参数: 无
// 输出参数: 无
// 返回值  : 成功返回0;zookeeper异常返回-1
//-------------------------------------------------------------------------------
int ZookeeperClient::ReleaseMutualLock()
{

	 int rtn = 0;

    if(m_strMutualLockName == "")
    {
        return 0;
    }
    /* Used for reconnect */
    ReConnect();

    rtn = zoo_delete(m_ctx.zh, m_strMutualLockName.c_str(), -1);

    m_strMutualLockName = "";

    if(rtn != ZOK)
    {
        return -1;
    }

	return 0;
}

SubscriberWatcher ZookeeperClient::GetWatcher(const char *nodeName)
{
    SubscriberWatcher pFunc=NULL;

    if(NULL == nodeName)
    {
        DCS_LOG(DEBUG,"ZookeeperClient::GetWatcher()nodeName:NULL");
        return m_watcher;
    }

    DCS_LOG(DEBUG,"ZookeeperClient::GetWatcher()nodeName:" << nodeName);

    map < string, SubscriberWatcher >::iterator it = m_SubWathers.find(nodeName);
    if(it!=m_SubWathers.end())
    {
        pFunc = it->second;
        m_SubWathers.erase(it);
    }

    return pFunc;
}


void ZookeeperClient::Watcher(zhandle_t *, int type, int state, const char *path,void*v)
{
    watchctx_t *ctx = (watchctx_t*)v;

    if(type == ZOO_SESSION_EVENT) //异常检测并重连
    {
        if(state == ZOO_AUTH_FAILED_STATE)
        {
            DCS_LOG(DEBUG, "ZOO_AUTH_FAILED_STATE:" << ctx->hostPorts );
        }
        /* Set flag be false when connect lost */
        else if(state == ZOO_CONNECTING_STATE)
        {
            DCS_LOG(DEBUG, "ZOO_CONNECTING_STATE Lost:" << ctx->hostPorts);

            /* connect flag */
            ctx->connected = false;

            //重连
            //if(ctx->zk_cli)
            //{
            //    ctx->zk_cli->ReConnect();
            //}
        }
        else if(state == ZOO_ASSOCIATING_STATE)
        {
            DCS_LOG(DEBUG, "ZOO_ASSOCIATING_STATE:" << ctx->hostPorts );
        }
        /* Set flag be true wen connected */
        else if(state == ZOO_CONNECTED_STATE)
        {
            DCS_LOG(DEBUG, "ZOO_CONNECTED_STATE connected:" << ctx->hostPorts);

            /* connect flag */
            ctx->connected = true;

            //连接就绪通知
            if(ctx->zk_cli)
            {
                SubscriberWatcher pFunc = ctx->zk_cli->GetWatcher(NULL);
                if(pFunc)
                {
                    pFunc(2, NULL);
                }
            }
        }
        /* Reconnect when session expired */
        else if(state == ZOO_EXPIRED_SESSION_STATE)
        {
            DCS_LOG(DEBUG, "ZOO_EXPIRED_SESSION_STATE host:" << ctx->hostPorts);

            /* connect flag */
            ctx->connected = false;

            //重连
            //if(ctx->zk_cli)
            //{
            //    ctx->zk_cli->ReConnect();
            //}
        }
        else
        {
            DCS_LOG(DEBUG, "ZookeeperClient::Watcher host:" << ctx->hostPorts
                << "ZOO type:" << type  << "state:" << state);
        }
    }
    else  //正常流程触发回调
    {
        ctx->bevent = true;

        //回调
        if(ctx->zk_cli)
        {
            SubscriberWatcher pFunc = ctx->zk_cli->GetWatcher(path);
            if(pFunc)
            {
                pFunc(1, path);
            }
        }

    }

}

void ZookeeperClient::ReConnect( const bool ifWait )
{
    //m_SafeLock.lock(); 死锁了

    if( ( ( false == ifWait ) || ( false == m_ctx.waitForConnected(10) ) )
        && m_ServerList.size() > 0)
    {
        //ifWait=true则需要建立完成才返回
        while(1)
        {
            string selectServer;

            //随机选取
            if(m_ServerList.size() > 0)
            {
                srand( (unsigned)time( NULL ) );
                int idx = rand() % m_ServerList.size();
                selectServer = m_ServerList[idx];
            }

            /* connect flag */
            m_ctx.hostPorts = selectServer;
            m_ctx.connected = false;
            zookeeper_close(m_ctx.zh);
            m_ctx.zh=0;

            /* reconnect */
            zhandle_t *zk = zookeeper_init(m_ctx.hostPorts.c_str(), ZookeeperClient::Watcher, 10000, 0, &m_ctx, 0);

            if(zk == NULL)
            {
                DCS_LOG(INFO, "ZookeeperClient connect host:" << m_ctx.hostPorts << ",failed:" << strerror(errno)
                    << "ifWait:" << ifWait );
                return;
            }

            m_ctx.zh = zk;

            if( ( false == ifWait )
                || ( true == m_ctx.waitForConnected(10) ) )
            {
                DCS_LOG(INFO, "ZookeeperClient connect host:" << m_ctx.hostPorts << " succeed." << "ifWait:" << ifWait );
                break;
            }
            else
            {
                DCS_LOG(INFO, "ZookeeperClient connect host:" << m_ctx.hostPorts << " failed." << "ifWait:" << ifWait );
            }
        }
    }

    //m_SafeLock.unlock();
}


/*

void gehwWatcher(int type, int state, const char *path)
{
    cout << "gehwWatcher()type:" << type
        << "state:" << state
        << "path:" << path
        << endl;
}

int main(int argc, char **argv)
{
    int retCode=0;

    ZookeeperClient test("0:2181");
    string nodeName="/app/dcs/cacheserver/1.2.3.4:13000";
    char nodeData[]="1.2.3.4:13000 1000";
    retCode = test.SetNode(nodeName.c_str(), nodeData, strlen(nodeData), 1);
    cout << "test.SetNode(" << nodeName << ")ret:" << retCode << endl;

    char dataBuf[1024]={0};
    int dataLen = sizeof(dataBuf);
    retCode = test.GetNode(nodeName.c_str(), gehwWatcher, dataBuf, dataLen);
    cout << "test.GetNode(" << nodeName << ")ret:" << retCode << ",dataBuf:" << dataBuf << endl;

    string path="/app/dcs/cacheserver";
    set< string > childs;
    retCode = test.GetNodeChildren(path.c_str(), gehwWatcher, childs);
    cout << "test.GetNodeChildren(" << path << ")ret:" << retCode << ",childs:" << childs.size() << endl;
    set< string >::iterator it = childs.begin();
    for(; it!=childs.end(); it++)
    {
        cout << "childs[?]:" << *it << endl;
    }

    retCode = test.DelNode(nodeName.c_str());
    cout << "test.DelNode(" << nodeName << ")ret:" << retCode << endl;


    string lock="/app/dcs/master";
    retCode = test.AcquireMutualLock(lock.c_str());
    cout << "test.AcquireMutualLock(" << lock << ")ret:" << retCode << endl;

    retCode = test.AcquireMutualLock(lock.c_str());
    cout << "test.AcquireMutualLock(" << lock << ")ret:" << retCode << endl;

    retCode = test.ReleaseMutualLock();
    cout << "test.ReleaseMutualLock()ret:" << retCode << endl;


    string errtest="/xx/xx/xx";
    retCode = test.SetNode(errtest.c_str(), nodeData, strlen(nodeData), 1);
    cout << "test.SetNode(" << errtest << ")ret:" << retCode << endl;


    string stmp;
    cin >> stmp;

    return 0;
}
*/

}

