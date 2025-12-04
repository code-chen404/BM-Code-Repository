#pragma once
#include <QObject>

#define		kValue		"value"
#define		kRemark		"remark"
#define		kState		"state"

class ubFunctionObjectInterface : public QObject
{
public:
	/**
	* process node
	*/
	virtual int process() = 0;
};

#define ubFunctionObjectInterface_IID  "com.Bomming.interface.ubFunctionObjectInterface"
Q_DECLARE_INTERFACE(ubFunctionObjectInterface, ubFunctionObjectInterface_IID)