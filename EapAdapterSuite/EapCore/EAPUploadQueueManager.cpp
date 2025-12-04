// EAPUploadQueueManager.cpp
#include "EAPUploadQueueManager.h"
#include <QSqlError>
#include <QSqlRecord>
#include <QDebug>

EAPUploadQueueManager::EAPUploadQueueManager(EAPInterfaceManager* uploader, QObject* parent)
    : QObject(parent), uploader(uploader) {
    connect(&timer, &QTimer::timeout, this, &EAPUploadQueueManager::trySendNext);
    connect(uploader, &EAPInterfaceManager::requestFailed, this, &EAPUploadQueueManager::onRequestFailed);
    connect(uploader, &EAPInterfaceManager::mappedResultReady, this, &EAPUploadQueueManager::onMappedResultReady);
    initDb();
}

/**
 * @brief 尝试从队列中取出下一条任务并发送
 * @details 若当前仍有队列任务在发送中（currentId 不为空）则直接返回。
 *          若成功从本地队列中取出任务，则把 JSON 负载转换为 QVariantMap，
 *          添加 "__fromQueue__" 标记，记录当前任务 id，并调用 uploader->post() 发送。
 */
void EAPUploadQueueManager::trySendNext() {
    if (!currentId.isEmpty()) return; // 等待上一个完成
    QString id, key;
    QJsonObject payload;
    if (!dequeue(id, key, payload)) return;

    QVariantMap params = payload.toVariantMap();
    params["__fromQueue__"] = true;
    currentId = id;
    uploader->post(key, params);
}

/**
 * @brief 处理请求成功（结果已就绪）的回调
 * @param key     完成请求所对应的接口 key
 * @param result  已映射的结果数据（此处未使用）
 * @details 若 currentId 不为空，说明这是当前队列任务的成功回调，
 *          则从本地队列数据库中删除该任务并清空 currentId，
 *          以便后续可以继续发送下一条队列任务。
 */
void EAPUploadQueueManager::onMappedResultReady(const QString& key, const QVariantMap&) {
    if (!currentId.isEmpty()) {
        remove(currentId);
        currentId.clear();
    }
}

/**
 * @brief 处理请求失败的回调
 * @param key        失败请求所对应的接口 key
 * @param errorDesc  失败原因描述（此处未使用）
 * @details 若 currentId 不为空，说明失败的是队列中的任务，仅清空 currentId，
 *          保留数据库记录以便之后由定时器再次重试发送；
 *          若 currentId 为空，则认为失败的是外部直接提交的请求，
 *          若接口 key 为 "upload_panel_data"，则将其写入本地上传队列，
 *          以便后续自动重试（实际使用中应由调用方保存并传入原始参数）。
 */
void EAPUploadQueueManager::onRequestFailed(const QString& key, const QString&) {
    if (!currentId.isEmpty()) {
        // 当前是队列中的任务，保留等待下次重试
        currentId.clear();
        return;
    }
    // 否则是外部提交失败，写入队列
    if ("upload_panel_data" == key)
        submit(key, QVariantMap(), false); // 注意：这里不能写空 map，建议 caller 把原始参数保存后传入
}

/**
 * @brief 初始化上传队列使用的本地 SQLite 数据库
 * @details 创建名为 "upload_queue" 的数据库连接并打开 "upload_queue.db" 文件。
 *          若 upload_queue 表不存在则创建，该表用于存储待上传任务的接口 key
 *          及其 JSON 负载，实现跨进程/重启的持久化队列。
 */
void EAPUploadQueueManager::initDb() {
    db = QSqlDatabase::addDatabase("QSQLITE", "upload_queue");
    db.setDatabaseName("upload_queue.db");
    if (!db.open()) {
        return;
    }
    QSqlQuery query(db);
    query.exec("CREATE TABLE IF NOT EXISTS upload_queue ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "interface_key TEXT NOT NULL,"
        "json TEXT NOT NULL"
        ")");
}

void EAPUploadQueueManager::start() {
    timer.start(10000); // 每10秒检查一次
}

void EAPUploadQueueManager::stop() {
    timer.stop();
}

/**
 * @brief 提交一条需要入队的上传任务
 * @param interfaceKey 接口标识（例如 "upload_panel_data"）
 * @param params       本次请求的参数键值对
 * @param fromQueue    是否来自队列的重试请求
 * @details 若 fromQueue 为 true，表示该请求本身已在队列中，不再重复入队以避免死循环；
 *          否则将参数从 QVariantMap 转换为 QJsonObject，并调用 enqueue 将任务写入本地队列表。
 */
void EAPUploadQueueManager::submit(const QString& interfaceKey, const QVariantMap& params, bool fromQueue) {
    if (fromQueue) return; // 队列中的请求失败不再重复提交
    QJsonObject payload = QJsonObject::fromVariantMap(params);
    enqueue(interfaceKey, payload);
}

/**
 * @brief 将一条上传任务插入本地 SQLite 队列表
 * @param interfaceKey 接口标识，用于后续重试时调用对应接口
 * @param payload      需要持久化保存的请求数据（JSON 对象）
 * @details 使用已初始化的数据库连接 db，向 upload_queue 表插入一条记录，
 *          将接口名和压缩后的 JSON 字符串写入表中，以实现上传任务的持久化存储。
 */
void EAPUploadQueueManager::enqueue(const QString& interfaceKey, const QJsonObject& payload) {
    QSqlQuery query(db);
    query.prepare("INSERT INTO upload_queue (interface_key, json) VALUES (?, ?)");
    query.addBindValue(interfaceKey);
	QString jason = QJsonDocument(payload).toJson(QJsonDocument::Compact);  // 压缩 JSON
    query.addBindValue(jason);
    if (!query.exec()) {
    }
}

/**
 * @brief 从本地上传队列表中取出一条任务（只读取不删除）
 * @param id           [out] 任务在表中的主键 id（按 id 升序取最早一条）
 * @param interfaceKey [out] 接口标识，用于后续调用对应接口
 * @param payload      [out] 解析后的请求数据（JSON 对象）
 * @return true 表示成功取到一条有效记录并成功解析 JSON，false 表示队列为空、
 *         查询失败或 JSON 解析错误。
 * @details 该函数只负责读取队列头部任务，并不会删除记录；实际删除由 remove()
 *          在任务成功上传后完成。若队列中存储的 json 无法解析为对象，将返回 false。
 */
bool EAPUploadQueueManager::dequeue(QString& id, QString& interfaceKey, QJsonObject& payload) {
    QSqlQuery query(db);
    if (!query.exec("SELECT id, interface_key, json FROM upload_queue ORDER BY id ASC LIMIT 1"))
        return false;
    if (!query.next()) return false;

    id = query.value(0).toString();
    interfaceKey = query.value(1).toString();

    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(query.value(2).toByteArray(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) return false;
    payload = doc.object();
    return true;
}

/**
 * @brief 从本地上传队列表中删除指定任务
 * @param id 要删除任务在表中的主键 id
 * @details 使用已初始化的数据库连接 db，执行 DELETE 语句删除 upload_queue 表中
 *          对应 id 的记录。通常在队列任务成功上传并得到确认结果后调用。
 */
void EAPUploadQueueManager::remove(const QString& id) {
    QSqlQuery query(db);
    query.prepare("DELETE FROM upload_queue WHERE id = ?");
    query.addBindValue(id);
    query.exec();
}


