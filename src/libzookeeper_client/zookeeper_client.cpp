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

    //�����б�
    StringSplit(serverList, ",", m_ServerList);

    //zokeeper�ͻص�
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
// ��������: SetNode
// ��������: ���ýڵ㣬���������Զ�����
// �������:
//const char *nodeName �ڵ�����(ȫ·������),
//const char *nodeData(�ڵ�����),
//const int dataLen(�ڵ����ݳ���),
//int flags(�ڵ�����:0����;1��ʱ.)
// �������: ��
// ����ֵ  : �ɹ�����0;�������󷵻�-1;��Ŀ¼�����ڷ���-2;zookeeper�쳣����-3
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
// ��������: GetNode
// ��������: ��ȡ�ڵ�
// �������:
//const char *nodeName �ڵ�����(ȫ·������),
//SubscriberWatcher watcher(�û���ʵ�е�watcher��������ΪNULL),
// �������: char *dataBuf (���������߳��е��ڴ棬zookeeper���������صĽڵ����ݽ�����)
// int &dataLen (dataBuf�ĳ��ȣ���������ʱ����д�ɽڵ����ݵĳ��ȣ���������-1)
// ����ֵ  : �ɹ�����0;�������󷵻�-1;�ڵ㲻���ڷ���-2;zookeeper�쳣����-3
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
// ��������: GetNodeChildren
// ��������: ��ȡ�ڵ���ӽڵ��б�
// �������:
//const char *nodeName �ڵ�����(ȫ·������),
//SubscriberWatcher watcher(�û���ʵ�е�watcher��������ΪNULL),
// �������: set< string >& childs �ӽڵ��б�
// ����ֵ  : �ɹ�����0;�������󷵻�-1;�����ڷ���-2;zookeeper�쳣����-3
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
// ��������: DelNode
// ��������: ɾ���ڵ�
// �������:
//const char *nodeName �ڵ�����(ȫ·������),
// �������: ��
// ����ֵ  : �ɹ�����0;�������󷵻�-1;zookeeper�쳣����-2
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
// ��������: AcquireMutualLock
// ��������: ��û�����
// �������:
//const char *nodeName �ڵ�����(ȫ·������),
// �������: ��
// ����ֵ  : �ɹ�����0;�������󷵻�-1;�ڵ��Ѵ��ڷ���-2;zookeeper�쳣����-3
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

    //��������
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
		    //�ڵ��Ѵ���
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
// ��������: ReleaseMutualLock
// ��������: �ͷ��Ѿ���õ���
// �������: ��
// �������: ��
// ����ֵ  : �ɹ�����0;zookeeper�쳣����-1
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

    if(type == ZOO_SESSION_EVENT) //�쳣��Ⲣ����
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

            //����
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

            //���Ӿ���֪ͨ
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

            //����
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
    else  //�������̴����ص�
    {
        ctx->bevent = true;

        //�ص�
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
    //m_SafeLock.lock(); ������

    if( ( ( false == ifWait ) || ( false == m_ctx.waitForConnected(10) ) )
        && m_ServerList.size() > 0)
    {
        //ifWait=true����Ҫ������ɲŷ���
        while(1)
        {
            string selectServer;

            //���ѡȡ
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

