#ifndef PARAMETERHELPER_H
#define PARAMETERHELPER_H

#include <QVariant>
#include <QVariantMap>
#include <QString>
#include <QReadWriteLock>
#include <QJsonObject>

#include "eapcore_global.h"
class EAPCORE_EXPORT ParameterHelper
{
public:
    static bool loadDefaultParam(const QString& filepath);
    static QVariant getParam(const QString& interfaceName, const QString& keyName);
    static void mergeTo(QVariantMap& inputMap, const QString& interfaceName, const QString& keyName);
    static void mergeAllTo(QVariantMap& inputMap, const QString& interfaceName);

    static void mergeToFromJson(QVariantMap& inputMap, const QJsonObject& obj, const QString& keyName);
    static void mergeAllToFromJson(QVariantMap& inputMap, const QJsonObject& obj);

    // QJsonObject 版本的合并接口
    static void JsonmergeTo(QJsonObject& inputMap, const QString& interfaceName, const QString& keyName);
    static void JsonmergeAllTo(QJsonObject& inputMap, const QString& interfaceName);

    static void JsonmergeToFromJson(QJsonObject& inputMap, const QJsonObject& obj, const QString& keyName);
    static void JsonmergeAllToFromJson(QJsonObject& inputMap, const QJsonObject& obj);

    // 更新/追加路径方式（支持类似 "rfid_infos.rfid.lot_id" 的路径）
    // 语义：
    // - path 用点分隔，找到对应的容器/数组并在目标位置应用 value
    // - 当路径中遇到数组：如果最后目标是数组元素的 key（例如 .rfid.lot_id）：
    //    * 如果传入 value 为数组，则为数组每个元素追加一个新对象 { finalKey: element }
    //    * 如果传入单值，则为数组中每个元素（object）设置/替换 finalKey；若数组为空则追加一个对象
    // - 会自动创建丢失的中间对象/数组（按合理默认）以确保能写回结果
    static void JsonUpdateRfidKey(QJsonObject& inputObj, const QString& path, const QJsonValue& value);
    static void JsonUpdateRfidKeyFromVariant(QJsonObject& inputObj, const QString& path, const QVariant& value);

private:
    ParameterHelper() = delete;
     
    static QVariantMap s_params;

    // 不再声明拷贝不可用的静态成员，改为通过函数获取引用
    static QReadWriteLock& lock();

    // 判断 QVariant 是否“空”——用于判定已存在但“无值”的情况
    static bool isEmptyVariant(const QVariant& v);
};

#endif // PARAMETERHELPER_H