#ifndef __CACHE_SRV_H__
#define __CACHE_SRV_H__

#include "file_conf.h"
#include <sys/types.h>
#include <signal.h>
#include <string>
using namespace std;


namespace DCS {

class CacheServer:
        INHERIT_TSINGLETON( CacheServer )
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
    ~CacheServer();

private:

    FileConfIni *m_Conf;

};

}



#endif
