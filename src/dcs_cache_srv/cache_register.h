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
    // ��������: RegistAll
    // ��������: ��zookeeperע�����У�ͨ����ȡ�����ļ���ȡzookeeper��mem��Ϣ
    // �������: const string& confFile �����ļ�ȫ·���������Ϊ����ʹ��ȫ������g_conf
    // �������: ��
    // ����ֵ  : �ɹ�����0;�����ļ��쳣����-1;zookeeper�쳣����-2
    //-------------------------------------------------------------------------------
    int RegistAll(const string& confFile="");

    //
    //-------------------------------------------------------------------------------
    // ��������: RepairMemRegist
    // ��������: �޸�����memע����Ϣ
    // �������: const string& cacheNode memע���ļ��� ��ʽip:port
    // �������: ��
    // ����ֵ  : �ɹ�����0;����ά�����з���-1;zookeeper�쳣����-2
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

    //zookeeper�ص�
    static void MyWatcher(int type, const char *path);

    //����masterָ��Իָ��쳣�ڵ�����
    static void *ThreadUdpRecv( void *pVoid );

    //ά��memcached����Ľ���
    static void *ThreadMemProbe( void *pVoid );

};

}

#endif

