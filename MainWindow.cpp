#include "MainWindow.h"
#include <cmath>
#include <QtGlobal>
#include <QTimer>
#include <QMetaObject> // [新增] 支持底层的信号事件派发

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    resize(1280, 768);
    setWindowTitle("多通道产线并行测试系统 (Dynamic Grid)");

    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_gridLayout = new QGridLayout(m_centralWidget);
    m_gridLayout->setContentsMargins(4, 4, 4, 4);
    m_gridLayout->setSpacing(4);

    QToolBar *toolbar = addToolBar("Settings");
    toolbar->setMovable(false);

    QLabel *lblSet = new QLabel("当前检测通道数: ", this);
    lblSet->setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    toolbar->addWidget(lblSet);

    m_spinBoxCount = new QSpinBox(this);
    m_spinBoxCount->setRange(1, 9);
    m_spinBoxCount->setValue(2);
    m_spinBoxCount->setFont(QFont("Arial", 10));
    m_spinBoxCount->setMinimumWidth(80);
    m_spinBoxCount->setFocusPolicy(Qt::NoFocus);
    toolbar->addWidget(m_spinBoxCount);

    connect(m_spinBoxCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onChannelCountChanged);

    onChannelCountChanged(2);
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_channels);
    m_channels.clear();
}

void MainWindow::onChannelCountChanged(int count)
{
    for(auto channel : qAsConst(m_channels)) {
        if(channel && channel->isTesting()) {
            QMessageBox::warning(this, "操作禁止",
                                 "检测正在进行中！\n请先停止所有通道的测试，再调整通道数量。");

            m_spinBoxCount->blockSignals(true);
            m_spinBoxCount->setValue(m_channels.size());
            m_spinBoxCount->blockSignals(false);
            return;
        }
    }

    setUpdatesEnabled(false);
    qDeleteAll(m_channels);
    m_channels.clear();

    int cols = (int)std::ceil(std::sqrt(count));
    if (count == 2) cols = 2;

    for(int i = 0; i < count; i++) {
        DeviceChannelWidget *w = new DeviceChannelWidget(i + 1, this);

        connect(w, &DeviceChannelWidget::barcodeReturnPressed, this, &MainWindow::onChannelScanFinished);
        connect(w, &DeviceChannelWidget::barcodeCleared, this, &MainWindow::onChannelCleared);

        m_channels.append(w);

        int row = i / cols;
        int col = i % cols;
        m_gridLayout->addWidget(w, row, col);
    }

    setUpdatesEnabled(true);

    // [优化] 使用底层的事件队列投递（比固定写死毫秒数延时更稳）
    QMetaObject::invokeMethod(this, [=]() {
        if (!m_channels.isEmpty() && m_channels[0]->isBarcodeEmpty()) {
            m_channels[0]->setFocusToBarcode();
        }
    }, Qt::QueuedConnection);
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    if(event->key() != Qt::Key_Return && event->key() != Qt::Key_Enter) {
        // [优化] 增加防抖超时处理，防止误触键盘导致字符一直卡在缓冲区
        if (m_scanTimer.isValid() && m_scanTimer.elapsed() > 100) {
            m_scanBuffer.clear(); // 超过 100ms 没输入新的字符，判定为手误按键/乱码，丢弃
        }
        m_scanTimer.start(); // 重新开始计时
        m_scanBuffer.append(event->text());
        return;
    }

    QString code = m_scanBuffer.trimmed();
    m_scanBuffer.clear();
    if(code.isEmpty()) return;

    for(auto channel : qAsConst(m_channels)) {
        if(channel->checkScanInput(code) == ScanResult::Match) return;
    }

    for(int i = 0; i < m_channels.size(); ++i) {
        auto channel = m_channels[i];
        if(channel->hasFocusInBar()) {
            channel->checkScanInput(code);

            for(int j = 1; j < m_channels.size(); ++j) {
                int nextIdx = (i + j) % m_channels.size();
                if(m_channels[nextIdx]->isBarcodeEmpty()) {
                    m_channels[nextIdx]->setFocusToBarcode();
                    break;
                }
            }
            return;
        }
    }

    for(int i = 0; i < m_channels.size(); ++i) {
        auto channel = m_channels[i];
        if(channel->isBarcodeEmpty()) {
            channel->forceInjectBarcode(code);

            for(int j = 1; j < m_channels.size(); ++j) {
                int nextIdx = (i + j) % m_channels.size();
                if(m_channels[nextIdx]->isBarcodeEmpty()) {
                    // [优化] 取消死时间，用事件派发解决焦点切换异步问题
                    QMetaObject::invokeMethod(this, [=]() {
                        m_channels[nextIdx]->setFocusToBarcode();
                    }, Qt::QueuedConnection);
                    break;
                }
            }
            return;
        }
    }

    QMessageBox::warning(this, "扫码错误",
                         QString("扫描内容: [%1]\n未找到匹配的设备！\n\n请检查：\n1. 串口数据是否已读取？\n2. 是否扫描了错误的标签？").arg(code));
}

void MainWindow::onChannelScanFinished(int currentId) {
    int currentIndex = currentId - 1;
    int total = m_channels.size();

    for(int j = 1; j < total; ++j) {
        int nextIdx = (currentIndex + j) % total;

        if(m_channels[nextIdx]->isBarcodeEmpty()) {
            QMetaObject::invokeMethod(this, [=]() {
                m_channels[nextIdx]->setFocusToBarcode();
            }, Qt::QueuedConnection);
            break;
        }
    }
}

void MainWindow::onChannelCleared(bool isGlobal) {
    // [优化] 分辨是全局重压工装，还是某个通道私下重置
    if (isGlobal) {
        for(auto channel : qAsConst(m_channels)) {
            channel->clearBarcodeOnly();
        }
    }

    // 焦点回归
    QMetaObject::invokeMethod(this, [=]() {
        if(!m_channels.isEmpty()) {
            m_channels[0]->setFocusToBarcode();
        }
    }, Qt::QueuedConnection);
}
