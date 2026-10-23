#pragma once
#include <QSqlDatabase>
#include <QJsonObject>
#include <QJsonArray>
#include "heatcalc.h"
struct Material { QString name; double conductivity, stress, density; };
class Database {
    QSqlDatabase db;
public:
    explicit Database(const QString& path);
    ~Database();
    QStringList fluids() const;
    QVector<PropertyPoint> fluidTable(const QString& name) const;
    QVector<Material> materials() const;
    QMap<QString,double> foulings() const;
    QJsonArray geometryCatalog() const;
    QMap<QString,QString> metadata() const;
    int save(const InputData& in,const CalcResult& result);
    QVector<QJsonObject> history() const;
};
QJsonObject inputJson(const InputData& in);
InputData inputFromJson(const QJsonObject& json);
QJsonObject resultJson(const CalcResult& r);
CalcResult resultFromJson(const QJsonObject& json);
QVector<QPair<QString,QString>> specification(const CalcResult& r,const InputData& in);
