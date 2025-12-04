#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>

#define CREATE_QUILL_FILE_HANDLER(filename, handler,max_size,backup_file,overwrite) \
    handler = quill::file_handler(filename, \
        []() { \
            quill::RotatingFileHandlerConfig cfg; \
            cfg.set_rotation_max_file_size(max_size);\
            cfg.set_max_backup_files(backup_file); \
            cfg.set_overwrite_rolled_files(overwrite); \
            return cfg; \
        }());


//#define CREATE_QUILL_FILE_LOGGER(logger_name,filename,max_size,backup_file,overwrite,logger_foo) \
//    std::shared_ptr<quill::Handler> logger_foo##file_handler##__LINE__ =  quill::file_handler(filename, \
//        []() { \
//            quill::RotatingFileHandlerConfig cfg; \
//            cfg.set_rotation_max_file_size(max_size);\
//            cfg.set_max_backup_files(backup_file); \
//            cfg.set_overwrite_rolled_files(overwrite); \
//            return cfg; \
//        }()); \
//    logger_foo = quill::create_logger(logger_name, std::move(logger_foo##file_handler##__LINE__));

#define CREATE_QUILL_FILE_LOGGER(logger_name, filename, max_size, backup_file, overwrite, logger_foo) \
    do { \
        auto max_size_captured = max_size; \
        auto backup_file_captured = backup_file; \
        auto overwrite_captured = overwrite; \
        std::shared_ptr<quill::Handler> logger_foo##file_handler##__LINE__ = quill::file_handler(filename, \
            [max_size_captured, backup_file_captured, overwrite_captured]() { \
                quill::RotatingFileHandlerConfig cfg; \
                cfg.set_rotation_max_file_size(max_size_captured); \
                cfg.set_max_backup_files(backup_file_captured); \
                cfg.set_overwrite_rolled_files(overwrite_captured); \
                return cfg; \
            }()); \
        logger_foo = quill::create_logger((logger_name), std::move(logger_foo##file_handler##__LINE__)); \
    } while (0)


#define CREATE_SPDLOG_FILE_HANDLER(filename, handler, max_size, max_files) \
    handler = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(filename, max_size, max_files);

#define CREATE_SPDLOG_FILE_LOGGER(logger_name, filename, max_size, max_files, logger_var) \
    std::shared_ptr<spdlog::sinks::rotating_file_sink_mt> logger_var##file_sink##__LINE__ = nullptr; \
    CREATE_SPDLOG_FILE_HANDLER(filename, logger_var##file_sink##__LINE__, max_size, max_files); \
    logger_var = std::make_shared<spdlog::logger>((logger_name), logger_var##file_sink##__LINE__); \
    logger_var->set_level(spdlog::level::trace); \
    logger_var->flush_on(spdlog::level::info);
