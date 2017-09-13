
#include "memcache_client.h"
#include "common.h"
#include <signal.h>
#include "logger.h"

namespace DCS
{

//构造
MemcacheClientManager::MemcacheClientManager()
{
    for(int i=0; i < MEM_CLIENT_INSTANCE_NUM; i++)
    {
        map < string, MemcacheClient * > *pMemCliMap = new map < string, MemcacheClient * >();
        m_MemCliMaps.push_back(pMemCliMap);
    }
}

//析构
MemcacheClientManager::~MemcacheClientManager()
{
}

//-------------------------------------------------------------------------------
// 函数名称: MemcacheClientManager::GetMemcacheClient
// 函数功能: 获取memcache客户端
// 输入参数: const string &server--服务器地址端口格式:host:port
//           unsigned int hashNum = 0 同一server有4个实例，hashNum决定返回哪个实例
// 输出参数: 无
// 返回值  : 成功返回MemcacheClient地址，否则返回NULL
//-------------------------------------------------------------------------------
MemcacheClient * MemcacheClientManager::GetMemcacheClient(const string &server, unsigned int hashNum)
{
    //DCS_LOG(DEBUG, "MemcacheClientManager::GetMemcacheClient()server:" << server << ",hashNum:" << hashNum );

    map < string, MemcacheClient * > *pMemCliMap=NULL;
    MemcacheClient *memCli=NULL;

    //hash计算
    unsigned int hash = (hashNum<0)?0:hashNum;
    pMemCliMap = m_MemCliMaps[hash%MEM_CLIENT_INSTANCE_NUM];

    //获取MemCli实例
    m_MemCliMapLock.lock();
    map < string, MemcacheClient * >::iterator it = pMemCliMap->find(server);
    if(it == pMemCliMap->end())
    {
        memCli = new MemcacheClient(server);
        if(memCli)
        {
            pMemCliMap->insert(
                map < string, MemcacheClient * >::value_type(server, memCli) );
        }
    }
    else
    {
        memCli = it->second;
    }

    ////如果对象不可用
    //if(0 != memCli->BadOrRecovery())
    //{
    //    memCli=NULL;
    //}
    m_MemCliMapLock.unlock();

    return memCli;
}

//server=host:port
MemcacheClient::MemcacheClient(const string &server):m_MemServer(server)
{
    testNum=0;

    m_SafeLock.lock();

    MemInit();

    m_SafeLock.unlock();
}

MemcacheClient::~MemcacheClient()
{
    m_SafeLock.lock();

    MemDestroy();

    m_SafeLock.unlock();
}

//set
int MemcacheClient::CacheSet(const char *key, const size_t key_len, const void *val, const size_t bytes, const time_t expire)
{
    int retCode=0;
    m_SafeLock.lock();

    memcached_return_t rc= memcached_set(memc, key, key_len, (const char *)val, bytes, expire, 0);


    retCode = BadOrRecovery(rc);


    m_SafeLock.unlock();

    return retCode;
}

//get
int MemcacheClient::CacheGet(const char *key, const size_t len, void **value, size_t *retlen)
{
    int retCode=0;

    m_SafeLock.lock();

    uint32_t flags= 0;
    memcached_return_t rc;

    char *memValue = memcached_get(memc, (char *) key, len, retlen, &flags, &rc);

    if( memValue != NULL )
    {
      *value = memValue;
    }

    retCode = BadOrRecovery(rc);


    m_SafeLock.unlock();


    return retCode;
}

//del
int MemcacheClient::CacheDel(const char *key, const size_t len)
{
    int retCode=0;
    m_SafeLock.lock();

    memcached_return_t rc= memcached_delete(memc, key, len,0);

    retCode = BadOrRecovery(rc);

    m_SafeLock.unlock();

    return retCode;

}

//初始化Mem对象
int MemcacheClient::MemInit()
{
    string config="--server="+m_MemServer;

    memc = memcached(config.c_str(), config.size());

    return 0;
}

//销毁Mem对象
int MemcacheClient::MemDestroy()
{
    memcached_free(memc);

    return 0;
}


//-------------------------------------------------------------------------------
// 函数名称: MemcacheClient::BadOrRecovery
// 函数功能: 对象是否有效，定期自动恢复
// 输入参数: 无
// 输出参数: 无
// 返回值  : 有效或者恢复成功返回0，无效返回-1
//-------------------------------------------------------------------------------
int MemcacheClient::BadOrRecovery(memcached_return_t &rc)
{

    if( memcached_fatal(rc) )
    {
        DCS_LOG(INFO, "MemcacheClient::BadOrRecovery() reset server:" << m_MemServer
            << "errmsg:" << memcached_last_error_message(memc) );
        MemDestroy();
        MemInit();
        return -1;
    }

    /*
    if(testNum++ > 100)
    {
        DCS_LOG(DEBUG, "---------------------------------reset server:" << m_MemServer
            << "errmsg:" << memcached_last_error_message(memc) );
        testNum=0;
        MemDestroy();
        MemInit();
    }
    */

    return 0;
}


/*
int Init()
{
	string home = getenv("HOME");
	string confPath = home + "/etc/libdcs_client.conf";
	string logPath = home + "/log/DCSLOG";

    DCS_LOG_INIT(logPath, "memcache_client" );

    DCS_SET_LOG_LEVEL( TRACE );

    return 0;

}

static void *gehwTest(void *p_void)
{
    int retCode=0;
    char key[]="gehwTest";
    size_t k_len=strlen(key);
    char value[]="1qaz2wsx3edc4rfv5tgb";
    size_t v_len=strlen(value);

    DCS_LOG(INFO, "gehwTest-start");

    return NULL;

    while(1)
    {
        //TimeWait(5000);

        MemcacheClient *pMemCli=MemcacheClientManager::GetInstance()->GetMemcacheClient("127.0.0.1:13000");

        if(NULL == pMemCli)
        {
            DCS_LOG(INFO, "gehwTest()-NULL == pMemCli");
            continue;
        }

        retCode = pMemCli->CacheSet(key,k_len,value,v_len,0);

        char *retValue=NULL;
        size_t retLen=0;
        retCode = pMemCli->CacheGet(key,k_len, &retValue, &retLen);
        DCS_LOG(INFO, "gehwTest-CacheGet()retCode:"<< retCode << ",retLen:" << retLen << ",retValue:" << retValue);
        if(retValue)
        {
            free(retValue);
        }

    }
    return NULL;
}

int main( int argc, char **argv )
{
    Init();

	struct sigaction sa;
	sa.sa_handler = SIG_IGN;
	sigaction( SIGPIPE, &sa, 0 );

    pthread_t id;

    for(int i=0; i<1; i++)
    {
        pthread_create(&id, NULL, gehwTest, NULL);
    }

	//启动本地通信器，自动阻塞
	while(1)
	{
	    TimeWait(1000);
	}

	return 0;
}
*/

}

