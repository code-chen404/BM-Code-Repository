#include "myLogger.h"
#include "SpdLogHandler.h"

using namespace myLog;

#define CHECK(Handler) \
    do { \
        if (!(Handler)) { \
            return; \
        } \
    } while (0)

#define CHECK_BOOL(Handler, arg) \
    do { \
        if (!(Handler)) { \
            return false; \
        } \
        return (Handler)->contains(arg); \
    } while (0)


#define CHECK_BOOL2(Handler, arg) \
    do { \
        if (!(Handler) && (Handler)->contains(arg)) { \
            return false; \
        } \
    } while (0)


myLogger::myLogger(QObject* parent, log_source_type log_type)
{
    /*   switch (log_type)
       {
       case log_source_type::log_spd:
           {
               handler = new SpdLogHandler();
           }
           break;
       case log_source_type::log_boostlog:
           {
           handler = new BoostLogHandler();
           }
           break;
       }*/
    handler = new SpdLogHandler();
}

myLogger::~myLogger()
{
    if (handler)
        delete handler;
}

myLogger* myLogger::get_instance(log_source_type log_type)
{
    static myLogger ins(nullptr, log_type);
    return &ins;
}


void myLogger::mylog_write(logType type, logLevel level, const std::string& msg, const std::string& msg_with_file_info)
{
    CHECK(handler);
    handler->log_write(type, msg, msg_with_file_info, level);
}

void myLogger::mylog_write(const std::string& type, logLevel level, const std::string& msg,
    const std::string& msg_with_file_info)
{
    CHECK(handler);
    handler->log_write(type, msg, msg_with_file_info, level);
}


bool myLogger::contains(logType type)
{
    CHECK_BOOL(handler, type);
    return handler->contains(type);
}

bool myLogger::regist_log(logType type, std::string filename)
{
    CHECK_BOOL2(handler, type);
    logConfig lconfig;
    lconfig.log_filename = filename;
    lconfig.log_name = filename;

    handler->regist_log(type, lconfig);
    return true;
}

bool myLogger::regist_log(const std::string& type, std::string filename)
{
    CHECK_BOOL2(handler, type);
    logConfig lconfig;
    lconfig.log_filename = filename;
    lconfig.log_name = filename;

    handler->regist_log(type, lconfig);
    return true;
}

