#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QList>
#include <QGridLayout>
#include <QSpinBox>
#include <QLabel>
#include <QToolBar>
#include <QMessageBox>
#include <QKeyEvent>
#include "DeviceChannelWidget.h" // 引用你的通道组件头文件

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    // [核心槽函数] 当通道数量设置改变时触发
    void onChannelCountChanged(int count);

private:
    QWidget *m_centralWidget;      // 中心部件
    QGridLayout *m_gridLayout;     // 网格布局管理器

    // [核心容器] 存储当前所有活跃的通道对象指针
    QList<DeviceChannelWidget*> m_channels;

    // 顶部工具栏控件
    QSpinBox *m_spinBoxCount;      // 用于设置通道数量

    // [新增] 扫码枪输入缓存
    QString m_scanBuffer;
};

#endif // MAINWINDOW_H
