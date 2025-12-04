#include "ParameterHelper.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QDebug>
#include <QJsonValue>
#include <QWriteLocker>
#include <QReadLocker>
#include <QJsonArray>
QVariantMap ParameterHelper::s_params; // 全局变量定义

// 返回对单例锁的引用（函数局部静态，C++11 保证线程安全的初始化）
QReadWriteLock& ParameterHelper::lock()
{
    static QReadWriteLock theLock;
    return theLock;
}

/**
 * @brief 从 JSON 文件加载默认参数，并刷新全局参数缓存
 * @param filepath 默认参数配置文件路径（JSON 格式，顶层为对象）
 * @return true 表示加载并解析成功，且已用写锁安全更新 s_params；
 *         false 表示文件打开失败、JSON 解析失败或顶层结构非法
 */
bool ParameterHelper::loadDefaultParam(const QString& filepath)
{
    QFile f(filepath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "ParameterHelper::loadDefaultParam: cannot open file" << filepath;
        return false;
    }

    const QByteArray data = f.readAll();
    f.close();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(data, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "ParameterHelper::loadDefaultParam: JSON parse error:" << err.errorString();
        return false;
    }

    if (!doc.isObject()) {
        qWarning() << "ParameterHelper::loadDefaultParam: top-level JSON is not an object";
        return false;
    }

    QVariantMap map = doc.toVariant().toMap();

    // 写锁保护
    QWriteLocker locker(&lock());
    s_params.swap(map);
    return true;
}

/**
 * @brief 从全局默认参数表中获取指定接口下的参数值
 * @param interfaceName 接口名（如 "UploadPanelData"），对应 s_params 顶层的键
 * @param keyName       参数名：
 *                      - 若为空字符串，则返回该接口对应的整块配置（通常是 QVariantMap）；
 *                      - 否则返回该接口下 keyName 对应的参数值。
 * @return 若找到对应值，则返回有效 QVariant；
 *         若接口不存在、结构不为 map 或 keyName 未配置，则返回一个无效 QVariant。
 */
QVariant ParameterHelper::getParam(const QString& interfaceName, const QString& keyName)
{
    QReadLocker locker(&lock());

    QVariant ifaceVar = s_params.value(interfaceName);
    if (!ifaceVar.isValid())
        return QVariant();

    if (keyName.isEmpty())
        return ifaceVar;

    if (ifaceVar.type() == QVariant::Map) {
        QVariantMap ifaceMap = ifaceVar.toMap();
        return ifaceMap.value(keyName);
    }

    return QVariant();
}

/**
 * @brief 将指定接口的某个默认参数合并到输入 map 中（仅在当前值为空时才补默认值）
 * @param inputMap      目标参数表，函数会在其中按需插入默认值
 * @param interfaceName 接口名，用于从全局默认参数表 s_params 中查找对应配置
 * @param keyName       需要合并的字段名
 */
void ParameterHelper::mergeTo(QVariantMap& inputMap, const QString& interfaceName, const QString& keyName)
{
    if (keyName.isEmpty())
        return;

    QVariant existing = inputMap.value(keyName, QVariant());
    if (existing.isValid() && !isEmptyVariant(existing))
        return;

    QVariant def = getParam(interfaceName, keyName);
    if (def.isValid())
        inputMap.insert(keyName, def);
}

/**
 * @brief 将指定接口的全部默认参数批量合并到输入 map 中
 * @param inputMap      目标参数表，函数会在其中按需插入多个默认字段
 * @param interfaceName 接口名，对应 s_params 顶层的键，用于获取该接口的默认配置
 */
void ParameterHelper::mergeAllTo(QVariantMap& inputMap, const QString& interfaceName)
{
    QReadLocker locker(&lock());
    QVariant ifaceVar = s_params.value(interfaceName);
    if (!ifaceVar.isValid() || ifaceVar.type() != QVariant::Map)
        return;

    QVariantMap ifaceMap = ifaceVar.toMap();
    for (auto it = ifaceMap.constBegin(); it != ifaceMap.constEnd(); ++it) {
        const QString key = it.key();
        QVariant existing = inputMap.value(key, QVariant());
        if (!inputMap.contains(key) || isEmptyVariant(existing)) {
            inputMap.insert(key, it.value());
        }
    }
}

/**
 * @brief 从给定 JSON 对象中按 keyName 合并单个字段到输入 map（仅在当前值为空时才补）
 * @param inputMap  目标参数表，函数会在其中按需插入对应字段
 * @param obj       JSON 源对象，作为“默认值来源”
 * @param keyName   要合并的字段名
 */
void ParameterHelper::mergeToFromJson(QVariantMap& inputMap, const QJsonObject& obj, const QString& keyName)
{
    if (keyName.isEmpty())
        return;

    QVariant existing = inputMap.value(keyName, QVariant());
    if (existing.isValid() && !isEmptyVariant(existing))
        return;

    if (!obj.contains(keyName))
        return;

    QJsonValue jv = obj.value(keyName);
    if (jv.isUndefined() || jv.isNull())
        return;

    QVariant var = jv.toVariant();
    inputMap.insert(keyName, var);
}

/**
 * @brief 将给定 JSON 对象中的所有字段批量合并到输入 map（仅在当前值为空时才补默认）
 * @param inputMap  目标参数表，函数会在其中按需插入多个字段
 * @param obj       JSON 源对象，作为一整块默认配置来源
 */
void ParameterHelper::mergeAllToFromJson(QVariantMap& inputMap, const QJsonObject& obj)
{
    if (obj.isEmpty())
        return;

    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString key = it.key();
        QVariant existing = inputMap.value(key, QVariant());
        if (inputMap.contains(key) && !isEmptyVariant(existing))
            continue;
        QJsonValue jv = it.value();
        if (jv.isUndefined())
            continue;
        QVariant var = jv.toVariant();
        inputMap.insert(key, var);
    }
}

// helper: 判断 QVariant 是否“空”
// 将 null/invalid、空字符串（trim 后）、空 QVariantList/空 QVariantMap、空 QByteArray 视为“无值”
// 数值类型（包括 0）和布尔 false 被视为有值（不会被覆盖）
/**
 * @brief 判断一个 QVariant 是否可视为“空值”
 * @param v 待检查的 QVariant
 * @return true 表示该值被视为“空”（可用默认值覆盖或补齐），false 表示为“非空”
 */
bool ParameterHelper::isEmptyVariant(const QVariant& v)
{
    if (!v.isValid() || v.isNull())
        return true;

    switch (v.type()) {
    case QMetaType::QByteArray:
        return v.toByteArray().isEmpty();
    case QVariant::String:
        return v.toString().trimmed().isEmpty();
    case QVariant::List:
        return v.toList().isEmpty();
    case QVariant::Map:
        return v.toMap().isEmpty();
    default:
        break;
    }

    // 额外尝试转换为容器判断
    if (v.canConvert<QVariantList>()) {
        QVariantList lst = v.toList();
        if (lst.isEmpty()) return true;
    }
    if (v.canConvert<QVariantMap>()) {
        QVariantMap mp = v.toMap();
        if (mp.isEmpty()) return true;
    }

    return false;
}

// ---------- QJsonObject 版本的合并接口 ----------

// 从默认参数（s_params）合并单个 key 到 QJsonObject（当不存在或存在但无值时插入）
/**
 * @brief 从全局默认参数表（s_params）中合并单个字段到 QJsonObject
 * @param inputMap      目标 JSON 对象，将在其中按需插入 keyName 对应的默认值
 * @param interfaceName 接口名，用于从 s_params 中获取该接口下的默认配置
 * @param keyName       需要合并的字段名
 */
void ParameterHelper::JsonmergeTo(QJsonObject& inputMap, const QString& interfaceName, const QString& keyName)
{
    if (keyName.isEmpty())
        return;

    // 检查现有值：存在且非空则跳过
    if (inputMap.contains(keyName)) {
        QJsonValue ev = inputMap.value(keyName);
        QVariant existingVar = ev.toVariant();
        if (existingVar.isValid() && !isEmptyVariant(existingVar))
            return;
    }
     
    QVariant def = getParam(interfaceName, keyName); // 从全局默认参数表中获取指定接口下的参数值
    if (!def.isValid())
        return;

    QJsonValue jv = QJsonValue::fromVariant(def);
    inputMap.insert(keyName, jv);
}

// 从默认参数（s_params）合并所有键到 QJsonObject（当不存在或存在但无值时插入）
/**
 * @brief 将指定接口的全部默认参数批量合并到 QJsonObject 中
 * @param inputMap      目标 JSON 对象，将在其中按需插入多个默认字段
 * @param interfaceName 接口名，对应 s_params 顶层键，用于获取该接口的默认配置
 */
void ParameterHelper::JsonmergeAllTo(QJsonObject& inputMap, const QString& interfaceName)
{
    QReadLocker locker(&lock());
    QVariant ifaceVar = s_params.value(interfaceName);
    if (!ifaceVar.isValid() || ifaceVar.type() != QVariant::Map)
        return;

    QVariantMap ifaceMap = ifaceVar.toMap();
    for (auto it = ifaceMap.constBegin(); it != ifaceMap.constEnd(); ++it) {
        const QString key = it.key();
        if (inputMap.contains(key)) {
            QVariant existingVar = inputMap.value(key).toVariant();
            if (existingVar.isValid() && !isEmptyVariant(existingVar))
                continue;
        }
        QJsonValue jv = QJsonValue::fromVariant(it.value());
        inputMap.insert(key, jv);
    }
}

// 从另一个 QJsonObject 来源合并单个 key 到目标 QJsonObject（当目标不存在或存在但无值时插入）
/**
 * @brief 从另一个 QJsonObject 中按 keyName 合并单个字段到目标 QJsonObject
 * @param inputMap  目标 JSON 对象，当目标不存在该键或该键值为空时才会被插入/覆盖
 * @param obj       源 JSON 对象，提供默认值/模板值
 * @param keyName   需要合并的字段名
 */
void ParameterHelper::JsonmergeToFromJson(QJsonObject& inputMap, const QJsonObject& obj, const QString& keyName)
{
    if (keyName.isEmpty())
        return;

    if (inputMap.contains(keyName)) {
        QVariant existingVar = inputMap.value(keyName).toVariant();
        if (existingVar.isValid() && !isEmptyVariant(existingVar))
            return;
    }

    if (!obj.contains(keyName))
        return;

    QJsonValue src = obj.value(keyName);
    if (src.isUndefined() || src.isNull())
        return;

    inputMap.insert(keyName, src);
}

// 从另一个 QJsonObject 来源合并所有键到目标 QJsonObject（当目标不存在或存在但无值时插入）
/**
 * @brief 从另一个 QJsonObject 中批量合并所有键到目标 QJsonObject
 * @param inputMap  目标 JSON 对象，当目标键不存在或值为空时才会被插入/覆盖
 * @param obj       源 JSON 对象，作为一整块默认配置或模板来源
 */
void ParameterHelper::JsonmergeAllToFromJson(QJsonObject& inputMap, const QJsonObject& obj)
{
    if (obj.isEmpty())
        return;

    for (auto it = obj.constBegin(); it != obj.constEnd(); ++it) {
        const QString key = it.key();
        if (inputMap.contains(key)) {
            QVariant existingVar = inputMap.value(key).toVariant();
            if (existingVar.isValid() && !isEmptyVariant(existingVar))
                continue;
        }
        QJsonValue src = it.value();
        if (src.isUndefined())
            continue;
        inputMap.insert(key, src);
    }
}

// 递归应用：在 object 上根据 parts[idx..] 应用 value
static void applyPathToObject(QJsonObject& obj, const QStringList& parts, int idx, const QJsonValue& value);

// 递归应用：在 array 上根据 parts[idx..] 应用 value
/**
 * @brief 递归地在 QJsonArray 上按照给定路径片段应用写入值
 * @param arr   当前要操作的数组结点，对应 parts[idx-1] 所指向的容器
 * @param parts 预先按 '.' 拆分好的路径片段列表
 * @param idx   当前要处理的路径片段下标（下一段路径为 parts[idx]）
 * @param value 最终要写入的值（可为标量或数组）
 */
static void applyPathToArray(QJsonArray& arr, const QStringList& parts, int idx, const QJsonValue& value)
{
    // arr corresponds to the container referenced by parts[idx-1]
    // next part is parts[idx]
    if (idx >= parts.size()) return;

    // 如果下一部分是最后一段（final key），则对数组的行为取决于 value 类型
    if (idx == parts.size() - 1) {
        const QString finalKey = parts[idx];
        if (value.isArray()) {
            // append new objects for each element in value array: { finalKey: elem }
            for (const QJsonValue& elem : value.toArray()) {
                QJsonObject newObj;
                newObj.insert(finalKey, elem);
                arr.append(newObj);
            }
        }
        else {
            // 对于单值：设置每个已有元素的 finalKey，若数组为空则追加新对象
            if (arr.isEmpty()) {
                QJsonObject newObj;
                newObj.insert(finalKey, value);
                arr.append(newObj);
            }
            else {
                for (int i = 0; i < arr.size(); ++i) {
                    QJsonValue item = arr.at(i);
                    QJsonObject itemObj;
                    if (item.isObject()) {
                        itemObj = item.toObject();
                    }
                    else {
                        // 保留原值到 "_value"（可选），并转换为 object
                        itemObj = QJsonObject();
                        itemObj.insert("_value", item);
                    }
                    itemObj.insert(finalKey, value);
                    arr[i] = itemObj;
                }
            }
        }
        return;
    }

    // 否则需要对数组中的每个元素递归处理下一段路径
    const QString nextKey = parts[idx];
    for (int i = 0; i < arr.size(); ++i) {
        QJsonValue element = arr.at(i);
        if (element.isObject()) {
            QJsonObject elObj = element.toObject();
            applyPathToObject(elObj, parts, idx, value); // 部分 idx 属于对象的 key
            arr[i] = elObj;
        }
        else if (element.isArray()) {
            QJsonArray elArr = element.toArray();
            applyPathToArray(elArr, parts, idx, value);
            arr[i] = elArr;
        }
        else {
            // 非对象非数组：将其转换为对象并继续递归
            QJsonObject elObj;
            elObj.insert("_value", element);
            applyPathToObject(elObj, parts, idx, value);
            arr[i] = elObj;
        }
    }
}

/**
 * @brief 按路径片段递归地向 QJsonObject 中写入值（对象节点版本）
 * @param obj   当前要操作的 JSON 对象结点
 * @param parts 预先按 '.' 拆分好的路径片段列表
 * @param idx   当前处理的路径片段下标（parts[idx] 为当前 key）
 * @param value 最终要写入的值（可为标量或数组）
 */
static void applyPathToObject(QJsonObject& obj, const QStringList& parts, int idx, const QJsonValue& value)
{
    if (idx >= parts.size()) return;

    QString key = parts[idx];

    // 如果这是最后一段，直接写入 obj[key]（覆盖或创建）
    if (idx == parts.size() - 1) {
        obj.insert(key, value);
        return;
    }

    // 下一个节点索引
    int nextIdx = idx + 1;

    QJsonValue child = obj.value(key);

    // 如果 child 是数组，则把剩余路径应用到数组上（数组元素上会继续递归）
    if (child.isArray()) {
        QJsonArray arr = child.toArray();
        // apply to arr at nextIdx
        applyPathToArray(arr, parts, nextIdx, value);
        obj.insert(key, arr);
        return;
    }

  
    if (child.isObject()) {
        QJsonObject childObj = child.toObject();
        applyPathToObject(childObj, parts, nextIdx, value);
        obj.insert(key, childObj);
        return;
    }

 
    if (nextIdx == parts.size() - 1 && value.isArray()) {
        // 在 obj[key] 创建一个数组，每个 value 元素创建一个对象 { finalKey: elem }
        QJsonArray newArr;
        for (const QJsonValue& elem : value.toArray()) {
            QJsonObject newObj;
            newObj.insert(parts[nextIdx], elem);
            newArr.append(newObj);
        }
        obj.insert(key, newArr);
        return;
    }

    // 否则创建一个中间对象并继续递归
    QJsonObject newChildObj;
    applyPathToObject(newChildObj, parts, nextIdx, value);
    obj.insert(key, newChildObj);
}

// 主公开函数：path 可以是 "a.b.c" 这样的点分隔路径
/**
 * @brief 按点分隔路径将 QJsonValue 写入到 QJsonObject 中
 * @param inputObj  目标 JSON 根对象，将在其内部按照路径创建/更新节点
 * @param path      点分隔路径，例如 "body.lot_infos.lot.pnl_id"
 * @param value     要写入的 JSON 值（可为标量或数组）
 */
void ParameterHelper::JsonUpdateRfidKey(QJsonObject& inputObj, const QString& path, const QJsonValue& value)
{
    if (path.trimmed().isEmpty()) return;
    QStringList parts = path.split('.', QString::SkipEmptyParts);
    if (parts.isEmpty()) return;

    // 从根对象开始递归应用
    applyPathToObject(inputObj, parts, 0, value);
}

/**
 * @brief 按路径将 QVariant 值写入到 QJsonObject（支持 QVariantList 自动转换为数组）
 * @param inputObj  目标 JSON 根对象
 * @param path      点分隔路径，例如 "body.pnl_infos.pnl_id"
 * @param value     要写入的 QVariant 值：
 *                  - 若为 QVariant::List 或可转换为 QVariantList，
 *                    将被转换为 QJsonArray 后传给 JsonUpdateRfidKey；
 *                  - 否则直接使用 QJsonValue::fromVariant(value) 转为 QJsonValue。
 *
 * @details
 *  - 这是 JsonUpdateRfidKey 的便利重载，适用于上层数据以 QVariant 形式存在的场景；
 *  - 对于列表类型，会先将每个元素转换为 QJsonValue 并组成 QJsonArray，
 *    再交由 JsonUpdateRfidKey 按路径写入；
 *  - 结合 applyPathToObject/applyPathToArray，列表值可在某些路径下自动“展开”成
 *    多条对象记录，例如：
 *       path = "body.pnl_infos.pnl_id"
 *       value = QVariantList{"P1","P2"}
 *    将生成：
 *       "body": { "pnl_infos": [ { "pnl_id": "P1" }, { "pnl_id": "P2" } ] }。
 */
void ParameterHelper::JsonUpdateRfidKeyFromVariant(QJsonObject& inputObj, const QString& path, const QVariant& value)
{
    QJsonValue jv;
    if (value.type() == QVariant::List || value.canConvert<QVariantList>()) {
        QVariantList lst = value.toList();
        QJsonArray arr;
        for (const QVariant& elem : lst) arr.append(QJsonValue::fromVariant(elem));
        jv = arr;
    }
    else {
        jv = QJsonValue::fromVariant(value);
    }
    JsonUpdateRfidKey(inputObj, path, jv);
}