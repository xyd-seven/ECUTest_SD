#ifndef TESTDEFS_H
#define TESTDEFS_H

#include <QString>

enum CheckType {
    Type_Match, // 精确匹配 (字符串)
    Type_Range, // 数值范围
    Type_Exist  // 非空即可
};

// 遥测规则 (对应 $info 后面的数据)
struct TestRule {
    QString key;
    QString name;
    CheckType type;
    double minVal;
    double maxVal;
    QString targetVal;
    bool enable;
};

// 身份规则 (对应 IMEI:, MAC: 等前缀数据)
struct IdentityRule {
    QString key;
    QString name;
    QString prefix;
    bool enable;
};

#endif // TESTDEFS_H
