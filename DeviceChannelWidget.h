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

signals:
    void barcodeReturnPressed(int currentId);
    // [优化] 增加 bool 标志，区分是工装压下(全局清空) 还是 仅单一通道重置焦点
    void barcodeCleared(bool isGlobal);

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSerialReadyRead();
    void onBarcodeChanged(const QString &text);
    // ================= [新增] =================
    void onSerialError(QSerialPort::SerialPortError error); // 处理串口异常断开
    void tryReconnect();                                    // 定时尝试重连
    // ==========================================

private:
    void setupUi();
    void resetUI();
    void processBuffer();
    void parseLine(const QString &line);
    void parseTelemetry(const QString &dataPart);
    void updateResultItem(const QString &key, const QString &val);
    void performComparison();
    void setChannelStatus(bool active);
    void updateSerialDisplay();

private:
    int m_id;
    bool m_isTesting = false;
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

    qint64 m_lastResetTime = 0;
    QFile *m_logFile = nullptr;
    QDate m_logDate; // [优化] 用于处理跨天日志分割

    // [优化] 通道独立持有 ConfigManager，消除配置串扰
    ConfigManager m_config;

    // ================= [新增] =================
    QTimer *m_reconnectTimer;     // 自动重连定时器
    // ==========================================
};

#endif // DEVICECHANNELWIDGET_H
