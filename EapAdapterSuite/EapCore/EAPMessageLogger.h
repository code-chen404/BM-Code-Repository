#pragma once
#ifdef _MSVC_LANG
#pragma execution_character_set("utf-8")
#endif 
#include "eapcore_global.h"
#include "EAPMessageRecord.h"
#include <QObject>
#include <QString>
#include <QDate>
#include <QVector>
#include <QSqlDatabase>

// 用于EAP消息记录的数据库管理器，支持基于日期的持久化
class EAPCORE_EXPORT EAPMessageLogger : public QObject
{
    Q_OBJECT
public:
    explicit EAPMessageLogger(QObject* parent = nullptr);
    ~EAPMessageLogger() override;

    // Initialize database with specified path (creates database if not exists)
    bool initialize(const QString& dbPath);

    // Close database connection
    void close();

    // Insert a new message record
    bool insertRecord(const EAPMessageRecord& record);

    // Query records by date range
    QVector<EAPMessageRecord> queryByDateRange(const QDate& startDate, const QDate& endDate);

    // Query records by date (single day)
    QVector<EAPMessageRecord> queryByDate(const QDate& date);

    // Query records by type
    QVector<EAPMessageRecord> queryByType(EAPMessageRecord::MessageType type, const QDate& startDate, const QDate& endDate);

    // Query records by interface key
    QVector<EAPMessageRecord> queryByInterfaceKey(const QString& interfaceKey, const QDate& startDate, const QDate& endDate);

    // Delete records older than specified date
    bool deleteRecordsBefore(const QDate& date);

    // Get last error message
    QString lastError() const;

    // Check if database is initialized
    bool isInitialized() const;

signals:
    void recordInserted(const EAPMessageRecord& record);
    void queryCompleted(int recordCount);

private: 
    bool createTables();
    QSqlDatabase getDatabase();
    QString connectionName_;
    QString lastError_;
    bool initialized_;
};
