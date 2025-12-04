#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(EAPCORE_LIB)
#  define EAPCORE_EXPORT Q_DECL_EXPORT
# else
#  define EAPCORE_EXPORT Q_DECL_IMPORT
# endif
#else
# define EAPCORE_EXPORT
#endif
