#pragma once
#ifdef _MSVC_LANG
#pragma execution_character_set("utf-8")
#endif 
#include "eapcore_global.h"
#include "EAPMessageLogger.h"
#include "EAPMessageRecord.h"
#include <QWidget>
#include <QTableWidget>
#include <QDateEdit>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QTextEdit>
#include <QSplitter>

// 基于QWidget的用户界面，用于显示和查询EAP消息日志
class EAPCORE_EXPORT EAPMessageLogWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EAPMessageLogWidget(QWidget* parent = nullptr);
    ~EAPMessageLogWidget() override;

    // Set the message logger instance
    void setMessageLogger(EAPMessageLogger* logger);

    // Load and display records
    void loadRecords();

    // Load records for today
    void loadTodayRecords();

public slots:
    // Slot called when new record is inserted
    void onRecordInserted(const EAPMessageRecord& record);

    // Query button clicked
    void onQueryClicked();

    // Clear button clicked
    void onClearClicked();

    // Refresh button clicked
    void onRefreshClicked();

    // Row selection changed
    void onRowSelectionChanged();

private:
    void setupUI();
    void setupConnections();
    void displayRecords(const QVector<EAPMessageRecord>& records);
    void displayRecordDetail(const EAPMessageRecord& record);
    QString formatPayloadForDisplay(const QJsonObject& payload);

private:
    EAPMessageLogger* logger_; // 数据库管理类

    // UI components
    QTableWidget* tableWidget_;
    QTextEdit* detailTextEdit_;
    
    // Filter controls
    QDateEdit* startDateEdit_;
    QDateEdit* endDateEdit_;
    QComboBox* typeComboBox_;
    QLineEdit* interfaceKeyEdit_;
    QPushButton* queryButton_;
    QPushButton* clearButton_;
    QPushButton* refreshButton_;
    QLabel* statusLabel_;

    // Current displayed records
    QVector<EAPMessageRecord> currentRecords_;
};
