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
#include <QDateTime> // 用于防抖时间戳
#include <QFile>
#include <QDir>

// 假设 ConfigManager 已经定义在其他地方，这里前置声明或包含
#include "ConfigManager.h"

// 在类外部定义一个状态枚举
enum class ScanResult {
    Match,      // 匹配成功 (Green)
    Mismatch,   // 就在这个通道里，但是对不上 (Red)
    Ignore      // 不是这个通道的事 (Pass)
};

class DeviceChannelWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DeviceChannelWidget(int id, QWidget *parent = nullptr);
    ~DeviceChannelWidget();

    // 供主窗口查询状态，防止测试中途误删
    bool isTesting() const { return m_isTesting; }

    // 修改函数签名，返回枚举
    ScanResult checkScanInput(const QString &code);
    // 获取当前是否拥有焦点
    bool hasFocusInBar() const {
        return m_editBarcode->hasFocus();
    }

private slots:
    void onStartClicked();
    void onStopClicked();
    void onSerialReadyRead();
    void onBarcodeChanged(const QString &text);

private:
    void setupUi();
    void resetUI();

    // 核心解析函数
    void processBuffer();
    void parseLine(const QString &line);
    void parseTelemetry(const QString &dataPart);

    // UI 更新辅助
    void updateResultItem(const QString &key, const QString &val);
    void performComparison();
    void setChannelStatus(bool active);

    // [新增] 动态更新串口读取显示框 (拼凑字符串并比对)
    void updateSerialDisplay();

private:
    int m_id;
    bool m_isTesting = false;
    bool m_hasError = false;

    QSerialPort *m_serial;
    QByteArray m_buffer;

    // --- UI 控件 ---
    QGroupBox *m_group;
    QComboBox *m_cbModel;
    QComboBox *m_cbPort;
    QComboBox *m_cbBaud;

    QLineEdit *m_editBarcode;     // 扫码枪输入框
    QLineEdit *m_editSerialRead;  // [新增] 串口读取内容显示框 (只读)

    QTableWidget *m_tableRes;     // 右侧结果表格
    QPlainTextEdit *m_logView;    // 底部日志窗口

    // --- 数据映射 ---
    // [修改] 使用 Map 动态存储读取到的身份信息 (Key: imei/mac, Value: xxxx)
    QMap<QString, QString> m_currentIds;

    // 结果表映射: Key -> 表格中的索引位置
    QMap<QString, int> m_mapResRow;

    // --- 状态控制 ---
    qint64 m_lastResetTime = 0;   // 上次重置的时间戳 (用于防抖)

    // [新增] 日志文件对象
    QFile *m_logFile = nullptr;
};

#endif // DEVICECHANNELWIDGET_H
