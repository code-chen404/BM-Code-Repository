#pragma once

#include <QObject>
#include "uicore_global.h"
#include <QMap>
#include <QMutex>

enum MediatorState
{
    Idle,
    Running,
    Stop
};

#define tUserLevelChanged "user_level_changed"
#define tModelChanged "model_changed"


class ubAbstractController;
class UICORE_EXPORT UiMediator  : public QObject
{
    Q_OBJECT
    
    
public:
    ~UiMediator();
    static UiMediator* instance();

    int setController(ubAbstractController* ctrl);
    ubAbstractController* getController();
    /// <summary>
    /// 注册对象
    /// 对象如果需要处理广播消息，需要实现槽函数onUnivesalMsg(QVariantMap&)
    /// </summary>
    /// <param name="objectName">对象名称-应该全局唯一</param>
    /// <param name="object">对象</param>
    void regist(QString& objectName,QObject* object);
    void deInitialize();

    /// <summary>
    /// 脚本直接调用ui对象的指定函数
    /// </summary>
    /// <param name="destination">目标，ui对象的object name</param>
    /// <param name="method">对应方法</param>
    /// <param name="args">list对象</param>
    /// <param name="kwargs">json对象</param>
    /// <returns></returns>
    QVariant call(QString& destination,QString& method,QVariantList& args,QVariantMap& kwargs);
    /// <summary>
    /// 广播json对象
    /// </summary>
    /// <param name="topic"></param>
    /// <param name="kwargs"></param>
    /// <returns></returns>
    int broadcast(QString& topic,QVariantMap& kwargs);
    /// <summary>
    /// 广播数组
    /// </summary>
    /// <param name="topic"></param>
    /// <param name="list"></param>
    /// <returns></returns>
    int broadcast(QString& topic,QVariantList& list);
    void initialize_globalvar();

    
signals:
    void universalMessage(QVariantMap& kwargs); //
private:
    UiMediator(QObject* parent=nullptr);

    ubAbstractController* controller;
    QMap<QString,QObject*> members;
    QMutex mutex;
    MediatorState state;
};
