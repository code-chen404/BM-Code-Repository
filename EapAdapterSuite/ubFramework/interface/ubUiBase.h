#pragma once

#include <QWidget>
#include <QVariantMap>
#include <QVariantList>
#include "uicore_global.h"

class UICORE_EXPORT ubUiBase  : public QWidget
{
    Q_OBJECT

public:
    ubUiBase(QWidget *parent);
    ~ubUiBase();

    virtual void setParameter(QString config) {}
public slots:
    virtual void onUnivesalMsg(QVariantMap& msg);

protected:
    virtual void onUserChanged(int level)=0;
    virtual void onModelChanged(QString modelName)=0;
    Q_INVOKABLE virtual void onStructMsg(QString& topic,QVariantMap& msg)=0;
    virtual void onListMsg(QString& topic, QVariantList& list)=0;
};
