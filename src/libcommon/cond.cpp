#include "cond.h"

namespace DCS
{

int Cond::wait( unsigned int timeout )
{
	struct timespec to;
	to.tv_sec = time( NULL ) + timeout;
	to.tv_nsec = 0;
	return pthread_cond_timedwait( &m_cond, &( m_mutex->getMutex() ), &to );
}

int Cond::signal()
{
	return pthread_cond_signal( &m_cond );
}

int Cond::waitTill()
{
	return pthread_cond_wait(&m_cond, &( m_mutex->getMutex() ));
}

int Cond::broadcast()
{
	return pthread_cond_broadcast( &m_cond );
}

}

