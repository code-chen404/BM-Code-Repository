#pragma once
#include <log4cplus/logger.h>
#include <log4cplus/configurator.h>
#include <log4cplus/loggingmacros.h>
#include <log4cplus/helpers/stringhelper.h>
#include <log4cplus/fileappender.h>
#include <log4cplus/layout.h>
#include <log4cplus/loglevel.h>

#include "ILogHandler.h"

namespace myLog
{

    class Log4cplusHandler : public ILogHandler
    {
    public:
        explicit Log4cplusHandler(QObject* parent = nullptr);
        ~Log4cplusHandler() override;
        void log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite) override;
        void log_write(logType log_type, const std::string& smg, const std::string& msg_withfileinfo,
            logLevel level) override;
        void log_flush() override;
        bool contains(logType type) override;
        int regist_log(logType type, logConfig l_config) override;

    private:
        log4cplus::Logger get_logger(logType logtype);
    private:
        void message_distribution(logLevel level, logType type, const std::string& msg);
    private:
        static std::string get_folder_name();
  
    private:
        std::map<logType, log4cplus::Logger> logger_map;

        std::string log_dir;
        size_t log_maxsize;
        int log_maxfile;
        bool m_Init = false;

        std::string template_filename;
    };
}
// 实现 Log4cplusHandler 的成员函数
