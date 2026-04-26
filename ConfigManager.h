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

enum TestType { Type_Match, Type_Range, Type_Exist, Type_NotMatch, Type_Display, Type_Toggle };

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

class ConfigManager {
public:
    // [优化] 设为公开构造函数，不再使用全局单例，每个通道自己持有一份配置规则
    ConfigManager() {}

    static QStringList getConfigFileList() {
        QDir dir("configs");
        QStringList filters;
        filters << "*.json";
        return dir.entryList(filters, QDir::Files | QDir::NoDotAndDotDot);
    }

    void loadConfig(const QString& fileName) {
        QString fullPath = QString("configs/%1").arg(fileName);
        QFile file(fullPath);
        if (!file.exists()) {
            fullPath = fileName;
            file.setFileName(fullPath);
        }

        if (!file.open(QIODevice::ReadOnly)) {
            qDebug() << "Config load failed: File not found.";
            return;
        }

        QByteArray data = file.readAll();
        file.close();

        QJsonParseError error;
        QJsonDocument doc = QJsonDocument::fromJson(data, &error);

        if (doc.isNull() || error.error != QJsonParseError::NoError) {
            QString strGBK = QString::fromLocal8Bit(data);
            doc = QJsonDocument::fromJson(strGBK.toUtf8(), &error);
            if(doc.isNull()) return;
        }

        QJsonObject root = doc.object();
        m_timeout = root.value("timeout").toInt(0); // 默认为0，表示不限制
        parseIdentityRules(root.value("identity_rules").toArray());
        parseTelemetryRules(root.value("telemetry_rules").toArray());
    }

    const QVector<IdentityRule>& getIdentityRules() const { return m_identities; }
    const QVector<TestRule>& getTelemetryRules() const { return m_telemetries; }
    int getTimeout() const { return m_timeout; }

private:
    int m_timeout = 0;
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
            } else if (typeStr == "not_match" || typeStr == "!=") {
                rule.type = Type_NotMatch;
                rule.targetVal = obj.value("target").toString();
                if(rule.targetVal.isEmpty() && obj.contains("target"))
                    rule.targetVal = QString::number(obj.value("target").toDouble());
            } else if (typeStr == "display" || typeStr == "show" || typeStr == "read") {
                rule.type = Type_Display;
            }
            // ================= [新增] 翻转测试类型 =================
            else if (typeStr == "toggle") {
                rule.type = Type_Toggle;
            }
            else {
                rule.type = Type_Match;
                rule.targetVal = obj.value("target").toString();
                if(rule.targetVal.isEmpty() && obj.contains("target"))
                    rule.targetVal = QString::number(obj.value("target").toDouble());
            }
            m_telemetries.append(rule);
        }
    }
};

#endif // CONFIGMANAGER_H
