#include "mutex.h"

namespace DCS
{

int Mutex::lock()
{
	return pthread_mutex_lock( &m_mutex );
}

int Mutex::unlock()
{
	return pthread_mutex_unlock( &m_mutex );
}

int Mutex::acquire()
{
	return pthread_mutex_lock( &m_mutex );
}

int Mutex::release()
{
	return pthread_mutex_unlock( &m_mutex );
}

pthread_mutex_t &Mutex::getMutex()
{
	return m_mutex;
}

pthread_rwlock_t &RWMutex::getMutex()
{
	return m_mutex;
}

RWMutex::RWMutex()
{
	pthread_rwlock_init( &m_mutex, NULL );
}

RWMutex::~RWMutex()
{
	pthread_rwlock_destroy( &m_mutex );
}

int RWMutex::rlock()
{
	return pthread_rwlock_rdlock( &m_mutex );
}

int RWMutex::wlock()
{
	return pthread_rwlock_wrlock( &m_mutex );
}

int RWMutex::unlock()
{
	return pthread_rwlock_unlock( &m_mutex );
}

}

