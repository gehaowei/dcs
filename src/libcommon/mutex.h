#ifndef __MUTEX_H__
#define __MUTEX_H__

#include <pthread.h>

namespace DCS
{

class Mutex
{
	private:
		pthread_mutex_t m_mutex;

    public:
		Mutex()
		{
			pthread_mutex_init( &m_mutex, NULL ); //¶¯Ì¬³õÊ¼»¯
		}
		~Mutex()
		{
			pthread_mutex_destroy( &m_mutex );
		}

		int lock();
		int unlock();
        int acquire();
        int release();
		pthread_mutex_t &getMutex();
};

class RWMutex
{
	private:
	    pthread_rwlock_t m_mutex;

	public:
	    RWMutex();
	    ~RWMutex();

	    int rlock();
	    int wlock();
	    int unlock();
	    pthread_rwlock_t &getMutex();
};

}

#endif
