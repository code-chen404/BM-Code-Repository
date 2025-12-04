#include "EAPWebService.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QMetaObject>
#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QDebug>
#include <atomic>
#include <thread>
#include <future>
#include <mutex>

#include "VendorConfigLoader.h"
#include "JsonBuilder.h"
#include "EAPMessageLogger.h"
#include "EAPMessageRecord.h"
#include "EAPDataCache.h"

// cpp-httplib
#include "JsonParser.h"
#include "LoggerInterface.h"
#include "third_party/cpp-httplib/httplib.h"

namespace {
    inline std::string toStdString(const QByteArray& ba) { // 把 QByteArray 转成 std::string
        return std::string(ba.constData(), static_cast<size_t>(ba.size()));
    }
    inline std::string jsonString(const QJsonObject& obj) { // 把 QJsonObject 包装成 QJsonDocument
        return toStdString(QJsonDocument(obj).toJson(QJsonDocument::Compact));
    }
} // namespace

struct EAPWebService::Impl {
    // 1.接口配置
    QMap<QString, EapInterfaceMeta> interfaces; // 描述每个 EAP 接口的元数据（地址、body 映射等）
    QString baseUrl; //MES 或 HTTP 服务的基础 URL
    QString err; // 最近一次错误信息

    // 2.外壳键名配置
    EAPEnvelope::Config envelopeCfg; // 

    // 3.运行期对象
    std::unique_ptr<httplib::Server> server; // httplib::Server 对象（HTTP 监听服务）
    std::thread worker; // 运行 server 的工作线程
    std::atomic_bool running{ false }; // 原子 bool，表示服务是否在运行
    QString host = QStringLiteral("0.0.0.0"); // 监听地址与端口（默认 0.0.0.0:8026）
    quint16 port = 8026; 
    size_t payloadMax = 0; // 最大负载大小（用来限制请求 body 大小）

    int readTimeoutMs = 0; // HTTP 读/写的超时时间、空闲轮询间隔
    int writeTimeoutMs = 0;
    int idleIntervalMs = 0;

    // 4.行为开关
    bool onlyPush = false; // 是否只允许被动推送（例如只允许 MES 调用你，不对外发出请求）
    bool caseInsensitiveMatch = true; // 是否忽略大小写地匹配函数名（接口名）
    bool strictHeadFunctionMatch = true; // 与 envelopeCfg.strictMatch 一起控制
    bool autoStopOnAppQuit = true; //应用退出时是否自动停止 HTTP 服务

    QStringList allowList; // 允许访问的函数名白名单
    QSet<QString> allowListL; // 对应的小写版本集合，方便做不区分大小写的检查

    // 5.回调与超时
    std::function<QJsonObject(const QString&, const QJsonObject&, const QVariantMap&)> rawResponder; // 原始 JSON 回调，比如直接处理 MES 原始 body
    std::function<QVariantMap(const QString&, const QJsonObject&, const QVariantMap&)> mappedResponder; // 映射后的业务回调，可能会用 JsonBuilder 之类先把 body 解析成 QVariantMap 再给业务层
    int responderTimeoutMs = 0; // 回调执行超时时间，配合下面的 runWithTimeout 使用

    // 6.索引
    QMap<QString, QString> fnToKeyExact; // 函数名精确匹配表
    QMap<QString, QString> fnToKeyLower; // 小写版索引

    // 7.=== P0: 线程安全 ===
    mutable std::mutex configMutex_;      // 保护配置数据（interfaces, baseUrl, envelopeCfg 等）
    mutable std::mutex callbackMutex_;    // 保护回调函数（rawResponder, mappedResponder）
    mutable std::mutex indexMutex_;       // 保护索引数据（fnToKeyExact, fnToKeyLower）

    // 8.消息日志记录器
    EAPMessageLogger* messageLogger_ = nullptr; // 消息日志记录器（例如记录入/出站 JSON）
    mutable std::mutex loggerMutex_;      // 保护日志记录器

    // 9.数据缓存
    EAPDataCache* dataCache_ = nullptr; // 数据缓存组件
    mutable std::mutex cacheMutex_; // 保护数据缓存

    // 不区分大小写的前缀判断
    static bool startsWithInsensitive(const std::string& s, const char* prefix) {
        size_t n = strlen(prefix);
        if (s.size() < n) return false;
        for (size_t i = 0; i < n; ++i) {
            char a = static_cast<char>(::tolower(static_cast<unsigned char>(s[i])));
            char b = static_cast<char>(::tolower(static_cast<unsigned char>(prefix[i])));
            if (a != b) return false;
        }
        return true;
    }

    // 判断 Content-Type 是否 JSON
    static bool isJsonContentType(const std::string& v) {
        return startsWithInsensitive(v, "application/json");
    }

    // 这个函数名是否被允许调用、并且把它对应到内部真正的接口 key 上
    std::pair<bool, QString> resolveKey(const QString& functionName) const {
        std::lock_guard<std::mutex> lock(indexMutex_);
        if (!allowList.isEmpty()) {
            if (caseInsensitiveMatch) {
                if (!allowListL.contains(functionName.toLower()))
                    return { false, QString() };
            }
            else {
                if (!allowList.contains(functionName))
                    return { false, QString() };
            }
        }
        if (caseInsensitiveMatch) {
            auto it = fnToKeyLower.find(functionName.toLower());
            if (it != fnToKeyLower.end())
                return { true, it.value() };
        }
        else {
            auto it = fnToKeyExact.find(functionName);
            if (it != fnToKeyExact.end())
                return { true, it.value() };
        }
        
        // 需要访问 interfaces，锁定 configMutex_
        std::lock_guard<std::mutex> configLock(configMutex_);
        if (interfaces.contains(functionName))
            return { true, functionName };
        return { false, QString() };
    }

    // 找到真正的接口 key，并配合白名单做访问控制
    void rebuildFnIndex() {
        std::lock_guard<std::mutex> indexLock(indexMutex_);
        std::lock_guard<std::mutex> configLock(configMutex_);
        
        fnToKeyExact.clear();
        fnToKeyLower.clear();
        for (auto it = interfaces.begin(); it != interfaces.end(); ++it) {
            const QString key = it.key();
            const EapInterfaceMeta& meta = it.value();
            fnToKeyExact.insert(key, key);
            fnToKeyLower.insert(key.toLower(), key);
            QString fn = meta.name;
            if (fn.startsWith('/')) fn.remove(0, 1);
            if (!fn.isEmpty()) {
                fnToKeyExact.insert(fn, key);
                fnToKeyLower.insert(fn.toLower(), key);
            }
        }
        allowListL.clear();
        for (const auto& f : allowList) 
            allowListL.insert(f.toLower());
    }

    // 在限定时间内执行一个函数 f，如果按时执行完就返回结果
    template <typename T, typename Func>
    std::pair<bool, T> runWithTimeout(Func f, int timeoutMs) {
        try {
            if (timeoutMs <= 0) {
                T r = f();
                return { true, std::move(r) };
            }
            auto fut = std::async(std::launch::async, std::move(f));
            if (fut.wait_for(std::chrono::milliseconds(timeoutMs)) == std::future_status::ready) {
                return { true, fut.get() };
            }
            return { false, T{} };
        }
        catch (...) {
            return { false, T{} };
        }
    }
};

EAPWebService::EAPWebService(QObject* parent)
    : QObject(parent), d(new Impl) {
}

EAPWebService::~EAPWebService() {
    stop();
}

/**
 * @brief 从配置文件加载接口配置并应用到 EAPWebService
 * @param configPath 接口配置文件路径（例如 JSON 配置文件）
 * @return true 表示加载并应用成功；false 表示加载失败（错误信息会记录到 d->err 中）
 */
bool EAPWebService::loadInterfaceConfig(const QString& configPath) {
    QMap<QString, EapInterfaceMeta> map;
    QString url, err;
    if (!VendorConfigLoader::loadFromFile(configPath, map, url, err)) {
        std::lock_guard<std::mutex> lock(d->configMutex_);
        d->err = err;
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(d->configMutex_);
        d->interfaces = map;
        d->baseUrl = url;
        d->err.clear();
    }
    
    d->rebuildFnIndex();
    return true;
}

/**
 * @brief 判断当前 EAPWebService 配置是否有效
 * @return true 表示当前服务具备基本可用的配置；false 表示尚未配置或配置不完整
 */
bool EAPWebService::isValid() const
{
    std::lock_guard<std::mutex> lock(d->configMutex_);
    // 修复逻辑：配置有效应该是 baseUrl 不为空或至少有一个接口
    return !d->baseUrl.isEmpty() || !d->interfaces.isEmpty();
}

/**
 * @brief 从策略文件加载 Envelope 配置（外壳/head/body 字段策略）,
 * 加载成功后，后续请求/响应的封装与解析将按照新的 envelopeCfg（如 header/body 键名、
 * messagename/timestamp 字段名及严格匹配策略等）来处理。
 * @param policyPath Envelope 策略配置文件路径（例如 JSON 文件）
 * @param errorOut 可选的错误输出指针；当加载失败时，会将错误描述写入此参数
 * @return true 表示加载成功并已应用到当前服务；false 表示加载失败
 */
bool EAPWebService::loadEnvelopePolicy(const QString& policyPath, QString* errorOut) {
    EAPEnvelope::Config newCfg;
    QString err;
    if (!EAPEnvelope::loadConfigFromFile(policyPath, newCfg, &err)) {
        std::lock_guard<std::mutex> lock(d->configMutex_);
        d->err = err;
        if (errorOut) *errorOut = err;
        return false;
    }
    
    {
        std::lock_guard<std::mutex> lock(d->configMutex_);
        d->envelopeCfg = newCfg;
    }
    return true;
}

/**
 * @brief 设置是否只允许“推送方向”的交互（仅被动接收调用）
 * @param on true 表示仅允许对端推数据到本服务（onlyPush 模式），
 *           false 表示允许正常双向（包括本服务主动调用对端接口）的行为。
 */
void EAPWebService::setAllowOnlyPushDirection(bool on) {
    d->onlyPush = on;
}

/**
 * @brief 设置允许访问的函数名白名单
 * @param functions 允许被访问的函数名列表（可为接口 key 或函数名）
 */
void EAPWebService::setAllowedFunctions(const QStringList& functions) {
    d->allowList = functions;
    d->allowListL.clear();
    for (const auto& f : functions) d->allowListL.insert(f.toLower());
}

/**
 * @brief 设置函数名匹配时是否不区分大小写
 * @param on true 表示不区分大小写（使用 fnToKeyLower / allowListL），
 *           false 表示区分大小写（使用 fnToKeyExact / allowList）。
 */
void EAPWebService::setCaseInsensitiveFunctionMatch(bool on) {
    d->caseInsensitiveMatch = on;
}

/**
 * @brief 设置是否严格按照报文头中的函数名进行匹配
 * @param on true 表示启用严格 head 函数名匹配（通常与 envelopeCfg.strictMatch 配合使用），
 *           false 表示使用相对宽松的匹配策略。
 */
void EAPWebService::setStrictHeadFunctionMatch(bool on) {
    d->strictHeadFunctionMatch = on;
}

/**
 * @brief 设置 HTTP 请求体（payload）的最大允许字节数
 * @param bytes 最大字节数上限；超过该大小的请求将被 httplib 拒绝或视为错误
 */
void EAPWebService::setMaxPayloadBytes(size_t bytes) {
    d->payloadMax = bytes;
    if (d->server) d->server->set_payload_max_length(bytes);
}

/**
 * @brief 设置 HTTP 服务器的网络超时时间参数
 * @param readTimeoutMs  读取请求数据的超时时间（毫秒）。>0 时生效，<=0 表示不设置读超时
 * @param writeTimeoutMs 写入响应数据的超时时间（毫秒）。>0 时生效，<=0 表示不设置写超时
 * @param idleIntervalMs 空闲检测（keep-alive 检查）间隔时间（毫秒）。>0 时生效，<=0 表示不设置空闲检测
 */
void EAPWebService::setNetworkTimeouts(int readTimeoutMs, int writeTimeoutMs, int idleIntervalMs) {
    d->readTimeoutMs = readTimeoutMs;
    d->writeTimeoutMs = writeTimeoutMs;
    d->idleIntervalMs = idleIntervalMs;
    if (d->server) {
        if (d->readTimeoutMs > 0)
            d->server->set_read_timeout(d->readTimeoutMs / 1000, (d->readTimeoutMs % 1000) * 1000);
        if (d->writeTimeoutMs > 0)
            d->server->set_write_timeout(d->writeTimeoutMs / 1000, (d->writeTimeoutMs % 1000) * 1000);
        if (d->idleIntervalMs > 0)
            d->server->set_idle_interval(d->idleIntervalMs / 1000, (d->idleIntervalMs % 1000) * 1000);
        d->server->set_tcp_nodelay(true);
    }
}

/**
 * @brief 设置应用退出时是否自动停止 Web 服务
 * @param on true 表示在应用程序退出时自动停止/清理内部 HTTP 服务，
 *           false 表示需要调用方自行控制停止时机。
 */
void EAPWebService::setAutoStopOnAppQuit(bool on) {
    d->autoStopOnAppQuit = on;
}

/**
 * @brief 注册“原始 JSON 模式”的业务处理回调
 * @param cb 回调函数，签名为 QJsonObject(const QString& fnName,
 *                                     const QJsonObject& requestJson,
 *                                     const QVariantMap& context)
 */
void EAPWebService::setRawResponder(std::function<QJsonObject(const QString&, const QJsonObject&, const QVariantMap&)> cb) {
    std::lock_guard<std::mutex> lock(d->callbackMutex_);
    d->rawResponder = std::move(cb);
}

/**
 * @brief 注册“映射模式”的业务处理回调
 * @param cb 回调函数，签名为 QVariantMap(const QString& fnName,
 *                                         const QJsonObject& requestJson,
 *                                         const QVariantMap& context)
 */
void EAPWebService::setMappedResponder(std::function<QVariantMap(const QString&, const QJsonObject&, const QVariantMap&)> cb) {
    std::lock_guard<std::mutex> lock(d->callbackMutex_);
    d->mappedResponder = std::move(cb);
}

/**
 * @brief 设置业务回调执行的超时时间
 * @param timeoutMs 超时时间（毫秒）。<=0 表示不启用超时控制
 */
void EAPWebService::setResponderTimeoutMs(int timeoutMs) {
    std::lock_guard<std::mutex> lock(d->callbackMutex_);
    d->responderTimeoutMs = timeoutMs;
}

/**
 * @brief 获取最近一次记录的错误信息
 * @return 最近一次配置/策略加载等操作写入的错误字符串；若无错误则通常为空字符串
 */
QString EAPWebService::lastError() const {
    std::lock_guard<std::mutex> lock(d->configMutex_);
    return d->err;
}

/**
 * @brief 根据函数名或接口 key 获取对应的接口元数据
 * @param functionOrKey 可传入：
 *        - 接口配置中的 key（例如 "panel_info"），或
 *        - 映射后的函数名/路径（例如 "getPanelInfo"、"GETPANELINFO" 等）
 * @return 若能在当前配置中找到对应的 EapInterfaceMeta，则返回其指针；
 *         若找不到，则返回 nullptr。
 */
const EapInterfaceMeta* EAPWebService::getMeta(const QString& functionOrKey) const
{
    std::lock_guard<std::mutex> lock(d->configMutex_);
    auto it = d->interfaces.constFind(functionOrKey);
    if (it != d->interfaces.constEnd()) return &it.value();

    const auto pair = d->resolveKey(functionOrKey);
    if (!pair.first) return nullptr;
    it = d->interfaces.constFind(pair.second);
    if (it == d->interfaces.constEnd()) return nullptr;
    return &it.value();
}

/**
 * @brief 为 EAPWebService 设置消息日志记录器
 * @param logger 外部提供的 EAPMessageLogger 指针（生命周期由调用方管理）
 */
void EAPWebService::setMessageLogger(EAPMessageLogger* logger)
{
    std::lock_guard<std::mutex> lock(d->loggerMutex_);
    d->messageLogger_ = logger;
}

/**
 * @brief 为 EAPWebService 设置数据缓存对象
 * @param cache 外部提供的 EAPDataCache 指针（生命周期由调用方管理）
 */
void EAPWebService::setDataCache(EAPDataCache* cache)
{
    std::lock_guard<std::mutex> lock(d->cacheMutex_);
    d->dataCache_ = cache;
}

/**
 * @brief 停止内部 HTTP 服务并清理相关资源
 */
void EAPWebService::stop() {
    if (!d->server) return;
    d->server->stop();
    if (d->worker.joinable()) d->worker.join();
    d->server.reset();
    d->running.store(false);
}

/**
 * @brief 启动 EAPWebService HTTP 服务，在指定 host:port 上监听 JSON 请求
 * @param port 监听的 TCP 端口号
 * @param host 绑定的主机地址（如 "0.0.0.0"、"127.0.0.1"）
 * @return true 表示启动成功或已在运行；false 表示启动失败（可通过 lastError() 获取原因）
 */
bool EAPWebService::start(quint16 port, const QString& host) {
    if (d->running.load()) return true;

    d->host = host;
    d->port = port;

    d->server.reset(new httplib::Server());
    if (d->payloadMax > 0) d->server->set_payload_max_length(d->payloadMax);
    d->server->set_keep_alive_max_count(100);
    d->server->set_tcp_nodelay(true);
    if (d->readTimeoutMs > 0)
        d->server->set_read_timeout(d->readTimeoutMs / 1000, (d->readTimeoutMs % 1000) * 1000);
    if (d->writeTimeoutMs > 0)
        d->server->set_write_timeout(d->writeTimeoutMs / 1000, (d->writeTimeoutMs % 1000) * 1000);
    if (d->idleIntervalMs > 0)
        d->server->set_idle_interval(d->idleIntervalMs / 1000, (d->idleIntervalMs % 1000) * 1000);

    d->server->set_error_handler([this](const httplib::Request& req, httplib::Response& res) {
        res.status = 500;
        const std::string body = R"({"code":500,"message":"Internal Server Error"})";
        res.set_content(body, "application/json");
        QString fn = "-";
        if (req.matches.size() >= 2) fn = QString::fromStdString(req.matches[1]);
        const QString remote = QString::fromStdString(req.remote_addr);
        QMetaObject::invokeMethod(this, [this, fn, remote]() {
            emit requestRejected(fn, 500, QStringLiteral("Unhandled error"), remote);
            }, Qt::QueuedConnection);
        });

    // 路由：POST /api/<function_name>
    d->server->Post(R"(/api/([A-Za-z0-9_]+))",
        [this](const httplib::Request& req, httplib::Response& res) {
            const QString remote = QString::fromStdString(req.remote_addr);
            QString functionName = (req.matches.size() >= 2)
                ? QString::fromStdString(req.matches[1])
                : QStringLiteral("-");

            if (!Impl::isJsonContentType(req.get_header_value("Content-Type"))) {
                res.status = 415;
                const std::string body = R"({"code":415,"message":"Unsupported Media Type. Expect application/json"})";
                res.set_content(body, "application/json");
                QMetaObject::invokeMethod(this, [this, functionName, remote]() {
                    emit requestRejected(functionName, 415, QStringLiteral("Unsupported Media Type"), remote);
                    }, Qt::QueuedConnection);
                return;
            }

            QJsonParseError jerr{};
            const QByteArray bytes(req.body.data(), static_cast<int>(req.body.size()));
            const QJsonDocument doc = QJsonDocument::fromJson(bytes, &jerr);
            if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
                const std::string body = jsonString(QJsonObject{
                    {"code", 400},
                    {"message", QString("Invalid JSON: %1").arg(jerr.errorString())}
                    });
                res.status = 400;
                res.set_content(body, "application/json");
                QMetaObject::invokeMethod(this, [this, functionName, remote, jerr]() {
                    emit requestRejected(functionName, 400, QString("Invalid JSON: %1").arg(jerr.errorString()), remote);
                    }, Qt::QueuedConnection);
                return;
            }

            const QJsonObject reqObj = doc.object();

            // === P0: 线程安全 - 获取配置的副本 ===
            EapInterfaceMeta meta;
            EAPEnvelope::Config envelopeCfg;
            bool onlyPush;
            bool strictHeadFunctionMatch;
            {
                // 路由解析 -> 配置 key
                const auto [found, key] = d->resolveKey(functionName);
                if (!found) {
                    res.status = 404;
                    const std::string body = R"({"code":404,"message":"Unknown API function"})";
                    res.set_content(body, "application/json");
                    QMetaObject::invokeMethod(this, [this, functionName, remote]() {
                        emit requestRejected(functionName, 404, QStringLiteral("Unknown API function"), remote);
                        }, Qt::QueuedConnection);
                    return;
                }

                // 获取配置副本（线程安全）
                std::lock_guard<std::mutex> lock(d->configMutex_);
                if (!d->interfaces.contains(key)) {
                    res.status = 404;
                    const std::string body = R"({"code":404,"message":"Interface not found"})";
                    res.set_content(body, "application/json");
                    return;
                }
                meta = d->interfaces.value(key);
                envelopeCfg = d->envelopeCfg;
                onlyPush = d->onlyPush;
                strictHeadFunctionMatch = d->strictHeadFunctionMatch;
            }

            if (!meta.enabled) {
                res.status = 404;
                const std::string body = R"({"code":404,"message":"API disabled"})";
                res.set_content(body, "application/json");
                QMetaObject::invokeMethod(this, [this, functionName, remote]() {
                    emit requestRejected(functionName, 404, QStringLiteral("API disabled"), remote);
                    }, Qt::QueuedConnection);
                return;
            }

            if (onlyPush) {
                if (meta.direction.compare(QStringLiteral("push"), Qt::CaseInsensitive) != 0) {
                    res.status = 403;
                    const std::string body = R"({"code":403,"message":"Forbidden for non-push interface"})";
                    res.set_content(body, "application/json");
                    QMetaObject::invokeMethod(this, [this, functionName, remote]() {
                        emit requestRejected(functionName, 403, QStringLiteral("Forbidden"), remote);
                        }, Qt::QueuedConnection);
                    return;
                }
            }

            // 归一化（根据配置的 in_head/in_body 等）
            bool hadEnvelope = false;
            QString envType;
            const QJsonObject normalized = EAPEnvelope::normalizeIncoming(reqObj, envelopeCfg, &hadEnvelope, &envType);

            // 严格一致性校验（与开关 AND，忽略大小写）
            QString reason;
            const bool needStrict = (strictHeadFunctionMatch && envelopeCfg.strictMatch);
            if (!EAPEnvelope::strictFunctionMatchOk(normalized, functionName, needStrict, &reason)) {
                res.status = 400;
                const std::string body = jsonString(QJsonObject{ {"code", 400}, {"message", reason} });
                res.set_content(body, "application/json");
                QMetaObject::invokeMethod(this, [this, functionName, remote, reason]() {
                    emit requestRejected(functionName, 400, reason, remote);
                    }, Qt::QueuedConnection);
                return;
            }

            // 收集 HTTP 头
            QMap<QString, QString> headers;
            for (const auto& h : req.headers)
                headers.insert(QString::fromStdString(h.first), QString::fromStdString(h.second));

            // 记录接收到的请求
         // 记录接收到的请求
            {
                EAPMessageRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.type = EAPMessageRecord::WebServiceReceived;
                record.interfaceKey = functionName;
                record.interfaceDescription = meta.m_interface_description;
                record.remoteAddress = remote;
                record.payload = reqObj;
                record.isSuccess = true;

                QMetaObject::invokeMethod(this, [this, record]() mutable {
                    std::lock_guard<std::mutex> lock(d->loggerMutex_);
                    if (d->messageLogger_ && d->messageLogger_->isInitialized()) {
                        d->messageLogger_->insertRecord(record);
                    
                    }
                    }, Qt::QueuedConnection);
            }
            LOG_TYPE_DEBUG("MES", "webservice receive  [{}]", QJsonDocument(reqObj).toJson().toStdString().c_str());
            QMetaObject::invokeMethod(this, [this, functionName, reqObj, headers, remote]() {
                emit requestReceived(functionName, reqObj, headers, remote);
                }, Qt::QueuedConnection);

            // 映射（两次解析合并，兼容数组键值对路径）
            QVariantMap mapped1 = JsonBuilder::parseResponse(meta, normalized);
            QVariantMap mapped2 = JsonBuilder::parseResponse(meta, normalized.value("body").toObject());
            for (auto it = mapped2.begin(); it != mapped2.end(); ++it)
                if (!mapped1.contains(it.key())) mapped1.insert(it.key(), it.value());

            // 保存到数据缓存（根据 saveToDb 配置）
            // 支持扩展格式: "function_name.{body.field1}.{body.field2}.{response.field3}"
            {
                std::lock_guard<std::mutex> lock(d->cacheMutex_);
                if (d->dataCache_ && d->dataCache_->isInitialized() && !meta.saveToDb.isEmpty()) {
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
                            QVariant value = JsonParser::resolvePlaceholderValue(normalized,placeholder);
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
                            // 如果dbKeyValue 含有,
                            // 那么应该 split
                    /*        if (dbKeyValue.contains(",")) {
                                QStringList keys = dbKeyValue.split(",");
                                for (const QString& key : keys) {
                                    const QString saveKey = QString("%1.%2").arg(functionNameFromConfig, key);
                                
                                    QMetaObject::invokeMethod(this, [this, saveKey, normalized]() mutable {
                                        std::lock_guard<std::mutex> lock(d->cacheMutex_);
                                        if (d->dataCache_ && d->dataCache_->isInitialized()) {
                                            d->dataCache_->saveData(saveKey, normalized.toVariantMap());
                                        }
                                        }, Qt::QueuedConnection);
                                }
                            }
                            else
                            {
                                QString saveKey = QString("%1.%2").arg(functionNameFromConfig, dbKeyValue);

                                QMetaObject::invokeMethod(this, [this, saveKey, normalized]() mutable {
                                    std::lock_guard<std::mutex> lock(d->cacheMutex_);
                                    if (d->dataCache_ && d->dataCache_->isInitialized()) {
                                        d->dataCache_->saveData(saveKey, normalized.toVariantMap());
                                    }
                                    }, Qt::QueuedConnection);
                            }*/
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
                                    std::lock_guard<std::mutex> lock(d->cacheMutex_);
                                    if (!d->dataCache_ || !d->dataCache_->isInitialized()) return;
                                    for (const QString& key : keys) {
                                        const QString saveKey = QString("%1.%2").arg(functionNameFromConfig, key);
                                        d->dataCache_->saveData(saveKey, payloadMap);
                                    }
                                    }, Qt::QueuedConnection);
                            }
                          
                        }
                    }
                }
            }

            QMetaObject::invokeMethod(this, [this, functionName, mapped1, reqObj]() {
                emit mappedRequestReady(functionName, mapped1, reqObj);
                }, Qt::QueuedConnection);

            // 默认 head（从 normalized.header 回显）
            auto makeDefaultHeadOK = [&]() {
                QJsonObject head = normalized.value("header").toObject();
                head.insert("result", "OK");
                head.insert("rtn_code", "");
                head.insert("rtn_msg", "");
                return head;
                };
            auto makeDefaultHeadNG = [&](const QString& code, const QString& msg) {
                QJsonObject head = normalized.value("header").toObject();
                head.insert("result", "NG");
                head.insert("rtn_code", code);
                head.insert("rtn_msg", msg);
                return head;
                };

            int status = 200;
            QJsonObject respJson;

            // === P0: 线程安全 - 获取回调副本 ===
            std::function<QJsonObject(const QString&, const QJsonObject&, const QVariantMap&)> rawResponderCopy;
            std::function<QVariantMap(const QString&, const QJsonObject&, const QVariantMap&)> mappedResponderCopy;
            int responderTimeoutMs;
            {
                std::lock_guard<std::mutex> lock(d->callbackMutex_);
                rawResponderCopy = d->rawResponder;
                mappedResponderCopy = d->mappedResponder;
                responderTimeoutMs = d->responderTimeoutMs;
            }

            // 方式一：rawResponder
            if (rawResponderCopy) {
                auto job = [rawResponderCopy, functionName, reqObj, mapped1]() -> QJsonObject {
                    return rawResponderCopy(functionName, reqObj, mapped1);
                    };
                auto result = d->runWithTimeout<QJsonObject>(job, responderTimeoutMs);
                const bool ok = result.first;
                QJsonObject outObj = result.second;

                if (!ok) {
                    status = 504;
                    respJson = EAPEnvelope::makeResponseEnvelope(QJsonObject(), reqObj, envelopeCfg, true,
                        makeDefaultHeadNG("EIC0504", "Handler timeout"));
                    
                    // P0: 超时控制 - 发出超时信号
                    QMetaObject::invokeMethod(this, [this, functionName, responderTimeoutMs, remote]() {
                        emit responderTimeout(functionName, responderTimeoutMs, remote);
                        }, Qt::QueuedConnection);
                }
                else {
                    if (outObj.isEmpty()) {
                        // 空 -> 默认 ACK
                        if (hadEnvelope) {
                            respJson = EAPEnvelope::makeResponseEnvelope(QJsonObject(), reqObj, envelopeCfg, true,
                                makeDefaultHeadOK());
                        }
                        else {
                            respJson = QJsonObject{ {"code", 0}, {"message", "OK"} };
                        }
                    }
                    else {
                        // 业务返回任意 JSON；若是 {header,body} 则会被包成响应外壳
                        respJson = EAPEnvelope::makeResponseEnvelope(outObj, reqObj, envelopeCfg, hadEnvelope,
                            makeDefaultHeadOK());
                    }
                }
            }
            // 方式二：mappedResponder
            else if (mappedResponderCopy) {
                auto job = [mappedResponderCopy, functionName, reqObj, mapped1]() -> QVariantMap {
                    return mappedResponderCopy(functionName, reqObj, mapped1);
                    };
                auto result = d->runWithTimeout<QVariantMap>(job, responderTimeoutMs);
                const bool ok = result.first;
                QVariantMap outMap = result.second;

                if (!ok) {
                    status = 504;
                    respJson = EAPEnvelope::makeResponseEnvelope(QJsonObject(), reqObj, envelopeCfg, true,
                        makeDefaultHeadNG("EIC0504", "Handler timeout"));
                    
                    // P0: 超时控制 - 发出超时信号
                    QMetaObject::invokeMethod(this, [this, functionName, responderTimeoutMs, remote]() {
                        emit responderTimeout(functionName, responderTimeoutMs, remote);
                        }, Qt::QueuedConnection);
                }
                else {
                    if (outMap.isEmpty()) {
                        if (hadEnvelope) {
                            respJson = EAPEnvelope::makeResponseEnvelope(QJsonObject(), reqObj, envelopeCfg, true,
                                makeDefaultHeadOK());
                        }
                        else {
                            respJson = QJsonObject{ {"code", 0}, {"message", "OK"} };
                        }
                    }
                    else {
                        // 将本地字段构建为 {header,body}，再包回响应外壳
                        const QJsonObject hb = JsonBuilder::buildPayload(meta, outMap);
                        respJson = EAPEnvelope::makeResponseEnvelope(hb, reqObj, envelopeCfg, hadEnvelope,
                            makeDefaultHeadOK());
                    }
                }
            }
            // 默认：ACK
            else {
                if (hadEnvelope) {
                    respJson = EAPEnvelope::makeResponseEnvelope(QJsonObject(), reqObj, envelopeCfg, true,
                        makeDefaultHeadOK());
                }
                else {
                    respJson = QJsonObject{ {"code", 0}, {"message", "OK"} };
                }
            }

            const std::string body = jsonString(respJson);
            res.status = status;
            res.set_content(body, "application/json");

            // 记录发送的响应
          // 记录发送的响应
            LOG_TYPE_DEBUG("MES", "webservice response  [{}]", body);
            {
                EAPMessageRecord record;
                record.timestamp = QDateTime::currentDateTime();
                record.type = EAPMessageRecord::WebServiceSent;
                record.interfaceKey = functionName;
                record.interfaceDescription = meta.m_interface_description;
                record.remoteAddress = remote;
                record.payload = respJson;
                record.isSuccess = (status >= 200 && status < 300);
                if (!record.isSuccess) {
                    record.errorMessage = QString("HTTP %1").arg(status);
                }

                QMetaObject::invokeMethod(this, [this, record]() mutable {
                    std::lock_guard<std::mutex> lock(d->loggerMutex_);
                    if (d->messageLogger_ && d->messageLogger_->isInitialized()) {
                        d->messageLogger_->insertRecord(record);
                      

                    }
                    }, Qt::QueuedConnection);
            }

            QMetaObject::invokeMethod(this, [this, functionName, status, respJson, remote]() {
                emit responseSent(functionName, status, respJson, remote);
                }, Qt::QueuedConnection);
        }
    );

    if (!d->server->bind_to_port(d->host.toStdString().c_str(), d->port)) {
        d->server.reset();
        d->err = QStringLiteral("Failed to bind %1:%2").arg(d->host).arg(d->port);
        return false;
    }

    d->running.store(true);
    d->worker = std::thread([this]() {
        d->server->listen_after_bind();
        d->running.store(false);
        });

    if (d->autoStopOnAppQuit) {
        QObject::connect(qApp, &QCoreApplication::aboutToQuit, this, [this]() {
            this->stop();
            }, Qt::UniqueConnection);
    }

    d->err.clear();
    return true;
}

