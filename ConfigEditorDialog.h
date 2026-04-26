#ifndef CONFIGEDITORDIALOG_H
#define CONFIGEDITORDIALOG_H

#include <QDialog>
#include <QListWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QHeaderView>
#include <QComboBox>
#include <QMessageBox>
#include <QDir>
#include <QFile>
#include <QDebug>
#include <QCheckBox>
#include <QLabel>
#include <QSpinBox>

class ConfigEditorDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ConfigEditorDialog(QWidget *parent = nullptr);
    ~ConfigEditorDialog();

signals:
    void configSaved();

private slots:
    void loadFileList();
    void onFileSelected(QListWidgetItem *item);
    void createNewConfig();
    void deleteConfig();
    void saveConfig();

    void addIdentityRow();
    void addTelemetryRow();
    void removeIdentityRow();
    void removeTelemetryRow();
    void moveRowUp(QTableWidget *table);
    void moveRowDown(QTableWidget *table);

    void onTelemetryTypeChanged(int index, int row);

private:
    void setupUi();
    void clearEditors();
    void loadJsonToEditors(const QString &fileName);
    QJsonObject saveEditorsToJson();
    void moveRow(QTableWidget *table, int offset);

    QListWidget *m_listFiles;
    QLineEdit *m_editFileName;
    QTableWidget *m_tableIdentity;
    QTableWidget *m_tableTelemetry;
    QSpinBox *m_spinTimeout;

    QPushButton *m_btnSave;
    QString m_baseDir = "configs";
};

#endif // CONFIGEDITORDIALOG_H
