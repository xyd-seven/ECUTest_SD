#ifndef DEVICECHANNELWIDGET_H
#define DEVICECHANNELWIDGET_H

#include <QWidget>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QPlainTextEdit>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QTimer>
#include <QMap>
#include <QDateTime>
#include <QFile>
#include <QDir>
#include "ConfigManager.h"

enum class ScanResult {
    Match,
    Mismatch,
    Ignore
};

class DeviceChannelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceChannelWidget(int id, QWidget *parent = nullptr);
    ~DeviceChannelWidget();

    bool isTesting() const { return m_isTesting; }
    ScanResult checkScanInput(const QString &code);
    bool hasFocusInBar() const { return m_editBarcode->hasFocus(); }
    bool isBarcodeEmpty() const { return m_editBarcode->text().trimmed().isEmpty(); }

    void setFocusToBarcode() {
        m_editBarcode->setFocus();
        m_editBarcode->selectAll();
    }

    void forceInjectBarcode(const QString &code) {
        m_editBarcode->setText(code);
        updateSerialDisplay();
    }

    void clearBarcodeOnly() {
        if (m_editBarcode) m_editBarcode->clear();
    }

    void reloadConfigList();
    void resetStats();
    
    int getPassCount() const { return m_passCount; }
    int getNgCount() const { return m_ngCount; }

signals:
    void barcodeReturnPressed(int currentId);
    // [优化] 增加 bool 标志，区分是工装压下(全局清空) 还是 仅单一通道重置焦点
    void barcodeCleared(bool isGlobal);
    void statsUpdated(); // [新增] 通知主窗口更新全局统计

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSerialReadyRead();
    void onBarcodeChanged(const QString &text);
    // ================= [新增] =================
    void onSerialError(QSerialPort::SerialPortError error); // 处理串口异常断开
    void tryReconnect();                                    // 定时尝试重连
    void onTimeout();                                       // 处理测试超时
    // ==========================================

private:
    void setupUi();
    void resetUI(bool isGlobal = false); // [修改这行] 增加 isGlobal 参数
    void processBuffer();
    void parseLine(const QString &line);
    void parseTelemetry(const QString &dataPart);
    void updateResultItem(const QString &key, const QString &val);
    void performComparison();
    void setChannelStatus(bool active);
    void updateSerialDisplay();
    void finishTest(bool isPass, bool isTimeout);
    void logYieldData(bool isPass, const QString &terminalId);
    void loadStatsLog();
    void updateStatsUI();

private:
    int m_id;
    bool m_isTesting = false;      // 表示通道串口是否已打开
    bool m_isDeviceFinished = false; // 表示当前这台设备的测试生命周期是否已结束
    bool m_hasError = false;

    QSerialPort *m_serial;
    QByteArray m_buffer;

    QGroupBox *m_group;
    QComboBox *m_cbModel;
    QComboBox *m_cbPort;
    QComboBox *m_cbBaud;
    QLineEdit *m_editBarcode;
    QLineEdit *m_editSerialRead;
    QTableWidget *m_tableRes;
    QPlainTextEdit *m_logView;

    QMap<QString, QString> m_currentIds;
    QMap<QString, int> m_mapResRow;

    qint64 m_lastAuthTime = 0;     // [新增] 用于记录最后一次收到 IMEI 的时间
    QFile *m_logFile = nullptr;
    QDate m_logDate; // [优化] 用于处理跨天日志分割

    // [优化] 通道独立持有 ConfigManager，消除配置串扰
    ConfigManager m_config;

    // ================= [新增] =================
    QTimer *m_reconnectTimer;     // 自动重连定时器
    
    QTimer *m_timeoutTimer;       // 超时机制定时器
    QLabel *m_lbStats;            // 统计展示标签
    QPushButton *m_btnResetStats; // 独立清零按钮
    
    int m_passCount = 0;
    int m_ngCount = 0;
    QHash<QString, bool> m_testHistory; // 履历追溯表
    // ==========================================
};

#endif // DEVICECHANNELWIDGET_H
