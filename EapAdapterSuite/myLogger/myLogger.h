#pragma once

#include <QObject>
#include "mylogger_global.h"
#include "myLogCommon.h"

namespace myLog
{
    class ILogHandler;

    class MYLOGGER_EXPORT myLogger : public QObject
    {
        Q_OBJECT

    protected:
        explicit myLogger(QObject* parent = nullptr, log_source_type log_type = log_source_type::log_spd);

    public:
        ~myLogger() override;
        static myLogger* get_instance(log_source_type log_type = log_source_type::log_spd);

        void mylog_write(logType type, logLevel level, const std::string& msg, const std::string& msg_with_file_info);
        void mylog_write(const std::string& type, logLevel level, const std::string& msg, const std::string& msg_with_file_info);
        bool contains(logType type);

        bool regist_log(logType type, std::string filename);
        bool regist_log(const std::string& type, std::string filename);
    signals:
        void send_message(logLevel loglevel, logType logtype, const std::string& msg);


    private:
        ILogHandler* handler = nullptr;
    };


}
