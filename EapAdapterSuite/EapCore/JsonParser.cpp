#include "JsonParser.h"

#include <QJsonArray>
#include <QJsonValue>
#include <QStringList>
#include <QJsonDocument>

/**
 * @brief 不区分大小写地比较 QString 与 C 字符串是否相等
 * @param a 待比较的 QString
 * @param b 右侧 C 风格字符串（以 Latin1 视图比较）
 * @return true 表示两个字符串在 Qt::CaseInsensitive 模式下相等，否则为 false
 */
static inline bool ieq(const QString& a, const char* b) {
    return a.compare(QLatin1String(b), Qt::CaseInsensitive) == 0;
}

/**
 * @brief 将带点和下标的路径字符串解析为 PathElement 序列
 * @param path 路径字符串，例如：
 *             - "body.lot_infos[0].items[-1].name"
 *             - "[0][1].field"
 * @return 解析出的 PathElement 列表：
 *         - Field 类型：kind = Field,  field 为字段名， index = -1
 *         - Index 类型：kind = Index,  index 为整数下标（可为负），field 为空
 *         若语法非法（如下标非数字、缺少 ']' 等），返回空列表表示失败。
 */
QList<JsonParser::PathElement> JsonParser::tokenizePath(const QString& path)
{
    QList<PathElement> out;
    if (path.trimmed().isEmpty()) return out;

    // 先按 '.' 粗分段，然后每段再解析 [index] 链
    const QStringList segments = path.split('.');
    for (const QString& segRaw : segments) {
        QString seg = segRaw;
        seg = seg.trimmed();
        if (seg.isEmpty()) continue;

        // 解析单段: 形如 name[0][1] 或 [2]
        int i = 0;
        const int n = seg.size();
        QString fieldBuf;

        auto pushFieldIfAny = [&]() {
            if (!fieldBuf.isEmpty()) {
                PathElement e; e.kind = PathElement::Field; e.field = fieldBuf; e.index = -1;
                out.push_back(e);
                fieldBuf.clear();
            }
            };

        while (i < n) {
            const QChar ch = seg.at(i);
            if (ch == QLatin1Char('[')) {
                // 推入之前累积的字段名
                pushFieldIfAny();

                // 解析 [number]
                int j = i + 1;
                bool neg = false;
                if (j < n && seg.at(j) == QLatin1Char('-')) {
                    neg = true; ++j;
                }
                int val = 0;
                bool hasDigit = false;
                while (j < n && seg.at(j).isDigit()) {
                    hasDigit = true;
                    val = val * 10 + (seg.at(j).unicode() - '0');
                    ++j;
                }
                if (!hasDigit || j >= n || seg.at(j) != QLatin1Char(']')) {
                    // 语法不合法，直接清空并返回空列表表示失败
                    out.clear();
                    return out;
                }
                int idx = neg ? -val : val;
                PathElement e; e.kind = PathElement::Index; e.index = idx;
                out.push_back(e);

                i = j + 1; // 跳过 ']'
            }
            else {
                // 普通字段名字符
                fieldBuf.append(ch);
                ++i;
            }
        }
        // 段结束，如尚有字段名则推入
        pushFieldIfAny();
    }

    return out;
}

/**
 * @brief 判断一条路径是否符合“键值对数组”老语义，并拆解出相关字段信息
 * @param parts     按 '.' 拆分后的路径片段列表，例如：
 *                  {"body","parameter_list","parameter_name","parameter_value","LOT_ID"}
 * @param baseIndex 输出：数组段的起始下标：
 *                  - 无前缀时为 0（如 "parameter_list.parameter_name.parameter_value.LOT_ID"）
 *                  - 有 "body." 或 "request_body." 前缀时为 1
 * @param arrayName 输出：数组字段名（如 "parameter_list" 或 "para_list"）
 * @param keyField  输出：数组元素中存放“键名”的字段（如 "parameter_name" 或 "para_name"）
 * @param valField  输出：数组元素中存放“键值”的字段（如 "parameter_value" 或 "para_value"）
 * @param matchKey  输出：要匹配的键名字符串（路径中的第 4 段，如 "LOT_ID"）
 * @return true 表示该路径符合老的键值对数组语义，
 *         false 表示不符合（例如字段名不匹配或长度不足）
 */
bool JsonParser::isKvArrayPath(const QStringList& parts,
    int& baseIndex,
    QString& arrayName,
    QString& keyField,
    QString& valField,
    QString& matchKey)
{
    // 允许前缀为 body 或 request_body（可无前缀）
    if (parts.size() < 4) return false;

    int s = 0;
    if (ieq(parts[0], "body") || ieq(parts[0], "request_body")) {
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
 * @brief 按给定路径从 JSON 对象中解析出对应的值（支持键值对数组老语义与新路径语法）
 * @param meta  接口元数据（当前未使用，保留扩展）
 * @param jsonObj 输入的顶层 JSON 对象（通常为接口返回体）
 * @param path 路径描述字符串：
 *        - 老语义键值对数组形式（不区分大小写），例如：
 *            "body.parameter_list.parameter_name.parameter_value.LOT_ID"
 *            "request_body.para_list.para_name.para_value.PNL_ID"
 *        - 新语法通用路径形式，支持字段 + 数组下标，例如：
 *            "body.panel_infos[1].id"
 *            "request_body.items[-1].name"
 * @return 若解析成功，返回对应 JSON 节点的 QVariant 值；若路径非法或未命中，则返回无效 QVariant。
 *
 * @details 解析流程：
 *  1) 优先尝试“键值对数组”老语义：
 *       - 使用 isKvArrayPath(parts, baseIndex, arrayName, keyField, valField, matchKey)
 *         判断 path 是否为键值对数组路径；
 *       - 若是，则根据 baseIndex 定位到 body/request_body 节点，
 *         再在 arrayName 对应的数组中查找 keyField == matchKey 的元素，
 *         返回该元素的 valField 对应值；
 *       - 若未找到匹配项，则返回无效 QVariant。
 *
 *  2) 若非老语义路径，则走通用路径解析：
 *       - 使用 tokenizePath(path) 将路径拆分为 PathElement 序列：
 *           * Field：对象字段访问
 *           * Index：数组下标访问（支持负下标，如 -1 表示最后一个元素）
 *       - 从 jsonObj 作为起点，依次应用每个 PathElement：
 *           a) 对 Field：
 *               · 若当前值为对象且包含该字段名，直接取该字段；
 *               · 否则尝试 body/header 与 request_body/request_head 之间的常用别名映射；
 *               · 若仍未命中，则解析失败，返回无效 QVariant。
 *           b) 对 Index：
 *               · 当前值必须为数组；
 *               · 使用 indexIntoArray 支持正/负下标访问数组元素；
 *               · 若下标越界或类型不匹配，则解析失败。
 *       - 遍历结束后，将最终 QJsonValue 转为 QVariant 返回。
 */
QVariant JsonParser::parseJson(const EapInterfaceMeta& meta,
    const QJsonObject& jsonObj,
    const QString& path)
{
    Q_UNUSED(meta);

    if (path.trimmed().isEmpty()) return QVariant();

    // 1) 先尝试“键值对数组”老语义路径
    {
        const QStringList parts = path.split('.');
        int base = 0; QString arrName, keyField, valField, matchKey;
        if (isKvArrayPath(parts, base, arrName, keyField, valField, matchKey)) {
            QJsonObject baseObj = jsonObj;
            if (base == 1) {
                const QString& prefix = parts[0];
                if (jsonObj.contains(prefix) && jsonObj.value(prefix).isObject()) {
                    baseObj = jsonObj.value(prefix).toObject();
                }
                else if (ieq(prefix, "body") && jsonObj.contains(QStringLiteral("request_body"))) {
                    baseObj = jsonObj.value(QStringLiteral("request_body")).toObject();
                }
            }
            const QJsonArray arr = baseObj.value(arrName).toArray();
            for (const QJsonValue& v : arr) {
                const QJsonObject o = v.toObject();
                const QString name = o.value(keyField).toString();
                if (name.compare(matchKey, Qt::CaseInsensitive) == 0) {
                    return o.value(valField).toVariant();
                }
            }
            return QVariant();
        }
    }

    // 2) 常规路径 + 数组下标解析
    const QList<PathElement> elems = tokenizePath(path);
    if (elems.isEmpty()) return QVariant();

    QJsonValue cur = jsonObj;

    auto indexIntoArray = [](const QJsonArray& a, int idx, QJsonValue& out) -> bool {
        int realIdx = idx;
        if (idx < 0) realIdx = a.size() + idx; // 支持负索引：-1 表示最后一个
        if (realIdx < 0 || realIdx >= a.size()) return false;
        out = a.at(realIdx);
        return true;
        };

    for (const PathElement& e : elems) {
        if (e.kind == PathElement::Field) {
            if (!cur.isObject()) return QVariant();
            const QJsonObject o = cur.toObject();

            // 直接取字段
            if (o.contains(e.field)) {
                cur = o.value(e.field);
                continue;
            }

            // 兼容常见别名（可按需扩展）
            if (ieq(e.field, "body") && o.contains(QStringLiteral("request_body"))) {
                cur = o.value(QStringLiteral("request_body"));
                continue;
            }
            if (ieq(e.field, "header") && o.contains(QStringLiteral("request_head"))) {
                cur = o.value(QStringLiteral("request_head"));
                continue;
            }
            if (ieq(e.field, "request_body") && o.contains(QStringLiteral("body"))) {
                cur = o.value(QStringLiteral("body"));
                continue;
            }
            if (ieq(e.field, "request_head") && o.contains(QStringLiteral("header"))) {
                cur = o.value(QStringLiteral("header"));
                continue;
            }

            // 未命中
            return QVariant();
        }
        else {
            // Index
            if (!cur.isArray()) return QVariant();
            const QJsonArray a = cur.toArray();
            QJsonValue next;
            if (!indexIntoArray(a, e.index, next)) return QVariant();
            cur = next;
        }
    }

    return cur.toVariant();
}

/**
 * @brief 按带有 name[] 标记的路径递归收集 JSON 中的多个值
 * @param node  当前递归访问到的 JSON 节点（可为 object / array / 标量 / null）
 * @param parts 预先用 '.' 拆分好的路径片段列表，例如：
 *              {"body", "pnl_infos[]", "pnl_id"}
 * @param idx   当前处理的路径片段索引，从 0 开始
 * @param out   输出列表，函数会将所有匹配到的值以 QVariant 形式 append 到其中
 */
void JsonParser::collectValuesByPath(const QJsonValue& node, const QStringList& parts, int idx, QList<QVariant>& out)
{
    if (idx >= parts.size()) {
        // 到达末尾：收集当前节点的值（标量或对象/数组）
        if (node.isNull() || node.isUndefined()) return;
        if (node.isArray()) {
            // 把数组里的每一项都加入
            for (const QJsonValue& v : node.toArray())
                out.append(v.toVariant());
        }
        else if (node.isObject()) {
            // 对象直接序列化为 compact JSON 字符串，或按需处理
            out.append(QJsonDocument(node.toObject()).toJson(QJsonDocument::Compact));
        }
        else {
            out.append(node.toVariant());
        }
        return;
    }

    const QString part = parts[idx];

    // 是否数组标记 name[]
    if (part.endsWith("[]")) {
        const QString name = part.left(part.length() - 2);
        if (!node.isObject()) return;
        QJsonValue arrVal = node.toObject().value(name);
        if (!arrVal.isArray()) return;
        QJsonArray arr = arrVal.toArray();
        // 对数组每一项继续解析剩余 path
        for (const QJsonValue& elem : arr) {
            collectValuesByPath(elem, parts, idx + 1, out);
        }
        return;
    }
    else {
        // 普通字段
        if (!node.isObject()) return;
        QJsonValue next = node.toObject().value(part);
        collectValuesByPath(next, parts, idx + 1, out);
        return;
    }
}

/**
 * @brief 解析占位符路径，从标准化 JSON 中取出对应的值（支持数组收集）
 * @param normalized 已标准化的 JSON 对象（作为根结点），内部可包含 body/header 等字段
 * @param placeholder 占位符路径字符串，使用 '.' 分隔，支持 name[] 语法，例如：
 *                    - "body.pnl_infos[].pnl_id"
 *                    - "header.device_id"
 *                    - "body.lot_infos[].detail"
 * @return 若未匹配到任何值，返回无效 QVariant；
 *         若仅匹配到 1 个值，直接返回该值；
 *         若匹配到多个值，则返回 QVariantList，其中每个元素为：
 *            - 标量节点：对应的 QVariant；
 *            - 数组节点：数组中每个元素依次 toVariant() 后展平收集；
 *            - 对象节点：序列化为紧凑 JSON 字符串后收集。
 */
QVariant JsonParser::resolvePlaceholderValue(const QJsonObject& normalized, const QString& placeholder)
{
    if (placeholder.isEmpty()) return QVariant();

    // 支持以 "body." 或 "header." 开头，也支持直接路径（默认从 normalized 根开始）
    QString path = placeholder;
    QJsonValue startNode = normalized;
    // 若要只从 body/header 开始，可在这里切换 startNode:
    // 但保持以 normalized(root) 作为入口更灵活：占位符里可以写 body.xxx / header.xxx
    QStringList parts = path.split('.');
    if (parts.isEmpty()) return QVariant();

    // 收集解析结果
    QList<QVariant> collected;
    collectValuesByPath(startNode, parts, 0, collected);

    if (collected.isEmpty()) return QVariant();

    if (collected.size() == 1) {
        return collected.first();
    }
    else {
        // 返回 QVariantList（调用方可选择如何序列化）
        QVariantList vlist;
        for (const QVariant& v : collected) vlist.append(v);
        return vlist;
    }
}
