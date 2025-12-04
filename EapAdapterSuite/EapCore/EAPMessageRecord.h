#pragma once

#include "eapcore_global.h"
#include <QString>
#include <QDateTime>
#include <QJsonObject>

// EAP通信日志记录的消息记录结构
struct EAPCORE_EXPORT EAPMessageRecord
{
    enum MessageType {
        InterfaceManagerSent,      // EAPInterfaceManager 发送出去的消息（例如：向 MES 发送请求）
        InterfaceManagerReceived,  // EAPInterfaceManager 收到的响应
        WebServiceReceived,        // EAPWebService 收到的请求（比如外部系统调用你这边的 Web API）
        WebServiceSent            // EAPWebService 发出的响应
    };

    qint64 id;                    // 自增主键，每条消息一条记录
    QDateTime timestamp;          // 精确时间戳（包含日期+时间）
    MessageType type;             // 消息类型（整数），比如“请求 / 响应 / 错误”等
    QString interfaceKey;         // 接口键值，比如接口名称 / 接口编号
    QString interfaceDescription; // 接口描述
    QString remoteAddress;        // 远端地址，比如 "192.168.1.10:8080" 或 URL
    QJsonObject payload;          // 具体消息内容
    bool isSuccess;               // 是否成功，通常 0 / 1，表示这条消息对应操作是否成功
    QString errorMessage;         // 出错时的错误说明，比如异常文本、返回码解释等

    EAPMessageRecord()
        : id(0), // 还没写进数据库
        type(InterfaceManagerSent), // 默认当成“EAPInterfaceManager 发送”类型，可以根据实际再改
        isSuccess(true) // 默认认为是成功
    {}

    // 将类型转换为字符串以供显示
    static QString typeToString(MessageType type) {
        switch (type) {
        case InterfaceManagerSent:
            return QStringLiteral("EAP Send");
        case InterfaceManagerReceived:
            return QStringLiteral("EAP receive");
        case WebServiceReceived:
            return QStringLiteral("Web receive");
        case WebServiceSent:
            return QStringLiteral("Web send");
        default:
            return QStringLiteral("未知");
        }
    }

    // 将类型从字符串转换
    static MessageType stringToType(const QString& str) {
        if (str == QStringLiteral("EAP Send")) return InterfaceManagerSent;
        if (str == QStringLiteral("EAP receive")) return InterfaceManagerReceived;
        if (str == QStringLiteral("Web send")) return WebServiceSent;
        if (str == QStringLiteral("Web receive")) return WebServiceReceived;
        return InterfaceManagerSent;
    }
};
