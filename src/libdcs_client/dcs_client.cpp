#include "dcs_client.h"

#include "common.h"
#include "logger.h"
#include "file_conf.h"
//#include "ice_implement.h"

#include "dcs_cache_manager.h"
#include "local_cache_manager.h"

using namespace DCS;

namespace DCS{

pthread_mutex_t DCSClient::m_Mutex=PTHREAD_MUTEX_INITIALIZER;
DCSClient* DCSClient::m_Instance=NULL;

DCSClient::DCSClient()
{
}
DCSClient::~DCSClient()
{
}

DCSClient * DCSClient::GetInstance()
{
    if (m_Instance == NULL)
    {
        pthread_mutex_lock( &m_Mutex );
        if (m_Instance == NULL)
            m_Instance = new DCSClient();
        pthread_mutex_unlock( &m_Mutex );
    }
    return m_Instance;
}


//-------------------------------------------------------------------------------
// 函数名称: Open
// 函数功能: 初始化并启动
// 输入参数: confFile配置文件
// 输出参数: 无
// 返回值  : 成功返回0;异常返回负数
//-------------------------------------------------------------------------------
int DCSClient::Open(const char *confFile)
{
    DCS_LOG(TRACE, "DCSClient::Open() begin.");

    string cfg = getenv("HOME");
    cfg += "/etc/dcs_client.conf";

    //配置文件
    if(NULL == confFile)
    {
        DCS_LOG(INFO, "DCSClient::Open()confFile is NULL. use default:" << cfg);
    }
    else
    {
        DCS_LOG(INFO, "DCSClient::Open()confFile:" << confFile );
        cfg = confFile;
    }


    int retCode=0;
    FileConfIni *conf = new FileConfIni( cfg );

	string home = getenv("HOME");
	string log_path=conf->getData("DCS_CLIENT", "log_path");
	string logPath = home + "/";
	if(log_path.length()>0)
	{
	    logPath+=log_path+"/DCSLOG";
	}
	else
	{
	    logPath+="log/DCSLOG";
	}

    DCS_LOG_INIT(logPath, "DCSClient" );

    DCSLogger::GetInstance()->setLogLevel( conf->getData("DCS_CLIENT", "log_level") );

    m_module_name = conf->getData("DCS_CLIENT", "module_name");

    //dcs cache管理类
    retCode += DCSCacheManager::GetInstance()->Init(conf);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->Init() failed.");
    }

    //local cache管理类
    retCode += LocalCacheManager::GetInstance()->Init(conf);
    if(retCode)
    {
        DCS_LOG(WARN, "LocalCacheManager::GetInstance()->Init() failed.");
    }

    if(conf)
    {
        delete conf;
    }

    DCS_LOG(INFO, "DCSClient::Open() start ...");

    DCS_LOG(TRACE, "DCSClient::Open() end.");

    return 0;
}


//-------------------------------------------------------------------------------
// 函数名称: CacheSet
// 函数功能: 写cache
// 输入参数: key键(库内部会自动base64编码), key_len 键长度
//      val值，val_len值长度,  expire缓存过期时间(单位秒，缺省为0表示永不过期)
// 输出参数: 无
// 返回值  : 成功返回0;异常返回负数
//-------------------------------------------------------------------------------
int DCSClient::CacheSet(const char *key, const size_t key_len, const void *val, const size_t val_len,
    const time_t expire, const int consistency)
{
    DCS_LOG(TRACE, "DCSClient::CacheSet() begin.");

    if(NULL==key || 0>=key_len || NULL==val || 0>=val_len)
    {
        DCS_LOG(WARN, "DCSClient::CacheSet() arg check failed.");
    }

    int retCode=0;
    char *keyTmp=new char[key_len+1];
    memcpy(keyTmp, key, key_len);
    *(keyTmp+key_len)='\0';
    string keyBase64=m_module_name + "_" +keyTmp;
    delete [] keyTmp;
    keyTmp=EncodeBase64(keyBase64.c_str(), keyBase64.length());
    if(keyTmp)
    {
        keyBase64=keyTmp;
        free(keyTmp);
        keyTmp=NULL;
    }

    unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( keyBase64.c_str() );

    //获取机群下cache列表:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
        return retCode;
    }

    DCS_LOG(DEBUG, "DCSClient::CacheSet() keyBase64:" <<keyBase64<< ",curHashValue:" << curHashValue << ",cacheMG:" << cacheMG );

    //随机单台
    vector < string > server_list;
    StringSplit(cacheMG, ",", server_list);
    string selectServer;

    //弱数据一致则双写随机读
    srand( (unsigned)time( NULL ) );
    int idx = rand() % server_list.size();

    //强一致性单写单读
    if(1 == consistency)
    {
        idx = curHashValue % server_list.size();
    }

    string failedServers;
    string::size_type tryTime=0;
    while(1)
    {
        tryTime++;

        //随机选取
        if(server_list.size() > 0)
        {
            idx = idx % server_list.size();
            selectServer = server_list[idx++];
        }

        //操作cache
        //DCS_LOG(DEBUG, "set key:" << keyBase64 << " from selectServer:" << selectServer);
        MemcacheClient *PMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(selectServer, curHashValue);
        if(PMemCli)
        {
            retCode = PMemCli->CacheSet(keyBase64.c_str(), keyBase64.length(), val, val_len, expire);
            if( 0 == retCode )
            {
                //success
                //DCS_LOG(DEBUG, "CacheSet succeed. key:" << keyBase64 << " from selectServer:" << selectServer);
                break;
            }
            else
            {
                //failed
                DCS_LOG(DEBUG, "CacheSet failed. key:" << keyBase64 << " from selectServer:" << selectServer);
            }
        }

        //检查失败次数
        if(tryTime >= server_list.size())
        {
            //全部失败
            break;
        }
    }

    //剔除成功服务器
    std::string::size_type index = cacheMG.find( selectServer+"," );
    if( index != std::string::npos )
    {
        cacheMG.replace( index , selectServer.length()+1, "" );
    }
    else
    {
        index = cacheMG.find( selectServer );
        if( index != std::string::npos )
        {
            cacheMG.replace( index , selectServer.length(), "" );
        }
    }

    //增加任务队列set其他node
    if(0 == retCode && cacheMG.length() > 0 )
    {

        //弱数据一致性则异步处理
        if( 0 == consistency )
        {
            stCacheOpTask oneTask(1, cacheMG.c_str(), cacheMG.length(), keyBase64.c_str(), keyBase64.length(), (const char *) val, val_len, expire);
            retCode = DCSCacheManager::GetInstance()->PutCacheOpTask(oneTask);
            if(retCode)
            {
                retCode=DCSCacheManager::DoCacheOpTask(oneTask);
            }
        }
        else
        {
            //强一致不双写
            //retCode=DCSCacheManager::DoCacheOpTask(oneTask);
        }

    }

    //set本地
    if(0 == consistency)
    {
        LocalCacheManager::GetInstance()->CacheSet(keyBase64.c_str(), keyBase64.length(), val, val_len, expire);
    }

    DCS_LOG(TRACE, "DCSClient::CacheSet() end.");

    return retCode;
}

//-------------------------------------------------------------------------------
// 函数名称: CacheGet
// 函数功能: 取cache
// 输入参数: key键(库内部会自动base64编码), key_len 键长度
//      value返回数据(应用传入指针地址，库创建内存并赋值指针，应用需自行free释放内存)
//      retlen数据的长度(size_t对象由应用创建，传入size_t对象地址，库对size_t对象进行赋值)
// 输出参数: 无
// 返回值  : 成功返回0;异常返回负数
//-------------------------------------------------------------------------------
int DCSClient::CacheGet(const char *key, const size_t key_len, void **value, size_t *retlen, const int consistency)
{
    DCS_LOG(TRACE, "DCSClient::CacheGet() begin.");

    if(NULL==key || 0>=key_len || NULL==value || NULL==retlen)
    {
        DCS_LOG(WARN, "DCSClient::CacheGet() arg check failed.");
    }

    int retCode=0;
    char *keyTmp=new char[key_len+1];
    memcpy(keyTmp, key, key_len);
    *(keyTmp+key_len)='\0';
    string keyBase64=m_module_name + "_" +keyTmp;
    delete [] keyTmp;
    keyTmp=EncodeBase64(keyBase64.c_str(), keyBase64.length());
    if(keyTmp)
    {
        keyBase64=keyTmp;
        free(keyTmp);
        keyTmp=NULL;
    }


    *retlen=0;

    //弱数据一致性
    if( 0 == consistency )
    {
        //get本地，成功则返回
        retCode = LocalCacheManager::GetInstance()->CacheGet(keyBase64.c_str(), keyBase64.length(), value, retlen);
        if(0 == retCode && 0 != (*retlen) )
        {
            return 0;
        }
    }

    unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( keyBase64.c_str() );

    //获取机群下cache列表:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
        return retCode;
    }

    DCS_LOG(DEBUG, "DCSClient::CacheGet() keyBase64:" <<keyBase64<< ",curHashValue:" << curHashValue << ",cacheMG:" << cacheMG );

    //随机单台get，????失败则尝试get其他台??consistency??
    vector < string > server_list;
    StringSplit(cacheMG, ",", server_list);
    string selectServer;

    //弱数据一致则双写随机读
    srand( (unsigned)time( NULL ) );
    int idx = rand() % server_list.size();

    //强一致性单写单读
    if(1 == consistency)
    {
        idx = curHashValue % server_list.size();
    }

    string failedServers;
    string::size_type tryTime=0;
    while(1)
    {
        tryTime++;

        //随机选取
        if(server_list.size() > 0)
        {
            idx = idx % server_list.size();
            selectServer = server_list[idx++];
        }

        //操作cache
        MemcacheClient *PMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(selectServer, curHashValue);
        if(PMemCli)
        {
            retCode = PMemCli->CacheGet( keyBase64.c_str(), keyBase64.length(), value, retlen);
            if(0 == retCode && 0 != (*retlen) )
            {
                //success
                DCS_LOG(DEBUG, "CacheGet succeed. key:" << keyBase64 << " from selectServer:" << selectServer);
                break;
            }
            else
            {
                //failed
                DCS_LOG(DEBUG, "CacheGet failed. key:" << keyBase64 << " from selectServer:" << selectServer);

                //20130529
                //不再读多台和回写
                break;

                if(0 == failedServers.length())
                {
                    failedServers = selectServer;
                }
                else
                {
                    failedServers += "," + selectServer;
                }
            }
        }
        else
        {
            DCS_LOG(WARN, "not ready selectServer:" << selectServer);
        }

        //检查是否需要再读下一台:强一致性则不可再读，弱数据一致性可以继续读
        if(tryTime >= server_list.size()
            || 1 == consistency )
        {
            break;
        }
    }

    //获取数据成功
    if( 0 == retCode
        && NULL != (*value)
        && (*retlen) > 0)
    {
        //set本地但不同步
        if(0 == consistency)
        {
            LocalCacheManager::GetInstance()->CacheSet(keyBase64.c_str(), keyBase64.length(), (*value), (*retlen), 0, false);
        }

        //增加任务队列set失败node(20130529此逻辑失效)
        if(failedServers.length())
        {
            stCacheOpTask oneTask(1, failedServers.c_str(), failedServers.length(), keyBase64.c_str(), keyBase64.length(), (const char *) (*value), (*retlen), 0);

            DCSCacheManager::GetInstance()->PutCacheOpTask(oneTask);
        }
    }
    else
    {
        DCS_LOG(DEBUG, "retCode:" << retCode << ",*retlen:" << *retlen);
    }

    DCS_LOG(TRACE, "DCSClient::CacheGet() end.");

    return retCode;
}

//-------------------------------------------------------------------------------
// 函数名称: CacheDel
// 函数功能: 删cache
// 输入参数: key键(库内部会自动base64编码), key_len 键长度
// 输出参数: 无
// 返回值  : 成功返回0;异常返回负数
//-------------------------------------------------------------------------------
int DCSClient::CacheDel(const char *key, const size_t key_len, const int consistency)
{
    DCS_LOG(TRACE, "DCSClient::CacheDel() begin.");

    if(NULL==key || 0>=key_len)
    {
        DCS_LOG(WARN, "DCSClient::CacheDel() arg check failed.");
    }


    int retCode=0;
    char *keyTmp=new char[key_len+1];
    memcpy(keyTmp, key, key_len);
    *(keyTmp+key_len)='\0';
    string keyBase64=m_module_name + "_" +keyTmp;
    delete [] keyTmp;

    keyTmp=EncodeBase64(keyBase64.c_str(), keyBase64.length());
    if(keyTmp)
    {
        keyBase64=keyTmp;
        free(keyTmp);
        keyTmp=NULL;
    }

    //DCS_LOG(DEBUG, "DCSClient::CacheDel()keyBase64:" << keyBase64 );
    unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( keyBase64.c_str() );

    //获取机群下cache列表:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
        return retCode;
    }

    DCS_LOG(DEBUG, "DCSClient::CacheDel() keyBase64:" <<keyBase64<< ",curHashValue:" << curHashValue << ",cacheMG:" << cacheMG );

    //随机单台
    vector < string > server_list;
    StringSplit(cacheMG, ",", server_list);
    string selectServer;

    //弱数据一致则双写随机读
    srand( (unsigned)time( NULL ) );
    int idx = rand() % server_list.size();

    //强一致性单写单读
    if(1 == consistency)
    {
        idx = curHashValue % server_list.size();
    }

    string failedServers;
    string::size_type tryTime=0;
    while(1)
    {
        tryTime++;

        //随机选取
        if(server_list.size() > 0)
        {
            idx = idx % server_list.size();
            selectServer = server_list[idx++];
        }

        //操作cache
        //DCS_LOG(DEBUG, "del key:" << keyBase64 << " from selectServer:" << selectServer);
        MemcacheClient *PMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(selectServer, curHashValue);
        if(PMemCli)
        {
            retCode = PMemCli->CacheDel(keyBase64.c_str(), keyBase64.length());
            if( 0 == retCode || 1 == retCode ) //返回1表示不存在
            {
                //success
                //DCS_LOG(DEBUG, "CacheDel success. key:" << keyBase64 << " from selectServer:" << selectServer);
                retCode = 0;
                break;
            }
            else
            {
                //failed
                DCS_LOG(DEBUG, "CacheDel failed. key:" << keyBase64 << " from selectServer:" << selectServer);
            }
        }

        //检查失败次数
        if(tryTime >= server_list.size())
        {
            //全部失败
            break;
        }
    }

    //剔除成功服务器
    std::string::size_type index = cacheMG.find( selectServer+"," );
    if( index != std::string::npos )
    {
        cacheMG.replace( index , selectServer.length()+1, "" );
    }
    else
    {
        index = cacheMG.find( selectServer );
        if( index != std::string::npos )
        {
            cacheMG.replace( index , selectServer.length(), "" );
        }
    }

    //DCS_LOG(DEBUG, "cacheMG-2:" << cacheMG );


    //增加任务队列del其他node
    if( cacheMG.length() > 0 )
    {

        //弱数据一致性则异步处理
        if( 0 == consistency )
        {
            stCacheOpTask oneTask(2, cacheMG.c_str(), cacheMG.length(), keyBase64.c_str(), keyBase64.length(), NULL, 0, 0);
            retCode = DCSCacheManager::GetInstance()->PutCacheOpTask(oneTask);
            if(retCode)
            {
                retCode=DCSCacheManager::DoCacheOpTask(oneTask);
            }
        }
        else
        {
            //强一致不双写
            //retCode=DCSCacheManager::DoCacheOpTask(oneTask);
        }
    }

    //弱数据一致性
    if( 0 == consistency )
    {
        //set本地
        LocalCacheManager::GetInstance()->CacheDel(keyBase64.c_str(), keyBase64.length());
    }

    DCS_LOG(TRACE, "DCSClient::CacheDel() end.");

    return retCode;
}

//-------------------------------------------------------------------------------
// 函数名称: Close
// 函数功能: 关闭客户端库
// 输入参数: 无
// 输出参数: 无
// 返回值  : 返回0
//-------------------------------------------------------------------------------
int DCSClient::Close()
{
    DCS_LOG(TRACE, "DCSClient::Close() begin.");

    int retCode=0;

    //dcs cache管理类

    //local cache管理类

    //memcached连接释放

    DCS_LOG(TRACE, "DCSClient::Close() begin.");

    return retCode;
}


string DCSClient::GetMGInfo(const char *key, const size_t key_len)
{
    DCS_LOG(TRACE, "DCSClient::GetMGInfo() begin.");

    if(NULL==key || 0>=key_len)
    {
        DCS_LOG(WARN, "DCSClient::GetMGInfo() arg check failed.");
    }

    int retCode=0;
    char *keyTmp=new char[key_len+1];
    memcpy(keyTmp, key, key_len);
    *(keyTmp+key_len)='\0';
    string keyBase64=m_module_name + "_" +keyTmp;
    delete [] keyTmp;
    keyTmp=EncodeBase64(keyBase64.c_str(), keyBase64.length());
    if(keyTmp)
    {
        keyBase64=keyTmp;
        free(keyTmp);
        keyTmp=NULL;
    }

    unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( keyBase64.c_str() );

    //获取机群下cache列表:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
    }

    return cacheMG;

}


}

