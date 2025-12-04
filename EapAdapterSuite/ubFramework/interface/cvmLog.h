#pragma once

#include "cvmlog_global.h"
#include "cvmLogDef.h"
#include <QObject>
#include "LogerBase.h"
#include <spdlog/fmt/fmt.h>

namespace cvm
{
    class CVMLOG_EXPORT cvmLog : public QObject
    {
        Q_OBJECT

        explicit cvmLog();
    public:
        ~cvmLog();
        static cvmLog *getInstance();

        void initLog(std::string name,LogType type,LogLevel level,std::string path,int maxsize,int maxcount);
        void log(std::string name,LogLevel level,std::string msg);

    signals:
        void logSig(QString name, int level, QString msg);

    private:
        LogerBase* m_log;
    };
}

#define LOG_INFO(name,...) cvm::cvmLog::getInstance()->log(name,cvm::LogLevel::info,fmt::format(__VA_ARGS__))

#define LOG_DEBUG(name,...) cvm::cvmLog::getInstance()->log(name,cvm::LogLevel::debug,fmt::format(__VA_ARGS__))

#define LOG_WARN(name,...) cvm::cvmLog::getInstance()->log(name,cvm::LogLevel::warn,fmt::format(__VA_ARGS__))

#define LOG_ERROR(name,...) cvm::cvmLog::getInstance()->log(name,cvm::LogLevel::err,fmt::format(__VA_ARGS__))