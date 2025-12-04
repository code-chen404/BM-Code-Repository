#pragma once

#include <mutex>

#include "eapcore_global.h"
#include <QString>
#include <QMap>
#include <QJsonObject>
#include <QVariantMap>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "EapInterfaceMeta.h"
#include "eap/EAPEnvelopeShim.h"
#include "eap/EAPHeaderBinder.h"

class EAPMessageLogger;
class EAPDataCache;

class EAPCORE_EXPORT EAPInterfaceManager : public QObject
{
    Q_OBJECT
public:
    explicit EAPInterfaceManager(QObject* parent = nullptr);
    ~EAPInterfaceManager();

    // 加载接口配置（VendorConfigLoader）
    bool loadInterfaceConfig(const QString& configPath);

    // 加载外壳键名策略（仅 default 段），不加载则使用 EAPEnvelopeShim 的默认值
    bool loadEnvelopePolicy(const QString& policyPath);
    // 新增：加载 header 参数（默认/按接口），用于填充 header_mapping 的本地字段
    bool loadHeaderParams(const QString& paramsPath);

    QString getLastError() const;
    QString getBaseUrl() const;
    int interfaceCount() const;

    void post(const QString& interfaceKey, const QVariantMap& params);

    // 新增：构造最终出网 payload（用于队列保存等场景）
    QJsonObject composePayloadForSend(const QString& interfaceKey, const QVariantMap& params);

    QStringList getInterfaceKeys() const;
    EapInterfaceMeta& getInterface(const QString& key);

    // 访问 HeaderBinder 以注册自定义 provider
    EAPHeaderBinder& headerBinder();

    // 设置消息日志记录器
    void setMessageLogger(EAPMessageLogger* logger);

    // 设置数据缓存
    void setDataCache(EAPDataCache* cache);

signals:
    void requestSent(const QString& key, const QJsonObject& payload);
    void responseReceived(const QString& key, const QJsonObject& response);
    void mappedResultReady(const QString& key, const QVariantMap& result);
    void requestFailed(const QString& key, const QString& error);

private:
    void postWithRetry(const QString& interfaceKey, const EapInterfaceMeta& meta,
        const QJsonObject& payload, int retriesLeft);

    // 检查响应是否需要重试（基于 RetryStrategy）
    bool shouldRetryBasedOnResponse(const EapInterfaceMeta& meta, const QVariantMap& parsedResponse) const;

    void setInterfaces(const QMap<QString, EapInterfaceMeta>& list);
    void setBaseUrl(const QString& url);

private:
    QMap<QString, EapInterfaceMeta> interfaces;
    QString baseUrl;
    QString lastError;

    // 外壳键名配置
    EAPEnvelope::Config envelopeCfg;

    EAPHeaderBinder headerBinder_; // 新增

    // 复用网络管理器
    QNetworkAccessManager* networkManager_;

    // 消息日志记录器
    EAPMessageLogger* messageLogger_;

    // 数据缓存
    EAPDataCache* dataCache_;


    mutable std::mutex cacheMutex_; // 保护数据缓存
};