#pragma once

#include <QObject>
#include <QVariantMap>
#include "ubframework_global.h"
#include "ubBase.h"

class UBFRAMEWORK_EXPORT ubContext
{
public:
	ubContext();
	virtual ~ubContext();

public:
	static ubContext * instance();
	void setGlobal(QString name, QVariant data);
	QVariant getGlobal(QString name);
	QVariantMap globalVariables();
	bool hasGlobal(QString name);

	void setContext(QString name, QVariant data);
	QVariant getContext(QString name);
	QVariantMap context();
private:
	QVariantMap _mapSystem;
	QVariantMap _mapGlobal;
};