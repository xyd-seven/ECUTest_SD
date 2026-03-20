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
#include <QElapsedTimer>
#include "DeviceChannelWidget.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

protected:
    // [新增] 全局事件过滤器，用于拦截整个软件的键盘输入
    bool eventFilter(QObject *watched, QEvent *event) override;

private slots:
    void onChannelCountChanged(int count);
    void onChannelScanFinished(int currentId);
    void onChannelCleared(bool isGlobal); // [修改] 增加判定参数
    void processScanBuffer();

private:
    QWidget *m_centralWidget;
    QGridLayout *m_gridLayout;

    QList<DeviceChannelWidget*> m_channels;

    QSpinBox *m_spinBoxCount;

    QString m_scanBuffer;
    QTimer *m_scanProcessTimer; // [修改] 改为 QTimer 延时处理
};

#endif // MAINWINDOW_H
