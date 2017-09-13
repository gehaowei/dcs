#ifndef _LOGGER_H_
#define _LOGGER_H_

#include <log4cplus/logger.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/ndc.h>
#include <log4cplus/helpers/loglog.h>

#include <string>
#include "template_singleton.h"

using namespace std;
using namespace log4cplus;

namespace DCS
{

#define DCS_LOG_INIT( filePathName, moduleName ) \
    DCSLogger::GetInstance()->init(filePathName, moduleName);

//level:TRACE,DEBUG,INFO,WARN,ERROR,FATAL
//logevent:  "cnt" << cnt << "."
//example: DCS_LOG(INFO, "this is a test,sock=" << sock << ",name:" << name );
#define DCS_LOG( level, logevent ) \
    do \
	{ \
        if(DCSLogger::GetInstance()->isReady())\
        {\
    		NDCContextCreator ndc( LOG4CPLUS_TEXT("DCS") ); \
    		LOG4CPLUS_##level( DCSLogger::GetInstance()->getLogger(), logevent ); \
        }\
	}while( 0 )


//level:TRACE<DEBUG<INFO<WARN<ERROR<FATAL
#define DCS_SET_LOG_LEVEL( level ) \
    DCSLogger::GetInstance()->setLogLevel( level##_LOG_LEVEL );


//////////////////////////////////////////////////////////////////////////////
//logger wrapper of log4cplus
class DCSLogger:
    INHERIT_TSINGLETON( DCSLogger )
public:
    //µ¥ÊµÀý
    ~DCSLogger(){}

    void init(const string &filePathName, const string  &moduleName );

    //TRACE,DEBUG,INFO,WARN,ERROR,FATAL
    void setLogLevel(const string & level);

    void setLogLevel(const log4cplus::LogLevel level);

    Logger &getLogger(){ return m_Logger; }

    bool isReady(){ return m_InitFlag; }

private:
    Logger m_Logger;
    bool m_InitFlag;
};

}

#endif

