#pragma execution_character_set("utf-8")
#include "EapManager.h"
#include "EapManagerConstants.h"
#include "EapAlarmDialog.h"
#include "EapTimeCalibration.h"
#include "UiMediator.h"
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QFile>
#include <QMessageBox>
#include <QCoreApplication>
#include <QSettings>
#include <cvmLog.h>
#include <QTimer>
#include <ubAbstractController.h>
#include "ubcontext.h"
#include <QDateTime>

#include "JsonBuilder.h"
#include "JsonParser.h"
#include "ParameterHelper.h"
#include "LoggerInterface.h"
#define EAPMANAGER_LOG "EapManager"

using namespace EapConstants;

// 占位符: ${now:fmt} / ${global:key} / ${msg:key} / ${var:key}

// 快速拿到“当前时间的字符串表示”
static QString expandNow(const QString& fmt) {
	return QDateTime::currentDateTime().toString(fmt.isEmpty() ? DEFAULT_DATETIME_FORMAT : fmt);
}

EapManager::EapManager(QWidget* parent)
	: ubUiBase(parent), m_manager(new EAPInterfaceManager(this))
{
	// 获取程序路径 + 注册日志类型
	const QString appPath = QCoreApplication::applicationDirPath();
	REGIST_LOG_TYPE(EAPMANAGER_LOG, EAPMANAGER_LOG);

	// 加载初始参数 & 接口配置 & 映射参数
	if (!loadInitialParams(appPath + "/config/eap/eap.ini")) {
		QString msg = QObject::tr("EAP/MES加载配置文件失败");
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, msg.toLocal8Bit().data());
	}
	else {
		if (!m_manager->loadInterfaceConfig(m_interfaceFilePath)) { // m_interfaceFilePath：/config/eap/config_interfaces.merged.json
			QString msg = QObject::tr("EAP/MES加载接口文档失败");
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, msg.toLocal8Bit().data());
		}
		if (!loadDeviceRequestParams(appPath + "/config/eap/infoMap.json", m_mapParams)) {
			QString msg = QObject::tr("EAP/MES加载映射参数文档失败");
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, msg.toLocal8Bit().data());
		}
	}

	// 新增：加载 Envelope 策略 + Header 参数（便于新 MES 头/外壳兼容）
	m_manager->loadEnvelopePolicy(m_payloadParamFilePath); // m_payloadParamFilePath：/config/eap/payload_policy.json
	m_manager->loadHeaderParams(m_headerParamFilePath); // m_headerParamFilePath：/config/eap/config_header_params.json

	// 新增：加载路由（可选，不存在则忽略）
	loadRoutes(appPath + "/config/eap/routes.json");

	// 创建上传队列管理器
	m_uploadQueueManager = new EAPUploadQueueManager(m_manager, this);

	// 1) 创建 WebService（被 MES 调用的服务端）
	m_service = new EAPWebService(this);

	if (!m_service->loadInterfaceConfig(m_serviceInterfaceFilePath)) { // m_serviceInterfaceFilePath：/config/eap/config_interfaces.downstream.merged.json
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, QString("加载下发接口配置失败: %1").arg(m_service->lastError()).toLocal8Bit().data());
	}
	// 3) 统一外壳策略
	QString envErr;
	if (!m_service->loadEnvelopePolicy(m_payloadParamFilePath, &envErr)) { // m_payloadParamFilePath：/config/eap/payload_policy.json
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, QString("加载外壳策略失败: %1").arg(envErr).toLocal8Bit().data());
	}

	// 创建数据缓存并注入 manager/service
	m_data_cache = new EAPDataCache(this);
	m_data_cache->initialize("./dataCache");
	m_service->setDataCache(m_data_cache);
	m_manager->setDataCache(m_data_cache);

	// 加载默认参数 /config/eap/default_params.json
	loadDefaultParam();

	// 连接信号：内部消息发送，收到消息，进行广播
	connect(this, &EapManager::sigMessage, this, &EapManager::handleSendMessage, Qt::QueuedConnection);

	// 4) 设置 rawResponder
	// 说明：这里用一个占位 provider，你可以改为访问数据库/HTTP 等，返回“本地响应字段”
	// 定义 provider：按接口名做本地业务处理
	auto provider = [this](const QString& fn, const QJsonObject& reqJson, const QVariantMap& req) -> QVariantMap {
		QVariantMap out = req;

		// 处理时间校准
		// 超颖定制
		out[JSON_RESULT] = "OK"; // 结果字段
		out[JSON_RTN_CODE] = RTN_CODE_SUCCESS; // 返回码：成功
		out[JSON_RTN_MSG] = RTN_MSG_SUCCESS; // 返回消息：成功

		///** @brief 时间校准接口 - MES 下发时间校准命令 */
		//constexpr const char* INTERFACE_DATE_TIME_CALIBRATION = "DateTimeCalibration";
		if (fn == INTERFACE_DATE_TIME_CALIBRATION) {

			handleDateCalibration(fn, req, out); // 处理时间校准/时间查询请求
		}
		
		else if (fn == INTERFACE_CIM_MESSAGE) { // CIM 消息接口 - MES 下发 CIM 消息（需要弹窗确认）
			handleCIMMessage(fn, reqJson, req);
			// 返回成功状态

		}
		else if (fn == INTERFACE_ARE_YOU_THERE) // 你是否在线
		{
			// EAP 请求获取设备是否在线
			handleAreYouThere(fn, reqJson, req, out);
		}
		else if (fn == INTERFACE_CIMMODE_CHANGE_COMMAND) // constexpr const char* INTERFACE_CIMMODE_CHANGE_COMMAND = "CIMModeChangeCommand"
		{
			// 处理 CIM 模式切换命令

			//QMetaObject::invokeMethod(this, [this, fn, reqJson, req, &out]() mutable {
			//    handleCimModeChangeCommand(fn, reqJson, req, out);
			//    }, Qt::QueuedConnection);

			handleCimModeChangeCommand(fn, reqJson, req, out);
		}
		else if (fn == INTERFACE_LOTCOMMAND_DOWNLOAD) // constexpr const char* INTERFACE_LOTCOMMAND_DOWNLOAD = "LotCommandDownload";
		{
			// 处理批次命令下载
			handleLotCommandDownload(fn, reqJson, req, out);
		}
		else if (fn == INTERFACE_PRODUCTIONINFODOWNLOAD) // constexpr const char* INTERFACE_PRODUCTIONINFODOWNLOAD = "ProductionInfoDownload";
		{
			// 处理生产信息下载
			handleProductionInfoDownload(fn, reqJson, req, out);
		}

		return out;
	};

	// 设置 rawResponder：把本地字段映射为真正 JSON 响应
	m_service->setRawResponder([this, provider](const QString& fn, const QJsonObject& reqJson, const QVariantMap& mappedReq) -> QJsonObject {
		QJsonObject out;
		// 取 meta，用 response_mapping 里 body.* 反向构 body
		const EapInterfaceMeta* meta = m_service->getMeta(fn);
		if (!meta) {
			QJsonObject h; h.insert(JSON_RESULT, STATUS_NG); h.insert(JSON_RTN_CODE, ERROR_CODE_EIC404); h.insert(JSON_RTN_MSG, "meta not found");
			out.insert(JSON_HEADER, h);
			return out;
		}

		// 外部数据源
		const QVariantMap respLocal = provider(fn, reqJson, mappedReq);

		// 若外部返回了结果码，写入响应头（其余 function_name/trx_id 框架会自动回显）
		if (respLocal.contains(JSON_RESULT) || respLocal.contains(JSON_RTN_CODE) || respLocal.contains(JSON_RTN_MSG)) {
			QJsonObject h;
			for (auto it = respLocal.begin(); it != respLocal.end(); ++it)
			{
				h.insert(it.key(), it.value().toString());
			}
			/*         if (respLocal.contains(JSON_RESULT))   h.insert(JSON_RESULT, respLocal.value(JSON_RESULT).toString());
					 if (respLocal.contains(JSON_RTN_CODE)) h.insert(JSON_RTN_CODE, respLocal.value(JSON_RTN_CODE).toString());
					 if (respLocal.contains(JSON_RTN_MSG))  h.insert(JSON_RTN_MSG, respLocal.value(JSON_RTN_MSG).toString());*/ 

			out.insert(JSON_HEADER, h);
		}

		// 用 JsonBuilder 反向构 body：临时 meta2 仅包含 bodyMap（来自 response_mapping 的 body.*）
		EapInterfaceMeta meta2 = *meta;
		meta2.enableHeader = false;
		meta2.enableBody = true;
		meta2.headerMap.clear();
		meta2.bodyMap.clear();
		for (auto it = meta->responseMap.begin(); it != meta->responseMap.end(); ++it) {
			const QString& jsonPath = it.key();
			const QString& localKey = it.value();
			if (!jsonPath.startsWith("body.")) continue;
			meta2.bodyMap.insert(localKey, jsonPath.mid(5)); // 去掉 "body."
		}

		// 仅落存在于 respLocal 的键，避免空字段输出（使用“整组透传”的 JsonBuilder 版本）
		const QJsonObject hb = JsonBuilder::buildPayload(meta2, respLocal);
		const QJsonObject body = hb.value(JSON_BODY).toObject();
		if (!body.isEmpty()) out.insert(JSON_BODY, body);

		return out;
	});

	// 启动 WebService
	if (m_service->isValid() && !m_service->start(8026, "0.0.0.0")) {
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, QString("EAPWebService 启动失败: %1").arg(m_service->lastError()).toLocal8Bit().data());
	}
	else {
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, "EAPWebService started on 0.0.0.0:8026");
	}

	// 离线测试代码
	// 离线测试信号连接
	connect(this, &EapManager::sigTestProcess, this, &EapManager::handleTestProcess, Qt::QueuedConnection);
}

EapManager::~EapManager() {
}

/**
 * @brief 加载默认参数 /config/eap/default_params.json
 */
void EapManager::loadDefaultParam()
{
	// 判断if (!QFile::exists(m_defaultReturnParamFilePath))
	if (!QFile::exists(m_defaultReturnParamFilePath)) { // m_defaultReturnParamFilePath：加载接口默认返回参数完整路径
		return;
	}
	ParameterHelper::loadDefaultParam(m_defaultReturnParamFilePath);
}

/**
 * @brief 发送用户校验状态消息
 * @param msg 原始的用户校验消息内容（不要求包含 topic 字段）
 */
void EapManager::handleSendUserVerifyMessage(const QVariantMap& msg)
{
	QVariantMap  user_verify_msg =msg;
	user_verify_msg["topic"] = "user_verify_status"; // 用户校验
	emit sigMessage(user_verify_msg);
}

void EapManager::onUserChanged(int level) {
}

/**
 * @brief 机种/型号变更回调
 * @param modelName 新的机种/型号名称
 */
void EapManager::onModelChanged(QString modelName)
{
	if (!m_isOnline) return;
	QVariantMap map = creatMapParams(INTERFACE_EQUIPMENT_INFORMATION);
	post(INTERFACE_EQUIPMENT_INFORMATION, map);
}

void EapManager::onListMsg(QString& topic, QVariantList& list) {
}

/**
 * @brief 处理结构化消息回调（topic + QVariantMap）
 * 典型用法：
 * - 其它模块只需构造 topic + msg（如 TOPIC_ONLINE_STATUS_CHANGE、TOPIC_UPLOAD_PROCESS_DATA），
 *   通过统一通道发送，EapManager 在本函数中完成与 MES 接口的组包、补参、排队与上报等逻辑，
 *   实现 UI/设备逻辑与 MES 通讯逻辑的解耦。
 * @param topic 消息主题，用于路由与分支判断
 * @param msg   消息内容，包含当前状态、参数等键值对，可在函数内部被补充和修改
 */
void EapManager::onStructMsg(QString& topic, QVariantMap& msg)
{
	// 0) 优先按路由配置处理（新增）
	if (m_topicRoutes.contains(topic)) {
		const RouteRule& r = m_topicRoutes[topic];
		QVariantMap params = buildParamsFromTemplate(r.paramTemplate, msg);
		if (r.useQueue) {
			// 入队上传（离线缓存等）
			if (m_isOnline || m_isCacheData) {
				m_uploadQueueManager->submit(r.interfaceKey, params);
			}
		}
		else {
			if (m_isOnline) post(r.interfaceKey, params);
		}
		// 命中路由后直接返回，避免重复走旧逻辑
		return;
	}

	// 1) 旧逻辑（保留，保障兼容）
	if (topic == TOPIC_ONLINE_STATUS_CHANGE) { // 在线状态变化主题 - 设备上线/离线时触发
		if (msg.contains(FIELD_STATUS)) {
			bool status = msg[FIELD_STATUS].toBool();
			// 在线离线 切换
			if (status) {
				setConnection(true);
				if (isInerfaceEnabled(INTERFACE_HEARTBEAT)) {
					QVariantMap statusMap = creatMapParams(INTERFACE_HEARTBEAT); // 根据接口名构造标准上报参数
					post(INTERFACE_HEARTBEAT, statusMap); // 发送指定接口的请求
					m_heartBeatTimer.start(m_heartBeatTime);
				}
				else {
					setOnlineStatus(true);
					QString s = QObject::tr("已切换至在线模式");
					cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, s.toLocal8Bit().data());
				}


				if (isInerfaceEnabled(INTERFACE_CIMMOE_CHANGE_REPORT))
				{
					// 通知 模式切换
					const QString s_userid = msg["user_id"].toString();

					//
					QVariantMap cimMessageMap;
					//1. 
					cimMessageMap[FIELD_ONLINE_MODE] = m_mapParams[FIELD_ONLINE_MODE].value(STATUS_ONLINE).toString();
					cimMessageMap["user_id"] = s_userid;
					post(INTERFACE_CIMMOE_CHANGE_REPORT, cimMessageMap);
				}
			}
			else {
				m_isOnline = status;
				QVariantMap statusMap = creatMapParams(INTERFACE_EQUIPMENT_INFORMATION);
				post(INTERFACE_EQUIPMENT_INFORMATION, statusMap);
				setOnlineStatus(false);
				QString s = QObject::tr("已切换至离线模式");
				cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, s.toLocal8Bit().data());


				if (isInerfaceEnabled(INTERFACE_CIMMOE_CHANGE_REPORT))
				{

					// 通知 模式切换
					const QString s_userid = msg["user_id"].toString();

					//
					QVariantMap cimMessageMap;
					//1. 
					cimMessageMap[FIELD_ONLINE_MODE] = m_mapParams[FIELD_ONLINE_MODE].value(STATUS_OFFLINE).toString();
					cimMessageMap["user_id"] = s_userid;
					post(INTERFACE_CIMMOE_CHANGE_REPORT, cimMessageMap);
				}
			}
		}
	}
	else if (topic == TOPIC_EQP_STATUS_CHANGED) { // 设备状态变化主题 - 设备状态改变时触发（运行/停止/急停等）
		if (!m_isOnline) return;
		if (msg.contains(FIELD_STATUS)) {
			QString status = msg[FIELD_STATUS].toString();
			m_deviceStatus = status;
			QVariantMap statusMap = creatMapParams(INTERFACE_EQUIPMENT_STATUS);
			post(INTERFACE_EQUIPMENT_STATUS, statusMap);

			if (m_alarmStatus == STATUS_OCCURRENCE) {
				m_alarmStatus = STATUS_CLEAR;
				QVariantMap a = creatMapParams(INTERFACE_ALARM_WARNING);
				post(INTERFACE_ALARM_WARNING, a);
			}

			
			QVariantMap realstatus = creatMapParams("StatusChangeReport");
			post("StatusChangeReport",statusMap);
		}
	}
	else if (topic == TOPIC_DOWNLOAD_PROCESS_DATA) { // 下载工艺数据主题 - 从 MES 下载工艺参数
		if (!m_isOnline) return;
		if (msg.contains(FIELD_DEVICE_ID) && msg[FIELD_DEVICE_ID].toString().isEmpty()) {
			msg[FIELD_DEVICE_ID] = m_deviceId;
			setGlobal(FIELD_DEVICE_ID, m_deviceId);
		}
		if (!msg.contains(FIELD_PROCESS_STEP_MANUAL) || msg[FIELD_PROCESS_STEP_MANUAL].toString().isEmpty()) {
			m_process_manual = UiMediator::instance()->getController()->context()->getGlobal(FIELD_PROCESS_STEP_MANUAL).toString();
			msg[FIELD_PROCESS_STEP_MANUAL] = m_process_manual;
		}

		if (msg.contains(FIELD_WORK_ORDER))
		{
			m_wordOrder = msg[FIELD_WORK_ORDER].toString();
		}
		msg[FIELD_PROCESS_STEP] = m_process;
		post(INTERFACE_DOWNLOAD_PROCESS_DATA, msg);
	}
	else if (topic == TOPIC_UPLOAD_PROCESS_DATA) { // 上传工艺数据主题 - 向 MES 上传工艺数据
		if (!m_isOnline && !m_isCacheData) return;
		if (!msg.contains(FIELD_PROCESS_STEP_MANUAL) || msg[FIELD_PROCESS_STEP_MANUAL].toString().isEmpty()) {
			m_process_manual = UiMediator::instance()->getController()->context()->getGlobal(FIELD_PROCESS_STEP_MANUAL).toString();
			msg[FIELD_PROCESS_STEP_MANUAL] = m_process_manual;
		}
		m_uploadQueueManager->submit(INTERFACE_UPLOAD_PROCESS_DATA, msg);
	}
	else if (topic == TOPIC_UPLOAD_PANEL_DATA) { // 上传面板数据主题 - 向 MES 上传面板信息
		if (!m_isOnline && !m_isCacheData) return;
		if (!msg.contains(FIELD_PROCESS_STEP_MANUAL) || msg[FIELD_PROCESS_STEP_MANUAL].toString().isEmpty()) {
			m_process_manual = UiMediator::instance()->getController()->context()->getGlobal(FIELD_PROCESS_STEP_MANUAL).toString();
			msg[FIELD_PROCESS_STEP_MANUAL] = m_process_manual;
			msg[INI_KEY_PROCESS] = m_process;
			msg[INI_KEY_DEVICE_PLACE] = m_devicePlace;
			msg[INI_KEY_DEVICE_ID] = m_deviceId;
		}
		m_uploadQueueManager->submit(INTERFACE_UPLOAD_PANEL_DATA, msg);
	}
	else if (topic == TOPIC_ALARM_WARNING) { // 告警主题 - 设备告警或警告时触发
		if (!m_isOnline) return;
		if (msg.contains(FIELD_CODE) && msg.contains(FIELD_STATUS)) {
			m_alarmCode = msg[FIELD_CODE].toString();
			m_alarmStatus = msg[FIELD_STATUS].toString();
			QVariantMap a = creatMapParams(INTERFACE_ALARM_WARNING);
			post(INTERFACE_ALARM_WARNING, a);

			if (m_alarmStatus == STATUS_OCCURRENCE) {
				m_deviceStatus = STATUS_EMG;
				QVariantMap s = creatMapParams(INTERFACE_EQUIPMENT_STATUS);
				post(INTERFACE_EQUIPMENT_STATUS, s);
			}
		}
	}
	else if (topic == TOPIC_USER_LEVEL_CHANGED) { // 用户等级变化主题 - 用户登录/登出时触发
		if (!m_isOnline)
		{
			QVariantMap  msg;
			msg["result"] = true;
			msg["errorInfo"] = "offline";
			handleSendUserVerifyMessage(msg);
			return; 
		}
		if (msg.contains(FIELD_USER_NAME) && msg.contains(FIELD_USER_LEVEL)
			&& msg.contains(FIELD_USER_ID) && msg.contains(FIELD_USER_PASSWORD)) {
			if (/*0 == msg[FIELD_USER_LEVEL].toInt() || -1 == msg[FIELD_USER_LEVEL].toInt()*/ true) {
				m_userInfo.userName = msg[FIELD_USER_NAME].toString();
				m_userInfo.userId = msg[FIELD_USER_ID].toString();
				m_userInfo.userLevel = msg[FIELD_USER_LEVEL].toString();
				m_userInfo.userStatus = msg[FIELD_USER_LEVEL].toInt() == -1 ? FIELD_USER_LOGOUT : FIELD_USER_LOGIN;
				m_userInfo.password = msg[FIELD_USER_PASSWORD].toString();

				QVariantMap tmp = creatMapParams(TOPIC_USER_LEVEL_CHANGED);
				post(INTERFACE_USERVERIFY, tmp);

				if (m_userInfo.userLevel.toInt() != -1) {
					m_alarmStatus = STATUS_CLEAR;
					QVariantMap a = creatMapParams(INTERFACE_EQUIPMENT_STATUS);
					post(INTERFACE_EQUIPMENT_STATUS, a);
				}
			}
		}
	}
	else if (topic == TOPIC_UTILITY_REPORT) //
	{
		post(TOPIC_UTILITY_REPORT, msg);
	}
}

/**
 * @brief EAP/MES加载映射参数文档失败
 *
 * 读取指定的 JSON 配置文件，将其转换为
 * QMap<QString, QVariantMap> 形式的参数映射表。
 * @param filename        JSON 配置文件路径
 * @param interfaceParams 输出参数映射表
 * @return true  加载并解析成功
 * 
 * 无引用--R
 */
bool EapManager::loadDeviceRequestParams(const QString& filename, QMap<QString, QVariantMap>& interfaceParams) {
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly)) return false;
	QJsonParseError err;
	QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &err);
	if (err.error != QJsonParseError::NoError) return false;

	QJsonObject obj = doc.object();
	interfaceParams.clear();
	for (const QString& key : obj.keys()) {
		QJsonObject paramObj = obj.value(key).toObject();
		QVariantMap paramMap;
		for (const QString& k : paramObj.keys()) {
			paramMap[k] = paramObj.value(k).toVariant();
		}
		interfaceParams[key] = paramMap;
	}
	return true;
}

/**
 * @brief 获取指定接口的参数配置
 * @param interfaceName 接口名/接口键（如 "INTERFACE_EQUIPMENT_STATUS"）
 * @return 该接口对应的参数表，或空的 QVariantMap（当未找到时）
 */
QVariantMap EapManager::getParamsFor(const QString& interfaceName) const {
	return m_interfaceParams.value(interfaceName);
}

/**
 * @brief 从 ini 配置文件加载 EAP/MES 初始参数
 * @param filename ini 配置文件的完整路径
 * @return true  读取并解析成功，成员变量已更新
 * @return false 打开失败或 QSettings 状态异常，成员变量保持原值
 * 
 * 无引用--R
 */
bool EapManager::loadInitialParams(const QString& filename)
{
	QSettings settings(filename, QSettings::IniFormat);
	if (settings.status() != QSettings::NoError) return false;

	settings.beginGroup(INI_GROUP_MACHINE_INFO);
	m_deviceId = settings.value(INI_KEY_DEVICE_ID, VALUE_EMPTY).toString(); // 设备 ID 
	m_deviceIp = settings.value(INI_KEY_DEVICE_IP, VALUE_EMPTY).toString(); // 设备 IP
	m_devicePlace = settings.value(INI_KEY_DEVICE_PLACE, VALUE_EMPTY).toString(); // 设备地址
	settings.endGroup();

	settings.beginGroup(INI_GROUP_FILE_PATH);
	const QString appPath = QCoreApplication::applicationDirPath();
	m_interfaceFilePath = appPath + settings.value(INI_KEY_INTERFACE, VALUE_EMPTY).toString(); // 接口文件路径
	m_testPostDataFilePath = appPath + settings.value(INI_KEY_TEST_DATA, VALUE_EMPTY).toString(); // 测试数据文件路径
	m_headerParamFilePath = appPath + settings.value(INI_KEY_HEADERPARAM_EVOELOPE_FILEPATH, VALUE_EMPTY).toString(); // EvoElope 文件路径

	m_payloadParamFilePath = appPath + settings.value(INI_KEY_PAYLOAD_PARAM_FILEPATH, VALUE_EMPTY).toString(); // payload 文件路径
	m_serviceInterfaceFilePath = appPath + settings.value(INI_KEY_SERVICE_INTERFACE_FILEPATH, VALUE_EMPTY).toString(); // 服务接口文件路径

	m_defaultReturnParamFilePath = appPath + settings.value("defaultParamFile", VALUE_EMPTY).toString(); // 加载接口默认返回参数完整路径
	settings.endGroup();

	settings.beginGroup(INI_GROUP_INTERFACE);
	m_heartBeatTime = settings.value(INI_KEY_HEART_BEAT_TIME, 60000).toInt(); // 心跳时间间隔
	m_synTimeTime = settings.value(INI_KEY_SYN_TIME_TIME, 3600000).toInt(); // 时间同步间隔
	m_isCacheData = settings.value(INI_KEY_OFFLINE_CACHE, false).toBool(); // 离线缓存开关
	m_token = settings.value(INI_KEY_TOKEN, VALUE_EMPTY).toString(); // 令牌
	settings.endGroup();

	settings.beginGroup(INI_GROUP_OTHER);
	m_process = settings.value(INI_KEY_PROCESS, VALUE_EMPTY).toString(); // 工艺
	m_process_manual = settings.value(INI_KEY_PROCESS_MANUAL, VALUE_EMPTY).toString(); // 手动工艺
	settings.endGroup();

	return true;
}

/**
 * @brief 设置全局上下文变量
 * @param name 全局变量名称（键）
 * @param var  要写入的值
 */
void EapManager::setGlobal(QString name, QVariant var)
{
	UiMediator::instance()->getController()->context()->setGlobal(name, var);
}

/**
 * @brief 切换 EAP 在线状态并联动相关模块
 * @param status true 表示切换至在线模式，false 表示切换至离线模式
 * @return 始终返回 true，预留以后扩展错误处理的可能
 */
bool EapManager::setOnlineStatus(bool status)
{
	// P0: 线程安全
	{
		QMutexLocker locker(&m_stateMutex);
		m_isOnline = status;
	}

	setGlobal(GLOBAL_ONLINE_STATUS, m_isOnline);

	if (!m_isOnline) {
		m_heartBeatTimer.stop();
		m_synTimeTimer.stop();
		setConnection(false);
		m_uploadQueueManager->stop();
	}
	else {
		m_uploadQueueManager->start();
	}
	return true;
}

/**
 * @brief 发送指定接口的请求（带默认参数、时间与 token 自动补全）
 * @param interfaceKey MES/EAP 接口键名（需在接口配置中存在且为 PUSH 方向）
 * @param params       业务参数；若为空则使用 m_interfaceParams 中的默认/测试参数
 */
void EapManager::post(const QString& interfaceKey, const QVariantMap& params)
{
	if (!m_manager->getInterfaceKeys().contains(interfaceKey)) return;

	loadDeviceRequestParams(m_testPostDataFilePath, m_interfaceParams);
	const QString key = interfaceKey;
	EapInterfaceMeta meta = m_manager->getInterface(key);
	if (!meta.enabled || meta.direction != DIRECTION_PUSH) return;

	QVariantMap paramsTmp = params;
	if (paramsTmp.isEmpty())
		paramsTmp = getParamsFor(key);

	for (auto it = meta.bodyMap.begin(); it != meta.bodyMap.end(); ++it) {


		if (it.key().contains(FIELD_TIME, Qt::CaseInsensitive) &&
			((paramsTmp[it.key()] == VALUE_EMPTY || paramsTmp[it.key()] == VALUE_AUTO))) {
			paramsTmp[it.key()] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
		}
		else if (it.key().contains(FIELD_TOKEN, Qt::CaseInsensitive) &&
			((paramsTmp[it.key()] == VALUE_EMPTY || paramsTmp[it.key()] == VALUE_NULL || paramsTmp[it.key()].isValid() == false))) {
			if (!m_token.isEmpty() && m_token != VALUE_NULL) {
				paramsTmp[it.key()] = m_token;
			}
		}
	}

	m_manager->post(key, paramsTmp);
}

/**
 * @brief 启用或关闭与 EAPInterfaceManager 的信号连接
 * @param status true 表示建立信号连接，false 表示断开信号连接
 */
void EapManager::setConnection(bool status)
{
	if (status) {
		connect(m_manager, &EAPInterfaceManager::requestSent, this, &EapManager::onRequestStarted);
		connect(m_manager, &EAPInterfaceManager::responseReceived, this, &EapManager::onRequestSuccess);
		connect(m_manager, &EAPInterfaceManager::mappedResultReady, this, &EapManager::onMappedResultReady);
		connect(m_manager, &EAPInterfaceManager::requestFailed, this, &EapManager::onRequestFailed);
	}
	else {
		disconnect(m_manager, &EAPInterfaceManager::requestSent, this, &EapManager::onRequestStarted);
		disconnect(m_manager, &EAPInterfaceManager::responseReceived, this, &EapManager::onRequestSuccess);
		disconnect(m_manager, &EAPInterfaceManager::mappedResultReady, this, &EapManager::onMappedResultReady);
		disconnect(m_manager, &EAPInterfaceManager::requestFailed, this, &EapManager::onRequestFailed);
	}
}

/**
 * @brief 请求发送时的回调
 * @param key   接口键名（如 INTERFACE_EQUIPMENT_STATUS）
 * @param json  即将发送的请求 JSON 内容
 */
void EapManager::onRequestStarted(const QString& key, const QJsonObject& json)
{
	QString msg = tr("请求 [%1] 已发送:\n%2").arg(key, QString::fromUtf8(QJsonDocument(json).toJson(QJsonDocument::Indented)));
	cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, msg.toLocal8Bit().data());
}

/**
 * @brief 请求成功返回时的回调（原始 JSON）
 * @param interfaceKey 接口键名
 * @param result       原始响应 JSON 对象
 */
void EapManager::onRequestSuccess(const QString& interfaceKey, const QJsonObject& result)
{
	if (INTERFACE_HEARTBEAT == interfaceKey) {
		if (!m_isOnline) {
			setOnlineStatus(true);
			QString s = tr("心跳请求成功，已切换到在线模式 [%1]").arg(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented)));
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, s.toLocal8Bit().data());

			QVariantMap map = creatMapParams(FIELD_CURRENT_TIME);
			post(FIELD_CURRENT_TIME, map);
			connect(&m_synTimeTimer, &QTimer::timeout, this, [=]() {
				post(FIELD_CURRENT_TIME, map);
				});
			m_synTimeTimer.start(m_synTimeTime);

			map = creatMapParams(INTERFACE_INITIAL_DATA);
			post(INTERFACE_INITIAL_DATA, map);

			map = creatMapParams(INTERFACE_EQUIPMENT_INFORMATION);
			post(INTERFACE_EQUIPMENT_INFORMATION, map);

			connect(&m_heartBeatTimer, &QTimer::timeout, this, [=]() {
				QVariantMap statusMap = creatMapParams(INTERFACE_HEARTBEAT);
				post(INTERFACE_HEARTBEAT, statusMap);
				});
			m_heartBeatTimer.start(m_heartBeatTime);
		}
	}
	else if (INTERFACE_DOWNLOAD_PROCESS_DATA == interfaceKey) {
		QString s = tr("工单录入信息数据请求成功 [%1]").arg(QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented)));
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, s.toLocal8Bit().data());
	}

	else if (INTERFACE_USERVERIFY == interfaceKey)
	{
		// 用户校验:
		QString s = tr("请求成功 [%1]: %2").arg(interfaceKey, QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented)));
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, s.toLocal8Bit().data());
		QVariantMap  msg;
		msg["result"] = true;

		handleSendUserVerifyMessage(msg);
	}
	else {
		QString s = tr("请求成功 [%1]: %2").arg(interfaceKey, QString::fromUtf8(QJsonDocument(result).toJson(QJsonDocument::Indented)));
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, s.toLocal8Bit().data());
	}
}

/**
 * @brief 映射后的业务结果就绪回调
 * @param key     接口键名
 * @param result  已映射的结果数据（业务字段）
 */
void EapManager::onMappedResultReady(const QString& key, const QVariantMap& result)
{
	QJsonDocument doc = QJsonDocument::fromVariant(result);
	if (INTERFACE_DOWNLOAD_PROCESS_DATA == key) {
		if (result.contains(FIELD_IS_SUCCESS)) {

			if (result[FIELD_IS_SUCCESS].toBool())
			{
				QVariantMap modelMap = result;
				QString topic = QString(INTERFACE_UPDATE_PROCESS_DATA);
				UiMediator::instance()->broadcast(topic, modelMap);
			}
			else
			{
				QString s = tr("工单录入信息数据下载失败 [%1]").arg(QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
				cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, s.toLocal8Bit().data());
			}

		}
		else {


			QString s = tr("工单: %1 请求成功").arg(m_wordOrder);
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, s.toLocal8Bit().data());

			QVariantMap map = creatMapParams(TOPIC_CARRIER_STATUS_REPORT);
			map[FIELD_CARRIER_STATUS] = "WaitData";
			post(TOPIC_CARRIER_STATUS_REPORT, map);

		}
	}
	if (INTERFACE_USERVERIFY == key)
	{
		//离线测试代码
		m_userInfo.dept_id = result.value("dept_id").toString();
		m_userInfo.shift_id = result.value("shift_id").toString();
	}
}

/**
 * @brief 请求失败时的回调
 * @param interfaceKey 失败的接口键名
 * @param errorMsg     失败原因描述
 */
void EapManager::onRequestFailed(const QString& interfaceKey, const QString& errorMsg)
{
	if (INTERFACE_HEARTBEAT == interfaceKey) {
		QString s;
		if (!m_isOnline) {
			s = tr("建立心跳请求失败 [%1]: %2").arg(interfaceKey, errorMsg);
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, s.toLocal8Bit().data());
		}
		else {
			s = tr("心跳请求失败，已切换到离线模式 [%1]: %2").arg(interfaceKey, errorMsg);
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, s.toLocal8Bit().data());
		}
		setOnlineStatus(false);
		QVariantMap modelMap;
		modelMap[JSON_RESULT] = false;
		modelMap[FIELD_MESSAGE] = s;
		QString topic = QString(INTERFACE_ONLINE_STATUS_SET);
		UiMediator::instance()->broadcast(topic, modelMap);
	}
	else if (INTERFACE_USERVERIFY == interfaceKey)
	{
		QVariantMap  msg;
		msg["result"] = false;
		msg["errorInfo"] = "connection failed";
		handleSendUserVerifyMessage(msg);
	}
}

/**
 * @brief 根据接口名构造标准上报参数
 * @param interfaceName 接口名或主题名，用于区分不同的参数构造逻辑
 * @return QVariantMap  预填充好的参数表，可直接传入 post() 使用
 */
QVariantMap EapManager::creatMapParams(const QString& interfaceName)
{
	QVariantMap map;
	if (INTERFACE_HEARTBEAT == interfaceName) { // 心跳接口 - 定期向 MES 发送心跳保持连接
		map[FIELD_DEVICE_ID] = m_deviceId;
		map[FIELD_DEVICE_IP] = m_deviceIp;
		map[FIELD_DATETIME] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
	}
	else if (INTERFACE_INITIAL_DATA == interfaceName) { // 初始数据接口 - 上报初始化数据
		map[FIELD_DATETIME] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
		if (m_mapParams.contains(FIELD_ONLINE_MODE) && m_mapParams[FIELD_ONLINE_MODE].size() > 0) {
			map[FIELD_ONLINE_MODE] = m_isOnline ? m_mapParams[FIELD_ONLINE_MODE].value(STATUS_ONLINE) : m_mapParams[FIELD_ONLINE_MODE].value(STATUS_OFFLINE);
		}
		if (m_mapParams.contains(INTERFACE_EQUIPMENT_STATUS) && m_mapParams[INTERFACE_EQUIPMENT_STATUS].size() > 0) {
			
			map[INTERFACE_EQUIPMENT_STATUS] = m_mapParams[INTERFACE_EQUIPMENT_STATUS].value(m_deviceStatus);
		}
		map[FIELD_RECIPE_NAME] = UiMediator::instance()->getController()->context()->getGlobal(FIELD_RECIPE_NAME).toString();
	}
	else if (FIELD_CURRENT_TIME == interfaceName) { // 当前时间字段
		QString format = DEFAULT_DATETIME_FORMAT;
		if (m_mapParams.contains(INTERFACE_TIME_CONFIG) && m_mapParams[INTERFACE_TIME_CONFIG].size() > 0) {
			format = m_mapParams[INTERFACE_TIME_CONFIG].value(JSON_FORMAT).toString();
		}
		map[FIELD_DATETIME] = QDateTime::currentDateTime().toString(format);
	}
	else if (INTERFACE_EQUIPMENT_INFORMATION == interfaceName) { // 设备信息接口 - 上报设备基本信息
		if (m_mapParams.contains(FIELD_ONLINE_MODE) && m_mapParams[FIELD_ONLINE_MODE].size() > 0) {
			map[LINE_CLEANING] = "1";
			map[FIELD_ONLINE_MODE] = m_isOnline ? m_mapParams[FIELD_ONLINE_MODE].value(STATUS_ONLINE) : m_mapParams[FIELD_ONLINE_MODE].value(STATUS_OFFLINE);
			map[FIELD_RECIPE_NAME] = UiMediator::instance()->getController()->context()->getGlobal(FIELD_CURRENT_MODEL).toString();
		}
	}
	else if (INTERFACE_EQUIPMENT_STATUS == interfaceName) { // 设备状态接口 - 上报设备运行状态
		map[FIELD_DATETIME] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
		if (m_mapParams.contains(FIELD_ONLINE_MODE) && m_mapParams[FIELD_ONLINE_MODE].size() > 0) {
			map[FIELD_ONLINE_MODE] = m_isOnline ? m_mapParams[FIELD_ONLINE_MODE].value(STATUS_ONLINE) : m_mapParams[FIELD_ONLINE_MODE].value(STATUS_OFFLINE);
		}
		if (m_mapParams.contains(INTERFACE_EQUIPMENT_STATUS) && m_mapParams[INTERFACE_EQUIPMENT_STATUS].size() > 0) {

			map[INTERFACE_EQUIPMENT_STATUS] = m_mapParams[INTERFACE_EQUIPMENT_STATUS].value(m_deviceStatus);
		}
		if (m_mapParams.contains(FIELD_ALARM_CONTENT) && m_mapParams[FIELD_ALARM_CONTENT].size() > 0) {
			map[FIELD_ALARM_CODE] = m_alarmCode;
			map[FIELD_ALARM_TEXT] = m_mapParams[FIELD_ALARM_CONTENT].value(m_alarmCode);
		}


		map[FIELD_WORK_ORDER] = UiMediator::instance()->getController()->context()->getGlobal(FIELD_WORK_ORDER).toString();
		map[FIELD_USER_ID] = m_userInfo.userId;
	}
	else if ( "StatusChangeReport" == interfaceName)
	{
		map[FIELD_DATETIME] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
		if (m_mapParams.contains(FIELD_ONLINE_MODE) && m_mapParams[FIELD_ONLINE_MODE].size() > 0) {
			map[FIELD_ONLINE_MODE] = m_isOnline ? m_mapParams[FIELD_ONLINE_MODE].value(STATUS_ONLINE) : m_mapParams[FIELD_ONLINE_MODE].value(STATUS_OFFLINE);
		}
		if (m_mapParams.contains(INTERFACE_EQUIPMENT_STATUS) && m_mapParams[INTERFACE_EQUIPMENT_STATUS].size() > 0) {

			map[INTERFACE_EQUIPMENT_STATUS] = m_mapParams[INTERFACE_EQUIPMENT_STATUS].value(m_deviceStatus);
		}
		if (m_mapParams.contains(FIELD_ALARM_CONTENT) && m_mapParams[FIELD_ALARM_CONTENT].size() > 0) {
			map[FIELD_ALARM_CODE] = m_alarmCode;
			map[FIELD_ALARM_TEXT] = m_mapParams[FIELD_ALARM_CONTENT].value(m_alarmCode);
		}


		map[FIELD_WORK_ORDER] = UiMediator::instance()->getController()->context()->getGlobal(FIELD_WORK_ORDER).toString();
		map[FIELD_USER_ID] = m_userInfo.userId;
	}

	else if (INTERFACE_ALARM_WARNING == interfaceName) { // 告警接口 - 上报告警和警告信息
		if (m_mapParams[FIELD_ALARM_CONTENT].size() > 0
			&& m_mapParams[FIELD_ALARM_STATUS].size() > 0 && m_mapParams[FIELD_ALARM_LEVEL].size() > 0) {
			map[FIELD_ALARM_CODE] = m_alarmCode;
			map[FIELD_ALARM_TEXT] = m_mapParams[FIELD_ALARM_CONTENT].value(m_alarmCode);
			map[FIELD_ALARM_STATUS] = m_mapParams[FIELD_ALARM_STATUS].value(m_alarmStatus);
			map[FIELD_ALARM_LEVEL] = getAlarmLevel(m_alarmCode);
			map[FIELD_DATETIME] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
			
		}
	}
	else if (TOPIC_USER_LEVEL_CHANGED == interfaceName) { // 用户等级变化主题 - 用户登录/登出时触发
		if (m_mapParams[FIELD_USER_STATE].size() > 0) {
			map[FIELD_USER_ID] = m_userInfo.userId;
			map[FIELD_USER_NAME] = m_userInfo.userName;
			map[FIELD_USER_STATE] = m_mapParams[FIELD_USER_STATE].value(m_userInfo.userStatus);
			map[FIELD_USER_PASSWORD] = m_userInfo.password;
		}
	}

	else if (TOPIC_CARRIER_STATUS_REPORT == interfaceName) { // 载具状态上报
		map[FIELD_WORK_ORDER] = m_wordOrder;
		map[FIELD_LOT_QTY] = m_lotQty;
	}
	// 加载默认参数

	return map;
}

/**
 * @brief 判断指定接口是否已启用
 * @param interfaceName 接口键名（如 INTERFACE_HEARTBEAT）
 * @return true  表示接口存在且已启用
 * @return false 接口不存在或未启用
 */
bool EapManager::isInerfaceEnabled(const QString& interfaceName)
{
	if (!m_manager->getInterfaceKeys().contains(interfaceName)) return false;
	EapInterfaceMeta meta = m_manager->getInterface(interfaceName);
	return meta.enabled;
}

/**
 * @brief 根据告警代码获取告警等级编码
 * @param alarmCode 告警代码（如安全门打开、安全光栅触发等）
 * @return 告警等级编码字符串
 */
QString EapManager::getAlarmLevel(const QString& alarmCode)
{
	if (ALARM_SAFETY_DOOR_OPEN == alarmCode || ALARM_SAFETY_GRATING_TRIGGERED == alarmCode) {
		return m_mapParams[FIELD_ALARM_LEVEL].value(ALARM_LEVEL_WARNING).toString();
	}
	else {
		return m_mapParams[FIELD_ALARM_LEVEL].value(ALARM_LEVEL_ALARM).toString();
	}
}

/**
 * @brief 将带 topic 的消息广播到 UI/其他模块
 * @param message 包含 "topic" 字段的消息体，其他字段为具体业务内容
 */
void EapManager::handleSendMessage(const QVariantMap& message)
{
	QString topic = message["topic"].toString();
	QVariantMap map = message;
	UiMediator::instance()->broadcast(topic, map);
}

// --------- 新增：路由加载与占位符展开 ---------

/**
 * @brief 从 JSON 路由配置文件加载 topic → 接口 路由规则
 * @param filename 路由配置文件路径（通常为 routes.json 的完整路径）
 * @return true  文件存在且 JSON 结构正确（无论是否包含 routes）
 * @return false 文件无法打开或 JSON 解析失败
 */
bool EapManager::loadRoutes(const QString& filename)
{
	QFile f(filename);
	if (!f.open(QIODevice::ReadOnly)) return false;
	QJsonParseError jerr{};
	const auto doc = QJsonDocument::fromJson(f.readAll(), &jerr);
	if (jerr.error != QJsonParseError::NoError || !doc.isObject()) return false;

	m_topicRoutes.clear();
	const QJsonObject root = doc.object();
	if (!root.contains(JSON_ROUTES) || !root.value(JSON_ROUTES).isObject()) return true;

	const QJsonObject routes = root.value(JSON_ROUTES).toObject();
	for (auto it = routes.begin(); it != routes.end(); ++it) {
		if (!it.value().isObject()) continue;
		const QString topic = it.key();
		const QJsonObject r = it.value().toObject();
		RouteRule rule;
		rule.interfaceKey = r.value(INI_KEY_INTERFACE).toString();
		rule.useQueue = r.value(JSON_QUEUE).toBool(false);
		if (r.contains(JSON_PARAMS) && r.value(JSON_PARAMS).isObject())
			rule.paramTemplate = r.value(JSON_PARAMS).toObject().toVariantMap();
		if (!rule.interfaceKey.isEmpty())
			m_topicRoutes.insert(topic, rule);
	}
	return true;
}

/**
 * @brief 根据参数模板和消息内容展开占位符，生成最终请求参数
 * 示例：
 * @code
 * QVariantMap tmpl;
 * tmpl["trx_id"]    = "TRX_${now:yyyyMMddHHmmss}";
 * tmpl["device_id"] = "${var:device_id}";
 * tmpl["panel_id"]  = "${msg:panel_id}";
 *
 * QVariantMap msg;
 * msg["panel_id"] = "PNL001";
 *
 * QVariantMap params = buildParamsFromTemplate(tmpl, msg);
 * // params 结果类似：
 * //  trx_id    = "TRX_20251202123045"
 * //  device_id = "EQP001"
 * //  panel_id  = "PNL001"
 * @endcode
 *
 * @param tmpl 参数模板（键名即最终参数名，值中可以包含占位符）
 * @param msg  当前消息内容，用于 ${msg:xxx} 占位符展开
 * @return 展开后的参数表，可直接用于调用 post()/uploadQueueManager->submit()
 */
QVariantMap EapManager::buildParamsFromTemplate(const QVariantMap& tmpl,
	const QVariantMap& msg) const
{
	QVariantMap out;
	for (auto it = tmpl.begin(); it != tmpl.end(); ++it) {
		out.insert(it.key(), expandOneValue(it.value(), msg));
	}
	return out;
}

/**
 * @brief 对单个 QVariant 值进行占位符展开（递归支持 map 和 list）
 * @param in   输入的 QVariant 值（可能是字符串、map、list 或简单类型）
 * @param msg  当前消息内容，用于 ${msg:xxx} 占位符展开
 * @return     展开占位符后的 QVariant 值
 */
QVariant EapManager::expandOneValue(const QVariant& in, const QVariantMap& msg) const
{
	switch (in.userType()) {
	case QMetaType::QString: {
		QString s = in.toString();

		// ${now:fmt}
		int pos = 0;
		while ((pos = s.indexOf("${now:", pos)) >= 0) {
			int end = s.indexOf('}', pos);
			if (end < 0) break;
			const QString token = s.mid(pos, end - pos + 1);
			const QString fmt = token.mid(6, token.size() - 7); // 去掉 ${now: 和 }
			s.replace(token, expandNow(fmt));
			pos = pos + 1;
		}

		// ${global:key}
		int gpos = 0;
		while ((gpos = s.indexOf("${global:", gpos)) >= 0) {
			int end = s.indexOf('}', gpos);
			if (end < 0) break;
			const QString token = s.mid(gpos, end - gpos + 1);
			const QString key = token.mid(9, token.size() - 10);
			const QVariant v = UiMediator::instance()->getController()->context()->getGlobal(key);
			s.replace(token, v.toString());
			gpos = gpos + 1;
		}

		// ${msg:key}
		int mpos = 0;
		while ((mpos = s.indexOf("${msg:", mpos)) >= 0) {
			int end = s.indexOf('}', mpos);
			if (end < 0) break;
			const QString token = s.mid(mpos, end - mpos + 1);
			const QString key = token.mid(6, token.size() - 7);
			s.replace(token, msg.value(key).toString());
			mpos = mpos + 1;
		}

		// ${var:key} 来自 EapManager 成员
		auto readVar = [&](const QString& k)->QString {
			if (k == "device_id") return m_deviceId;
			if (k == "device_ip") return m_deviceIp;
			if (k == "process") return m_process;
			if (k == "process_manual") return m_process_manual;
			if (k == "alarm_code") return m_alarmCode;
			if (k == "alarm_status") return m_alarmStatus;
			if (k == "status") return m_deviceStatus;
			return QString();
			};
		int vpos = 0;
		while ((vpos = s.indexOf("${var:", vpos)) >= 0) {
			int end = s.indexOf('}', vpos);
			if (end < 0) break;
			const QString token = s.mid(vpos, end - vpos + 1);
			const QString key = token.mid(6, token.size() - 7);
			s.replace(token, readVar(key));
			vpos = vpos + 1;
		}

		return s;
	}
	case QMetaType::QVariantMap: {
		QVariantMap out;
		const QVariantMap inMap = in.toMap();
		for (auto it = inMap.begin(); it != inMap.end(); ++it)
			out.insert(it.key(), expandOneValue(it.value(), msg));
		return out;
	}
	case QMetaType::QVariantList: {
		QVariantList out;
		const QVariantList inList = in.toList();
		for (const auto& v : inList)
			out.push_back(expandOneValue(v, msg));
		return out;
	}
	default:
		return in;
	}
}

// ============================================================================
// CIM 消息处理 - 弹窗显示报警信息
// ============================================================================

/**
 * @brief 处理 MES 下发的 CIM 消息（屏幕提示/报警），并在 UI 上弹出对话框
 * @param functionName 当前 WebService 接口名（如 INTERFACE_CIM_MESSAGE）
 * @param reqJson      MES 下发的原始请求 JSON 对象
 * @param mappedReq    已映射的本地参数（当前实现中未使用，预留扩展）
 */
void EapManager::handleCIMMessage(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq)
{
	// 从请求中提取消息内容
	const EapInterfaceMeta* meta = m_service->getMeta(functionName);
	// 提取内部字段信息
	const QString cim_body_key = QString("body.%1").arg(meta->bodyMap[FIELD_CIMMessage]);
	QString message = JsonParser::parseJson(*meta, reqJson, cim_body_key).toString();

	// 控制方式
	const QString screen_control_body = QString("body.%1").arg(meta->bodyMap[FIELD_SCREEN_CONTROL]);


	QString screen_control = JsonParser::parseJson(*meta, reqJson, screen_control_body).toString();

	const QString screen_code_body = QString("body.%1").arg(meta->bodyMap[FIELD_ScreenCode]);

	QString screen_code = JsonParser::parseJson(*meta, reqJson, screen_code_body).toString();

	const QString autocloseTime_body = QString("body.%1").arg(meta->bodyMap[FIELD_INTERNAL_TIME]);

	// 获取自动关闭时间（秒），默认 30 秒，0 表示需要手动确认
	int autoCloseSeconds = JsonParser::parseJson(*meta, reqJson, autocloseTime_body).toInt();

	// 记录日志
	QString logMsg = QString("收到 CIM 消息: %1, 自动关闭时间: %2 秒").arg(message).arg(autoCloseSeconds);
	cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, logMsg.toLocal8Bit().data());

	// 创建并显示报警对话框（使用 Qt::QueuedConnection 确保在主线程执行）
	QMetaObject::invokeMethod(this, [this, message, autoCloseSeconds]() {
		EapAlarmDialog* dialog = new EapAlarmDialog(message, autoCloseSeconds, this);

		// 连接确认信号
		connect(dialog, &EapAlarmDialog::confirmed, this, [this, message]() {
			QString confirmMsg = QString("CIM 消息已确认: %1").arg(message);
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, confirmMsg.toLocal8Bit().data());
			});

		// 显示对话框（模态）
		dialog->exec();
		dialog->deleteLater();
		}, Qt::QueuedConnection);
}

// ============================================================================
// 时间校准处理
// ============================================================================

/**
 * @brief 处理时间校准/时间查询请求
 *
 * 用于处理 MES 下发的时间相关接口（如 INTERFACE_DATE_TIME_CALIBRATION）。
 * @param functionName 接口名称（如 INTERFACE_DATE_TIME_CALIBRATION，当前实现未直接使用）
 * @param mappedReq    已映射的请求参数表（本地字段名），从中解析目标时间和格式
 * @param out          输出参数表，写入 IP、当前时间及标准 result/code/msg 等响应字段
 */
void EapManager::handleDateCalibration(const QString& functionName, const QVariantMap& mappedReq, QVariantMap& out)
{
	// 1. 获取本机 IP 地址
	QString localIp = EapTimeCalibration::getLocalIpAddress();
	///** @brief IP 地址字段 */ 
	//constexpr const char* FIELD_IP = "ip";
	out[FIELD_IP] = localIp;

	// 2. 从请求里尝试找“目标时间”字段（你这里为了兼容多种字段名）
	//INTERFACE_DATETIME_CALIBRATION = "datetimeCalibration";
	//FIELD_NOW = "now";
	const QString datetime_key = m_mapParams[INTERFACE_DATETIME_CALIBRATION].value(FIELD_NOW, FIELD_NOW).toString();
	QString calibrationTime = mappedReq.value(datetime_key).toString();
	if (calibrationTime.isEmpty()) {
		calibrationTime = mappedReq.value("time").toString();
	}
	if (calibrationTime.isEmpty()) {
		calibrationTime = mappedReq.value("calibration_time").toString();
	}

	// 如果找到了目标时间 → 设置系统时间
	if (!calibrationTime.isEmpty()) {
		// 需要设置系统时间
		QString format = mappedReq.value("format", DEFAULT_DATETIME_FORMAT).toString();

		QString logMsg = QString("收到时间校准指令，目标时间: %1").arg(calibrationTime);
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, logMsg.toLocal8Bit().data());

		bool success = EapTimeCalibration::setSystemTime(calibrationTime, format);

		if (success) {
			out[JSON_RESULT] = "OK";
			out[JSON_RTN_CODE] = RTN_CODE_SUCCESS;
			out[JSON_RTN_MSG] = RTN_MSG_SUCCESS;
			out[FIELD_NOW] = EapTimeCalibration::getLocalTime(format);

			QString successMsg = QString("系统时间已校准为: %1").arg(out[FIELD_NOW].toString());
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, successMsg.toLocal8Bit().data());
		}
		else {
			out[JSON_RESULT] = STATUS_NG;
			out[JSON_RTN_CODE] = RTN_CODE_INTERNAL_ERROR;
			out[JSON_RTN_MSG] = QString("时间校准失败: %1").arg(EapTimeCalibration::lastError());
			out[FIELD_NOW] = EapTimeCalibration::getLocalTime(format);

			QString errorMsg = QString("时间校准失败: %1").arg(EapTimeCalibration::lastError());
			cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::warn, errorMsg.toLocal8Bit().data());
		}
	}
	else {
		// 仅查询当前时间，不设置
		QString format = mappedReq.value("format", DEFAULT_DATETIME_FORMAT).toString();
		out[FIELD_NOW] = EapTimeCalibration::getLocalTime(format);
		out[JSON_RESULT] = "OK";
		out[JSON_RTN_CODE] = RTN_CODE_SUCCESS;
		out[JSON_RTN_MSG] = RTN_MSG_SUCCESS;

		QString queryMsg = QString("查询本机时间: %1, IP: %2").arg(out[FIELD_NOW].toString(), localIp);
		cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, queryMsg.toLocal8Bit().data());
	}
}

// ============================================================================
// Are You There 处理
// ============================================================================

/**
 * @brief 处理 MES 的 AreYouThere 探活请求
 * @param functionName 接口名（如 INTERFACE_ARE_YOU_THERE，当前实现未直接使用）
 * @param reqJson      原始请求 JSON 对象（当前实现未使用，预留扩展）
 * @param mappedReq    已映射的请求参数表（可包含 trx_id 等，当前实现仅在注释中示例）
 * @param out          输出参数表，当前版本只在外部设置通用 result/code/msg，
 *                     本函数主要用于记录日志，如需返回更多状态可在此填充 out
 */
void EapManager::handleAreYouThere(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out)
{
	// 线程安全：读取在线状态
	bool isOnline;
	{
		QMutexLocker locker(&m_stateMutex);
		isOnline = m_isOnline;
	}

	//// 提取 trx_id（如果存在）
	//if (mappedReq.contains(FIELD_TRX_ID)) {
	//    out[FIELD_TRX_ID] = mappedReq.value(FIELD_TRX_ID);
	//}
	//
	//// 返回设备在线状态和基本信息
	//out["device_id"] = m_deviceId;
	//out["device_ip"] = m_deviceIp;
	//out["online_status"] = isOnline ? "online" : "offline";
	//out["device_status"] = m_deviceStatus;
	//out["timestamp"] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);


	QString logMsg = QString("收到 AreYouThere 请求，设备状态: %1, 在线状态: %2")
		.arg(m_deviceStatus, isOnline ? "online" : "offline");
	cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, logMsg.toLocal8Bit().data());
}

// ============================================================================
// CIM 模式切换命令处理
// ============================================================================

/**
 * @brief 处理 MES 下发的 CIM 模式切换命令（在线/离线切换）
 * @param functionName 接口名称（如 INTERFACE_CIMMODE_CHANGE_COMMAND）
 * @param reqJson      MES 下发的原始 JSON 请求
 * @param mappedReq    已映射到本地字段名的请求参数（当前实现中未直接使用）
 * @param out          输出参数，写入错误信息/业务字段等返回给 MES
 */
void EapManager::handleCimModeChangeCommand(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out)
{
	const EapInterfaceMeta* meta = m_service->getMeta(functionName);
	if (!meta) {
		out[JSON_RESULT] = STATUS_NG;
		out[JSON_RTN_CODE] = RTN_CODE_INTERNAL_ERROR;
		out[JSON_RTN_MSG] = "Interface metadata not found";
		return;
	}

	// 提取模式切换参数
	const QString cimMode_route = meta->bodyMap[FIELD_ONLINE_MODE];
	const QString userid_route = meta->bodyMap[FIELD_USER_ID];
	QString cimMode = JsonParser::parseJson(*meta, reqJson, cimMode_route).toString();
	QString transactionId = JsonParser::parseJson(*meta, reqJson, "body.trx_id").toString();
	QString s_userId = JsonParser::parseJson(*meta, reqJson, userid_route).toString();
	if (cimMode.isEmpty()) {
		out[JSON_RESULT] = STATUS_NG;
		out[JSON_RTN_CODE] = RTN_CODE_REQUIRED_FIELD_MISSING;
		out[JSON_RTN_MSG] = "CIM mode is required";
		return;
	}

	// 记录日志
	QString logMsg = QString("收到 CIM 模式切换命令，目标模式: %1, trx_id: %2").arg(cimMode, transactionId);
	cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, logMsg.toLocal8Bit().data());

	// 将这个模式与 m_mapParams中的进行比较

	// 是否是在线模式
	const QString internal_online_value = m_mapParams[FIELD_ONLINE_MODE].value(STATUS_ONLINE).toString();
	if (cimMode == internal_online_value)
	{
		// 在线模式
		QVariantMap broadcastData;
		setOnlineStatus(true);;
		broadcastData[JSON_RESULT] = true;
		broadcastData["topic"] = INTERFACE_ONLINE_STATUS_SET;
		emit sigMessage(broadcastData);

		
		//UiMediator::instance()->broadcast(topic, broadcastData);
	}
	else
	{
		// 离线模式
		QVariantMap broadcastData;
		setOnlineStatus(false);;
		broadcastData[JSON_RESULT] = false;
		broadcastData["topic"] = INTERFACE_ONLINE_STATUS_SET;
		emit sigMessage(broadcastData);
		//UiMediator::instance()->broadcast(topic, broadcastData);

		
	}

	//
	//// 保存到数据缓存（如果配置了 saveToDb）
	//if (m_data_cache && m_data_cache->isInitialized()) {
	//    QVariantMap dataToSave;
	//    dataToSave["cim_mode"] = cimMode;
	//    dataToSave["transaction_id"] = transactionId;
	//    dataToSave["timestamp"] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
	//    dataToSave["status"] = "pending";
	//    
	//    QString saveKey = QString("%1.%2").arg(functionName, transactionId);
	//    if (m_data_cache->saveData(saveKey, dataToSave)) {
	//        QString saveMsg = QString("CIM 模式切换命令已保存到缓存: %1").arg(saveKey);
	//        cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, saveMsg.toLocal8Bit().data());
	//    }
	//}

	// 设置全局变量通知其他模块
	setGlobal("cim_mode", cimMode);
	setGlobal("cim_mode_transaction_id", transactionId);

	// 返回成功响应
	out["cim_mode"] = cimMode;
	out["transaction_id"] = transactionId;
}

// ============================================================================
// 批次命令下载处理
// ============================================================================

/**
 * @brief 处理 MES 下发的批次命令下载（LotCommandDownload）请求
 * @param functionName 接口名称（如 INTERFACE_LOTCOMMAND_DOWNLOAD）
 * @param reqJson      MES 下发的原始 JSON 请求报文
 * @param mappedReq    已映射的本地请求参数（当前实现未直接使用，预留扩展）
 * @param out          输出参数表，用于向 MES 返回处理结果和业务字段
 */
void EapManager::handleLotCommandDownload(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out)
{
	const EapInterfaceMeta* meta = m_service->getMeta(functionName);
	if (!meta) {
		out[JSON_RESULT] = STATUS_NG;
		out[JSON_RTN_CODE] = RTN_CODE_INTERNAL_ERROR;
		out[JSON_RTN_MSG] = "Interface metadata not found";
		return;
	}

	QVariantMap message = JsonBuilder::buildMapping(meta->bodyMap, reqJson);


	//// 保存到数据缓存
	//if (m_data_cache && m_data_cache->isInitialized()) {
	//    QVariantMap dataToSave;
	//    dataToSave["lot_id"] = lotId;
	//    dataToSave["command"] = command;
	//    dataToSave["transaction_id"] = transactionId;
	//    dataToSave["timestamp"] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
	//    dataToSave["status"] = "pending";
	//    
	//    // 提取其他参数（如果有）
	//    QVariant parameters = JsonParser::parseJson(*meta, reqJson, "body.parameters");
	//    if (parameters.isValid()) {
	//        dataToSave["parameters"] = parameters;
	//    }
	//    
	//    QString saveKey = QString("%1.%2").arg(functionName, lotId);
	//    if (m_data_cache->saveData(saveKey, dataToSave)) {
	//        QString saveMsg = QString("批次命令已保存到缓存: %1").arg(saveKey);
	//        cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, saveMsg.toLocal8Bit().data());
	//    }
	//}

	//// 广播消息到其他模块
	//QVariantMap broadcastData;
	//broadcastData["lot_id"] = lotId;
	//broadcastData["command"] = command;
	//broadcastData["data"] = reqJson;
	//broadcastData["topic"] = INTERFACE_LOTCOMMAND_DOWNLOAD;
	//emit sigMessage(broadcastData);

	// 广播消息到其他模块
	QVariantMap broadcastData;
	broadcastData = message;
	broadcastData["topic"] = INTERFACE_LOTCOMMAND_DOWNLOAD;
	emit sigMessage(broadcastData);
	//broadcastData["transaction_id"] = transactionId;
	//QString topic = QString("lot_command_received");
	//UiMediator::instance()->broadcast(topic, broadcastData);

	// 返回成功响应

	out["status"] = "received";

	//离线测试代码
	emit sigTestProcess(2, message);
}

// ============================================================================
// 生产信息下载处理
// ============================================================================

/**
 * @brief 处理 MES 下发的生产信息下载（ProductionInfoDownload）请求
 * @param functionName 接口名称（如 INTERFACE_PRODUCTIONINFODOWNLOAD）
 * @param reqJson      MES 下发的原始 JSON 请求报文
 * @param mappedReq    已映射的本地请求参数（当前实现未直接使用，预留扩展）
 * @param out          输出参数表，用于向 MES 返回处理状态及业务字段（lotId 等）
 */
void EapManager::handleProductionInfoDownload(const QString& functionName, const QJsonObject& reqJson, const QVariantMap& mappedReq, QVariantMap& out)
{
	const EapInterfaceMeta* meta = m_service->getMeta(functionName);

	QVariantMap message = JsonBuilder::buildMapping(meta->bodyMap,reqJson);

	if (!meta) {
		out[JSON_RESULT] = STATUS_NG;
		out[JSON_RTN_CODE] = RTN_CODE_INTERNAL_ERROR;
		out[JSON_RTN_MSG] = "Interface metadata not found";
		return;
	}

	const QString s_lotId = JsonParser::parseJson(*meta, reqJson, "request_body.lot_id").toString();
	if (s_lotId.isEmpty()) {
		out[JSON_RESULT] = STATUS_NG;
		out[JSON_RTN_CODE] = RTN_CODE_REQUIRED_FIELD_MISSING;
		out[JSON_RTN_MSG] = "lot_id is required";
		return;
	}

	// 记录日志
	QString logMsg = QString("收到生产信息下载，lotid : %1")
		.arg(s_lotId);
	cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, logMsg.toLocal8Bit().data());

	//// 保存到数据缓存
	//if (m_data_cache && m_data_cache->isInitialized()) {
	//    QVariantMap dataToSave;
	//    dataToSave["product_id"] = productId;
	//    dataToSave["recipe_id"] = recipeId;
	//    dataToSave["transaction_id"] = transactionId;
	//    dataToSave["timestamp"] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
	//    dataToSave["status"] = "pending";
	//    
	//    // 提取生产参数（如果有）
	//    QVariant productionParams = JsonParser::parseJson(*meta, reqJson, "body.production_params");
	//    if (productionParams.isValid()) {
	//        dataToSave["production_params"] = productionParams;
	//    }
	//    
	//    // 提取工艺参数（如果有）
	//    QVariant processParams = JsonParser::parseJson(*meta, reqJson, "body.process_params");
	//    if (processParams.isValid()) {
	//        dataToSave["process_params"] = processParams;
	//    }
	//    
	//    // 使用产品 ID 或配方 ID 作为键
	//    QString keyValue = !productId.isEmpty() ? productId : recipeId;
	//    QString saveKey = QString("%1.%2").arg(functionName, keyValue);
	//    if (m_data_cache->saveData(saveKey, dataToSave)) {
	//        QString saveMsg = QString("生产信息已保存到缓存: %1").arg(saveKey);
	//        cvm::cvmLog::getInstance()->log(LOG_CATEGORY_SYSTEM, cvm::LogLevel::info, saveMsg.toLocal8Bit().data());
	//    }
	//}

	// 更新全局变量
	//if (!recipeId.isEmpty()) {
	//    setGlobal(FIELD_RECIPE_NAME, recipeId);
	//}
	//if (!productId.isEmpty()) {
	//    setGlobal("current_product_id", productId);
	//}

	// 广播消息到其他模块
	QVariantMap broadcastData;
	broadcastData = message;
	broadcastData["topic"] = INTERFACE_UPDATE_PROCESS_DATA;
	emit sigMessage(broadcastData);

	if (message.contains(FIELD_LOT_QTY))
	{
		m_lotQty = message[FIELD_LOT_QTY].toInt();
	}

	// 返回成功响应
	out["lotId"] = s_lotId;

	out["status"] = "received";

	//离线测试代码
	emit sigTestProcess(1, message);
}

/**
 * @brief 离线测试流程模拟处理函数
 * @param step   流程步骤标志：
 *               1 = 生产信息下载后，进入 WaitStart；
 *               2 = 批次命令下载后，根据 port_command 模拟 Start/Cancel 流程。
 * @param result 对应步骤传入的参数：
 *               - step==1 时为生产信息映射结果（包含 panel 参数等）；
 *               - step==2 时为批次命令映射结果（包含 port_command 等）。
 */
void EapManager::handleTestProcess(int step, QVariantMap result)
{
	_sleep(2000);
	static QVariantMap s_tempProductionInfo; 
	if (step == 1)
	{
		QVariantMap map = creatMapParams(TOPIC_CARRIER_STATUS_REPORT);
		map[FIELD_CARRIER_STATUS] = "WaitStart";
		post(TOPIC_CARRIER_STATUS_REPORT, map);
		s_tempProductionInfo = result;
	}

	else if (step == 2)
	{
		if (result.contains("port_command") && result["port_command"] == "Start")
		{
			QVariantMap map = creatMapParams(TOPIC_CARRIER_STATUS_REPORT);
			map[FIELD_CARRIER_STATUS] = "WaitProc";
			post(TOPIC_CARRIER_STATUS_REPORT, map);

			map = creatMapParams(TOPIC_CARRIER_STATUS_REPORT);
			map[FIELD_CARRIER_STATUS] = "Process";
			post(TOPIC_CARRIER_STATUS_REPORT, map);


			// 运行
			//m_deviceStatus = "run";
			QVariantMap statusMap = creatMapParams(INTERFACE_EQUIPMENT_STATUS);
			statusMap[INTERFACE_EQUIPMENT_STATUS] = m_mapParams[INTERFACE_EQUIPMENT_STATUS].value("run");
			post(INTERFACE_EQUIPMENT_STATUS, statusMap);


			//_sleep(2000);
			QString startTime = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
			for (int i = 0; i < m_lotQty; i++)
			{
				map.clear();
				map["job_count"] = m_lotQty - i - 1;
				map["work_order"] = m_wordOrder;
				post(INTERFACE_JOBCOUNTREPORT, map);

				map.clear();
				map["datetime"] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
				post(INTERFACE_UPLOAD_PANEL_DATA, map);
			}
			QString endTime = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);

			map.clear();
			map[FIELD_USER_ID] = m_userInfo.userId;
			map["dept_id"] = m_userInfo.dept_id;
			map["shift_id"] = m_userInfo.shift_id;
			map["start_dt"] = startTime;
			map["end_dt"] = endTime;
			map["work_order"] = m_wordOrder;
			map["output_qty"] = m_lotQty;
			map["lot_qty"] = m_lotQty;
			map["wip_mode"] = 1;
			map["panel_length"] = s_tempProductionInfo["panel_length"];
			map["panel_width"] = s_tempProductionInfo["panel_width"];
			map["panel_thickness"] = s_tempProductionInfo["panel_thickness"];
			map["file_drill_c"] = s_tempProductionInfo["file_drill_c"];
			map["file_drill_s"] = s_tempProductionInfo["file_drill_s"];
			map["datetime"] = QDateTime::currentDateTime().toString(DEFAULT_DATETIME_FORMAT);
			post(INTERFACE_WIPTRACKINGREPORT, map);



			map = creatMapParams(TOPIC_CARRIER_STATUS_REPORT);
			map[FIELD_CARRIER_STATUS] = "ProcessEnd";
			post(TOPIC_CARRIER_STATUS_REPORT, map);


		    statusMap = creatMapParams(INTERFACE_EQUIPMENT_STATUS);
			statusMap[INTERFACE_EQUIPMENT_STATUS] = m_mapParams[INTERFACE_EQUIPMENT_STATUS].value("Idle");
			post(INTERFACE_EQUIPMENT_STATUS, statusMap);
		}
		else
		{
			QVariantMap map = creatMapParams(TOPIC_CARRIER_STATUS_REPORT);
			map[FIELD_CARRIER_STATUS] = "CancelBC";
			post(TOPIC_CARRIER_STATUS_REPORT, map);

			
		}
	}
	
}

