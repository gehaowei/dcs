#ifndef __MONITOR_H__
#define __MONITOR_H__

#include "file_conf.h"
#include <sys/types.h>
#include <signal.h>
#include <string>
using namespace std;
using namespace DCS;

#define CONF_PATH "/etc/monitor.conf"
#define PROGRAM_NAME_LEN 256
#define ORDER_NAME_LEN 256
#define PATH_NAME_LEN 256

typedef struct st_program
{
	int pid; //进程号
	char home[ PATH_NAME_LEN + 1 ]; //命令位置
	char program[ PROGRAM_NAME_LEN + 1 ]; //进程名称
	char command[ ORDER_NAME_LEN + 1 ]; //启动命令
	struct st_program *next;
} program_t;

class Monitor
{
	private:
	    FileConfIni *m_conf; //加载配置文件
	    program_t *m_proglist;
        string m_user;
        string m_me;

	public:
	    Monitor(const char *me): m_conf( NULL ), m_proglist( NULL ){

            m_me = me;
            m_user = getenv("USER");

         }
	    ~Monitor()
	    {
            if( m_conf )
            {
	    	    delete m_conf;
            }
		}

        int init();   //加载配置文件并检测已经存在的进程ID
		int detect(); //检测启动线程
		int detectStop(); //检测启动线程
		program_t *add( const char *name );
		int startupProgram( program_t *prog );
		int del();
		int getPID( const char *name );

        int getMonitorPPID();
        int checkSelf();
		int start();
		int stop();
		int kill();
		int usage();
};

#endif
