#ifndef __COND_H__
#define __COND_H__

#include "mutex.h"
#include <pthread.h>
#include <iostream>

namespace DCS
{

class Cond
{
	private:
	    Mutex *m_mutex;
		pthread_cond_t m_cond;

    public:
		Cond( Mutex *mutex )
		{
			m_mutex = mutex;
			pthread_cond_init( &m_cond, NULL );
		}

		~Cond( void )
		{
			pthread_cond_destroy( &m_cond );
		}

		int wait( unsigned int timeout = 1 );
		int signal( void );
		int broadcast( void );
        int waitTill();
};

}

#endif
