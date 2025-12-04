#include  "SpdLogHandler.h"
#include "implement.h"
#include <fmt/core.h>
using namespace myLog;

constexpr int max_size_m = 1024 * 1024;
constexpr int min_size = 10;

SpdLogHandler::SpdLogHandler(QObject* parent)
{
    log_init("./", max_size_m * 500, 10, true);
}

SpdLogHandler::~SpdLogHandler()
{
    spdlog::shutdown();
}

void SpdLogHandler::log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite)
{
    log_dir = dir;
    log_maxsize = std::max(max_size, static_cast<size_t>(c_max_log_size));
    log_maxfile = max_file;

    if (m_Init)
    {
        return;
    }
    std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    struct std::tm timeinfo;
    localtime_s(&timeinfo, &now);
    char date_buf[80];
    std::strftime(date_buf, sizeof(date_buf), "%Y-%m-%d", &timeinfo);

    char date_dir[80];
    std::strftime(date_dir, sizeof(date_dir), "%Y%m%d", &timeinfo);

    // const std::string alg_filename = "logs/" + get_folder_name() + "/alg_" + std::string(date_buf) + ".log";
    const std::string run_filename = "logs/" + get_folder_name() + "/running_" + std::string(date_buf) + ".log";
    const std::string operation_filename = "logs/" + get_folder_name() + "/operation_" + std::string(date_buf) + ".log";
    template_filename = "logs/" + get_folder_name() + "/TEMPLATE_" + std::string(date_buf) + ".log";

    PTR run_logger;
    CREATE_SPDLOG_FILE_LOGGER("running_log", run_filename, log_maxsize, log_maxfile, run_logger);
    logger_map.insert(std::make_pair(logType::logtype_running, std::move(run_logger)));

    PTR operation_logger;
    CREATE_SPDLOG_FILE_LOGGER("operation_log", operation_filename, log_maxsize, log_maxfile, operation_logger);
    logger_map.insert(std::make_pair(logType::logtype_operation, std::move(operation_logger)));

    m_Init = true;
}

void SpdLogHandler::log_write(logType log_type, const std::string& msg, const std::string& msg_withfileinfo,
                              logLevel level)
{
    const auto spd_logger = get_logger(log_type);
    message_distribution(level, log_type, msg);
    if (!spd_logger)
    {
        return;
    }
    switch (level)
    {
    case logLevel::lv_debug:
        {
            spd_logger->debug(msg_withfileinfo);

            spd_logger->flush();
        }
        break;
    case logLevel::lv_info:
        {
            spd_logger->info(msg_withfileinfo);

            spd_logger->flush();
        }
        break;
    case logLevel::lv_error:
        {
            spd_logger->error(msg_withfileinfo);
            spd_logger->flush();
        }
        break;
    }
}

void SpdLogHandler::log_write(const std::string& log_type, const std::string& smg, const std::string& msg_withfileinfo,
                              logLevel level)
{
    const auto spd_logger = get_logger(log_type);
    message_distribution(level, log_type, smg);
    if (!spd_logger)
    {
        return;
    }
    switch (level)
    {
    case logLevel::lv_debug:
        {
            spd_logger->debug(msg_withfileinfo);

            spd_logger->flush();
        }
        break;
    case logLevel::lv_info:
        {
            spd_logger->info(msg_withfileinfo);

            spd_logger->flush();
        }
        break;
    case logLevel::lv_error:
        {
            spd_logger->error(msg_withfileinfo);
            spd_logger->flush();
        }
        break;
    }
}

void SpdLogHandler::log_flush()
{
}

bool SpdLogHandler::contains(logType type)
{
    return logger_map.contains(type);
}

bool SpdLogHandler::contains(const std::string& type)
{
    return logger_map2.contains(type);
}

int SpdLogHandler::regist_log(logType type, logConfig l_config)
{
    if (contains(type))
    {
        return 0;
    }
    PTR logger;
    QString str = QString::fromStdString(template_filename);
    const std::string filename = str.replace("TEMPLATE", l_config.log_filename.c_str()).toStdString();
    CREATE_SPDLOG_FILE_LOGGER(l_config.log_name, filename, log_maxsize, log_maxfile, logger);
    logger_map.insert(std::make_pair(type, std::move(logger)));
    return 0;
}

int SpdLogHandler::regist_log(const std::string& type, logConfig l_config)
{
    if (contains(type))
    {
        return 0;
    }
    PTR logger;
    QString str = QString::fromStdString(template_filename);
    const std::string filename = str.replace("TEMPLATE", l_config.log_filename.c_str()).toStdString();
    CREATE_SPDLOG_FILE_LOGGER(l_config.log_name, filename, log_maxsize, log_maxfile, logger);
    logger_map2.insert(std::make_pair(type, std::move(logger)));
    return 0;
}

void SpdLogHandler::message_distribution(logLevel level, logType type, const std::string& msg)
{
    emit sig_message(level, type, msg);
}

void SpdLogHandler::message_distribution(logLevel level, const std::string& type, const std::string& msg)
{
    emit sig_message2(level, type, msg);
}

std::string SpdLogHandler::get_folder_name()
{
    std::ostringstream oss;
    const auto now = std::chrono::system_clock::now();
    const auto timeInfo = std::chrono::system_clock::to_time_t(now);
    std::tm tmInfo;
    localtime_s(&tmInfo, &timeInfo);
    oss << std::put_time(&tmInfo, "%Y%m%d");
    return oss.str();
}

std::shared_ptr<spdlog::logger> SpdLogHandler::get_logger(logType logtype)
{
    if (contains(logtype))
    {
        return logger_map[logtype];
    }
    return nullptr;
}

std::shared_ptr<spdlog::logger> SpdLogHandler::get_logger(const std::string& logtype)
{
    if (contains(logtype))
    {
        return logger_map2[logtype];
    }
    return nullptr;
}
