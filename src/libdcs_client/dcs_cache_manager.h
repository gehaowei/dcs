#ifndef __DCS_CACHE_MANAGER_H__
#define __DCS_CACHE_MANAGER_H__

#include "template_singleton.h"
#include "zookeeper_client.h"
#include "file_conf.h"
#include <queue>
#include "cond.h"

typedef int (*compfn)( const void*, const void* );

namespace DCS {

//һ����hash���ϵĻ�Ⱥ(machine group)��
typedef struct
{
    unsigned int m_Point;  // point on circle
    char * m_Servers;    //��Ⱥcache server�б��Զ��ŷָ�,���ݸ�ʽ:1.2.3.4:13000,1.2.3.5:13000
} stContinuumMG;

//��Ⱥ(machine group)��Ϣ
typedef struct
{
    string m_MGName;  //��Ⱥ����
    unsigned long m_CacheSize;  //��Ⱥ����
    string m_Servers;  //��Ⱥcache server�б�
} stMGInfo;

//cache��������Ԫ
typedef struct stCacheOpTask
{
    int    m_OpType; //1:set,2:del
    char * m_CacheServers; //�����б�
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


//����ڵ������:һ����hash�洢cache�ڵ�
class DCSCacheManager:
        INHERIT_TSINGLETON( DCSCacheManager )
public:

    //-------------------------------------------------------------------------------
    // ��������: Init
    // ��������: ��ʼ��������
    // �������: conf�����ļ�ָ��
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int Init(FileConfIni *conf);

    //-------------------------------------------------------------------------------
    // ��������: GetCacheMG
    // ��������: ��ȡcache��Ⱥ��Ϣ
    // �������: key��
    // �������: cacheMg ��Ⱥcache server�б��Զ��ŷָ�����ʽ:1.2.3.4:13000,1.2.3.5:13001
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int GetCacheMG(const unsigned int key, string &cacheMg);

    //-------------------------------------------------------------------------------
    // ��������: PutCacheOpTask
    // ��������: ���cache�������񵽶����첽ִ��
    // �������: cacheOp�ṹ���ⲿ�����ڴ���䡣���ദ����Ϻ���ͷš�
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int PutCacheOpTask(const stCacheOpTask &cacheOp );

    //-------------------------------------------------------------------------------
    // ��������: DoCacheOpTask
    // ��������: ֱ��ִ��cache��������
    // �������: oneTask�ṹ���ⲿ�����ڴ���䡣���ദ����Ϻ���ͷš�
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    static int DoCacheOpTask(const stCacheOpTask &oneTask );


    //�ַ���ת����һ����hash��ֵ
    unsigned int Str2Hash( const char* inString );

public:
    ~DCSCacheManager();

private:
    //����һ����hash��
    int CreateContinuum();

    //����zookeeper
    int ConnectZookeeper();

    //��ȡzookeeper��cache all��Ϣ
    int GetZkCacheAll(vector< stMGInfo * > &m_MGVector, unsigned long &m_TotalSize);

    //zookeeper�ص����${zk_home}/cacheserver/all�ڵ�ı仯
    static void CacheAllWatcher(int type, const char *path);

    //cache�����������߳�
    static void *ThreadCacheOp( void *pVoid );

private:
    //��ӡһ����hash��
    void PrintContinuum();

    //md5���� 16λ
    void Md5Digest16( const char* inString, unsigned char md5pword[16] );

private:
    //zookeeper�����б��Զ��ŷָ�����ʽ:192.168.188.51:2181,192.168.188.52:2181
    string m_ZkServers;

    ZookeeperClient *m_ZookeeperClient;

    //���ڴ��zk��ȡ�����л�Ⱥԭʼ����
    char *m_ZKALLDataBuf;

    //${zk_home}/cacheserver/all
    string m_ZkCacheAll;

    //��zookeeper��ȡ���Ļ�Ⱥ����
    //vector< stMGInfo * > m_MGVector;
    //unsigned long m_TotalSize;
    Mutex m_ZookeeperMutex;

    //�ؽ�һ����hash���Ŀ���
    bool m_ReBuildFlag;

    //��Ҫ���´���һ����hash����
    bool m_NeedReBuild;
    time_t m_Expire;


    //һ����hash������
    stContinuumMG *m_ContinuumMG;
    //һ����hash������
    int m_NumPoints;

    //һ����hash������
    Mutex m_ContHashMutex;


    //cache��������
    queue< stCacheOpTask > m_CacheOpTaskQueue;
    Mutex m_CacheOpTaskMutex;
    Cond *m_CacheOpTaskCond;
    //������С
    int m_OpTaskProtectSize;

};

}



#endif
