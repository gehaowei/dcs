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

//�û���ѡ����ʵ�ֵ�watcher
//type��ֵ��Χ����:
//1.node�����仯
//2.�����쳣����

//nodeΪ�¼������Ľڵ�������Ϊ�쳣��������nodeΪNULL
typedef void (*SubscriberWatcher)(int type, const char *node);


/* Class of zookeeper client ��֧�ֶ��̷߳��� */
class ZookeeperClient
{
public:
    //���켴���� serverList=1.2.3.4:12000,1.2.3.5:12000
    ZookeeperClient(const string &serverList, SubscriberWatcher watcher=NULL);

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
    int SetNode(const char *nodeName, const char *nodeData, const int dataLen, int flags);

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
    int GetNode(const char *nodeName, SubscriberWatcher watcher, char *dataBuf, int &dataLen);


    //-------------------------------------------------------------------------------
    // ��������: GetNodeChildren
    // ��������: ��ȡ�ڵ���ӽڵ��б�
    // �������:
    //const char *nodeName �ڵ�����(ȫ·������),
    //SubscriberWatcher watcher(�û���ʵ�е�watcher��������ΪNULL),
    // �������: set< string >& childs �ӽڵ��б�
    // ����ֵ  : �ɹ�����0;�������󷵻�-1;�����ڷ���-2;zookeeper�쳣����-3
    //-------------------------------------------------------------------------------
    int GetNodeChildren(const char *nodeName, SubscriberWatcher watcher, set< string >& childs );

    //-------------------------------------------------------------------------------
    // ��������: DelNode
    // ��������: ɾ���ڵ�
    // �������:
    //const char *nodeName �ڵ�����(ȫ·������),
    // �������: ��
    // ����ֵ  : �ɹ�����0;�������󷵻�-1;zookeeper�쳣����-2
    //-------------------------------------------------------------------------------
    int DelNode(const char *nodeName);

    //-------------------------------------------------------------------------------
    // ��������: AcquireMutualLock
    // ��������: ��û�����
    // �������:
    //const char *nodeName �ڵ�����(ȫ·������),
    // �������: ��
    // ����ֵ  : �ɹ�����0;�������󷵻�-1;�ڵ��Ѵ��ڷ���-2;zookeeper�쳣����-3
    //-------------------------------------------------------------------------------
    int AcquireMutualLock(const char *nodeName);

    //-------------------------------------------------------------------------------
    // ��������: ReleaseMutualLock
    // ��������: �ͷ��Ѿ���õ���
    // �������: ��
    // �������: ��
    // ����ֵ  : �ɹ�����0;zookeeper�쳣����-1
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

    //�۲��ߺ����б�(һ��������ʱֻ֧��һ��watch)
    map < string, SubscriberWatcher > m_SubWathers;


    //�̰߳�ȫ
    Mutex m_SafeLock;

    //ά�������б�
    vector < string > m_ServerList;

};

}

#endif
