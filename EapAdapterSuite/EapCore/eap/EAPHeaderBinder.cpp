#include "EAPHeaderBinder.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QUuid>
#include <QProcessEnvironment>

/**
 * @brief 按指定格式返回当前时间字符串
 * @param fmt 时间格式字符串（如 "yyyyMMddHHmmss"），为空则使用默认格式
 * @return 格式化后的当前时间字符串
 */
static QString expandNow(const QString& fmt) {
    return QDateTime::currentDateTime().toString(fmt.isEmpty() ? "yyyy-MM-dd HH:mm:ss" : fmt);
}

/**
 * @brief 从 JSON 配置文件加载 EAP 请求 header 参数模板
 *
 * 从指定路径的 JSON 文件中读取 header 参数配置：
 * - "default" 段会被保存到成员 defaultHeader_，作为全局默认 header 字段集合；
 * - "interfaces" 段中每个接口对应的对象会被保存到 perInterfaceHeader_，用于覆盖或扩展默认 header。
 * 若文件无法打开或 JSON 解析失败，则返回 false，并在 errorOut 中给出错误信息。
 *
 * @param path     header 参数配置文件路径（JSON 格式）
 * @param errorOut 若非空，在加载失败时写入错误描述字符串
 * @return true  加载成功，defaultHeader_ / perInterfaceHeader_ 已更新
 * 
 * 无引用--R
 */
bool EAPHeaderBinder::loadFromFile(const QString& path, QString* errorOut) {
    // 打开文件
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (errorOut) *errorOut = QStringLiteral("无法打开 header 参数文件: %1").arg(path);
        return false;
    }

    // 解析 JSON
    QJsonParseError jerr{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &jerr);
    if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
        if (errorOut) *errorOut = QStringLiteral("header 参数 JSON 解析失败: %1").arg(jerr.errorString());
        return false;
    }

    defaultHeader_.clear();
    perInterfaceHeader_.clear();

    // 读取 default 段（全局 header 模板）
    const QJsonObject root = doc.object();
    if (root.contains("default") && root.value("default").isObject()) {
        defaultHeader_ = root.value("default").toObject().toVariantMap();
    }

    // 读取 interfaces 段（各接口单独 header 模板）
    if (root.contains("interfaces") && root.value("interfaces").isObject()) {
        const auto m = root.value("interfaces").toObject();
        for (auto it = m.begin(); it != m.end(); ++it) {
            if (!it.value().isObject()) continue;
            perInterfaceHeader_.insert(it.key(), it.value().toObject().toVariantMap());
        }
    }
    return true;
}

/**
 * @brief 展开 header 模板字符串中的占位符，生成最终可发送的字符串值
 *
 * 支持在配置文件中通过特殊占位符动态生成字段内容，例如：
 * - ${uuid}              ：替换为随机 UUID（无花括号）
 * - ${name}              ：替换为接口配置中的 meta.name
 * - ${nameNoSlash}       ：替换为去掉前导 '/' 的接口名（若有）
 * - ${now:fmt}           ：按给定时间格式 fmt 展开当前时间，如 ${now:yyyyMMddHHmmss}
 * - ${env:VAR}           ：从系统环境变量 VAR 中读取值
 * - ${ts:epoch_ms}       ：当前时间戳（毫秒，自 1970 起）
 * - ${ts:epoch_s}        ：当前时间戳（秒，自 1970 起）
 * - ${custom:Provider}   ：调用自定义 provider（customProviders_ 中注册）生成值
 *
 * 函数会在传入的字符串 s 上依次执行以上占位符替换，并返回替换后的结果字符串（封装在 QVariant 中）。
 * 若字符串不包含任何占位符，则原样返回。
 *
 * @param s            原始模板字符串（可能包含 ${...} 占位符）
 * @param interfaceKey 当前接口 key，用于传入自定义 provider（如 "CheckUser"）
 * @param meta         当前接口的元数据（EapInterfaceMeta，用于 ${name}/${nameNoSlash} 等）
 * @return QVariant    展开占位符后的最终字符串值
 */
QVariant EAPHeaderBinder::expandValue(const QString& s,
    const QString& interfaceKey,
    const EapInterfaceMeta& meta) const {
    // 仅处理简单字符串占位符，其他类型原样返回
    QString out = s;

    // ${uuid}--新生成的 UUID
    out.replace("${uuid}", QUuid::createUuid().toString(QUuid::WithoutBraces));

    // ${name} / ${nameNoSlash}--使用接口名
    out.replace("${name}", meta.name);
    {
        QString ns = meta.name;
        if (ns.startsWith('/')) ns.remove(0, 1);
        out.replace("${nameNoSlash}", ns);
    }

    // ${now:...}--当前时间按指定格式
    // 形如 ${now:yyyy-MM-dd HH:mm:ss}
    int idx = 0;
    while ((idx = out.indexOf("${now:", idx)) >= 0) {
        int end = out.indexOf('}', idx);
        if (end < 0) break;
        const QString token = out.mid(idx, end - idx + 1);
        const QString fmt = token.mid(QString("${now:").size(),
            token.size() - QString("${now:").size() - 1);
        out.replace(token, expandNow(fmt));
        idx = idx + 1;
    }

    // ${env:XXX} -- 读取系统环境变量
    idx = 0;
    while ((idx = out.indexOf("${env:", idx)) >= 0) {
        int end = out.indexOf('}', idx);
        if (end < 0) break;
        const QString token = out.mid(idx, end - idx + 1);
        const QString envName = token.mid(QString("${env:").size(),
            token.size() - QString("${env:").size() - 1);
        const QString envValue = QProcessEnvironment::systemEnvironment().value(envName, "");
        out.replace(token, envValue);
        idx = idx + 1;
    }

    // ${ts:epoch_ms} -- 当前时间戳（毫秒）
    out.replace("${ts:epoch_ms}", QString::number(QDateTime::currentMSecsSinceEpoch()));
    
    // ${ts:epoch_s} -- 当前时间戳（秒）
    out.replace("${ts:epoch_s}", QString::number(QDateTime::currentSecsSinceEpoch()));

    // ${custom:XXX} -- 自定义占位符
    idx = 0;
    while ((idx = out.indexOf("${custom:", idx)) >= 0) {
        int end = out.indexOf('}', idx);
        if (end < 0) break;
        const QString token = out.mid(idx, end - idx + 1);
        const QString providerName = token.mid(QString("${custom:").size(),
            token.size() - QString("${custom:").size() - 1);
        if (customProviders_.contains(providerName)) {
            const QString customValue = customProviders_.value(providerName)(interfaceKey, meta);
            out.replace(token, customValue);
        }
        idx = idx + 1;
    }

    return out;
}

/**
 * @brief 注册自定义占位符提供者（用于 ${custom:XXX} 展开）
 * @param name     占位符提供者名称，对应占位符中的 XXX（如 "sign" 对应 ${custom:sign}）
 * @param callback 回调函数，签名为 QString(const QString& interfaceKey,
 *                 const EapInterfaceMeta& meta)，用于生成实际替换值
 */
void EAPHeaderBinder::registerPlaceholderProvider(const QString& name, PlaceholderProvider callback) {
    customProviders_[name] = callback;
}

/**
 * @brief 生成指定接口最终要使用的 header 参数集合
 * @param interfaceKey 当前接口 key（如 "CheckUser"），用于选择接口级 header 和占位符展开
 * @param meta         当前接口的元数据（包含 headerMap、name 等信息）
 * @param inputParams  调用方传入的初始 header 参数（优先级最高，不会被配置覆盖）
 * @return QVariantMap 合并并展开占位符后的 header 参数集合
 * 
 * 1. expandValue
 */
QVariantMap EAPHeaderBinder::mergedParamsFor(const QString& interfaceKey,
    const EapInterfaceMeta& meta,
    const QVariantMap& inputParams) const 
{
    // 仅针对 meta.headerMap 中的本地字段进行合并
    QVariantMap out = inputParams;

    // 先拿 default
    QVariantMap merged = defaultHeader_;
    // 覆盖接口定制
    if (perInterfaceHeader_.contains(interfaceKey)) {
        for (auto it = perInterfaceHeader_[interfaceKey].begin();
            it != perInterfaceHeader_[interfaceKey].end(); ++it) {
            merged.insert(it.key(), it.value());
        }
    }

    // 展开占位符，并仅在 headerMapping 中存在时写入
    for (auto it = meta.headerMap.begin(); it != meta.headerMap.end(); ++it) {
        const QString localKey = it.key(); // 本地 header 字段
        if (out.contains(localKey)) continue; // 已传入则不覆盖
        if (!merged.contains(localKey)) continue;

        const QVariant v = merged.value(localKey);
        if (v.userType() == QMetaType::QString) {
            out.insert(localKey, expandValue(v.toString(), interfaceKey, meta));
        }
        else {
            out.insert(localKey, v);
        }
    }

    return out;
}