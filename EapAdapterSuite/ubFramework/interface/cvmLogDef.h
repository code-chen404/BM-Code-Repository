#pragma once

namespace cvm
{
    enum LogType
    {
        Daily,
        Rotating,
        DailyRotating
    };

    enum LogLevel
    {
        trace,
        debug,
        info,
        warn,
        err,
        critical,
        off
    };
}