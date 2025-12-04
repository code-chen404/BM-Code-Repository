#pragma once
#include <QObject>
#include <QMetaObject>

//#include "IOperatorPlugin.h"
//#include "IFunctionObject.h"
//#include "IFunctionModulePlugin.h"

class MetaSystem
{
public:
	MetaSystem();
	virtual ~MetaSystem();

public:
	static QObject * CreateObjectByName(const char * typeName);
	static int GetType(const char * typeName);

};

