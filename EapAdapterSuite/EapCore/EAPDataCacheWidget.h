#pragma once

#include "eapcore_global.h"
#include "EAPDataCache.h"
#include <QWidget>
#include <QTableWidget>
#include <QComboBox>
#include <QPushButton>
#include <QLabel>
#include <QTextEdit>
#include <QSplitter>

/**
 * @brief EAP 数据缓存管理浮动窗口
 * 
 * 提供可视化的缓存数据查看和管理功能。
 * 支持浮动窗口模式，可拖动、最小化。
 */
class EAPCORE_EXPORT EAPDataCacheWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EAPDataCacheWidget(QWidget* parent = nullptr);
    ~EAPDataCacheWidget() override;

    /**
     * @brief 设置数据缓存实例
     * @param cache 缓存实例指针
     */
    void setDataCache(EAPDataCache* cache);

    /**
     * @brief 切换浮动模式
     * @param floating true 表示浮动窗口模式
     */
    void setFloating(bool floating);

    /**
     * @brief 是否为浮动模式
     */
    bool isFloating() const;

public slots:
    /**
     * @brief 刷新数据列表
     */
    void refreshData();

    /**
     * @brief 加载指定 function 的数据
     * @param functionName 接口名称
     */
    void loadFunctionData(const QString& functionName);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private slots:
    void onFunctionSelected(int index);
    void onRecordSelected();
    void onRefreshClicked();
    void onDeleteClicked();
    void onClearAllClicked();
    void onToggleFloatingClicked();
    void onMinimizeClicked();

private:
    void setupUI();
    void loadFunctionList();
    void updateRecordTable(const QList<QVariantMap>& records);
    void displayRecordDetail(const QVariantMap& data);

private:
    EAPDataCache* dataCache_;
    
    // UI 组件
    QComboBox* functionComboBox_;
    QPushButton* refreshButton_;
    QPushButton* deleteButton_;
    QPushButton* clearAllButton_;
    QPushButton* floatingButton_;
    QPushButton* minimizeButton_;
    QLabel* statusLabel_;
    
    QTableWidget* recordTable_;
    QTextEdit* detailTextEdit_;
    QSplitter* splitter_;
    
    // 浮动窗口状态
    bool floating_; // 浮动模式标志位
    bool dragging_; // 标记现在正在拖动
    QPoint dragPosition_;
    bool minimized_;
    QSize normalSize_;
};
