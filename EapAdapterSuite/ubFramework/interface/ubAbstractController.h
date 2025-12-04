#pragma once
#include "ubBase.h"
#include "ubframework_global.h"
#include <QVariant>
#include <QWidget>

class ubHWManager;
class ubContext;
class QScriptEngine;

#define		kDeviceBaseClassName		"ubAbstractDevice"
#define		kCameraBaseClassName		"ubCameraBase"
#define		kMotionBaseClassName		"ubMotionBase"
#define		kLightCtrlBaseClassName		"ubLightCtrlBase"
#define		kPLCBaseClassName			"ubPLCBase"

class ubAbstractProject
{
public:
	//load & unload
	virtual int open(const char * file) = 0;
	virtual int close() = 0;

	//information
	virtual QStringList fileSequence() = 0;
	virtual QVariantMap infor() = 0;
	


private:

};



class UBFRAMEWORK_EXPORT ubAbstractController : public QObject
{
	Q_OBJECT
public:
	enum Argument_Type
	{
		ARG_BASE,
		ARG_ADVANNCE
	};
	Q_ENUM(Argument_Type)

	enum Mode
	{
		MODE_EDITOR = 0x01,
		MODE_RUNNING = 0x02
	};

	enum CtrlCmd
	{
		//Engine
		CMD_RUN,		//data : [no used]
		CMD_PAUSE,		//data : [no used]
		CMD_RESUME,		//data : [no used]
		CMD_STOP,		//data : [no used]

		//project
		CMD_LOAD_PROJECT,		//data : [input] project file path
		CMD_CLOSE_PROJECT,		//data : no used
		CMD_FILE_SEQUENCES,		//data : [output] sequence file list of current project

		CMD_USER = 1024		//user define
	};
	Q_ENUM(CtrlCmd)

public:
	ubAbstractController();
	virtual ~ubAbstractController();

public:
	//system context
	virtual ubContext * context() = 0;		//设备，全局对象，全局变量...
	virtual QObjectList nodes() = 0;		//节点
	virtual QScriptEngine * scriptEngine() = 0;

	//virtual QMap<QString,QObject*> devices() = 0;
	//virtual QMap<QString, QObject*> objects() = 0;
	
	//virtual QObject * getDevices(QString& name) = 0;
	//virtual QObject * getNode(QString& name) = 0;
	//virtual QObject * getObject(QString& name) = 0;
	//virtual QVariant getVariable(QString name) = 0;

public:
	//项目相关,project management
	virtual int doCmd(CtrlCmd cmd, const QVariant& data = QVariant()) = 0;
	virtual int setMode(Mode mode) = 0;
	//virtual int loadProject(const char * file = nullptr) = 0;;
	//virtual int closeProject() = 0;
	//virtual int runProject(QStringList& sequences = QStringList()) = 0;
	//virtual int stopProject() = 0;
	//virtual int pauseProject() = 0;
	//virtual int resumeProject() = 0;

	
	//virtual int configNode(QVariantMap& nodeInfor, QVariant& data) = 0;
	//virtual int config(QString key, QVariant& data) = 0;
	
	

	//virtual QStringList nodeList() = 0;
	//virtual QStringList propertys(QString& nodeName) = 0;

signals:
	void projectLoaded(QString fileName);
	void projectClosed();
	void system_event(int event, QVariant data);

	void OnSeqStart(QVariantMap context);
	void OnSeqEnd(int state, QVariantMap context);
	void OnNodeStart(QString nodeName, QVariantMap data);
	void OnNodeFinished(QString nodeName, QVariantMap data);
	void OnNodeError(QString nodeName, QVariantMap data);
	void OnNodeCancel(QString nodeName, QVariantMap data);

private:

};
