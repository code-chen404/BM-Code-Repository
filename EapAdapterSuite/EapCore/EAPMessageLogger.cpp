#include "EAPMessageLogger.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QVariant>
#include <QDebug>
#include <QFileInfo>
#include <QDir>
#include <QUuid>

// 用于EAP消息记录的数据库管理器，支持基于日期的持久化

EAPMessageLogger::EAPMessageLogger(QObject* parent)
    : QObject(parent), 
    initialized_(false)
{
    // Generate unique connection name for this instance
    connectionName_ = QStringLiteral("EAPMessageLogger_") + QUuid::createUuid().toString(QUuid::WithoutBraces); // 创建一个全局唯一的 UUID（通用唯一标识符）
}

EAPMessageLogger::~EAPMessageLogger()
{
    close(); // 关闭消息日志使用的数据库连接
}

/**
 * @brief 初始化消息日志数据库连接并创建表结构
 * @param dbPath 数据库文件的路径（可以是相对路径或绝对路径）
 * @return true  初始化成功，数据库可用
 */
bool EAPMessageLogger::initialize(const QString& dbPath)
{
    if (initialized_) {
        close(); // 关闭数据库连接
    } 

    // Ensure directory exists
    QFileInfo fileInfo(dbPath);
    QDir dir = fileInfo.absoluteDir();
    if (!dir.exists()) {
        if (!dir.mkpath(dir.absolutePath())) {
            lastError_ = QStringLiteral("无法创建数据库目录: %1").arg(dir.absolutePath());
            return false;
        }
    }

    // Create database connection
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connectionName_);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        lastError_ = QStringLiteral("无法打开数据库: %1").arg(db.lastError().text());
        QSqlDatabase::removeDatabase(connectionName_);
        return false;
    }

    if (!createTables()) {
        db.close();
        QSqlDatabase::removeDatabase(connectionName_);
        return false;
    }

    initialized_ = true;
    return true;
}

/**
 * @brief 关闭消息日志使用的数据库连接
 */
void EAPMessageLogger::close()
{
    if (initialized_) {
        QSqlDatabase db = QSqlDatabase::database(connectionName_, false);
        if (db.isOpen()) {
            db.close();
        }
        QSqlDatabase::removeDatabase(connectionName_); // 把这个连接从 Qt 的“连接池”里注销掉
        initialized_ = false;
    }
}

/**
 * @brief 创建消息日志表及相关索引
 * @return true  表及索引创建成功或已存在
 */
bool EAPMessageLogger::createTables()
{
    QSqlQuery query(getDatabase());

    // Create messages table with date-based indexing
    QString createTableSql = QStringLiteral(
        "CREATE TABLE IF NOT EXISTS eap_messages (" // 创建表 eap_messages
        "id INTEGER PRIMARY KEY AUTOINCREMENT, " // 自增主键，每条消息一条记录
        "timestamp TEXT NOT NULL, " // 精确时间戳（包含日期+时间）
        "date TEXT NOT NULL, "  // 单独存一份“日期”（不含时间），比如 "2025-11-28"
        "type INTEGER NOT NULL, " // 消息类型（整数），比如“请求 / 响应 / 错误”等
        "interface_key TEXT NOT NULL, " // 接口键值，比如接口名称 / 接口编号
        "interface_description TEXT, "  // 接口描述
        "remote_address TEXT, " // 远端地址，比如 "192.168.1.10:8080" 或 URL
        "payload TEXT NOT NULL, "  // 具体消息内容
        "is_success INTEGER NOT NULL, " // 是否成功，通常 0 / 1，表示这条消息对应操作是否成功
        "error_message TEXT" // 出错时的错误说明，比如异常文本、返回码解释等
        ")"
    );

    if (!query.exec(createTableSql)) {
        lastError_ = QStringLiteral("创建表失败: %1").arg(query.lastError().text());
        return false;
    }

    // Create indexes for efficient querying
    query.exec("CREATE INDEX IF NOT EXISTS idx_date ON eap_messages(date)"); // idx_date → 对 date 建索引，方便按日期/日期范围查
    query.exec("CREATE INDEX IF NOT EXISTS idx_type ON eap_messages(type)"); // idx_type → 对 type 建索引，方便按消息类型过滤
    query.exec("CREATE INDEX IF NOT EXISTS idx_interface_key ON eap_messages(interface_key)"); // idx_interface_key→ 对接口键建索引，方便按接口名查某个接口的所有请求/响应
    query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON eap_messages(timestamp)"); // idx_timestamp → 对时间戳建索引，方便按时间排序或做区间查询

    return true;
}

/**
 * @brief 获取当前消息日志使用的数据库连接
 * @return 当前 EAPMessageLogger 使用的 QSqlDatabase 连接句柄
 */
QSqlDatabase EAPMessageLogger::getDatabase()
{
    return QSqlDatabase::database(connectionName_);
}

/**
 * @brief 向消息日志表插入一条记录
 * @param record 要插入的消息记录对象结构体
 * @return true  插入成功
 */
bool EAPMessageLogger::insertRecord(const EAPMessageRecord& record)
{
    if (!initialized_) {
        lastError_ = QStringLiteral("sqlite not initialized");
        return false;
    }

    QSqlQuery query(getDatabase());
    query.prepare(
        "INSERT INTO eap_messages (timestamp, date, type, interface_key, interface_description, remote_address, payload, is_success, error_message) "
        "VALUES (:timestamp, :date, :type, :interface_key, :interface_description, :remote_address, :payload, :is_success, :error_message)" //使用命名占位符（:timestamp 等），防止 SQL 注入，也方便绑定参数
    );

    query.bindValue(":timestamp", record.timestamp.toString(Qt::ISODate)); // 精确时间戳（包含日期+时间）
    query.bindValue(":date", record.timestamp.date().toString(Qt::ISODate)); // 单独存一份“日期”（不含时间），比如 "2025-11-28"
    query.bindValue(":type", static_cast<int>(record.type)); // 消息类型（整数），比如“请求 / 响应 / 错误”等
    query.bindValue(":interface_key", record.interfaceKey); // 接口键值，比如接口名称 / 接口编号
    query.bindValue(":interface_description", record.interfaceDescription); // 接口描述
    query.bindValue(":remote_address", record.remoteAddress); // 远端地址，比如 "192.168.1.10:8080" 或 URL
    
    QJsonDocument doc(record.payload);
    query.bindValue(":payload", QString::fromUtf8(doc.toJson(QJsonDocument::Compact))); // 具体消息内容
    
    query.bindValue(":is_success", record.isSuccess ? 1 : 0); // 是否成功，通常 0 / 1，表示这条消息对应操作是否成功
    query.bindValue(":error_message", record.errorMessage); // 出错时的错误说明，比如异常文本、返回码解释等

    if (!query.exec()) {
        lastError_ = QStringLiteral("插入记录失败: %1").arg(query.lastError().text());
        return false;
    }

    emit recordInserted(record);
    return true;
}

/**
 * @brief 按日期范围查询消息日志记录
 * @param startDate 查询起始日期（包含该日）
 * @param endDate   查询结束日期（包含该日）
 * @return QVector<EAPMessageRecord> 查询到的消息记录列表，按时间戳降序排列
 */
QVector<EAPMessageRecord> EAPMessageLogger::queryByDateRange(const QDate& startDate, const QDate& endDate)
{
    QVector<EAPMessageRecord> results;

    if (!initialized_) {
        lastError_ = QStringLiteral("sql not initialized");
        return results;
    }

    QSqlQuery query(getDatabase());
    query.prepare(
        "SELECT id, timestamp, type, interface_key, interface_description, remote_address, payload, is_success, error_message "
        "FROM eap_messages "
        "WHERE date >= :start_date AND date <= :end_date " // 使用之前表里的 date 字段（只有日期的那一列）
        "ORDER BY timestamp DESC" // 按时间戳从最近到最早排序
    );

    query.bindValue(":start_date", startDate.toString(Qt::ISODate)); // 把 startDate、endDate 转成 YYYY-MM-DD 这种 ISO 字符串格式，绑定到 SQL 的命名占位符
    query.bindValue(":end_date", endDate.toString(Qt::ISODate));

    if (!query.exec()) {
        lastError_ = QStringLiteral("查询失败: %1").arg(query.lastError().text());
        return results;
    }

    while (query.next()) {
        EAPMessageRecord record;
        record.id = query.value(0).toLongLong();
        record.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
        record.type = static_cast<EAPMessageRecord::MessageType>(query.value(2).toInt());
        record.interfaceKey = query.value(3).toString();
        record.interfaceDescription = query.value(4).toString();
        record.remoteAddress = query.value(5).toString();
        
        QJsonDocument doc = QJsonDocument::fromJson(query.value(6).toString().toUtf8());
        record.payload = doc.object(); // QJsonObject（具体消息内容）
        
        record.isSuccess = query.value(7).toInt() == 1; // 1 表示成功，0 表示失败
        record.errorMessage = query.value(8).toString(); // 出错时的错误说明

        results.append(record);
    }

    emit queryCompleted(results.size()); // 本次查询返回了多少条记录
    return results;
}

/**
 * @brief 查询指定日期的消息日志记录
 * @param date 要查询的日期（仅包含该日期当天的记录）QDate(2025, 11, 28)
 * @return QVector<EAPMessageRecord> 指定日期的消息记录列表
 */
QVector<EAPMessageRecord> EAPMessageLogger::queryByDate(const QDate& date)
{
    return queryByDateRange(date, date); // 开始日期 = 结束日期 = 同一天
}

/**
 * @brief 按消息类型和日期范围查询消息日志记录
 * @param type      要筛选的消息类型
 * @param startDate 查询起始日期（包含该日）
 * @param endDate   查询结束日期（包含该日）
 * @return QVector<EAPMessageRecord> 符合类型和日期范围条件的消息记录列表，
 *                                   按时间戳降序排列
 */
QVector<EAPMessageRecord> EAPMessageLogger::queryByType(EAPMessageRecord::MessageType type, const QDate& startDate, const QDate& endDate)
{
    QVector<EAPMessageRecord> results;

    if (!initialized_) {
        lastError_ = QStringLiteral("sql not initialized");
        return results;
    }

    QSqlQuery query(getDatabase());
    query.prepare(
        "SELECT id, timestamp, type, interface_key, interface_description, remote_address, payload, is_success, error_message "
        "FROM eap_messages "
        "WHERE type = :type AND date >= :start_date AND date <= :end_date " // 筛选出指定类型、并且日期在区间 [startDate, endDate] 内的记录
        "ORDER BY timestamp DESC"
    );

    query.bindValue(":type", static_cast<int>(type)); // 将枚举 MessageType 转成 int 绑定到 :type
    query.bindValue(":start_date", startDate.toString(Qt::ISODate)); //起始和结束日期转换成 "YYYY-MM-DD" ISO 字符串，绑定到 :start_date、:end_date
    query.bindValue(":end_date", endDate.toString(Qt::ISODate));

    if (!query.exec()) {
        lastError_ = QStringLiteral("查询失败: %1").arg(query.lastError().text());
        return results;
    }

    while (query.next()) {
        EAPMessageRecord record;
        record.id = query.value(0).toLongLong();
        record.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
        record.type = static_cast<EAPMessageRecord::MessageType>(query.value(2).toInt());
        record.interfaceKey = query.value(3).toString();
        record.interfaceDescription = query.value(4).toString();
        record.remoteAddress = query.value(5).toString();
        
        QJsonDocument doc = QJsonDocument::fromJson(query.value(6).toString().toUtf8());
        record.payload = doc.object(); 
        
        record.isSuccess = query.value(7).toInt() == 1;
        record.errorMessage = query.value(8).toString();

        results.append(record);
    }

    emit queryCompleted(results.size()); // 本次查询返回了多少条记录
    return results;
}

/**
 * @brief 按接口键和日期范围查询消息日志记录
 * @param interfaceKey 要筛选的接口键（interface_key 字段的值）
 * @param startDate    查询起始日期（包含该日）
 * @param endDate      查询结束日期（包含该日）
 * @return QVector<EAPMessageRecord> 符合接口键和日期范围条件的消息记录列表，
 *                                   按时间戳降序排列
 */
QVector<EAPMessageRecord> EAPMessageLogger::queryByInterfaceKey(const QString& interfaceKey, const QDate& startDate, const QDate& endDate)
{
    QVector<EAPMessageRecord> results;

    if (!initialized_) {
        lastError_ = QStringLiteral("sql not initialized");
        return results;
    }

    QSqlQuery query(getDatabase());
    query.prepare(
        "SELECT id, timestamp, type, interface_key, interface_description, remote_address, payload, is_success, error_message "
        "FROM eap_messages "
        "WHERE interface_key = :interface_key AND date >= :start_date AND date <= :end_date "
        "ORDER BY timestamp DESC"
    );

    query.bindValue(":interface_key", interfaceKey); // 绑定你传进来的接口键（函数名 / 接口标识）
    query.bindValue(":start_date", startDate.toString(Qt::ISODate)); // 日期转为 ISO 格式字符串 "YYYY-MM-DD"，与建表时 date 列保持一致
    query.bindValue(":end_date", endDate.toString(Qt::ISODate));

    if (!query.exec()) {
        lastError_ = QStringLiteral("查询失败: %1").arg(query.lastError().text());
        return results;
    }

    while (query.next()) {
        EAPMessageRecord record;
        record.id = query.value(0).toLongLong();
        record.timestamp = QDateTime::fromString(query.value(1).toString(), Qt::ISODate);
        record.type = static_cast<EAPMessageRecord::MessageType>(query.value(2).toInt());
        record.interfaceKey = query.value(3).toString();
        record.interfaceDescription = query.value(4).toString();
        record.remoteAddress = query.value(5).toString();
        
        QJsonDocument doc = QJsonDocument::fromJson(query.value(6).toString().toUtf8());
        record.payload = doc.object();
        
        record.isSuccess = query.value(7).toInt() == 1;
        record.errorMessage = query.value(8).toString();

        results.append(record);
    }

    emit queryCompleted(results.size()); // 这次查了多少条
    return results;
}

/**
 * @brief 删除指定日期之前的历史消息日志记录
 * @param date 作为保留边界的日期，删除所有 date < 此日期的记录
 * @return true  删除操作执行成功
 */
bool EAPMessageLogger::deleteRecordsBefore(const QDate& date)
{
    if (!initialized_) {
        lastError_ = QStringLiteral("sql not initialized");
        return false;
    }

    QSqlQuery query(getDatabase());
    query.prepare("DELETE FROM eap_messages WHERE date < :date");
    query.bindValue(":date", date.toString(Qt::ISODate)); // 使用 ISO 格式的日期字符串（例如 "2025-11-28"），和表里的 date 字段格式保持一致

    if (!query.exec()) {
        lastError_ = QStringLiteral("删除记录失败: %1").arg(query.lastError().text());
        return false;
    }

    return true;
}

/**
 * @brief 获取最近一次数据库或日志操作的错误信息
 * @return QString 最近一次操作的错误信息；若此前未发生错误，可能为空字符串
 */
QString EAPMessageLogger::lastError() const
{
    return lastError_;
}

/**
 * @brief 判断消息日志模块是否已成功初始化
 * @return true  已初始化完成，可以正常进行数据库操作
 */
bool EAPMessageLogger::isInitialized() const
{
    return initialized_;
}
