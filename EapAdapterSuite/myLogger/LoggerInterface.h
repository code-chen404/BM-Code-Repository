#pragma once
#include  "myLogger.h"
#include  "fmt/format.h"
#include "myLogCommon.h"


#define REGIST_LOG_TYPE(type,filename) \
    do { \
     myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->regist_log(type,filename);\
     }while(0)


#define LOG_RUNNING_DEBUG(...) myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(myLog::logType::logtype_operation,myLog::logLevel::lv_debug,fmt::format(__VA_ARGS__),fmt::format("[{}] - [{}:{} ]", fmt::format(__VA_ARGS__),__FILE__,__LINE__))
#define LOG_RUNNING_INFO(...) myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(myLog::logType::logtype_operation,myLog::logLevel::lv_info,fmt::format(__VA_ARGS__),fmt::format("[{}] - [{}:{} ]", fmt::format(__VA_ARGS__),__FILE__,__LINE__))
#define LOG_RUNNING_ERROR(...) myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(myLog::logType::logtype_operation,myLog::logLevel::lv_error,fmt::format(__VA_ARGS__),fmt::format("[{}] - [{}:{} ]", fmt::format(__VA_ARGS__),__FILE__,__LINE__) )

#define LOG_TYPE_DEBUG(type,...)  myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type,myLog::logLevel::lv_debug,fmt::format(__VA_ARGS__),fmt::format("[{}] - [{}:{} ]", fmt::format(__VA_ARGS__),__FILE__,__LINE__))
#define LOG_TYPE_INFO(type,...) myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type,myLog::logLevel::lv_info,fmt::format(__VA_ARGS__),fmt::format("[{}] - [{}:{} ]", fmt::format(__VA_ARGS__),__FILE__,__LINE__))
#define LOG_TYPE_ERROR(type,...)  myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type,myLog::logLevel::lv_error,fmt::format(__VA_ARGS__),fmt::format("[{}] - [{}:{} ]", fmt::format(__VA_ARGS__),__FILE__,__LINE__))

#define LOG_TYPE_DEBUG_WITHOUT_FILEINFO(type,...)  myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type,myLog::logLevel::lv_debug,fmt::format(__VA_ARGS__),fmt::format("[{}]", fmt::format(__VA_ARGS__)))
#define LOG_TYPE_INFO_WITHOUT_FILEINFO(type,...) myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type,myLog::logLevel::lv_info,fmt::format(__VA_ARGS__),fmt::format("[{}]", fmt::format(__VA_ARGS__)))
#define LOG_TYPE_ERROR_WITHOUT_FILEINFO(type,...)  myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type,myLog::logLevel::lv_error,fmt::format(__VA_ARGS__),fmt::format("[{}]", fmt::format(__VA_ARGS__)))

#define LOG_STL_DEBUG(type, format, ...) \
    do { \
        size_t size = snprintf(nullptr, 0, format, __VA_ARGS__); \
        char* buffer = new char[size + 1]; \
        snprintf(buffer, size + 1, format, __VA_ARGS__); \
        std::string logMessage = std::string("[") + buffer + "] - [" + __FILE__ + ":" + std::to_string(__LINE__) + "]"; \
        myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type, myLog::logLevel::lv_debug, buffer, logMessage.c_str()); \
        delete[] buffer; \
    } while(0)


#define LOG_STL_INFO(type, format, ...) \
    do { \
        size_t size = snprintf(nullptr, 0, format, __VA_ARGS__); \
        char* buffer = new char[size + 1]; \
        snprintf(buffer, size + 1, format, __VA_ARGS__); \
        std::string logMessage = std::string("[") + buffer + "] - [" + __FILE__ + ":" + std::to_string(__LINE__) + "]"; \
        myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type, myLog::logLevel::lv_info, buffer, logMessage.c_str()); \
        delete[] buffer; \
    } while(0)


#define LOG_STL_ERROR(type, format, ...) \
    do { \
        size_t size = snprintf(nullptr, 0, format, __VA_ARGS__); \
        char* buffer = new char[size + 1]; \
        snprintf(buffer, size + 1, format, __VA_ARGS__); \
        std::string logMessage = std::string("[") + buffer + "] - [" + __FILE__ + ":" + std::to_string(__LINE__) + "]"; \
        myLog::myLogger::get_instance(myLog::log_source_type::log_spd)->mylog_write(type, myLog::logLevel::lv_error, buffer, logMessage.c_str()); \
        delete[] buffer; \
    } while(0)
