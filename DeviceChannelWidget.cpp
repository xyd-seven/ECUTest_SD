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

void DeviceChannelWidget::setupUi() {
    // [新增] 实例化自动重连定时器
    m_reconnectTimer = new QTimer(this);
    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(2, 2, 2, 2);
    mainLayout->setSpacing(2);

    m_group = new QGroupBox(QString("通道 %1").arg(m_id));
    setChannelStatus(true);

    QVBoxLayout *groupLayout = new QVBoxLayout(m_group);
    groupLayout->setContentsMargins(4, 6, 4, 4);
    groupLayout->setSpacing(4);

    QHBoxLayout *topLayout = new QHBoxLayout();

    m_cbModel = new QComboBox();
    QStringList configFiles = ConfigManager::getConfigFileList();
    if (configFiles.isEmpty()) {
        m_cbModel->addItem("默认配置");
        m_cbModel->setEnabled(false);
    } else {
        m_cbModel->addItems(configFiles);
        m_cbModel->setCurrentIndex(0);
        m_config.loadConfig(m_cbModel->currentText()); // 使用本地配置
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

    QSettings settings("settings.ini", QSettings::IniFormat);
    QString savedModel = settings.value(QString("Channel_%1/Model").arg(m_id)).toString();
    if (!savedModel.isEmpty()) {
        int idx = m_cbModel->findText(savedModel);
        if (idx >= 0) m_cbModel->setCurrentIndex(idx);
    }
    QString savedPort = settings.value(QString("Channel_%1/Port").arg(m_id)).toString();
    if (!savedPort.isEmpty()) {
        int idx = m_cbPort->findText(savedPort);
        if (idx >= 0) m_cbPort->setCurrentIndex(idx);
    }
    QString savedBaud = settings.value(QString("Channel_%1/Baud").arg(m_id)).toString();
    if (!savedBaud.isEmpty()) {
        int idx = m_cbBaud->findText(savedBaud);
        if (idx >= 0) m_cbBaud->setCurrentIndex(idx);
    }

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

    m_logView = new QPlainTextEdit();
    m_logView->setReadOnly(true);
    m_logView->setMinimumHeight(60);
    m_logView->setMaximumHeight(150);
    m_logView->setFont(QFont("Consolas", 9));
    m_logView->setMaximumBlockCount(3000);

    groupLayout->addLayout(topLayout);
    groupLayout->addLayout(compareLayout);
    groupLayout->addWidget(m_tableRes);
    groupLayout->addWidget(m_logView);
    mainLayout->addWidget(m_group);

    // Kiosk 防呆
    m_cbModel->setFocusPolicy(Qt::NoFocus);
    m_cbPort->setFocusPolicy(Qt::NoFocus);
    m_cbBaud->setFocusPolicy(Qt::NoFocus);
    btnStart->setFocusPolicy(Qt::NoFocus);
    btnStop->setFocusPolicy(Qt::NoFocus);
    btnClear->setFocusPolicy(Qt::NoFocus);
    m_editSerialRead->setFocusPolicy(Qt::NoFocus);
    m_tableRes->setFocusPolicy(Qt::NoFocus);
    m_logView->setFocusPolicy(Qt::NoFocus);

    //信号
    connect(btnStart, &QPushButton::clicked, this, &DeviceChannelWidget::onStartClicked);
    connect(btnStop, &QPushButton::clicked, this, &DeviceChannelWidget::onStopClicked);
    connect(m_serial, &QSerialPort::readyRead, this, &DeviceChannelWidget::onSerialReadyRead);
    connect(m_editBarcode, &QLineEdit::textChanged, this, &DeviceChannelWidget::onBarcodeChanged);
    connect(m_editBarcode, &QLineEdit::returnPressed, this, [=](){
        emit barcodeReturnPressed(m_id);
    });
    // ================= [新增] =================
    // 监听串口底层错误（如 USB 被拔出）
    connect(m_serial, &QSerialPort::errorOccurred, this, &DeviceChannelWidget::onSerialError);
    // 监听定时器，执行重连动作
    connect(m_reconnectTimer, &QTimer::timeout, this, &DeviceChannelWidget::tryReconnect);
    // ==========================================

    connect(m_cbModel, &QComboBox::currentTextChanged, this, [=](const QString &fileName){
        if (fileName.isEmpty() || fileName == "默认配置") return;
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.setValue(QString("Channel_%1/Model").arg(m_id), fileName);
        m_logView->appendPlainText(QString(">>> Load: %1").arg(fileName));
        m_config.loadConfig(fileName); // 使用本地配置
        resetUI();
    });

    connect(m_cbPort, &QComboBox::currentTextChanged, this, [=](const QString &portName){
        if(portName.isEmpty()) return;
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.setValue(QString("Channel_%1/Port").arg(m_id), portName);
    });

    connect(m_cbBaud, &QComboBox::currentTextChanged, this, [=](const QString &baudRate){
        if(baudRate.isEmpty()) return;
        QSettings settings("settings.ini", QSettings::IniFormat);
        settings.setValue(QString("Channel_%1/Baud").arg(m_id), baudRate);
    });

    connect(btnClear, &QPushButton::clicked, this, [=](){
        m_buffer.clear();
        m_logView->clear();
        resetUI();
    });
}

void DeviceChannelWidget::resetUI() {
    m_currentIds.clear();
    m_editSerialRead->clear();
    m_editSerialRead->setStyleSheet("background-color: #F0F0F0; color: #555;");

    auto teleRules = m_config.getTelemetryRules();
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
        m_editBarcode->clear();
        emit barcodeCleared(true); // 任何触发重置的情况，发送全局清空请求
    }
}

void DeviceChannelWidget::onSerialReadyRead() {
    QByteArray data = m_serial->readAll();

    // [优化] 跨天日志切割判定
    if (m_logFile && m_logFile->isOpen()) {
        QDate currentDate = QDate::currentDate();
        if (currentDate != m_logDate) {
            m_logFile->close();
            delete m_logFile;

            QString dirPath = QString("Logs/%1").arg(currentDate.toString("yyyyMMdd"));
            QDir().mkpath(dirPath);
            QString fileName = QString("%1/Ch%2_%3.txt").arg(dirPath).arg(m_id).arg(QDateTime::currentDateTime().toString("HHmmss"));
            m_logFile = new QFile(fileName);
            if (m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                m_logDate = currentDate;
                m_logView->appendPlainText(">>> [New Day Log Rotation]");
            }
        }

        if (m_logFile->isOpen()) {
            m_logFile->write(data);
            m_logFile->flush();
        }
    }

    m_buffer.append(data);

    // [优化] 防止极端溢出截断有效数据
    if(m_buffer.size() > 20480) {
        int lastIdx = m_buffer.lastIndexOf('\n');
        if(lastIdx != -1) {
            m_buffer.remove(0, lastIdx + 1); // 保留最后一个换行符之后的残余数据
        } else {
            m_buffer.clear(); // 真的全是乱码，直接清空
        }
    }

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

void DeviceChannelWidget::parseLine(const QString &line) {
    if(!m_isTesting) return;

    if(line.contains("$info,")) {
        int start = line.indexOf("$info,");
        parseTelemetry(line.mid(start + 6));
        return;
    }

    if (line.trimmed().startsWith("@") && line.trimmed().endsWith("#")) return;

    const QStringList parts = line.split(' ', Qt::SkipEmptyParts);
    bool needCompare = false;

    const auto idRules = m_config.getIdentityRules();

    for (const QString& part : parts) {
        for(const auto& rule : idRules) {
            if(part.startsWith(rule.prefix, Qt::CaseInsensitive)) {
                QString deviceVal = part.mid(rule.prefix.length()).trimmed();
                bool isAuthority = (rule.key.compare("imei", Qt::CaseInsensitive) == 0);

                if (isAuthority && !deviceVal.isEmpty()) {
                    QString currentVal = m_currentIds.value(rule.key);
                    qint64 now = QDateTime::currentMSecsSinceEpoch();
                    bool isNew = (currentVal != deviceVal);
                    bool isRetest = (now - m_lastResetTime > 8000);

                    if (isNew || isRetest) {
                        resetUI();
                        m_lastResetTime = now;
                        m_currentIds.insert(rule.key, deviceVal);
                    }
                }

                m_currentIds.insert(rule.key, deviceVal);
                updateSerialDisplay();
                break;
            }
        }
    }
    if(needCompare) performComparison();
}

void DeviceChannelWidget::updateSerialDisplay() {
    QStringList displayParts;
    const auto idRules = m_config.getIdentityRules();

    for(const auto& rule : idRules) {
        if(m_currentIds.contains(rule.key)) {
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

void DeviceChannelWidget::onStartClicked() {
    m_reconnectTimer->stop(); // [新增] 点击开启时，先停掉可能存在的重连定时器
    if(m_serial->isOpen()) m_serial->close();

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

        m_logDate = QDate::currentDate(); // 记录开启日期
        QString dirPath = QString("Logs/%1").arg(m_logDate.toString("yyyyMMdd"));
        QDir().mkpath(dirPath);

        QString fileName = QString("%1/Ch%2_%3.txt").arg(dirPath).arg(m_id).arg(QDateTime::currentDateTime().toString("HHmmss"));
        m_logFile = new QFile(fileName);
        if(m_logFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
            m_logView->appendPlainText(QString(">>> Log: %1").arg(fileName));
        }
        resetUI();
    } else {
        m_logView->appendPlainText("错误: 打开串口失败!");
    }
}

void DeviceChannelWidget::onStopClicked() {
    m_reconnectTimer->stop(); // [新增] 用户主动点击停止，彻底放弃重连尝试
    if(m_serial->isOpen()) {
        m_serial->close();
        m_isTesting = false;
        m_logView->appendPlainText("--- 端口已关闭 ---");

        if(m_logFile) {
            if(m_logFile->isOpen()) m_logFile->close();
            delete m_logFile;
            m_logFile = nullptr;
        }
        setChannelStatus(true);
    }
}

DeviceChannelWidget::~DeviceChannelWidget() {
    if (m_serial->isOpen()) m_serial->close();
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

    auto rules = m_config.getTelemetryRules();
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
    bool allPass = true;
    for(int i=0; i<m_tableRes->rowCount(); i++) {
        for(int j=1; j<8; j+=2) {
            QTableWidgetItem *item = m_tableRes->item(i, j);
            if(item && (item->text().startsWith("NG") || item->text() == "WAIT")) {
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
    const QStringList parts = dataPart.split(',', Qt::SkipEmptyParts);
    bool isNextT2 = false;
    bool anyUpdate = false;

    for(const QString &part : parts) {
        QString cleanPart = part.trimmed();
        if(cleanPart.contains(':')) {
            QStringList kv = cleanPart.split(':');
            if(kv.size() == 2) {
                QString key = kv[0].trimmed();
                QString val = kv[1].trimmed();
                if(key.compare("t", Qt::CaseInsensitive) == 0) {
                    updateResultItem("t1", val);
                    isNextT2 = true;
                } else {
                    updateResultItem(key, val);
                    isNextT2 = false;
                }
                anyUpdate = true;
            }
        } else if(!cleanPart.isEmpty()) {
            if(isNextT2) {
                updateResultItem("t2", cleanPart);
                isNextT2 = false;
                anyUpdate = true;
            }
        }
    }
    if(anyUpdate) performComparison();
}

ScanResult DeviceChannelWidget::checkScanInput(const QString &code) {
    QString mySerialData = m_editSerialRead->text().trimmed();

    if(mySerialData.isEmpty()) {
        if(m_editBarcode->hasFocus()) {
            m_editBarcode->setText(code);
            return ScanResult::Mismatch;
        }
        return ScanResult::Ignore;
    }

    if(code == mySerialData) {
        m_editBarcode->setText(code);
        updateSerialDisplay();
        return ScanResult::Match;
    }

    if(m_editBarcode->hasFocus()) {
        m_editBarcode->setText(code);
        updateSerialDisplay();
        return ScanResult::Mismatch;
    }
    return ScanResult::Ignore;
}

// ====================================================================
// [新增逻辑] 拦截串口异常底层信号
// ====================================================================
void DeviceChannelWidget::onSerialError(QSerialPort::SerialPortError error) {
    // ResourceError 通常代表设备被意外移除 (如 USB 线被拔掉)
    if (error == QSerialPort::ResourceError) {
        if (m_serial->isOpen()) {
            m_serial->close(); // 安全关闭已失效的句柄
        }

        m_logView->appendPlainText(QString("\n>>> 警告: 串口[%1]意外断开，正在尝试重连...").arg(m_cbPort->currentText()));
        setChannelStatus(false); // 界面边框变红提示工人

        // 只要用户没有主动点“停止”(m_isTesting 仍为 true)，就启动重连机制
        if (m_isTesting) {
            m_reconnectTimer->start(2000); // 每隔 2 秒尝试重连一次
        }
    }
}

// ====================================================================
// [新增逻辑] 定时轮询重连
// ====================================================================
void DeviceChannelWidget::tryReconnect() {
    if (!m_isTesting) {
        m_reconnectTimer->stop();
        return;
    }

    // 1. 先检查物理层面上，这个 COM 口是不是已经插回来了
    bool portExists = false;
    const auto ports = QSerialPortInfo::availablePorts();
    for (const auto &info : ports) {
        if (info.portName() == m_cbPort->currentText()) {
            portExists = true;
            break;
        }
    }

    // 2. 如果设备插回来了，尝试打开它
    if (portExists) {
        m_serial->setPortName(m_cbPort->currentText());
        if (m_serial->open(QIODevice::ReadWrite)) {
            m_reconnectTimer->stop(); // 重连成功，关闭定时器
            m_logView->appendPlainText(">>> 自动重连成功! 继续测试...");
            setChannelStatus(true);   // 界面边框恢复绿色

            // 注意：这里不需要调用 resetUI()，这样掉线前没测完的数据可以接着测
        }
    }
}
