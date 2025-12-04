#pragma once

/**
 * @file EapManagerConstants.h
 * @brief EapManager 模块的常量定义
 * 
 * 本文件包含 EapManager 中使用的所有魔法字符串常量，
 * 便于维护和避免拼写错误。
 */

namespace EapConstants {

    // ============================================================================
    // Topic 名称 - 用于消息订阅和路由
    // ============================================================================
    
    /** @brief 在线状态变化主题 - 设备上线/离线时触发 */
    constexpr const char* TOPIC_ONLINE_STATUS_CHANGE = "online_status_change";
    
    /** @brief 设备状态变化主题 - 设备状态改变时触发（运行/停止/急停等） */
    constexpr const char* TOPIC_EQP_STATUS_CHANGED = "eqp_status_changed";
    
    /** @brief 下载工艺数据主题 - 从 MES 下载工艺参数 */
    constexpr const char* TOPIC_DOWNLOAD_PROCESS_DATA = "download_process_data";
    
    /** @brief 上传工艺数据主题 - 向 MES 上传工艺数据 */
    constexpr const char* TOPIC_UPLOAD_PROCESS_DATA = "upload_process_data";
    
    /** @brief 上传面板数据主题 - 向 MES 上传面板信息 */
    constexpr const char* TOPIC_UPLOAD_PANEL_DATA = "upload_panel_data";
    
    /** @brief 告警主题 - 设备告警或警告时触发 */
    constexpr const char* TOPIC_ALARM_WARNING = "alarm_warning";
    
    /** @brief 用户等级变化主题 - 用户登录/登出时触发 */
    constexpr const char* TOPIC_USER_LEVEL_CHANGED = "userLevelChanged";

	constexpr const char* TOPIC_UTILITY_REPORT = "UtilityReport";

	/** @brief 载具状态上报 */
	constexpr const char* TOPIC_CARRIER_STATUS_REPORT = "CarrierStatusReport";

    // ============================================================================
    // Interface 接口名称 - MES 接口标识
    // ============================================================================
    
    /** @brief 心跳接口 - 定期向 MES 发送心跳保持连接 */
    constexpr const char* INTERFACE_HEARTBEAT = "heartbeat";

    // 发送 模式切换接口, 在线离线切换通知
    constexpr const char* INTERFACE_CIMMOE_CHANGE_REPORT = "CIMModeChangeReport";

    
    /** @brief 设备信息接口 - 上报设备基本信息 */
    constexpr const char* INTERFACE_EQUIPMENT_INFORMATION = "equipment_information";
    
    /** @brief 设备状态接口 - 上报设备运行状态 */
    constexpr const char* INTERFACE_EQUIPMENT_STATUS = "equipment_status";
    
    /** @brief 初始数据接口 - 上报初始化数据 */
    constexpr const char* INTERFACE_INITIAL_DATA = "initial_data";
    
    /** @brief 告警接口 - 上报告警和警告信息 */
    constexpr const char* INTERFACE_ALARM_WARNING = "alarm_warning";
    
    /** @brief 上传操作员状态接口 - 上报操作员登录/登出状态  人员登入设备管控: 上工 ，下工 */
    constexpr const char* INTERFACE_USERVERIFY = "user_verify";
    
    /** @brief 下载工艺数据接口 - 从 MES 下载工艺参数 */
    constexpr const char* INTERFACE_DOWNLOAD_PROCESS_DATA = "download_process_data";
    
    /** @brief 上传工艺数据接口 - 向 MES 上传工艺数据 */
    constexpr const char* INTERFACE_UPLOAD_PROCESS_DATA = "upload_process_data";
    
    /** @brief 上传面板数据接口 - 向 MES 上传面板信息 */
    constexpr const char* INTERFACE_UPLOAD_PANEL_DATA = "upload_panel_data";
    
    /** @brief 更新工艺数据接口 - 更新工艺参数 */
    constexpr const char* INTERFACE_UPDATE_PROCESS_DATA = "update_process_data";
    
    /** @brief 时间校准接口 - MES 下发时间校准命令 */
    constexpr const char* INTERFACE_DATE_TIME_CALIBRATION = "DateTimeCalibration"; 
    
    /** @brief CIM 消息接口 - MES 下发 CIM 消息（需要弹窗确认） */
    constexpr const char* INTERFACE_CIM_MESSAGE = "CimMessage";


    /** @brief 你是否在线*/
    constexpr const char* INTERFACE_ARE_YOU_THERE = "AreYouThere";

    constexpr const char* INTERFACE_CIMMODE_CHANGE_COMMAND = "CIMModeChangeCommand";
    constexpr const char* INTERFACE_LOTCOMMAND_DOWNLOAD = "LotCommandDownload";
    constexpr const char* INTERFACE_PRODUCTIONINFODOWNLOAD = "ProductionInfoDownload";
    constexpr const char* INTERFACE_JOBCOUNTREPORT = "JobCountReport";
    constexpr const char* INTERFACE_WIPTRACKINGREPORT = "WIPTrackingReport";

    
    /** @brief 在线状态设置接口 - MES 设置设备在线状态 */
    constexpr const char* INTERFACE_ONLINE_STATUS_SET = "online_status_set";
    
    /** @brief 时间配置接口 - 配置时间相关参数 */
    constexpr const char* INTERFACE_TIME_CONFIG = "time_config";



    /**  时间校准
     * */

    constexpr const char* INTERFACE_DATETIME_CALIBRATION = "datetimeCalibration";

    // ============================================================================
    // 消息字段名称 - QVariantMap 中的键名
    // ============================================================================
    
    /** @brief 状态字段 - 通用状态标识 */
    constexpr const char* FIELD_STATUS = "status";

    /** @brief 载具状态 */
    constexpr const char* FIELD_CARRIER_STATUS = "carrier_status";
    
    /** @brief 设备 ID 字段 */
    constexpr const char* FIELD_DEVICE_ID = "device_id";
    
    /** @brief 设备 IP 字段 */
    constexpr const char* FIELD_DEVICE_IP = "device_ip";
    
    /** @brief 告警代码字段 */
    constexpr const char* FIELD_ALARM_CODE = "alarm_code";
    
    /** @brief 告警代码字段（简称） */
    constexpr const char* FIELD_CODE = "code";
    
    /** @brief 告警状态字段 */
    constexpr const char* FIELD_ALARM_STATUS = "alarm_status";
    
    /** @brief 告警等级字段 */
    constexpr const char* FIELD_ALARM_LEVEL = "alarm_level";
    
    /** @brief 告警内容字段 */
    constexpr const char* FIELD_ALARM_CONTENT = "alarm_content";
    
    /** @brief 告警文本字段 */
    constexpr const char* FIELD_ALARM_TEXT = "alarm_text";
    
    /** @brief 用户名字段 */
    constexpr const char* FIELD_USER_NAME = "user_name";

    constexpr const char* FIELD_USER_ID = "user_id"; 
    
    /** @brief 用户等级字段 */
    constexpr const char* FIELD_USER_LEVEL = "user_level";
    
    /** @brief 工艺步骤字段 */
    constexpr const char* FIELD_PROCESS_STEP = "process_step";
    
    /** @brief 手动工艺步骤字段 */
    constexpr const char* FIELD_PROCESS_STEP_MANUAL = "process_step_manual";

	/** @brief 工单号（简称） */
	constexpr const char* FIELD_WORK_ORDER = "work_order";

	/** @brief 批次板书 */
	constexpr const char* FIELD_LOT_QTY = "lot_qty";
    
    /** @brief 手动工艺字段（简称） */
    constexpr const char* FIELD_PROCESS_MANUAL = "process_manual";
    
    /** @brief 日期时间字段 */
    constexpr const char* FIELD_DATETIME = "datetime";
    
    /** @brief 当前时间字段 */
    constexpr const char* FIELD_CURRENT_TIME = "current_time";
    
    /** @brief 时间字段 */
    constexpr const char* FIELD_TIME = "time";
    
    /** @brief 当前型号字段 */
    constexpr const char* FIELD_CURRENT_MODEL = "current_model";
    
    /** @brief 配方名称字段 */
    constexpr const char* FIELD_RECIPE_NAME = "recipe_name";
    
    /** @brief 在线模式字段 */
    constexpr const char* FIELD_ONLINE_MODE = "online_mode";

    // 用户状态
    constexpr const char* FIELD_USER_STATE = "user_state";

    constexpr const char* FIELD_USER_LOGIN = "login";
    constexpr const char* FIELD_USER_LOGOUT = "logout"; 
    constexpr const char* FIELD_USER_PASSWORD = "user_password";

    
    /** @brief 设备状态码字段 */
    constexpr const char* FIELD_EQP_STATUS_CODE = "eqp_status_code";
    
    /** @brief 登录状态字段 */
    constexpr const char* FIELD_LOGIN_STATE = "login_state";
    
    /** @brief 操作员在线字段 */
    constexpr const char* FIELD_OP_ONLINE = "op_online";
    
    /** @brief 操作员登录状态字段 */
    constexpr const char* FIELD_OP_LOGIN_STATE = "op_login_state";
    
    /** @brief IP 地址字段 */
    constexpr const char* FIELD_IP = "ip";
    
    /** @brief 当前时间字段（now） */
    constexpr const char* FIELD_NOW = "now";
    
    /** @brief 令牌字段 */
    constexpr const char* FIELD_TOKEN = "token";
    
    /** @brief 成功标志字段 */
    constexpr const char* FIELD_IS_SUCCESS = "is_success";
    
    /** @brief 消息字段 */
    constexpr const char* FIELD_MESSAGE = "message";
    
    /** @brief 重试标志字段 - 用于判断是否需要重试 */
    constexpr const char* FIELD_NEED_RETRY = "needRetry";

    /** @brief 内部字段 - CIMMESSAGE 消息内容 */
    constexpr const char* FIELD_CIMMessage = "cim_msg";
    /** @brief 内部字段 - CIMMESSAGE 消息显示代码 */
    constexpr const char* FIELD_ScreenCode = "screen_code";
    /** @brief 内部字段 - CIMMESSAGE 消息显示时长 */
    constexpr const char* FIELD_INTERNAL_TIME = "interval_second_time";
    /** @brief 内部字段 - CIMMESSAGE 显示控制 */
    constexpr const char* FIELD_SCREEN_CONTROL = "screen_control";

    //
    constexpr const char* FIELD_TRX_ID = "trx_id"; 

    // ============================================================================
    // JSON 结构字段 - JSON 对象中的键名
    // ============================================================================
    
    /** @brief JSON header 字段 */
    constexpr const char* JSON_HEADER = "header";
    
    /** @brief JSON body 字段 */
    constexpr const char* JSON_BODY = "body";
    
    /** @brief 结果字段 */
    constexpr const char* JSON_RESULT = "result";
    
    /** @brief 返回码字段 */
    constexpr const char* JSON_RTN_CODE = "rtn_code";
    
    /** @brief 返回消息字段 */
    constexpr const char* JSON_RTN_MSG = "rtn_msg";
    
    /** @brief 参数字段 */
    constexpr const char* JSON_PARAMS = "params";
    
    /** @brief 格式字段 */
    constexpr const char* JSON_FORMAT = "format";
    
    /** @brief 路由字段 */
    constexpr const char* JSON_ROUTES = "routes";
    
    /** @brief 队列字段 */
    constexpr const char* JSON_QUEUE = "queue";

    // ============================================================================
    // 状态值 - 设备和告警状态
    // ============================================================================
    
    /** @brief 停止状态 */
    constexpr const char* STATUS_STOP = "stop";
    
    /** @brief 急停状态 */
    constexpr const char* STATUS_EMG = "emg";
    
    /** @brief 在线状态 */
    constexpr const char* STATUS_ONLINE = "online";
    
    /** @brief 离线状态 */
    constexpr const char* STATUS_OFFLINE = "offline";
    
    /** @brief 告警发生状态 */
    constexpr const char* STATUS_OCCURRENCE = "occurrence";
    
    /** @brief 告警清除状态 */
    constexpr const char* STATUS_CLEAR = "clear";
    
    /** @brief NG 状态（不合格） */
    constexpr const char* STATUS_NG = "NG";

    // ============================================================================
    // 接口方向 - Interface direction
    // ============================================================================
    
    /** @brief 推送方向 - 主动向 MES 推送数据 */
    constexpr const char* DIRECTION_PUSH = "push";

    // ============================================================================
    // 配置相关 - INI 文件配置项
    // ============================================================================
    
    /** @brief INI 配置组：机器信息 */
    constexpr const char* INI_GROUP_MACHINE_INFO = "machineInfo";
    
    /** @brief INI 配置组：文件路径 */
    constexpr const char* INI_GROUP_FILE_PATH = "filePath";
    
    /** @brief INI 配置组：接口配置 */
    constexpr const char* INI_GROUP_INTERFACE = "interface";
    
    /** @brief INI 配置组：其他配置 */
    constexpr const char* INI_GROUP_OTHER = "other";
    
    /** @brief INI 配置项：设备 ID */
    constexpr const char* INI_KEY_DEVICE_ID = "deviceId";
    
    /** @brief INI 配置项：设备 IP */
    constexpr const char* INI_KEY_DEVICE_IP = "deviceIp";

	/** @brief INI 配置项：设备地址 */
	constexpr const char* INI_KEY_DEVICE_PLACE = "devicePlace";
    
    /** @brief INI 配置项：接口文件路径 */
    constexpr const char* INI_KEY_INTERFACE = "interface";

    /** @brief INI 配置项：EvoElope 文件路径 */
    constexpr const char* INI_KEY_HEADERPARAM_EVOELOPE_FILEPATH = "headerParamEvelopePath";

    /** @brief INI 配置项：payload 文件路径 */
    constexpr const char* INI_KEY_PAYLOAD_PARAM_FILEPATH = "payloadParamFilePath";

    /** @brief INI 配置项：服务接口文件路径 */
    constexpr const char* INI_KEY_SERVICE_INTERFACE_FILEPATH = "serviceInterfaceFilePath";
    
    /** @brief INI 配置项：测试数据文件路径 */
    constexpr const char* INI_KEY_TEST_DATA = "testData";
    
    /** @brief INI 配置项：心跳时间间隔 */
    constexpr const char* INI_KEY_HEART_BEAT_TIME = "heartBeatTime";
    
    /** @brief INI 配置项：时间同步间隔 */
    constexpr const char* INI_KEY_SYN_TIME_TIME = "synTimeTime";
    
    /** @brief INI 配置项：离线缓存开关 */
    constexpr const char* INI_KEY_OFFLINE_CACHE = "offlineCache";
    
    /** @brief INI 配置项：令牌 */
    constexpr const char* INI_KEY_TOKEN = "token";
    
    /** @brief INI 配置项：工艺 */
    constexpr const char* INI_KEY_PROCESS = "process";
    
    /** @brief INI 配置项：手动工艺 */
    constexpr const char* INI_KEY_PROCESS_MANUAL = "processManual";

    // ============================================================================
    // 全局变量名称 - Context 中的全局变量
    // ============================================================================
    
    /** @brief 全局变量：在线状态 */
    constexpr const char* GLOBAL_ONLINE_STATUS = "online_Status";

    // ============================================================================
    // 特殊值 - 特殊含义的字符串
    // ============================================================================
    
    /** @brief 自动值 - 表示自动填充当前时间 */
    constexpr const char* VALUE_AUTO = "auto";
    
    /** @brief 空值 */
    constexpr const char* VALUE_NULL = "null";
    
    /** @brief 空字符串（用于默认值） */
    constexpr const char* VALUE_EMPTY = "";

    // ============================================================================
    // 告警代码 - 预定义的告警类型
    // ============================================================================
    
    /** @brief 告警：安全门打开 */
    constexpr const char* ALARM_SAFETY_DOOR_OPEN = "Alarm_SafetyDoorOpen";
    
    /** @brief 告警：安全光栅触发 */
    constexpr const char* ALARM_SAFETY_GRATING_TRIGGERED = "Alarm_SafetyGratingTriggered";

    // ============================================================================
    // 告警等级 - 告警严重程度
    // ============================================================================
    
    /** @brief 告警等级：告警 */
    constexpr const char* ALARM_LEVEL_ALARM = "alarm";
    
    /** @brief 告警等级：警告 */
    constexpr const char* ALARM_LEVEL_WARNING = "warning";

    // ============================================================================
    // 标准返回码 - WebService 通用返回码定义
    // ============================================================================
    
    /** @brief 返回码：成功 */
    constexpr const char* RTN_CODE_SUCCESS = "0000";
    
    /** @brief 返回码：参数错误 */
    constexpr const char* RTN_CODE_PARAM_ERROR = "1001";
    
    /** @brief 返回码：数据不存在 */
    constexpr const char* RTN_CODE_DATA_NOT_FOUND = "1002";
    
    /** @brief 返回码：数据格式错误 */
    constexpr const char* RTN_CODE_DATA_FORMAT_ERROR = "1003";
    
    /** @brief 返回码：必填字段缺失 */
    constexpr const char* RTN_CODE_REQUIRED_FIELD_MISSING = "1004";
    
    /** @brief 返回码：业务逻辑错误 */
    constexpr const char* RTN_CODE_BUSINESS_ERROR = "2001";
    
    /** @brief 返回码：设备状态异常 */
    constexpr const char* RTN_CODE_DEVICE_STATUS_ERROR = "2002";
    
    /** @brief 返回码：操作不允许 */
    constexpr const char* RTN_CODE_OPERATION_NOT_ALLOWED = "2003";
    
    /** @brief 返回码：系统内部错误 */
    constexpr const char* RTN_CODE_INTERNAL_ERROR = "5000";
    
    /** @brief 返回码：数据库错误 */
    constexpr const char* RTN_CODE_DATABASE_ERROR = "5001";
    
    /** @brief 返回码：网络错误 */
    constexpr const char* RTN_CODE_NETWORK_ERROR = "5002";
    
    /** @brief 返回码：超时 */
    constexpr const char* RTN_CODE_TIMEOUT = "5003";
    
    /** @brief 返回码：接口未找到 */
    constexpr const char* RTN_CODE_INTERFACE_NOT_FOUND = "EIC404";

    // ============================================================================
    // 标准返回消息 - WebService 通用返回消息定义
    // ============================================================================
    
    /** @brief 返回消息：成功 */
    constexpr const char* RTN_MSG_SUCCESS = "Success";
    
    /** @brief 返回消息：参数错误 */
    constexpr const char* RTN_MSG_PARAM_ERROR = "Parameter error";
    
    /** @brief 返回消息：数据不存在 */
    constexpr const char* RTN_MSG_DATA_NOT_FOUND = "Data not found";
    
    /** @brief 返回消息：数据格式错误 */
    constexpr const char* RTN_MSG_DATA_FORMAT_ERROR = "Data format error";
    
    /** @brief 返回消息：必填字段缺失 */
    constexpr const char* RTN_MSG_REQUIRED_FIELD_MISSING = "Required field missing";
    
    /** @brief 返回消息：业务逻辑错误 */
    constexpr const char* RTN_MSG_BUSINESS_ERROR = "Business logic error";
    
    /** @brief 返回消息：设备状态异常 */
    constexpr const char* RTN_MSG_DEVICE_STATUS_ERROR = "Device status error";
    
    /** @brief 返回消息：操作不允许 */
    constexpr const char* RTN_MSG_OPERATION_NOT_ALLOWED = "Operation not allowed";
    
    /** @brief 返回消息：系统内部错误 */
    constexpr const char* RTN_MSG_INTERNAL_ERROR = "Internal error";
    
    /** @brief 返回消息：数据库错误 */
    constexpr const char* RTN_MSG_DATABASE_ERROR = "Database error";
    
    /** @brief 返回消息：网络错误 */
    constexpr const char* RTN_MSG_NETWORK_ERROR = "Network error";
    
    /** @brief 返回消息：超时 */
    constexpr const char* RTN_MSG_TIMEOUT = "Timeout";
    
    /** @brief 返回消息：接口未找到 */
    constexpr const char* RTN_MSG_INTERFACE_NOT_FOUND = "Interface not found";

    // ============================================================================
    // 错误码 - 自定义错误代码（保留向后兼容）
    // ============================================================================
    
    /** @brief 错误码：接口未找到（已弃用，使用 RTN_CODE_INTERFACE_NOT_FOUND） */
    constexpr const char* ERROR_CODE_EIC404 = "EIC404";

    // ============================================================================
    // 日志相关 - 日志系统使用的常量
    // ============================================================================
    
    /** @brief 日志类别：系统日志 */
    constexpr const char* LOG_CATEGORY_SYSTEM = "system";

    // ============================================================================
    // 其他常量
    // ============================================================================
    
    /** @brief 默认时间格式 */
    constexpr const char* DEFAULT_DATETIME_FORMAT = "yyyy-MM-dd HH:mm:ss";
    
    /** @brief 清洁产线 */
    constexpr const char* LINE_CLEANING = "Line_Cleaning";

} // namespace EapConstants
