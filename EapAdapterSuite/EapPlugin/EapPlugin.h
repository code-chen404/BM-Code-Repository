#pragma once

#include "eapplugin_global.h"
#include <ubDeviceModule.h>

class QMenu;
class EapManager;

class EAPPLUGIN_EXPORT EapPlugin : public QObject, public ubDeviceModule
{
    Q_OBJECT
        Q_PLUGIN_METADATA(IID ubDeviceModule_IID FILE "EapPlugin.json")
        Q_INTERFACES(ubDeviceModule)
public:
    Q_INVOKABLE EapPlugin();
    ~EapPlugin(); 

    virtual QString DeviceName() override;
    virtual QString Description() override;
    virtual int Load(void* sender)override;
    virtual int Register(void* engine, void* context) override;
    virtual QMap<QString, QWidget*> DebugPanels()override;
    virtual int SelfTest(void* context /* = nullptr */) override;
    virtual int UnLoad(void* sender)override;
    virtual int DoCmd(const char* cmd, void* buffer = nullptr)override;

public slots:
    void attachUI(QWidget* widget, QMenu* menu, void* context);

private:
    EapManager* manager_;  // 替代全局对象
};
