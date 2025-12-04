#include "EapAlarmDialog.h"
#include <QFont>
#include <QScreen>
#include <QApplication>

EapAlarmDialog::EapAlarmDialog(const QString& message, int autoCloseSeconds, QWidget* parent)
    : QDialog(parent)
    , autoCloseSeconds_(autoCloseSeconds)
    , remainingSeconds_(autoCloseSeconds)
{
    setupUI(message);
    
    // 设置为模态对话框
    setModal(true);
    
    // 设置窗口标志：保持在最前面，无系统菜单
    setWindowFlags(Qt::Dialog | Qt::WindowStaysOnTopHint | Qt::CustomizeWindowHint | Qt::WindowTitleHint);
    
    // 如果设置了自动关闭，启动定时器
    if (autoCloseSeconds_ > 0) {
        autoCloseTimer_ = new QTimer(this);
        connect(autoCloseTimer_, &QTimer::timeout, this, &EapAlarmDialog::onTimerTimeout);
        autoCloseTimer_->start(1000); // 每秒触发一次
        updateTimerLabel();
    }
    
    // 居中显示
    if (parent) {
        move(parent->geometry().center() - rect().center());
    } else { 
        // 在屏幕中央显示
        QScreen* screen = QApplication::primaryScreen();
        if (screen) {
            QRect screenGeometry = screen->geometry();
            move(screenGeometry.center() - rect().center());
        }
    }
}

EapAlarmDialog::~EapAlarmDialog()
{
}

/**
 * @brief 构建告警对话框的界面布局
 * @param message 要在对话框中显示的提示/告警文本
 */
void EapAlarmDialog::setupUI(const QString& message)
{
    setWindowTitle(QStringLiteral("CIM 消息提醒"));
    
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(20);
    mainLayout->setContentsMargins(30, 30, 30, 30);
    
    // 消息标签
    messageLabel_ = new QLabel(message, this);
    messageLabel_->setWordWrap(true);
    messageLabel_->setAlignment(Qt::AlignCenter);
    QFont messageFont = messageLabel_->font();
    messageFont.setPointSize(12);
    messageFont.setBold(true); 
    messageLabel_->setFont(messageFont);
    messageLabel_->setStyleSheet("QLabel { color: #D32F2F; padding: 20px; }");
    mainLayout->addWidget(messageLabel_);
    
    // 倒计时标签
    timerLabel_ = new QLabel(this);
    timerLabel_->setAlignment(Qt::AlignCenter);
    QFont timerFont = timerLabel_->font();
    timerFont.setPointSize(10);
    timerLabel_->setFont(timerFont);
    timerLabel_->setStyleSheet("QLabel { color: #757575; }");
    mainLayout->addWidget(timerLabel_);
    
    // 按钮布局
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    
    // 确认按钮
    confirmButton_ = new QPushButton(QStringLiteral("确认"), this);
    confirmButton_->setMinimumSize(100, 40);
    confirmButton_->setStyleSheet(
        "QPushButton {"
        "    background-color: #1976D2;"
        "    color: white;"
        "    border: none;"
        "    border-radius: 4px;"
        "    font-size: 11pt;"
        "    font-weight: bold;"
        "    padding: 8px 16px;"
        "}"
        "QPushButton:hover {"
        "    background-color: #1565C0;"
        "}"
        "QPushButton:pressed {"
        "    background-color: #0D47A1;"
        "}"
    );
    connect(confirmButton_, &QPushButton::clicked, this, &EapAlarmDialog::onConfirmClicked);
    buttonLayout->addWidget(confirmButton_);
    
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);
    
    setLayout(mainLayout);
    setMinimumSize(400, 200);
}

/**
 * @brief 设置对话框的自动关闭倒计时
 * @param seconds 倒计时秒数；>0 表示启用自动关闭，<=0 表示关闭自动倒计时功能
 * 典型用法：
 *  setAutoCloseTime(10);  // 对话框将从 10 秒开始倒计时，并在 10 秒后自动关闭
 */
void EapAlarmDialog::setAutoCloseTime(int seconds)
{
    autoCloseSeconds_ = seconds;
    remainingSeconds_ = seconds;
    
    if (autoCloseTimer_) {
        autoCloseTimer_->stop();
        delete autoCloseTimer_;
        autoCloseTimer_ = nullptr;
    }
    
    if (seconds > 0) {
        autoCloseTimer_ = new QTimer(this);
        connect(autoCloseTimer_, &QTimer::timeout, this, &EapAlarmDialog::onTimerTimeout);
        autoCloseTimer_->start(1000);
        updateTimerLabel();
    } else {
        timerLabel_->clear();
    }
}

/**
 * @brief 获取当前对话框剩余的自动关闭时间
 * @return 剩余秒数；若未启用自动关闭或已倒数完毕，则可能为 0 或负数
 */
int EapAlarmDialog::getRemainingTime() const
{
    return remainingSeconds_;
}

/**
 * @brief 自动关闭定时器的超时槽函数（每秒触发一次）
 */
void EapAlarmDialog::onTimerTimeout()
{
    remainingSeconds_--;
    emit remainingTimeChanged(remainingSeconds_);
    
    if (remainingSeconds_ <= 0) {
        autoCloseTimer_->stop();
        emit confirmed();
        accept(); // 自动关闭，返回 Accepted
    } else {
        updateTimerLabel();
    }
}

/**
 * @brief “确认”按钮点击槽函数
 *
 * 当用户点击对话框中的“确认”按钮时：
 *  - 若自动关闭定时器存在，则先停止定时器，防止后续继续触发；
 *  - 发出 confirmed() 信号，通知外部逻辑该对话框已被确认；
 *  - 调用 accept() 关闭对话框，使 exec() 返回 QDialog::Accepted。
 *
 * 与 onTimerTimeout() 中倒计时结束的处理保持一致，方便外部统一处理“确认完成”的场景。
 */
void EapAlarmDialog::onConfirmClicked()
{
    if (autoCloseTimer_) {
        autoCloseTimer_->stop();
    }
    emit confirmed();
    accept(); // 手动确认，返回 Accepted
}

/**
 * @brief 根据当前自动关闭配置与剩余时间刷新倒计时标签
 */
void EapAlarmDialog::updateTimerLabel()
{
    if (autoCloseSeconds_ > 0) {
        timerLabel_->setText(QString(QStringLiteral("窗口将在 %1 秒后自动关闭")).arg(remainingSeconds_));
    } else {
        timerLabel_->clear();
    }
}
