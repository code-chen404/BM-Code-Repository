#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(MYLOGGER_LIB)
#  define MYLOGGER_EXPORT Q_DECL_EXPORT
# else
#  define MYLOGGER_EXPORT Q_DECL_IMPORT
# endif
#else
# define MYLOGGER_EXPORT
#endif
