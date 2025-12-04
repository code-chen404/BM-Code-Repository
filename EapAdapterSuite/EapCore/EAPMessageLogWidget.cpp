#include "EAPMessageLogWidget.h"
#include <QHeaderView>
#include <QMessageBox>
#include <QJsonDocument>
#include <QDateTime>
#include <QGroupBox>
#include <QScrollBar>
#include <QApplication>

// 基于QWidget的用户界面，用于显示和查询EAP消息日志

EAPMessageLogWidget::EAPMessageLogWidget(QWidget* parent)
    : QWidget(parent), 
    logger_(nullptr) // 数据库管理类
{
    setupUI(); // 初始化并构建消息日志查询界面的所有控件和布局
    setupConnections(); // 查询按钮，清空显示按钮，刷新按钮，tablewidget表格变化，槽函数连接
}

EAPMessageLogWidget::~EAPMessageLogWidget()
{
}

/**
 * @brief 初始化并构建消息日志查询界面的所有控件和布局
 */
void EAPMessageLogWidget::setupUI()
{
    QVBoxLayout* mainLayout = new QVBoxLayout(this); // 垂直布局 mainLayout，整个控件的所有内容都往里塞

    // 标题标签：“EAP 查询”
    QLabel* titleLabel = new QLabel("EAP 查询", this);
    QFont titleFont = titleLabel->font();
    titleFont.setPointSize(12);
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);
    mainLayout->addWidget(titleLabel);

    // “查询条件”区域，用水平布局filterLayout把所有过滤控件一行排开
    QGroupBox* filterGroup = new QGroupBox(QString("查询条件"), this);
    QHBoxLayout* filterLayout = new QHBoxLayout(filterGroup);

    filterLayout->addWidget(new QLabel(QString("开始日期:"), this));
    startDateEdit_ = new QDateEdit(this); //开始日期下拉框
    startDateEdit_->setCalendarPopup(true);
    startDateEdit_->setDate(QDate::currentDate());
    startDateEdit_->setDisplayFormat("yyyy-MM-dd");
    filterLayout->addWidget(startDateEdit_);

    filterLayout->addWidget(new QLabel(QString("结束日期:"), this));
    endDateEdit_ = new QDateEdit(this); //结束日期下拉框
    endDateEdit_->setCalendarPopup(true);
    endDateEdit_->setDate(QDate::currentDate());
    endDateEdit_->setDisplayFormat("yyyy-MM-dd");
    filterLayout->addWidget(endDateEdit_);

    filterLayout->addWidget(new QLabel(QString("消息类型:"), this));
    typeComboBox_ = new QComboBox(this); // 消息类型下拉框
    typeComboBox_->addItem(QString("全部"), -1);
    typeComboBox_->addItem(QString("EAP send"), EAPMessageRecord::InterfaceManagerSent);
    typeComboBox_->addItem(QString("EAP receive"), EAPMessageRecord::InterfaceManagerReceived);
    typeComboBox_->addItem(QString("Web receive"), EAPMessageRecord::WebServiceReceived);
    typeComboBox_->addItem(QString("Web send"), EAPMessageRecord::WebServiceSent);
    filterLayout->addWidget(typeComboBox_);

    filterLayout->addWidget(new QLabel(QString("接口名称:"), this));
    interfaceKeyEdit_ = new QLineEdit(this); // 接口名称文本框
    interfaceKeyEdit_->setPlaceholderText(QString("checkable"));
    filterLayout->addWidget(interfaceKeyEdit_);

    queryButton_ = new QPushButton(QString("查询"), this);
    filterLayout->addWidget(queryButton_); //查询按钮

    refreshButton_ = new QPushButton(QString("刷新"), this);
    filterLayout->addWidget(refreshButton_); // 刷新按钮

    clearButton_ = new QPushButton(QString("清空显示"), this);
    filterLayout->addWidget(clearButton_); // 清空显示按钮

    mainLayout->addWidget(filterGroup);

    // 初始文字是 “就绪”
    statusLabel_ = new QLabel(QString("就绪"), this);
    mainLayout->addWidget(statusLabel_);

    // 上下分割区域（splitter）
    QSplitter* splitter = new QSplitter(Qt::Vertical, this);

    // 上半部分：表格（消息列表）
    tableWidget_ = new QTableWidget(this);
    tableWidget_->setColumnCount(8);
    QStringList headers;
    headers << QString("时间") << QString("类型") << QString("接口名称") 
            << QString("功能描述") << QString("远程地址") << QString("status") 
            << QString("错误信息") << QString("ID");
    tableWidget_->setHorizontalHeaderLabels(headers);
    tableWidget_->setSelectionBehavior(QAbstractItemView::SelectRows);
    tableWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    tableWidget_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    tableWidget_->horizontalHeader()->setStretchLastSection(false);
    tableWidget_->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    tableWidget_->setColumnWidth(0, 150);  // 时间
    tableWidget_->setColumnWidth(1, 120);  // 类型
    tableWidget_->setColumnWidth(2, 150);  // 接口名称
    tableWidget_->setColumnWidth(3, 200);  // 功能描述
    tableWidget_->setColumnWidth(4, 120);  // 远程地址
    tableWidget_->setColumnWidth(5, 80);   // 状态
    tableWidget_->setColumnWidth(6, 200);  // 错误信息
    tableWidget_->setColumnWidth(7, 60);   // ID
    
    // Enable word wrap for description column
    tableWidget_->setWordWrap(true);
    tableWidget_->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    
    splitter->addWidget(tableWidget_); // 把表格加到 splitter 的上半部分

    // 下半部分：详情 JSON 视图
    QGroupBox* detailGroup = new QGroupBox(QString("message infomation JSON"), this);
    QVBoxLayout* detailLayout = new QVBoxLayout(detailGroup);
    detailTextEdit_ = new QTextEdit(this); // JSON 视图文本框
    detailTextEdit_->setReadOnly(true);
    detailTextEdit_->setFont(QFont("Courier New", 9));
    detailLayout->addWidget(detailTextEdit_);
    splitter->addWidget(detailGroup);

    splitter->setStretchFactor(0, 2); // 调整上下比例
    splitter->setStretchFactor(1, 1);

    mainLayout->addWidget(splitter); // 最后把 splitter 扔到主布局

    setLayout(mainLayout); // 把布局设到当前 EAPMessageLogWidget 上
    resize(1000, 700); // 将窗口默认大小设为 1000×700
}

/**
 * @brief 查询按钮，清空显示按钮，刷新按钮，tablewidget表格变化，槽函数连接
 */
void EAPMessageLogWidget::setupConnections()
{
    connect(queryButton_, &QPushButton::clicked, this, &EAPMessageLogWidget::onQueryClicked); // 查询按钮
    connect(clearButton_, &QPushButton::clicked, this, &EAPMessageLogWidget::onClearClicked); // 清空显示按钮
    connect(refreshButton_, &QPushButton::clicked, this, &EAPMessageLogWidget::onRefreshClicked); // 刷新按钮
    connect(tableWidget_, &QTableWidget::itemSelectionChanged, this, &EAPMessageLogWidget::onRowSelectionChanged); // tablewidget表格
}

/**
 * @brief 设置用于查询和接收通知的消息日志对象，并建立/更新信号连接
 
 * 为当前 EAPMessageLogWidget 指定一个 EAPMessageLogger 实例，用于执行
 * 数据库查询和接收新记录插入的通知。若在调用本函数前已存在旧的 logger_，
 * 则首先断开旧 logger_ 与本窗口之间的所有信号连接，避免重复触发槽函数。

 * @param logger 要绑定到本窗口的消息日志对象指针，可以为 nullptr
 */
void EAPMessageLogWidget::setMessageLogger(EAPMessageLogger* logger)
{
    if (logger_) { // 如果之前已经有 logger_，先把旧连接断开
        disconnect(logger_, nullptr, this, nullptr);
    }

    logger_ = logger;

    if (logger_) {
        connect(logger_, &EAPMessageLogger::recordInserted, this, &EAPMessageLogWidget::onRecordInserted);
    }
}

/**
 * @brief 按当前界面筛选条件加载消息记录
 */
void EAPMessageLogWidget::loadRecords()
{
    onQueryClicked(); // 处理“查询”按钮点击事件，按当前筛选条件查询消息日志
}

/**
 * @brief 快速加载“今日日志”的便捷函数
 */
void EAPMessageLogWidget::loadTodayRecords()
{
    QDate today = QDate::currentDate();
    startDateEdit_->setDate(today);
    endDateEdit_->setDate(today);
    typeComboBox_->setCurrentIndex(0); // 把消息类型下拉框切到第 0 项
    interfaceKeyEdit_->clear(); // 清空接口名称过滤
    loadRecords(); //按当前界面筛选条件加载消息记录
}

/**
 * @brief 处理 EAPMessageLogger 新插入记录的通知，实现“今日日志”的自动刷新
 
 * 作为 EAPMessageLogger::recordInserted 信号的槽函数，当有新的日志记录
 * 写入数据库时被调用。函数仅在当前查询起始日期和结束日期均为“今天”时
 * 才会尝试自动刷新界面

 * @param record 刚刚插入到数据库中的消息记录
 */
void EAPMessageLogWidget::onRecordInserted(const EAPMessageRecord& record)
{
    // Auto-refresh if displaying today's records
    QDate today = QDate::currentDate();
    if (startDateEdit_->date() == today && endDateEdit_->date() == today) { // 用户现在是在看“今天的日志”,才尝试自动把新插入的这条记录显示出来
        // Only add if matches current filter
        int typeFilter = typeComboBox_->currentData().toInt(); // 当前选中的消息类型
        QString keyFilter = interfaceKeyEdit_->text().trimmed(); // 当前输入的接口名称过滤，去掉前后空格

        bool matches = true;
        // 如果类型过滤不是“全部”（typeFilter != -1），但这条记录的 record.type 和过滤条件不一致 → 不匹配
        if (typeFilter != -1 && record.type != typeFilter) {
            matches = false;
        }
        // 如果接口名称过滤不为空，并且 record.interfaceKey != keyFilter → 不匹配
        if (!keyFilter.isEmpty() && record.interfaceKey != keyFilter) {
            matches = false;
        }

        if (matches) {
            currentRecords_.prepend(record);  // 把这条新记录插到 currentRecords_ 的 最前面，也就是表格的第一行
            displayRecords(currentRecords_); // 在表格中显示给定的消息记录列表
        }
    }
}

/**
 * @brief 处理“查询”按钮点击事件，按当前筛选条件查询消息日志
 */
void EAPMessageLogWidget::onQueryClicked()
{
    if (!logger_ || !logger_->isInitialized()) {
        statusLabel_->setText(QString("错误: 消息日志未初始化")); // “就绪”标题处进行信息的提示
        QMessageBox::warning(this, QString("错误"), QString("消息日志未初始化"));
        return;
    }

    QDate startDate = startDateEdit_->date(); // 开始时间
    QDate endDate = endDateEdit_->date(); // 结束时间
    int typeFilter = typeComboBox_->currentData().toInt(); // 消息类型下拉框
    QString keyFilter = interfaceKeyEdit_->text().trimmed(); // 接口名称文本框

    statusLabel_->setText(QString("查询中..."));
    QApplication::processEvents(); // 强制处理一下事件队列，这样 UI 能及时刷新状态

    QVector<EAPMessageRecord> records;

    // Query based on filters
    if (typeFilter != -1) {
        records = logger_->queryByType(static_cast<EAPMessageRecord::MessageType>(typeFilter), startDate, endDate); // 按“类型 + 日期范围”查询
    } else if (!keyFilter.isEmpty()) {
        records = logger_->queryByInterfaceKey(keyFilter, startDate, endDate); // 按“接口名称 + 日期范围”查询
    } else {
        records = logger_->queryByDateRange(startDate, endDate); // 只按日期范围，不做类型/接口过滤
    }

    // 用户既选择了具体的消息类型，又填了接口名称
    if (typeFilter != -1 && !keyFilter.isEmpty()) {
        // Filter by both type and key
        QVector<EAPMessageRecord> filtered;
        for (const auto& record : records) {
            if (record.interfaceKey == keyFilter) {
                filtered.append(record);
            }
        }
        records = filtered; //覆盖原来的结果
    }

    currentRecords_ = records; // 保存当前结果
    displayRecords(records); // 把这些记录渲染到 QTableWidget 上

    statusLabel_->setText(QString("query finished: found %1 records").arg(records.size())); // “就绪”标题处进行信息的提示
}

/**
 * @brief 处理“清空显示”按钮点击事件，清除当前界面中的所有记录显示
 */
void EAPMessageLogWidget::onClearClicked()
{
    tableWidget_->setRowCount(0);
    detailTextEdit_->clear(); // 清空下面那块 JSON 详情文本框
    currentRecords_.clear(); // 把内部保存的当前查询结果 currentRecords_ 也清空
    statusLabel_->setText(QString("cleared records")); // “就绪”标题处进行信息的提示
}

/**
 * @brief 刷新按钮点击事件，重新显示“查询”的结果
 */
void EAPMessageLogWidget::onRefreshClicked()
{
    onQueryClicked();
}

/**
 * @brief 表格选中行变化时的槽函数，更新下方详情显示
 * 本函数通常连接到 tableWidget_ 的 itemSelectionChanged() 信号。
 */
void EAPMessageLogWidget::onRowSelectionChanged()
{
    QList<QTableWidgetItem*> selected = tableWidget_->selectedItems();
    if (selected.isEmpty()) { // 表示当前没有选中行,清空下方的json详情框，然后返回
        detailTextEdit_->clear();
        return;
    }

    int row = selected.first()->row();
    if (row >= 0 && row < currentRecords_.size()) {
        displayRecordDetail(currentRecords_[row]); // 显示当前行数据库查询的结果
    }
}

/**
 * @brief 在表格中显示给定的消息记录列表
 * @param records 要显示的消息记录列表
 */
void EAPMessageLogWidget::displayRecords(const QVector<EAPMessageRecord>& records)
{
    tableWidget_->setRowCount(0);
    tableWidget_->setRowCount(records.size());

    for (int i = 0; i < records.size(); ++i) {
        const EAPMessageRecord& record = records[i];

        tableWidget_->setItem(i, 0, new QTableWidgetItem(record.timestamp.toString("yyyy-MM-dd HH:mm:ss"))); // 第 0 列：时间（格式化为 "yyyy-MM-dd HH:mm:ss"）
        tableWidget_->setItem(i, 1, new QTableWidgetItem(EAPMessageRecord::typeToString(record.type))); // 类型
        tableWidget_->setItem(i, 2, new QTableWidgetItem(record.interfaceKey)); // 接口名称
        
        // Create description item with tooltip for full text
        QTableWidgetItem* descItem = new QTableWidgetItem(record.interfaceDescription);
        descItem->setToolTip(record.interfaceDescription);  // 同时给这个单元格设置 ToolTip，鼠标悬停时可以看到完整文本
        tableWidget_->setItem(i, 3, descItem); // 功能描述
        
        tableWidget_->setItem(i, 4, new QTableWidgetItem(record.remoteAddress)); // 远程地址
        tableWidget_->setItem(i, 5, new QTableWidgetItem(record.isSuccess ? QString("成功") : QString("失败"))); // status，用 "成功" 或 "失败" 显示当前记录是否成功
        tableWidget_->setItem(i, 6, new QTableWidgetItem(record.errorMessage)); // 错误信息（失败时显示错误原因）
        tableWidget_->setItem(i, 7, new QTableWidgetItem(QString::number(record.id))); // ID（数据库自增主键）

        // Color code by type
        QColor rowColor;
        switch (record.type) {
        case EAPMessageRecord::InterfaceManagerSent:
            rowColor = QColor(230, 240, 255);  // Light blue
            break;
        case EAPMessageRecord::InterfaceManagerReceived:
            rowColor = QColor(230, 255, 230);  // Light green
            break;
        case EAPMessageRecord::WebServiceReceived:
            rowColor = QColor(255, 245, 230);  // Light orange
            break;
        case EAPMessageRecord::WebServiceSent:
            rowColor = QColor(255, 230, 255);  // Light pink
            break;
        }

        for (int col = 0; col < 8; ++col) {
            if (tableWidget_->item(i, col)) {
                tableWidget_->item(i, col)->setBackground(rowColor); // 给每个非空单元格设置背景色
                
                // Mark failed operations in red
                if (!record.isSuccess) {
                    tableWidget_->item(i, col)->setForeground(Qt::red); // 把这整行的文字颜色设为红色
                }
            }
        }
    }
}

/**
 * @brief 在详情区域显示指定消息记录的详细信息
 * @param record 要显示详细信息的消息记录
 */
void EAPMessageLogWidget::displayRecordDetail(const EAPMessageRecord& record)
{
    QString detail;
    detail += QString("=== 消息详情 ===\n");
    detail += QString("ID: %1\n").arg(record.id);
    detail += QString("时间: %1\n").arg(record.timestamp.toString("yyyy-MM-dd HH:mm:ss.zzz"));
    detail += QString("类型: %1\n").arg(EAPMessageRecord::typeToString(record.type));
    detail += QString("接口名称: %1\n").arg(record.interfaceKey);
    detail += QString("远程地址: %1\n").arg(record.remoteAddress);
    detail += QString("状态: %1\n").arg(record.isSuccess ? QString("成功") : QString("失败"));
    if (!record.errorMessage.isEmpty()) {
        detail += QString("错误信息: %1\n").arg(record.errorMessage);
    }
    detail += QString("\n=== JSON Payload ===\n");
    detail += formatPayloadForDisplay(record.payload);

    detailTextEdit_->setPlainText(detail); // 将拼好的整段文本显示到详情框，原来的内容会被替换掉
}

/**
 * @brief 将 JSON 对象格式化为可读性较好的字符串
 * @param payload 要格式化显示的 JSON 对象
 * @return QString 已格式化的 JSON 字符串（带缩进和换行）
 */
QString EAPMessageLogWidget::formatPayloadForDisplay(const QJsonObject& payload)
{
    QJsonDocument doc(payload);
    return QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
}
