#include "boostlogHandler.h"

#include <boost/log/core.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/trivial.hpp>
#include <boost/property_tree/ini_parser.hpp>

using namespace myLog;


BoostLogHandler::BoostLogHandler(QObject* parent)
{

}

BoostLogHandler::~BoostLogHandler()
{

}

void BoostLogHandler::log_init(const std::string& dir, size_t max_size, int max_file, bool overwrite)
{
    log_dir = dir;
    log_maxsize = max_size;
    log_maxfile = max_file;

    boost::log::add_common_attributes();

    boost::log::register_simple_formatter_factory<boost::log::trivial::severity_level, char>("Severity");
    boost::property_tree::ptree config;
    boost::property_tree::ini_parser::read_ini("log_config.ini", config);

    // 解析配置并进行相应的配置
    std::string target = config.get<std::string>("Sinks.File.Target");

    if (target == "filename")
    {
        std::string fileName = config.get<std::string>("Sinks.File.FileName");
        size_t rotationSize = config.get<size_t>("Sinks.File.RotationSize");
        int maxFiles = config.get<int>("Sinks.File.MaxFiles");

        // 根据配置项设置文件输出目标
        boost::log::add_file_log(
            boost::log::keywords::file_name = fileName,
            boost::log::keywords::rotation_size = rotationSize,
            boost::log::keywords::max_files = maxFiles
        );
    }
    else
    {
        boost::log::add_file_log(
            boost::log::keywords::file_name = log_dir + "/log_%Y-%m-%d_%H-%M-%S.%N.log",
            boost::log::keywords::rotation_size = log_maxsize,
            boost::log::keywords::max_size = log_maxfile * log_maxsize,
            boost::log::keywords::open_mode = (overwrite ? std::ios::out : std::ios::app)
        );
    }

    boost::log::add_console_log(std::cout, boost::log::keywords::format = "[%TimeStamp%] [%Severity%]: %Message%");

    boost::log::core::get()->set_filter(boost::log::trivial::severity >= boost::log::trivial::trace);

    m_Init = true;
}

void BoostLogHandler::log_write(logType log_type, const std::string& smg, const std::string& msg_withfileinfo,
    logLevel level)
{
    if (!m_Init)
        return;

    boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>& logger = get_logger(log_type);

    switch (level)
    {
    case logLevel::lv_debug:
        BOOST_LOG_SEV(logger, boost::log::trivial::debug) << msg_withfileinfo;
        break;
    case logLevel::lv_info:
        BOOST_LOG_SEV(logger, boost::log::trivial::info) << msg_withfileinfo;
        break;
    case logLevel::lv_warn:
        BOOST_LOG_SEV(logger, boost::log::trivial::warning) << msg_withfileinfo;
        break;
    case logLevel::lv_error:
        BOOST_LOG_SEV(logger, boost::log::trivial::error) << msg_withfileinfo;
        break;
    case logLevel::lv_critical:
        BOOST_LOG_SEV(logger, boost::log::trivial::fatal) << msg_withfileinfo;
        break;
    default:
        break;
    }
}

void BoostLogHandler::log_flush()
{
    if (!m_Init)
        return;

    boost::log::core::get()->flush();
}

bool BoostLogHandler::contains(logType type)
{
    return logger_map.contains(type);
}

int BoostLogHandler::regist_log(logType type, logConfig l_config)
{
    boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> logger;
    logger_map[type] = logger;
    return 0;
}

boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level>&  BoostLogHandler::get_logger(logType logtype)
{
    auto it = logger_map.find(logtype);
    if (it != logger_map.end())
        return it->second;

    boost::log::sources::severity_logger_mt<boost::log::trivial::severity_level> logger;
    logger_map[logtype] = logger;
    return logger;
}

void BoostLogHandler::message_distribution(logLevel level, logType type, const std::string& msg)
{

}

std::string BoostLogHandler::get_folder_name()
{
    std::ostringstream oss;
    const auto now = std::chrono::system_clock::now();
    const auto timeInfo = std::chrono::system_clock::to_time_t(now);
    std::tm tmInfo;
    localtime_s(&tmInfo, &timeInfo);
    oss << std::put_time(&tmInfo, "%Y%m%d");
    return oss.str();
}

void BoostLogHandler::configure_logger(logType logtype)
{
    // 根据需要配置每个 logger 的特定属性
    // 例如，可以设置日志格式、过滤器等
    // 使用 boost::log::set_formatter 和 boost::log::set_filter 函数
}
