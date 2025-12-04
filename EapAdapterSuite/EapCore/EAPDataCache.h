#pragma once

#include "eapcore_global.h"
#include <QObject>
#include <QString>
#include <QVariantMap>
#include <QReadWriteLock>
#include <QSqlDatabase>
#include <QHash>
#include <QDateTime>

/**
 * @brief EAP 数据缓存系统
 * 
 * 提供线程安全的数据持久化和缓存功能。
 * 支持按 function_name.db_key 格式存储和读取数据。
 * 内置内存缓存以提高性能。
 */
class EAPCORE_EXPORT EAPDataCache : public QObject
{
    Q_OBJECT
public:
    explicit EAPDataCache(QObject* parent = nullptr);
    ~EAPDataCache() override;

    /**
     * @brief 初始化数据缓存系统
     * @param basePath 数据库文件基础路径
     * @return true 表示成功
     */
    bool initialize(const QString& basePath);

    /**
     * @brief 保存数据到缓存和数据库
     * @param saveKey 保存键，格式：function_name.db_key
     * @param data 要保存的数据（QVariantMap）
     * @return true 表示成功
     */
    bool saveData(const QString& saveKey, const QVariantMap& data);

    /**
     * @brief 从缓存或数据库读取数据
     * @param readKey 读取键，格式：function_name.db_key 或 function_name.db_key.field_name
     * @return 读取到的数据（QVariant）
     */
    QVariant readData(const QString& readKey);

    /**
     * @brief 从缓存或数据库读取数据（支持占位符）
     * @param readKeyPattern 读取键模板，支持占位符语法
     * @param placeholderValues 占位符值映射
     * @return 读取到的数据（QVariant）
     * 
     * 示例：readKeyPattern = "function_name.{key1}.{key2}"
     *      placeholderValues = {{"key1", "value1"}, {"key2", "value2"}}
     *      实际读取键 = "function_name.value1.value2"
     */
    QVariant readDataWithPlaceholders(const QString& readKeyPattern, const QVariantMap& placeholderValues);

    /**
     * @brief 从缓存或数据库读取完整记录
     * @param saveKey 保存键，格式：function_name.db_key
     * @return 完整的数据映射
     */
    QVariantMap readRecord(const QString& saveKey);

    /**
     * @brief 查询指定 function_name 的所有记录
     * @param functionName 接口名称
     * @return 记录列表，每条记录包含 db_key、data、timestamp
     */
    QList<QVariantMap> queryRecordsByFunction(const QString& functionName);

    /**
     * @brief 删除指定记录
     * @param saveKey 保存键，格式：function_name.db_key
     * @return true 表示成功
     */
    bool deleteRecord(const QString& saveKey);

    /**
     * @brief 清空指定 function_name 的所有记录
     * @param functionName 接口名称
     * @return true 表示成功
     */
    bool clearFunctionRecords(const QString& functionName);

    /**
     * @brief 设置缓存大小限制
     * @param maxSize 最大缓存条目数（0 表示无限制）
     */
    void setCacheMaxSize(int maxSize);

    /**
     * @brief 清空内存缓存（不影响数据库）
     */
    void clearCache();

    /**
     * @brief 获取上次错误信息
     */
    QString lastError() const;

    /**
     * @brief 检查是否已初始化
     */
    bool isInitialized() const;

signals:
    /**
     * @brief 数据已保存信号
     * @param saveKey 保存键
     */
    void dataSaved(const QString& saveKey);

    /**
     * @brief 数据已删除信号
     * @param saveKey 保存键
     */
    void dataDeleted(const QString& saveKey);

private:
    struct CacheEntry {
        QVariantMap data; // 缓存数据
        QDateTime timestamp; // 时间戳
        int accessCount; // 访问次数
    };

    bool createTableForFunction(const QString& functionName);
    QSqlDatabase getDatabaseForFunction(const QString& functionName);
    QString getConnectionName(const QString& functionName) const;
    bool parseSaveKey(const QString& saveKey, QString& functionName, QString& dbKey) const;
    bool parseReadKey(const QString& readKey, QString& functionName, QString& dbKey, QString& fieldName) const;
    QVariant getNestedValue(const QVariantMap& data, const QString& fieldPath) const;
    void updateCache(const QString& saveKey, const QVariantMap& data);
    bool loadFromDatabase(const QString& saveKey, QVariantMap& data);
    void evictLRUCache();

private:
    QString basePath_; // 数据库文件基础路径（./dataCache）
    bool initialized_;
    QString lastError_;
    
    // 内存缓存：saveKey -> CacheEntry
    QHash<QString, CacheEntry> cache_;
    int cacheMaxSize_; // 默认缓存数量1000
    
    // 读写锁保护缓存
    mutable QReadWriteLock cacheLock_;
    
    // 数据库连接名管理
    QHash<QString, QString> dbConnections_;
};
