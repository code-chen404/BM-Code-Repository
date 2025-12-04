#pragma once

#include <QString>
#include <QMap>
#include <QVariant>
/**
 * 成功判定策略
 */
struct SuccessPolicy {
    QString type;       // "equals" | "regex" | "code_in" | "always"
    QString path;       // JSON 路径，如 "response_head.result" 或 "header.result"
    QVariant expected;  // 期望值：字符串、整数或数组
};

/**
 * 限流配置
 */
struct RateLimit {
    int rpm = 0;        // 每分钟请求数
    int burst = 0;      // 突发容量
}; 

/**
 * 重试策略配置
 * 根据响应中的特定字段值决定是否需要重试
 */
struct RetryStrategy {
    QString responseField;           // 响应中用于判断的字段路径（如 "body.needRetry" 或 "header.result"）
    QVariant retryValue;            // 触发重试的值（如 "1", "true", "retry"）
    QVariant noRetryValue;          // 不需要重试的值（如 "0", "false", "success"）
    bool enabled = false;           // 是否启用基于响应的重试策略
};

/**
 * 描述一个 WebAPI 接口的结构配置
 */
struct EapInterfaceMeta {
    QString name;                         // 接口路径，如 /postAreYouThereRequest
    QString method;                       // 请求方法，POST/GET/PUT/DELETE
    QString direction;                    // 数据方向，如 push / pull
    QString m_interface_description;      // 接口功能描述（如"查询用户信息"、"上传凭证"）
    bool enabled = true;                  // 是否启用此接口
    bool enableHeader = true;             // 是否启用 header
    bool enableBody = true;               // 是否启用 body
    int timeoutMs = 5000;                 // 默认超时：5000 ms
    int retryCount = 0;                   // 默认不重试

    QMap<QString, QString> headerMap;     // 本地字段 → JSON header 映射
    QMap<QString, QString> bodyMap;       // 本地字段 → JSON body 映射
    QMap<QString, QString> responseMap;   // JSON返回字段 → 本地字段 映射

    // === 新增可选字段（向后兼容） ===
    QString endpoint;                     // 完整 URL，优先于 base_url + name
    QMap<QString, QString> headers;       // 固定请求头（HTTP headers）
    SuccessPolicy successPolicy;          // 成功判定策略
    RateLimit rateLimit;                  // 限流配置
    QVariantMap auth;                     // 认证配置扩展点
    bool enableRawInjection = false;      // 是否启用 @raw 注入（body_mapping）

    // === 重试策略配置 ===
    RetryStrategy retryStrategy;          // 基于响应内容的重试策略

    // === 数据持久化配置 ===
    QString saveToDb;                     // 数据库持久化配置，格式：function_name.db_key
    // 如果配置了此字段，响应数据会按此唯一键存储到数据库
    // responseMap 中可使用 function_name.db_key.field_name 格式从数据库读取

// === 新增：内部数据注入映射 ===
// 键：目标 JSON 路径（支持以 "body." 或 "header." 开头，未指定则默认 body）
// 值：读取键模板，格式：function_name.db_key 或 function_name.{占位符}.[field.path]
//     当包含 {占位符} 时，将使用出网参数 params 中同名键替换后再读取。
    QMap<QString, QString> internalDBMap; // json 字段 -> 数据库名.{数据库唯一键}
};