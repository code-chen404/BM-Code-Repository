#pragma once

#include "ubframework_global.h"
#include "ubFunctionNodeInterface.h"
#include "ubBase.h"

class ubGraphicsRoudrectNodeItem;
class UBFRAMEWORK_EXPORT ubFunctionNodeBase :public ubFunctionNodeInterface
{
public:
	ubFunctionNodeBase();
	virtual ~ubFunctionNodeBase();

public:
	/**
	* initial function node in here
	*/
	virtual void initial(void * context) override;

	/**
	* main item of function node.will be added into scene.
	*/
	virtual QGraphicsItem * graphicsItem() override;

	/**
	* Object serialize
	* @return serialize data,normally is JSON string.
	*/
	virtual QByteArray serialize() override;

	/**
	*  De serialize object.
	*
	* @param data, data will be de-serialize to initial object,normally should be JSON string.
	*/
	virtual int deserialize(QByteArray data) override;


	/**
	* Preview widget for item.
	* @return widget will be displayed in preview tool box.
	*/
	virtual QWidget * previewWidget() override;

	/**
	* editor widget
	* @return widget will displayed
	*/
	virtual QWidget * editor() override;


	//Object
	/**
	* process node
	*/
	virtual int process() =0;

public:
	virtual void setUserData(QString name, QVariant value) override;
	virtual QVariant getUserData(QString name) override;

public:
	enum NODE_TYPE
	{
		NODE_NORMAL,
		NODE_CASE,
		NODE_LOOP
	};

	bool isCaseNode() { return _type == NODE_CASE; }
	bool isLoopNode() { return _type == NODE_LOOP; }
protected:
	void setNodeType(NODE_TYPE type) 
	{ 
		_type = type; 
		if (_type == NODE_CASE) this->setProperty(IS_NODE_CASE, true);
		else if (_type == NODE_LOOP) this->setProperty(IS_NODE_LOOP, true);
	}
private:
	//ubGraphicsRoudrectNodeItem * _nodeItem{ nullptr };
	NODE_TYPE _type{ NODE_NORMAL };

	QVariantMap _userData;
};

