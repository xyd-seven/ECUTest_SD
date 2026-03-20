#include "MainWindow.h"
#include <cmath>
#include <QtGlobal>
#include <QTimer>
#include <QMetaObject> // [新增] 支持底层的信号事件派发
#include <QApplication>

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

    // --- 新增：注册全局拦截器与定时器 ---
    qApp->installEventFilter(this);
    m_scanProcessTimer = new QTimer(this);
    m_scanProcessTimer->setSingleShot(true);
    connect(m_scanProcessTimer, &QTimer::timeout, this, &MainWindow::processScanBuffer);

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

// ====================================================================
// [终极拦截器] 拦截软件内的所有键盘输入，将多行条码揉成单行
// ====================================================================
bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    // 安全机制：如果弹出了错误提示框，不要拦截键盘，让用户能按回车关闭提示框
    if (qApp->activeWindow() != this) {
        return QMainWindow::eventFilter(watched, event);
    }

    if (event->type() == QEvent::KeyPress) {
        QKeyEvent *ke = static_cast<QKeyEvent *>(event);

        // 忽略 Shift/Ctrl 等修饰键
        if (ke->key() == Qt::Key_Shift || ke->key() == Qt::Key_Control || ke->key() == Qt::Key_Alt) {
            return false;
        }
        // 在头部定义一个统一的时间常量（比如拉长到 250 毫秒）
        const int SCAN_TIMEOUT = 300;
        // 核心：如果扫码枪发出了“回车”或“换行”，强行把它替换成“空格”！
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            m_scanBuffer.append(" ");
            m_scanProcessTimer->start(SCAN_TIMEOUT); // 使用常量
            return true; // 吞掉这个回车，不让底层的输入框知道！
        }
        // 收集普通字符
        else if (!ke->text().isEmpty()) {
            m_scanBuffer.append(ke->text());
            m_scanProcessTimer->start(SCAN_TIMEOUT); // 使用常量
            return true; // 吞掉字符
        }
    }
    return QMainWindow::eventFilter(watched, event);
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
    if (isGlobal) { // [新增判定] 只有在工装整体重置时，才清空全部并抢回焦点
        for(auto channel : qAsConst(m_channels)) {
            channel->clearBarcodeOnly();
        }

        // 焦点回归第一通道
        QMetaObject::invokeMethod(this, [=]() {
            if(!m_channels.isEmpty()) {
                m_channels[0]->setFocusToBarcode();
            }
        }, Qt::QueuedConnection);
    }
}
// ====================================================================
// [条码分发] 定时器 150ms 超时，说明一长串多行条码彻底扫完了
// ====================================================================
void MainWindow::processScanBuffer() {
    QString code = m_scanBuffer.simplified();
    m_scanBuffer.clear();
    if(code.isEmpty()) return;

    // =================================================================
    // 【核心修正】：彻底删除“全局完美匹配”！
    // 强制按照物理光标顺序录入。就算壳子装反了，也要强行塞进当前通道，
    // 让底层界面爆红报警，暴露装配错误！
    // =================================================================

    // 1. 寻找当前持有光标的通道 (严格按物理顺序强行写入)
    for(int i = 0; i < m_channels.size(); ++i) {
        auto channel = m_channels[i];
        if(channel->hasFocusInBar()) {
            channel->forceInjectBarcode(code); // 强行塞入

            // 写入完毕，光标自动跳到下一个空位
            for(int j = 1; j < m_channels.size(); ++j) {
                int nextIdx = (i + j) % m_channels.size();
                if(m_channels[nextIdx]->isBarcodeEmpty()) {
                    QMetaObject::invokeMethod(this, [=]() {
                        m_channels[nextIdx]->setFocusToBarcode();
                    }, Qt::QueuedConnection);
                    break;
                }
            }
            return;
        }
    }

    // 2. 智能找空位 (如果因为意外没有光标，按顺序填第一个空坑)
    for(int i = 0; i < m_channels.size(); ++i) {
        auto channel = m_channels[i];
        if(channel->isBarcodeEmpty()) {
            channel->forceInjectBarcode(code);

            for(int j = 1; j < m_channels.size(); ++j) {
                int nextIdx = (i + j) % m_channels.size();
                if(m_channels[nextIdx]->isBarcodeEmpty()) {
                    QMetaObject::invokeMethod(this, [=]() {
                        m_channels[nextIdx]->setFocusToBarcode();
                    }, Qt::QueuedConnection);
                    break;
                }
            }
            return;
        }
    }

    // 3. 全局报错 (所有通道的码都扫满了，工人还多扫了一下)
    QMessageBox::warning(this, "扫码越界",
                         QString("扫描内容: [%1]\n当前所有通道均已录入条码！\n请先清空或完成测试后再扫码。").arg(code));
}
