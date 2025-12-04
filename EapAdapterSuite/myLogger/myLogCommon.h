#pragma once
#include <string>

namespace myLog
{
    constexpr int c_max_log_size = 1024 * 1024 * 1024;
    constexpr int c_max_files = 5;
    constexpr int c_overwrite = false;

    enum class log_source_type
    {
        log_spd,
        log_boostlog
    };

    enum class logLevel
    {
        lv_trace,
        lv_debug,
        lv_info,
        lv_warn,
        lv_error,
        lv_critical
    };


    enum class logType : int
    {
        logtype_operation,
        logtype_running,
        logtype_alg,
        logtype_software = logtype_alg + 10,
        logtype_camera,
        logtype_ligthcontrol,
        logtype_plc,
        logtype_robot,
        logtype_plugins,
        logtype_ubplatform,
        logtype_mainUI,

        logtype_tcp = logtype_software + 30,
        logtype_serialport = logtype_software + 90,
        logtype_serialport_lightcontrol,
        logtype_voltage_resistance,

        logtype_reverse1 = logtype_software + 200,
        logtype_reverse2,
        logtype_reverse3,
        logtype_reverse4,
        logtype_reverse5,
        logtype_rpcserver,
        logtype_rpcclient,

        logtype_crypto = logtype_reverse5 + 100,
        logtype_usermanager = logtype_crypto + 10,
        logtype_totalcount = 1000
    };

    struct logConfig
    {
        size_t max_file_size;
        int max_files;
        bool m_overwrite;
        std::string log_name;
        std::string log_filename;

        logConfig();
      
        logConfig(const logConfig& other);

        
        logConfig& operator=(const logConfig& other);

 
        logConfig(logConfig&& other) noexcept;
    };
}
