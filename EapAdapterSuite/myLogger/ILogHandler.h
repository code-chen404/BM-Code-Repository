#pragma once
#include "myLogCommon.h"
#include <QObject>

namespace myLog
{
    class ILogHandler : public QObject
    {
        Q_OBJECT

    public:
        explicit ILogHandler(QObject* parent = nullptr);
        ~ILogHandler() override;

        virtual void log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite) = 0;

        virtual void log_write(logType log_type, const std::string& smg, const std::string& msg_withfileinfo,
                               logLevel level) = 0;
        virtual void log_write(const std::string& log_type, const std::string& smg, const std::string& msg_withfileinfo,
                               logLevel level) = 0;
        virtual void log_flush() = 0;

        virtual bool contains(logType type) = 0;
        virtual bool contains(const std::string& type) = 0;
        virtual int regist_log(logType type, logConfig l_config) = 0;

        virtual int regist_log(const std::string& type, logConfig l_config) = 0;
    signals:
        void sig_message(logLevel loglevel, logType logtype, const std::string& msg);
        void sig_message2(logLevel loglevel, const std::string& logtype, const std::string& msg);
    };
}
