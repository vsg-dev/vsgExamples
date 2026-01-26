//
// Created on 2026/1/14.
//

#ifndef _HARMONYLOGGER_H
#define _HARMONYLOGGER_H
#include <vsg/io/Logger.h>  
#include <hilog/log.h>  

#undef LOG_DOMAIN
#undef LOG_TAG
#define LOG_DOMAIN 0x3200          // Business domain(0x0000-0xFFFF)
#define LOG_TAG    "MY_VSG"     // Module name, not exceeding 15 bytes
class HarmonyLogger : public vsg::Inherit<vsg::Logger, HarmonyLogger>  
{  
public:  
    HarmonyLogger() = default;
    static void  Init(vsg::Logger::Level  level)
    {
        vsg::Logger::instance() = HarmonyLogger::create();
        auto logger = vsg::Logger::instance();
        logger->level = level;
        switch (level)
        {
        case vsg::Logger::LOGGER_DEBUG:
            OH_LOG_SetMinLogLevel(LOG_DEBUG);
            break;
        case vsg::Logger::LOGGER_INFO:
            OH_LOG_SetMinLogLevel(LOG_INFO);
            break;
        case vsg::Logger::LOGGER_WARN:
            OH_LOG_SetMinLogLevel(LOG_WARN);
            break;
        case vsg::Logger::LOGGER_ERROR:
            OH_LOG_SetMinLogLevel(LOG_ERROR);
            break;
        case vsg::Logger::LOGGER_FATAL:
            OH_LOG_SetMinLogLevel(LOG_FATAL);
            break;
        case vsg::Logger::LOGGER_ALL:
            break;
        case vsg::Logger::LOGGER_OFF:
            break;
        }
    }

protected:  
    void debug_implementation(const std::string_view& message) override  
    {  
        OH_LOG_DEBUG(LOG_APP, "%{public}s", message.data());
    }  
      
    void info_implementation(const std::string_view& message) override  
    {  
         OH_LOG_INFO( LOG_APP, "%{public}s",  message.data());
    }  
      
    void warn_implementation(const std::string_view& message) override  
    {  
          OH_LOG_WARN(LOG_APP, "%{public}s",  message.data()); 
    }  
      
    void error_implementation(const std::string_view& message) override  
    {  
          OH_LOG_ERROR( LOG_APP, "%{public}s",  message.data());
    }  
      
    void fatal_implementation(const std::string_view& message) override  
    {  
         OH_LOG_FATAL( LOG_APP, "%{public}s",  message.data());
    }  
};
#endif //_HARMONYLOGGER_H
