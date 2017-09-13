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
    // º¯ÊıÃû³Æ: DoWork
    // º¯Êı¹¦ÄÜ: Ä£¿é³õÊ¼»¯²¢Æô¶¯¹¤×÷
    // ÊäÈë²ÎÊı: Í¸´«main²ÎÊı
    // Êä³ö²ÎÊı: ÎŞ
    // ·µ»ØÖµ  : ³É¹¦·µ»Ø0;Òì³£·µ»Ø¸ºÊı
    //-------------------------------------------------------------------------------
    int DoWork(int& argc,	char *argv[]);

    //»ñÈ¡ÅäÖÃÎÄ¼ş¶ÔÏóÖ¸Õë
    FileConfIni *GetCfg()
    {
        return m_Conf;
    }

public:
    ~MasterServer();

private:
    //³õÊ¼»¯»¯zookeeperÄ¿Â¼
    int InitZookeeperDir();

    //ÕùÇÀmasterËø
    int AcquireMasterLock();

    //zookeeper»Øµ÷¼à¿Øcache server ÁĞ±í±ä»¯
    static void CacheServerWatcher(int type, const char *path);

    //ÉèÖÃ±ä»¯±êÖ¾
    void SetChanged()
    {
        m_ChangedMutex.lock();
        m_ChangedFlag=1;
        m_ChangedMutex.unlock();
    }

    //»ñÈ¡±ä»¯
    bool IfChanged(){
        bool ret=false;

        m_ChangedMutex.lock();
        ret = m_ChangedFlag?true:false;
        m_ChangedMutex.unlock();
        return ret;
    }

    //¼à¿Øcacheserver½Úµã£¬µ±·¢Éú±ä»¯Ôò×öÏà¹Ø´¦Àí
    static void * ThreadMonitor( void * p_void );

    //Ö÷Âß¼­
    //1.»ñÈ¡${zk_home}/cacheserver/all
    int GetZkCacheAll();

    //2.»ñÈ¡${zk_home}/cacheserver/Ä¿Â¼ÏÂ×Ó½Úµã
    //3.´¦ÀíÉÏÊöÊı¾İ:·ÖÀë»úÈº/¹ÊÕÏÌ½²â
    //4.»ØĞ´${zk_home}/cacheserver/allºÍ±¾µØ
    int MonitorCacheNodes();

    //×ÔÔö»ñÈ¡ÏÂÒ»¸öMG±àºÅ
    void IncMgNum(const string& lastMg, string &newMg);

    //¼ì²éÊÇ·ñ·ûºÏ¼ÓÈë»úÈºµÄÌõ¼ş,²¢¼ÓÈë¬ÊÇ·µ»Ø0£¬·ñÔò·µ»Ø-1
    int CheckMGAndAppend(string &orgMg, const string &cacheIp, const string &cachePort, const string &cacheSize);


private:
    //ÅäÖÃÎÄ¼ş
    FileConfIni *m_Conf;

    //cache serverÁĞ±í±ä»¯±êÖ¾
    int m_ChangedFlag;
    Mutex m_ChangedMutex;

    //zookeeper¿Í»§¶Ë
    ZookeeperClient *m_ZookeeperClient;

    //ÓÃÓÚ´æ·Åzk»ñÈ¡µÄËùÓĞ»úÈºÔ­Ê¼Êı¾İ
    char *m_ZKALLDataBuf;


    //all cache»úÈºĞÅÏ¢mg0000=123.123.123.123:12345:2000,123.123.123.123:12345:2000|mg0001=123.123.123.123:12345:1500,123.123.123.123:12345:1500|
    Mutex m_CacheMGMutex;

    map < string, string > m_CacheMGAll;
    set < string > m_CacheMGName;

    //¹ÊÕÏcacheÁĞ±í< 123.123.123.123:12345:2000, time >
    //ÅäÖÃÏî:failure_period
    map < string, time_t > m_FailedCacheList;

    int m_WriteBackFlag;


    //Ô¶³ÌĞŞ¸´½Úµã
    int RemoteRepairNode(const string &ip, const string &port, const string &cacheNode);


};

}


#endif
