#include "EapPlugin.h"

#include <ubEngineInterface.h>
#include<QMap>
#include"EapManager.h"
#include"UiMediator.h"

EapPlugin::EapPlugin() : manager_(nullptr)
{
}

EapPlugin::~EapPlugin()
{
    // manager_ 会被 Qt 父对象自动删除
}

// 返回这个插件/设备模块的名字
QString EapPlugin::DeviceName()
{
    return tr("eapManager");
}

// 返回插件的描述
QString EapPlugin::Description()
{
    return tr("eapManager");
}

// 插件加载
int EapPlugin::Load(void* sender)
{
    return 0;
}

// 插件卸载
int EapPlugin::UnLoad(void* sender)
{
    return 0;
}

// 把 EapManager 注册进两套体系
int EapPlugin::Register(void* engine, void* contex)
{
    ubEngineInterface* _engine = static_cast<ubEngineInterface*>(engine); // 把 void* 转成引擎接口
    
    // 创建 EapManager 实例（作为成员变量）
    QString str = "EapManager";
    if (!manager_) {
        manager_ = new EapManager();      
        manager_->setObjectName(str); // 给 Qt 对象命名，方便日志、调试、QSS/查找控件时使用
    }

    // 注册到引擎
    _engine->RegisterObject(manager_, "eapManager"); // 之后其他模块可以通过引擎，用 "eapManager" 这个名字拿到这个对象
    UiMediator::instance()->regist(str, manager_); // 注册进 UiMediator，后面 UI 层就可以通过 UiMediator 找到这个 EapManager 对象、给它发消息或让它监听 topic
    
    return 0;
}

// 调试面板
QMap<QString, QWidget*> EapPlugin::DebugPanels()
{
    QMap<QString, QWidget*> panelsMap;
    return panelsMap;
}

// 自检接口
int EapPlugin::SelfTest(void* context)
{
    return 0;
}

// 附加 UI 元素
void EapPlugin::attachUI(QWidget* widget, QMenu* menu, void* context)
{

}

// 命令接口
int EapPlugin::DoCmd(const char* cmd, void* buffer)
{
    return 0;
}
