#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(EAPPLUGIN_LIB)
#  define EAPPLUGIN_EXPORT Q_DECL_EXPORT
# else
#  define EAPPLUGIN_EXPORT Q_DECL_IMPORT
# endif
#else
# define EAPPLUGIN_EXPORT
#endif
