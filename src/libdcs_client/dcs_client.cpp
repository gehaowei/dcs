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
// ��������: Open
// ��������: ��ʼ��������
// �������: confFile�����ļ�
// �������: ��
// ����ֵ  : �ɹ�����0;�쳣���ظ���
//-------------------------------------------------------------------------------
int DCSClient::Open(const char *confFile)
{
    DCS_LOG(TRACE, "DCSClient::Open() begin.");

    string cfg = getenv("HOME");
    cfg += "/etc/dcs_client.conf";

    //�����ļ�
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

    //dcs cache������
    retCode += DCSCacheManager::GetInstance()->Init(conf);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->Init() failed.");
    }

    //local cache������
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
// ��������: CacheSet
// ��������: дcache
// �������: key��(���ڲ����Զ�base64����), key_len ������
//      valֵ��val_lenֵ����,  expire�������ʱ��(��λ�룬ȱʡΪ0��ʾ��������)
// �������: ��
// ����ֵ  : �ɹ�����0;�쳣���ظ���
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

    //��ȡ��Ⱥ��cache�б�:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
        return retCode;
    }

    DCS_LOG(DEBUG, "DCSClient::CacheSet() keyBase64:" <<keyBase64<< ",curHashValue:" << curHashValue << ",cacheMG:" << cacheMG );

    //�����̨
    vector < string > server_list;
    StringSplit(cacheMG, ",", server_list);
    string selectServer;

    //������һ����˫д�����
    srand( (unsigned)time( NULL ) );
    int idx = rand() % server_list.size();

    //ǿһ���Ե�д����
    if(1 == consistency)
    {
        idx = curHashValue % server_list.size();
    }

    string failedServers;
    string::size_type tryTime=0;
    while(1)
    {
        tryTime++;

        //���ѡȡ
        if(server_list.size() > 0)
        {
            idx = idx % server_list.size();
            selectServer = server_list[idx++];
        }

        //����cache
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

        //���ʧ�ܴ���
        if(tryTime >= server_list.size())
        {
            //ȫ��ʧ��
            break;
        }
    }

    //�޳��ɹ�������
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

    //�����������set����node
    if(0 == retCode && cacheMG.length() > 0 )
    {

        //������һ�������첽����
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
            //ǿһ�²�˫д
            //retCode=DCSCacheManager::DoCacheOpTask(oneTask);
        }

    }

    //set����
    if(0 == consistency)
    {
        LocalCacheManager::GetInstance()->CacheSet(keyBase64.c_str(), keyBase64.length(), val, val_len, expire);
    }

    DCS_LOG(TRACE, "DCSClient::CacheSet() end.");

    return retCode;
}

//-------------------------------------------------------------------------------
// ��������: CacheGet
// ��������: ȡcache
// �������: key��(���ڲ����Զ�base64����), key_len ������
//      value��������(Ӧ�ô���ָ���ַ���ⴴ���ڴ沢��ֵָ�룬Ӧ��������free�ͷ��ڴ�)
//      retlen���ݵĳ���(size_t������Ӧ�ô���������size_t�����ַ�����size_t������и�ֵ)
// �������: ��
// ����ֵ  : �ɹ�����0;�쳣���ظ���
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

    //������һ����
    if( 0 == consistency )
    {
        //get���أ��ɹ��򷵻�
        retCode = LocalCacheManager::GetInstance()->CacheGet(keyBase64.c_str(), keyBase64.length(), value, retlen);
        if(0 == retCode && 0 != (*retlen) )
        {
            return 0;
        }
    }

    unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( keyBase64.c_str() );

    //��ȡ��Ⱥ��cache�б�:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
        return retCode;
    }

    DCS_LOG(DEBUG, "DCSClient::CacheGet() keyBase64:" <<keyBase64<< ",curHashValue:" << curHashValue << ",cacheMG:" << cacheMG );

    //�����̨get��????ʧ������get����̨??consistency??
    vector < string > server_list;
    StringSplit(cacheMG, ",", server_list);
    string selectServer;

    //������һ����˫д�����
    srand( (unsigned)time( NULL ) );
    int idx = rand() % server_list.size();

    //ǿһ���Ե�д����
    if(1 == consistency)
    {
        idx = curHashValue % server_list.size();
    }

    string failedServers;
    string::size_type tryTime=0;
    while(1)
    {
        tryTime++;

        //���ѡȡ
        if(server_list.size() > 0)
        {
            idx = idx % server_list.size();
            selectServer = server_list[idx++];
        }

        //����cache
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
                //���ٶ���̨�ͻ�д
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

        //����Ƿ���Ҫ�ٶ���һ̨:ǿһ�����򲻿��ٶ���������һ���Կ��Լ�����
        if(tryTime >= server_list.size()
            || 1 == consistency )
        {
            break;
        }
    }

    //��ȡ���ݳɹ�
    if( 0 == retCode
        && NULL != (*value)
        && (*retlen) > 0)
    {
        //set���ص���ͬ��
        if(0 == consistency)
        {
            LocalCacheManager::GetInstance()->CacheSet(keyBase64.c_str(), keyBase64.length(), (*value), (*retlen), 0, false);
        }

        //�����������setʧ��node(20130529���߼�ʧЧ)
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
// ��������: CacheDel
// ��������: ɾcache
// �������: key��(���ڲ����Զ�base64����), key_len ������
// �������: ��
// ����ֵ  : �ɹ�����0;�쳣���ظ���
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

    //��ȡ��Ⱥ��cache�б�:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
        return retCode;
    }

    DCS_LOG(DEBUG, "DCSClient::CacheDel() keyBase64:" <<keyBase64<< ",curHashValue:" << curHashValue << ",cacheMG:" << cacheMG );

    //�����̨
    vector < string > server_list;
    StringSplit(cacheMG, ",", server_list);
    string selectServer;

    //������һ����˫д�����
    srand( (unsigned)time( NULL ) );
    int idx = rand() % server_list.size();

    //ǿһ���Ե�д����
    if(1 == consistency)
    {
        idx = curHashValue % server_list.size();
    }

    string failedServers;
    string::size_type tryTime=0;
    while(1)
    {
        tryTime++;

        //���ѡȡ
        if(server_list.size() > 0)
        {
            idx = idx % server_list.size();
            selectServer = server_list[idx++];
        }

        //����cache
        //DCS_LOG(DEBUG, "del key:" << keyBase64 << " from selectServer:" << selectServer);
        MemcacheClient *PMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(selectServer, curHashValue);
        if(PMemCli)
        {
            retCode = PMemCli->CacheDel(keyBase64.c_str(), keyBase64.length());
            if( 0 == retCode || 1 == retCode ) //����1��ʾ������
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

        //���ʧ�ܴ���
        if(tryTime >= server_list.size())
        {
            //ȫ��ʧ��
            break;
        }
    }

    //�޳��ɹ�������
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


    //�����������del����node
    if( cacheMG.length() > 0 )
    {

        //������һ�������첽����
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
            //ǿһ�²�˫д
            //retCode=DCSCacheManager::DoCacheOpTask(oneTask);
        }
    }

    //������һ����
    if( 0 == consistency )
    {
        //set����
        LocalCacheManager::GetInstance()->CacheDel(keyBase64.c_str(), keyBase64.length());
    }

    DCS_LOG(TRACE, "DCSClient::CacheDel() end.");

    return retCode;
}

//-------------------------------------------------------------------------------
// ��������: Close
// ��������: �رտͻ��˿�
// �������: ��
// �������: ��
// ����ֵ  : ����0
//-------------------------------------------------------------------------------
int DCSClient::Close()
{
    DCS_LOG(TRACE, "DCSClient::Close() begin.");

    int retCode=0;

    //dcs cache������

    //local cache������

    //memcached�����ͷ�

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

    //��ȡ��Ⱥ��cache�б�:1.2.3.4:13000,1.2.3.5:13001
    string cacheMG;
    retCode = DCSCacheManager::GetInstance()->GetCacheMG(curHashValue, cacheMG);
    if(retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetInstance()->GetCacheMG() failed.keyBase64:" << keyBase64.c_str());
    }

    return cacheMG;

}


}

