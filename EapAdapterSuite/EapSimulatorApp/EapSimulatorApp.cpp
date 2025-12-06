#include "EapSimulatorApp.h"


#include <QTabWidget>
#include <QSplitter>
#include <QGroupBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QPlainTextEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QScrollArea>
#include <QFileDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>
#include <QLabel>
#include <QCheckBox>
#include <QApplication>

#include "VendorConfigLoader.h"
#include "JsonBuilder.h"
#include "EAPMessageLogWidget.h"
#include "EAPMessageLogger.h"
#include "ParameterHelper.h"

#pragma execution_character_set("utf-8")

MainWindow::MainWindow(QWidget* parent)
    : QWidget(parent)
    , uploadMgr(new EAPInterfaceManager(this))
    , webService(new EAPWebService(this)) {

    setupUI();

    // 日志连接
    connectManagerLogs(uploadMgr, logUpload, "[UPLOAD]"); // 绑定 EAPInterfaceManager 的日志信号到指定日志区域
    connectWebServiceLogs(); // 绑定 EAPWebService 的服务器事件日志到 logWeb

    uploadMgr->loadEnvelopePolicy("./config/payload_policy.json"); // (从 JSON 策略文件加载 EAP 报文外壳配置)
    webService->loadEnvelopePolicy("./config/payload_policy.json"); // (从策略文件加载 Envelope 配置)
    uploadMgr->loadHeaderParams("./config/config_header _params.json"); // (从 JSON 配置文件加载 EAP 请求 header 参数模板)

    //1 . editUploadCfg
    editUploadCfg->setText("./config/config_interfaces.merged.json"); // 上传"加载配置"
    editUploadParams->setText("./config/config_interfaces.merged.defaults.json"); // 上传"加载参数"

    editWebCfg->setText("./config/config_interfaces.downstream.merged.json"); // WebService "加载配置"
    doLoadUploadConfig(editUploadCfg->text()); // 从指定配置文件加载上传接口配置并刷新按钮区
    doLoadUploadParams(editUploadParams->text()); // 从指定文件加载上传参数并记录加载结果
    doLoadWebConfig(editWebCfg->text()); // 从指定配置文件加载 WebService 接口配置并初始化相关选项与默认响应逻辑

    createLogger(); // 创建并初始化全局消息日志记录器
    createLoggerWidget(); // 创建消息日志查看窗口并添加到主界面标签页
    doTestJson(); // 测试 JsonUpdateRfidKeyFromVariant 对 JSON 的更新行为

    m_data_cache = new EAPDataCache(this);
    m_data_cache->initialize("./dataCache"); //设置缓存路径
    webService->setDataCache(m_data_cache); // 为 EAPWebService 设置数据缓存对象
    uploadMgr->setDataCache(m_data_cache); // 设置数据缓存对象

  /*  m_data_cache_widget = new EAPDataCacheWidget(this);
    m_data_cache_widget->setDataCache(m_data_cache);
    tabs->addTab(m_data_cache_widget, "缓存");*/

    // 轻量美化--给整个 UI 套了一层统一的“皮肤”
    setStyleSheet(R"(
        QWidget { font-family: "Microsoft YaHei", "Segoe UI", sans-serif; }
        QGroupBox { font-weight: bold; border: 1px solid #bbb; border-radius: 6px; margin-top: 8px; }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QPushButton { padding: 6px 10px; }
        QLineEdit { padding: 4px 6px; }
        QPlainTextEdit { background: #111; color: #e6e6e6; font-family: Consolas, "Courier New", monospace; }
    )");
}

MainWindow::~MainWindow() {
    if (webService) webService->stop();
}

/**
 * @brief 初始化主窗口界面
 *
 * 创建主布局和标签页控件，构建并添加“上传(客户端)”与 “WebService(服务端)”两个功能页，
 * 并设置主窗口的最小尺寸。
 */
void MainWindow::setupUI() {
    auto* mainLayout = new QVBoxLayout(this);
    tabs = new QTabWidget(this);

    tabUpload = createUploadTab();
    tabWeb = createWebServiceTab();

    tabs->addTab(tabUpload, tr("上传(客户端)"));
    tabs->addTab(tabWeb, tr("WebService(服务端)"));

    mainLayout->addWidget(tabs);
    setMinimumSize(1000, 720);
}

/**
 * @brief 构建“上传(客户端)”标签页界面
 */
QWidget* MainWindow::createUploadTab() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);

    // 顶部控制
    auto* ctrl = new QGroupBox(tr("上传接口配置与参数"), page);
    auto* g = new QGridLayout(ctrl);

    editUploadCfg = new QLineEdit(ctrl);
    editUploadCfg->setPlaceholderText(tr("选择上传接口配置 JSON"));
    btnUploadCfg = new QPushButton(tr("加载配置"), ctrl);
    connect(btnUploadCfg, &QPushButton::clicked, this, &MainWindow::onLoadUploadConfig); // 按钮“加载配置”，点击后调用 onLoadUploadConfig()

    editUploadParams = new QLineEdit(ctrl);
    editUploadParams->setPlaceholderText(tr("选择上传接口参数 JSON（可选）"));
    btnUploadParams = new QPushButton(tr("加载参数"), ctrl);
    connect(btnUploadParams, &QPushButton::clicked, this, &MainWindow::onLoadUploadParams); // “加载参数”按钮，对应槽函数 onLoadUploadParams()

    g->addWidget(new QLabel(tr("配置文件:")), 0, 0);
    g->addWidget(editUploadCfg, 0, 1);
    g->addWidget(btnUploadCfg, 0, 2);
    g->addWidget(new QLabel(tr("参数文件:")), 1, 0);
    g->addWidget(editUploadParams, 1, 1);
    g->addWidget(btnUploadParams, 1, 2);

    ctrl->setLayout(g);

    // 中部：左按钮区 + 右日志区
    auto* split = new QSplitter(Qt::Horizontal, page);

    // 左侧：按钮滚动区（修复点：scroll 的 widget 就是 uploadBtnsContainer，本容器拥有布局）
    auto* leftPanel = new QWidget(split);
    auto* leftLayout = new QVBoxLayout(leftPanel);

    uploadBtnsContainer = new QWidget(leftPanel); // 真正挂按钮的容器 widget，后续创建按钮往这里进行添加
    uploadBtnsLayout = new QVBoxLayout(uploadBtnsContainer);

    auto* btnsScroll = new QScrollArea(leftPanel);
    btnsScroll->setWidgetResizable(true); // 让容器随滚动区大小伸缩
    btnsScroll->setWidget(uploadBtnsContainer);

    auto* tips = new QLabel(tr("仅展示 direction=push 且 enabled=true 的接口"), leftPanel);
    leftLayout->addWidget(tips);
    leftLayout->addWidget(btnsScroll);
    leftPanel->setLayout(leftLayout);

    // 右侧：日志
    auto* rightPanel = new QWidget(split);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    logUpload = new QPlainTextEdit(rightPanel); // 上传日志输出框（只读）
    logUpload->setReadOnly(true);
    rightLayout->addWidget(new QLabel(tr("日志输出")));
    rightLayout->addWidget(logUpload);
    rightPanel->setLayout(rightLayout);

    split->addWidget(leftPanel);
    split->addWidget(rightPanel);
    split->setStretchFactor(0, 4);
    split->setStretchFactor(1, 6);

    v->addWidget(ctrl);
    v->addWidget(split);
    return page;
}

/**
 * @brief 构建“WebService(服务端)”标签页界面
 */
QWidget* MainWindow::createWebServiceTab() {
    auto* page = new QWidget(this);
    auto* v = new QVBoxLayout(page);

    // 顶部控制
    auto* ctrl = new QGroupBox(tr("WebService 服务端"), page);
    auto* g = new QGridLayout(ctrl);

    editWebCfg = new QLineEdit(ctrl);
    editWebCfg->setPlaceholderText(tr("选择 WebService 配置 JSON（用于路由/映射/白名单）"));
    btnWebCfg = new QPushButton(tr("加载配置"), ctrl);
    connect(btnWebCfg, &QPushButton::clicked, this, &MainWindow::onLoadWebConfig); // 加载配置按钮

    editWebPort = new QLineEdit(ctrl);
    editWebPort->setPlaceholderText(tr("端口，默认 8026"));
    editWebPort->setText("8026");
    connect(editWebPort, &QLineEdit::editingFinished, this, &MainWindow::onPortChanged); // "监听端口" 编辑结束时调用 onPortChanged()

    chkCaseInsensitive = new QCheckBox(tr("function_name 大小写不敏感"), ctrl);
    chkCaseInsensitive->setChecked(true);
    chkStrictHeadMatch = new QCheckBox(tr("严格校验 request_head.function_name 与 URL"), ctrl);
    chkStrictHeadMatch->setChecked(true);
    chkOnlyPush = new QCheckBox(tr("仅允许 direction=push 路由"), ctrl);
    chkOnlyPush->setChecked(false);

    editResponderTimeout = new QLineEdit(ctrl);
    editResponderTimeout->setPlaceholderText(tr("回调超时(ms)，0=不限"));
    editResponderTimeout->setText("0");
    connect(editResponderTimeout, &QLineEdit::editingFinished, this, &MainWindow::onResponderTimeoutChanged); // "回调超时(ms)" 编辑结束时调用 onResponderTimeoutChanged() 

    btnAllowListFromCfg = new QPushButton(tr("白名单取配置Keys"), ctrl);
    connect(btnAllowListFromCfg, &QPushButton::clicked, this, &MainWindow::onRefreshAllowListFromConfig); // "白名单取配置Keys"按钮

    btnWebStart = new QPushButton(tr("启动服务"), ctrl);
    btnWebStop = new QPushButton(tr("停止服务"), ctrl);
    connect(btnWebStart, &QPushButton::clicked, this, &MainWindow::onStartWebService); // "启动服务"按钮
    connect(btnWebStop, &QPushButton::clicked, this, &MainWindow::onStopWebService); // "停止服务"按钮

    int r = 0;
    g->addWidget(new QLabel(tr("配置文件:")), r, 0); g->addWidget(editWebCfg, r, 1); g->addWidget(btnWebCfg, r, 2); ++r;
    g->addWidget(new QLabel(tr("监听端口:")), r, 0); g->addWidget(editWebPort, r, 1); g->addWidget(btnWebStart, r, 2); ++r;
    g->addWidget(new QLabel(tr("回调超时(ms):")), r, 0); g->addWidget(editResponderTimeout, r, 1); g->addWidget(btnWebStop, r, 2); ++r;
    g->addWidget(chkCaseInsensitive, r, 0); g->addWidget(chkStrictHeadMatch, r, 1); g->addWidget(chkOnlyPush, r, 2); ++r;
    g->addWidget(btnAllowListFromCfg, r, 0, 1, 3);

    ctrl->setLayout(g);

    // 白名单 + 日志
    auto* split = new QSplitter(Qt::Horizontal, page);

    auto* leftPanel = new QWidget(split);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    auto* gbList = new QGroupBox(tr("允许的 function_name 白名单"), leftPanel);
    auto* vb = new QVBoxLayout(gbList);
    listAllowed = new QListWidget(gbList);
    vb->addWidget(listAllowed);
    gbList->setLayout(vb);
    leftLayout->addWidget(gbList);
    leftPanel->setLayout(leftLayout);

    auto* rightPanel = new QWidget(split);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    logWeb = new QPlainTextEdit(rightPanel); // 服务端日志输出（只读）
    logWeb->setReadOnly(true);
    rightLayout->addWidget(new QLabel(tr("服务端日志")));
    rightLayout->addWidget(logWeb);
    rightPanel->setLayout(rightLayout);

    split->addWidget(leftPanel);
    split->addWidget(rightPanel);
    split->setStretchFactor(0, 3);
    split->setStretchFactor(1, 7);

    v->addWidget(ctrl);
    v->addWidget(split);
    return page;
}

/**
 * @brief 向指定日志区域追加一行带时间戳的日志
 * @param area  要写入的日志显示控件（QPlainTextEdit）
 * @param line  要输出的日志文本内容（不包含时间戳部分）
 */
void MainWindow::appendLog(QPlainTextEdit* area, const QString& line) {
    if (!area) return;
    area->appendPlainText(QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("HH:mm:ss.zzz"), line));
}

/**
 * @brief 向指定日志区域追加一段带标题的格式化 JSON 文本
 */
void MainWindow::appendJson(QPlainTextEdit* area, const QString& title, const QJsonObject& json) {
    if (!area) return;
    QJsonDocument doc(json);
    appendLog(area, QString("%1:\n%2")
        .arg(title, QString::fromUtf8(doc.toJson(QJsonDocument::Indented))));
}

// -------------------- 上传客户端 --------------------

/**
 * @brief 从指定配置文件加载上传接口配置并刷新按钮区
 * @param file 上传接口配置 JSON 文件路径
 */
void MainWindow::doLoadUploadConfig(const QString& file)
{
    if (!uploadMgr->loadInterfaceConfig(file)) { // 从指定 JSON 文件里加载“上传接口配置”
        appendLog(logUpload, tr("配置加载失败: %1").arg(uploadMgr->getLastError()));
        return;
    }
    appendLog(logUpload, tr("配置已加载，共 %1 项，base_url=%2")
        .arg(uploadMgr->interfaceCount()).arg(uploadMgr->getBaseUrl()));

    // 重建按钮（先清空布局）
    qDeleteAll(uploadButtons);
    uploadButtons.clear();
    QLayoutItem* child;
    while ((child = uploadBtnsLayout->takeAt(0)) != nullptr) {
        if (child->widget()) child->widget()->deleteLater();
        delete child;
    }

    // 生成按钮
    for (const QString& key : uploadMgr->getInterfaceKeys()) { // 遍历 uploadMgr->getInterfaceKeys() 中所有接口 key
        EapInterfaceMeta& meta = uploadMgr->getInterface(key);
        if (!meta.enabled || meta.direction.compare("push", Qt::CaseInsensitive) != 0) continue;

        // 为每个符合条件的接口建一个按钮：文本“发送 key”
        auto* btn = new QPushButton(tr("发送 %1").arg(key), uploadBtnsContainer);
        uploadButtons.insert(key, btn);
        uploadBtnsLayout->addWidget(btn);
        connect(btn, &QPushButton::clicked, this, [=]() { onUploadButtonClicked(key); });
    }
    uploadBtnsLayout->addStretch(); // 在末尾加一个伸缩项，让所有按钮靠上排列
}

/**
 * @brief 按钮“加载配置”
 */
void MainWindow::onLoadUploadConfig() {
    const QString file = QFileDialog::getOpenFileName(this, tr("选择上传接口配置 JSON"), QString(), "*.json");
    if (file.isEmpty()) return;
    editUploadCfg->setText(file);
    doLoadUploadConfig(file);
}

/**
 * @brief 从指定文件加载上传参数并记录加载结果
 * @param file 上传接口参数 JSON 文件路径
 */
void MainWindow::doLoadUploadParams(const QString& file)
{
    if (!loadParamsFromFile(file, uploadParams, logUpload)) return;
    appendLog(logUpload, tr("参数文件加载完成，共 %1 项").arg(uploadParams.size()));
}

/**
 * @brief 按钮“加载参数”
 */
void MainWindow::onLoadUploadParams() {
    const QString file = QFileDialog::getOpenFileName(this, tr("选择上传接口参数 JSON"), QString(), "*.json");
    if (file.isEmpty()) return;
    editUploadParams->setText(file);
    doLoadUploadParams(file);
}

/**
 * @brief 槽函数：处理单个上传接口按钮的点击事件
 * @param key 被点击的上传接口标识（接口名称 / key）
 */
void MainWindow::onUploadButtonClicked(const QString& key) {
    if (!uploadMgr->getInterfaceKeys().contains(key)) {
        appendLog(logUpload, tr("接口未找到: %1").arg(key));
        return;
    }
    EapInterfaceMeta& meta = uploadMgr->getInterface(key);
    QVariantMap params = paramsFor(key, uploadParams); // 取出该接口的参数
    applyAutoTimeFields(meta, params); // 自动填充时间类字段
    uploadMgr->post(key, params); // 发起请求
}

// -------------------- WebService 服务端 --------------------

/**
 * @brief 从指定配置文件加载 WebService 接口配置并初始化相关选项与默认响应逻辑
 * @param file WebService 配置 JSON 文件路径
 */
void MainWindow::doLoadWebConfig(const QString& file)
{
    webConfigPath = file;

    if (!webService->loadInterfaceConfig(file)) { //加载 WebService 的接口配置
        appendLog(logWeb, tr("WebService 配置加载失败: %1").arg(webService->lastError()));
        return;
    }
    appendLog(logWeb, tr("WebService 配置已加载: %1").arg(file));

    // 默认白名单 = 配置 keys
    onRefreshAllowListFromConfig();

    // 基本选项--根据三个勾选框，设置 WebService 的行为开关
    webService->setCaseInsensitiveFunctionMatch(chkCaseInsensitive->isChecked());
    webService->setStrictHeadFunctionMatch(chkStrictHeadMatch->isChecked());
    webService->setAllowOnlyPushDirection(chkOnlyPush->isChecked());

    // provider 负责“产生本地业务数据 r（result, rtn_code, 各种字段）”，后面会再用 response_mapping 把这些字段映射到真正的 JSON header/body 中
    EAPWebService::Provider provider = [](const QString& fn, const QJsonObject& reqJson, const QVariantMap& p) -> QVariantMap {

        QVariantMap r = p;
        r["result"] = "OK";

        if (fn == "DateTimeCalibration") {
            r["now"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            r["ip"] = p.value("ip").toString();   
        }
        else if (fn == "ProductionInfoDownload") {
            //r["lot_id"] = p.value("lot_id").toString();
        }
        else if (fn == "UserVerify") {
            const QString uid = p.value("user_id").toString();
            if (uid.isEmpty()) {
                r["result"] = "NG"; r["rtn_code"] = "EIC4201"; r["rtn_msg"] = "user_id empty"; return r;
            }
            r["clock_type"] = p.value("clock_type").toString();
            r["user_id"] = uid;
            r["user_name"] = "Demo User";
            r["dept_id"] = "DEPT-01";
            r["shift_id"] = "DAY";
            r["nick_name"] = "du";
            
        }
        return r;
    };

    // 默认 rawResponder：ACK（OK）
    webService->setRawResponder([=](const QString& fn, const QJsonObject& req, const QVariantMap& p) -> QJsonObject {
        auto headOK = []() { QJsonObject h; h["result"] = "OK"; h["rtn_code"] = ""; h["rtn_msg"] = ""; return h; };
        auto headNG = [](QString c, QString m) { QJsonObject h; h["result"] = "NG"; h["rtn_code"] = c; h["rtn_msg"] = m; return h; };

        //QJsonObject out;
        //if (fn == "DateTimeCalibration") {
        //    QJsonObject body;
        //    body["now"] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
        //    body["ip"] = p.value("ip").toString();
        //    out["body"] = body;            // 头默认 OK
        //    return out;
        //}

        //if (fn == "ProductionInfoDownload") {
        //    QJsonObject body;
        //    body["lot_id"] = p.value("lot_id").toString();  // 回显
        //    out["body"] = body;
        //    return out;
        //}

        //if (fn == "LotCommandDownload") {
        //    const QString cmd = p.value("port_command").toString();
        //    if (cmd != "Start" && cmd != "Cancel" && cmd != "Abort") {
        //        out["header"] = headNG("EIC4001", "invalid port_command");
        //    }
        //    return out; // 空 body → 默认 ACK 或 NG（若上面设置了 header）
        //}

        //if (fn == "CIMModeChangeCommand") {
        //    const QString mode = p.value("cim_mode").toString();
        //    if (mode != "Local" && mode != "Semi-Auto" && mode != "Remote") {
        //        out["header"] = headNG("EIC4101", "unsupported cim_mode");
        //    }
        //    return out;
        //}

        //if (fn == "UserVerify") {
        //    // 外部数据源查询示例（伪代码）
        //    // auto user = hrRepo.getUser(p["user_id"].toString());
        //    // if (!user.ok) { out["header"]=headNG("EIC4201","user not found"); return out; }
        //    QJsonObject body;
        //    body["clock_type"] = p.value("clock_type").toString();
        //    body["user_id"] = p.value("user_id").toString();
        //    body["user_name"] = "Demo User";
        //    body["dept_id"] = "DEPT-01";
        //    body["shift_id"] = "DAY";
        //    body["nick_name"] = "du";
        //    out["body"] = body;
        //    return out;
        //}

        //if (fn == "CIMModeChangeReport" || fn == "CIMMessage") {
        //    return out; // 仅 ACK
        //}

        //return out;

        QJsonObject out;

        // 1) 获取该接口原始 meta
        const EapInterfaceMeta* meta = webService->getMeta(fn);
        if (!meta) {
            QJsonObject h; h["result"] = "NG"; h["rtn_code"] = "EIC404"; h["rtn_msg"] = "meta not found";
            out["header"] = h; return out;
        }
         
        const QVariantMap respLocal = provider(fn,req, p);

        const QString result = respLocal.value("result").toString();
        const QString rtn_code = respLocal.value("rtn_code").toString();
        const QString rtn_msg = respLocal.value("rtn_msg").toString();
        //if (!result.isEmpty() || !rtn_code.isEmpty() || !rtn_msg.isEmpty()) {
        //  /*  QJsonObject h;
        //    if (!result.isEmpty())   h["result"] = result;
        //    if (!rtn_code.isEmpty()) h["rtn_code"] = rtn_code;
        //    if (!rtn_msg.isEmpty())  h["rtn_msg"] = rtn_msg;
        //    out["header"] = h;*/
        //    QJsonObject h;
        //    for (auto it = respLocal.begin(); it != respLocal.end(); ++it)
        //    {
        //        h.insert(it.key(), it.value().toString());
        //    }
        //    out["header"] = h;
        //}
        QJsonObject h;
        for (auto it = meta->responseMap.begin(); it != meta->responseMap.end(); ++it) {
            const QString& jsonPath = it.key();
            const QString& localKey = it.value();
            if (!jsonPath.startsWith("header.")) continue;
           h.insert(localKey, respLocal.value(localKey).toString());
        }
        out["header"] = h;
     
        EapInterfaceMeta meta2 = *meta;
        meta2.enableHeader = false; 
        meta2.enableBody = true;
        meta2.headerMap.clear();
        meta2.bodyMap.clear();

       QVariantMap map =  JsonBuilder::buildMapping(meta->bodyMap,req);
    
        // 用 response_mapping 中的 body.* 条目反向生成 bodyMap：localKey -> "xxx"
        for (auto it = meta->responseMap.begin(); it != meta->responseMap.end(); ++it) {
            const QString& jsonPath = it.key();
            const QString& localKey = it.value();
            if (!jsonPath.startsWith("body.")) continue;
            meta2.bodyMap.insert(localKey, jsonPath.mid(5)); 
        }

        // 生成 {"body": {...}}，取 body 放入 out
        const QJsonObject hb = JsonBuilder::buildPayload(meta2, respLocal);
        const QJsonObject body = hb.value("body").toObject();
        if (!body.isEmpty()) out["body"] = body;

        return out; 
    });
}

/**
 * @brief 测试 JsonUpdateRfidKeyFromVariant 对 JSON 的更新行为
 */
void MainWindow::doTestJson()
{
    // 原始 JSON（来自你的示例）
    const QByteArray jsonText = R"json(
    {
      "carrier_id": "CARR-0001",
      "carrier_seq": "1",
      "port_type": "Load",
      "port_no": "01",
      "prod_mode": "Production",
      "production_mode": "Default",
      "S5": "SM07",
      "S6": "First",
      "S7": "", "S8": "", "S9": "", "S10": "",
      "read_type": "S",
      "batch_no":"LOT-20251025-01",
      "rfid_infos": {
        "rfid": [
          { "lot_id": "LOT-20251025-01" }
        ]
      }
    }
    )json";

    // 解析为 QJsonObject
    QJsonParseError err;
    QJsonDocument doc = QJsonDocument::fromJson(jsonText, &err);
    if (err.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << err.errorString();
        return ;
    }
    QJsonObject obj = doc.object();

    qDebug() << "== 原始 JSON ==";
    qDebug().noquote() << QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));

    // 例1：将 rfid_infos.rfid.lot_id 更新为单个新值（对数组中每个元素生效）
    ParameterHelper::JsonUpdateRfidKeyFromVariant(obj, "rfid_infos.rfid.lot_id", QVariant(QString("UPDATED-LOT-0001")));

    qDebug() << "\n== 更新 lot_id 为单值（所有现有元素被设置为 UPDATED-LOT-0001） ==";
    qDebug().noquote() << QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));

    // 例2：传入数组，则会追加新的 rfid 元素（每个元素为 { lot_id: elem }）
    QVariantList newLots;
    newLots << QVariant(QString("LOT-20251025-03")) << QVariant(QString("LOT-20251025-04"));
    ParameterHelper::JsonUpdateRfidKeyFromVariant(obj, "rfid_infos.rfid.lot_id", QVariant::fromValue(newLots));

    qDebug() << "\n== 追加两个新的 lot_id 元素（作为对象追加到 rfid 数组） ==";
    qDebug().noquote() << QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));

    // 例3：更新/创建更深路径（如果中间对象不存在会自动创建）
    // 向 path "meta.info.created_by" 写入字符串
    ParameterHelper::JsonUpdateRfidKeyFromVariant(obj, "meta.info.created_by", QVariant(QString("operator-1")));

    qDebug() << "\n== 向不存在的路径 meta.info.created_by 写入字符串（会自动创建中间对象） ==";
    qDebug().noquote() << QString::fromUtf8(QJsonDocument(obj).toJson(QJsonDocument::Indented));
}

/**
 * @brief 按钮"加载配置"
 */
void MainWindow::onLoadWebConfig() {
    const QString file = QFileDialog::getOpenFileName(this, tr("选择 WebService 配置 JSON"), QString(), "*.json");
    if (file.isEmpty()) return;
    editWebCfg->setText(file);
    doLoadWebConfig(file);
}

/**
 * @brief 槽函数：启动 WebService 服务
 */
void MainWindow::onStartWebService() {
    if (webConfigPath.isEmpty()) {
        appendLog(logWeb, tr("请先加载 WebService 配置"));
        return;
    }

    bool ok = false;
    quint16 port = editWebPort->text().toUShort(&ok);
    if (!ok || port == 0) port = 8026;

    webService->setCaseInsensitiveFunctionMatch(chkCaseInsensitive->isChecked());
    webService->setStrictHeadFunctionMatch(chkStrictHeadMatch->isChecked());
    webService->setAllowOnlyPushDirection(chkOnlyPush->isChecked());

    // 回调超时
    bool okTimeout = false;
    int timeoutMs = editResponderTimeout->text().toInt(&okTimeout);
    if (!okTimeout || timeoutMs < 0) timeoutMs = 0;
    webService->setResponderTimeoutMs(timeoutMs);

    // 读取白名单
    QStringList allow;
    for (int i = 0; i < listAllowed->count(); ++i) {
        allow << listAllowed->item(i)->text();
    }
    webService->setAllowedFunctions(allow);

    if (!webService->start(port, "0.0.0.0")) {
        appendLog(logWeb, tr("启动失败: %1").arg(webService->lastError()));
        return;
    }
    appendLog(logWeb, tr("WebService 已启动，端口 %1").arg(port));
}

/**
 * @brief 槽函数：停止 WebService 服务
 */
void MainWindow::onStopWebService() {
    webService->stop();
    appendLog(logWeb, tr("WebService 已停止"));
}

/**
 * @brief 从当前 WebService 配置文件刷新 function_name 白名单列表
 * 通常在加载 WebService 配置后调用该函数，用配置中的所有接口名初始化白名单。
 */
void MainWindow::onRefreshAllowListFromConfig() {
    listAllowed->clear();

    if (webConfigPath.isEmpty()) return;

    QMap<QString, EapInterfaceMeta> map;
    QString url, err;
    if (!VendorConfigLoader::loadFromFile(webConfigPath, map, url, err)) {
        appendLog(logWeb, tr("读取配置失败: %1").arg(err));
        return;
    }
    for (const auto& key : map.keys()) {
        if (key == "base_url") continue;
        listAllowed->addItem(key);
    }
    appendLog(logWeb, tr("白名单已从配置刷新，共 %1 条").arg(listAllowed->count()));
}

/**
 * @brief 设置回调超时
 */
void MainWindow::onResponderTimeoutChanged() {
    bool ok = false;
    int t = editResponderTimeout->text().toInt(&ok);
    if (!ok || t < 0) t = 0;
    webService->setResponderTimeoutMs(t);
    appendLog(logWeb, tr("回调超时设置为 %1 ms").arg(t));
}

/**
 * @brief 获取端口输入
 */
void MainWindow::onPortChanged() {
    appendLog(logWeb, tr("端口设置为 %1（重启服务后生效）").arg(editWebPort->text()));
}

// -------------------- 工具函数 --------------------

/**
 * @brief 从 JSON 参数文件加载参数集合到内存
 * @param filename 参数 JSON 文件路径
 * @param store    用于保存结果的参数映射（键为字符串，值为参数表 QVariantMap），函数内会先 clear
 * @param logArea  日志输出区域，用于记录错误信息，可为 nullptr（此时不输出日志）
 * @return true 表示加载并解析成功，false 表示过程中发生错误
 */
bool MainWindow::loadParamsFromFile(const QString& filename, QMap<QString, QVariantMap>& store, QPlainTextEdit* logArea) {
    QFile f(filename);
    if (!f.open(QIODevice::ReadOnly)) {
        appendLog(logArea, tr("无法打开参数文件: %1").arg(filename));
        return false;
    }
    QJsonParseError err{};
    const auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        appendLog(logArea, tr("参数 JSON 解析失败: %1").arg(err.errorString()));
        return false;
    }
    store.clear();
    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        if (!it.value().isObject()) continue;
        QVariantMap m = it.value().toObject().toVariantMap();
        store.insert(it.key(), m);
    }
    return true;
}

/**
 * @brief 根据接口 key 从参数存储中取出对应的参数表
 * @param key   接口标识，用于在参数存储中查找
 * @param store 参数存储映射（通常为 uploadParams）
 * @return 找到时为对应的参数表，未找到时为空的 QVariantMap
 */
QVariantMap MainWindow::paramsFor(const QString& key, const QMap<QString, QVariantMap>& store) const {
    return store.value(key);
}

/**
 * @brief 为接口参数中的时间相关字段自动填充当前时间
 * @param meta   接口元数据，提供 bodyMap 用于确定需处理的字段名
 * @param params 本次调用的参数表，将在其中就地更新时间相关字段
 */
void MainWindow::applyAutoTimeFields(const EapInterfaceMeta& meta, QVariantMap& params) const {
    for (auto it = meta.bodyMap.begin(); it != meta.bodyMap.end(); ++it) {
        const QString localKey = it.key();
        if (localKey.contains("time", Qt::CaseInsensitive)) {
            const QVariant v = params.value(localKey);
            if (!v.isValid() || v.toString().trimmed().isEmpty() || v.toString().trimmed() == "auto") {
                params[localKey] = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
            }
        }
    }
}

/**
 * @brief 绑定 EAPInterfaceManager 的日志信号到指定日志区域
 * @param mgr   要监听的接口管理器实例
 * @param area  日志输出区域（QPlainTextEdit），可为空指针（为空则内部 appendLog/appendJson 会忽略）
 * @param tag   日志前缀标签，用于标识该 mgr 的来源，例如 "上传客户端" 等
 */
void MainWindow::connectManagerLogs(EAPInterfaceManager* mgr, QPlainTextEdit* area, const QString& tag) {
    connect(mgr, &EAPInterfaceManager::requestSent, this, // 请求已经发出
        [=](const QString& key, const QJsonObject& payload) {
            appendJson(area, QString("%1 请求 [%2] 已发送").arg(tag, key), payload);
        });
    connect(mgr, &EAPInterfaceManager::responseReceived, this, // 收到原始响应 JSON
        [=](const QString& key, const QJsonObject& resp) {
            appendJson(area, QString("%1 响应 [%2] 原始内容").arg(tag, key), resp);
        });
    connect(mgr, &EAPInterfaceManager::mappedResultReady, this, // 响应已经按 mapping 规则映射成本地 QVariantMap
        [=](const QString& key, const QVariantMap& result) {
            QJsonDocument doc = QJsonDocument::fromVariant(result);
            appendLog(area, QString("%1 映射字段 [%2]:\n%3")
                .arg(tag, key, QString::fromUtf8(doc.toJson(QJsonDocument::Indented))));
        });
    connect(mgr, &EAPInterfaceManager::requestFailed, this, // 请求失败
        [=](const QString& key, const QString& err) {
            appendLog(area, QString("%1 请求失败 [%2]: %3").arg(tag, key, err));
        });
}

/**
 * @brief 绑定 EAPWebService 的服务器事件日志到 logWeb
 * 调用本函数一次后，WebService 生命周期内所有请求/响应相关事件都会集中在 logWeb 中呈现，方便调试和追踪。
 */
void MainWindow::connectWebServiceLogs() {
    // 服务器事件
    connect(webService, &EAPWebService::requestReceived, this, // 收到请求
        [=](const QString& fn, const QJsonObject& json,
            const QMap<QString, QString>& headers, const QString& remote) {
                QJsonObject meta;
                for (auto it = headers.begin(); it != headers.end(); ++it) meta.insert(it.key(), it.value());
                appendLog(logWeb, tr("收到请求 fn=%1 from=%2").arg(fn, remote));
                appendJson(logWeb, tr("headers"), meta);
                appendJson(logWeb, tr("payload"), json);
        });
    connect(webService, &EAPWebService::mappedRequestReady, this, // 请求参数映射完成
        [=](const QString& fn, const QVariantMap& params, const QJsonObject&) {
            QJsonDocument doc = QJsonDocument::fromVariant(params);
            appendLog(logWeb, tr("映射完成 fn=%1").arg(fn));
            appendLog(logWeb, QString::fromUtf8(doc.toJson(QJsonDocument::Indented)));
        });
    connect(webService, &EAPWebService::requestRejected, this, // 请求被拒绝（白名单、方向不符、校验失败等）
        [=](const QString& fn, int http, const QString& reason, const QString& remote) {
            appendLog(logWeb, tr("请求拒绝 fn=%1 http=%2 reason=%3 from=%4")
                .arg(fn).arg(http).arg(reason, remote));
        });
    connect(webService, &EAPWebService::responseSent, this, // 已经将响应发送给客户端
        [=](const QString& fn, int http, const QJsonObject& resp, const QString& remote) {
            appendLog(logWeb, tr("已响应 fn=%1 http=%2 to=%3").arg(fn).arg(http).arg(remote));
            appendJson(logWeb, tr("response"), resp);
        });
}

/**
 * @brief 创建并初始化全局消息日志记录器
 */
void MainWindow::createLogger()
{
    messageLogger = new EAPMessageLogger(this);
    messageLogger->initialize("./eap_messages.db");
    uploadMgr->setMessageLogger(messageLogger);
    webService->setMessageLogger(messageLogger);
}

/**
 * @brief 创建消息日志查看窗口并添加到主界面标签页
 */
void MainWindow::createLoggerWidget()
{
    messageLogWidget = new EAPMessageLogWidget(this);
    messageLogWidget->setMessageLogger(messageLogger);
    tabs->addTab(messageLogWidget, tr("EAP交互记录"));
}
