#include "EAPInterfaceManager.h"
#include "JsonBuilder.h"
#include "VendorConfigLoader.h"
#include "EAPMessageLogger.h"
#include "EAPMessageRecord.h"
#include "EAPDataCache.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>

#include "JsonParser.h"
#include "LoggerInterface.h"
#pragma execution_character_set("utf-8")


/*
一.配置输入：
1. 接口元数据--bool loadInterfaceConfig(const QString& configPath); // config_interfaces.merged.json
2. 外壳策略--bool loadEnvelopePolicy(const QString& policyPath); // config_interfaces.merged.json
3. 请求头模板--bool loadHeaderParams(const QString& paramsPath); // config_interfaces.merged.json

二.报文装配--QJsonObject EAPInterfaceManager::composePayloadForSend(const QString& interfaceKey, const QVariantMap& params)：





*/

/**
 * @brief 按“点路径”在 QJsonObject 中设置嵌套字段的值
 * @param obj   原始 JSON 对象（按值传入，在其拷贝上进行修改）
 * @param parts 通过 '.' 分割后的路径片段列表（如 ["header","user","name"]）
 * @param value 要设置的最终字段值
 * @return      设置完指定路径后的新 QJsonObject 对象
 */
static QJsonObject setByDotPathObj(QJsonObject obj, const QStringList& parts, const QJsonValue& value) {
    if (parts.isEmpty()) return obj;
    const QString& key = parts.first();
    if (parts.size() == 1) {
        obj.insert(key, value);
        return obj;
    }
    QJsonObject child = obj.value(key).toObject();
    child = setByDotPathObj(child, parts.mid(1), value);
    obj.insert(key, child);
    return obj;
}

EAPInterfaceManager::EAPInterfaceManager(QObject* parent)
    : QObject(parent), 
    networkManager_(new QNetworkAccessManager(this)), // 复用网络管理器
    messageLogger_(nullptr), // 消息日志记录器
    dataCache_(nullptr) // 数据缓存
{
    // 注册日志
    REGIST_LOG_TYPE("MES", "mes");
}

EAPInterfaceManager::~EAPInterfaceManager() {
    // networkManager_ 会被 Qt 父对象自动删除
}

/**
 * @brief 从配置文件加载所有 EAP 接口配置及基础 URL
 * @param path 配置文件路径（相对或绝对）
 * @return true  配置文件加载并解析成功，接口表和基础 URL 已更新
 * 
 * 0. R
 * 1. VendorConfigLoader::loadFromFile 无引用--R
 * 2. setInterfaces 无引用--R
 * 3. setBaseUrl 无引用--R
 */
bool EAPInterfaceManager::loadInterfaceConfig(const QString& path) 
{
    QMap<QString, EapInterfaceMeta> map;
    QString err, url;

    // 从 path 指定的配置文件中读取 JSON
    if (!VendorConfigLoader::loadFromFile(path, map, url, err)) {
        lastError = err;
        return false;
    }

    setInterfaces(map);
    setBaseUrl(url);
    return true;
}

/**
 * @brief 设置当前管理器维护的接口元数据映射
 * @param list 要设置的接口配置映射
 * 
 * 0. R
 */
void EAPInterfaceManager::setInterfaces(const QMap<QString, EapInterfaceMeta>& list) {
    interfaces = list;
}

/**
 * @brief 设置接口调用的基础 URL
 * @param url 要设置的基础 URL（例如 MES 服务器地址或统一网关地址）
 * 
 * 0. R
 */
void EAPInterfaceManager::setBaseUrl(const QString& url) {
    baseUrl = url;
}

/**
 * @brief 从 JSON 策略文件加载 EAP 报文外壳配置
 * @param policyPath 外壳策略配置文件路径（JSON 文件，绝对或相对路径）
 * @return true  加载成功，envelopeCfg 中保存了最新策略
 * 
 * 0. R
 * 1. EAPEnvelope::loadConfigFromFile 无引用--R
 */
bool EAPInterfaceManager::loadEnvelopePolicy(const QString& policyPath) {
    QString err;
    if (!EAPEnvelope::loadConfigFromFile(policyPath, envelopeCfg, &err)) { // 从 JSON 文件里把外壳配置读到 Config cfg 里（包括 default + 每个接口单独配置）
        lastError = err;
        return false;
    }
    return true;
}

/**
 * @brief 从 JSON 配置文件加载 EAP 请求 header 参数模板
 * @param paramsPath header 参数配置文件路径（JSON 格式）
 * @return true  加载成功，headerBinder_ 中已保存最新模板
 * 
 * 0. R
 * 1. EAPHeaderBinder::loadFromFile 无引用--R
 */
bool EAPInterfaceManager::loadHeaderParams(const QString& paramsPath) {
    QString err;
    if (!headerBinder_.loadFromFile(paramsPath, &err)) { //从 JSON 配置文件加载 EAP 请求 header 参数模板
        lastError = err;
        return false;
    }
    return true;
}

/**
 * @brief 获取内部使用的 EAPHeaderBinder 对象
 * @return EAPHeaderBinder&  内部 headerBinder_ 的引用
 */
EAPHeaderBinder& EAPInterfaceManager::headerBinder() {
    return headerBinder_;
}

/**
 * @brief 设置消息日志记录器
 * @param logger 外部创建的消息日志记录器指针
 */
void EAPInterfaceManager::setMessageLogger(EAPMessageLogger* logger) {
    messageLogger_ = logger; // 消息日志记录器 
}

/**
 * @brief 设置数据缓存对象
 * @param cache 外部创建的数据缓存对象指针
 */
void EAPInterfaceManager::setDataCache(EAPDataCache* cache) {
    dataCache_ = cache; // 数据缓存
}

/**
 * @brief 获取最近一次操作的错误信息
 * @return QString 最近记录的错误说明字符串
 */
QString EAPInterfaceManager::getLastError() const {
    return lastError;
}

/**
 * @brief 获取当前配置的基础 URL（base_url）
 * @return QString 当前生效的基础 URL
 */
QString EAPInterfaceManager::getBaseUrl() const {
    return baseUrl;
}

/**
 * @brief 获取当前已配置的接口数量
 * @return int 接口配置条目数量
 */
int EAPInterfaceManager::interfaceCount() const {
    return interfaces.size();
}

/**
 * @brief 获取所有已配置接口的 key 列表
 * @return QStringList 所有接口 key 的字符串列表
 */
QStringList EAPInterfaceManager::getInterfaceKeys() const {
    return interfaces.keys();
}

/**
 * @brief 根据接口 key 获取对应的接口元数据引用
 * @param key 接口 key（如 "CheckUser"、"UploadResult" 等）
 * @return EapInterfaceMeta& 对应接口配置的可修改引用
 */
EapInterfaceMeta& EAPInterfaceManager::getInterface(const QString& key) {
    return interfaces[key];
}

/**
 * @brief 为指定接口组装最终要发送的 JSON 报文 payload
 * @param interfaceKey 接口 key（如 "CheckUser"、"UploadResult" 等）
 * @param params       业务层传入的本地参数集合，用于构建 body、展开占位符等
 * @return QJsonObject 组装好的、经过 header 补充、内部缓存注入以及外壳包装后的 JSON 报文
 */
QJsonObject EAPInterfaceManager::composePayloadForSend(const QString& interfaceKey, const QVariantMap& params) {
    if (!interfaces.contains(interfaceKey)) { // 检查接口是否存在
        return QJsonObject{};
    }

    const EapInterfaceMeta& meta = interfaces.value(interfaceKey);

    // 1) 构建 {header, body}
    QJsonObject hb = JsonBuilder::buildPayload(meta, params);

    // 2) 补充外部 header 参数
    if (meta.enableHeader)
    {
        const QVariantMap headerVals = headerBinder_.mergedParamsFor(interfaceKey, meta, QVariantMap{});
        if (!headerVals.isEmpty()) {
            QJsonObject header = hb.value("header").toObject();
            for (auto it = meta.headerMap.begin(); it != meta.headerMap.end(); ++it) {
                const QString localKey = it.key();
                if (!headerVals.contains(localKey)) continue;
                const QString jsonPath = it.value();
                const QJsonValue val = QJsonValue::fromVariant(headerVals.value(localKey));
                header = setByDotPathObj(header, jsonPath.split('.'), val);
            }
            hb.insert("header", header);
        }
    }

    // 2.5) 新增：根据 internalDBMap 从内部数据缓存读取并注入
    if (dataCache_ && dataCache_->isInitialized() && !meta.internalDBMap.isEmpty()) {
        QJsonObject headerObj = hb.value("header").toObject(); 
        QJsonObject bodyObj = hb.value("body").toObject();

        for (auto it = meta.internalDBMap.begin(); it != meta.internalDBMap.end(); ++it) {
            const QString jsonPath = it.key();       // 目标 JSON 路径（支持 header./body. 前缀）
            const QString readKeyPattern = it.value(); // 读取键模板（可含 {占位符}）

            if (readKeyPattern.trimmed().isEmpty())
                continue;

            QVariant fetched;
            // 当存在占位符时，优先使用带占位符读取
            if (readKeyPattern.contains('{')) {
                fetched = dataCache_->readDataWithPlaceholders(readKeyPattern, params);
            }
            else {
                fetched = dataCache_->readData(readKeyPattern);
            }
            if (!fetched.isValid() || fetched.isNull())
                continue;

            // 注入到 header/body 指定路径；未指明则默认为 body
            QString path = jsonPath.trimmed();
            QJsonValue jv = QJsonValue::fromVariant(fetched);

            if (path.startsWith("header.", Qt::CaseInsensitive)) {
                QStringList parts = path.mid(QStringLiteral("header.").size()).split('.');
                headerObj = setByDotPathObj(headerObj, parts, jv);
            }
            else if (path.startsWith("body.", Qt::CaseInsensitive)) {
                QStringList parts = path.mid(QStringLiteral("body.").size()).split('.');
                bodyObj = setByDotPathObj(bodyObj, parts, jv);
            }
            else {
                // 默认 body
                QStringList parts = path.split('.');
                bodyObj = setByDotPathObj(bodyObj, parts, jv);
            }
        }

        if (meta.enableHeader) hb.insert("header", headerObj);
        if (meta.enableBody)   hb.insert("body", bodyObj);
    }


    // 3) 应用外壳包装（传入 interfaceKey 以支持 per-interface 配置）
    const QJsonObject payload = EAPEnvelope::wrapOutgoing(hb, envelopeCfg, interfaceKey);
    return payload;
}

/**
 * @brief 发送指定接口的 POST 请求（组装 payload、记录日志并带重试）
 * @param interfaceKey 要调用的接口 key（需已在接口配置中存在）
 * @param params       业务层传入的参数集合，用于构造请求 body 及部分 header 字段
 */
void EAPInterfaceManager::post(const QString& interfaceKey, const QVariantMap& params) {
    // 检查接口配置是否存在
    if (!interfaces.contains(interfaceKey)) {
        emit requestFailed(interfaceKey, tr("接口未找到: %1").arg(interfaceKey));
        return;
    }

    // 取接口元数据 ＋ 构造最终 JSON 报文
    const EapInterfaceMeta& meta = interfaces.value(interfaceKey);

    // 构造最终 payload
    const QJsonObject payload = composePayloadForSend(interfaceKey, params); // 为指定接口组装最终要发送的 JSON 报文 payload

    // 写一条“发送记录”到消息日志数据库
    if (messageLogger_ && messageLogger_->isInitialized()) {
        EAPMessageRecord record;
        record.timestamp = QDateTime::currentDateTime();
        record.type = EAPMessageRecord::InterfaceManagerSent;
        record.interfaceKey = interfaceKey;
        record.interfaceDescription = meta.m_interface_description;
        record.payload = payload;
        record.isSuccess = true;
        messageLogger_->insertRecord(record);
    }

    // 写一条文件日志（spdlog）
    LOG_TYPE_DEBUG("MES", "post  [{}]", QJsonDocument(payload).toJson().toStdString().c_str());

    emit requestSent(interfaceKey, payload); // 发出 “已发出请求” 信号

    postWithRetry(interfaceKey, meta, payload, meta.retryCount);
}

/**
 * @brief 发送 HTTP 请求并按配置执行超时控制、自动重试、响应解析与结果缓存
 * @param interfaceKey 当前请求的接口 key
 * @param meta         对应接口的元数据配置（URL、method、headers、重试/缓存策略等）
 * @param payload      由 composePayloadForSend() 组装好的最终请求 JSON
 * @param retriesLeft  剩余重试次数（初始值通常为 meta.retryCount）
 */
void EAPInterfaceManager::postWithRetry(const QString& interfaceKey, const EapInterfaceMeta& meta,
    const QJsonObject& payload, int retriesLeft)
{
    // 优先使用 endpoint，否则拼接 baseUrl + name
    QNetworkRequest request(QUrl((meta.endpoint.isEmpty() ? (baseUrl + meta.name) : meta.endpoint))); // 如果 meta.endpoint 有值：直接用它做完整 URL
    
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    // 添加 per-interface 固定请求头
    for (auto it = meta.headers.begin(); it != meta.headers.end(); ++it) {
        request.setRawHeader(it.key().toUtf8(), it.value().toUtf8());
    }

    // 复用网络管理器，根据 method 选择不同的 HTTP 方法
    QNetworkReply* reply = nullptr;
    QByteArray payloadBytes = QJsonDocument(payload).toJson(QJsonDocument::Compact);
    
    // 根据 method 选择 GET/POST/PUT/DELETE
    const QString method = meta.method.toUpper();
    if (method == "GET") {
        reply = networkManager_->get(request);
    } else if (method == "PUT") {
        reply = networkManager_->put(request, payloadBytes);
    } else if (method == "DELETE") {
        reply = networkManager_->deleteResource(request);
    } else {
        // 默认 POST
        reply = networkManager_->post(request,  payloadBytes);
    }

    // 构建“单次请求”的超时定时器 + 超时标记
    QTimer* timer = new QTimer(this);
    timer->setSingleShot(true);

    //bool* timeoutFlag = new bool(false);
    QSharedPointer<bool> timeoutFlag = QSharedPointer<bool>::create(false);

    // 超时处理逻辑（timeout 回调）
    connect(timer, &QTimer::timeout, this, [=]() {
        if (reply->isRunning()) {
            *timeoutFlag = true;
            reply->abort();
            if (retriesLeft > 0) {
                QTimer::singleShot(100, this, [=]() {
                    postWithRetry(interfaceKey, meta, payload, retriesLeft - 1);
                    });
            }
            else {
                QString errorMsg = tr("请求超时 (%1 ms)").arg(meta.timeoutMs * (meta.retryCount + 1));
                
                // 记录失败的请求
                if (messageLogger_ && messageLogger_->isInitialized()) {
                    EAPMessageRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.type = EAPMessageRecord::InterfaceManagerReceived;
                    record.interfaceKey = interfaceKey;
                    record.interfaceDescription = meta.m_interface_description;
                    record.payload = QJsonObject();
                    record.isSuccess = false;
                    record.errorMessage = errorMsg;
                    messageLogger_->insertRecord(record);
                   
                }
                LOG_TYPE_DEBUG("MES", "post  [{}]", errorMsg.toStdString().c_str());
                emit requestFailed(interfaceKey, errorMsg);
            }
        }
        });

    // 正常完成回调（reply->finished）
    connect(reply, &QNetworkReply::finished, this, [=]() {
        timer->stop();

        if (*timeoutFlag) {
            // 超时已处理（已重试或上报）
        }
        else {
            QByteArray raw = reply->readAll();
            QJsonParseError err{};
            QJsonDocument doc = QJsonDocument::fromJson(raw, &err);

            // 情况 A：响应 JSON 解析失败
            if (err.error != QJsonParseError::NoError || !doc.isObject()) {
                if (retriesLeft > 0) {
                    QTimer::singleShot(100, this, [=]() {
                        postWithRetry(interfaceKey, meta, payload, retriesLeft - 1);
                        });
                }
                else {
                    QString errorMsg = tr("响应解析失败: %1").arg(err.errorString());
                    
                    // 记录失败的响应
                    if (messageLogger_ && messageLogger_->isInitialized()) {
                        EAPMessageRecord record;
                        record.timestamp = QDateTime::currentDateTime();
                        record.type = EAPMessageRecord::InterfaceManagerReceived;
                        record.interfaceKey = interfaceKey;
                        record.interfaceDescription = meta.m_interface_description;
                        record.payload = QJsonObject();
                        record.isSuccess = false;
                        record.errorMessage = errorMsg;
                        messageLogger_->insertRecord(record);
                      
                    }
                    LOG_TYPE_DEBUG("MES", "post  [{}]", errorMsg.toStdString().c_str());
                    emit requestFailed(interfaceKey, errorMsg);
                }
            } // 情况 B：解析成功，记录响应 + 发信号
            else {
                const QJsonObject obj = doc.object();
                
                // 记录接收到的响应
                if (messageLogger_ && messageLogger_->isInitialized()) {
                    EAPMessageRecord record;
                    record.timestamp = QDateTime::currentDateTime();
                    record.type = EAPMessageRecord::InterfaceManagerReceived;
                    record.interfaceKey = interfaceKey;
                    record.interfaceDescription = meta.m_interface_description;
                    record.payload = obj;
                    record.isSuccess = true;
                    messageLogger_->insertRecord(record);
                  
                }
                LOG_TYPE_DEBUG("MES", "response  [{}]", QJsonDocument(obj).toJson().toStdString().c_str());
                emit responseReceived(interfaceKey, obj);

                // 归一化响应：{in_head,in_body} -> {header,body}（传入 interfaceKey）
                const QJsonObject normalized = EAPEnvelope::normalizeIncoming(obj, envelopeCfg, nullptr, nullptr, interfaceKey);

                // 映射两次合并（兼容数组键值对路径）
                QVariantMap parsed1 = JsonBuilder::parseResponse(meta, normalized);
                QVariantMap parsed2 = JsonBuilder::parseResponse(meta, normalized.value("body").toObject());
                for (auto it = parsed2.begin(); it != parsed2.end(); ++it)
                    if (!parsed1.contains(it.key())) parsed1.insert(it.key(), it.value());

                // 检查是否需要基于响应内容重试
                if (retriesLeft > 0 && shouldRetryBasedOnResponse(meta, parsed1)) {
                    // 响应指示需要重试，延迟后重试
                    QTimer::singleShot(100, this, [=]() {
                        postWithRetry(interfaceKey, meta, payload, retriesLeft - 1);
                    });
                    return; // 不继续处理，等待重试
                }

                // 保存到数据缓存（根据 saveToDb 配置）
                //if (dataCache_ && dataCache_->isInitialized() && !meta.saveToDb.isEmpty()) {
                //    // 解析 saveToDb: function_name.db_key
                //    // 从 parsed1 中提取 db_key 的值
                //    int dotPos = meta.saveToDb.indexOf('.');
                //    if (dotPos > 0 && dotPos < meta.saveToDb.length() - 1) {
                //        QString functionName = meta.saveToDb.left(dotPos);
                //        QString dbKeyField = meta.saveToDb.mid(dotPos + 1);
                //        
                //        // 从响应中提取 db_key 的值
                //        if (parsed1.contains(dbKeyField)) {
                //            QString dbKeyValue = parsed1.value(dbKeyField).toString();
                //            if (!dbKeyValue.isEmpty()) {
                //                QString saveKey = QString("%1.%2").arg(functionName, dbKeyValue);
                //                dataCache_->saveData(saveKey, parsed1);
                //            }
                //        }
                //    }
                //}

                // 按 saveToDb 配置保存响应到数据缓存（带占位符）
                {
                    std::lock_guard<std::mutex> lock(cacheMutex_);
                    if (dataCache_ && dataCache_->isInitialized() && !meta.saveToDb.isEmpty()) {
                        // 解析 saveToDb 配置
                        int firstDotPos = meta.saveToDb.indexOf('.');
                        if (firstDotPos > 0 && firstDotPos < meta.saveToDb.length() - 1) {
                            QString functionNameFromConfig = meta.saveToDb.left(firstDotPos);
                            QString dbKeyPattern = meta.saveToDb.mid(firstDotPos + 1);

                            // 展开占位符 {path} 构建 db_key
                            QString dbKeyValue;
                            int pos = 0;
                            while (pos < dbKeyPattern.length()) {
                                int startBrace = dbKeyPattern.indexOf('{', pos);
                                if (startBrace == -1) {
                                    // 没有更多占位符，添加剩余部分
                                    dbKeyValue.append(dbKeyPattern.mid(pos));
                                    break;
                                }

                                // 添加占位符之前的文本
                                if (startBrace > pos) {
                                    dbKeyValue.append(dbKeyPattern.mid(pos, startBrace - pos));
                                }

                                // 查找匹配的 }
                                int endBrace = dbKeyPattern.indexOf('}', startBrace);
                                if (endBrace == -1) {
                                    // 格式错误，终止解析
                                    QString errMsg = QString("Invalid saveToDb placeholder format: missing closing brace in pattern '%1'").arg(meta.saveToDb);
                                    qWarning() << errMsg;
                                    dbKeyValue.clear();
                                    break;
                                }

                                // 提取占位符路径并解析值
                                QString placeholder = dbKeyPattern.mid(startBrace + 1, endBrace - startBrace - 1);
                                //QVariant value = JsonParser::parseJson(meta, normalized, placeholder);
                                QVariant value = JsonParser::resolvePlaceholderValue(normalized, placeholder);
                                // 验证解析的值是否有效
                                if (!value.isValid() || value.isNull()) {
                                    QString warnMsg = QString("Placeholder '%1' in saveToDb pattern '%2' resolved to empty value")
                                        .arg(placeholder, meta.saveToDb);
                                    qWarning() << warnMsg;
                                }
                                else {
                                    // 若返回的是列表，则把元素 join 成字符串（逗号分隔）
                                    if (value.type() == QVariant::List) {
                                        QVariantList list = value.toList();
                                        QStringList strs;
                                        for (const QVariant& x : list) strs << x.toString();
                                        dbKeyValue.append(strs.join(",")); // 或者用其他分隔符
                                    }
                                    else {
                                        dbKeyValue.append(value.toString());
                                    }
                                }
                                //dbKeyValue.append(value.toString());

                                pos = endBrace + 1;
                            }

                            if (!dbKeyValue.isEmpty()) {
                                QStringList rawKeys = dbKeyValue.split(',');
                                QStringList keys;
                                QSet<QString> seen;
                                for (QString k : rawKeys) {
                                    k = k.trimmed();
                                    if (k.isEmpty()) continue;
                                    QString out = k.trimmed();
                                    out.replace(QRegExp(R"([\s/\\]+)"), "_");
                                    k = out.replace(QRegExp(R"([^A-Za-z0-9_.\-])"), "_");

                                    if (k.isEmpty()) continue;
                                    if (seen.contains(k)) continue;
                                    seen.insert(k);
                                    keys.append(k);
                                }

                                if (!keys.isEmpty()) {
                                    // 预先把要保存的数据转换一次（避免在 lambda 中重复转换）
                                    QVariantMap payloadMap = normalized.toVariantMap();
                                    // 捕获 functionNameFromConfig、keys、payloadMap（通过拷贝或 move）
                                    QMetaObject::invokeMethod(this, [this, functionNameFromConfig, keys, payloadMap]() mutable {
                                        std::lock_guard<std::mutex> lock(cacheMutex_);
                                        if (!dataCache_ || !dataCache_->isInitialized()) return;
                                        for (const QString& key : keys) {
                                            const QString saveKey = QString("%1.%2").arg(functionNameFromConfig, key);
                                            dataCache_->saveData(saveKey, payloadMap);
                                        }
                                        }, Qt::QueuedConnection);
                                }

                            }
                        }
                    }
                }

                // 通知上层映射后的结果
                emit mappedResultReady(interfaceKey, parsed1);
            }
        }

        
        reply->deleteLater();
        timer->deleteLater();
        });

    timer->start(meta.timeoutMs);
}

/**
 * @brief 根据接口配置的重试策略检查响应内容是否需要重试
 * @param meta           当前接口的元数据（包含 retryStrategy 配置）
 * @param parsedResponse 通过 JsonBuilder::parseResponse 等方法解析后的响应结果（键值对形式）
 * @return true 表示需要根据响应内容重试；false 表示不需要重试
 */
bool EAPInterfaceManager::shouldRetryBasedOnResponse(const EapInterfaceMeta& meta, const QVariantMap& parsedResponse) const
{
    // 如果没有启用重试策略，返回 false（不重试）
    if (!meta.retryStrategy.enabled || meta.retryStrategy.responseField.isEmpty()) {
        return false;
    }
    
    // 从响应中获取指定字段的值
    QString fieldPath = meta.retryStrategy.responseField;
    QVariant fieldValue;
    
    // 支持简单字段名（如 "needRetry"）和路径（如 "body.needRetry"）
    if (fieldPath.contains('.')) {
        // 路径方式，如 "body.needRetry" 或 "header.result"
        QStringList parts = fieldPath.split('.');
        QVariantMap current = parsedResponse;
        
        for (int i = 0; i < parts.size() - 1; ++i) {
            if (current.contains(parts[i])) {
                QVariant value = current.value(parts[i]);
                if (value.canConvert<QVariantMap>()) {
                    current = value.toMap();
                } else {
                    return false; // 路径无效
                }
            } else {
                return false; // 字段不存在
            }
        }
        
        fieldValue = current.value(parts.last());
    } else { // 简单字段名
        fieldValue = parsedResponse.value(fieldPath);
    }
    
    // 如果字段不存在，不重试
    if (!fieldValue.isValid()) {
        return false;
    }
    
    // 检查是否匹配重试值
    if (meta.retryStrategy.retryValue.isValid()) {
        // 比较值（支持字符串、数字、布尔值）
        if (fieldValue.toString() == meta.retryStrategy.retryValue.toString()) {
            return true; // 需要重试
        }
        // 也支持数值比较
        if (fieldValue.toInt() == meta.retryStrategy.retryValue.toInt() && 
            meta.retryStrategy.retryValue.canConvert<int>()) {
            return true;
        }
    }
    
    // 检查是否匹配不重试值
    if (meta.retryStrategy.noRetryValue.isValid()) {
        if (fieldValue.toString() == meta.retryStrategy.noRetryValue.toString()) {
            return false; // 明确不需要重试
        }
        if (fieldValue.toInt() == meta.retryStrategy.noRetryValue.toInt() && 
            meta.retryStrategy.noRetryValue.canConvert<int>()) {
            return false;
        }
    }
    
    // 默认不重试
    return false;
}