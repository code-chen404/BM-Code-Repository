#pragma once
// 说明：
// - 这个极简适配只做“外壳键名映射”，不改变你已有的 JsonBuilder 逻辑。
// - 发送：把 {header, body}（或纯 body）改名为 {out_head, out_body}（例如 request_head/request_body）。
// - 接收：把 {in_head, in_body}（例如 response_head/response_body）或 {request_head, request_body} 或 {header, body}
//        统一归一化为 {header, body}，便于继续用 JsonBuilder::parseResponse。
// - 支持 strip_forced_header：去除 JsonBuilder 强插的 header 字段（messagename/timestamp/token）。
// - 支持 strict_match：按需对比 URL function_name 与归一化 header 中 function_name 是否一致。
// - 配置文件 payload_policy.json 仅包含 default 段：
//   {
//     "default": {
//       "out_head": "request_head",
//       "out_body": "request_body",
//       "in_head": "response_head",
//       "in_body": "response_body",
//       "strip_forced_header": true,
//       "strict_match": true
//     }
//   }

#include <QString>
#include <QFile>
#include <QJsonObject>
#include <QJsonDocument>

/*
EAPEnvelope 这套代码，就是帮你适配各种“包一层壳”的 JSON 协议——有的对端用 header/body，
有的用 request_head/request_body，有的又用 response_head/response_body，还想在不同接口上改成别的名字。
你的业务内部只想统一用 {header, body} 来处理，这个命名空间就是负责「进来时统一、出去时改名」。
*/
namespace EAPEnvelope 
{
    // 外壳键名与行为配置（仅全局 default）
    struct Config {
        QString outHead = "head";    // 发送出去时，头部字段的 key
        QString outBody = "body";    // 发送出去时，body 字段的 key

        QString inHead = "head";     // 接收时，响应里的“头部”字段 key
        QString inBody = "body";     // 接收时，响应里的“body”字段 key

        bool stripForcedHeader = true; // 是否移除 JsonBuilder 强制加的 messagename/timestamp/token
        bool strictMatch = true;       // 是否严格校验 URL 中 function_name 与 header.function_name 一致

        QMap<QString, Config> interfaces; // 每个接口单独的配置（覆盖 default）
    };

    // 将接口中的含有 Config 的参数配置改为 Config 的参数配置形式
    /*无引用--R*/
    inline bool loadConfigFromFile(const QString& path, Config& cfg, QString* errorOut = nullptr) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly)) {
            if (errorOut) *errorOut = QStringLiteral("无法打开策略文件: %1").arg(path);
            return false;
        }

        QJsonParseError jerr{};
        const auto doc = QJsonDocument::fromJson(f.readAll(), &jerr);
        if (jerr.error != QJsonParseError::NoError || !doc.isObject()) {
            if (errorOut) *errorOut = QStringLiteral("策略 JSON 解析失败: %1").arg(jerr.errorString());
            return false;
        }

        const QJsonObject root = doc.object();
        if (!root.contains("default") || !root.value("default").isObject())
            return true; //无 default 段则使用内置默认

        // 含有 default 的根配置
        const QJsonObject def = root.value("default").toObject();
        if (def.contains("out_head")) cfg.outHead = def.value("out_head").toString(cfg.outHead);
        if (def.contains("out_body")) cfg.outBody = def.value("out_body").toString(cfg.outBody);
        if (def.contains("in_head"))  cfg.inHead = def.value("in_head").toString(cfg.inHead);
        if (def.contains("in_body"))  cfg.inBody = def.value("in_body").toString(cfg.inBody);

        if (def.contains("strip_forced_header")) cfg.stripForcedHeader = def.value("strip_forced_header").toBool(cfg.stripForcedHeader);
        if (def.contains("strict_match"))         cfg.strictMatch = def.value("strict_match").toBool(cfg.strictMatch);

        //  含有 interfaces 的根配置
        if (root.contains("interfaces") && root.value("interfaces").isObject()) {
            const QJsonObject ifaces = root.value("interfaces").toObject();
            for (auto it = ifaces.begin(); it != ifaces.end(); ++it) {
                if (!it.value().isObject()) continue;
                const QJsonObject ifaceObj = it.value().toObject();
                Config ifaceCfg = cfg; // 使用 default
                if (ifaceObj.contains("out_head")) ifaceCfg.outHead = ifaceObj.value("out_head").toString(ifaceCfg.outHead);
                if (ifaceObj.contains("out_body")) ifaceCfg.outBody = ifaceObj.value("out_body").toString(ifaceCfg.outBody);
                if (ifaceObj.contains("in_head"))  ifaceCfg.inHead = ifaceObj.value("in_head").toString(ifaceCfg.inHead);
                if (ifaceObj.contains("in_body"))  ifaceCfg.inBody = ifaceObj.value("in_body").toString(ifaceCfg.inBody);
                if (ifaceObj.contains("strip_forced_header")) ifaceCfg.stripForcedHeader = ifaceObj.value("strip_forced_header").toBool(ifaceCfg.stripForcedHeader);
                if (ifaceObj.contains("strict_match")) ifaceCfg.strictMatch = ifaceObj.value("strict_match").toBool(ifaceCfg.strictMatch);
                cfg.interfaces.insert(it.key(), ifaceCfg);
            }
        }

        return true;
    }

    // 这个函数接收一个 header 对象（JSON 头部），把其中 3 个“强制头字段”删掉
    inline void removeForcedHeaderKeys(QJsonObject& header) {
        header.remove(QStringLiteral("messagename"));
        header.remove(QStringLiteral("timestamp"));
        header.remove(QStringLiteral("token"));
    }

    // 给定一个接口名，返回“这个接口对应的 Config”，如果没单独配置就返回默认 Config
    inline Config resolveConfig(const QString& interfaceKey, const Config& baseCfg) {
        if (interfaceKey.isEmpty() || !baseCfg.interfaces.contains(interfaceKey)) {
            return baseCfg; // ʹ�� default
        }
        return baseCfg.interfaces.value(interfaceKey);
    }

    // 判断一个配置字段是不是“直通模式”（空字符串就代表直通）
    inline bool isPassthrough(const QString& str) {
        return str.isEmpty();
    }

    // 目的：发送前，把你内部的 {header, body} 或“纯 body”按照配置打包成对端想要的外壳结构
    // 发送侧：把 {header, body} 或“纯 body”包装/改名为 {outHead, outBody}
    // - 若 outHead="header" 且 outBody="body"，则保持旧协议键名不变
    // - 当 stripForcedHeader=true 时，移除强制头字段
    inline QJsonObject wrapOutgoing(const QJsonObject& hbOrBody, const Config& baseCfg, const QString& interfaceKey = QString()) {
        // 按接口解析出生效配置
        const Config cfg = resolveConfig(interfaceKey, baseCfg);
        // 直通模式（passthrough）直接返回
        if (isPassthrough(cfg.outHead) && isPassthrough(cfg.outBody)) {
            return hbOrBody;
        }

        // 从入参里拆出 header 和 body
        QJsonObject h, b;

        if (hbOrBody.contains(QStringLiteral("header")) && hbOrBody.value(QStringLiteral("header")).isObject())
            h = hbOrBody.value(QStringLiteral("header")).toObject();

        if (hbOrBody.contains(QStringLiteral("body")) && hbOrBody.value(QStringLiteral("body")).isObject())
            b = hbOrBody.value(QStringLiteral("body")).toObject();
        else if(h.isEmpty() && b.isEmpty())
            return hbOrBody;
        else if (!hbOrBody.contains(QStringLiteral("header")))
            b = hbOrBody;  // 视为“纯 body”

        // 按配置决定是否移除强制头字段
        if (cfg.stripForcedHeader && !h.isEmpty()) {
            removeForcedHeaderKeys(h);
        }

        // 旧协议：直接用 header/body 作为键名
        if (cfg.outHead == QStringLiteral("header") && cfg.outBody == QStringLiteral("body")) {
            QJsonObject out;
            if (!h.isEmpty()) out.insert(QStringLiteral("header"), h);
            if (!b.isEmpty()) out.insert(QStringLiteral("body"), b);
            // 若两者皆空（极少），返回空对象
            return out.isEmpty() ? QJsonObject{} : out;
        }

        // 新协议：改名为 outHead/outBody（通常是 request_head/request_body）
        QJsonObject out;
        if (!isPassthrough(cfg.outHead)) {
            out.insert(cfg.outHead, h);
        }
        if (!isPassthrough(cfg.outBody)) {
            out.insert(cfg.outBody, b);
        }
        return out;
    }

    // 目的：
    // 接收后，把各种：
    // { response_head,response_body }
    // {request_head, request_body}
    // { header, body }
    // 纯 JSON
    // 统一整理成内部格式：{ "header": ..., "body" : ... }
    inline QJsonObject normalizeIncoming(const QJsonObject& in, const Config& baseCfg,
        bool* hadEnvelope = nullptr, QString* envelopeType = nullptr, const QString& interfaceKey = QString()) {
        // 先解析当前接口的配置，并初始化标记
        const Config cfg = resolveConfig(interfaceKey, baseCfg);
        if (hadEnvelope) *hadEnvelope = false;
        if (envelopeType) envelopeType->clear();

        // passthrough 模式：原样返回
        if (isPassthrough(cfg.inHead) && isPassthrough(cfg.inBody)) {
            return in;
        }

        // 1) 优先按配置的 inHead/inBody 拆壳（通常是 response_head/response_body）
        if (in.contains(cfg.inHead) || in.contains(cfg.inBody)) {
            QJsonObject out;
            if (in.value(cfg.inHead).isObject())
                out.insert(QStringLiteral("header"), in.value(cfg.inHead).toObject());
            if (in.value(cfg.inBody).isObject())
                out.insert(QStringLiteral("body"), in.value(cfg.inBody).toObject());
            if (hadEnvelope) *hadEnvelope = true;
            if (envelopeType) *envelopeType = QStringLiteral("response");
            return out;
        }

        // 2) 其次兼容 request_head/request_body（有些对端可能回显原样）
        if (in.contains(QStringLiteral("request_head")) || in.contains(QStringLiteral("request_body"))) {
            QJsonObject out;
            if (in.value(QStringLiteral("request_head")).isObject())
                out.insert(QStringLiteral("header"), in.value(QStringLiteral("request_head")).toObject());
            if (in.value(QStringLiteral("request_body")).isObject())
                out.insert(QStringLiteral("body"), in.value(QStringLiteral("request_body")).toObject());
            if (hadEnvelope) *hadEnvelope = true;
            if (envelopeType) *envelopeType = QStringLiteral("request");
            return out;
        }

        // 3) 再次兼容原生 header/body 格式
        if (in.contains(QStringLiteral("header")) || in.contains(QStringLiteral("body"))) {
            QJsonObject out;
            if (in.value(QStringLiteral("header")).isObject())
                out.insert(QStringLiteral("header"), in.value(QStringLiteral("header")).toObject());
            if (in.value(QStringLiteral("body")).isObject())
                out.insert(QStringLiteral("body"), in.value(QStringLiteral("body")).toObject());
            return out;
        }

        // 4) 都不是：当成“纯 body”
        QJsonObject out;
        out.insert(QStringLiteral("body"), in);
        return out;
    }

    // 检查 URL 中解析出来的 function_name 和 header.function_name 是否一致（忽略大小写），用于防错和安全校验
    inline bool strictFunctionMatchOk(const QJsonObject& normalizedHB,
        const QString& urlFunctionName,
        bool strict,
        QString* reasonOut = nullptr) {
        if (!strict) return true; // 如果没开启严格模式，直接放行
        const QString fnInHead = normalizedHB.value(QStringLiteral("header")).toObject()
            .value(QStringLiteral("function_name")).toString(); // 从规范化后的 {header, body} 里取出 function_name
        if (fnInHead.isEmpty()) return true; // 头部里没有 function_name 时，认为没法比，对它网开一面
        if (fnInHead.compare(urlFunctionName, Qt::CaseInsensitive) == 0) return true; // 忽略大小写比较 URL 与 header 里的 function_name
        if (reasonOut) *reasonOut = QStringLiteral("function_name mismatch between URL and header"); // 走到这里说明不匹配 → 返回 false，并给出原因
        return false;
    }

    // 目的：服务端发回响应时，把业务层的{ header,body } 或其它 JSON，包装成“响应外壳”{ inHead,inBody }，比如 response_head / response_body
    inline QJsonObject makeResponseEnvelope(const QJsonObject& src,
        const QJsonObject& originalReq,
        const Config& cfg,
        bool hadRequestEnvelope,
        const QJsonObject& defaultHead) {
        // 已经是“响应外壳”则直接返回
        if (src.contains(QStringLiteral("response_head")) || src.contains(QStringLiteral("response_body")) ||
            src.contains(cfg.inHead) || src.contains(cfg.inBody)) {
            return src;
        }

        // 如果 src 是 {header, body} → 映射成 {inHead, inBody}
        if (src.contains(QStringLiteral("header")) || src.contains(QStringLiteral("body"))) {
            QJsonObject out;
            if (src.contains(QStringLiteral("header")))
                out.insert(cfg.inHead, src.value(QStringLiteral("header")).toObject());
            else
                out.insert(cfg.inHead, QJsonObject{});
            if (src.contains(QStringLiteral("body")))
                out.insert(cfg.inBody, src.value(QStringLiteral("body")).toObject());
            else
                out.insert(cfg.inBody, QJsonObject{});
            return out;
        }

        // 请求是“包壳格式”但业务没有返回任何内容 → 返回默认 ACK
        if (hadRequestEnvelope) {
            QJsonObject out;
            out.insert(cfg.inHead, defaultHead);
            out.insert(cfg.inBody, QJsonObject{});
            return out;
        }

        // 否则：非 envelope 协议 → 原样返回
        return src;
    }

} // namespace EAPEnvelope