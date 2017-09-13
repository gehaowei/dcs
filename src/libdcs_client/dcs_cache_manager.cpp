#include "dcs_cache_manager.h"
#include "logger.h"
#include "md5.h"
#include <math.h>
#include "common.h"
#include <stdlib.h>
#include "memcache_client.h"


//qsort�����Ͻڵ� �ȽϺ���
static int MGCompare( DCS::stContinuumMG *a, DCS::stContinuumMG *b )
{
    return ( a->m_Point < b->m_Point ) ?  -1 : ( ( a->m_Point > b->m_Point ) ? 1 : 0 );
}

namespace DCS{
DCSCacheManager::DCSCacheManager()
{
    m_ZookeeperClient=NULL;
    m_ContinuumMG=NULL;
    m_NumPoints=0;

    m_ReBuildFlag=true;
    m_NeedReBuild=true;

    m_CacheOpTaskCond = new Cond(&m_CacheOpTaskMutex);

    m_ZKALLDataBuf = new char[1024*1024];

    m_OpTaskProtectSize = 0;

}

DCSCacheManager::~DCSCacheManager()
{
    if(m_ZookeeperClient)
    {
        delete m_ZookeeperClient;
    }

    if(m_ZKALLDataBuf)
    {
        delete [] m_ZKALLDataBuf;
    }
}

//-------------------------------------------------------------------------------
// ��������: Init
// ��������: ��ʼ��������
// �������: conf�����ļ�ָ��
// �������: ��
// ����ֵ  : �ɹ�����0;�쳣���ظ���
//-------------------------------------------------------------------------------
int DCSCacheManager::Init(FileConfIni *conf)
{
    DCS_LOG(TRACE, "DCSCacheManager::Init() begin.");

    int retCode=0;

    //��ȡ������
    m_ZkServers = conf->getData("DCS_CLIENT", "zk_server");

    string zk_home = conf->getData("DCS_CLIENT", "zk_home");
    m_ZkCacheAll = zk_home + "/cacheserver/all";

    char * sReBuildFlag = conf->getData("DCS_CLIENT", "zk_rebuild");
    if(NULL != sReBuildFlag
        && 0 == std::string(sReBuildFlag).compare("0") )
    {
        m_ReBuildFlag = false;
    }
    m_Expire = time(NULL);

    //buildһ���Թ�ϣ��
    retCode = CreateContinuum();
    if(0 != retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::Init()-CreateContinuum failed.retCode:" << retCode );
        return retCode;
    }

    pthread_t id;

    for(int i=0; i<4; i++)
    {
        //����ͬ��ͨ��
        retCode=pthread_create(&id, NULL, ThreadCacheOp, NULL);
        if(0 != retCode)
        {
            DCS_LOG(WARN, "pthread_create ThreadCacheOp.retCode=" << retCode);
            return retCode;
        }
    }

    DCS_LOG(TRACE, "DCSCacheManager::Init() end.");

    return retCode;
}

//-------------------------------------------------------------------------------
// ��������: GetCacheMG
// ��������: ��ȡcache��Ⱥ��Ϣ
// �������: key��
// �������: cacheMg ��Ⱥcache server�б��Զ��ŷָ�����ʽ:1.2.3.4:13000,1.2.3.5:13001
// ����ֵ  : �ɹ�����0;�쳣���ظ���
//-------------------------------------------------------------------------------
int DCSCacheManager::GetCacheMG(const unsigned int key, string &cacheMg)
{
    DCS_LOG(TRACE, "DCSCacheManager::GetCacheMG() begin.key:" << key);

    int retCode=0;
    //����һ����hash��
    retCode = CreateContinuum();
    if(0 != retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::GetCacheMG()-CreateContinuum failed.retCode:" << retCode );
        return retCode;
    }

    //����һ����hash��
    m_ContHashMutex.lock();
    cacheMg="";

    int highp = m_NumPoints;
    int lowp = 0, midp=0;
    unsigned int midval, midval1;

    // divide and conquer array search to find server with next biggest
    // point after what this key hashes to
    while ( 1 )
    {
        midp = (int)( ( lowp+highp ) / 2 );

        //if at the end, roll back to zeroth
        if ( midp == m_NumPoints )
        {
            cacheMg = m_ContinuumMG[0].m_Servers;
            //DCS_LOG(DEBUG, "DCSCacheManager::GetCacheMG(break1).key:" << key << ",m_ContinuumMG["<< 0 << "].m_Servers:" << m_ContinuumMG[0].m_Servers << ",cacheMg:" << cacheMg );

            break;
        }

        midval = m_ContinuumMG[midp].m_Point;
        midval1 = ( midp == 0 ) ? 0 : m_ContinuumMG[midp-1].m_Point;

        //match
        if ( key <= midval && key > midval1 )
        {
            cacheMg = m_ContinuumMG[midp].m_Servers;
            //DCS_LOG(DEBUG, "DCSCacheManager::GetCacheMG(break2).key:" << key << ",m_ContinuumMG["<< midp << "].m_Servers:" << m_ContinuumMG[midp].m_Servers << ",cacheMg:" << cacheMg );
            break;
        }

        if ( midval < key )
        {
            lowp = midp + 1;
        }
        else
        {
            highp = midp - 1;
        }

        //through, roll back to zeroth
        if ( lowp > highp )
        {
            cacheMg = m_ContinuumMG[0].m_Servers;
            //DCS_LOG(DEBUG, "DCSCacheManager::GetCacheMG(break3).key:" << key << ",m_ContinuumMG["<< 0 << "].m_Servers:" << m_ContinuumMG[0].m_Servers << ",cacheMg:" << cacheMg );
            break;
        }
    }

    if(cacheMg.length() <= 0)
    {
        retCode=-1;
        DCS_LOG(WARN, "DCSCacheManager::GetCacheMG() not match.key:" << key << ",cacheMg:" << cacheMg);
    }

    m_ContHashMutex.unlock();

    DCS_LOG(TRACE, "DCSCacheManager::GetCacheMG() end.key:" << key << ",cacheMg:" << cacheMg);
    return retCode;
}

//-------------------------------------------------------------------------------
// ��������: PutCacheOpTask
// ��������: ���cache�������񵽶����첽ִ��
// �������: cacheOp�ṹ���ⲿ�����ڴ���䡣���ദ����Ϻ���ͷš�
// �������: ��
// ����ֵ  : �ɹ�����0;�쳣���ظ���
//-------------------------------------------------------------------------------
int DCSCacheManager::PutCacheOpTask(const stCacheOpTask &cacheOp )
{
    DCS_LOG(TRACE, "DCSCacheManager::PutCacheOpTask() begin.");

    if(m_OpTaskProtectSize>100*1024*1024)
    {
        //DCS_LOG(WARN, "DCSCacheManager::PutCacheOpTask() Mem size exceeded ! size=" << m_OpTaskProtectSize/(1024*1024) << "M" );
        //TimeWait(10);
        return -1;
    }

    m_CacheOpTaskMutex.lock();

    //ռ���ڴ����
    m_OpTaskProtectSize += (sizeof(stCacheOpTask) + cacheOp.m_ServersLen+cacheOp.m_KeyLen+cacheOp.m_ValLen);

    //����
    m_CacheOpTaskQueue.push(cacheOp);
    m_CacheOpTaskCond->signal();
    m_CacheOpTaskMutex.unlock();

    DCS_LOG(TRACE, "DCSCacheManager::PutCacheOpTask() end.");
    return 0;
}

//-------------------------------------------------------------------------------
// ��������: DoCacheOpTask
// ��������: ֱ��ִ��cache��������
// �������: oneTask�ṹ���ⲿ�����ڴ���䡣���ദ����Ϻ���ͷš�
// �������: ��
// ����ֵ  : �ɹ�����0;�쳣���ظ���
//-------------------------------------------------------------------------------
int DCSCacheManager::DoCacheOpTask(const stCacheOpTask &oneTask )
{
    DCS_LOG(TRACE, "DCSCacheManager::DoCacheOpTask() begin.");

    //����
    if( oneTask.m_CacheServers && oneTask.m_Key )
    {
        DCS_LOG(DEBUG, "Do oneTask.m_CacheServers:" << oneTask.m_CacheServers << ",m_Key:" << oneTask.m_Key
            << ",m_OpType:" << oneTask.m_OpType );

        unsigned int curHashValue = DCSCacheManager::GetInstance()->Str2Hash( oneTask.m_Key );

        string servers = oneTask.m_CacheServers;
        string oneServer;
        int ret;
        while(servers.length())
        {
            CutStringHead(servers, ",", oneServer, servers);
            MemcacheClient *pMemCli = MemcacheClientManager::GetInstance()->GetMemcacheClient(oneServer,curHashValue);
            {
                if(pMemCli)
                {
                    if( 1 == oneTask.m_OpType  && oneTask.m_Val )
                    {
                        ret = pMemCli->CacheSet(oneTask.m_Key, oneTask.m_KeyLen, oneTask.m_Val, oneTask.m_ValLen, oneTask.m_Expire);
                        if(ret)
                        {
                            DCS_LOG(WARN, "CacheSet failed.server:" << oneServer << ",key:" << oneTask.m_Key << ",ret:" << ret);
                        }
                    }
                    else if ( 2 == oneTask.m_OpType )
                    {
                        ret = pMemCli->CacheDel(oneTask.m_Key, oneTask.m_KeyLen);
                        if(ret)
                        {
                            DCS_LOG(WARN, "CacheDel failed.server:" << oneServer << ",key:" << oneTask.m_Key << ",ret:" << ret);
                        }
                    }
                }
                else
                {
                    DCS_LOG(WARN, "MemcacheClient not ready.server:" << oneServer << ",key:" << oneTask.m_Key);
                }
            }
        }
    }
    else
    {
        DCS_LOG(WARN, "oneTask illegal.");
    }

    //�ͷ��ڴ�
    if(oneTask.m_CacheServers)
    {
        delete [] oneTask.m_CacheServers;
    }
    if( oneTask.m_Key )
    {
        delete [] oneTask.m_Key;
    }
    if( oneTask.m_Val )
    {
        delete [] oneTask.m_Val;
    }

    DCS_LOG(TRACE, "DCSCacheManager::DoCacheOpTask() end.");

    return 0;
}


//����һ����hash��
int DCSCacheManager::CreateContinuum()
{
    int retCode=0;
    DCS_LOG(TRACE, "DCSCacheManager::CreateContinuum() begin.");

    time_t curTime = time(NULL);
    if(!DCSCacheManager::GetInstance()->m_ReBuildFlag)
    {
        curTime = m_Expire;
    }

    if(false == m_NeedReBuild
        && ( ( curTime - m_Expire ) < 3600 ) )
    {
        return retCode;
    }


    //��zookeeper��ȡ���Ļ�Ⱥ����
    vector< stMGInfo * > m_MGVector;
    unsigned long m_TotalSize=0;

    m_ZookeeperMutex.lock();

    //����zookeeper
    retCode = ConnectZookeeper();
    if(0 != retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::CreateContinuum()-ConnectZookeeper failed.retCode:" << retCode );
        m_ZookeeperMutex.unlock();
        return retCode;
    }

    //��ȡzookeeper��cache all��Ϣ
    retCode = GetZkCacheAll(m_MGVector,m_TotalSize);
    if(0 != retCode)
    {
        DCS_LOG(WARN, "DCSCacheManager::CreateContinuum()-GetZkCacheAll failed.retCode:" << retCode );
        m_ZookeeperMutex.unlock();
        return retCode;
    }

    m_ZookeeperMutex.unlock();

    m_NeedReBuild = false;
    m_Expire = curTime;

    //����һ����hash����
    m_ContHashMutex.lock();

    if(m_ContinuumMG)
    {
        stContinuumMG *pMG = m_ContinuumMG;
        int idx=0;
        while( (idx++) < m_NumPoints
            && pMG->m_Servers)
        {
            FREE(pMG->m_Servers);
            pMG++;
        }

        FREE(m_ContinuumMG);
        m_ContinuumMG = NULL;
    }

    MALLOC(m_ContinuumMG, (sizeof(stContinuumMG) * m_MGVector.size() * 160), stContinuumMG );
    m_NumPoints=0;
    unsigned int i, k, cont = 0;

    for( i = 0; i < m_MGVector.size(); i++ )
    {
        stMGInfo *pMGInfo = m_MGVector[i];
        if(NULL == pMGInfo)
        {
            DCS_LOG(WARN, "DCSCacheManager::CreateContinuum()-m_MGVector[" << i << "] is NULL.");
            continue;
        }

        float pct = (float)pMGInfo->m_CacheSize / (float)m_TotalSize;
        unsigned int ks = (unsigned int) floorf( pct * 40.0 * (float)m_MGVector.size() );

        int hpct = (int) floorf( pct * 100.0 );

        DCS_LOG(DEBUG, "DCSCacheManager::CreateContinuum()-Server no:"
            << i << ",servers:" << pMGInfo->m_Servers
            << ",(mem:" << pMGInfo->m_CacheSize << " = " << hpct << " or " << ks << " of " << m_MGVector.size() * 40 );

        for( k = 0; k < ks; k++ )
        {
            /* 40 hashes, 4 numbers per hash = 160 points per server */
            char ss[30];
            unsigned char digest[16];

            sprintf( ss, "%s-%d", pMGInfo->m_MGName.c_str(), k );
            Md5Digest16( ss, digest );

            /* Use successive 4-bytes from hash as numbers
             * for the points on the circle: */
            int h;
            for( h = 0; h < 4; h++ )
            {
                m_ContinuumMG[m_NumPoints].m_Point = ( digest[3+h*4] << 24 )
                                                      | ( digest[2+h*4] << 16 )
                                                      | ( digest[1+h*4] <<  8 )
                                                      |   digest[h*4];

                MALLOC( m_ContinuumMG[m_NumPoints].m_Servers, (pMGInfo->m_Servers.length()+1), char);
                strcpy(m_ContinuumMG[m_NumPoints].m_Servers, pMGInfo->m_Servers.c_str());
                m_NumPoints++;
            }
        }
    }

    /* Sorts in ascending order of "point" */
    qsort( (void*) m_ContinuumMG, m_NumPoints, sizeof( stContinuumMG ), (compfn) MGCompare );

    //��ӡ
    PrintContinuum();

    m_ContHashMutex.unlock();

    //��վ�����
    vector< stMGInfo * >::iterator it = m_MGVector.begin();
    for(; it != m_MGVector.end(); it++ )
    {
        delete *it;
    }
    m_MGVector.clear();
    m_TotalSize=0;


    DCS_LOG(TRACE, "DCSCacheManager::CreateContinuum() end.");

    return retCode;
}

//����zookeeper
int DCSCacheManager::ConnectZookeeper()
{
    DCS_LOG(TRACE, "DCSCacheManager::ConnectZookeeper() begin.");

    int retCode=0;
    if(m_ZookeeperClient)
    {
        return retCode;
    }

    //����zookeeper
    m_ZookeeperClient = new ZookeeperClient(m_ZkServers, CacheAllWatcher);


    DCS_LOG(TRACE, "DCSCacheManager::ConnectZookeeper() end.");
    return retCode;
}

//��ȡzookeeper��cache all��Ϣ
int DCSCacheManager::GetZkCacheAll(vector< stMGInfo * > &m_MGVector, unsigned long &m_TotalSize)
{
    DCS_LOG(TRACE, "DCSCacheManager::GetZkCacheAll() begin.");
    int retCode=0, ret=0;

    int vectorSize=0;

    if(m_ZkCacheAll.length() > 0
        && NULL != m_ZookeeperClient )
    {
        //1.��ȡ${zk_home}/cacheserver/all
        //�ļ���ʽ:mg0000=123.123.123.123:12345:2000,123.123.123.123:12345:2000|mg0001=123.123.123.123:12345:1500,123.123.123.123:12345:1500|
        char *dataBuf = m_ZKALLDataBuf;
        memset(m_ZKALLDataBuf, 0x0, sizeof(m_ZKALLDataBuf));
        int dataLen = 1024*1024;

        retCode = m_ZookeeperClient->GetNode(m_ZkCacheAll.c_str(), DCSCacheManager::CacheAllWatcher, dataBuf, dataLen);
        if(0 != retCode)
        {
            DCS_LOG(WARN, "MasterServer::GetZkCacheAll() step 1 failed.GetNode:" << m_ZkCacheAll
                << "retCode:" << retCode );
            //delete m_ZookeeperClient;
            //m_ZookeeperClient = NULL;
            return retCode;
        }

        if(dataLen < 1024*1024)
        {
            *(dataBuf+dataLen) = '\0';
        }
        else
        {
            *(dataBuf+1024*1024-1) = '\0';
        }


        //m_ContHashMutex.lock();

        //��վ�����
        vector< stMGInfo * >::iterator it = m_MGVector.begin();
        for(; it != m_MGVector.end(); it++ )
        {
            delete *it;
        }
        m_MGVector.clear();
        m_TotalSize=0;

        //����������
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

            DCS_LOG(INFO, "DCSCacheManager::GetZkCacheAll()-mgUint:" << mgUnit);

            //�����Ⱥ��Ԫ����
            ret=CutStringHead(mgUnit, "=", mgName, mgUnit);
            if(ret != 0)
            {
                //bad format
                DCS_LOG(WARN, "DCSCacheManager::GetZkCacheAll(bad format1)-mgUint:" << mgUnit);
                continue;
            }

            //check and append
            mgContent="";
            bool bQuit=false;
            while(!bQuit)
            {
                ret = CutStringHead(mgUnit, ",", cacheNode, mgUnit);
                //Ԫ�ز�������ǰ����
                if(ret != 0)
                {
                    bQuit=true;
                }

                //
                ret=CutStringHead(cacheNode, ":", cacheIp, cacheNode);
                if(ret != 0)
                {
                    DCS_LOG(WARN, "DCSCacheManager::GetZkCacheAll(bad format2)-cacheNode:" << cacheIp);
                    continue;
                }

                ret=CutStringHead(cacheNode, ":", cachePort, cacheNode);
                if(ret != 0)
                {
                    DCS_LOG(WARN, "DCSCacheManager::GetZkCacheAll(bad format3)-cacheNode:" << cachePort);
                    continue;
                }

                ret=CutStringHead(cacheNode, ":", cacheSize, cacheNode);

                cacheNode = cacheIp + ":" + cachePort;
                DCS_LOG(DEBUG, "DCSCacheManager::GetZkCacheAll()-cacheNode:" << cacheNode);

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
                //bad format
                DCS_LOG(WARN, "DCSCacheManager::GetZkCacheAll(0 == mgContent.length())-mgName:" << mgName);
            }
            else
            {
                //����ȫ�ֱ���
                stMGInfo *pMG = new stMGInfo();
                pMG->m_MGName=mgName;
                pMG->m_Servers=mgContent;
                pMG->m_CacheSize=atoi( cacheSize.c_str() );
                m_MGVector.push_back( pMG );

                m_TotalSize+=pMG->m_CacheSize;
            }
        }  //end of while

        vectorSize = m_MGVector.size();

        //m_ContHashMutex.unlock();

    }   //end of if

    //�ռ��
    if( vectorSize <= 0 )
    {
        DCS_LOG(WARN, "DCSCacheManager::GetZkCacheAll( m_MGVector.size() <= 0 )");
        return -1;
    }

    DCS_LOG(TRACE, "DCSCacheManager::GetZkCacheAll() end.");
    return 0;
}


//zookeeper�ص����${zk_home}/cacheserverĿ¼�仯
void DCSCacheManager::CacheAllWatcher(int type,  const char *path)
{
    DCS_LOG(TRACE, "DCSCacheManager::CacheAllWatcher() begin.");

    DCS_LOG(DEBUG, "DCSCacheManager::CacheAllWatcher(type:" << type);

    //rebuild cache continuum hash
    //����ż���쳣�ᵼ��EMS���������ӿ��ؿ����Ƿ��Զ��ؽ����ܣ�Ĭ���ǹر�״̬
    if(DCSCacheManager::GetInstance()->m_ReBuildFlag)
    {
        DCSCacheManager::GetInstance()->m_NeedReBuild=true;
    }
    //DCSCacheManager::GetInstance()->CreateContinuum();

    DCS_LOG(TRACE, "DCSCacheManager::CacheAllWatcher() end.");
}

//cache�����������߳�
void *DCSCacheManager::ThreadCacheOp( void *pVoid )
{
    DCS_LOG(TRACE, "DCSCacheManager::ThreadCacheOp() begin");

    while(1)
    {
        stCacheOpTask oneTask;

        //������
        DCSCacheManager::GetInstance()->m_CacheOpTaskMutex.lock();
        while(1)
        {
            //��ȡ
            if(!DCSCacheManager::GetInstance()->m_CacheOpTaskQueue.empty())
            {
                oneTask = DCSCacheManager::GetInstance()->m_CacheOpTaskQueue.front();
                DCSCacheManager::GetInstance()->m_CacheOpTaskQueue.pop();

                //ռ���ڴ����
                DCSCacheManager::GetInstance()->m_OpTaskProtectSize -= (sizeof(stCacheOpTask) + oneTask.m_ServersLen+oneTask.m_KeyLen+oneTask.m_ValLen);

                break;
            }

            DCSCacheManager::GetInstance()->m_CacheOpTaskCond->wait();
        }
        DCSCacheManager::GetInstance()->m_CacheOpTaskMutex.unlock();

        //����
        DCSCacheManager::DoCacheOpTask(oneTask);
    }


    return NULL;
}


//��ӡһ����hash��
void DCSCacheManager::PrintContinuum()
{
    DCS_LOG(TRACE, "DCSCacheManager::PrintContinuum() begin.");

    stContinuumMG *pMG=NULL;

    for(int i=0; i < m_NumPoints; i++)
    {
        pMG = m_ContinuumMG + i;
        if(pMG)
        {
            DCS_LOG(DEBUG, "idx:[" << i << "],point:[" << pMG->m_Point << "],servers:" << pMG->m_Servers );
        }
    }

    DCS_LOG(TRACE, "DCSCacheManager::PrintContinuum() end.");

    return;
}



//�ַ���ת����һ����hash��ֵ
unsigned int DCSCacheManager::Str2Hash( const char* inString )
{
    DCS_LOG(TRACE, "DCSCacheManager::Str2Hash() begin.inString:" << inString);
    unsigned int retHash;

    unsigned char digest[16];

    Md5Digest16( inString, digest );
    retHash =  (unsigned int)(( digest[3] << 24 )
                        | ( digest[2] << 16 )
                        | ( digest[1] <<  8 )
                        |   digest[0] );

    DCS_LOG(TRACE, "DCSCacheManager::Str2Hash() end.:retHash:" << retHash);

    return retHash;
}


//md5���� 16λ
void DCSCacheManager::Md5Digest16( const char* inString, unsigned char md5pword[16] )
{
    DCS_LOG(TRACE, "DCSCacheManager::Md5Digest16() begin.inString:" << inString);

    md5_state_t md5state;

    md5_init( &md5state );
    md5_append( &md5state, (unsigned char *)inString, strlen( inString ) );
    md5_finish( &md5state, md5pword );

    DCS_LOG(TRACE, "DCSCacheManager::Md5Digest16() end." );

    return;
}




}

