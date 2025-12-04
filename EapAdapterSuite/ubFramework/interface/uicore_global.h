#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(UICORE_LIB)
#  define UICORE_EXPORT Q_DECL_EXPORT
# else
#  define UICORE_EXPORT Q_DECL_IMPORT
# endif
#else
# define UICORE_EXPORT
#endif
