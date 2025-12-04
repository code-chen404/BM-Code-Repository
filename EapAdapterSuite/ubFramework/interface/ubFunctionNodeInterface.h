#pragma once
#include <QByteArray>
#include <QVariantMap>
#include <QObject>
#include "ubFunctionObjectInteface.h"

#define		kNodeRawType	"__type"
class QGraphicsItem;
class QWidget;
class ubFunctionNodeInterface:public ubFunctionObjectInterface
{
public:
	/**
	 * initial function node in here
	 */
	virtual void initial(void * context) = 0;

	/**
	 * main item of function node.will be added into scene.
	 */
	virtual QGraphicsItem * graphicsItem() = 0;

	/**
	 * Object serialize 
	 * @return serialize data,normally is JSON string.
	 */
	virtual QByteArray serialize() = 0;

	/**
	 *  De serialize object.
	 *
	 * @param data, data will be de-serialize to initial object,normally should be JSON string.
	 */
	virtual int deserialize(QByteArray data) = 0;


	/**
	 * Preview widget for item.
	 * @return widget will be displayed in preview tool box.
	 */
	virtual QWidget * previewWidget() = 0;

	/**
	 * editor widget
	 * @return widget will displayed
	 */
	virtual QWidget * editor() = 0;

public:
	virtual void setUserData(QString name, QVariant value) =0;
	virtual QVariant getUserData(QString name) =0;
};

class ubFunctionNodeMgrInterface
{
public:
	virtual int newNode(QObject * metaObject, QString name) = 0;
	virtual void clear() = 0;
};