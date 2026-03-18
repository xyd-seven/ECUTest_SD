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
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onChannelCountChanged(int count);
    void onChannelScanFinished(int currentId);
    void onChannelCleared(bool isGlobal); // [修改] 增加判定参数

private:
    QWidget *m_centralWidget;
    QGridLayout *m_gridLayout;

    QList<DeviceChannelWidget*> m_channels;

    QSpinBox *m_spinBoxCount;

    QString m_scanBuffer;
    QElapsedTimer m_scanTimer; // [新增] 用于防抖处理断帧的乱码
};

#endif // MAINWINDOW_H
