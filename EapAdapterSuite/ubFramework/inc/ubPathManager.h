#pragma once
#include <QString>
#include "ubframework_global.h"

class UBFRAMEWORK_EXPORT ubPathManager
{
public:
	ubPathManager();
	virtual ~ubPathManager();

public:
	static QString appPath();
	static QString configPath();
	static QString pluginPath();
	static QString modulePath();
	static QString libPath();
};

