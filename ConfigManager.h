#ifndef CONFIGMANAGER_H
#define CONFIGMANAGER_H

#include <QObject>
#include <QVector>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFile>
#include <QDebug>
#include <QFileInfo>
#include <QDir>

// --- 定义数据结构 ---
enum TestType { Type_Match, Type_Range, Type_Exist, Type_NotMatch ,Type_Display }; // <--- 新增 Type_NotMatch

struct IdentityRule {
    QString key;
    QString name;
    QString prefix;
    bool enable;
};

struct TestRule {
    QString key;
    QString name;
    TestType type;
    QString targetVal;
    double minVal;
    double maxVal;
    bool enable;
};

// --- 配置管理器类 ---
class ConfigManager {
public:
    static ConfigManager& instance() {
        static ConfigManager instance;
        return instance;
    }

    // === 新增：扫描 configs 文件夹下的所有 .json 文件 ===
    static QStringList getConfigFileList() {
        QDir dir("configs");
        // 只找 .json 结尾的文件
        QStringList filters;
        filters << "*.json";
        // 返回文件名列表 (不带路径)
        return dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    }

    // 加载指定文件 (传入文件名即可，函数内部拼接路径)
    void loadConfig(const QString& fileName) {
        // 自动补全路径: configs/文件名
        QString fullPath = QString("configs/%1").arg(fileName);

        QFile file(fullPath);
        // 如果 configs 目录下找不到，尝试在根目录找 (兼容旧版本)
        if (!file.exists()) {
            fullPath = fileName; // 回退到直接读取
            file.setFileName(fullPath);
        }

        QFileInfo info(file);
        qDebug() << "正在加载配置:" << info.absoluteFilePath();

        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Config load failed: File not found.";
            return;
        }

        QByteArray data = file.readAll();
        file.close();

        // --- 智能编码修复 (Win7 GBK 兼容) ---
        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (doc.isNull() || error.error != QJsonParseError::NoError) {
            qDebug() << "UTF-8 解析失败，尝试使用 Local8Bit (GBK) ...";
            QString strGBK = QString::fromLocal8Bit(data);
            doc = QJsonDocument::fromJson(strGBK.toUtf8(), &error);
            if(doc.isNull()) {
                qDebug() << "JSON 格式严重错误，请检查 config.json";
                return;
            }
        }

        QJsonObject root = doc.object();
        parseIdentityRules(root.value("identity_rules").toArray());
        parseTelemetryRules(root.value("telemetry_rules").toArray());

        qDebug() << "配置加载完成。ID规则数:" << m_identities.size()
                 << " 检测项数:" << m_telemetries.size();
    }

    QVector<IdentityRule> getIdentityRules() const { return m_identities; }
    QVector<TestRule> getTelemetryRules() const { return m_telemetries; }

private:
    QVector<IdentityRule> m_identities;
    QVector<TestRule> m_telemetries;

    void parseIdentityRules(const QJsonArray& arr) {
        m_identities.clear();
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            if (!obj.value("enable").toBool(true)) continue;
            m_identities.append({
                obj.value("key").toString(),
                obj.value("name").toString(),
                obj.value("prefix").toString(),
                true
            });
        }
    }

    void parseTelemetryRules(const QJsonArray& arr) {
        m_telemetries.clear();
        for (const auto& val : arr) {
            QJsonObject obj = val.toObject();
            if (!obj.value("enable").toBool(true)) continue;

            TestRule rule;
            rule.key = obj.value("key").toString();
            rule.name = obj.value("name").toString();
            rule.enable = true;

            QString typeStr = obj.value("type").toString();
            if (typeStr == "range") {
                rule.type = Type_Range;
                rule.minVal = obj.value("min").toDouble(-9999);
                rule.maxVal = obj.value("max").toDouble(9999);
            }
            // === 新增：不等于判断 ===
            else if (typeStr == "not_match" || typeStr == "!=") {
                rule.type = Type_NotMatch;
                rule.targetVal = obj.value("target").toString();
                // 兼容数字写法
                if(rule.targetVal.isEmpty() && obj.contains("target"))
                    rule.targetVal = QString::number(obj.value("target").toDouble());
            }
            // === [新增] 显示模式 ===
            else if (typeStr == "display" || typeStr == "show" || typeStr == "read") {
                rule.type = Type_Display;
                // 显示模式不需要 target/min/max，这里什么都不用读
            }
            // ======================
            else {
                // 默认为相等匹配 (match)
                rule.type = Type_Match;
                rule.targetVal = obj.value("target").toString();
                if(rule.targetVal.isEmpty() && obj.contains("target"))
                    rule.targetVal = QString::number(obj.value("target").toDouble());
            }
            m_telemetries.append(rule);
        }
    }

    ConfigManager() {}
    ConfigManager(const ConfigManager&) = delete;
    ConfigManager& operator=(const ConfigManager&) = delete;
};

#endif // CONFIGMANAGER_H
