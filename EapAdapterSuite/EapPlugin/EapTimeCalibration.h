#pragma once

#include <QString>
#include <QDateTime>
#include <QHostAddress>
#include <QNetworkInterface>

/**
 * @brief EAP 时间校准工具类
 * 
 * 提供获取本机 IP 地址、获取/设置系统时间的功能。
 */
class EapTimeCalibration
{
public:
    /**
     * @brief 获取本机 IP 地址
     * @return IP 地址字符串（优先返回非回环的 IPv4 地址）
     */
    static QString getLocalIpAddress();

    /**
     * @brief 获取本机当前时间
     * @param format 时间格式字符串，默认为 "yyyy-MM-dd HH:mm:ss"
     * @return 格式化后的时间字符串
     */
    static QString getLocalTime(const QString& format = "yyyy-MM-dd HH:mm:ss");

    /**
     * @brief 设置系统时间
     * @param dateTimeStr 时间字符串
     * @param format 时间格式字符串，默认为 "yyyy-MM-dd HH:mm:ss"
     * @return true: 设置成功，false: 设置失败
     * 
     * @note Windows 系统需要管理员权限
     * @note Linux 系统需要 root 权限或使用 sudo
     */
    static bool setSystemTime(const QString& dateTimeStr, const QString& format = "yyyy-MM-dd HH:mm:ss");

    /**
     * @brief 设置系统时间（使用 QDateTime 对象） 
     * @param dateTime 要设置的时间
     * @return true: 设置成功，false: 设置失败
     */
    static bool setSystemTime(const QDateTime& dateTime);

    /**
     * @brief 获取上次操作的错误信息
     * @return 错误信息字符串
     */
    static QString lastError();


private:
    static QString lastError_;
};
