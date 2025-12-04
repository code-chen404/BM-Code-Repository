#pragma once
#include <QObject>
class ubMetaSystem;
class ubContext;
class ubEngineInterface
{
public:
	virtual int Initial() = 0;
	virtual int De_initial() = 0;
	virtual int RegisterObject(QObject * obj, QString name) = 0;
	virtual int RegisterScript(const char *filepath) = 0;
	virtual int RegisterString(QString buffer) = 0;
	virtual QObject * GetObject(QString name) = 0;
	//virtual int  LoadProfile(const char * filepath) = 0;
	//virtual int LoadSeq(QMap<QString, QObject*>& nodes,QByteArray& seqData) = 0;
	//virtual int LoadSeq(QByteArray& seqData) = 0;
	virtual const QVariantMap globalObjects() = 0;
	

	//Action
	virtual int Run(QByteArray& seqData, const char * postfix = nullptr, int loop = 1)=0;
	virtual int Stop(const char * postfix = nullptr) = 0;	//nullptr will stop all
	virtual int Pause() = 0;
	virtual int Resume() = 0;
	virtual int Step(int index) = 0;
	virtual int Jump(int index) = 0;

	//Interface
	//virtual IScriptEngine * scriptEngin() = 0;
	//virtual IOperatorManager * operatorManager() = 0;
	virtual ubContext * Context() = 0;
	virtual ubMetaSystem * metaSystem() =0;
	

	
private:

};

#define ubEngineInterface_IID  "com.Bomming.interface.ubEngineInterface"
Q_DECLARE_INTERFACE(ubEngineInterface, ubEngineInterface_IID)