
#include "common.h"
#include "logger.h"
#include "file_conf.h"
//#include "ice_implement.h"

#include "master_srv.h"

#include <fcntl.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

using namespace DCS;

namespace DCS{

MasterServer::MasterServer(){

	string home = getenv("HOME");
	string confPath = home + "/etc/dcs_master_srv.conf";
	string logPath = home + "/log/DCSLOG";

    m_Conf = new FileConfIni( confPath );

    DCS_LOG_INIT(logPath, "dcs_master_srv" );

    //DCS_SET_LOG_LEVEL( TRACE );

    DCSLogger::GetInstance()->setLogLevel( m_Conf->getData("SYSTEM", "log_level") );

    m_ChangedFlag=1;
    m_WriteBackFlag=0;

    m_ZKALLDataBuf = new char[1024*1024];
}

MasterServer::~MasterServer(){
    if(m_Conf)
    {
        delete m_Conf;
    }

    if(m_ZookeeperClient)
    {
        delete m_ZookeeperClient;
    }
    if(m_ZKALLDataBuf)
    {
        delete [] m_ZKALLDataBuf;
    }
}


int MasterServer::DoWork(int& argc,	char *argv[])
{
    DCS_LOG(TRACE, "MasterServer::DoWork() begin.");

    DCS_LOG(INFO, "MasterServer start..." );

    int retCode=0;

    //��ʼ��Ŀ¼
    retCode = InitZookeeperDir();
    if(0 != retCode)
    {
        DCS_LOG(WARN, "MasterServer::DoWork failed(InitZookeeperDir).retCode=" << retCode);
        return retCode;
    }

    //������
    while(1)
    {
        retCode = AcquireMasterLock();
        if(0 != retCode)
        {
            DCS_LOG(DEBUG, "MasterServer::DoWork (AcquireMasterLock).retCode=" << retCode);

            TimeWait(10 * 1000);

            continue;
        }
        else
        {
            break;
        }
    }

    //�����߳�ά��cache server ��Ϣ
    pthread_t id;
    retCode=pthread_create(&id, NULL, ThreadMonitor, NULL);
    if(0 != retCode)
    {
        DCS_LOG(WARN, "MasterServer::DoWork failed(pthread_create ThreadMonitor).retCode=" << retCode);
        return retCode;
    }


    while(1)
    {
        //2����
        TimeWait( 120*1000 );
    }

	//��������ͨ�������Զ�����
	//retCode=MasterSrvICEImpl::GetInstance()->ICEInit( argc, argv );
    //if(0 != retCode)
    //{
    //    DCS_LOG(WARN, "MasterServer::DoWork failed(MasterSrvICEImpl).retCode=" << retCode);
    //    return retCode;
    //}


    DCS_LOG(TRACE, "MasterServer::DoWork() end.");

    return retCode;
}

//��ʼ����zookeeperĿ¼
int MasterServer::InitZookeeperDir()
{
    DCS_LOG(TRACE, "MasterServer::InitZookeeperDir() begin.");

    string zk_home = m_Conf->getData("SYSTEM", "zk_home");
    if(!m_ZookeeperClient)
    {
        //zookeeper���
        string zk_server = m_Conf->getData("SYSTEM", "zk_server");

        //����zookeeper
        m_ZookeeperClient = new ZookeeperClient(zk_server, MasterServer::CacheServerWatcher);
    }


    //${zk_home}/cacheserver
    string cacheNode = zk_home + "/cacheserver";
    size_t idx=0;
    string dir;

    while(idx+1 < cacheNode.length())
    {
        dir = cacheNode.substr(0, cacheNode.find("/",idx+1));
        idx = dir.length();
        m_ZookeeperClient->SetNode(dir.c_str(), NULL, 0, 0);
    }


    DCS_LOG(TRACE, "MasterServer::InitZookeeperDir() end.");

    return 0;
}

//����master��
int MasterServer::AcquireMasterLock()
{
    DCS_LOG(TRACE, "MasterServer::AcquireMasterLock() begin.");

    int retCode=-1;

    string zk_home = m_Conf->getData("SYSTEM", "zk_home");
    zk_home += "/master";

    if(m_ZookeeperClient)
    {
        retCode = m_ZookeeperClient->AcquireMutualLock(zk_home.c_str());
    }


    DCS_LOG(TRACE, "MasterServer::AcquireMasterLock() end.");

    return retCode;
}


//���cacheserver�ڵ㣬�������仯������ش���
void * MasterServer::ThreadMonitor( void * p_void )
{
    DCS_LOG(TRACE, "MasterServer::ThreadMonitor() begin.");

    int retCode=0;

    //retCode=MasterServer::GetInstance()->GetZkCacheAll();
    //if( 0 != retCode )
    //{
    //    DCS_LOG(WARN,"MasterServer::GetInstance()->GetZkCacheAll failed.retCode:" << retCode);
    //    exit(-1);
    //}

    int iCnt=0;
    int lifeCycle=0;

    while(1)
    {

        //��ֹzokeeper֪ͨʧЧ��ÿ10����ˢ��һ��
        if( iCnt++ >= 20 )
        {
            iCnt=0;
            MasterServer::GetInstance()->SetChanged();
        }

        retCode = MasterServer::GetInstance()->AcquireMasterLock();
        if(0 == retCode)
        {
            //ˢ�²���
            if( MasterServer::GetInstance()->IfChanged() )
            {
                MasterServer::GetInstance()->GetZkCacheAll();
                MasterServer::GetInstance()->MonitorCacheNodes();
            }
        }

        TimeWait( 30 * 1000 );

        //���ݻ���,600���Ӻ��˳�
        if( lifeCycle++ > 300)
        {
            exit(1);
        }
    }

    DCS_LOG(TRACE, "MasterServer::ThreadMonitor() end.");

    return NULL;
}

//���߼�
//1.��ȡ${zk_home}/cacheserver/all
int MasterServer::GetZkCacheAll()
{
    DCS_LOG(TRACE, "MasterServer::GetAllCacheServer() begin.");
    int retCode=0,ret=0;
    string zk_home = m_Conf->getData("SYSTEM", "zk_home");
    int mg_node_number = m_Conf->getIntData("SYSTEM", "mg_node_number");

    //1.��ȡ${zk_home}/cacheserver/all
    //�ļ���ʽ:mg0000=123.123.123.123:12345:2000,123.123.123.123:12345:2000|mg0001=123.123.123.123:12345:1500,123.123.123.123:12345:1500|
    char *dataBuf = m_ZKALLDataBuf;
    memset(m_ZKALLDataBuf, 0x0, sizeof(m_ZKALLDataBuf));
    int dataLen = 1024*1024;
    string zkCacheServerAll=zk_home+"/cacheserver/all";

    retCode = m_ZookeeperClient->GetNode(zkCacheServerAll.c_str(), MasterServer::CacheServerWatcher, dataBuf, dataLen);
    if(-3 == retCode)
    {
        DCS_LOG(WARN, "MasterServer::GetAllCacheServer() step 1 failed.GetNode:" << zkCacheServerAll
            << "retCode:" << retCode );
        return retCode;
    }
    else if( -2 == retCode )
    {
        m_ZookeeperClient->SetNode(zkCacheServerAll.c_str(), NULL, 0, 0);
        retCode=0;
    }

    if(dataLen < 1024*1024)
    {
        *(dataBuf+dataLen) = '\0';
    }
    else
    {
        *(dataBuf+1024*1024-1) = '\0';
    }

    DCS_LOG(INFO, "get ${zk_home}/cacheserver/all-size:" << dataLen << "data:" << dataBuf );

    m_CacheMGMutex.lock();

    m_CacheMGAll.clear();
    m_CacheMGName.clear();

    char *pBegin=dataBuf;
    char *pFind=NULL;
    string mgUnit, mgName, mgContent;
    string cacheNode,cacheIp,cachePort,cacheSize;
    string::size_type findPos;
    while( NULL != ( pFind = strstr(pBegin,"|") ) )
    {
        //ȡһ����Ⱥ��Ԫ����
        //mg0000=123.123.123.123:12345:2000,123.123.123.123:12345:2000
        *pFind++ = '\0';
        mgUnit = pBegin;
        pBegin = pFind;

        DCS_LOG(DEBUG, "MasterServer::GetZkCacheAll()-mgUint:" << mgUnit);

        //�����Ⱥ��Ԫ����
        ret=CutStringHead(mgUnit, "=", mgName, mgUnit);
        if(ret != 0)
        {
            //bad format
            DCS_LOG(WARN, "MasterServer::GetZkCacheAll(bad format1)-mgUint:" << mgUnit);
            continue;
        }

        //check and append
        mgContent="";
        for(int idx=0; idx<mg_node_number; idx++)
        {
            ret = CutStringHead(mgUnit, ",", cacheNode, mgUnit);
            //Ԫ�ز�������ǰ����
            if(ret != 0)
            {
                idx=mg_node_number;
            }

            //
            ret=CutStringHead(cacheNode, ":", cacheIp, cacheNode);
            if(ret != 0)
            {
                DCS_LOG(DEBUG, "MasterServer::GetZkCacheAll(bad format2)-cacheNode:" << cacheIp);
                continue;
            }

            ret=CutStringHead(cacheNode, ":", cachePort, cacheNode);
            if(ret != 0)
            {
                DCS_LOG(DEBUG, "MasterServer::GetZkCacheAll(bad format3)-cacheNode:" << cachePort);
                continue;
            }

            ret=CutStringHead(cacheNode, ":", cacheSize, cacheNode);


            cacheNode = cacheIp + ":" + cachePort + ":" + cacheSize;
            DCS_LOG(DEBUG, "MasterServer::GetZkCacheAll()-cacheNode:" << cacheNode);

            if( 0 == mgContent.length() )
            {
                mgContent = cacheNode;
            }
            else
            {
                mgContent += "," + cacheNode;
            }
        }

        if(0 == mgContent.length())
        {
            //bad format or ����
            DCS_LOG(DEBUG, "MasterServer::GetZkCacheAll(0 == mgContent.length())-mgName:" << mgName);
        }

        //����ȫ�ֱ���
        m_CacheMGAll.insert( map< string, string >::value_type( mgName, mgContent ) );
        m_CacheMGName.insert( mgName );
        //m_WriteBackFlag=1;
        DCS_LOG(DEBUG, "GetZkCacheAll()insert-mgName:" << mgName << ",mgContent:" << mgContent);
    }

    m_CacheMGMutex.unlock();


    DCS_LOG(TRACE, "MasterServer::GetAllCacheServer() end.");

    return retCode;
}


//2.��ȡ${zk_home}/cacheserver/Ŀ¼������cache server
//3.������������:/����̽��/��Ⱥ���ݻ���
//4.��д${zk_home}/cacheserver/all�ͱ���
int MasterServer::MonitorCacheNodes()
{
    DCS_LOG(TRACE, "MasterServer::MonitorCacheNodes() begin.");

    m_CacheMGMutex.lock();

    int retCode=0;
    int ret=0;
    string zk_home = m_Conf->getData("SYSTEM", "zk_home");
    int mg_node_number = m_Conf->getIntData("SYSTEM", "mg_node_number");
    string cache_ice_port = m_Conf->getData("SYSTEM", "cache_ice_port");
    int failure_period = m_Conf->getIntData("SYSTEM", "failure_period");

    //��ȡ���cacheNodes
    string cacheServerDir = zk_home + "/cacheserver";
    set< string > childs;
    retCode = m_ZookeeperClient->GetNodeChildren( cacheServerDir.c_str(), MasterServer::CacheServerWatcher, childs );
    if(-3 == retCode)
    {
        DCS_LOG(WARN, "MasterServer::MonitorCacheNodes() step 1 failed.GetNodeChildren:" << cacheServerDir
            << "retCode:" << retCode );
        m_CacheMGMutex.unlock();
        return retCode;
    }

    childs.erase("all");

    string mgContent;
    string cacheNode,cacheIp,cachePort,cacheSize;
    set< string >::iterator itChild;

    //debug
    for( itChild=childs.begin(); itChild!=childs.end(); itChild++  )
    {
        mgContent += *itChild;
    }

    DCS_LOG(DEBUG, "GetNodeChildren(cacheserver):" << mgContent);
    mgContent.clear();
    //debug

    //�����Ų�
    map< string , string >::iterator itCacheMGAll=m_CacheMGAll.begin();
    for(; itCacheMGAll != m_CacheMGAll.end(); itCacheMGAll++)
    {
        //check and append
        mgContent=itCacheMGAll->second;
        if(0 == mgContent.length())
        {
            //bad format or ����
            DCS_LOG(DEBUG, "MasterServer::MonitorCacheNodes()-null content,mgName:" << itCacheMGAll->first);
            continue;
        }

        DCS_LOG(DEBUG, "Failure check mgName:" << itCacheMGAll->first << ",mgContent:" << mgContent);

        //������ǰ��Ⱥ������node
        for(int idx=0; idx<mg_node_number; idx++)
        {
            ret = CutStringHead(mgContent, ",", cacheNode, mgContent);
            //Ԫ�ز�������ǰ����
            if(ret != 0)
            {
                idx=mg_node_number;
            }

            ret=CutStringHead(cacheNode, ":", cacheIp, cacheNode);
            if(ret != 0)
            {
                DCS_LOG(WARN, "MasterServer::MonitorCacheNodes(bad format!)-cacheNode:" << cacheIp);
                continue;
            }

            ret=CutStringHead(cacheNode, ":", cachePort, cacheNode);
            if(ret != 0)
            {
                DCS_LOG(WARN, "MasterServer::MonitorCacheNodes(bad format!!)-cacheNode:" << cachePort);
                continue;
            }

            ret=CutStringHead(cacheNode, ":", cacheSize, cacheNode);

            cacheNode = cacheIp + ":" + cachePort + ":" + cacheSize;

            //���ڵ��Ƿ���
            itChild=childs.find(cacheNode);
            if( itChild == childs.end() )
            {
                //�����������̽��
                //ret = MasterSrvICEImpl::GetInstance()->CacheZookeeperLost(cacheIp, cache_ice_port, cacheNode);
                ret = RemoteRepairNode(cacheIp, cache_ice_port, cacheNode);
                DCS_LOG(DEBUG, "RemoteRepairNode cacheIp:" << cacheIp
                << ",cache_ice_port:" << cache_ice_port << ",cacheNode:" << cacheNode
                << ",ret:" << ret );
                if(0 == ret)
                {
                    //̽��ɹ����ʧ���б���ɾ��
                    m_FailedCacheList.erase(cacheNode);
                }
                else
                {
                    //̽��ʧ��
                    time_t curTime = time(NULL);
                    map< string, time_t >::iterator itFailed = m_FailedCacheList.find( cacheNode );
                    if( itFailed == m_FailedCacheList.end() )
                    {
                        //�����ʧ���б�
                        m_FailedCacheList.insert( map< string, time_t >::value_type( cacheNode, curTime ) );
                        DCS_LOG(DEBUG, "m_FailedCacheList.insert(cacheNode:" << cacheNode << ",time:" << curTime );
                    }
                    else
                    {
                        //�ж��Ƿ����ʧ������
                        if( curTime > (itFailed->second + failure_period) )
                        {
                            //��Ҫ��all��Ϣ��ɾ����node
                            string mgNewContent =itCacheMGAll->second;
                            std::string::size_type index = mgNewContent.find( cacheNode+"," );
                            if( index != std::string::npos )
                            {
                                mgNewContent.replace( index , cacheNode.length()+1, "" );
                            }
                            else
                            {
                                index = mgNewContent.find( cacheNode );
                                if( index != std::string::npos )
                                {
                                    mgNewContent.replace( index , cacheNode.length(), "" );
                                }
                            }

                            DCS_LOG(INFO, "m_FailedCacheList.exceed failure_period(cacheNode:" << cacheNode << ",time:" << itFailed->second );

                            m_FailedCacheList.erase(itFailed);

                            //���²��û�д��־
                            if( mgNewContent.rfind(",") != std::string::npos
                                && mgNewContent.rfind(",") == (mgNewContent.length()-1) )
                            {
                                itCacheMGAll->second = mgNewContent.substr(0,mgNewContent.length()-1);
                            }
                            else
                            {
                                itCacheMGAll->second = mgNewContent;
                            }

                            DCS_LOG(DEBUG, "Update Mg:"<< itCacheMGAll->first << ",mgNewContent:" << itCacheMGAll->second);

                            m_WriteBackFlag=1;
                        }
                    }

                }

            }  //end of if( itChild == childs.end() )
            else
            {
                //������childs��ɾ��
                childs.erase( itChild );

                //��ʧ���б���ɾ��
                m_FailedCacheList.erase(cacheNode);
            }

        } //end of for(������ǰ��Ⱥ������node)

    } //end of for(�����ų� ������Ⱥ)


    //��Ⱥ���ݻ���,childsʣ��node��Ϊ�����ڵ�
    //��ȺMG��������:cacheIp��ͬ��cacheSize��ͬ
    //�Ȳ���ԭ��MG��Ϣ�����Ƿ��п���λ�ã�������뵽ԭ��MG
    //��������MG
    bool ifNewMG=false;
    for( itChild = childs.begin(); itChild != childs.end(); itChild++)
    {
        ifNewMG=false;

        //��ʽ���
        cacheNode = itChild->c_str();
        DCS_LOG(DEBUG, "new cacheNode:" << cacheNode);
        ret=CutStringHead(cacheNode, ":", cacheIp, cacheNode);
        if(ret != 0)
        {
            DCS_LOG(WARN, "MasterServer::MonitorCacheNodes(zookeeper childs bad format@)-cacheNode:" << cacheIp);
            continue;
        }


        ret=CutStringHead(cacheNode, ":", cachePort, cacheNode);
        if(ret != 0)
        {
            DCS_LOG(WARN, "MasterServer::MonitorCacheNodes(zookeeper childs bad format@@)-cacheNode:" << cachePort);
            continue;
        }

        ret=CutStringHead(cacheNode, ":", cacheSize, cacheNode);

        cacheNode = cacheIp + ":" + cachePort + ":" + cacheSize;

        //��λ�ϻ�Ⱥ
        if( false == ifNewMG )
        {
            ifNewMG=true;

            //����ԭ��MG��Ϣ
            map< string , string >::iterator itCacheMGAll = m_CacheMGAll.begin();
            for( ; itCacheMGAll != m_CacheMGAll.end(); itCacheMGAll++ )
            {
                ret = CheckMGAndAppend(itCacheMGAll->second, cacheIp, cachePort, cacheSize);
                DCS_LOG(DEBUG, "CheckMGAndAppend(orgMg:" << itCacheMGAll->second << ",cacheIp:" << cacheIp
                    << ",cachePort:" << cachePort << ",cacheSize:" << cacheSize << ",ret:" << ret );
                if(0 == ret)
                {
                    DCS_LOG(INFO, "CheckMGAndAppend(orgMg:" << itCacheMGAll->second << ",cacheIp:" << cacheIp
                        << ",cachePort:" << cachePort << ",cacheSize:" << cacheSize << ",ret:" << ret );

                    //�ϼ�Ⱥ��λ�ɹ�����Ϊ������Ҫ������Ⱥ
                    ifNewMG = false;
                    break;
                }

            }  //end of for

        }  //end of ��λ�ϻ�Ⱥ

        //������Ⱥ:��β��׷��
        if( ifNewMG )
        {
            string lastMg, newMg;

            //��ȡ���һ����Ⱥ����
            set<string>::reverse_iterator rbegin = m_CacheMGName.rbegin();
            if( rbegin == m_CacheMGName.rend() )
            {
                lastMg="";
            }
            else
            {
                lastMg = rbegin->c_str();
            }

            //��m_CacheMGAll�в�������ϸ��Ϣ
            map< string, string >::iterator itCacheMGAll = m_CacheMGAll.find(lastMg);
            if( itCacheMGAll == m_CacheMGAll.end() )
            {
                //û�ҵ�����Ҫ���
                if( lastMg.length() <= 0 )
                {
                    //��һ��Ⱥ
                    IncMgNum(lastMg, newMg);
                    m_CacheMGName.insert(newMg);
                    m_CacheMGAll.insert( map< string, string >::value_type( newMg, cacheNode ) );

                    m_WriteBackFlag=1;

                    DCS_LOG(DEBUG, "First machine group()newMg:" << newMg << ",cacheNode:" << cacheNode);
                }
                else
                {
                    //m_CacheMGName��m_CacheMGAll�����ṹ��һ������Ϊ�쳣���
                    DCS_LOG(WARN, "m_CacheMGAll is not consistent with m_CacheMGName!!lastMg:" << lastMg );
                    m_CacheMGName.insert(lastMg);
                    m_CacheMGAll.insert( map< string, string >::value_type( lastMg, cacheNode ) );

                    m_WriteBackFlag=1;
                }
            }
            else
            {
                //�ҵ��Ļ�������Ƿ����׷�ӣ������½�
                ret = CheckMGAndAppend(itCacheMGAll->second, cacheIp, cachePort, cacheSize);
                DCS_LOG(INFO, "CheckMGAndAppend(orgMg:" << itCacheMGAll->second << ",cacheIp:" << cacheIp
                    << ",cachePort:" << cachePort << ",cacheSize:" << cacheSize << ",ret:" << ret );
                if(0 != ret)
                {
                    IncMgNum(lastMg, newMg);
                    m_CacheMGName.insert(newMg);
                    m_CacheMGAll.insert( map< string, string >::value_type( newMg, cacheNode ) );

                    m_WriteBackFlag=1;
                }
            }
        }  //end of if ������Ⱥ:��β��׷��
    }  //end of for(��Ⱥ����)


    //��д${zk_home}/cacheserver/all�ͱ���
    if( 1 == m_WriteBackFlag )
    {

        m_WriteBackFlag = 0;

        string zkCacheServerAll=zk_home+"/cacheserver/all";
        string cacheServerAll;

        map< string , string >::iterator itCacheMGAll = m_CacheMGAll.begin();
        for( ; itCacheMGAll != m_CacheMGAll.end(); itCacheMGAll++ )
        {
            cacheServerAll += itCacheMGAll->first + "=" + itCacheMGAll->second + "|";
        }

        DCS_LOG(INFO, "write back ${zk_home}/cacheserver/all:" << cacheServerAll);

        if(m_ZookeeperClient)
        {
            ret = m_ZookeeperClient->SetNode(zkCacheServerAll.c_str(), cacheServerAll.c_str(), cacheServerAll.length(), 0);
            if(0 != ret)
            {
                DCS_LOG(WARN, "zookeeper write back cacheserverall failed. ret:" << ret );
            }
        }

    	string home = getenv("HOME");
    	string cacheAllPath = home + "/etc/app.dcs.cacheserver.all";
        int pFile=open(cacheAllPath.c_str(), O_CREAT|O_RDWR|O_TRUNC, S_IREAD|S_IWRITE);
        write(pFile, cacheServerAll.c_str(), cacheServerAll.length());
        close(pFile);
    }

    //�ñ仯��־
    if( m_FailedCacheList.empty() )
    {
        m_ChangedFlag = 0;
    }
    else
    {
        map< string , time_t >::iterator itFailed = m_FailedCacheList.begin();
        time_t curTime = time(NULL);
        DCS_LOG(INFO, "m_FailedCacheList.size():" << m_FailedCacheList.size());
        for(; itFailed != m_FailedCacheList.end(); itFailed++)
        {
            DCS_LOG(DEBUG, "m_FailedCacheList[cacheNode:" << itFailed->first
                << ",period:" << curTime-itFailed->second << "s].");
        }
    }

    m_CacheMGMutex.unlock();


    DCS_LOG(TRACE, "MasterServer::MonitorCacheNodes() end.");
    return 0;
}

//zookeeper�ص����cache server �б�仯
void MasterServer::CacheServerWatcher(int type, const char *path)
{
    DCS_LOG(TRACE, "MasterServer::CacheServerWatcher() begin.");

    DCS_LOG(DEBUG, "MasterServer::CacheServerWatcher(type:" << type);

    MasterServer::GetInstance()->SetChanged();

    DCS_LOG(TRACE, "MasterServer::CacheServerWatcher() end.");
}

//[mg0000,mg9999]
void MasterServer::IncMgNum(const string& lastMg, string &newMg)
{
    DCS_LOG(TRACE, "MasterServer::IncMgNum() begin.lastMg:" << lastMg );

        if(lastMg.length() <= 0
                || string::npos == lastMg.find("mg"))
        {
                newMg="mg0000";
                return;
        }

        newMg="";

        int lastNum = atoi( lastMg.substr(2).c_str() );
        char sBuf[8]={0};
        sprintf(sBuf,"mg%04d",lastNum);
        if(lastMg != sBuf)
        {
            DCS_LOG(WARN, "MasterServer::IncMgNum()  parse failed.lastMg:" << lastMg );
            return;
        }

        lastNum++;
        if(lastNum >= 10000)
        {
            DCS_LOG(WARN, "MasterServer::IncMgNum()  exceeded.lastMg:" << lastMg );
            return;
        }

        memset(sBuf, 0x0, sizeof(sBuf) );
        sprintf(sBuf,"mg%04d",lastNum);
        newMg=sBuf;

        DCS_LOG(TRACE, "MasterServer::IncMgNum() end.newMg:" << newMg );

        return;
}

//�����Ⱥ���Ƿ���0�����򷵻�-1
int MasterServer::CheckMGAndAppend(string &orgMg, const string &cacheIp, const string &cachePort, const string &cacheSize)
{
    DCS_LOG(TRACE, "MasterServer::CheckMGAndAppend() begin." );

    int mg_node_number = m_Conf->getIntData("SYSTEM", "mg_node_number");
    int nodeCnt=0;

    vector < string > server_list;
    StringSplit(orgMg, ",", server_list);
    nodeCnt = server_list.size();

    //node����
    if(nodeCnt >= mg_node_number)
    {
        return -1;
    }

    string cacheNode = cacheIp + ":" + cachePort + ":" + cacheSize;

    //����Ⱥ����ԭ��:cacheIp��ͬ��cacheSize��ͬ
    if(nodeCnt > 0)
    {

        //cacheIp��ͬ
        string::size_type ipFind = orgMg.find(cacheIp);
        if( ipFind != string::npos )
        {
            return -2;
        }

        //cacheSize��ͬ
        string oneNode = server_list[0];
        string::size_type sizeFind = oneNode.rfind(":");
        string mgSize;
        if( sizeFind != string::npos )
        {
            mgSize = oneNode.substr( sizeFind + 1 );
        }


        if( cacheSize != mgSize )
        {
            DCS_LOG(INFO, "MasterServer::CheckMGAndAppend() orgMg:" << orgMg << "|sizeFind:" << sizeFind
            << "|cacheSize:" << cacheSize << "|mgSize:" << mgSize );

            return -3;
        }

        //append
        server_list.push_back(cacheNode);
        vector< string >::iterator it=server_list.begin();
        orgMg.clear();
        size_t idxNum=0;
        for( ; it!=server_list.end(); it++ )
        {
            idxNum++;
            if(idxNum == server_list.size())
            {
                orgMg += *it;
            }
            else
            {
                orgMg += *it + ",";
            }
        }

    }
    else
    {
        //MG�ѿ���
        orgMg = cacheNode;
    }

    m_WriteBackFlag=1;

    DCS_LOG(TRACE, "MasterServer::CheckMGAndAppend() end." );

    return 0;
}

//Զ���޸��ڵ�
int MasterServer::RemoteRepairNode(const string &ip, const string &port, const string &cacheNode)
{
    DCS_LOG(TRACE, "MasterServer::RemoteRepairNode() begin." );

    DCS_LOG(DEBUG, "MasterServer::RemoteRepairNode() ip:" << ip
     << ",port:" << port
     << ",cacheNode:" << cacheNode );

    int sockfd;

    struct sockaddr_in servaddr;

    ::bzero( &servaddr, sizeof(servaddr) );

    servaddr.sin_family = AF_INET;

    servaddr.sin_port = htons( stringToInt(port) );

    if( ::inet_pton(AF_INET, ip.c_str(), &servaddr.sin_addr) <= 0 )
    {
        return -1;
    }
    //DCS_LOG(DEBUG, "MasterServer::RemoteRepairNode()1111111111111");

    sockfd = ::socket(AF_INET, SOCK_DGRAM, 0);

    struct timeval timeout={3,0};//3s
    int ret=setsockopt(sockfd,SOL_SOCKET,SO_SNDTIMEO,&timeout,sizeof(timeout));
    ret=setsockopt(sockfd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));

    if(::connect(sockfd, (struct sockaddr *)&servaddr, sizeof(struct sockaddr)) == -1)
    {
        ::close(sockfd);
        return -2;
    }
    //DCS_LOG(DEBUG, "MasterServer::RemoteRepairNode()222222222222");

    //��������
    std::string sendData;
    sendData = "REPAIRNODE|" + cacheNode;
    ::write(sockfd, sendData.c_str(), sendData.length());

    char sBuf[100];
    ::read(sockfd, sBuf, 100);
    if(0 != ::strncmp(sBuf, "+OK", 3))
    {
        ::close(sockfd);
        return -3;
    }
    //DCS_LOG(DEBUG, "MasterServer::RemoteRepairNode()333333333333");

    ::close(sockfd);


    DCS_LOG(TRACE, "MasterServer::RemoteRepairNode() end." );

    return 0;
}



}

//����������
int main( int argc, char **argv )
{
    MasterServer::GetInstance()->DoWork(argc, argv);

	return 0;
}
