#include "QUillLogHandler.h"
#include "implement.h"
#include <fmt/color.h>
#include <fmt/core.h>

using namespace myLog;

constexpr int max_size_m = 1024 * 1024;
constexpr int min_size = 10;

QUtillLogHandler::QUtillLogHandler(QObject* parent)
{
    quill::start();
    log_init("./", 1000, 10, true);
}

QUtillLogHandler::~QUtillLogHandler()
{
   
}


const int max_logger_size = 1024 * 1024 * 1024;
const int max_backup_file = 5;
const bool i_overwrite = true;

void QUtillLogHandler::log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite)
{
    log_dir = dir;
    log_maxsize = std::max(max_size, static_cast<size_t>(max_size_m));
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
    //const std::string run_filename = "logs/" + get_folder_name() + "/running_" + std::string(date_buf) + ".log";
    //const std::string operation_filename = "logs/" + get_folder_name() + "/operation_" + std::string(date_buf) + ".log";
    //template_filename = "logs/" + get_folder_name() + "/TEMPLATE_" + std::string(date_buf) + ".log";

    //QLOG_PTR run_logger;
    //CREATE_QUILL_FILE_LOGGER("running_log", run_filename, log_maxsize, log_maxfile, m_overwrite, run_logger);
    //logger_map.insert(std::make_pair(logType::logtype_running, std::move(run_logger)));
    //LOG_DEBUG(run_logger, "{}", "test running log");

    //QLOG_PTR operation_logger;
    //CREATE_QUILL_FILE_LOGGER("operation_log", operation_filename, log_maxsize, log_maxfile, m_overwrite, operation_logger);
    //logger_map.insert(std::make_pair(logType::logtype_operation, std::move(operation_logger)));
    //LOG_DEBUG(operation_logger, "{}", "test operation log");
    std::shared_ptr<quill::Handler> logger;
    CREATE_QUILL_FILE_HANDLER("test002.log", logger, max_logger_size, max_backup_file, i_overwrite);
    quill::Logger* logger_foo = quill::create_logger("logger_foo", std::move(logger));

    for (int i = 0; i < 100000; ++i)
        {
            // 在这里执行并行化的任务
            LOG_INFO(logger_foo, "Hello from {} {} ", "library foo", i);

        };

    /*quill::Logger* test_logger;
    CREATE_QUILL_FILE_LOGGER("logger_foo2", "test002.log", max_logger_size, max_backup_file, i_overwrite, test_logger);
    LOG_INFO(test_logger, "{}", "test test_logger log");

    for (int i = 0; i < 100000; ++i)
    {
        LOG_INFO(test_logger, "Hello from {} {} ", "library foo", i);
        LOG_DEBUG(test_logger, "Hello from {} {} ", "library foo", i);
        LOG_ERROR(test_logger, "Hello from {} {} ", "library foo", i);
        LOG_TRACE_L3(test_logger, "Hello from {} {} ", "library foo", i);

    }*/
    m_Init = true;
}

void QUtillLogHandler::log_write(logType log_type, const std::string& msg, const std::string& msg_withfileinfo,
    logLevel level)
{
    const auto  spd_logger = get_logger(log_type);
    message_distribution(level, log_type, msg);
    if (!spd_logger)
    {
        return;
    }
    switch (level)
    {
    case logLevel::lv_debug:
    {
        // 白底 黑字
        LOG_DEBUG(spd_logger, "{}",msg_withfileinfo);
    }
    break;
    case logLevel::lv_info:
    {
        LOG_INFO(spd_logger, "{}", msg_withfileinfo);
    }
    break;
    case logLevel::lv_error:
    {
        // 黑底 红字
        auto fd = fmt::format(fg(fmt::terminal_color::red), msg_withfileinfo);
        auto bgmsg = fmt::format(bg(fmt::terminal_color::cyan), fd);
        LOG_ERROR(spd_logger, "{}", msg_withfileinfo);
    }
    break;
    }
}

void QUtillLogHandler::log_flush()
{

}

bool QUtillLogHandler::contains(logType type)
{
    return logger_map.contains(type);
}

int QUtillLogHandler::regist_log(logType type, logConfig l_config)
{
    if (contains(type))
    {
        return 0;
    }
    QLOG_PTR logger;
    QString str = QString::fromStdString(template_filename);
    const std::string filename = str.replace("TEMPLATE", l_config.log_filename.c_str()).toStdString();
    CREATE_QUILL_FILE_LOGGER(l_config.log_name, filename, log_maxsize, log_maxfile, m_overwrite,logger);
    logger_map.insert(std::make_pair(type, std::move(logger)));
    return 0;
}

void QUtillLogHandler::message_distribution(logLevel level, logType type, const std::string& msg)
{
    emit sig_message(level, type, msg);
}

std::string QUtillLogHandler::get_folder_name()
{
    std::ostringstream oss;
    const auto now = std::chrono::system_clock::now();
    const auto timeInfo = std::chrono::system_clock::to_time_t(now);
    std::tm tmInfo;
    localtime_s(&tmInfo, &timeInfo);
    oss << std::put_time(&tmInfo, "%Y%m%d");
    return oss.str();
}

QLOG_PTR QUtillLogHandler::get_logger(logType logtype)
{
    if (contains(logtype))
    {
        return logger_map[logtype];
    }
    return nullptr;
}
