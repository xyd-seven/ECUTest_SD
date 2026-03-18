#include "MainWindow.h"
#include <cmath> //用于计算平方根 ceil/sqrt
#include <QtGlobal>
#include <QTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 1. 设置主窗口基本属性
    resize(1280, 768);
    setWindowTitle("多通道产线并行测试系统 (Dynamic Grid)");

    // 2. 初始化中心部件和网格布局
    m_centralWidget = new QWidget(this);
    setCentralWidget(m_centralWidget);

    m_gridLayout = new QGridLayout(m_centralWidget);
    m_gridLayout->setContentsMargins(4, 4, 4, 4); // 边距留白
    m_gridLayout->setSpacing(4);                  // 通道间的间隙

    // 3. 创建顶部工具栏 (提供修改通道数量的入口)
    QToolBar *toolbar = addToolBar("Settings");
    toolbar->setMovable(false);

    QLabel *lblSet = new QLabel("当前检测通道数: ", this);
    lblSet->setFont(QFont("Microsoft YaHei", 10, QFont::Bold));
    toolbar->addWidget(lblSet);

    // [设置控件] 数字微调框 (范围 1 ~ 9)
    m_spinBoxCount = new QSpinBox(this);
    m_spinBoxCount->setRange(1, 9);
    m_spinBoxCount->setValue(2);    // 默认启动时为 4 通道
    m_spinBoxCount->setFont(QFont("Arial", 10));
    m_spinBoxCount->setMinimumWidth(80);
    // [新增] 禁止微调框获取键盘焦点 (只能用鼠标点上下箭头修改通道数)
    m_spinBoxCount->setFocusPolicy(Qt::NoFocus);
    toolbar->addWidget(m_spinBoxCount);

    // 4. 连接信号: 当数字改变时，执行重建逻辑
    // 使用 lambda 表达式稍微延迟一下，或者直接连接
    connect(m_spinBoxCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onChannelCountChanged);

    // 5. 初始化默认界面
    onChannelCountChanged(2);
}

MainWindow::~MainWindow()
{
    qDeleteAll(m_channels);
    // Qt 的对象树机制会自动清理 m_centralWidget 及其子对象
    // 但为了保险，我们可以手动清空列表
    m_channels.clear();
}

// ====================================================================
// [核心逻辑] 动态重建界面
// ====================================================================
void MainWindow::onChannelCountChanged(int count)
{
    // --- A. 安全检查 ---
    // 遍历当前所有通道，如果有任何一个正在测试，禁止修改布局
    // 防止测试到一半界面没了，导致串口悬空或数据丢失
    for(auto channel : qAsConst(m_channels)) {
        if(channel && channel->isTesting()) {
            QMessageBox::warning(this, "操作禁止",
                                 "检测正在进行中！\n请先停止所有通道的测试，再调整通道数量。");

            // 把 SpinBox 的值拨回去 (暂时阻断信号防止递归)
            m_spinBoxCount->blockSignals(true);
            m_spinBoxCount->setValue(m_channels.size());
            m_spinBoxCount->blockSignals(false);
            return;
        }
    }

    // --- B. 清理旧现场 ---
    // 1. 锁住界面更新，防止闪烁
    setUpdatesEnabled(false);

    // 2. 删除所有旧的通道对象
    // qDeleteAll 会调用每个对象的析构函数(自动关闭串口)
    qDeleteAll(m_channels);
    m_channels.clear();

    // 3. 此时 m_gridLayout 里还有指向已删除对象的悬空指针吗？
    // 调用 delete item 时，Qt 的 Layout 通常会自动感知并移除。
    // 但为了稳健，我们可以显式清空 Layout 里的所有 Item (虽然 qDeleteAll 已经析构了 widget)
    // 注意：这里不用再 delete widget，因为上面已经 delete 过了。

    // --- C. 创建新通道并计算布局 ---

    // 算法: 计算最佳列数
    // 如果 count=2, sqrt(2)=1.41 -> 2列 (1x2)
    // 如果 count=4, sqrt(4)=2.00 -> 2列 (2x2)
    // 如果 count=6, sqrt(6)=2.44 -> 3列 (2x3)
    int cols = (int)std::ceil(std::sqrt(count));
    // 修正: 为了宽屏显示优化，当数量少时，强制最大列数
    if (count == 2) cols = 2;

    for(int i = 0; i < count; i++) {
        // 1. 创建新对象 (ID 从 1 开始)
        DeviceChannelWidget *w = new DeviceChannelWidget(i + 1, this);

        // [新增代码] 监听通道内的扫码回车动作
        connect(w, &DeviceChannelWidget::barcodeReturnPressed, this, &MainWindow::onChannelScanFinished);
        // [新增] 监听通道被清空的信号
        connect(w, &DeviceChannelWidget::barcodeCleared, this, &MainWindow::onChannelCleared);

        // 2. 存入列表管理
        m_channels.append(w);

        // 3. 计算网格坐标
        int row = i / cols; // 行号
        int col = i % cols; // 列号

        // 4. 加入布局
        m_gridLayout->addWidget(w, row, col);
    }

    // --- D. 恢复界面更新 ---
    setUpdatesEnabled(true);
    // [新增保险1]：每次重建网格后，强制让第 1 个通道获取焦点
    if (!m_channels.isEmpty()) {
        // 使用一个极短的延时，确保 UI 渲染完成后再抢夺焦点
        QTimer::singleShot(50, this, [=]() {
            if (m_channels[0]->isBarcodeEmpty()) {
                m_channels[0]->setFocusToBarcode();
            }
        });
    }
}

void MainWindow::keyPressEvent(QKeyEvent *event) {
    // 1. 缓冲扫码枪输入 (模拟键盘)
    if(event->key() != Qt::Key_Return && event->key() != Qt::Key_Enter) {
        m_scanBuffer.append(event->text());
        return;
    }

    QString code = m_scanBuffer.trimmed();
    m_scanBuffer.clear(); // 清空缓存
    if(code.isEmpty()) return;

    // --- 三级判定逻辑开始 ---


    // 1. 第一轮遍历：寻找【完美匹配者】
    // 优先照顾“对”的设备，防止误伤
    for(auto channel : qAsConst(m_channels)) {
        if(channel->checkScanInput(code) == ScanResult::Match) {
            // 找到了！通道会自动变绿
            // 可以在这里加个成功提示音
            return;
        }
    }

    // 2. 第二轮遍历：寻找【焦点持有者】
    // 如果没有完美匹配，检查用户当前鼠标是不是点在某个框里
    for(int i = 0; i < m_channels.size(); ++i) {
        auto channel = m_channels[i];
        if(channel->hasFocusInBar()) {
            channel->checkScanInput(code); // 强塞给这个通道

            // 【核心修复】：扫码填入后，自动把光标跳到下一个“还为空”的通道
            for(int j = 1; j < m_channels.size(); ++j) {
                int nextIdx = (i + j) % m_channels.size();
                if(m_channels[nextIdx]->isBarcodeEmpty()) {
                    m_channels[nextIdx]->setFocusToBarcode(); // 焦点顺延
                    break;
                }
            }
            return;
        }
    }

    // 3. 第三轮(新增体验优化)：如果都没匹配，工人也没有鼠标点击任何框
    // 智能降级：自动寻找顺位的“第一个空框”塞进去
    for(int i = 0; i < m_channels.size(); ++i) {
        auto channel = m_channels[i];
        if(channel->isBarcodeEmpty()) {

            // 【核心修复1】：不再调用 checkScanInput，直接强行写入条码，绕过底层的异步焦点判定！
            channel->forceInjectBarcode(code);

            // 填完后，寻找下一个空位
            for(int j = 1; j < m_channels.size(); ++j) {
                int nextIdx = (i + j) % m_channels.size();
                if(m_channels[nextIdx]->isBarcodeEmpty()) {

                    // 【核心修复2】：加上 10 毫秒延时，确保系统有足够时间平滑转移焦点
                    QTimer::singleShot(10, this, [=]() {
                        m_channels[nextIdx]->setFocusToBarcode();
                    });
                    break;
                }
            }
            return;
        }
    }

    // 4. 第四轮：全局孤儿警告
    // 既没有匹配的，用户也没指定通道(没聚焦)
    QMessageBox::warning(this, "扫码错误",
                         QString("扫描内容: [%1]\n未找到匹配的设备！\n\n请检查：\n1. 串口数据是否已读取？\n2. 是否扫描了错误的标签？").arg(code));
}

// ====================================================================
// [新增逻辑] 捕获通道内扫码完成动作，寻找下一个空位并跳转焦点
// ====================================================================
void MainWindow::onChannelScanFinished(int currentId) {
    // 传入的 currentId 是从 1 开始的，转为数组索引 (-1)
    int currentIndex = currentId - 1;
    int total = m_channels.size();

    // 往后遍历，寻找下一个没填条码的空框
    for(int j = 1; j < total; ++j) {
        int nextIdx = (currentIndex + j) % total;

        if(m_channels[nextIdx]->isBarcodeEmpty()) {
            // [关键技巧] 使用 QTimer::singleShot 异步延时 10 毫秒设置焦点。
            // 原因是 QLineEdit 处理回车键的底层事件还没结束，如果立刻抢走焦点会导致光标乱闪或失效。
            QTimer::singleShot(10, this, [=]() {
                m_channels[nextIdx]->setFocusToBarcode();
            });
            break; // 找到一个空位跳过去就停止
        }
    }
}

// ====================================================================
// [修改逻辑] 当任意一个通道触发重新检测时，全局清空所有扫码框
// ====================================================================
void MainWindow::onChannelCleared() {
    // 1. 工装同时压下，视为整批测试开始。直接遍历清空所有通道的扫码框！
    for(auto channel : qAsConst(m_channels)) {
        channel->clearBarcodeOnly();
    }

    // 2. 延时 20 毫秒（等待所有并发的串口数据处理完），强制把光标拉回【通道 1】
    QTimer::singleShot(20, this, [=]() {
        if(!m_channels.isEmpty()) {
            m_channels[0]->setFocusToBarcode(); // 永远从通道 1 开始新一轮扫码
        }
    });
}
