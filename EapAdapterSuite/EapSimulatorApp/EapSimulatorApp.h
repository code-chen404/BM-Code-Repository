#pragma once
#ifdef _MSVC_LANG
#pragma execution_character_set("utf-8")
#endif 
#include <QWidget>
#include <QMap>
#include <QVariantMap>
#include <QPointer>

class QTabWidget;
class QSplitter;
class QGroupBox;
class QVBoxLayout;
class QHBoxLayout;
class QGridLayout;
class QPlainTextEdit;
class QLineEdit;
class QPushButton;
class QListWidget;
class QScrollArea;
class QCheckBox;

#include "EAPInterfaceManager.h"
#include "EAPWebService.h"
#include "EAPDataCache.h"
#include "EAPDataCacheWidget.h"
class EAPMessageLogger;
class EAPMessageLogWidget;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

private slots:
    // 上传客户端
    void onLoadUploadConfig();
    void onLoadUploadParams();
    void onUploadButtonClicked(const QString& key);

    // WebService 服务端
    void onLoadWebConfig();
    void onStartWebService();
    void onStopWebService();
    void onRefreshAllowListFromConfig();
    void onResponderTimeoutChanged();
    void onPortChanged();

private:
    void doLoadUploadConfig(const QString& file);
    void doLoadUploadParams(const QString& file);

    void doLoadWebConfig(const QString& file);

    void doTestJson();

private:
    // UI
    void setupUI();
    QWidget* createUploadTab();
    QWidget* createWebServiceTab();

    void appendLog(QPlainTextEdit* area, const QString& line);
    void appendJson(QPlainTextEdit* area, const QString& title, const QJsonObject& json);

    // 参数与自动填充
    bool loadParamsFromFile(const QString& filename, QMap<QString, QVariantMap>& store, QPlainTextEdit* logArea);
    QVariantMap paramsFor(const QString& key, const QMap<QString, QVariantMap>& store) const;
    void applyAutoTimeFields(const EapInterfaceMeta& meta, QVariantMap& params) const;

    // 连接日志信号
    void connectManagerLogs(EAPInterfaceManager* mgr, QPlainTextEdit* logArea, const QString& tag);
    void connectWebServiceLogs();

    void createLogger();
    void createLoggerWidget(); 
private:
    // 通用
    QTabWidget* tabs = nullptr;

    // 上传客户端
    QWidget* tabUpload = nullptr;
    QPlainTextEdit* logUpload = nullptr;
    QLineEdit* editUploadCfg = nullptr;
    QLineEdit* editUploadParams = nullptr;
    QPushButton* btnUploadCfg = nullptr;
    QPushButton* btnUploadParams = nullptr;
    QWidget* uploadBtnsContainer = nullptr;
    QVBoxLayout* uploadBtnsLayout = nullptr;
    QMap<QString, QPushButton*> uploadButtons;
    QMap<QString, QVariantMap> uploadParams;
    EAPInterfaceManager* uploadMgr = nullptr;


    
    EAPMessageLogger* messageLogger = nullptr;
    EAPMessageLogWidget* messageLogWidget = nullptr;

    // WebService 服务端
    QWidget* tabWeb = nullptr;
    QPlainTextEdit* logWeb = nullptr;
    QLineEdit* editWebCfg = nullptr;
    QLineEdit* editWebPort = nullptr;
    QLineEdit* editResponderTimeout = nullptr;
    QPushButton* btnWebCfg = nullptr;
    QPushButton* btnWebStart = nullptr;
    QPushButton* btnWebStop = nullptr;
    QPushButton* btnAllowListFromCfg = nullptr;
    QListWidget* listAllowed = nullptr;
    QCheckBox* chkCaseInsensitive = nullptr;
    QCheckBox* chkStrictHeadMatch = nullptr;
    QCheckBox* chkOnlyPush = nullptr;

    EAPWebService* webService = nullptr;

    // 缓存
    EAPDataCache* m_data_cache = nullptr;
    EAPDataCacheWidget* m_data_cache_widget = nullptr; 
    QString webConfigPath;
};