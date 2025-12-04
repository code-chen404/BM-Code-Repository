#include "Log4cplusHandler.h"
using namespace myLog;


Log4cplusHandler::Log4cplusHandler(QObject* parent)
{
}

Log4cplusHandler::~Log4cplusHandler()
{
}

void Log4cplusHandler::log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite)
{
    log_dir = dir;
    log_maxsize = max_size;
    log_maxfile = max_file;

    log4cplus::initialize();
    log4cplus::tstring logFilePath = LOG4CPLUS_STRING_TO_TSTRING(log_dir + "/log.txt");

    log4cplus::SharedAppenderPtr appender(new log4cplus::RollingFileAppender(
        logFilePath, log_maxsize, log_maxfile, overwrite));

    log4cplus::tstring pattern = LOG4CPLUS_TEXT("%D{%Y-%m-%d %H:%M:%S.%q} [%p] %m%n");
    appender->setLayout(std::unique_ptr<log4cplus::Layout>(new log4cplus::PatternLayout(pattern)));

    log4cplus::Logger root = log4cplus::Logger::getRoot();
    root.addAppender(appender);
    root.setLogLevel(log4cplus::ALL_LOG_LEVEL);

    m_Init = true;
}

void Log4cplusHandler::log_write(logType log_type, const std::string& smg, const std::string& msg_withfileinfo,
    logLevel level)
{
    if (!m_Init)
        return;

    log4cplus::Logger logger = get_logger(log_type);

    switch (level)
    {
    case logLevel::lv_debug:
        LOG4CPLUS_DEBUG(logger, LOG4CPLUS_STRING_TO_TSTRING(msg_withfileinfo));
        break;
    case logLevel::lv_info:
        LOG4CPLUS_INFO(logger, LOG4CPLUS_STRING_TO_TSTRING(msg_withfileinfo));
        break;
    case logLevel::lv_warn:
        LOG4CPLUS_WARN(logger, LOG4CPLUS_STRING_TO_TSTRING(msg_withfileinfo));
        break;
    case logLevel::lv_error:
        LOG4CPLUS_ERROR(logger, LOG4CPLUS_STRING_TO_TSTRING(msg_withfileinfo));
        break;
    case logLevel::lv_critical:
        LOG4CPLUS_FATAL(logger, LOG4CPLUS_STRING_TO_TSTRING(msg_withfileinfo));
        break;
    default:
        break;
    }
}

void Log4cplusHandler::log_flush()
{
    if (!m_Init)
        return;

    log4cplus::Logger logger = log4cplus::Logger::getRoot();
    logger.removeAllAppenders();
}

bool Log4cplusHandler::contains(logType type)
{
    return logger_map.contains(type);
}

int Log4cplusHandler::regist_log(logType type, logConfig l_config)
{
    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_STRING_TO_TSTRING(logtype_to_string(type)));
    logger_map[type] = logger;
    return 0;
}

log4cplus::Logger Log4cplusHandler::get_logger(logType logtype)
{
    auto it = logger_map.find(logtype);
    if (it != logger_map.end())
        return it->second;

    log4cplus::Logger logger = log4cplus::Logger::getInstance(LOG4CPLUS_STRING_TO_TSTRING(logtype_to_string(logtype)));
    logger_map[logtype] = logger;
    return logger;
}

void Log4cplusHandler::message_distribution(logLevel level, logType type, const std::string& msg)
{

}

std::string Log4cplusHandler::get_folder_name()
{
    return std::string();
}




