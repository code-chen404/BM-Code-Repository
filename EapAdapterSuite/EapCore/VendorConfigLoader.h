#pragma once

#include <QString>
#include <QMap>
#include "EapInterfaceMeta.h"
#include "eapcore_global.h"
class EAPCORE_EXPORT VendorConfigLoader {
public:
    // 加载接口配置
    static bool loadFromFile(const QString& path,
        QMap<QString, EapInterfaceMeta>& outMap,
        QString& baseUrl,
        QString& error);

    // 解析并合并配置：default + interfaces[interfaceKey]
    // 返回最终生效的配置（支持深度合并） 
    static EapInterfaceMeta resolveConfig(
        const QString& interfaceKey,
        const QMap<QString, EapInterfaceMeta>& interfaces,
        const EapInterfaceMeta* defaultMeta = nullptr);
};
