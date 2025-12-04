#pragma once
#include "implement.h"
#include "ILogHandler.h"
#include <map>

namespace myLog
{
    using PTR = std::shared_ptr<spdlog::logger>;

    class SpdLogHandler : public ILogHandler
    {
    public:
        explicit SpdLogHandler(QObject* parent = nullptr);
        ~SpdLogHandler() override;
        void log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite) override;
        void log_write(logType log_type, const std::string& smg, const std::string& msg_withfileinfo,
                       logLevel level) override;
        void log_write(const std::string& log_type, const std::string& smg, const std::string& msg_withfileinfo,
                       logLevel level) override;
        void log_flush() override;
        bool contains(logType type) override;
        bool contains(const std::string& type) override;
        int regist_log(logType type, logConfig l_config) override;
        int regist_log(const std::string& type, logConfig l_config) override;

    private:
        void message_distribution(logLevel level, logType type, const std::string& msg);
        void message_distribution(logLevel level, const std::string& type, const std::string& msg);
    private:
        static std::string get_folder_name();

    private:
        std::shared_ptr<spdlog::logger> get_logger(logType logtype);
        std::shared_ptr<spdlog::logger> get_logger(const std::string& logtype);
    private:
        std::map<logType, PTR> logger_map;
        std::map<std::string, PTR> logger_map2;
        std::string log_dir;
        size_t log_maxsize;
        int log_maxfile;
        bool m_Init = false;

        std::string template_filename;
    };
}
