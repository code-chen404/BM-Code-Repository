#include "VendorConfigLoader.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QJsonArray>

#pragma execution_character_set("utf-8")

/**
 * @brief 从 JSON 配置文件加载接口配置及基础 URL
 *
 * 读取指定路径的 JSON 配置文件，并解析为：
 * - 基础访问地址 base_url（写入参数 baseUrl）
 * - 多个接口配置块（写入参数 outMap，键为接口 key，值为 EapInterfaceMeta）
 * @param path    配置文件路径（相对或绝对）
 * @param outMap  输出参数，用于接收解析后的接口配置映射
 * @param baseUrl 输出参数，用于接收配置中的基础 URL（"base_url" 字段）
 * @param error   输出参数，用于返回出错时的错误信息（成功时保持不改或为空）
 * @return true   配置文件读取并解析成功，outMap 与 baseUrl 已填充
 * @return false  打开文件失败或 JSON 解析失败，详细信息保存在 error 中
 */
bool VendorConfigLoader::loadFromFile(const QString& path,
    QMap<QString, EapInterfaceMeta>& outMap,
    QString& baseUrl,
    QString& error) 
{
    // 打开文件
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        error = QObject::tr("无法打开配置文件: %1").arg(path);
        return false;
    }

    // 读取并解析 JSON
    QByteArray content = file.readAll();
    QJsonParseError parseErr;
    QJsonDocument doc = QJsonDocument::fromJson(content, &parseErr);
    if (parseErr.error != QJsonParseError::NoError || !doc.isObject()) { // JSON 语法错误，或者解析结果不是一个 JSON 对象（根不是 { ... }）
        error = QObject::tr("配置解析失败: %1").arg(parseErr.errorString());
        return false;
    }

    QJsonObject root = doc.object(); // 取出根对象 root
    outMap.clear();
    baseUrl.clear();

    // 遍历根对象的所有键值
    for (auto it = root.begin(); it != root.end(); ++it) {
        const QString key = it.key();
        const QJsonValue val = it.value();

        if (key == "base_url") {
            baseUrl = val.toString();
            continue;
        }

        if (!val.isObject()) continue;

        const QJsonObject obj = val.toObject(); // obj 对应 JSON 里的某个接口配置对象
        EapInterfaceMeta meta;

        meta.name = obj.value("name").toString();
        meta.method = obj.value("method").toString("POST").toUpper();
        meta.direction = obj.value("direction").toString();
        meta.m_interface_description = obj.value("description").toString();
        meta.enabled = obj.value("enabled").toBool(true);
        meta.enableHeader = obj.value("enableHeader").toBool(true);
        meta.enableBody = obj.value("enableBody").toBool(true);
        meta.timeoutMs = obj.value("timeoutMs").toInt(5000); // 默认 5000ms
        meta.retryCount = obj.value("retryCount").toInt(0); // 默认 0 次重试

        // 定义一个解析小工具
        const auto parseMap = [](const QJsonObject& o) {
            QMap<QString, QString> map;
            for (auto it = o.begin(); it != o.end(); ++it)
                map[it.key()] = it.value().toString();
            return map;
            };

        // 用解析小工具解析 mapping
        meta.headerMap = parseMap(obj.value("header_mapping").toObject());
        meta.bodyMap = parseMap(obj.value("body_mapping").toObject());
        meta.responseMap = parseMap(obj.value("response_mapping").toObject());
        meta.saveToDb = (obj.value("saveToDb").toString());

        // === 新增：读取 internal_db_map / internalDBMap ===
        if (obj.contains("internal_db_map") && obj.value("internal_db_map").isObject()) {
            meta.internalDBMap = parseMap(obj.value("internal_db_map").toObject());
        }
        else if (obj.contains("internalDBMap") && obj.value("internalDBMap").isObject()) {
            meta.internalDBMap = parseMap(obj.value("internalDBMap").toObject());
        }

        // === 解析新增可选字段 ===
        if (obj.contains("endpoint")) {
            meta.endpoint = obj.value("endpoint").toString();
        }
        if (obj.contains("headers") && obj.value("headers").isObject()) {
            meta.headers = parseMap(obj.value("headers").toObject());
        }
        if (obj.contains("success_policy") && obj.value("success_policy").isObject()) {
            const QJsonObject sp = obj.value("success_policy").toObject();
            meta.successPolicy.type = sp.value("type").toString("equals");
            meta.successPolicy.path = sp.value("path").toString();
            meta.successPolicy.expected = sp.value("expected").toVariant();
        }
        if (obj.contains("rate_limit") && obj.value("rate_limit").isObject()) {
            const QJsonObject rl = obj.value("rate_limit").toObject();
            meta.rateLimit.rpm = rl.value("rpm").toInt(0);
            meta.rateLimit.burst = rl.value("burst").toInt(0);
        }
        if (obj.contains("auth") && obj.value("auth").isObject()) {
            meta.auth = obj.value("auth").toObject().toVariantMap();
        }
        if (obj.contains("enable_raw_injection")) {
            meta.enableRawInjection = obj.value("enable_raw_injection").toBool(false);
        }

        // === 解析重试策略 ===  （retry_strategy 用来描述“看响应内容决定要不要重试”的策略）
        if (obj.contains("retry_strategy") && obj.value("retry_strategy").isObject()) {
            const QJsonObject rs = obj.value("retry_strategy").toObject();
            meta.retryStrategy.enabled = rs.value("enabled").toBool(false);
            meta.retryStrategy.responseField = rs.value("response_field").toString();
            meta.retryStrategy.retryValue = rs.value("retry_value").toVariant();
            meta.retryStrategy.noRetryValue = rs.value("no_retry_value").toVariant();
        }

        outMap[key] = meta;
    }

    return true;
}

/**
 * @brief 根据接口键解析得到最终生效的接口配置（合并默认配置）
 *
 * 从给定的接口配置映射 interfaces 中查找指定接口键 interfaceKey 对应的
 * EapInterfaceMeta；若存在，则在此基础上与可选的默认配置 defaultMeta
 * 进行“深度合并”，生成一份最终可用的接口配置并返回。
 *
 * @param interfaceKey 要解析的接口键（如 "CheckUser"）
 * @param interfaces   所有接口配置的映射（键为接口键，值为 EapInterfaceMeta）
 * @param defaultMeta  可选的默认配置指针，用于补充未在接口中显式配置的字段，
 *                     若为 nullptr 则不进行默认合并
 * @return EapInterfaceMeta  解析并合并后的最终接口配置
 */
EapInterfaceMeta VendorConfigLoader::resolveConfig(
    const QString& interfaceKey,
    const QMap<QString, EapInterfaceMeta>& interfaces,
    const EapInterfaceMeta* defaultMeta)
{
    // 获取接口配置，不存在则返回空配置
    if (!interfaces.contains(interfaceKey)) {
        return defaultMeta ? *defaultMeta : EapInterfaceMeta{};
    }

    EapInterfaceMeta resolved = interfaces.value(interfaceKey);

    // 如果有 default 配置，进行深度合并（default 作为基础，interface 覆盖）
    if (defaultMeta) {
        // 合并 headers：default + interface
        for (auto it = defaultMeta->headers.begin(); it != defaultMeta->headers.end(); ++it) {
            if (!resolved.headers.contains(it.key())) {
                resolved.headers.insert(it.key(), it.value());
            }
        }

        // 合并 headerMap
        for (auto it = defaultMeta->headerMap.begin(); it != defaultMeta->headerMap.end(); ++it) {
            if (!resolved.headerMap.contains(it.key())) {
                resolved.headerMap.insert(it.key(), it.value());
            }
        }

        // 合并 bodyMap
        for (auto it = defaultMeta->bodyMap.begin(); it != defaultMeta->bodyMap.end(); ++it) {
            if (!resolved.bodyMap.contains(it.key())) {
                resolved.bodyMap.insert(it.key(), it.value());
            }
        }

        // 合并 responseMap
        for (auto it = defaultMeta->responseMap.begin(); it != defaultMeta->responseMap.end(); ++it) {
            if (!resolved.responseMap.contains(it.key())) {
                resolved.responseMap.insert(it.key(), it.value());
            }
        }

        // 合并 internalDBMap（新增）
        for (auto it = defaultMeta->internalDBMap.begin(); it != defaultMeta->internalDBMap.end(); ++it) {
            if (!resolved.internalDBMap.contains(it.key())) {
                resolved.internalDBMap.insert(it.key(), it.value());
            }
        }

        // 如果接口未定义 success_policy，使用 default
        if (resolved.successPolicy.type.isEmpty() && !defaultMeta->successPolicy.type.isEmpty()) {
            resolved.successPolicy = defaultMeta->successPolicy;
        }

        // 如果接口未定义 rate_limit，使用 default
        if (resolved.rateLimit.rpm == 0 && defaultMeta->rateLimit.rpm > 0) {
            resolved.rateLimit = defaultMeta->rateLimit;
        }

        // 如果接口未定义 auth，使用 default
        if (resolved.auth.isEmpty() && !defaultMeta->auth.isEmpty()) {
            resolved.auth = defaultMeta->auth;
        }
    }

    return resolved;
}