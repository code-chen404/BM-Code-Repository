#pragma once

#include <QStringList>

class AppConfig
{
public:
	AppConfig();
	virtual ~AppConfig();

public:
	int Load(const char * file);
	int LoadJs(const char * file);

public:
	QStringList appList;
	QStringList moduleList;
	QString enginePath;
	QString profile;
	QString uiPath;

protected:
	void _log(QString msg, int level = 0);
};

