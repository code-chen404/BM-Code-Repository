#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(UBFRAMEWORK_LIB)
#  define UBFRAMEWORK_EXPORT Q_DECL_EXPORT
# else
#  define UBFRAMEWORK_EXPORT Q_DECL_IMPORT
# endif
#else
# define UBFRAMEWORK_EXPORT
#endif
