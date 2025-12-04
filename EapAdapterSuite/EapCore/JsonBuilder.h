#pragma once

#include <QJsonObject>
#include <QVariantMap>
#include "EapInterfaceMeta.h"
#include "eapcore_global.h"
class EAPCORE_EXPORT JsonBuilder {
public:
    // 构造 JSON 请求包（包含 header + body）
    static QJsonObject buildPayload(const EapInterfaceMeta& meta,
        const QVariantMap& localParams);

    // 解析 JSON 响应，提取映射字段（返回设备字段结构）
    static QVariantMap parseResponse(const EapInterfaceMeta& meta,
        const QJsonObject& response);

    static QJsonObject buildHeader(const QMap<QString, QString>& headerMap, const QString& messageName);

    // map_guanxi: 右值为内部字段名，左值为 JSON 路径（支持 request_body./body.、request_head./response_head./header./head）
    // 特殊：路径片段形如 "lot[].S002" 表示在数组 lot 中找到 item_id == "S002" 的项，返回其 item_value（或继续取后缀字段）
    static QVariantMap buildMapping(const QMap<QString, QString>& map_guanxi,
        const QJsonObject& response);

// groupPath 示例：
//   "request_body.lot_infos.lot[]{key=lot_id}.pnl_infos.pnl[*].pnl_id"
// 语义：
//   - 在 lot 数组中，使用每个元素的 lot_id 作为分组 key
//   - 余下路径在每个 lot 元素下继续解析，结果写入到该 key 对应的 QVariantMap 中
//   - 内部字段名自动取余下路径的最后一个字段名（如上例为 "pnl_id"），若无法判定则为 "value"
// 返回：
//   QMap<lot_id, QVariantMap>，例如 { "A2405...23" => { "pnl_id": QStringList{...} }, ... }
    static QMap<QString, QVariantMap> buildGroupedByArrayKey(const QString& groupPath,
        const QJsonObject& response);
};