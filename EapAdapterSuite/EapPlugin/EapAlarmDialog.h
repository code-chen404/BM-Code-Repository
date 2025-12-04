#pragma once

#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QHBoxLayout>

/**
 * @brief EAP 报警对话框
 * 
 * 用于显示 CIM 消息的模态对话框。
 * 支持自动关闭和手动确认两种模式。
 */
class EapAlarmDialog : public QDialog
{
    Q_OBJECT
public:
    /**
     * @brief 构造函数
     * @param message 要显示的消息内容
     * @param autoCloseSeconds 自动关闭的秒数（0 表示不自动关闭，需要手动确认）
     * @param parent 父窗口
     */
    explicit EapAlarmDialog(const QString& message, int autoCloseSeconds = 0, QWidget* parent = nullptr);
    ~EapAlarmDialog() override;

    /**
     * @brief 设置自动关闭时间
     * @param seconds 自动关闭的秒数（0 表示不自动关闭）
     */
    void setAutoCloseTime(int seconds);

    /**
     * @brief 获取剩余时间
     * @return 剩余秒数
     */
    int getRemainingTime() const;

signals:
    /**
     * @brief 对话框已确认（手动或自动）
     */
    void confirmed();

    /**
     * @brief 剩余时间更新
     * @param seconds 剩余秒数
     */
    void remainingTimeChanged(int seconds);

private slots:
    void onTimerTimeout();
    void onConfirmClicked();

private:
    void setupUI(const QString& message);
    void updateTimerLabel();

private:
    QLabel* messageLabel_;           ///< 消息显示标签 
    QLabel* timerLabel_;             ///< 倒计时显示标签
    QPushButton* confirmButton_;     ///< 确认按钮
    QTimer* autoCloseTimer_;         ///< 自动关闭定时器

    int autoCloseSeconds_;           ///< 自动关闭总秒数（0 表示不自动关闭）
    int remainingSeconds_;           ///< 剩余秒数
};
