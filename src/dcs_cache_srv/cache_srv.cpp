
#include "common.h"
#include "logger.h"
#include "file_conf.h"
//#include "ice_implement.h"
#include "cache_register.h"

#include "cache_srv.h"

using namespace DCS;

namespace DCS{

CacheServer::CacheServer(){

    string home = getenv("HOME");
    string confPath = home + "/etc/dcs_cache_srv.conf";
    string logPath = home + "/log/DCSLOG";

    m_Conf = new FileConfIni( confPath );

    DCS_LOG_INIT(logPath, "dcs_cache_srv" );

    //DCS_SET_LOG_LEVEL( DEBUG );
    DCSLogger::GetInstance()->setLogLevel( m_Conf->getData("SYSTEM", "log_level") );
}

CacheServer::~CacheServer(){
    if(m_Conf)
    {
        delete m_Conf;
    }
}


int CacheServer::DoWork(int& argc,	char *argv[])
{
    int retCode=0;

    DCS_LOG(INFO, "CacheServer start..." );

    //注册zookeeper
    retCode = CacheRegister::GetInstance()->RegistAll();
    if(0 != retCode)
    {
        DCS_LOG(WARN, "CacheServer::DoWork failed(CacheRegister).retCode=" << retCode);
        return retCode;
    }

	//启动本地通信器，自动阻塞
	//retCode = CacheSrvICEImpl::GetInstance()->ICEInit( argc, argv );
    //if(0 != retCode)
    //{
    //    DCS_LOG(WARN, "CacheServer::DoWork failed(CacheSrvICEImpl).retCode=" << retCode);
    //    return retCode;
    //}

    while(1)
    {
        //2分钟
        TimeWait( 120*1000 );
    }


    return retCode;
}

}


//主驱动函数
int main( int argc, char **argv )
{
    CacheServer::GetInstance()->DoWork(argc, argv);

	return 0;
}

