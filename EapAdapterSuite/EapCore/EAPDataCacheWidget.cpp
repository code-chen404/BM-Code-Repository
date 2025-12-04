#include "EAPDataCacheWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMouseEvent>
#include <QMessageBox>
#include <QDir>

EAPDataCacheWidget::EAPDataCacheWidget(QWidget* parent)
    : QWidget(parent)
    , dataCache_(nullptr) // 缓存类
    , floating_(false) // 浮动模式标志位
    , dragging_(false) // 标记现在正在拖动
    , minimized_(false) // 最小化状态
{
    setupUI(); // 初始化数据缓存管理窗口的界面布局和控件
    setFloating(true); // 默认浮动模式
}

EAPDataCacheWidget::~EAPDataCacheWidget()
{
}

/**
 * @brief 设置数据缓存实例 (此函数未加载)
 * @param cache 缓存实例指针
 */
void EAPDataCacheWidget::setDataCache(EAPDataCache* cache)
{
    dataCache_ = cache;
    if (dataCache_) {
        connect(dataCache_, &EAPDataCache::dataSaved, this, [this]() { // 数据缓存成功
            if (functionComboBox_->currentIndex() >= 0) {
                loadFunctionData(functionComboBox_->currentText()); // 根据指定的接口名称加载缓存记录并更新界面显示
            }
        });
        connect(dataCache_, &EAPDataCache::dataDeleted, this, [this]() { // 删除指定记录
            if (functionComboBox_->currentIndex() >= 0) {
                loadFunctionData(functionComboBox_->currentText());
            }
        });
    }
    loadFunctionList(); // 重新加载接口名称到下拉框中
}

/**
 * @brief 切换浮动模式
 * @param floating true 表示浮动窗口模式
 */
void EAPDataCacheWidget::setFloating(bool floating)
{
    floating_ = floating;
    
    if (floating_) {
        setWindowFlags(Qt::Tool | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet(
            "QWidget#mainWidget {"
            "    background-color: #FFFFFF;"
            "    border: 2px solid #1976D2;"
            "    border-radius: 8px;"
            "}"
        );
        floatingButton_->setText(QStringLiteral("取消浮动"));
    } else {
        setWindowFlags(Qt::Window);
        setAttribute(Qt::WA_TranslucentBackground, false);
        setStyleSheet("");
        floatingButton_->setText(QStringLiteral("浮动模式"));
    }
    
    show();
}

/**
 * @brief 获取浮动窗口的浮动模式
 */
bool EAPDataCacheWidget::isFloating() const
{
    return floating_;
}

/**
 * @brief 根据 当前下拉框选中的接口名，重新加载表格数据
 */
void EAPDataCacheWidget::refreshData()
{
    if (functionComboBox_->currentIndex() >= 0) {
        loadFunctionData(functionComboBox_->currentText()); // 根据指定的接口名称加载缓存记录并更新界面显示
    }
}

/**
 * @brief 根据指定的接口名称加载缓存记录并更新界面显示
 * @param functionName 接口名称（用于查询对应的缓存记录）
 */
void EAPDataCacheWidget::loadFunctionData(const QString& functionName)
{
    if (!dataCache_ || functionName.isEmpty()) {
        return;
    }
    
    QList<QVariantMap> records = dataCache_->queryRecordsByFunction(functionName); // 查询指定 function_name 的所有记录
    updateRecordTable(records); // 使用给定的记录列表刷新表格显示
    
    statusLabel_->setText(QString(QStringLiteral("共 %1 条记录")).arg(records.size())); // 更新状态栏提示
}

/**
 * @brief 初始化数据缓存管理窗口的界面布局和控件
 */
void EAPDataCacheWidget::setupUI()
{
    setObjectName("mainWidget"); //此界面名称
    setWindowTitle(QStringLiteral("EAP 数据缓存管理")); // 窗口标题
    
    // 创建一个垂直方向的主布局 QVBoxLayout，四周留 8 像素边距，控件之间间隔 8 像素
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(8);
    
    // 标题栏
    QHBoxLayout* titleLayout = new QHBoxLayout();
    QLabel* titleLabel = new QLabel(QStringLiteral("📊 数据缓存管理"), this);
    titleLabel->setStyleSheet("font-size: 14pt; font-weight: bold; color: #1976D2;");
    titleLayout->addWidget(titleLabel);
    titleLayout->addStretch();
    
    minimizeButton_ = new QPushButton(QStringLiteral("−"), this); // 最小化按钮
    minimizeButton_->setFixedSize(30, 30);
    minimizeButton_->setStyleSheet("QPushButton { background: #FFC107; border: none; border-radius: 15px; color: white; font-weight: bold; }");
    connect(minimizeButton_, &QPushButton::clicked, this, &EAPDataCacheWidget::onMinimizeClicked); // 最小化按钮的点击实现
    titleLayout->addWidget(minimizeButton_);
    
    floatingButton_ = new QPushButton(QStringLiteral("浮动"), this); // 浮动按钮
    floatingButton_->setFixedSize(30, 30);
    floatingButton_->setStyleSheet("QPushButton { background: #4CAF50; border: none; border-radius: 15px; color: white; font-weight: bold; }");
    connect(floatingButton_, &QPushButton::clicked, this, &EAPDataCacheWidget::onToggleFloatingClicked); // 浮动按钮的点击实现
    titleLayout->addWidget(floatingButton_);
    
    QPushButton* closeButton = new QPushButton(QStringLiteral("×"), this); // 关闭按钮
    closeButton->setFixedSize(30, 30);
    closeButton->setStyleSheet("QPushButton { background: #F44336; border: none; border-radius: 15px; color: white; font-weight: bold; }");
    connect(closeButton, &QPushButton::clicked, this, &QWidget::close); // 关闭按钮的点击实现
    titleLayout->addWidget(closeButton);
    
    mainLayout->addLayout(titleLayout);
    
    // 控制栏
    QHBoxLayout* controlLayout = new QHBoxLayout();
    
    QLabel* funcLabel = new QLabel(QStringLiteral("接口:"), this);
    controlLayout->addWidget(funcLabel);
    
    functionComboBox_ = new QComboBox(this); // 下拉框
    functionComboBox_->setMinimumWidth(150);
    connect(functionComboBox_, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EAPDataCacheWidget::onFunctionSelected);  // 下拉框的点击实现
    controlLayout->addWidget(functionComboBox_, 1); // 给了拉伸因子，使它在这一行尽量占空间
    
    refreshButton_ = new QPushButton(QStringLiteral("🔄 刷新"), this); // 刷新按钮
    connect(refreshButton_, &QPushButton::clicked, this, &EAPDataCacheWidget::onRefreshClicked); // 点击实现
    controlLayout->addWidget(refreshButton_);
    
    deleteButton_ = new QPushButton(QStringLiteral("🗑 删除"), this); // 删除按钮
    connect(deleteButton_, &QPushButton::clicked, this, &EAPDataCacheWidget::onDeleteClicked); // 点击实现
    controlLayout->addWidget(deleteButton_);
    
    clearAllButton_ = new QPushButton(QStringLiteral("🗑 清空全部"), this); // 清空全部按钮
    clearAllButton_->setStyleSheet("QPushButton { color: #F44336; }");
    connect(clearAllButton_, &QPushButton::clicked, this, &EAPDataCacheWidget::onClearAllClicked); // 点击实现
    controlLayout->addWidget(clearAllButton_);
    
    mainLayout->addLayout(controlLayout);
    
    // 分割器
    splitter_ = new QSplitter(Qt::Vertical, this);
    
    // 记录表格
    recordTable_ = new QTableWidget(this);
    recordTable_->setColumnCount(3);
    recordTable_->setHorizontalHeaderLabels({QStringLiteral("唯一键"), QStringLiteral("时间戳"), QStringLiteral("数据预览")});
    recordTable_->horizontalHeader()->setStretchLastSection(true);
    recordTable_->setSelectionBehavior(QAbstractItemView::SelectRows);
    recordTable_->setSelectionMode(QAbstractItemView::SingleSelection);
    recordTable_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    recordTable_->setAlternatingRowColors(true);
    connect(recordTable_, &QTableWidget::itemSelectionChanged, this, &EAPDataCacheWidget::onRecordSelected); // 行选中变化时触发
    splitter_->addWidget(recordTable_);
    
    // 详情显示
    QWidget* detailWidget = new QWidget(this);
    QVBoxLayout* detailLayout = new QVBoxLayout(detailWidget);
    detailLayout->setContentsMargins(0, 0, 0, 0);
    
    QLabel* detailLabel = new QLabel(QStringLiteral("📄 记录详情"), this);
    detailLabel->setStyleSheet("font-weight: bold;");
    detailLayout->addWidget(detailLabel);
    
    detailTextEdit_ = new QTextEdit(this);
    detailTextEdit_->setReadOnly(true);
    detailTextEdit_->setStyleSheet("QTextEdit { border: 1px solid #E0E0E0; border-radius: 4px; }");
    detailLayout->addWidget(detailTextEdit_);
    
    splitter_->addWidget(detailWidget);
    splitter_->setStretchFactor(0, 2);
    splitter_->setStretchFactor(1, 1);
    
    mainLayout->addWidget(splitter_);
    
    // 状态栏
    statusLabel_ = new QLabel(QStringLiteral("就绪"), this);
    statusLabel_->setStyleSheet("color: #757575; font-size: 9pt;");
    mainLayout->addWidget(statusLabel_);
    
    setLayout(mainLayout); // 把 mainLayout 设置为整个窗口的布局
    normalSize_ = QSize(800, 600);
    resize(normalSize_);
}

/**
 * @brief 重新加载接口名称到下拉框中
 */
void EAPDataCacheWidget::loadFunctionList()
{
    functionComboBox_->clear(); // 清除接口下拉框
    
    if (!dataCache_ || !dataCache_->isInitialized()) { // 没有数据缓存对象，或缓存系统没初始化好，就不再往下做
        return;
    }
    
    // 扫描数据库文件
    QString basePath = QDir::currentPath() + "/dataCache";  // 可配置
    QDir dir(basePath);
    if (!dir.exists()) {
        return;
    }
    
    QStringList filters;
    filters << "*.db";
    QFileInfoList fileList = dir.entryInfoList(filters, QDir::Files); // 列出所有 .db 文件
    
    for (const QFileInfo& fileInfo : fileList) {
        QString functionName = fileInfo.baseName(); // 去掉路径和 .db 后的部分
        functionComboBox_->addItem(functionName); // 接口名称添加到下拉框中
    }
}

/**
 * @brief 使用给定的记录列表刷新表格显示
 * @param records 记录列表，每条记录包含 db_key、data、timestamp
 */
void EAPDataCacheWidget::updateRecordTable(const QList<QVariantMap>& records)
{
    recordTable_->setRowCount(0);
    
    for (const QVariantMap& record : records) {
        int row = recordTable_->rowCount();
        recordTable_->insertRow(row);
        
        // 唯一键
        QTableWidgetItem* keyItem = new QTableWidgetItem(record["db_key"].toString());
        recordTable_->setItem(row, 0, keyItem);
        
        // 时间戳
        QTableWidgetItem* timeItem = new QTableWidgetItem(record["timestamp"].toString());
        recordTable_->setItem(row, 1, timeItem);
        
        // 数据预览
        QVariantMap data = record["data"].toMap();
        QString preview;
        int count = 0;
        for (auto it = data.begin(); it != data.end() && count < 3; ++it, ++count) {
            if (count > 0) preview += ", ";
            preview += QString("%1: %2").arg(it.key(), it.value().toString());
        }
        if (data.size() > 3) {
            preview += "...";
        }
        QTableWidgetItem* previewItem = new QTableWidgetItem(preview);
        recordTable_->setItem(row, 2, previewItem);
    }
    
    recordTable_->resizeColumnsToContents(); // 自动调整列宽
}

/**
 * @brief 以格式化 JSON 形式显示记录详情
 * @param data 记录数据（键值对），将被转换为 JSON 并展示在详情文本框中
 */
void EAPDataCacheWidget::displayRecordDetail(const QVariantMap& data)
{
    QJsonDocument doc = QJsonDocument::fromVariant(data);
    QString jsonStr = QString::fromUtf8(doc.toJson(QJsonDocument::Indented));
    detailTextEdit_->setPlainText(jsonStr);
}

/**
 * @brief 当接口下拉框选中项变化时触发，加载对应接口的缓存数据
 * @param index 当前选中的索引（小于 0 表示无有效选中项）
 */
void EAPDataCacheWidget::onFunctionSelected(int index)
{
    if (index >= 0) {
        QString functionName = functionComboBox_->currentText();
        loadFunctionData(functionName); // 根据指定的接口名称加载缓存记录并更新界面显示
    }
}

/**
 * @brief 当表格中选中记录变化时触发，显示选中记录的详细内容
 */
void EAPDataCacheWidget::onRecordSelected()
{
    int row = recordTable_->currentRow();
    if (row < 0 || !dataCache_) {
        return;
    }
    
    QString dbKey = recordTable_->item(row, 0)->text(); // 从当前选中行的 第 0 列 取出文本
    QString functionName = functionComboBox_->currentText();
    QString saveKey = QString("%1.%2").arg(functionName, dbKey);
    
    QVariantMap data = dataCache_->readRecord(saveKey); // 把这一条记录完整读出来
    displayRecordDetail(data); // 以格式化 JSON 形式显示记录详情
}

/**
 * @brief 刷新接口列表并重新加载当前选中接口的缓存数据
 *
 * 点击“刷新”按钮时调用：先重新扫描数据缓存数据库文件，更新接口下拉框，
 * 然后根据当前选中的接口名称刷新表格中的记录显示。
 */
void EAPDataCacheWidget::onRefreshClicked()
{
    loadFunctionList();
    refreshData();
}

/**
 * @brief 删除当前选中的缓存记录
 */
void EAPDataCacheWidget::onDeleteClicked()
{
    int row = recordTable_->currentRow();
    if (row < 0 || !dataCache_) {
        QMessageBox::warning(this, QStringLiteral("警告"), QStringLiteral("请先选择要删除的记录"));
        return;
    }
    
    QString dbKey = recordTable_->item(row, 0)->text(); // 从当前行第 0 列拿到“唯一键”（db_key）
    QString functionName = functionComboBox_->currentText();
    QString saveKey = QString("%1.%2").arg(functionName, dbKey);
    
    auto reply = QMessageBox::question(this, QStringLiteral("确认删除"),
                                       QString(QStringLiteral("确定要删除记录 %1 吗？")).arg(dbKey),
                                       QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (dataCache_->deleteRecord(saveKey)) { // 删除指定记录
            statusLabel_->setText(QStringLiteral("记录已删除"));
            refreshData(); // 根据 当前下拉框选中的接口名，重新加载表格数据
        } else {
            QMessageBox::critical(this, QStringLiteral("错误"), dataCache_->lastError());
        }
    }
}

/**
 * @brief 清空指定接口的存储记录
 */
void EAPDataCacheWidget::onClearAllClicked()
{
    if (!dataCache_ || functionComboBox_->currentIndex() < 0) {
        return;
    }
    
    QString functionName = functionComboBox_->currentText();
    auto reply = QMessageBox::question(this, QStringLiteral("确认清空"),
                                       QString(QStringLiteral("确定要清空 %1 的所有缓存数据吗？")).arg(functionName),
                                       QMessageBox::Yes | QMessageBox::No);
    
    if (reply == QMessageBox::Yes) {
        if (dataCache_->clearFunctionRecords(functionName)) {
            statusLabel_->setText(QStringLiteral("已清空所有记录"));
            refreshData(); // 根据 当前下拉框选中的接口名，重新加载表格数据
        } else {
            QMessageBox::critical(this, QStringLiteral("错误"), dataCache_->lastError());
        }
    }
}

/**
 * @brief 浮动按钮的点击实现
 */
void EAPDataCacheWidget::onToggleFloatingClicked()
{
    setFloating(!floating_);
}

/**
 * @brief 最小化按钮的点击实现
 */
void EAPDataCacheWidget::onMinimizeClicked()
{
    if (minimized_) {
        // 恢复
        resize(normalSize_);
        splitter_->show();
        minimized_ = false;
        minimizeButton_->setText(QStringLiteral("−"));
    } else {
        // 最小化
        normalSize_ = size();
        resize(width(), 60);
        splitter_->hide();
        minimized_ = true;
        minimizeButton_->setText(QStringLiteral("+"));
    }
}

/**
 * @brief 按下左键 → 记录“我开始拖动窗口了”和鼠标/窗口偏移
 */
void EAPDataCacheWidget::mousePressEvent(QMouseEvent* event)
{
    if (floating_ && event->button() == Qt::LeftButton) { // 只有在 浮动模式 下才响应拖动
        // 只有点击标题区域才允许拖动
        if (event->pos().y() < 40) {
            dragging_ = true; // 标记现在正在拖动
            dragPosition_ = event->globalPos() - frameGeometry().topLeft();
            event->accept();
        }
    }
}

/**
 * @brief 按住左键移动 → 窗口跟着鼠标平滑移动
 */
void EAPDataCacheWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (dragging_ && floating_ && (event->buttons() & Qt::LeftButton)) { // 只有在“正在拖动 + 浮动模式 + 鼠标左键还按着”的情况下才执行
        move(event->globalPos() - dragPosition_);
        event->accept();
    }
}

/**
 * @brief 松开左键 → 停止拖动
 */
void EAPDataCacheWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        dragging_ = false; // 鼠标松开
    }
}

/**
 * @brief 双击 → 触发 onMinimizeClicked()，最小化窗口
 */
void EAPDataCacheWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (floating_ && event->pos().y() < 40) { // 浮动模式下双击标题栏，最小化窗口
        onMinimizeClicked(); // 最小化按钮的点击实现
    }
}
