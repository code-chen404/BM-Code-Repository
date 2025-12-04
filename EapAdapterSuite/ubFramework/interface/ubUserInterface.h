#pragma once
#include <QObject>

class ubUserInterface
{
public:
	virtual QString PluginName() = 0;		//plugin name
	virtual QString Description() = 0;		//version 

	/**
	 *load module, 
	 * @param sender : normally sender is an IVisionEngine Object.
	 * @return int£º 0 successful,otherwise is error code,<0 is error,>0 is warning.
	 */
	//
	virtual int Load(void * sender) = 0;	


	/**
	* Register function
	* @param context : context data from main application,is QVariantMap normally.
	*		  such as : map[engine] = IVisionEngine.
	* @return int : 0 successful,otherwise is error code,<0 is error,>0 is warning.
	*/
	//virtual int Register(void *  context) = 0;
	virtual int Register(void * engine, void *  context) = 0;



	/**
	 * Attach panel into main UI,normally attach into tools menu
	 */
	virtual int AttachPanels(QMap<QString, QWidget*> panels)=0;


	/**
	 * Normally return the main ui widgets of GUI.
	 */
	virtual QWidget * mainWidget() = 0;

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

#define ubUserInterface_IID  "com.Bomming.interface.ubUserInterface"
Q_DECLARE_INTERFACE(ubUserInterface, ubUserInterface_IID)