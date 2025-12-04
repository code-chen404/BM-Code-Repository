#pragma once

#include <QJsonObject>
#include <QVariant>
#include <QString>
#include <QList>
#include "EapInterfaceMeta.h"
#include "eapcore_global.h"

class EAPCORE_EXPORT JsonParser {
public:
    // 解析点路径，如 "request_body.alarm_id" 或 "arrayField[0].name"
    // 若路径不存在，返回无效 QVariant()
    static QVariant parseJson(const EapInterfaceMeta& meta,
        const QJsonObject& jsonObj,
        const QString& path);

    // ---------- 新增帮助函数：支持 `.name[]` 数组遍历的占位符解析 ----------
    static void collectValuesByPath(const QJsonValue& node, const QStringList& parts, int idx, QList<QVariant>& out);

    // path 形如 "body.edc_infos.edc[].item_id" 或 "body.carrier_id", 返回 QVariant:
    // - 若匹配单个值，则返回该标量 QVariant
    // - 若匹配多个值，则返回 QVariantList
    // - 若未匹配到任何值，返回 invalid QVariant()
    static QVariant resolvePlaceholderValue(const QJsonObject& normalized, const QString& placeholder);


private:
    struct PathElement {
        enum Kind {
            Field, Index
        } kind;
        QString field;  // kind == Field
        int index = -1; // kind == Index
    };

    // 将如 "a.b[0].c[1][2]" 切分为 {Field:a, Field:b, Index:0, Field:c, Index:1, Index:2}
    static QList<PathElement> tokenizePath(const QString& path); 

    // 兼容“键值对数组”老语义路径：
    // [body|request_body].(parameter_list|para_list).(parameter_name|para_name).(parameter_value|para_value).<matchKey>
    // 命中后会在对应数组中找到 key=matchKey 的项并返回其 value
    static bool isKvArrayPath(const QStringList& parts,
        int& baseIndex,
        QString& arrayName,
        QString& keyField,
        QString& valField,
        QString& matchKey);
};