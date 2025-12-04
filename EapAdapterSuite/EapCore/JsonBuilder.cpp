#include "JsonBuilder.h"
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonArray>
#include <QRegularExpression>
#include "ParameterHelper.h"

// 递归安全：按 a.b.c 路径把 value 写入 obj（创建缺失的中间对象）
// 数组段解析：name[] / name[]{...}（可指定 key/value 字段）
struct KvSpec {
    QString keyField = QStringLiteral("item_id"); 
    QString valField = QStringLiteral("item_value");
};

// 数组段解析：name[*]
/**
 * @brief 解析形如 name[*] 的数组收集段
 * @param seg        原始路径片段字符串，例如 "items[*]"
 * @param arrayName  [out] 提取出的数组字段名，例如 "items"
 * @return true 表示 seg 匹配 name[*] 形式并成功写入 arrayName，false 表示不匹配
 * @details 使用正则表达式 ^(\\w+)\\[\\*\\]$ 检查片段是否为 name[*] 形式，
 *          若匹配则返回数组名称，在路径解析过程中可据此识别“向某数组收集元素”的语义。
 */
static bool parseArrayCollectSeg(const QString& seg, QString& arrayName) {
    static QRegularExpression re(R"(^(\w+)\[\*\]$)");
    auto m = re.match(seg);
    if (!m.hasMatch()) return false;
    arrayName = m.captured(1);
    return true;
}

/**
 * @brief 解析形如 name[] 或 name[]{...} 的数组段并提取键值字段配置
 * @param seg        原始路径片段字符串，例如 "items[]", "items[]{key=id,value=name}"
 * @param arrayName  [out] 提取出的数组字段名，例如 "items"
 * @param specOut    [in,out] 键值字段配置，若 seg 中带有 {...} 选项则据此覆盖
 *                    默认的 keyField / valField（如 "item_id" / "item_value"）
 * @return true 表示 seg 符合 name[] / name[]{...} 形式并解析成功，false 表示不匹配
 */
static bool parseArraySegmentWithSpec(const QString& seg, QString& arrayName, KvSpec& specOut)
{
    static QRegularExpression re(R"(^(\w+)\[\](?:\{([^}]*)\})?$)");
    auto m = re.match(seg);
    if (!m.hasMatch()) return false;

    arrayName = m.captured(1);
    const QString opts = m.captured(2).trimmed();
    if (opts.isEmpty()) return true;

    if (opts.contains('=')) {
        const QStringList pairs = opts.split(',');
        for (const QString& p : pairs) {
            const int eq = p.indexOf('=');
            if (eq <= 0) continue;
            const QString k = p.left(eq).trimmed().toLower();
            const QString v = p.mid(eq + 1).trimmed();
            if (k == "key" || k == "k" || k == "id") {
                specOut.keyField = v;
            }
            else if (k == "value" || k == "v") {
                specOut.valField = v;
            }
        }
    }
    else {
        const QStringList two = opts.split(',');
        if (two.size() >= 2) {
            specOut.keyField = two[0].trimmed();
            specOut.valField = two[1].trimmed();
        }
    }
    return true;
}

// 写操作：在 root 上根据路径做修改，支持 []{key,val}.TOKEN 模式。
// 返回 true 如果写入成功（或已创建并写入），false 表示路径无法解析（例如父节点不是 object 等）
static bool writePathRecMutable(QJsonObject& nodeObj, const QStringList& parts, int idx, const QJsonValue& value);

// 公共入口：写入（以 root 为根），path 可以是相对 body 内部的路径（例如 "lot_infos.lot[]{item_id,item_value}.S001"）
// 在原来的json结构中，对键值对的值进行修改
static bool writeByPathWithLotIdMatch(QJsonObject& root, const QString& path, const QJsonValue& val) {
    const QStringList parts = path.split('.');
    if (parts.isEmpty()) return false;

    // 如果 path 指向 body/request_body 等并包含前缀，允许调用者传入 full path; 这里处理前缀以便在 root（通常为 body）上工作：
    QStringList effective = parts;
    if (!parts.isEmpty()) {
        const QString first = parts.front().toLower();
        if (first == "body" || first == "request_body" || first == "response_body") {
            effective = parts.mid(1);
        }
    }
    if (effective.isEmpty()) return false;

    return writePathRecMutable(root, effective, 0, val);
}

/**
 * @brief 根据点分路径在 JSON 对象中安全写入值，带智能数组处理能力
 * @param obj   作为写入起点的 JSON 对象（按值传入），函数返回修改后的副本
 * @param parts 已按 '.' 拆分好的路径片段，例如 {"edc_infos","edc[]","item_id"}
 *              或 {"edc_infos","edc","item_id"}
 * @param value 要写入的最终 JSON 值
 * @return 修改后的 QJsonObject，原始 obj 不会被就地修改
 */
static QJsonObject setByDotPathSafe(QJsonObject obj, const QStringList& parts, const QJsonValue& value) {
    if (parts.isEmpty()) return obj;

    const QString head = parts.front();
    const QStringList tail = parts.mid(1);

    static const QRegularExpression reAppend(QStringLiteral("^(\\w+)\\[\\]$"));             // name[]

    QRegularExpressionMatch m;

    //  name[] -> 追加（智能：尝试复用最后一项以支持 pairing）
    m = reAppend.match(head);
    if (m.hasMatch()) {
        const QString name = m.captured(1);
        QJsonArray arr = obj.value(name).toArray();

        if (tail.isEmpty()) {
            arr.append(value);
            obj.insert(name, arr);
            return obj;
        }

        // tail 非空：尝试复用最后一项（如果最后一项是 object 且尚未包含要写入的字段）
        const QString tailHead = tail.front();
        if (!arr.isEmpty() && arr.last().isObject()) {
            QJsonObject lastObj = arr.last().toObject();
            if (!lastObj.contains(tailHead)) {
                lastObj = setByDotPathSafe(lastObj, tail, value);
                arr.replace(arr.size() - 1, lastObj);
                obj.insert(name, arr);
                return obj;
            }
        }

        // 否则 append 一个新对象并在其上递归写入
        QJsonObject newObj;
        newObj = setByDotPathSafe(newObj, tail, value);
        arr.append(newObj);
        obj.insert(name, arr);
        return obj;
    }

    //  普通字段（非显式数组语法）
    // 如果 obj 中 head 对应的是数组（例如已存在 body.edc），并且 tail 非空（例如 .item_id），
    // 我们采用与 name[] 类似的智能策略以支持 mapping 写成 edc_infos.edc.item_id 的情况。
    if (obj.contains(head) && obj.value(head).isArray()) {
        QJsonArray arr = obj.value(head).toArray();
        if (tail.isEmpty()) {
            obj.insert(head, value);
            return obj;
        }

        const QString tailHead = tail.front();
        if (tail.size() == 1) {
            if (arr.isEmpty()) {
                QJsonObject newObj;
                newObj.insert(tailHead, value);
                arr.append(newObj);
            }
            else {
                QJsonValue last = arr.last();
                QJsonObject lastObj = last.isObject() ? last.toObject() : QJsonObject();
                if (!last.isObject() || lastObj.contains(tailHead)) {
                    QJsonObject newObj;
                    newObj.insert(tailHead, value);
                    arr.append(newObj);
                }
                else {
                    lastObj.insert(tailHead, value);
                    arr.replace(arr.size() - 1, lastObj);
                }
            }
            obj.insert(head, arr);
            return obj;
        }
        else {
            if (arr.isEmpty() || !arr.last().isObject()) {
                arr.append(QJsonObject());
            }
            QJsonObject lastObj = arr.last().toObject();
            lastObj = setByDotPathSafe(lastObj, tail, value);
            arr.replace(arr.size() - 1, lastObj);
            obj.insert(head, arr);
            return obj;
        }
    }

    //  普通字段写入（创建或递归）
    if (tail.isEmpty()) {
        obj.insert(head, value);
        return obj;
    }
    else {
        QJsonObject child = obj.value(head).toObject();
        child = setByDotPathSafe(child, tail, value);
        obj.insert(head, child);
        return obj;
    }
}

// 识别“键值对数组”老语义路径：
// 支持 [body.]parameter_list.parameter_name.parameter_value.<matchKey>
// 或  [body.]para_list.para_name.para_value.<matchKey>
/**
 * @brief 识别“键值对数组”老语义路径并拆解关键信息
 * @param parts     已按 '.' 拆分好的路径片段，例如：
 *                  - {"body","parameter_list","parameter_name","parameter_value","TEMP001"}
 *                  - {"para_list","para_name","para_value","TEMP001"}
 * @param baseIndex [out] 数组名所在的起始下标：
 *                  - 若以 "body" 开头，则 baseIndex = 1（数组名在 parts[1]）；
 *                  - 否则 baseIndex = 0（数组名在 parts[0]）。
 * @param arrayName [out] 数组字段名，例如 "parameter_list" 或 "para_list"
 * @param keyField  [out] 作为“键名”的字段名，只接受 "parameter_name"/"para_name"
 * @param valField  [out] 作为“键值”的字段名，只接受 "parameter_value"/"para_value"
 * @param matchKey  [out] 要匹配的 key 值（例如 "TEMP001"）
 * @return true 表示 parts 符合以下老式 KV 数组路径形式之一：
 *           [body.]parameter_list.parameter_name.parameter_value.<matchKey>
 *           [body.]para_list.para_name.para_value.<matchKey>
 *         并且成功填充输出参数；
 *         false 表示不匹配上述模式，输出参数内容未定义。
 */
static bool isKvArrayPath(const QStringList& parts,
    int& baseIndex,
    QString& arrayName,
    QString& keyField,
    QString& valField,
    QString& matchKey)
{
    if (parts.size() < 4) return false;
    int s = 0;
    if (parts[0].compare(QStringLiteral("body"), Qt::CaseInsensitive) == 0) {
        s = 1;
        if (parts.size() - s < 4) return false;
    }
    const QString& a = parts[s + 0];
    const QString& k = parts[s + 1];
    const QString& v = parts[s + 2];
    const QString& m = parts[s + 3];

    auto inSet = [](const QString& x, std::initializer_list<const char*> set) {
        for (auto it : set)
            if (x.compare(QLatin1String(it), Qt::CaseInsensitive) == 0) return true;
        return false;
        };

    if (!inSet(k, { "parameter_name","para_name" })) return false;
    if (!inSet(v, { "parameter_value","para_value" })) return false;

    baseIndex = s;
    arrayName = a;
    keyField = k;
    valField = v;
    matchKey = m;
    return true;
}

/**
 * @brief 根据接口元信息和本地参数构建最终请求 JSON payload
 * @param meta        接口元信息，包含 header/body 的映射规则、接口名及开关配置
 * @param localParams 调用方传入的本地参数键值对（键为映射表中的 localKey）
 * @return 构建好的 JSON 对象，通常包含 "header" 和 "body" 两部分（取决于开关）
 *
 * @details 构建过程分为三大步：
 *          1) 通过 buildHeader() 按 meta.headerMap 生成 header 部分；
 *          2) 遍历 meta.bodyMap，将 localParams 中的值按以下优先级写入 body：
 *             - 2.1 旧的“键值对数组”写法：
 *                   识别形如 [body.]arrayName.parameter_name/para_name.
 *                   parameter_value/para_value.matchKey 的路径（isKvArrayPath），
 *                   将每个 localParam 映射为数组 arrayName 中的
 *                   { keyField: matchKey, valField: value } 元素；
 *             - 2.2 原始注入写法（@raw）：
 *                   若 mapping 以 '@' 开头且启用 enableRawInjection，则将对应
 *                   QVariant Map/List 直接通过 setByDotPathSafe 整体注入目标路径；
 *             - 2.3 新的 KV 数组语法：
 *                   在路径中查找形如 "lot[]{item_id,item_value}" 的段
 *                   （parseArraySegmentWithSpec），并结合后续 token（如 ".S001"）构造
 *                   子路径，调用 writeByPathWithLotIdMatch 在 body 内按 item_id 匹配
 *                   或追加数组元素并写入值；
 *             - 2.4 默认写法：
 *                   对于以上情况都不匹配的，使用 setByDotPathSafe 按点分路径
 *                   普通写入，支持对象/数组/标量的整组透传与中间节点自动创建。
 *          3) 调用 ParameterHelper::JsonmergeAllTo(body, meta.name) 将数据库及默认配置中
 *             的公共参数合并进 body，然后根据 enableBody / enableHeader 将 body/header
 *             组合成最终 payload 并返回。
 */
QJsonObject JsonBuilder::buildPayload(const EapInterfaceMeta& meta, const QVariantMap& localParams)
{
    QJsonObject payload;
    QJsonObject header, body;

    // 1) 构建 header（保留旧逻辑；是否剥离由 Envelope 决定）
    header = buildHeader(meta.headerMap, meta.name);

    // 2) 遍历 body 映射：优先支持老“键值对数组”写法；否则一律按点路径原样落值（数组/对象整组透传）
    for (auto it = meta.bodyMap.begin(); it != meta.bodyMap.end(); ++it) {
        const QString& localKey = it.key();
        const QString& mapping = it.value();
        const QVariant& value = localParams.value(localKey);
        if (mapping.isEmpty()) continue;

        const QStringList parts = mapping.split('.');
        if (parts.isEmpty()) continue;

        // 2.1 老的“键值对数组”写法（parameter_name/para_name + parameter_value/para_value）
        int base = 0; QString arrName, keyField, valField, matchKey;
        if (isKvArrayPath(parts, base, arrName, keyField, valField, matchKey)) {
            QJsonArray arrJson = body.value(arrName).toArray();
            QJsonObject entry;
            entry.insert(keyField, matchKey);
            entry.insert(valField, QJsonValue::fromVariant(value));
            arrJson.append(entry);
            body.insert(arrName, arrJson);
            continue;
        }

        // 2.2 支持 @raw 注入：如果 mapping 以 @ 开头且接口启用了 enableRawInjection
        if (mapping.startsWith('@') && meta.enableRawInjection) {
            // 提取 @raw.xxx 中的路径部分
            const QString rawPath = mapping.mid(1); // 去掉 @
            const QStringList rawParts = rawPath.split('.');

            // 从 localParams 中提取对应的完整对象/数组
            QVariant rawValue = value;
            if (rawParts.size() > 1) {
                // 如果是 @raw.body 这样的嵌套路径，需要从 localParams 中深度提取
                // 这里简化处理：直接取 value，假设调用者已经传入了正确的值
                rawValue = value;
            }

            // 将完整的 JSON 对象/数组注入到目标路径
            if (rawValue.type() == QVariant::Map || rawValue.type() == QVariant::List) {
                body = setByDotPathSafe(body, parts, QJsonValue::fromVariant(rawValue));
            }
            else {
                // 如果不是对象或数组，按普通值处理
                body = setByDotPathSafe(body, parts, QJsonValue::fromVariant(rawValue));
            }
            continue;
        }

        // 2.3 新增：支持类似 "body.lot_infos.lot[]{item_id,item_value}.S001" 的写入（定位到数组中 item_id==S001 并写入 item_value）
        bool handledByKvSpecWrite = false;
        for (int i = 0; i < parts.size(); ++i) {
            KvSpec tmpSpec;
            QString tmpArr;
            if (parseArraySegmentWithSpec(parts[i], tmpArr, tmpSpec)) {
                // 确保后面有一个 token（如 .S001）
                if (i + 1 < parts.size()) {
                    // 计算要传入 write 函数的相对路径（以 body/请求 body 为根时，作相对处理）
                    QString subPath;
                    // 如果 mapping 以 body. / request_body. / response_body. 开头，去掉该前缀并从 body 出发
                    if (parts.front().compare(QStringLiteral("body"), Qt::CaseInsensitive) == 0 ||
                        parts.front().compare(QStringLiteral("request_body"), Qt::CaseInsensitive) == 0 ||
                        parts.front().compare(QStringLiteral("response_body"), Qt::CaseInsensitive) == 0) {
                        subPath = parts.mid(1).join('.'); // 从 body 内部开始
                    }
                    else {
                        subPath = mapping;
                    }
                    // 把 value 写入 body（写操作会在数组中查找/追加）
                    // writeByPathWithLotIdMatch 将在 body 内查找对应数组并写入
                    QJsonValue jv = QJsonValue::fromVariant(value);
                    // 调用写函数（定义在下方实现）
                    //extern bool writeByPathWithLotIdMatch(QJsonObject & root, const QString & path, const QJsonValue & val);
                    if (writeByPathWithLotIdMatch(body, subPath, jv)) {
                        handledByKvSpecWrite = true;
                        break;
                    }
                }
            }
        }
        if (handledByKvSpecWrite) continue;

        // 2.4 默认：普通点路径赋值（数组/对象/标量都原样透传）
        body = setByDotPathSafe(body, parts, QJsonValue::fromVariant(value));
    }

    // 可能需要 将参数加到这个body 中去
      // 这里进行 默认参数合并
    //先从数据库 merge

    // 再从默认参数merge
    ParameterHelper::JsonmergeAllTo(body, meta.name);
    // 3) 组合 payload
    if (meta.enableBody) {
        payload.insert(QStringLiteral("body"), body);
    }
    else {
        payload = body;
    }
    if (meta.enableHeader) {
        payload.insert(QStringLiteral("header"), header);
    }
    return payload;
}

/**
 * @brief 按响应映射规则从返回 JSON 中提取本地字段值
 * @param meta    接口元信息，包含 responseMap：JSON 路径 → 本地字段名
 * @param jsonObj 服务器/MES 返回的完整 JSON 对象
 * @return 解析结果，key 为本地字段名（localKey），value 为对应的 QVariant 值
 */
QVariantMap JsonBuilder::parseResponse(const EapInterfaceMeta& meta, const QJsonObject& jsonObj)
{
    QVariantMap result;

    for (auto it = meta.responseMap.begin(); it != meta.responseMap.end(); ++it) {
        const QString& jsonPath = it.key();   // 右：MES 路径（可到数组/对象/标量）
        const QString& localKey = it.value(); // 左：本地字段
        QStringList parts = jsonPath.split('.');
        if (parts.isEmpty()) continue;

        // 1) 老“键值对数组”写法的解析（parameter_list/para_list）
        int base = 0; QString arrName, keyField, valField, matchKey;
        if (isKvArrayPath(parts, base, arrName, keyField, valField, matchKey)) {
            QJsonObject baseObj = jsonObj;
            if (base == 1) // 有 body 前缀
                baseObj = jsonObj.value(QStringLiteral("body")).toObject();

            const QJsonArray arrJson = baseObj.value(arrName).toArray();
            for (const auto& item : arrJson) {
                const QJsonObject o = item.toObject();
                const QString name = o.value(keyField).toString().trimmed();
                if (name.compare(matchKey, Qt::CaseInsensitive) == 0) {
                    result.insert(localKey, o.value(valField).toVariant());
                    break;
                }
            }
            continue;
        }

        // 2) 通用点路径取值（不限段数）；如果是数组，会得到 QVariantList（整组透传）
        QJsonValue val = jsonObj;
        for (const auto& part : parts) {
            if (!val.isObject()) {
                val = QJsonValue(); break;
            }
            val = val.toObject().value(part);
        }
        if (!val.isUndefined()) {
            result.insert(localKey, val.toVariant());
        }
    }

    return result;
}

/**
 * @brief 根据配置表构建请求头 JSON，并自动补全基础字段
 * @param headerMap   头字段配置表：key 为字段名，value 为配置值：
 *                    - "null" 表示写入 JSON null
 *                    - "auto" 且 key 为 "timestamp" 时自动填充当前时间
 *                    - 其他情况按字面字符串写入
 * @param messageName 当前接口/消息名称，用于默认填充 messagename
 * @return 构建好的 header 对象，至少包含 messagename / timestamp / token 三个字段
 */
QJsonObject JsonBuilder::buildHeader(const QMap<QString, QString>& headerMap, const QString& messageName) {
    QJsonObject header;

    for (auto it = headerMap.begin(); it != headerMap.end(); ++it) {
        const QString& key = it.key();
        const QString& val = it.value();

        if (val == "null") {
            header.insert(key, QJsonValue::Null);
        }
        else if (val == "auto" && key == "timestamp") {
            header.insert(key, QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
        }
        else {
            header.insert(key, val);
        }
    }

    // 兼容老逻辑：强制基础三字段（Envelope 可配置 strip）
    if (!header.contains("messagename"))
        header.insert("messagename", messageName);
    if (!header.contains("timestamp"))
        header.insert("timestamp", QDateTime::currentDateTime().toString("yyyyMMddhhmmsszzz"));
    if (!header.contains("token"))
        header.insert("token", "");

    return header;
}

// 同义键名优先匹配
/**
 * @brief 带同义字段容错的 JSON 字段访问工具
 * @param obj 要查询的 JSON 对象
 * @param key 期望的字段名（如 "body"、"header"、"request_body" 等）
 * @return 若能在 obj 中找到 key 或其同义字段，则返回对应的 QJsonValue；
 *         否则返回一个默认构造的 QJsonValue（相当于 undefined）
 */
static QJsonValue getFieldWithSynonyms(const QJsonObject& obj, const QString& key) {
    if (obj.contains(key)) return obj.value(key);

    const QString k = key.toLower();
    if (k == "body") {
        if (obj.contains(QStringLiteral("request_body"))) return obj.value(QStringLiteral("request_body"));
        if (obj.contains(QStringLiteral("response_body"))) return obj.value(QStringLiteral("response_body"));
    }
    else if (k == "header" || k == "head") {
        if (obj.contains(QStringLiteral("request_head"))) return obj.value(QStringLiteral("request_head"));
        if (obj.contains(QStringLiteral("response_head"))) return obj.value(QStringLiteral("response_head"));
        if (obj.contains(QStringLiteral("header"))) return obj.value(QStringLiteral("header"));
        if (obj.contains(QStringLiteral("head"))) return obj.value(QStringLiteral("head"));
    }
    else if (k == "request_body" || k == "response_body") {
        if (obj.contains(QStringLiteral("body"))) return obj.value(QStringLiteral("body"));
    }
    else if (k == "request_head" || k == "response_head") {
        if (obj.contains(QStringLiteral("header"))) return obj.value(QStringLiteral("header"));
        if (obj.contains(QStringLiteral("head"))) return obj.value(QStringLiteral("head"));
    }
    return QJsonValue();
}

// 写入时需要找到真实存在的键名（考虑同义词），如果不存在则返回原 key（写入时会创建）
/**
 * @brief 在 JSON 对象中查找给定 key 或其同义字段名，并返回实际应使用的字段名
 * @param obj 要检查的 JSON 对象
 * @param key 期望使用的字段名（如 "body"、"header"、"request_body" 等）
 * @return 若 obj 中已存在 key 或其同义字段，则返回对应已存在的字段名；
 *         若都不存在，则返回原始 key（调用方可据此创建新字段）
 */
static QString findExistingKeyWithSynonyms(const QJsonObject& obj, const QString& key) {
    if (obj.contains(key)) return key;
    const QString k = key.toLower();
    if (k == "body") {
        if (obj.contains(QStringLiteral("request_body"))) return QStringLiteral("request_body");
        if (obj.contains(QStringLiteral("response_body"))) return QStringLiteral("response_body");
    }
    else if (k == "header" || k == "head") {
        if (obj.contains(QStringLiteral("request_head"))) return QStringLiteral("request_head");
        if (obj.contains(QStringLiteral("response_head"))) return QStringLiteral("response_head");
        if (obj.contains(QStringLiteral("header"))) return QStringLiteral("header");
        if (obj.contains(QStringLiteral("head"))) return QStringLiteral("head");
    }
    else if (k == "request_body" || k == "response_body") {
        if (obj.contains(QStringLiteral("body"))) return QStringLiteral("body");
    }
    else if (k == "request_head" || k == "response_head") {
        if (obj.contains(QStringLiteral("header"))) return QStringLiteral("header");
        if (obj.contains(QStringLiteral("head"))) return QStringLiteral("head");
    }
    // fallback: return original key (will create it)
    return key;
}


// 解析 lot[] 段中的可选字段声明：
// 支持： name[]               -> 默认 key=item_id, value=item_value
//       name[]{key=value,...} -> 显式命名（key/k/id；value/v）
//       name[]{item_id,item_value} -> 顺序指定


// 数组段解析：name[整数]
/**
 * @brief 解析形如 name[index] 的数组下标段
 * @param seg       原始路径片段字符串，例如 "items[0]"、"lot[-1]"
 * @param arrayName [out] 解析出的数组字段名，例如 "items"、"lot"
 * @param idxOut    [out] 解析出的下标整数，可为负数（具体含义由上层约定）
 * @return true 表示 seg 符合 ^(\\w+)\\[(-?\\d+)\\]$ 形式并成功解析，false 表示不匹配
 */
static bool parseArrayIndexSeg(const QString& seg, QString& arrayName, int& idxOut) {
    static QRegularExpression re(R"(^(\w+)\[(-?\d+)\]$)");
    auto m = re.match(seg);
    if (!m.hasMatch()) return false;
    arrayName = m.captured(1);
    idxOut = m.captured(2).toInt();
    return true;
}

// 递归读取（支持 [idx] / [*] / []{key=..,value=..}.TOKEN 模式）
static QJsonValue readPathRec(const QJsonValue& node, const QStringList& parts, int idx);

// 公共入口：路径读取（带 lot[] 键值匹配）
/**
 * @brief 按扩展路径语法从 JSON 根对象中读取值（支持 lotId 匹配等）
 * @param root 起始根对象（通常为 body 或完整响应中的某一段）
 * @param path 点分路径字符串，例如：
 *             - "device_id"
 *             - "lot_infos.lot[]{item_id,item_value}.S002"
 *             - "items[0].name"（视 readPathRec 实现而定）
 * @return 按路径解析后得到的 QJsonValue，若路径无效或未找到则通常为默认构造值
 */
static QJsonValue readByPathWithLotIdMatch(const QJsonObject& root, const QString& path) {
    const QStringList parts = path.split('.');
    return readPathRec(root, parts, 0);
}

/**
 * @brief 按扩展路径语法递归从 JSON 结点中读取值
 * @param node  当前递归所在的 JSON 结点（可以是 object / array / 标量）
 * @param parts 预先按 '.' 拆分好的路径片段列表
 * @param idx   当前处理的片段下标
 * @return 匹配路径后的 QJsonValue，若路径不合法或未命中则返回默认构造值（undefined）
 */
static QJsonValue readPathRec(const QJsonValue& node, const QStringList& parts, int idx) {
    if (idx >= parts.size()) return node;
    const QString seg = parts[idx];

    // name[整数]：数组下标
    QString arrName; int arrIdx = 0;
    if (parseArrayIndexSeg(seg, arrName, arrIdx)) {
        if (!node.isObject()) return QJsonValue();
        QJsonValue arrVal = getFieldWithSynonyms(node.toObject(), arrName);
        if (arrVal.isNull()) return QJsonValue(QJsonArray()); // null 视为空数组
        if (!arrVal.isArray()) return QJsonValue();
        QJsonArray arr = arrVal.toArray();
        int real = arrIdx < 0 ? (arr.size() + arrIdx) : arrIdx;
        if (real < 0 || real >= arr.size()) return QJsonValue();
        return readPathRec(arr.at(real), parts, idx + 1);
    }

    // name[*]：数组收集
    if (parseArrayCollectSeg(seg, arrName)) {
        if (!node.isObject()) return QJsonValue();
        QJsonValue arrVal = getFieldWithSynonyms(node.toObject(), arrName);
        if (arrVal.isNull()) return QJsonValue(QJsonArray());
        if (!arrVal.isArray()) return QJsonValue();
        QJsonArray in = arrVal.toArray();
        QJsonArray out;
        for (const QJsonValue& el : in) {
            QJsonValue v = readPathRec(el, parts, idx + 1);
            if (!v.isUndefined() && !v.isNull()) out.append(v);
        }
        return out;
    }

    // name[] / name[]{...}.TOKEN：键值数组匹配
    KvSpec spec;
    if (parseArraySegmentWithSpec(seg, arrName, spec)) {
        if (!node.isObject()) return QJsonValue();
        QJsonValue arrVal = getFieldWithSynonyms(node.toObject(), arrName);
        if (arrVal.isNull()) return QJsonValue();
        if (!arrVal.isArray()) return QJsonValue();
        QJsonArray arr = arrVal.toArray();

        // 下一个 token 是匹配值
        if (idx + 1 >= parts.size()) return QJsonValue();
        const QString token = parts[idx + 1];

        QJsonObject matched;
        for (const QJsonValue& v : arr) {
            if (!v.isObject()) continue;
            const QJsonObject it = v.toObject();
            const QString keyVal = it.value(spec.keyField).toString();
            if (keyVal == token) {
                matched = it; break;
            }
        }
        if (matched.isEmpty()) return QJsonValue();

        if (idx + 2 >= parts.size()) {
            return matched.value(spec.valField);
        }
        return readPathRec(matched, parts, idx + 2);
    }

    // 普通字段下钻（含同义）
    if (node.isObject()) {
        QJsonValue next = getFieldWithSynonyms(node.toObject(), seg);
        if (next.isUndefined()) return QJsonValue();
        return readPathRec(next, parts, idx + 1);
    }
    return QJsonValue();
}

// 实现写入函数：主要逻辑与 readPathRec 对称
/**
 * @brief 按扩展路径语法递归向 JSON 对象中写入值（可创建/扩展中间结构）
 * @param nodeObj 当前递归所在的 JSON 对象，将在此对象上就地修改
 * @param parts   预先按 '.' 拆分好的路径片段列表
 * @param idx     当前处理的片段下标
 * @param value   要写入的最终 QJsonValue
 * @return true 表示路径解析成功且已写入；false 表示路径不合法或写入失败
 */
static bool writePathRecMutable(QJsonObject& nodeObj, const QStringList& parts, int idx, const QJsonValue& value) {
    if (idx >= parts.size()) return false;
    const QString seg = parts[idx];

    // name[整数]：数组下标
    QString arrName; int arrIdx = 0;
    if (parseArrayIndexSeg(seg, arrName, arrIdx)) {
        QString realKey = findExistingKeyWithSynonyms(nodeObj, arrName);
        QJsonArray arr = nodeObj.value(realKey).toArray(); // 如果不存在则得到空数组

        int real = arrIdx < 0 ? (arr.size() + arrIdx) : arrIdx;
        if (real < 0) return false;
        // 扩展数组以容纳索引（如果需要，填充空对象）
        while (real >= arr.size()) arr.append(QJsonObject());
        if (idx + 1 >= parts.size()) {
            arr.replace(real, value);
            nodeObj.insert(realKey, arr);
            return true;
        }
        // 递归写入元素
        QJsonValue elem = arr.at(real);
        if (!elem.isObject()) {
            // 若不是 object，尝试用 object 覆盖
            elem = QJsonObject();
        }
        QJsonObject elemObj = elem.toObject();
        bool ok = writePathRecMutable(elemObj, parts, idx + 1, value);
        if (ok) {
            arr.replace(real, elemObj);
            nodeObj.insert(realKey, arr);
            return true;
        }
        return false;
    }

    // name[*]：数组收集（对数组中每个元素尝试写入）
    if (parseArrayCollectSeg(seg, arrName)) {
        QString realKey = findExistingKeyWithSynonyms(nodeObj, arrName);
        QJsonArray arr = nodeObj.value(realKey).toArray();
        if (arr.isEmpty()) {
            // 无元素时，根据语义，可能需要创建一个元素并写入
            QJsonObject newObj;
            if (idx + 1 >= parts.size()) {
                // 直接在数组中追加值
                arr.append(value);
                nodeObj.insert(realKey, arr);
                return true;
            }
            else {
                bool ok = writePathRecMutable(newObj, parts, idx + 1, value);
                if (ok) {
                    arr.append(newObj);
                    nodeObj.insert(realKey, arr);
                    return true;
                }
                return false;
            }
        }
        bool any = false;
        for (int i = 0; i < arr.size(); ++i) {
            QJsonValue el = arr.at(i);
            if (!el.isObject()) continue;
            QJsonObject elObj = el.toObject();
            bool ok = writePathRecMutable(elObj, parts, idx + 1, value);
            if (ok) {
                arr.replace(i, elObj);
                any = true;
            }
        }
        if (any) {
            nodeObj.insert(realKey, arr);
            return true;
        }
        return false;
    }

    // name[] / name[]{...}.TOKEN：键值数组匹配（可以找到匹配项或创建新项）
    KvSpec spec;
    if (parseArraySegmentWithSpec(seg, arrName, spec)) {
        QString realKey = findExistingKeyWithSynonyms(nodeObj, arrName);
        QJsonArray arr = nodeObj.value(realKey).toArray();

        // 下一个 token 是匹配值
        if (idx + 1 >= parts.size()) return false;
        const QString token = parts[idx + 1];

        int matchIndex = -1;
        for (int i = 0; i < arr.size(); ++i) {
            const QJsonValue& v = arr.at(i);
            if (!v.isObject()) continue;
            const QJsonObject it = v.toObject();
            const QString keyVal = it.value(spec.keyField).toString();
            if (keyVal == token) {
                matchIndex = i;
                break;
            }
        }

        if (matchIndex == -1) {
            // 没找到匹配项时，创建一个新对象并追加
            QJsonObject newObj;
            newObj.insert(spec.keyField, token);
            // 如果后续没有更多段，则直接插入 valField = value
            if (idx + 2 >= parts.size()) {
                newObj.insert(spec.valField, value);
                arr.append(newObj);
                nodeObj.insert(realKey, arr);
                return true;
            }
            else {
                // 递归写入余下路径（从 idx+2）
                bool ok = writePathRecMutable(newObj, parts, idx + 2, value);
                if (ok) {
                    arr.append(newObj);
                    nodeObj.insert(realKey, arr);
                    return true;
                }
                else {
                    return false;
                }
            }
        }
        else {
            // 找到匹配项，在匹配对象上写入
            QJsonObject matched = arr.at(matchIndex).toObject();
            if (idx + 2 >= parts.size()) {
                // 没有更多段：写入 valField
                matched.insert(spec.valField, value);
                arr.replace(matchIndex, matched);
                nodeObj.insert(realKey, arr);
                return true;
            }
            else {
                bool ok = writePathRecMutable(matched, parts, idx + 2, value);
                if (ok) {
                    arr.replace(matchIndex, matched);
                    nodeObj.insert(realKey, arr);
                    return true;
                }
                else {
                    return false;
                }
            }
        }
    }

    // 普通字段下钻（含同义）
    QString actualKey = findExistingKeyWithSynonyms(nodeObj, seg);
    if (idx + 1 >= parts.size()) {
        // 终点：直接写入（覆盖）
        nodeObj.insert(actualKey, value);
        return true;
    }
    else {
        QJsonValue childVal = nodeObj.value(actualKey);
        QJsonObject childObj = childVal.isObject() ? childVal.toObject() : QJsonObject();
        bool ok = writePathRecMutable(childObj, parts, idx + 1, value);
        if (ok) {
            nodeObj.insert(actualKey, childObj);
            return true;
        }
        return false;
    }

    return false;
}

/**
 * @brief 判断一条 JSON 路径在给定响应数据上是否可视为“分组路径”
 * @param path     点分路径字符串，支持包含形如 name[]{...} 的数组段，
 *                 例如 "request_body.lot_infos.lot[]{key=lot_id}.pnl_infos.pnl[*].pnl_id"
 * @param response 完整的响应 JSON 对象（通常包含 body/request_body 等）
 * @return true 表示 path 在当前 response 结构下看起来是“按某数组元素字段分组”的路径；
 *         false 表示不是分组路径或数据结构不满足分组判断条件
 */
static bool isGroupingPathAgainstData(const QString& path, const QJsonObject& response) {
    const QStringList parts = path.split('.');
    // 找到第一个 [] 段
    int groupIdx = -1;
    QString arrayName;
    KvSpec spec;
    for (int i = 0; i < parts.size(); ++i) {
        KvSpec tmp; QString arr;
        if (parseArraySegmentWithSpec(parts[i], arr, tmp)) {
            groupIdx = i;
            arrayName = arr;
            break;
        }
    }
    if (groupIdx < 0) return false;
    // 如果 [] 后面没有任何段，不能认为是分组（也不是匹配）
    if (groupIdx + 1 >= parts.size()) return false;

    // 定位到数组节点的父对象
    QStringList parentParts = parts.mid(0, groupIdx);
    QJsonValue parentNode = parentParts.isEmpty()
        ? QJsonValue(response)
        : readPathRec(QJsonValue(response), parentParts, 0);
    if (!parentNode.isObject()) return false;

    QJsonValue arrVal = getFieldWithSynonyms(parentNode.toObject(), arrayName);
    if (!arrVal.isArray()) return false;
    QJsonArray arr = arrVal.toArray();
    if (arr.isEmpty() || !arr.first().isObject()) {
        // 没有样本可判断，保守返回 false
        return false;
    }
    const QJsonObject firstEl = arr.first().toObject();

    const QString nextPart = parts[groupIdx + 1];
    // 如果下一段正好是元素对象的字段（比如 pnl_infos），我们认为是分组路径
    return firstEl.contains(nextPart);
}

/**
 * @brief 按映射表从响应 JSON 中构建本地字段映射，支持普通路径、lot 键值数组与分组路径
 * @param map_guanxi JSON 路径到本地字段名的映射关系：
 *                   key   = JSON 路径（可包含扩展语法）
 *                   value = 本地字段名（写入结果 QVariantMap 的 key）
 * @param response   完整响应 JSON 对象
 * @return 构建好的 QVariantMap，键为本地字段名，值为从 JSON 中解析出的 QVariant
 */
QVariantMap JsonBuilder::buildMapping(const QMap<QString, QString>& map_guanxi, const QJsonObject& response)
{
    QVariantMap out;

    for (auto it = map_guanxi.constBegin(); it != map_guanxi.constEnd(); ++it) {
        const QString jsonPath = it.key();
        const QString localKey = it.value();
        if (jsonPath.trimmed().isEmpty() || localKey.trimmed().isEmpty())
            continue;

        // 1) 先按普通/键值匹配读取（含 lot[].TOKEN 与 lot[]{...}.TOKEN）
        QJsonValue v = readByPathWithLotIdMatch(response, jsonPath);

        // 2) 宽松前缀替换，再试一次（request_body/response_body -> body；request_head/response_head -> header）
        if (v.isUndefined()) {
            QString alt = jsonPath;
            if (alt.startsWith(QStringLiteral("request_body."), Qt::CaseInsensitive)) {
                alt.replace(0, QStringLiteral("request_body.").size(), QStringLiteral("body."));
            }
            else if (alt.startsWith(QStringLiteral("response_body."), Qt::CaseInsensitive)) {
                alt.replace(0, QStringLiteral("response_body.").size(), QStringLiteral("body."));
            }
            else if (alt.startsWith(QStringLiteral("request_head."), Qt::CaseInsensitive) ||
                alt.startsWith(QStringLiteral("response_head."), Qt::CaseInsensitive)) {
                alt.replace(0, alt.indexOf('.') + 1, QStringLiteral("header."));
            }
            v = readByPathWithLotIdMatch(response, alt);
        }

        if (!v.isUndefined() && !v.isNull()) {
            out.insert(localKey, v.toVariant());
            continue;
        }

        // 3) 如果仍未取到，并且路径看起来是“分组路径”，则做分组提取
        //    例如：request_body.lot_infos.lot[]{key=lot_id}.pnl_infos.pnl[*].pnl_id
        if (isGroupingPathAgainstData(jsonPath, response)) {
            QMap<QString, QVariantMap> grouped = JsonBuilder::buildGroupedByArrayKey(jsonPath, response);
            if (!grouped.isEmpty()) {
                // 转成 QVariantMap 以便塞进 out
                QVariantMap groupedVariant;
                for (auto git = grouped.constBegin(); git != grouped.constEnd(); ++git) {
                    groupedVariant.insert(git.key(), git.value()); // QVariantMap 可直接放入 QVariant
                }
                out.insert(localKey, groupedVariant);
                continue;
            }
        }

        // 4) 上述都未命中：不写入（保持容错）
    }

    return out;
}

// 提取余下路径的“最后一个字段名”，用于作为分组内的键名
/**
 * @brief 根据路径片段推断分组结果中使用的内部字段名
 * @param parts 按 '.' 拆分好的剩余路径片段列表，如 {"pnl_infos","pnl[*]","pnl_id"}
 * @return 推断出的字段名，例如 "pnl_id" 或 "pnl"；
 *         若未找到符合格式的片段，则返回默认值 "value"
 */
static QString extractLastFieldKey(const QStringList& parts) {
    static QRegularExpression reField(R"(^(\w+)(?:\[.*\])?$)");
    for (int i = parts.size() - 1; i >= 0; --i) {
        const auto m = reField.match(parts[i]);
        if (m.hasMatch()) return m.captured(1);
    }
    return QStringLiteral("value");
}

// 新增实现：按数组 key 分组提取
/**
 * @brief 按数组元素的某个 key 对响应数据进行分组提取
 * @param groupPath 分组路径，包含第一个分组数组段 name[] / name[]{...}，例如：
 *        "request_body.lot_infos.lot[]{key=lot_id}.pnl_infos.pnl[*].pnl_id"
 * @param response  完整响应 JSON 对象
 * @return QMap<分组键, 分组内字段集合>：
 *         - key   为数组元素中 spec.keyField 对应的字符串（如 lot_id 的值）；
 *         - value 为 QVariantMap，内部字段名由剩余路径的最后一个字段推断
 *           （例如 "pnl_id"），对应的值为 readPathRec 提取的 QVariant。
 */
QMap<QString, QVariantMap> JsonBuilder::buildGroupedByArrayKey(const QString& groupPath,
    const QJsonObject& response)
{
    QMap<QString, QVariantMap> result;
    if (groupPath.trimmed().isEmpty()) return result;

    // 分段，找到第一个 "name[]/name[]{...}" 段，作为分组数组
    const QStringList parts = groupPath.split('.');
    int groupIdx = -1;
    QString arrayName;
    KvSpec spec; // 用于知道 key 字段名（如 lot_id）

    for (int i = 0; i < parts.size(); ++i) {
        KvSpec tmp;
        QString arr;
        if (parseArraySegmentWithSpec(parts[i], arr, tmp)) {
            groupIdx = i;
            arrayName = arr;
            spec = tmp; // 可能仍是默认 item_id/item_value；这里只需要 keyField
            break;
        }
    }
    if (groupIdx < 0) {
        // 没有分组数组段，直接返回空
        return result;
    }

    // 1) 解析分组数组的父节点
    QStringList parentParts = parts.mid(0, groupIdx);
    QJsonValue parentNode = parentParts.isEmpty()
        ? QJsonValue(response)
        : readPathRec(QJsonValue(response), parentParts, 0);
    if (!parentNode.isObject()) return result;

    // 2) 取出数组
    QJsonValue arrVal = getFieldWithSynonyms(parentNode.toObject(), arrayName);
    if (arrVal.isNull()) return result; // 视为空
    if (!arrVal.isArray()) return result;
    QJsonArray arr = arrVal.toArray();

    // 3) 余下路径（在每个数组元素下继续解析）
    QStringList restParts = parts.mid(groupIdx + 1);
    const QString innerKey = extractLastFieldKey(restParts.isEmpty() ? QStringList{ QStringLiteral("value") } : restParts);

    // 4) 遍历每个元素，以 keyField 作为 map 键
    for (const QJsonValue& el : arr) {
        if (!el.isObject()) continue;
        const QJsonObject obj = el.toObject();
        const QString lotKey = obj.value(spec.keyField).toString();
        if (lotKey.isEmpty()) continue;

        QJsonValue v;
        if (restParts.isEmpty()) {
            // 没有余下路径，直接把整个对象塞进去
            v = obj;
        }
        else {
            v = readPathRec(el, restParts, 0);
            // 对于收集型，readPathRec 可能返回 QJsonArray；保持原样
        }
        if (v.isUndefined() || v.isNull()) {
            // 没值则跳过；也可选择插入空数组/空字符串，按需修改
            continue;
        }

        QVariantMap& slot = result[lotKey]; // 自动创建
        slot.insert(innerKey, v.toVariant());
    }

    return result;
}