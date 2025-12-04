#pragma once
#include <QObject>
#include "ubframework_global.h"

class ubPluginInterface;

class ubEngineInterface;
class ubDeviceModule;
class ubUserInterface;
class UBFRAMEWORK_EXPORT ubManager:public QObject
{
	Q_OBJECT
public:
	ubManager();
	virtual ~ubManager();

public:
	int initial(const char * configFile = nullptr);
	int de_initial();

signals:
	void log_message(QString& message, int level = 0);

protected:
	int loadEngine(const char * fileEngine);
	int loadUi(const char * fileUi);
	int loadModule(const char * fileModule);
	ubPluginInterface * loadPlugin(const char * filePlugin);
	ubDeviceModule* loadDevice(const char * filePlugin);
	ubUserInterface* loadUserInterface(const char * filePlugin);


protected:
	void _log(QString msg, int level = 0);
private:
	QString _uiPath;
	ubEngineInterface * _engine{nullptr};
	ubUserInterface * _ui{ nullptr };
	QList<ubDeviceModule * > _devices;
};

