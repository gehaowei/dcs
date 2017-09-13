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

//memcache�ͻ����Զ��ָ�ʱ��(��)
#define MEM_CLIENT_RECOVERY_TIME 5

//ͬһserver��4������ʵ��
#define MEM_CLIENT_INSTANCE_NUM 4


class MemcacheClient;

/////////////////////////////////////////////////////////////////////
//memcache�ͻ��˹�����
//�������пͻ���
class MemcacheClientManager:
    INHERIT_TSINGLETON( MemcacheClientManager )
public:
    ~MemcacheClientManager();

    //-------------------------------------------------------------------------------
    // ��������: MemcacheClientManager::GetMemcacheClient
    // ��������: ��ȡmemcache�ͻ���
    // �������: const string &server--��������ַ�˿ڸ�ʽ:host:port
    //           unsigned int hashNum = 0 ͬһserver��4��ʵ����hashNum���������ĸ�ʵ��
    // �������: ��
    // ����ֵ  : �ɹ�����MemcacheClient��ַ�����򷵻�NULL
    //-------------------------------------------------------------------------------
    MemcacheClient * GetMemcacheClient(const string &server, unsigned int hashNum = 0 );



private:
    Mutex m_MemCliMapLock;
    vector < map < string, MemcacheClient * > * > m_MemCliMaps;

    //�ۻ�ʧЧmemcache�ͻ��˳�ʱɾ��
    //�ݲ��ṩ
};

///////////////////////////////////////////////////////////////////
//memcache�ͻ���
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
    //�̰߳�ȫ?�������Բ��ܴ󲢷����� ���??
    Mutex m_SafeLock;

    //ʧЧ����
    int testNum;

    //ip:port
    string m_MemServer;

    //������
    memcached_st *memc;


    //��ʼ��Mem����
    int MemInit();
    //����Mem����
    int MemDestroy();

    //bad or recovery
    int BadOrRecovery(memcached_return_t &rc);

};

}

#endif
