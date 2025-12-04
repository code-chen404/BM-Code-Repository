#pragma once

// Boost.Log

#include <boost/log/core.hpp>
#include <boost/log/trivial.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include "ILogHandler.h"

namespace myLog
{


    class BoostLogHandler : public myLog::ILogHandler
    {
    public:
        explicit BoostLogHandler(QObject* parent = nullptr);
        ~BoostLogHandler() override;
        void log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite) override;
        void log_write(logType log_type, const std::string& smg, const std::string& msg_withfileinfo,
            logLevel level) override;
        void log_flush() override;
        bool contains(logType type) override;
        int regist_log(logType type, logConfig l_config) override;

    private:
        void configure_logger(logType logtype);
    private:
        boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>& get_logger(logType logtype);
    private:
        void message_distribution(logLevel level, logType type, const std::string& msg);
    private:
        static std::string get_folder_name();
    private:
        std::map<logType, boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>> logger_map;

        std::string log_dir;
        size_t log_maxsize;
        int log_maxfile;
        bool m_Init = false;

        std::string template_filename;
    };
}
// 实现 BoostLogHandler 的成员函数
