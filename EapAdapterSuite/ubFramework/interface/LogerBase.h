#pragma once
#include "cvmLogDef.h"
#include "cvmlog_global.h"
#include <string>

namespace cvm
{
    class CVMLOG_EXPORT LogerBase
    {
    public:
        LogerBase();
        virtual ~LogerBase();

        virtual void init(std::string name,LogType type, LogLevel level,std::string path,int maxsize,int maxcount) = 0;
        virtual void log(std::string name,LogLevel level, std::string msg) = 0;
    };
}


