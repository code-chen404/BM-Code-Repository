#pragma once
#include <QObject>

class ubDeviceModule
{
public:
	virtual QString DeviceName() = 0;		//device name
	virtual QString Description() = 0;		//version 


	/**
	 *load module, 
	 * @param sender : normally sender is an IVisionEngine Object,
		for device module 
	 * @return int £º 0 successful,otherwise is error code,<0 is error,>0 is warning.
	 */
	//
	virtual int Load(void * sender) = 0;	


	virtual int Register(void * engine, void *  context) = 0;

	/**
	 * Register function
	 * @param context : context data from main application,is QVariantMap normally. 
	 *		  such as : map[engine] = IVisionEngine.
	 * @return int : 0 successful,otherwise is error code,<0 is error,>0 is warning.
	 */
	virtual int SelfTest(void * context = nullptr) = 0;
	

	virtual QMap<QString,QWidget*> DebugPanels() = 0;

	/**
	* Unload plugin
	* @param context : not use now,always nullptr
	* @return int : 0 successful,otherwise is error code,<0 is error,>0 is warning.
	*/
	virtual int UnLoad(void * sender) = 0;

	/**
	* plugin control function
	* @param cmd : command string
	* @param buffer : command data
	* @return int : 0 successful,otherwise is error code,<0 is error,>0 is warning.
	*/
	virtual int DoCmd(const char * cmd, void * buffer = nullptr) = 0; //reverse function, not use now.
};

#define ubDeviceModule_IID  "com.Bomming.interface.ubDeviceModule"
Q_DECLARE_INTERFACE(ubDeviceModule, ubDeviceModule_IID)