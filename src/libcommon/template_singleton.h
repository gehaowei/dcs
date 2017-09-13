//------------------------------------------------------------------------------
// ��    ��������ģ�������
// ��    �ߣ����΢
// ��    ע����
//------------------------------------------------------------------------------
#ifndef _TEMPLATE_SINGLETON_H_
#define _TEMPLATE_SINGLETON_H_

#include "mutex.h"

namespace DCS
{

template<typename TSINGLETON>
class TemplateSingleton
{
	static TSINGLETON* instance;
	static Mutex mutex;
public:
	static TSINGLETON* GetInstance()
	{
		if (instance == 0)
		{
			mutex.lock();
			if (instance == 0)
				instance = new TSINGLETON();
			mutex.unlock();
		}
		return instance;
    }

	virtual ~TemplateSingleton()
	{
		mutex.lock();
		if (instance!=0)
		{
			delete instance;
			instance = 0;
		}
		mutex.unlock();
	}
};

template<typename TSINGLETON>
TSINGLETON* TemplateSingleton<TSINGLETON>::instance=0;

template<typename TSINGLETON>
Mutex TemplateSingleton<TSINGLETON>::mutex;

//�����
//ClassName��������
//ʹ�þ�����class ClassName: INHERIT_TSINGLETON(ClassName)
#define INHERIT_TSINGLETON( ClassName )\
 public TemplateSingleton<ClassName>\
{\
private:\
	friend class TemplateSingleton<ClassName>;\
protected:\
	ClassName();\

}

#endif

