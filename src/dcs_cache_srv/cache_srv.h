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
    // 函数名称: DoWork
    // 函数功能: 模块初始化并启动工作
    // 输入参数: 透传main参数
    // 输出参数: 无
    // 返回值  : 成功返回0;异常返回负数
    //-------------------------------------------------------------------------------
    int DoWork(int& argc,	char *argv[]);

    //获取配置文件对象指针
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
