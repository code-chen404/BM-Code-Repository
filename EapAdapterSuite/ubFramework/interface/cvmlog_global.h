#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(CVMLOG_LIB)
#  define CVMLOG_EXPORT Q_DECL_EXPORT
# else
#  define CVMLOG_EXPORT Q_DECL_IMPORT
# endif
#else
# define CVMLOG_EXPORT
#endif
