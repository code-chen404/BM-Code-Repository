#include "EAPDataCache.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDir>
#include <QDebug>
#include <QWriteLocker>
#include <QReadLocker>


/**
 * @brief EAP 数据缓存系统
 *
 * 提供线程安全的数据持久化和缓存功能。
 * 支持按 function_name.db_key 格式存储和读取数据。
 * 内置内存缓存以提高性能。
 */


EAPDataCache::EAPDataCache(QObject* parent)
    : QObject(parent)
    , initialized_(false)
    , cacheMaxSize_(1000)  // 默认缓存 1000 条记录
{
}

EAPDataCache::~EAPDataCache()
{
    // 关闭所有数据库连接
    for (const QString& connName : dbConnections_.values()) {
        QSqlDatabase::removeDatabase(connName);
    }
}

/**
 * @brief 初始化数据缓存系统
 * @param basePath 数据库文件基础路径
 * @return true 表示成功
 */
bool EAPDataCache::initialize(const QString& basePath)
{
    basePath_ = basePath;
    
    // 确保目录存在
    QDir dir(basePath_);
    if (!dir.exists()) {
        if (!dir.mkpath(".")) {
            lastError_ = QString("Failed to create directory: %1").arg(basePath_); // 记录创建错误信息
            return false;
        }
    }
    
    initialized_ = true;
    lastError_.clear();
    return true;
}

/**
 * @brief 保存数据到缓存和数据库
 * @param saveKey 保存键，格式：function_name.db_key
 * @param data 要保存的数据（QVariantMap）
 * @return true 表示成功
 */
bool EAPDataCache::saveData(const QString& saveKey, const QVariantMap& data)
{
    // 缓存路径不存在
    if (!initialized_) {
        lastError_ = "Data cache not initialized";
        return false;
    }
        
    QString functionName, dbKey;
    if (!parseSaveKey(saveKey, functionName, dbKey)) { // 解析一个保存键（saveKey），将其拆分成 函数名 和 数据库主键
        lastError_ = QString("Invalid save key format: %1").arg(saveKey);
        return false;
    }
        
    updateCache(saveKey, data); // 负责将数据更新到内存缓存中
    
    // 保存到数据库
    QSqlDatabase db = getDatabaseForFunction(functionName); //获取与指定的 functionName 相关的数据库连接
    if (!db.isValid()) { //数据库连接是否已成功建立
        lastError_ = "Failed to get database connection";
        return false;
    }
    
    // 确保表存在
    if (!createTableForFunction(functionName)) { // 创建基于 functionName 的数据库表格
        return false;
    }
    
    // 将 QVariantMap 转换为 JSON
    QJsonDocument doc = QJsonDocument::fromVariant(data);
    QString jsonData = QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    
    // 使用 REPLACE INTO 实现插入或更新
    QSqlQuery query(db);
    query.prepare("REPLACE INTO cache_data (db_key, data, timestamp) VALUES (?, ?, ?)"); // 将 SQL 语句进行预处理，有记录，那么进行更新，没有记录，那么进行插入记录
    query.addBindValue(dbKey); // 主键
    query.addBindValue(jsonData); // data 字段
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate)); // 当前的日期和时间
    
    if (!query.exec()) {
        lastError_ = QString("Failed to save data: %1").arg(query.lastError().text());
        return false;
    }
    
    emit dataSaved(saveKey); // 发出信号通知数据已保存
    lastError_.clear();
    return true;
}

/**
 * @brief 从缓存或数据库读取数据
 * @param readKey 读取键，格式：function_name.db_key 或 function_name.db_key.field_name
 * @return 读取到的数据（QVariant）
 */
QVariant EAPDataCache::readData(const QString& readKey)
{
    QString functionName, dbKey, fieldName;
    if (!parseReadKey(readKey, functionName, dbKey, fieldName)) { // 把读取键 readKey 按约定格式拆开为3部分
        lastError_ = QString("Invalid read key format: %1").arg(readKey);
        return QVariant();
    }
    
    QString saveKey = QString("%1.%2").arg(functionName, dbKey);
    
    // 先尝试从缓存读取
    {
        QReadLocker locker(&cacheLock_);
        if (cache_.contains(saveKey)) {
            CacheEntry& entry = cache_[saveKey];
            entry.accessCount++;
            
            if (fieldName.isEmpty()) {
                return QVariant(entry.data);
            } else {
                return getNestedValue(entry.data, fieldName); // 根据字段路径从嵌套的 QVariantMap 中获取对应的值QVariant
            }
        }
    }
    
    // 缓存未命中，从数据库加载
    QVariantMap data;
    if (loadFromDatabase(saveKey, data)) { // 从数据库中加载指定保存键对应的记录
        updateCache(saveKey, data); // 负责将数据更新到内存缓存中
        
        if (fieldName.isEmpty()) {
            return QVariant(data);
        } else { 
            return getNestedValue(data, fieldName); // 根据字段路径从嵌套的 QVariantMap 中获取对应的值QVariant
        }
    }
    
    return QVariant();
}

/**
* @brief 从缓存或数据库读取完整记录
* @param saveKey 保存键，格式：function_name.db_key
* @return 完整的数据映射
*/
QVariantMap EAPDataCache::readRecord(const QString& saveKey)
{
    if (!initialized_) {
        lastError_ = "Data cache not initialized";
        return QVariantMap();
    }
    
    QString functionName, dbKey;
    if (!parseSaveKey(saveKey, functionName, dbKey)) { // 解析一个保存键（saveKey），将其拆分成 函数名 和 数据库键
        lastError_ = QString("Invalid save key format: %1").arg(saveKey);
        return QVariantMap();
    }
    
    // 先尝试从缓存读取
    {
        QReadLocker locker(&cacheLock_);
        if (cache_.contains(saveKey)) {
            CacheEntry& entry = cache_[saveKey];
            entry.accessCount++;
            return entry.data;
        }
    }
    
    // 从数据库加载
    QVariantMap data;
    if (loadFromDatabase(saveKey, data)) { // 从数据库中加载指定保存键对应的记录
        updateCache(saveKey, data); // 负责将数据更新到内存缓存中
        return data;
    }
    
    return QVariantMap();
}

/**
 * @brief 查询指定 function_name 的所有记录
 * @param functionName 接口名称
 * @return 记录列表，每条记录包含 db_key、data、timestamp
 */
QList<QVariantMap> EAPDataCache::queryRecordsByFunction(const QString& functionName)
{
    QList<QVariantMap> results;
    
    if (!initialized_) {
        lastError_ = "Data cache not initialized";
        return results;
    }
    
    QSqlDatabase db = getDatabaseForFunction(functionName); // 获取与指定的 functionName 相关的数据库连接
    if (!db.isValid()) {
        lastError_ = "Failed to get database connection";
        return results;
    }
    
    QSqlQuery query(db);
    if (!query.exec("SELECT db_key, data, timestamp FROM cache_data ORDER BY timestamp DESC")) {
        lastError_ = QString("Failed to query records: %1").arg(query.lastError().text());
        return results;
    }
    
    while (query.next()) {
        QVariantMap record;
        record["db_key"] = query.value(0).toString();
        
        QString jsonData = query.value(1).toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        record["data"] = doc.toVariant();
        
        record["timestamp"] = query.value(2).toString();
        results.append(record);
    }
    
    lastError_.clear();
    return results;
}

/**
 * @brief 删除指定记录
 * @param saveKey 保存键，格式：function_name.db_key
 * @return true 表示成功
 */
bool EAPDataCache::deleteRecord(const QString& saveKey)
{
    if (!initialized_) {
        lastError_ = "Data cache not initialized";
        return false;
    }
    
    QString functionName, dbKey;
    if (!parseSaveKey(saveKey, functionName, dbKey)) { // 解析一个保存键（saveKey），将其拆分成 函数名 和 数据库键
        lastError_ = QString("Invalid save key format: %1").arg(saveKey);
        return false;
    }
    
    // 从缓存删除
    {
        QWriteLocker locker(&cacheLock_);
        cache_.remove(saveKey);
    }
    
    // 从数据库删除
    QSqlDatabase db = getDatabaseForFunction(functionName); // 获取与指定的 functionName 相关的数据库连接
    if (!db.isValid()) {
        lastError_ = "Failed to get database connection";
        return false;
    }
    
    QSqlQuery query(db);
    query.prepare("DELETE FROM cache_data WHERE db_key = ?");
    query.addBindValue(dbKey);
    
    if (!query.exec()) {
        lastError_ = QString("Failed to delete record: %1").arg(query.lastError().text());
        return false;
    }
    
    emit dataDeleted(saveKey);
    lastError_.clear();
    return true;
}

/**
 * @brief 清空指定 function_name 的所有记录
 * @param functionName 接口名称
 * @return true 表示成功
 */
bool EAPDataCache::clearFunctionRecords(const QString& functionName)
{
    if (!initialized_) {
        lastError_ = "Data cache not initialized";
        return false;
    }
    
    // 从缓存删除相关记录
    {
        QWriteLocker locker(&cacheLock_);
        QStringList keysToRemove;
        for (auto it = cache_.begin(); it != cache_.end(); ++it) {
            if (it.key().startsWith(functionName + ".")) { // 是不是以 "functionName." 开头
                keysToRemove.append(it.key());
            }
        }
        for (const QString& key : keysToRemove) {
            cache_.remove(key);
        }
    }
    
    // 从数据库删除
    QSqlDatabase db = getDatabaseForFunction(functionName); // 获取与指定的 functionName 相关的数据库连接
    if (!db.isValid()) {
        lastError_ = "Failed to get database connection";
        return false;
    }
    
    QSqlQuery query(db);
    if (!query.exec("DELETE FROM cache_data")) { // 清掉这个库里的所有缓存记录
        lastError_ = QString("Failed to clear records: %1").arg(query.lastError().text());
        return false;
    }
    
    lastError_.clear();
    return true;
}

/**
 * @brief 设置缓存大小限制
 * @param maxSize 最大缓存条目数（0 表示无限制）
 */
void EAPDataCache::setCacheMaxSize(int maxSize)
{
    cacheMaxSize_ = maxSize;
}

/**
 * @brief 清空内存缓存（不影响数据库）
 */
void EAPDataCache::clearCache()
{
    QWriteLocker locker(&cacheLock_);
    cache_.clear();
}

/**
 * @brief 获取上次错误信息
 */
QString EAPDataCache::lastError() const
{
    return lastError_;
}

/**
 * @brief 检查是否已初始化
 */
bool EAPDataCache::isInitialized() const
{
    return initialized_;
}

// ============================================================================
// Private Methods
// ============================================================================

/**
 * @brief 创建基于 functionName 的数据库表格
 * @param functionName 解析后的函数名
 * @return true 创建成功
 */
bool EAPDataCache::createTableForFunction(const QString& functionName)
{
    QSqlDatabase db = getDatabaseForFunction(functionName); // 获取数据库连接
    if (!db.isValid()) {
        lastError_ = "Failed to get database connection";
        return false;
    }
    
    QSqlQuery query(db); // 创建查询对象

    QString sql = R"( // 表格cache_data格式
        CREATE TABLE IF NOT EXISTS cache_data (
            db_key TEXT PRIMARY KEY,
            data TEXT NOT NULL,
            timestamp TEXT NOT NULL
        )
    )";
    
    if (!query.exec(sql)) { // 创建表格
        lastError_ = QString("Failed to create table: %1").arg(query.lastError().text());
        return false;
    }
    
    // 创建索引
    query.exec("CREATE INDEX IF NOT EXISTS idx_timestamp ON cache_data(timestamp)");
    
    return true;
} 

/**
 * @brief 获取与指定的 functionName 相关的数据库连接
 * @param functionName 解析后的函数名
 * @return QSqlDatabase 数据库连接对象
 */
QSqlDatabase EAPDataCache::getDatabaseForFunction(const QString& functionName)
{
    QString connName = getConnectionName(functionName);
    
    // 检查是否已经有连接
    if (QSqlDatabase::contains(connName)) {
        return QSqlDatabase::database(connName);
    }
    
    // 创建新连接
    QString dbPath = QString("%1/%2.db").arg(basePath_, functionName);
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName); // 创建一个新的数据库连接，指定连接类型为 "QSQLITE"（SQLite 数据库）
    db.setDatabaseName(dbPath);
    
    if (!db.open()) {
        lastError_ = QString("Failed to open database: %1").arg(db.lastError().text());
        return QSqlDatabase(); // 返回一个无效的 QSqlDatabase 对象
    }
    
    dbConnections_[functionName] = connName; // 将 functionName 与该连接名称关联起来
    return db;
}

/**
 * @brief 根据 functionName 和当前对象的地址生成一个唯一的数据库连接名称
 * @param saveKey 要解析的键值
 * @param functionName 解析后的函数名
 * @return QString 数据库连接名称
 */
QString EAPDataCache::getConnectionName(const QString& functionName) const
{
    return QString("EAPDataCache_%1_%2").arg(functionName).arg(reinterpret_cast<quintptr>(this));
}

/**
 * @brief 解析一个保存键（saveKey），将其拆分成 函数名 和 数据库键
 * @param saveKey 要解析的键值
 * @param functionName 解析后的函数名
 * @param dbKey 解析后的数据库键
 * @return true 解析成功
 */
bool EAPDataCache::parseSaveKey(const QString& saveKey, QString& functionName, QString& dbKey) const
{
    int dotPos = saveKey.indexOf('.');
    if (dotPos <= 0 || dotPos >= saveKey.length() - 1) {
        return false;
    }
    
    functionName = saveKey.left(dotPos);
    dbKey = saveKey.mid(dotPos + 1);
    return true;
}

/**
 * @brief 把读取键 readKey 按约定格式拆开为3部分
 * @param readKey 要解析的字符串
 * @param functionName 函数名
 * @param dbKey 数据库键
 * @param fieldName 最后的字符串
 * @return true 解析成功
 */
bool EAPDataCache::parseReadKey(const QString& readKey, QString& functionName, QString& dbKey, QString& fieldName) const
{
    // 按 . 分割字符串
    QStringList parts = readKey.split('.');
    if (parts.size() < 2) {
        return false;
    }
    
    // 取出前两段：函数名 + 数据库键
    functionName = parts[0];
    dbKey = parts[1];
    
    // 如果有第三部分及以后，组合成字段路径
    if (parts.size() > 2) {
        fieldName = parts.mid(2).join('.');
    } else {
        fieldName.clear();
    }
    
    return true;
}

/**
 * @brief 根据字段路径从嵌套的 QVariantMap 中获取对应的值QVariant
 * @param data    源数据，可能包含多层嵌套的 QVariantMap
 * @param fieldPath 字段路径，使用`.`分隔各级键名，如 "address.city.name"
 * @return 如果路径存在，返回对应字段的值；如果任一层级不存在或类型不是 Map，则返回一个无效的 QVariant
 */
QVariant EAPDataCache::getNestedValue(const QVariantMap& data, const QString& fieldPath) const
{
    QStringList parts = fieldPath.split('.');
    QVariant current = QVariant(data);

      for (const QString& part : parts) {
          if (current.type() != QVariant::Map) {
              return QVariant();
          }

          QVariantMap map = current.toMap();
          if (!map.contains(part)) {
              return QVariant();
          }

          current = map[part];
      }
    return current;
}

/**
 * @brief 负责将数据更新到内存缓存中
 * @param saveKey 唯一标识缓存项的键
 * @param data 要保存的数据
 * @return true 解析成功
 */
void EAPDataCache::updateCache(const QString& saveKey, const QVariantMap& data)
{
    QWriteLocker locker(&cacheLock_);
    
    // 检查是否需要清理缓存
    if (cacheMaxSize_ > 0 && cache_.size() >= cacheMaxSize_) {
        evictLRUCache();
    }
    
    CacheEntry entry;
    entry.data = data;
    entry.timestamp = QDateTime::currentDateTime();
    entry.accessCount = 1;
    
    cache_[saveKey] = entry;
}

/**
 * @brief 从数据库中加载指定保存键对应的记录
 * @param saveKey 保存键，格式：function_name.db_key，用于定位数据库记录
 * @param data 输出参数，用于接收从数据库加载出的数据（QVariantMap）
 * @return true 表示加载成功，false 表示失败（错误信息可通过 lastError_ 获取）
 */
bool EAPDataCache::loadFromDatabase(const QString& saveKey, QVariantMap& data)
{
    QString functionName, dbKey;
    if (!parseSaveKey(saveKey, functionName, dbKey)) { // 解析一个保存键（saveKey），将其拆分成 函数名 和 数据库键
        lastError_ = QString("Invalid save key format: %1").arg(saveKey);
        return false;
    }
    
    QSqlDatabase db = getDatabaseForFunction(functionName); // 获取与指定的 functionName 相关的数据库连接
    if (!db.isValid()) {
        lastError_ = "Failed to get database connection";
        return false;
    }
    
    QSqlQuery query(db);
    query.prepare("SELECT data FROM cache_data WHERE db_key = ?");
    query.addBindValue(dbKey); // 查询主键
    
    if (!query.exec()) {
        lastError_ = QString("Failed to query data: %1").arg(query.lastError().text());
        return false;
    }
    
    if (query.next()) {
        QString jsonData = query.value(0).toString();
        QJsonDocument doc = QJsonDocument::fromJson(jsonData.toUtf8());
        data = doc.toVariant().toMap();
        return true;
    }
    
    lastError_ = "Record not found";
    return false;
}

/**
 * @brief 在缓存达到最大大小时，自动清理不常用或最旧的缓存数据，确保缓存不会超出设定的大小限制
 */
void EAPDataCache::evictLRUCache() 
{
    // 找到访问次数最少且最旧的记录 
    if (cache_.isEmpty()) 
    { 
        return; 
    }

    QString keyToRemove;
    int minAccessCount = INT_MAX;
    QDateTime oldestTime = QDateTime::currentDateTime();

    for (auto it = cache_.begin(); it != cache_.end(); ++it)
    {
        if (it.value().accessCount < minAccessCount ||
            (it.value().accessCount == minAccessCount && it.value().timestamp < oldestTime))
        {
            keyToRemove = it.key();
            minAccessCount = it.value().accessCount;
            oldestTime = it.value().timestamp;
        }
    }

    if (!keyToRemove.isEmpty())
    {
        cache_.remove(keyToRemove);
    }
}

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
QVariant EAPDataCache::readDataWithPlaceholders(const QString& readKeyPattern, const QVariantMap& placeholderValues)
{
    if (!initialized_) {
        lastError_ = "Data cache not initialized";
        return QVariant();
    }
    
    // 展开占位符构建实际的读取键
    QString actualReadKey;
    int pos = 0;
    while (pos < readKeyPattern.length()) {
        int startBrace = readKeyPattern.indexOf('{', pos);
        if (startBrace == -1) {
            // 没有更多占位符，添加剩余部分
            actualReadKey.append(readKeyPattern.mid(pos));
            break;
        }
        
        // 添加占位符之前的文本
        if (startBrace > pos) {
            actualReadKey.append(readKeyPattern.mid(pos, startBrace - pos));
        }
        
        // 查找匹配的 }
        int endBrace = readKeyPattern.indexOf('}', startBrace);
        if (endBrace == -1) {
            // 格式错误
            lastError_ = QString("Invalid placeholder syntax in pattern: %1").arg(readKeyPattern);
            return QVariant();
        }
        
        // 提取占位符名称并替换为实际值
        QString placeholderName = readKeyPattern.mid(startBrace + 1, endBrace - startBrace - 1);
        if (placeholderValues.contains(placeholderName)) {
            actualReadKey.append(placeholderValues.value(placeholderName).toString());
        } else {
            lastError_ = QString("Placeholder '%1' not found in pattern '%2'").arg(placeholderName, readKeyPattern);
            return QVariant();
        }
        
        pos = endBrace + 1;
    }
    
    // 使用展开后的键读取数据
    return readData(actualReadKey); // 从缓存或数据库读取数据
}
