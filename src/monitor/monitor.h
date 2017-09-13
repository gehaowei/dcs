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
	int pid; //���̺�
	char home[ PATH_NAME_LEN + 1 ]; //����λ��
	char program[ PROGRAM_NAME_LEN + 1 ]; //��������
	char command[ ORDER_NAME_LEN + 1 ]; //��������
	struct st_program *next;
} program_t;

class Monitor
{
	private:
	    FileConfIni *m_conf; //���������ļ�
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

        int init();   //���������ļ�������Ѿ����ڵĽ���ID
		int detect(); //��������߳�
		int detectStop(); //��������߳�
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
