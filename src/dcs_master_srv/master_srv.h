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
    // ��������: DoWork
    // ��������: ģ���ʼ������������
    // �������: ͸��main����
    // �������: ��
    // ����ֵ  : �ɹ�����0;�쳣���ظ���
    //-------------------------------------------------------------------------------
    int DoWork(int& argc,	char *argv[]);

    //��ȡ�����ļ�����ָ��
    FileConfIni *GetCfg()
    {
        return m_Conf;
    }

public:
    ~MasterServer();

private:
    //��ʼ����zookeeperĿ¼
    int InitZookeeperDir();

    //����master��
    int AcquireMasterLock();

    //zookeeper�ص����cache server �б�仯
    static void CacheServerWatcher(int type, const char *path);

    //���ñ仯��־
    void SetChanged()
    {
        m_ChangedMutex.lock();
        m_ChangedFlag=1;
        m_ChangedMutex.unlock();
    }

    //��ȡ�仯
    bool IfChanged(){
        bool ret=false;

        m_ChangedMutex.lock();
        ret = m_ChangedFlag?true:false;
        m_ChangedMutex.unlock();
        return ret;
    }

    //���cacheserver�ڵ㣬�������仯������ش���
    static void * ThreadMonitor( void * p_void );

    //���߼�
    //1.��ȡ${zk_home}/cacheserver/all
    int GetZkCacheAll();

    //2.��ȡ${zk_home}/cacheserver/Ŀ¼���ӽڵ�
    //3.������������:�����Ⱥ/����̽��
    //4.��д${zk_home}/cacheserver/all�ͱ���
    int MonitorCacheNodes();

    //������ȡ��һ��MG���
    void IncMgNum(const string& lastMg, string &newMg);

    //����Ƿ���ϼ����Ⱥ������,�������Ƿ���0�����򷵻�-1
    int CheckMGAndAppend(string &orgMg, const string &cacheIp, const string &cachePort, const string &cacheSize);


private:
    //�����ļ�
    FileConfIni *m_Conf;

    //cache server�б�仯��־
    int m_ChangedFlag;
    Mutex m_ChangedMutex;

    //zookeeper�ͻ���
    ZookeeperClient *m_ZookeeperClient;

    //���ڴ��zk��ȡ�����л�Ⱥԭʼ����
    char *m_ZKALLDataBuf;


    //all cache��Ⱥ��Ϣmg0000=123.123.123.123:12345:2000,123.123.123.123:12345:2000|mg0001=123.123.123.123:12345:1500,123.123.123.123:12345:1500|
    Mutex m_CacheMGMutex;

    map < string, string > m_CacheMGAll;
    set < string > m_CacheMGName;

    //����cache�б�< 123.123.123.123:12345:2000, time >
    //������:failure_period
    map < string, time_t > m_FailedCacheList;

    int m_WriteBackFlag;


    //Զ���޸��ڵ�
    int RemoteRepairNode(const string &ip, const string &port, const string &cacheNode);


};

}


#endif
