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

//�����ڴ�ṹ��ڵ�
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

//����cache����2�ַ�ʽ
//�ڴ淽ʽ:������δʹ���û��㷨(LRU, Least Recently Used)
//memcached����ʽ���ӿڲ������Ƴ�ʱ
class LocalCacheManager:
        INHERIT_TSINGLETON( LocalCacheManager )
public:

    //-------------------------------------------------------------------------------
    // ��������: Init
    // ��������: ��ʼ��������
    // �������: conf�����ļ�ָ��
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
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
    //ͬ��������ر���
    Mutex m_SyncQueueMutex;
    queue< string > m_SyncQueue;
    Cond * m_SyncCond;

    //��ģ���������ڵ���Ϣ
    set< string > m_ICERemotes;
    Mutex m_ICEMutex;

    ZookeeperClient *m_ZookeeperClient;

    //��ʼ��zookeeper
    int ConnectZookeeper();

    //��ʼ��zookeeperĿ¼
    int InitZookeeperDir();

    //ע�᱾��ͬ��ͨ��
    int RegSyncICE();

    //��ȡ��ģ���������ڵ�ͬ��ͨ��
    int GetICERemotes();

    //zookeeper �ص�
    static void RemotesWatcher(int type,  const char *path);


    //ͬ��ͨ������������
    static void * ThreadSyncICE( void *p_void );


    // ��������: cacheͬ���������߳�
    static void * ThreadCacheSync( void *p_void );

    //ˢ��ͬ����Ϣ
    static void *ThreadSyncZookeeper( void *pVoid );

    //ά��memcached����Ľ���
    static void *ThreadMemProbe( void *pVoid );

    //Զ��ҵ��ڵ�ı��ػ���ɾ��
    int RemoteLocCacheDel(const string &ip, const string &port, const string &key);

private:
    //zookeeper�����б��Զ��ŷָ�����ʽ:192.168.188.51:2181,192.168.188.52:2181
    string m_ZkServers;
    string m_ZkHome;
    //local cache open flag, 0:close,1:open
    int m_local_cache_open;
    //module inner sync flag, 0:close,1:open
    //passive ����   active ����
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

    //�����ڴ���ر������ӿ�
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

    //��������ɾ��node
    void Remove( stMemProcNode * node )
    {
    	node->m_Next->m_Pre = node->m_Pre;
    	node->m_Pre->m_Next = node->m_Next;
    }
};

}



#endif
