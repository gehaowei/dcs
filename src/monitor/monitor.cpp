#include "monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <string>
#include "common.h"
#include <errno.h>

int Monitor::startupProgram( program_t *prog )
{
	if( chdir( prog->home ) != 0 )
	{
		std::cout << "change path fail:" << prog->home << ".errno:" << errno << std::endl;
	}
	system( prog->command );
	sleep( 3 );
	prog->pid = this->getPID( prog->program );

    std::cout << "Monitor::startupProgram-pid:" << prog->pid << ",command:" << prog->command << std::endl;

	if( prog->pid < 0 )
	{
		std::cout << "start " << prog->program << " error" << std::endl;
	}
	return 0;
}

int Monitor::init()
{
    std::string confFile=getenv("HOME");
    confFile+=CONF_PATH;
	m_conf = new FileConfIni( confFile );
	m_conf->load();

	char *list = m_conf->getData( "LIST", "list" );
	if( list == NULL || strlen( list ) == 0 )
	{
		//std::cout << "list is empty" << std::endl;
		return -1;
	}
	//std::cout << "list=" << list << std::endl;

	char *pos = list;
	char *begin = list;
	char prog[ 64 ] = { 0 };
	int pid = 0;
	program_t *tmp = NULL;
	while( 1 )
	{
		pos = strchr( pos, '|' );
		if( pos == NULL && begin != NULL )
		{ //最后一个
			pid = this->getPID( m_conf->getData( begin, "program" ) );
            //std::cout << "Monitor::init-pid:" << pid << ",prog:" << begin << std::endl;
			tmp = add( begin );
			tmp->pid = pid;
			if( pid >= 0 )
			{ //说明程序已经启动
				//std::cout << "pid=" << pid << std::endl;
			}
			else
			{ //说明程序没有启动，启动程序
			    this->startupProgram( tmp );
			}
			break;
		}
		else
		{
		    memset(prog, 0x0, sizeof(prog));
			strncpy( prog, begin, pos - begin );
			pid = this->getPID( m_conf->getData( prog, "program" ) );
            //std::cout << "Monitor::init-pid:" << pid << ",prog:" << prog << std::endl;
			tmp = add( prog );
			tmp->pid = pid;
			if( pid >= 0 )
			{ //说明程序已经启动
				//std::cout << "pid=" << pid << std::endl;
			}
			else
			{ //说明程序没有启动
			    this->startupProgram( tmp );
			}
			pos += 1;
			begin = pos;
		}
	}

	return 0;
}

program_t *Monitor::add( const char *name )
{
	if( name == NULL || strlen( name ) == 0 )
	{
		return NULL;
	}

	program_t *tmp = NULL;
	MALLOC( tmp, sizeof( program_t ), program_t );

	snprintf(tmp->home, sizeof( tmp->home ) - 1, "%s/%s", getenv("HOME"), m_conf->getData( name, "home" ) );

	strncpy( tmp->program, m_conf->getData( name, "program" ), sizeof( tmp->program ) - 1 );
	strncpy( tmp->command, m_conf->getData( name, "command" ), sizeof( tmp->command ) - 1 );

	if( m_proglist != NULL )
	{
		tmp->next = m_proglist;
	}
	m_proglist = tmp;

	return tmp;
}

int Monitor::detectStop()
{
	program_t *tmp = m_proglist;
	int ret = 0;

	while( tmp != NULL )
	{
		if( tmp->pid >= 0 )
		{
			ret = ::kill( tmp->pid, 9 );
			std::cout << "ret:" << ret << std::endl;
			if( ret == -1 )
			{
				switch( errno )
				{
					case EINVAL:
						std::cout << "signal is invalid" << std::endl;
						break;
					case ESRCH:
						//std::cout << "progress is not exit" << std::endl;
						//this->startupProgram( tmp );
						break;
					case EPERM:
						std::cout << "not have permission to send the signal" << std::endl;
						break;
				}
			}
			else
			{
				//std::cout << tmp->program << " alive" << std::endl;
			}
		}
		tmp = tmp->next;
	}

	return 0;
}

int Monitor::detect()
{ //检测已经启动的进程是否存在
	program_t *tmp = NULL;
	int ret = 0;

	while( 1 )
	{
		tmp = m_proglist;

		while( tmp != NULL )
		{
			if( tmp->pid >= 0 )
			{
				ret = ::kill( tmp->pid, 0 );
				if( ret == -1 )
				{
					switch( errno )
					{
						case EINVAL:
							std::cout << "signal is invalid" << std::endl;
							break;
						case ESRCH:
							//std::cout << "progress is not exit" << std::endl;
							this->startupProgram( tmp );
							break;
						case EPERM:
							std::cout << "not have permission to send the signal" << std::endl;
							break;
					}
				}
				else
				{
					//std::cout << tmp->program << " alive" << std::endl;
				}
			}
			else
			{
				this->startupProgram( tmp );
			}
			tmp = tmp->next;
		}

		sleep( 2 );
	}

	return 0;
}

int Monitor::getMonitorPPID()
{
	FILE *fp = NULL;
	char data[ 100 ] = { 0 };

	//char cmd[] = "ps -eo ppid,pid,cmd | grep -v grep | grep monitor | awk -F' ' '{if( $1==1) print $2}'";
	char cmd[128]= { 0 };
	sprintf(cmd, "ps -u %s -o ppid,pid,cmd |grep -v grep | grep %s |awk -F' ' '{if( $1==1) print $2}'",
	    m_user.c_str(), m_me.c_str() );

    //std::cout << "Monitor::getMonitorPPID()cmd:" << cmd << std::endl;

	if( ( fp = popen( cmd, "r" ) ) == NULL )
	{
		std::cout << "popen error" << std::endl;
		return -1;
	}

	if( fgets( data, sizeof( data ) - 1, fp ) == NULL )
	{
		//std::cout << "fgets error" << std::endl;
		pclose( fp );
		return -1;
	}

	pclose( fp );

    //std::cout << "Monitor::getMonitorPPID()data:" << data << std::endl;

	return atoi( data );
}

int Monitor::usage()
{
	std::cout << "Usage: monitor [OPTION]" << std::endl;
	std::cout << "manage monitor daemon and these programs" << std::endl;
	std::cout << "that was controlled by it" << std::endl;
	std::cout << "OPTION means:" << std::endl;
	std::cout << "  start: to start monitor" << std::endl;
	std::cout << "  stop:  to stop monitor only" << std::endl;
	std::cout << "  kill:  to stop monitor and all programs" << std::endl;
	std::cout << "         that was controlled by it" << std::endl;

	return 0;
}

int Monitor::checkSelf()
{
	FILE *fp = NULL;
	char data[ 100 ] = { 0 };
	char cmd[128]= { 0 };
	sprintf(cmd, "ps -u %s -o pid,cmd |grep -v grep | grep %s | wc -l",
	    m_user.c_str(), m_me.c_str() );

	if( ( fp = popen( cmd, "r" ) ) == NULL )
	{
		std::cout << "popen error" << std::endl;
		return -1;
	}

	if( fgets( data, sizeof( data ) - 1, fp ) == NULL )
	{
		//std::cout << "fgets error" << std::endl;
		pclose( fp );
		return -1;
	}
	pclose( fp );

	return atoi( data );
}

int Monitor::getPID( const char *name )
{
	FILE *fp = NULL;
	char sData[ 256 ] = { 0 };
	//char cmd[ 256 ] = { 0 };
	//snprintf( cmd, sizeof( cmd ) - 1, "ps -ef | grep '%s' | awk -F' ' '{ print $2, substr( $0, index( $0, $8 ) ); }'", name );

	char cmd[512]= { 0 };
	snprintf(cmd, sizeof( cmd ) - 1, "ps -u %s -o pid,cmd |grep -v grep | grep '%s'",
	    m_user.c_str(), name );

    //std::cout << "Monitor::getPID()cmd:" << cmd << std::endl;

	if( ( fp = popen( cmd, "r" ) ) == NULL )
	{
		std::cout << "popen error" << std::endl;
		return -1;
	}

	while( 1 )
	{
	    char *data=sData;
	    memset(data, 0x0, sizeof(sData));
		if( fgets( data, sizeof( sData ) - 1, fp ) == NULL )
		{
			//std::cout << "fgets error" << std::endl;
			pclose( fp );
			return -1;
		}

        //兼容异常情况
        while( ' ' == (*data) && '\0' != (*data) )
        {
            data++;
        }

        //std::cout << "Monitor::getPID()data:" << data << std::endl;

		if( strstr( data, name ) != NULL )
		{
			char *pos = strchr( data, ' ' );
			if( pos != NULL )
			{
				*pos = '\0';
				if( ( *( pos + 1 ) == '.' && *( pos + 2 ) == '/' && strncmp( pos + 3, name, strlen( name ) ) == 0 ) ||
						strncmp( pos + 1, name, strlen( name ) ) == 0 )
				{
					pclose( fp );
					return atoi( data );
				}
			}
			else
			{
				pclose( fp );
				return -1;
			}
		}
	}

	pclose( fp );
	return -1;
}

int main( int argc, char **argv )
{
	bool flag = true;

	Monitor monitor(argv[0]);
	if( argc == 1 || strcmp( argv[ 1 ], "start" ) == 0 )
	{
		if( monitor.checkSelf() >= 2 ) //说明已经启动
		{
			std::cout << "monitor was started already" << std::endl;
		}
		else
		{
			if( fork() != 0 )
			{
				exit( 0 );
			}
			setsid();
			if( fork() != 0 )
			{
				exit( 0 );
			}
			std::cout << "monitor is started" << std::endl;
			monitor.init();
			monitor.detect();
		}
		return 0;
	}
	else if( argc >= 1 )
	{
		if( strcmp( argv[ 1 ], "stop" ) == 0 )
		{ //只是停止monitor
			int ppid = monitor.getMonitorPPID();
			//std::cout << "main-stop-ppid:" << ppid << std::endl;
			if( ppid > 0 )
			{ //说明存在monitor服务
				char cmd[ 50 ] = { 0 };
				snprintf( cmd, sizeof( cmd ) - 1, "kill -9 %d", ppid );
				system( cmd );
				std::cout << "monitor is stopped" << std::endl;
			}
			else
			{
				std::cout << "monitor was not started" << std::endl;
			}
			//std::cout << monitor.getMonitorPPID() << std::endl;
			return 0;
		}
		else if( strcmp( argv[ 1 ], "kill" ) == 0 )
		{ //停止所有程序
			int ppid = monitor.getMonitorPPID();
			//std::cout << "main-kill-ppid:" << ppid << std::endl;
			if( ppid > 0 )
			{ //说明存在monitor服务
				char cmd[ 50 ] = { 0 };
				snprintf( cmd, sizeof( cmd ) - 1, "kill -9 %d", ppid );
				system( cmd );
				std::cout << "monitor is killed" << std::endl;
			    monitor.init();
				monitor.detectStop();
			}
			else
			{
				std::cout << "monitor was not started" << std::endl;
			}
			return 0;
		}
	}
	monitor.usage();
	return 0;
}
