#pragma once
#include <QObject>
#include <QMetaType>
#include <QMetaClassInfo>

#include "ubframework_global.h"

class ubFunctionNodeMgrInterface;

#define		CLASS_TYPE_KEY		 "__typeName"
class UBFRAMEWORK_EXPORT ubMetaSystem
{
public:
	ubMetaSystem(ubFunctionNodeMgrInterface * nodeMgr);
	virtual ~ubMetaSystem();

public:
	QObject * newObject(const char * typeName,const char * name = nullptr);
	QObject * findByName(QString name);

	template<typename T>
	static	QString typeName()
	{
		QString _typeName;
		//firstly try to read from class infor
		const QMetaObject& metaObj = T::staticMetaObject;
		int index = metaObj.indexOfClassInfo(CLASS_TYPE_KEY);
		if (index >= 0)
		{
			QMetaClassInfo infor = metaObj.classInfo(index);
			_typeName = infor.value();
		}
		else
		{
			//use class name
			_typeName = metaObj.className();
		}
		return _typeName;
	}

	template<typename T>
	static int registerMetaType(const char * typeName = nullptr)
	{
		if (!typeName)
		{
			//firstly try to read from class infor
			const QMetaObject metaObj = T::staticMetaObject;
			int index = metaObj.indexOfClassInfo(CLASS_TYPE_KEY);
			if (index >= 0)
			{
				QMetaClassInfo infor = metaObj.classInfo(index);
				typeName = infor.value();
			}
			else
			{
				//use class name
				typeName = metaObj.className();
			}
			
		}
		if (!typeName) return -2;
		int ret = qRegisterMetaType<T*>(typeName);
		return ret;
	}
	int getType(const char * typeName);

private:
	ubFunctionNodeMgrInterface * _nodeManager{nullptr};
};

