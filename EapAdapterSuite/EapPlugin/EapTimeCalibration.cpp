#include "EapTimeCalibration.h"
#include <QProcess>
#include <QDebug>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

QString EapTimeCalibration::lastError_;

/**
 * @brief 获取本机 IPv4 地址
 */
QString EapTimeCalibration::getLocalIpAddress()
{
    // 获取所有网络接口
    QList<QNetworkInterface> interfaces = QNetworkInterface::allInterfaces();
    
    // 优先选择非回环的 IPv4 地址
    for (const QNetworkInterface& interfa : interfaces) {
        // 跳过未运行的接口和回环接口
        if (!(interfa.flags() & QNetworkInterface::IsUp) ||
            (interfa.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }
        
        // 获取接口的所有地址
        QList<QNetworkAddressEntry> entries = interfa.addressEntries();
        for (const QNetworkAddressEntry& entry : entries) {
            QHostAddress address = entry.ip();
            // 只返回 IPv4 地址
            if (address.protocol() == QAbstractSocket::IPv4Protocol) {
                return address.toString();
            }
        }
    }
    
    // 如果没有找到合适的地址，返回 127.0.0.1
    lastError_ = "No valid network interface found";
    return "127.0.0.1";
}

/**
 * @brief 获取当前时间字符串
 */
QString EapTimeCalibration::getLocalTime(const QString& format)
{
    return QDateTime::currentDateTime().toString(format);
}

/**
 * @brief 解析字符串并设置系统时间
 */
bool EapTimeCalibration::setSystemTime(const QString& dateTimeStr, const QString& format)
{
    QDateTime dateTime = QDateTime::fromString(dateTimeStr, format);
    if (!dateTime.isValid()) {
        lastError_ = QString("Invalid date time format: %1").arg(dateTimeStr);
        return false;
    }
    
    return setSystemTime(dateTime);
}

/**
 * @brief 真正执行“修改系统时间”的平台实现
 */
bool EapTimeCalibration::setSystemTime(const QDateTime& dateTime)
{
    if (!dateTime.isValid()) {
        lastError_ = "Invalid QDateTime object";
        return false;
    }

#ifdef Q_OS_WIN 
    // Windows 平台：使用 Windows API
    SYSTEMTIME st;
    memset(&st, 0, sizeof(st));
    
    QDate date = dateTime.date();
    QTime time = dateTime.time();
    
    st.wYear = date.year();
    st.wMonth = date.month();
    st.wDay = date.day();
    st.wHour = time.hour();
    st.wMinute = time.minute();
    st.wSecond = time.second();
    st.wMilliseconds = time.msec();
    
    if (!SetLocalTime(&st)) {
        DWORD error = GetLastError();
        lastError_ = QString("Failed to set system time. Error code: %1. Administrator privileges may be required.").arg(error);
        return false;
    }
    
    lastError_.clear();
    return true;

#elif defined(Q_OS_LINUX) || defined(Q_OS_UNIX)
    // Linux/Unix 平台：使用 date 命令
    // 格式：date -s "YYYY-MM-DD HH:MM:SS"
    QString dateTimeStr = dateTime.toString("yyyy-MM-dd HH:mm:ss");
    
    QProcess process;
    process.start("date", QStringList() << "-s" << dateTimeStr);
    
    if (!process.waitForFinished(3000)) {
        lastError_ = "Failed to execute date command: timeout";
        return false;
    }
    
    if (process.exitCode() != 0) {
        QString errorOutput = process.readAllStandardError();
        lastError_ = QString("Failed to set system time: %1. Root privileges may be required.").arg(errorOutput);
        return false;
    }
    
    lastError_.clear();
    return true;

#else // 如果不是 Windows / Linux / Unix，直接声明“不支持这个平台”
    lastError_ = "Platform not supported";
    return false;
#endif
}

/**
 * @brief 获取最近一次错误信息
 */
QString EapTimeCalibration::lastError()
{
    return lastError_;
}
