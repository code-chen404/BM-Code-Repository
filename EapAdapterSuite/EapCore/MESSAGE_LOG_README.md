# EAP 消息日志功能说明

## 功能概述

EAP 消息日志功能为 EapCore 模块提供了完整的消息记录、持久化存储和历史查询能力。该功能支持：

- ✅ 自动记录 `EAPInterfaceManager` 发送和接收的消息
- ✅ 自动记录 `EAPWebService` 接收和发送的消息
- ✅ 基于 SQLite 的持久化存储，按日期索引
- ✅ QWidget 可视化查询界面，支持多种筛选条件
- ✅ 线程安全的实现
- ✅ 历史记录查询和管理

## 架构设计

### 核心组件

1. **EAPMessageRecord** - 消息记录数据结构
2. **EAPMessageLogger** - 数据库管理和查询接口
3. **EAPMessageLogWidget** - 可视化查询界面
4. **集成到 EAPInterfaceManager** - 自动记录客户端消息
5. **集成到 EAPWebService** - 自动记录服务端消息

### 数据库结构

```sql
CREATE TABLE eap_messages (
    id INTEGER PRIMARY KEY AUTOINCREMENT,
    timestamp TEXT NOT NULL,           -- ISO格式时间戳
    date TEXT NOT NULL,                -- ISO格式日期（用于快速查询）
    type INTEGER NOT NULL,             -- 消息类型（0-3）
    interface_key TEXT NOT NULL,       -- 接口名称或功能名称
    remote_address TEXT,               -- 远程地址（WebService使用）
    payload TEXT NOT NULL,             -- JSON格式的消息内容
    is_success INTEGER NOT NULL,       -- 成功标志（0/1）
    error_message TEXT                 -- 错误信息（如果有）
);

-- 索引用于加速查询
CREATE INDEX idx_date ON eap_messages(date);
CREATE INDEX idx_type ON eap_messages(type);
CREATE INDEX idx_interface_key ON eap_messages(interface_key);
CREATE INDEX idx_timestamp ON eap_messages(timestamp);
```

## 使用方法

### 1. 基本初始化

```cpp
#include "EAPMessageLogger.h"
#include "EAPMessageLogWidget.h"

// 创建并初始化日志记录器
EAPMessageLogger* logger = new EAPMessageLogger();
if (!logger->initialize("./eap_messages.db")) {
    qWarning() << "初始化失败:" << logger->lastError();
}
```

### 2. 集成到 EAPInterfaceManager

```cpp
#include "EAPInterfaceManager.h"

EAPInterfaceManager* manager = new EAPInterfaceManager();
manager->setMessageLogger(logger);

// 现在所有通过 manager->post() 发送的消息都会自动记录
// 所有接收到的响应也会自动记录
manager->post("InterfaceKey", params);
```

### 3. 集成到 EAPWebService

```cpp
#include "EAPWebService.h"

EAPWebService* service = new EAPWebService();
service->setMessageLogger(logger);
service->start(8026);

// 现在所有接收的请求和发送的响应都会自动记录
```

### 4. 使用可视化查询界面

```cpp
EAPMessageLogWidget* widget = new EAPMessageLogWidget();
widget->setMessageLogger(logger);
widget->loadTodayRecords();  // 加载今天的记录
widget->show();
```

### 5. 编程方式查询记录

```cpp
// 按日期范围查询
QDate startDate = QDate::currentDate().addDays(-7);
QDate endDate = QDate::currentDate();
QVector<EAPMessageRecord> records = logger->queryByDateRange(startDate, endDate);

// 按消息类型查询
QVector<EAPMessageRecord> sentRecords = logger->queryByType(
    EAPMessageRecord::InterfaceManagerSent, startDate, endDate
);

// 按接口名称查询
QVector<EAPMessageRecord> interfaceRecords = logger->queryByInterfaceKey(
    "SomeInterfaceKey", startDate, endDate
);

// 按单日查询
QVector<EAPMessageRecord> todayRecords = logger->queryByDate(QDate::currentDate());
```

### 6. 清理旧记录

```cpp
// 删除30天前的记录
QDate cutoffDate = QDate::currentDate().addDays(-30);
logger->deleteRecordsBefore(cutoffDate);
```

## 消息类型

```cpp
enum MessageType {
    InterfaceManagerSent,      // EAPInterfaceManager 发送消息
    InterfaceManagerReceived,  // EAPInterfaceManager 接收响应
    WebServiceReceived,        // EAPWebService 接收请求
    WebServiceSent            // EAPWebService 发送响应
};
```

## UI 界面功能

### 查询条件
- **日期范围**：指定开始和结束日期
- **消息类型**：筛选特定类型的消息
- **接口名称**：按接口名称精确查询

### 显示功能
- 表格显示消息列表（时间、类型、接口、状态等）
- 按类型颜色编码（不同消息类型显示不同颜色）
- 失败消息红色高亮显示
- 点击记录查看详细 JSON 内容

### 操作按钮
- **查询**：根据筛选条件查询记录
- **刷新**：重新加载当前条件的记录
- **清空显示**：清空表格显示

## 性能考虑

1. **异步记录**：消息记录不会阻塞主业务流程
2. **索引优化**：数据库使用多个索引加速查询
3. **线程安全**：在多线程环境下安全使用
4. **内存管理**：UI 只加载查询结果，不会一次性加载所有记录

## 最佳实践

### 1. 数据库位置
建议将数据库文件放在应用程序数据目录：
```cpp
QString dbPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) 
                 + "/eap_messages.db";
```

### 2. 定期清理
建议在应用启动时或定期任务中清理旧记录：
```cpp
// 启动时清理90天前的记录
QDate cutoffDate = QDate::currentDate().addDays(-90);
logger->deleteRecordsBefore(cutoffDate);
```

### 3. 错误处理
始终检查初始化结果：
```cpp
if (!logger->initialize(dbPath)) {
    qCritical() << "无法初始化消息日志:" << logger->lastError();
    // 采取适当的错误处理措施
}
```

### 4. 生产环境考虑
在生产环境中，考虑：
- 限制日志保留时间（如30-90天）
- 监控数据库文件大小
- 考虑使用日志轮转策略
- 敏感信息脱敏处理

## 故障排查

### 问题：数据库无法创建
**可能原因**：
- 路径不存在或无写权限
- 磁盘空间不足

**解决方法**：
```cpp
QFileInfo fileInfo(dbPath);
QDir dir = fileInfo.absoluteDir();
if (!dir.exists()) {
    dir.mkpath(dir.absolutePath());
}
```

### 问题：记录没有显示
**可能原因**：
- 日志记录器未正确初始化
- 日志记录器未设置到 Manager/Service

**解决方法**：
```cpp
// 检查初始化状态
if (!logger->isInitialized()) {
    qWarning() << "Logger not initialized";
}

// 确保设置了日志记录器
manager->setMessageLogger(logger);
service->setMessageLogger(logger);
```

### 问题：UI 不更新
**可能原因**：
- 查询的日期范围不包含新消息
- 类型或接口筛选条件不匹配

**解决方法**：
- 使用 `loadTodayRecords()` 加载当天记录
- 检查筛选条件是否正确

## 技术细节

### 线程安全
- EAPWebService 中的日志记录使用 `std::mutex` 保护
- 数据库操作使用唯一的连接名称避免冲突
- 信号/槽机制确保 UI 更新在主线程执行

### 存储格式
- 时间戳使用 ISO 8601 格式
- JSON 数据以紧凑格式存储
- 日期单独存储以优化按日期查询

## 示例代码

完整的使用示例请参考 `MessageLogUsageExample.cpp` 文件。

## 国际化 (Internationalization)

当前实现的 UI 文本使用中文。如需支持多语言，可以：

1. 使用 `QObject::tr()` 包装所有显示文本
2. 创建 `.ts` 翻译文件
3. 使用 Qt Linguist 进行翻译

## 许可证

与 EapCore 模块相同的许可证。
