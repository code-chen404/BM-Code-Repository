#pragma once
#include <QVariantMap>
#include <QMap>
#include <QString>
#include <functional>
#include "EapInterfaceMeta.h"

class EAPHeaderBinder {
public:
    // 自定义占位符 provider 回调类型
    // 参数：interfaceKey, meta；返回值：占位符值
    using PlaceholderProvider = std::function<QString(const QString&, const EapInterfaceMeta&)>;
     
    // 加载结构：
    // {
    //   "default": { "eqp_id":"EQP-001", "function_name":"${nameNoSlash}", "trx_id":"${uuid}", "time_stamp":"${now:yyyy-MM-dd HH:mm:ss}" },
    //   "interfaces": {
    //     "UserVerify": { "eqp_id":"EQP-B2" }
    //   }
    // }
    bool loadFromFile(const QString& path, QString* errorOut = nullptr);

    // 基于接口与 meta 的 headerMapping，将"可用的 header 本地字段"从配置合并进输入 params
    // - 仅合并 meta.headerMap 里存在的本地字段键
    // - 支持占位符展开：${now:...}, ${uuid}, ${name}, ${nameNoSlash}, ${env:XXX}, ${ts:epoch_ms}, ${ts:epoch_s}, ${custom:XXX}
    QVariantMap mergedParamsFor(const QString& interfaceKey,
        const EapInterfaceMeta& meta,
        const QVariantMap& inputParams) const;

    // 注册自定义占位符 provider
    // name: 占位符名称，如 "sign", "tenant"（使用时为 ${custom:sign}）
    // callback: 回调函数，返回占位符值
    void registerPlaceholderProvider(const QString& name, PlaceholderProvider callback);

private:
    QVariant expandValue(const QString& s,
        const QString& interfaceKey,
        const EapInterfaceMeta& meta) const;

private:
    QVariantMap defaultHeader_;
    QMap<QString, QVariantMap> perInterfaceHeader_;
    QMap<QString, PlaceholderProvider> customProviders_;
};
