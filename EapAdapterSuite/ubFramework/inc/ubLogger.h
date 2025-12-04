#pragma once
#include "ubBase.h"
#include <QFile>
#include <QMutex>
class UBFRAMEWORK_EXPORT ubLogger
{
public:
	ubLogger();
	virtual ~ubLogger();

public:
	bool open();
	void close();
	void setLogFile(const char * file);
	void Log(QString msg, int level = ubBase::DEBUG);

	static ubLogger * defaultLogger();

private:
	QString _filepath;
	QFile * _file{nullptr};
	QMutex _mutex;
};

