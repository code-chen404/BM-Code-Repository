// 这是一个展示如何使用EAP消息日志功能的示例代码
// This is an example demonstrating how to use the EAP message logging functionality

#include "EAPInterfaceManager.h"
#include "EAPWebService.h"
#include "EAPMessageLogger.h"
#include "EAPMessageLogWidget.h"
#include <QApplication>
#include <QMainWindow>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // 1. 创建并初始化消息日志记录器
    // Create and initialize the message logger
    // Note: In production code, consider using smart pointers or ensuring proper cleanup
    EAPMessageLogger* logger = new EAPMessageLogger(&app);  // Set app as parent for auto cleanup
    QString dbPath = "./eap_messages.db";  // 数据库存储路径
    if (!logger->initialize(dbPath)) {
        qWarning() << "Failed to initialize logger:" << logger->lastError();
        return -1;
    }

    // 2. 创建 EAPInterfaceManager 并设置日志记录器
    // Create EAPInterfaceManager and set the message logger
    EAPInterfaceManager* interfaceManager = new EAPInterfaceManager(&app);  // Set app as parent
    interfaceManager->setMessageLogger(logger);
    
    // 加载接口配置
    // Load interface configuration
    if (!interfaceManager->loadInterfaceConfig("./interfaces_config.json")) {
        qWarning() << "Failed to load interface config:" << interfaceManager->getLastError();
    }

    // 3. 创建 EAPWebService 并设置日志记录器
    // Create EAPWebService and set the message logger
    EAPWebService* webService = new EAPWebService(&app);  // Set app as parent
    webService->setMessageLogger(logger);
    
    // 加载接口配置
    // Load interface configuration
    if (!webService->loadInterfaceConfig("./interfaces_config.json")) {
        qWarning() << "Failed to load interface config:" << webService->lastError();
    }

    // 启动 Web 服务
    // Start the web service
    if (!webService->start(8026)) {
        qWarning() << "Failed to start web service:" << webService->lastError();
    }

    // 4. 创建并显示消息日志查询界面
    // Create and show the message log widget
    QMainWindow* mainWindow = new QMainWindow();
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);  // Auto delete when closed
    
    EAPMessageLogWidget* logWidget = new EAPMessageLogWidget(mainWindow);  // Set mainWindow as parent
    logWidget->setMessageLogger(logger);
    
    // 加载今天的记录
    // Load today's records
    logWidget->loadTodayRecords();
    
    mainWindow->setCentralWidget(logWidget);
    mainWindow->setWindowTitle("EAP Message Log Viewer");
    mainWindow->resize(1200, 800);
    mainWindow->show();

    // 5. 现在，所有通过 EAPInterfaceManager 发送/接收的消息
    //    以及 EAPWebService 接收/发送的消息都会自动记录到数据库
    // Now, all messages sent/received through EAPInterfaceManager
    // and received/sent by EAPWebService will be automatically logged to the database

    // 示例：发送一个消息（会自动记录）
    // Example: Send a message (will be automatically logged)
    QVariantMap params;
    params["param1"] = "value1";
    params["param2"] = 123;
    interfaceManager->post("SomeInterfaceKey", params);

    // 6. 查询历史记录示例
    // Example of querying historical records
    QDate startDate = QDate::currentDate().addDays(-7);  // 7天前
    QDate endDate = QDate::currentDate();
    QVector<EAPMessageRecord> records = logger->queryByDateRange(startDate, endDate);
    qDebug() << "Found" << records.size() << "records in the past 7 days";

    // 7. 按接口名称查询
    // Query by interface key
    QVector<EAPMessageRecord> interfaceRecords = 
        logger->queryByInterfaceKey("SomeInterfaceKey", startDate, endDate);
    qDebug() << "Found" << interfaceRecords.size() << "records for SomeInterfaceKey";

    // 8. 按消息类型查询
    // Query by message type
    QVector<EAPMessageRecord> sentRecords = 
        logger->queryByType(EAPMessageRecord::InterfaceManagerSent, startDate, endDate);
    qDebug() << "Found" << sentRecords.size() << "sent messages";

    // 9. 删除旧记录（例如：删除30天前的记录）
    // Delete old records (e.g., records older than 30 days)
    QDate cutoffDate = QDate::currentDate().addDays(-30);
    logger->deleteRecordsBefore(cutoffDate);

    return app.exec();
}

/*
注意事项 / Notes:

1. 数据库文件会自动创建在指定路径
   The database file will be created automatically at the specified path

2. 消息记录是异步的，不会影响主业务流程
   Message logging is asynchronous and won't affect the main business flow

3. 可以在多个线程中安全使用（EAPWebService 中已经做了线程安全处理）
   Thread-safe usage is supported (thread safety is implemented in EAPWebService)

4. 建议定期清理旧记录以控制数据库大小
   Regular cleanup of old records is recommended to control database size

5. UI 界面会自动更新显示当天的新记录
   The UI will automatically update to show new records for the current day

6. 可以通过界面的查询条件筛选不同类型、日期范围、接口的消息
   You can filter messages by type, date range, and interface using the UI controls
*/
