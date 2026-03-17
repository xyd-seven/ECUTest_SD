#include "MainWindow.h"
#include <cmath> //用于计算平方根 ceil/sqrt
#include <QtGlobal>

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
    m_spinBoxCount->setValue(4);    // 默认启动时为 4 通道
    m_spinBoxCount->setFont(QFont("Arial", 10));
    m_spinBoxCount->setMinimumWidth(80);
    toolbar->addWidget(m_spinBoxCount);

    // 4. 连接信号: 当数字改变时，执行重建逻辑
    // 使用 lambda 表达式稍微延迟一下，或者直接连接
    connect(m_spinBoxCount, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &MainWindow::onChannelCountChanged);

    // 5. 初始化默认界面
    onChannelCountChanged(4);
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
    for(auto channel : qAsConst(m_channels)) {
        if(channel->hasFocusInBar()) {
            // 强行把这个错码塞给这个通道，让它变红报错
            channel->checkScanInput(code);
            // 可以在这里加个失败报警音
            return;
        }
    }

    // 3. 第三轮：全局孤儿警告
    // 既没有匹配的，用户也没指定通道(没聚焦)
    QMessageBox::warning(this, "扫码错误",
                         QString("扫描内容: [%1]\n未找到匹配的设备！\n\n请检查：\n1. 串口数据是否已读取？\n2. 是否扫描了错误的标签？").arg(code));
}
