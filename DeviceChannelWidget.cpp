#include "DeviceChannelWidget.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDebug>
#include <QDateTime>
#include <QSettings>

DeviceChannelWidget::DeviceChannelWidget(int id, QWidget *parent)
    : QWidget(parent), m_id(id)
{
    m_serial = new QSerialPort(this);
    setupUi();
}

// ====================================================================
// 1. 界面构建
// ====================================================================
void DeviceChannelWidget::setupUi() {
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    m_group = new QGroupBox(QString("通道 %1").arg(m_id));
    setChannelStatus(true);

    QVBoxLayout *groupLayout = new QVBoxLayout(m_group);
    groupLayout->setContentsMargins(4, 6, 4, 4);
    groupLayout->setSpacing(4);

    // --- A. 顶部控制栏 ---
    QHBoxLayout *topLayout = new QHBoxLayout();

    m_cbModel = new QComboBox();
    QStringList configFiles = ConfigManager::getConfigFileList();
    if (configFiles.isEmpty()) {
        m_cbModel->addItem("默认配置");
        m_cbModel->setEnabled(false);
    } else {
        m_cbModel->addItems(configFiles);
        m_cbModel->setCurrentIndex(0);
        ConfigManager::instance().loadConfig(m_cbModel->currentText());
    }
    m_cbModel->setMaximumWidth(100);

    m_cbPort = new QComboBox();
    const auto ports = QSerialPortInfo::availablePorts();
    for(const auto &info : ports) m_cbPort->addItem(info.portName());
    m_cbPort->setMaximumWidth(80);

    m_cbBaud = new QComboBox();
    m_cbBaud->addItems({"9600", "115200","460800", "921600"});
    m_cbBaud->setCurrentText("115200");
    m_cbBaud->setMaximumWidth(70);
    // ========================================================
    // [新增] 从本地 settings.ini 读取当前通道的历史配置
    // ========================================================
    QSettings settings("settings.ini", QSettings::IniFormat);

    // 1. 恢复机型配置
    QString savedModel = settings.value(QString("Channel_%1/Model").arg(m_id)).toString();
    if (!savedModel.isEmpty()) {
        int idx = m_cbModel->findText(savedModel);
        if (idx >= 0) m_cbModel->setCurrentIndex(idx);
    }

    // 2. 恢复串口号 (如果上次保存的串口当前没插上，findText 会返回 -1，安全跳过)
    QString savedPort = settings.value(QString("Channel_%1/Port").arg(m_id)).toString();
    if (!savedPort.isEmpty()) {
        int idx = m_cbPort->findText(savedPort);
        if (idx >= 0) m_cbPort->setCurrentIndex(idx);
    }

    // 3. 恢复波特率
    QString savedBaud = settings.value(QString("Channel_%1/Baud").arg(m_id)).toString();
    if (!savedBaud.isEmpty()) {
        int idx = m_cbBaud->findText(savedBaud);
        if (idx >= 0) m_cbBaud->setCurrentIndex(idx);
    }
    // ========================================================

    QPushButton *btnStart = new QPushButton("开启");
    QPushButton *btnStop = new QPushButton("停止");
    QPushButton *btnClear = new QPushButton("清空");
    btnStart->setStyleSheet("color: green; font-weight: bold;");
    btnStop->setStyleSheet("color: red;");
    btnStart->setMaximumWidth(50);
    btnStop->setMaximumWidth(50);
    btnClear->setMaximumWidth(50);

    topLayout->addWidget(new QLabel("机型:"));
    topLayout->addWidget(m_cbModel);
    topLayout->addWidget(new QLabel("端口:"));
    topLayout->addWidget(m_cbPort);
    topLayout->addWidget(new QLabel("波特:"));
    topLayout->addWidget(m_cbBaud);
    topLayout->addWidget(btnStart);
    topLayout->addWidget(btnStop);
    topLayout->addWidget(btnClear);
    topLayout->addStretch();

    // --- B. 身份比对区 (两行) ---
    QGridLayout *compareLayout = new QGridLayout();
    compareLayout->setContentsMargins(0, 0, 0, 0);
    compareLayout->setSpacing(4);

    QLabel *lblScan = new QLabel("扫码输入:");
    lblScan->setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    m_editBarcode = new QLineEdit();
    m_editBarcode->setPlaceholderText("请扫描设备二维码...");
    m_editBarcode->setFont(QFont("Arial", 10));

    QLabel *lblRead = new QLabel("串口读取:");
    lblRead->setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    m_editSerialRead = new QLineEdit();
    m_editSerialRead->setPlaceholderText("等待读取 IMEI/IMSI/MAC...");
    m_editSerialRead->setFont(QFont("Arial", 10));
    m_editSerialRead->setReadOnly(true);
    m_editSerialRead->setStyleSheet("background-color: #F0F0F0; color: #555;");

    compareLayout->addWidget(lblScan, 0, 0);
    compareLayout->addWidget(m_editBarcode, 0, 1);
    compareLayout->addWidget(lblRead, 1, 0);
    compareLayout->addWidget(m_editSerialRead, 1, 1);

    // --- C. 结果表格 (8列) ---
    m_tableRes = new QTableWidget();
    m_tableRes->setColumnCount(8);
    m_tableRes->setHorizontalHeaderLabels({"项", "值", "项", "值", "项", "值", "项", "值"});
    m_tableRes->verticalHeader()->setVisible(false);

    QHeaderView* header = m_tableRes->horizontalHeader();
    for(int k=0; k<4; k++) {
        header->setSectionResizeMode(k*2, QHeaderView::ResizeToContents);
        header->setSectionResizeMode(k*2+1, QHeaderView::Stretch);
    }
    m_tableRes->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_tableRes->verticalHeader()->setDefaultSectionSize(26);

    // --- D. 日志窗口 (大容量) ---
    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setMinimumHeight(60);
    m_logView->setMaximumHeight(150);
    m_logView->setFont(QFont("Consolas", 9));
    m_logView->setMaximumBlockCount(3000);

    // --- E. 组装 ---
    groupLayout->addLayout(topLayout);
    groupLayout->addLayout(compareLayout);
    groupLayout->addWidget(m_tableRes);
    groupLayout->addWidget(m_logView);
    mainLayout->addWidget(m_group);

    //========================================================
        // [新增防呆设计] 剥夺所有非核心控件的键盘焦点！
        // 确保无论工人鼠标怎么乱点，扫码枪的输入永远不会被这些控件吞掉
        // ========================================================
    m_cbModel->setFocusPolicy(Qt::NoFocus);
    m_cbPort->setFocusPolicy(Qt::NoFocus);
    m_cbBaud->setFocusPolicy(Qt::NoFocus);

    // 按钮只能用鼠标点，不能用空格/回车键触发
    btnStart->setFocusPolicy(Qt::NoFocus);
    btnStop->setFocusPolicy(Qt::NoFocus);
    btnClear->setFocusPolicy(Qt::NoFocus);

    // 只读框、表格、日志区严禁获取光标
    m_editSerialRead->setFocusPolicy(Qt::NoFocus);
    m_tableRes->setFocusPolicy(Qt::NoFocus);
    m_logView->setFocusPolicy(Qt::NoFocus);
    // ========================================================

    // --- F. 信号 ---
    connect(btnStart, &QPushButton::clicked, this, &DeviceChannelWidget::onStartClicked);
    connect(btnStop, &QPushButton::clicked, this, &DeviceChannelWidget::onStopClicked);
    connect(m_serial, &QSerialPort::readyRead, this, &DeviceChannelWidget::onSerialReadyRead);
    connect(m_editBarcode, &QLineEdit::textChanged, this, &DeviceChannelWidget::onBarcodeChanged);
    // [新增代码] 捕获扫码枪扫完自带的“回车键”，向外发送跳转信号
    connect(m_editBarcode, &QLineEdit::returnPressed, this, [=](){
        emit barcodeReturnPressed(m_id);
    });

    // ========================================================
    // [修改与新增] 实时监听用户的下拉选择，并立刻保存到本地
    // ========================================================

    // 1. 监听机型切换
    connect(m_cbModel, &QComboBox::currentTextChanged, this, [=](const QString &fileName){
        if (fileName.isEmpty() || fileName == "默认配置") return;

        // 保存配置
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.setValue(QString("Channel_%1/Model").arg(m_id), fileName);

        m_logView->appendPlainText(QString(">>> Load: %1").arg(fileName));
        ConfigManager::instance().loadConfig(fileName);
        resetUI();
    });

    // 2. 监听串口号切换
    connect(m_cbPort, &QComboBox::currentTextChanged, this, [=](const QString &portName){
        if(portName.isEmpty()) return;
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.setValue(QString("Channel_%1/Port").arg(m_id), portName);
    });

    // 3. 监听波特率切换
    connect(m_cbBaud, &QComboBox::currentTextChanged, this, [=](const QString &baudRate){
        if(baudRate.isEmpty()) return;
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.setValue(QString("Channel_%1/Baud").arg(m_id), baudRate);
    });
    // ========================================================

    connect(btnClear, &QPushButton::clicked, this, [=](){
        m_buffer.clear();
        m_logView->clear();
        resetUI();
    });
}

// ====================================================================
// 2. 界面重置
// ====================================================================
void DeviceChannelWidget::resetUI() {
    m_currentIds.clear();
    m_editSerialRead->clear();
    m_editSerialRead->setStyleSheet("background-color: #F0F0F0; color: #555;");

    auto teleRules = ConfigManager::instance().getTelemetryRules();
    int rows = (teleRules.size() + 3) / 4;

    m_tableRes->clear();
    m_tableRes->setRowCount(rows);
    m_tableRes->setColumnCount(8);
    m_tableRes->setHorizontalHeaderLabels({"项", "值", "项", "值", "项", "值", "项", "值"});
    m_tableRes->verticalHeader()->setVisible(false);

    m_mapResRow.clear();
    for(int i=0; i<teleRules.size(); i++) {
        int r = i / 4;
        int c_base = (i % 4) * 2;
        QTableWidgetItem *head = new QTableWidgetItem(teleRules[i].name);
        head->setBackground(QColor(240, 240, 240));
        head->setFont(QFont("Microsoft YaHei", 9));
        m_tableRes->setItem(r, c_base, head);

        QTableWidgetItem *resItem = new QTableWidgetItem("WAIT");
        resItem->setTextAlignment(Qt::AlignCenter);
        resItem->setFont(QFont("Arial", 9));
        m_tableRes->setItem(r, c_base + 1, resItem);

        m_mapResRow[teleRules[i].key] = i;
    }

    m_logView->appendPlainText("\n----------------- [New Test Start] -----------------");
    m_hasError = false;
    setChannelStatus(true);

    if(m_editBarcode) {
        m_editBarcode->clear();      // [新增] 彻底清空里面的文字
        emit barcodeCleared(); // [新增] 发送求助信号，让主窗口来分配焦点
    }
}

// ====================================================================
// 3. 串口数据处理
// ====================================================================
void DeviceChannelWidget::onSerialReadyRead() {
    QByteArray data = m_serial->readAll();

    // ========================================================
    // [新增] 实时写入文件
    // ========================================================
    if (m_logFile && m_logFile->isOpen()) {
        m_logFile->write(data);
        m_logFile->flush(); // 立即刷新，防止程序崩溃数据丢失
    }
    // ========================================================

    m_buffer.append(data);

    // 防止缓存爆炸 (保留最近 20KB)
    if(m_buffer.size() > 20480) m_buffer.clear();

    processBuffer();
}

void DeviceChannelWidget::processBuffer() {
    while(m_buffer.contains('\n')) {
        int idx = m_buffer.indexOf('\n');
        QString line = QString::fromLocal8Bit(m_buffer.left(idx)).trimmed();
        m_buffer.remove(0, idx + 1);

        if(!line.isEmpty()) {
            parseLine(line);
            m_logView->appendPlainText(line);
        }
    }
}

// ====================================================================
// 4. 核心解析逻辑 (修复重复重置BUG)
// ====================================================================
void DeviceChannelWidget::parseLine(const QString &line) {
    if(!m_isTesting) return;

    // A. 遥测数据
    if(line.contains("$info,")) {
        int start = line.indexOf("$info,");
        parseTelemetry(line.mid(start + 6));
        return;
    }

    // B. JSON
    if (line.trimmed().startsWith("@") && line.trimmed().endsWith("#")) {
        // (省略 JSON 解析代码，此处只负责填表，不负责Reset)
        // 您可以保留原来的 JSON 解析逻辑用于填 ID
        return;
    }

    // C. 标准文本 (核心修复点)
    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    bool needCompare = false;

    const auto idRules = ConfigManager::instance().getIdentityRules();

    for (const QString& part : parts) {
        for(const auto& rule : idRules) {
            if(part.startsWith(rule.prefix, Qt::CaseInsensitive)) {
                QString deviceVal = part.mid(rule.prefix.length()).trimmed();

                // =======================================================
                // [关键修复 1] 只有 IMEI 才有资格触发重置
                // 强制忽略 IMSI、MAC 等触发的重置，防止第二波数据清空第一波
                // =======================================================
                bool isAuthority = (rule.key.compare("imei", Qt::CaseInsensitive) == 0);

                if (isAuthority && !deviceVal.isEmpty()) {
                    QString currentVal = m_currentIds.value(rule.key);
                    qint64 now = QDateTime::currentMSecsSinceEpoch();

                    bool isNew = (currentVal != deviceVal);
                    // [关键修复 2] 将防抖时间延长到 8000ms
                    // 只要 8 秒内收到相同的 IMEI，都视为同一波数据，坚决不重置
                    bool isRetest = (now - m_lastResetTime > 8000);

                    if (isNew || isRetest) {
                        resetUI();
                        m_lastResetTime = now;
                        m_currentIds.insert(rule.key, deviceVal);
                    }
                }

                // 无论是否重置，都更新数据到 Map 和界面
                m_currentIds.insert(rule.key, deviceVal);
                updateSerialDisplay();

                break;
            }
        }
    }

    if(needCompare) performComparison();
}

// ====================================================================
// 5. 动态显示与比对
// ====================================================================
void DeviceChannelWidget::updateSerialDisplay() {
    QStringList displayParts;
    const auto idRules = ConfigManager::instance().getIdentityRules();

    for(const auto& rule : idRules) {
        if(m_currentIds.contains(rule.key)) {
            // 拼凑字符串，高效写法
            displayParts << QString("%1:%2").arg(rule.key.toUpper(), m_currentIds[rule.key]);
        }
    }

    QString fullInfo = displayParts.join(" ");
    m_editSerialRead->setText(fullInfo);

    QString scanText = m_editBarcode->text().trimmed();
    QString serialText = fullInfo.trimmed();

    if (scanText.isEmpty() || serialText.isEmpty()) {
        m_editSerialRead->setStyleSheet("background-color: #F0F0F0; color: #555;");
    } else if (scanText == serialText) {
        m_editSerialRead->setStyleSheet("background-color: #DFF0D8; color: #3C763D; font-weight: bold; border: 2px solid green;");
    } else {
        m_editSerialRead->setStyleSheet("background-color: #F2DEDE; color: #A94442; font-weight: bold; border: 2px solid red;");
    }
}

void DeviceChannelWidget::onBarcodeChanged(const QString &text) {
    Q_UNUSED(text);
    updateSerialDisplay();
}

// ====================================================================
// 6. 辅助函数
// ====================================================================
void DeviceChannelWidget::onStartClicked() {
    // 1. 先关闭旧的（如果有）
    if(m_serial->isOpen()) m_serial->close();

    // 关闭旧日志文件
    if(m_logFile) {
        if(m_logFile->isOpen()) m_logFile->close();
        delete m_logFile;
        m_logFile = nullptr;
    }

    m_serial->setPortName(m_cbPort->currentText());
    m_serial->setBaudRate(m_cbBaud->currentText().toInt());

    if(m_serial->open(QIODevice::ReadWrite)) {
        m_isTesting = true;
        m_logView->appendPlainText("--- 端口已打开 ---");

        // ========================================================
        // [新增] 创建日志文件逻辑
        // 路径格式: Logs/20260114/Ch1_123045.txt
        // ========================================================
        QString dirPath = QString("Logs/%1").arg(QDate::currentDate().toString("yyyyMMdd"));
        QDir dir;
        if(!dir.exists(dirPath)) dir.mkpath(dirPath); // 自动创建目录

        QString fileName = QString("%1/Ch%2_%3.txt")
                               .arg(dirPath)
                               .arg(m_id)
                               .arg(QDateTime::currentDateTime().toString("HHmmss"));

        m_logFile = new QFile(fileName);
        if(m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            m_logView->appendPlainText(QString(">>> Log: %1").arg(fileName));
        } else {
            m_logView->appendPlainText(">>> Warning: 创建日志文件失败!");
        }
        // ========================================================

        resetUI();
    } else {
        m_logView->appendPlainText("错误: 打开串口失败!");
    }
}

// 停止按钮
void DeviceChannelWidget::onStopClicked() {
    if(m_serial->isOpen()) {
        m_serial->close();
        m_isTesting = false;
        m_logView->appendPlainText("--- 端口已关闭 ---");

        // [新增] 关闭文件
        if(m_logFile) {
            if(m_logFile->isOpen()) m_logFile->close();
            delete m_logFile;
            m_logFile = nullptr;
        }

        setChannelStatus(true);
    }
}

// 析构函数
DeviceChannelWidget::~DeviceChannelWidget() {
    if (m_serial->isOpen()) m_serial->close();

    // [新增] 安全关闭文件
    if (m_logFile) {
        if (m_logFile->isOpen()) m_logFile->close();
        delete m_logFile;
    }
}

void DeviceChannelWidget::updateResultItem(const QString &key, const QString &val) {
    if(!m_mapResRow.contains(key)) return;
    int index = m_mapResRow[key];
    int row = index / 4;
    int col = (index % 4) * 2 + 1;

    QTableWidgetItem *item = m_tableRes->item(row, col);
    if(!item) return;

    auto rules = ConfigManager::instance().getTelemetryRules();
    if(index >= rules.size()) return;
    TestRule rule = rules[index];

    if (rule.type == Type_Display) {
        item->setText(val);
        item->setForeground(QBrush(QColor(0, 0, 200)));
        return;
    }
    if (item->text() == "OK") return;

    bool pass = false;
    double numVal = val.toDouble();
    if(rule.type == Type_Match) pass = (val == rule.targetVal);
    else if (rule.type == Type_NotMatch) pass = (val != rule.targetVal);
    else if (rule.type == Type_Range) pass = (numVal >= rule.minVal && numVal <= rule.maxVal);
    else pass = (val != "0" && !val.isEmpty());

    if(pass) {
        item->setText("OK");
        item->setBackground(QBrush(Qt::white));
        item->setForeground(QBrush(QColor(0, 150, 0)));
        item->setFont(QFont("Microsoft YaHei", 9, QFont::Bold));
    } else {
        item->setText(QString("NG (%1)").arg(val));
        item->setBackground(QBrush(QColor(255, 0, 0)));
        item->setForeground(QBrush(Qt::white));
        item->setFont(QFont("Arial", 8));
    }
}

void DeviceChannelWidget::performComparison() {
    // 简单示例：遍历所有结果，如果有 NG 则变红
    bool allPass = true;
    for(int i=0; i<m_tableRes->rowCount(); i++) {
        for(int j=1; j<8; j+=2) { // 检查值所在的列
            QTableWidgetItem *item = m_tableRes->item(i, j);
            if(item && (item->text().startsWith("NG") || item->text() == "WAIT")) {
                // 注意：这里WAIT可能意味着还没测完，暂不算失败，根据需求调整
                if(item->text().startsWith("NG")) allPass = false;
            }
        }
    }
    setChannelStatus(allPass);
}

void DeviceChannelWidget::setChannelStatus(bool active) {
    if(active) m_group->setStyleSheet("QGroupBox { border: 2px solid green; font-weight: bold; margin-top: 1ex; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; }");
    else m_group->setStyleSheet("QGroupBox { border: 2px solid red; font-weight: bold; margin-top: 1ex; } QGroupBox::title { subcontrol-origin: margin; subcontrol-position: top center; }");
}

void DeviceChannelWidget::parseTelemetry(const QString &dataPart) {
    // 原始数据示例: "... v:4.211,t:0.776,35,pwr:1 ..."

    // 1. 先按逗号粗暴分割
    // 结果变成 list: ["v:4.211", "t:0.776", "35", "pwr:1"]
    const QStringList parts = dataPart.split(',', Qt::SkipEmptyParts);

    bool isNextT2 = false; // 标记位：下一个纯数字是否为 t2
    bool anyUpdate = false;

    for(const QString &part : parts) {
        QString cleanPart = part.trimmed();

        // --- 情况 A: 标准的 key:value (例如 t:0.776) ---
        if(cleanPart.contains(':')) {
            QStringList kv = cleanPart.split(':');
            if(kv.size() == 2) {
                QString key = kv[0].trimmed();
                QString val = kv[1].trimmed();

                // [特殊处理] 如果检测到 key 是 "t"
                if(key.compare("t", Qt::CaseInsensitive) == 0) {
                    // 1. 把前半部分(0.776) 映射给 "t1"
                    updateResultItem("t1", val);

                    // 2. 标记：下一个没有冒号的数字，就是 "t2"
                    isNextT2 = true;
                }
                else {
                    // 其他普通项 (v, pwr, mac...)
                    updateResultItem(key, val);
                    isNextT2 = false; // 重置标记
                }
                anyUpdate = true;
            }
        }
        // --- 情况 B: 没有冒号的纯数值 (例如 35) ---
        else if(!cleanPart.isEmpty()) {
            // 如果上一个项是 t，那这个项肯定是 t2
            if(isNextT2) {
                updateResultItem("t2", cleanPart);
                isNextT2 = false; // 用完即焚，防止误判
                anyUpdate = true;
            }
        }
    }

    if(anyUpdate) performComparison();
}

ScanResult DeviceChannelWidget::checkScanInput(const QString &code) {
    QString mySerialData = m_editSerialRead->text().trimmed();

    // 情况 A: 还没有串口数据，或者没开机
    if(mySerialData.isEmpty()) {
        // 如果光标在这个框里，用户非要扫，那就先把码填进去，显示灰色等待
        if(m_editBarcode->hasFocus()) {
            m_editBarcode->setText(code);
            return ScanResult::Mismatch; // 暂时算错，或者你可以定义一个 Wait 状态
        }
        return ScanResult::Ignore;
    }

    // 情况 B: 完美匹配 (Auto-Routing)
    // 逻辑：不管光标在不在我这，只要码对上了，我就认领
    if(code == mySerialData) { // 或者 contains
        m_editBarcode->setText(code);
        updateSerialDisplay(); // 触发变绿
        return ScanResult::Match;
    }

    // 情况 C: 不匹配，但是光标在我这里 (Focus Trap)
    // 逻辑：光标在我这，说明用户就是在测我，但是扫错了 -> 判 NG
    if(m_editBarcode->hasFocus()) {
        m_editBarcode->setText(code);
        updateSerialDisplay(); // 触发变红
        return ScanResult::Mismatch;
    }

    // 情况 D: 既不匹配，光标也不在我这 -> 跟我无关
    return ScanResult::Ignore;
}
