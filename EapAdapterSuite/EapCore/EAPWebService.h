#pragma once

#include "eapcore_global.h"

#include <QObject>
#include <QJsonObject>
#include <QVariantMap>
#include <QMap>
#include <QSet>
#include <QStringList>
#include <functional>
#include <memory>

#include "EapInterfaceMeta.h"
#include "eap/EAPEnvelopeShim.h"

/*
EAPWebService（服务端）
- 使用 EAPEnvelopeShim 做“外壳键名映射”，仅配置请求/响应的 head/body 键名即可。
- 仍保留：大小写不敏感匹配（默认开）、严格校验 URL 与 function_name 一致（可配）、
         接口启用检查、仅允许 push、回调超时保护、优雅退出、payload 大小限制、网络超时等。
*/
 
class EAPMessageLogger;
class EAPDataCache;

class EAPCORE_EXPORT EAPWebService : public QObject {
    Q_OBJECT
public:
    using Provider = std::function<QVariantMap(const QString& fn, const QJsonObject& reqJson, const QVariantMap& mappedReq)>;

    explicit EAPWebService(QObject* parent = nullptr);
    ~EAPWebService() override;

    // 接口配置（VendorConfigLoader）
    bool loadInterfaceConfig(const QString& configPath);

    bool isValid()const;

    // 加载外壳键名策略（仅 default 段），不加载则使用 EAPEnvelopeShim 的默认值
    bool loadEnvelopePolicy(const QString& policyPath, QString* errorOut = nullptr);

    // 访问控制与行为
    void setAllowOnlyPushDirection(bool on);
    void setAllowedFunctions(const QStringList& functions);
    void setCaseInsensitiveFunctionMatch(bool on);
    void setStrictHeadFunctionMatch(bool on); // 将与 envelopeCfg.strictMatch 一起生效（与条件）

    // 服务
    bool start(quint16 port = 8026, const QString& host = QStringLiteral("0.0.0.0"));
    void stop();

    // 网络参数
    void setMaxPayloadBytes(size_t bytes);
    void setNetworkTimeouts(int readTimeoutMs, int writeTimeoutMs, int idleIntervalMs);
    void setAutoStopOnAppQuit(bool on);

    // 响应回调（选其一；raw 优先）
    void setRawResponder(std::function<QJsonObject(const QString& fn,
        const QJsonObject& reqJson,
        const QVariantMap& mappedParams)> cb);
    void setMappedResponder(std::function<QVariantMap(const QString& fn,
        const QJsonObject& reqJson,
        const QVariantMap& mappedParams)> cb);
    void setResponderTimeoutMs(int timeoutMs);

    QString lastError() const;

    const EapInterfaceMeta* getMeta(const QString& functionOrKey) const;

    // 设置消息日志记录器
    void setMessageLogger(EAPMessageLogger* logger);

    // 设置数据缓存
    void setDataCache(EAPDataCache* cache);


signals:
    void requestReceived(const QString& functionName,
        const QJsonObject& json,
        const QMap<QString, QString>& headers,
        const QString& remoteAddr);
    void mappedRequestReady(const QString& functionName,
        const QVariantMap& params,
        const QJsonObject& rawJson);
    void requestRejected(const QString& functionName,
        int httpStatus,
        const QString& reason,
        const QString& remoteAddr);
    void responseSent(const QString& functionName,
        int httpStatus,
        const QJsonObject& response,
        const QString& remoteAddr);
    
    // P0: 超时控制 - 新增信号
    void responderTimeout(const QString& functionName,
        int timeoutMs,
        const QString& remoteAddr);
    void networkTimeout(const QString& functionName,
        const QString& remoteAddr);

private:
    struct Impl;
    std::unique_ptr<Impl> d;
};