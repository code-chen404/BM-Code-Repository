#pragma once
#include "implement.h"
#include "ILogHandler.h"
#include <map>

namespace myLog
{
    using QLOG_PTR = quill::Logger*;
    class QUtillLogHandler : public ILogHandler
    {
    public:
        explicit  QUtillLogHandler(QObject* parent = nullptr);
        ~QUtillLogHandler() override;
        void log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite) override;
        void log_write(logType log_type, const std::string& smg, const std::string& msg_withfileinfo,
            logLevel level) override;
        void log_flush() override;
        bool contains(logType type) override;
        int regist_log(logType type, logConfig l_config) override;

    private:
        void message_distribution(logLevel level, logType type, const std::string& msg);
    private:
        static std::string get_folder_name();
    private:
        QLOG_PTR  get_logger(logType logtype);
    private:
        std::map<logType, QLOG_PTR> logger_map;

        std::string log_dir;
     
        bool m_Init = false;

        std::string template_filename;
    public:
        int log_maxsize;
        int log_maxfile;
        bool m_overwrite = true;
    };
}