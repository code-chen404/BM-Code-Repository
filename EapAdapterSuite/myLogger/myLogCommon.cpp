#include  "myLogCommon.h"
using namespace myLog;

logConfig::logConfig(): m_overwrite(false)
{
    max_file_size = c_max_log_size;
    max_files = c_max_files;
}

logConfig::logConfig(const logConfig& other): max_file_size(other.max_file_size),
                                              max_files(other.max_files),
                                              m_overwrite(other.m_overwrite),
                                              log_name(other.log_name),
                                              log_filename(other.log_filename)
{
}

logConfig& logConfig::operator=(const logConfig& other)
{
    if (this != &other)
    {
        max_file_size = other.max_file_size;
        max_files = other.max_files;
        m_overwrite = other.m_overwrite;
        log_name = other.log_name;
        log_filename = other.log_filename;
    }
    return *this;
}

logConfig::logConfig(logConfig&& other) noexcept: max_file_size(other.max_file_size),
                                                  max_files(other.max_files),
                                                  m_overwrite(other.m_overwrite),
                                                  log_name(std::move(other.log_name)),
                                                  log_filename(std::move(other.log_filename))
{
}
