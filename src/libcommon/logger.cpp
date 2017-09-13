
#include "logger.h"

namespace DCS
{

//单实例
DCSLogger::DCSLogger()
{
    m_InitFlag=false;
}

void DCSLogger::init(const string &filePathName, const string  &moduleName )
{
    //只需要调用一次
    if(m_InitFlag)
    {
        return;
    }

    m_InitFlag=true;

    /* step 1: Instantiate a FileAppender object */
    SharedAppenderPtr append_1(
        new DailyRollingFileAppender( LOG4CPLUS_TEXT(filePathName), ( log4cplus::DailyRollingFileSchedule )DAILY, true, 9 ) );

   /* step 2: Instantiate a layout object */
    std::string pattern = "<%c:%p>:%D{%Y-%m-%d %H:%M:%S,%q} desc:'%m',tid:%t[%l]%n";
    append_1->setLayout( std::auto_ptr<Layout>(new PatternLayout(LOG4CPLUS_TEXT(pattern))) );

    /* step 3: Instantiate a logger object */
    //Logger::getRoot().addAppender(append_1);
    m_Logger = Logger::getInstance(LOG4CPLUS_TEXT(moduleName));
    m_Logger.addAppender(append_1);
}

//TRACE,DEBUG,INFO,WARN,ERROR,FATAL
void DCSLogger::setLogLevel(const string & level)
{
    if("TRACE" == level)
    {
        setLogLevel( TRACE_LOG_LEVEL );
    }

    if("DEBUG" == level)
    {
        setLogLevel( DEBUG_LOG_LEVEL );
    }

    if("INFO" == level)
    {
        setLogLevel( INFO_LOG_LEVEL );
    }

    if("WARN" == level)
    {
        setLogLevel( WARN_LOG_LEVEL );
    }

    if("ERROR" == level)
    {
        setLogLevel( ERROR_LOG_LEVEL );
    }

    if("FATAL" == level)
    {
        setLogLevel( FATAL_LOG_LEVEL );
    }
}


void DCSLogger::setLogLevel(const log4cplus::LogLevel level)
{
    if(m_InitFlag)
    {
        m_Logger.setLogLevel(level);
    }
}

}

