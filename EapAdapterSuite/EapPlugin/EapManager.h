#pragma once
#include <QObject>
#include "ubUiBase.h"
#include "EAPInterfaceManager.h"
#include <QMap>
#include <QTimer>
#include <QMutex>
#include "EAPUploadQueueManager.h"
#include "EAPWebService.h"
#include "EapManagerConstants.h"
#include "EAPDataCache.h"
#include "EAPDataCacheWidget.h"

struct UserInfo
{
    QString userId;
    QString userName;
    QString password;
    QString role;
	QString userStatus;
    QString userLevel;
    QString dept_id;
    QString shift_id;

    UserInfo()
        : userId("")
        , userName("")
        , password("")
        , role("")
        , userStatus("")
        ,userLevel("-1") 
        , dept_id("")
        , shift_id(""){ }
};

class EapManager : public ubUiBase
{
    Q_OBJECT
public:
    EapManager(QWidget* parent = nullptr);
    ~EapManager();

protected:
    void onUserChanged(int level) override;
    void onModelChanged(QString modelName) override;
    void onStructMsg(QString& topic, QVariantMap& msg) override;
    void onListMsg(QString& topic, QVariantList& list) override;

private:
    bool loadDeviceRequestParams(const QString& filename, QMap<QString, QVariantMap>& interfaceParams);
    QVariantMap getParamsFor(const QString& interfaceName) const;
    bool loadInitialParams(const QString& filename);

    // 新增：加载路由配置（可选）
    bool loadRoutes(const QString& filename);

    // 新增：基于路由模板构造参数（支持占位符）
    QVariantMap buildParamsFromTemplate(const QVariantMap& tmpl,
        const QVariantMap& msg) const;
    QVariant expandOneValue(const QVariant& in,
        const QVariantMap& msg) const;

    void setGlobal(QString name, QVariant var);
    bool setOnlineStatus(bool status);
    void post(const QString& interfaceKey, const QVariantMap& params);
    void setConnection(bool status);
    QVariantMap creatMapParams(const QString& interfaceName);
    bool isInerfaceEnabled(const QString& interfaceName);
    QString getAlarmLevel(const QString& alarmCode);

    // 新增：处理 CIM 消息（显示报警对话框）
    void handleCIMMessage(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq);

    // 新增：处理时间校准
    void handleDateCalibration(const QString& functionName, const QVariantMap& mappedReq, QVariantMap& out);

    // 新增：处理 Are You There 请求
    void handleAreYouThere(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out);

    // 新增：处理 CIM 模式切换命令
    void handleCimModeChangeCommand(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out);

    // 新增：处理批次命令下载
    void handleLotCommandDownload(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out);

    // 新增：处理生产信息下载
    void handleProductionInfoDownload(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out);

	void handleTestProcess(int step, QVariantMap result);

private:
    void loadDefaultParam();

    void handleSendUserVerifyMessage(const QVariantMap& msg);
private slots:
    void onRequestStarted(const QString& key, const QJsonObject& json);
    void onRequestFailed(const QString& interfaceKey, const QString& errorMsg);
    void onMappedResultReady(const QString& key, const QVariantMap& result);
    void onRequestSuccess(const QString& interfaceKey, const QJsonObject& result);


    void handleSendMessage(const QVariantMap& message);

signals:
    void sigMessage(const QVariantMap& message);

    void sigTestProcess(int step, QVariantMap result);
private:
    /**
     * @brief 路由规则结构
     * 
     * 定义了消息主题到接口的路由映射规则。
     * 支持参数模板和队列上传。
     */
    struct RouteRule {
        QString interfaceKey;       ///< MES 接口键名
        bool useQueue = false;      ///< 是否使用队列上传（离线缓存）
        QVariantMap paramTemplate;  ///< 参数模板（支持占位符替换）
    };

    /** @brief EAP 接口管理器 - 负责与 MES 系统通信 */
    EAPInterfaceManager* m_manager;
    
    /** @brief 上传队列管理器 - 负责离线数据缓存和重传 */
    EAPUploadQueueManager* m_uploadQueueManager;
    
    /** @brief 接口参数映射表 - 存储各接口的默认参数（从 infoMap.json 加载） */
    QMap<QString, QVariantMap> m_interfaceParams;
    
    /** @brief 通用参数映射表 - 存储参数映射规则（从 infoMap.json 加载） */
    QMap<QString, QVariantMap> m_mapParams;

    /** @brief 主题路由表 - 消息主题到接口的路由规则（从 routes.json 加载） */
    QHash<QString, RouteRule> m_topicRoutes;

    /** @brief 在线状态标志 - true: 在线模式，false: 离线模式 */
    bool m_isOnline = false;
    
    /** @brief 设备 ID - 设备的唯一标识符（从 eap.ini 加载） */
    QString m_deviceId;
    
    /** @brief 设备 IP 地址 - 设备的 IP 地址（从 eap.ini 加载） */
    QString m_deviceIp;

	/** @brief 设备地址 - 设备地址（从 eap.ini 加载） */
	QString m_devicePlace;
    
    /** @brief 接口配置文件路径 - MES 接口定义文件的完整路径 */
    QString m_interfaceFilePath;
    
    /** @brief 测试数据文件路径 - 用于测试的参数数据文件路径 */
    QString m_testPostDataFilePath;

    /** @brief head,body 适配层定义文件的完整路径 */
    QString m_headerParamFilePath;

    /** @brief service 服务端接口定义文件的完整路径 */
    QString m_serviceInterfaceFilePath;

    /** @brief payload 适配层定义文件的完整路径 */
    QString m_payloadParamFilePath;

    /** @brief 加载接口默认返回参数完整路径*/

    QString m_defaultReturnParamFilePath;
    
    /** @brief 当前工艺步骤 - 设备正在执行的工艺步骤名称 */
    QString m_process;
    
    /** @brief 手动工艺步骤 - 手动设定的工艺步骤名称 */
    QString m_process_manual;
    
    /** @brief 设备状态 - 设备运行状态（stop/run/emg 等） */
    QString m_deviceStatus = "stop";

    /** @brief 心跳间隔时间 - 心跳发送间隔（毫秒），默认 60000ms（1分钟） */
    int m_heartBeatTime = 60000;
    
    /** @brief 时间同步间隔 - 时间同步间隔（毫秒），默认 3600000ms（1小时） */
    int m_synTimeTime = 3600000;
    
    /** @brief 访问令牌 - 用于 MES 接口认证的 token */
    QString m_token;
    
    /** @brief 离线缓存开关 - true: 离线时缓存数据，false: 离线时丢弃数据 */
    bool m_isCacheData = false;

    /** @brief 心跳定时器 - 定期向 MES 发送心跳 */
    QTimer m_heartBeatTimer;
    
    /** @brief 时间同步定时器 - 定期与 MES 同步时间 */
    QTimer m_synTimeTimer;

    /** @brief 当前告警代码 - 最新发生的告警代码 */
    QString m_alarmCode;
    
    /** @brief 告警状态 - 告警状态（occurrence: 发生，clear: 清除） */
    QString m_alarmStatus;

    /** @brief Web 服务 - 接收 MES 下发的指令和数据 */
    EAPWebService* m_service = nullptr;

    // 缓存
    EAPDataCache* m_data_cache = nullptr; 

    // ============================================================================
    // 线程安全
    // ============================================================================
    
    /** @brief 状态互斥锁 - 保护状态变量（m_isOnline, m_deviceStatus 等） */
    mutable QMutex m_stateMutex;
    
    /** @brief 参数互斥锁 - 保护参数映射（m_interfaceParams, m_mapParams） */
    mutable QMutex m_paramsMutex;

	//用户信息
	UserInfo m_userInfo;
    
    //工单号
    QString m_wordOrder;
    int m_lotQty = 0;
};