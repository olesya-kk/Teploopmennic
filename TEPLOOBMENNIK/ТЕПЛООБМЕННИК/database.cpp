#include "database.h"
#include "catalog.h"
#include <QSqlQuery>
#include <QSqlError>
#include <QJsonDocument>
#include <QJsonArray>
#include <QVariant>
#include <QUuid>
#include <stdexcept>
namespace {
void check(bool ok,const QSqlQuery& q) { if(!ok) throw std::runtime_error(q.lastError().text().toStdString()); }
QJsonObject propsJson(const FluidProps& p) { return {{"rho",p.density},{"mu",p.viscosity},{"cp",p.heatCapacity},{"lambda",p.conductivity}}; }
FluidProps propsFrom(const QJsonObject& o) { return {o["rho"].toDouble(),o["mu"].toDouble(),o["cp"].toDouble(),o["lambda"].toDouble()}; }
QJsonArray tableJson(const QVector<PropertyPoint>& table) { QJsonArray a; for(const auto& p:table) a.append(QJsonObject{{"t",p.temperature},{"p",propsJson(p.props)}}); return a; }
QVector<PropertyPoint> tableFrom(const QJsonArray& a) { QVector<PropertyPoint> t; for(auto v:a) {auto o=v.toObject(); t.push_back({o["t"].toDouble(),propsFrom(o["p"].toObject())});} return t; }
}
Database::Database(const QString& path) {
    db=QSqlDatabase::addDatabase("QSQLITE",QUuid::createUuid().toString()); db.setDatabaseName(path);
    if(!db.open()) throw std::runtime_error(db.lastError().text().toStdString());
    QSqlQuery q(db);
    for(const auto& sql:{
        "CREATE TABLE IF NOT EXISTS materials(name TEXT PRIMARY KEY, conductivity REAL, stress_mpa REAL, density REAL)",
        "CREATE TABLE IF NOT EXISTS fouling(name TEXT PRIMARY KEY, resistance REAL)",
        "CREATE TABLE IF NOT EXISTS properties(fluid TEXT, temperature REAL, rho REAL, mu REAL, cp REAL, lambda REAL, PRIMARY KEY(fluid,temperature))",
        "CREATE TABLE IF NOT EXISTS calculations(id INTEGER PRIMARY KEY, created TEXT DEFAULT (strftime('%Y-%m-%dT%H:%M:%fZ','now')), input TEXT NOT NULL, result TEXT NOT NULL)"}) check(q.exec(sql),q);
    check(q.exec("CREATE TABLE IF NOT EXISTS catalog_metadata(key TEXT PRIMARY KEY,value TEXT)"),q);
    check(q.exec("CREATE TABLE IF NOT EXISTS tube_catalog(id INTEGER PRIMARY KEY, geometry TEXT NOT NULL)"),q);
    if(!db.transaction()) throw std::runtime_error(db.lastError().text().toStdString());
    try {
        auto catalog=pakhtCatalog();
        q.prepare("SELECT value FROM catalog_metadata WHERE key='version'");check(q.exec(),q);
        bool migrate=!q.next() || q.value(0).toString()!="pakht-v2";
        if(migrate) {
            for(const auto& table:{"materials","fouling","properties","tube_catalog"}) check(q.exec("DELETE FROM "+QString(table)),q);
            check(q.exec("INSERT INTO materials VALUES ('Сталь 12Х18Н10Т',17.2,184,7850),('Сталь 20',46.5,147,7850)"),q);
            q.prepare("INSERT INTO properties VALUES (?,?,?,?,?,?)");
            auto fluids=catalog["fluids"].toObject();
            for(auto it=fluids.begin();it!=fluids.end();++it) for(auto point:it.value().toArray()) {
                auto a=point.toArray();q.bindValue(0,it.key());for(int i=0;i<5;++i)q.bindValue(i+1,a[i].toDouble());check(q.exec(),q);
            }
            q.prepare("INSERT INTO fouling VALUES (?,?)");
            for(auto value:catalog["fouling"].toArray()) {auto o=value.toObject();q.bindValue(0,o["name"].toString());q.bindValue(1,o["resistance"].toDouble());check(q.exec(),q);}
            q.prepare("INSERT INTO tube_catalog(geometry) VALUES (?)");
            for(auto g:catalog["geometry"].toArray()) {q.bindValue(0,QString::fromUtf8(QJsonDocument(g.toObject()).toJson(QJsonDocument::Compact)));check(q.exec(),q);}
            q.prepare("INSERT OR REPLACE INTO catalog_metadata VALUES (?,?)");
            QMap<QString,QString> metadata={{"version","pakht-v2"},{"source",catalog["source"].toString()},{"sha256",catalog["sha256"].toString()},
                {"fluids","ПАХТ: с.109–111,123–124,127–128; жидкая фаза; вода 0–100 °C"},
                {"fouling","ПАХТ с.136: R=1/среднее(проводимость); диапазоны сохранены в исходном JSON"},
                {"geometry","ПАХТ с.137: стандартные кожухотрубчатые аппараты; стенка 2 мм"},
                {"materials","λ: ПАХТ с.80–81; плотность: с.108; σ при 20 °C, ресурс 100000 ч: ГОСТ 34233.1-2017 А.1/А.3; не является прочностным расчётом"}};
            for(auto it=metadata.begin();it!=metadata.end();++it){q.bindValue(0,it.key());q.bindValue(1,it.value());check(q.exec(),q);}
        }
        if(!db.commit()) throw std::runtime_error(db.lastError().text().toStdString());
    } catch(...) {db.rollback(); throw;}
}
Database::~Database() {QString n=db.connectionName(); db.close(); db=QSqlDatabase(); QSqlDatabase::removeDatabase(n);}
QStringList Database::fluids() const {QSqlQuery q(db); check(q.exec("SELECT DISTINCT fluid FROM properties ORDER BY fluid"),q); QStringList a; while(q.next()) a<<q.value(0).toString(); return a;}
QVector<PropertyPoint> Database::fluidTable(const QString& name) const {
    QSqlQuery q(db); q.prepare("SELECT temperature,rho,mu,cp,lambda FROM properties WHERE fluid=? ORDER BY temperature"); q.addBindValue(name); check(q.exec(),q);
    QVector<PropertyPoint> a; while(q.next()) a.push_back({q.value(0).toDouble(),{q.value(1).toDouble(),q.value(2).toDouble(),q.value(3).toDouble(),q.value(4).toDouble()}}); return a;
}
QVector<Material> Database::materials() const {QSqlQuery q(db); check(q.exec("SELECT * FROM materials ORDER BY name"),q); QVector<Material> a; while(q.next()) a.push_back({q.value(0).toString(),q.value(1).toDouble(),q.value(2).toDouble(),q.value(3).toDouble()}); return a;}
QMap<QString,double> Database::foulings() const {QSqlQuery q(db); check(q.exec("SELECT * FROM fouling ORDER BY name"),q); QMap<QString,double> a; while(q.next()) a[q.value(0).toString()]=q.value(1).toDouble();return a;}
QJsonObject inputJson(const InputData& x) {
    QJsonObject o;
#define NUM(f) o[#f]=x.f;
    NUM(t_hot_in) NUM(t_hot_out) NUM(t_cold_in) NUM(t_cold_out) NUM(G_hot) NUM(G_cold)
    NUM(dp_max_hot) NUM(dp_max_cold) NUM(flowType) NUM(wallConductivity) NUM(foulHot) NUM(foulCold)
    NUM(tolerance) NUM(minimumMargin) NUM(maxIterations) NUM(threads) NUM(hotName) NUM(coldName) NUM(materialName)
#undef NUM
    o["geometryCatalog"]=x.geometryCatalog;
    o["props_hot"]=propsJson(x.props_hot); o["props_cold"]=propsJson(x.props_cold);
    o["hotTable"]=tableJson(x.hotTable);o["coldTable"]=tableJson(x.coldTable); return o;
}
InputData inputFromJson(const QJsonObject& o) {
    InputData x;
#define NUM(f) x.f=o[#f].toDouble();
    NUM(t_hot_in) NUM(t_hot_out) NUM(t_cold_in) NUM(t_cold_out) NUM(G_hot) NUM(G_cold)
    NUM(dp_max_hot) NUM(dp_max_cold) NUM(flowType) NUM(wallConductivity) NUM(foulHot) NUM(foulCold)
    NUM(tolerance) NUM(minimumMargin) NUM(maxIterations) NUM(threads)
#undef NUM
    x.geometryCatalog=o["geometryCatalog"].toArray();
    x.hotName=o["hotName"].toString();x.coldName=o["coldName"].toString();x.materialName=o["materialName"].toString();
    x.props_hot=propsFrom(o["props_hot"].toObject());x.props_cold=propsFrom(o["props_cold"].toObject());
    x.hotTable=tableFrom(o["hotTable"].toArray());x.coldTable=tableFrom(o["coldTable"].toArray());return x;
}
QJsonObject resultJson(const CalcResult& x) {
    QJsonObject o;
#define NUM(f) o[#f]=x.f;
    NUM(success) NUM(errorMsg) NUM(K) NUM(Q) NUM(dT_cp) NUM(correction) NUM(F_required) NUM(areaMargin)
    NUM(dp_hot) NUM(dp_cold) NUM(reHot) NUM(reCold) NUM(prHot) NUM(prCold) NUM(alphaHot) NUM(alphaCold)
    NUM(residual) NUM(elapsedMs) NUM(iterations) NUM(candidates) NUM(feasible) NUM(threadsUsed)
#undef NUM
    auto g=x.geometry;QJsonObject a;
#define NUM(f) a[#f]=g.f;
    NUM(id) NUM(tubeOuterDiameter) NUM(tubeWallThickness) NUM(tubeCount) NUM(tubeLength) NUM(tubePasses)
    NUM(shellPasses) NUM(shellDiameter) NUM(baffleSpacing) NUM(baffleCut) NUM(heatTransferArea) NUM(materialName) NUM(wallConductivity)
#undef NUM
    o["geometry"]=a;return o;
}
CalcResult resultFromJson(const QJsonObject& o) {
    CalcResult x;
#define NUM(f) x.f=o[#f].toDouble();
    NUM(K) NUM(Q) NUM(dT_cp) NUM(correction) NUM(F_required) NUM(areaMargin)
    NUM(dp_hot) NUM(dp_cold) NUM(reHot) NUM(reCold) NUM(prHot) NUM(prCold) NUM(alphaHot) NUM(alphaCold)
    NUM(residual) NUM(elapsedMs) NUM(iterations) NUM(candidates) NUM(feasible) NUM(threadsUsed)
#undef NUM
    x.success=o["success"].toBool(); x.errorMsg=o["errorMsg"].toString();auto a=o["geometry"].toObject();auto &g=x.geometry;
#define NUM(f) g.f=a[#f].toDouble();
    NUM(id) NUM(tubeOuterDiameter) NUM(tubeWallThickness) NUM(tubeCount) NUM(tubeLength) NUM(tubePasses)
    NUM(shellPasses) NUM(shellDiameter) NUM(baffleSpacing) NUM(baffleCut) NUM(heatTransferArea) NUM(wallConductivity)
#undef NUM
    if(!a.contains("shellPasses"))g.shellPasses=1;
    g.materialName=a["materialName"].toString(); return x;
}
int Database::save(const InputData& in,const CalcResult& r) {
    QSqlQuery q(db); q.prepare("INSERT INTO calculations(input,result) VALUES (?,?)");
    q.addBindValue(QString::fromUtf8(QJsonDocument(inputJson(in)).toJson(QJsonDocument::Compact)));
    q.addBindValue(QString::fromUtf8(QJsonDocument(resultJson(r)).toJson(QJsonDocument::Compact)));check(q.exec(),q);return q.lastInsertId().toInt();
}
QVector<QJsonObject> Database::history() const {
    QSqlQuery q(db);check(q.exec("SELECT id,created,input,result FROM calculations ORDER BY id DESC"),q);QVector<QJsonObject> a;
    while(q.next()) a.push_back({{"id",q.value(0).toInt()},{"created",q.value(1).toString()},
        {"input",QJsonDocument::fromJson(q.value(2).toByteArray()).object()}, {"result",QJsonDocument::fromJson(q.value(3).toByteArray()).object()}});return a;
}
QVector<QPair<QString,QString>> specification(const CalcResult& r,const InputData& in) {
    const auto& g=r.geometry; QVector<QPair<QString,QString>> a;
    auto add=[&](QString n,double v){a.push_back({n,QString::number(v,'g',9)});};
    add("Тепловая нагрузка, Вт",r.Q);add("Средний напор с поправкой, К",r.dT_cp);add("Поправка схемы F",r.correction);
    add("K по наружной площади, Вт/(м²·К)",r.K);add("Наружный диаметр труб, мм",g.tubeOuterDiameter*1000);
    add("Толщина стенки, мм",g.tubeWallThickness*1000);add("Число труб",g.tubeCount);add("Длина труб, м",g.tubeLength);
    add("Число трубных ходов",g.tubePasses);add("Число ходов кожуха",g.shellPasses);add("Диаметр кожуха, м",g.shellDiameter);
    add("Шаг перегородок, м",g.baffleSpacing);add("Срез перегородок, %",g.baffleCut);
    add("Площадь выбранная, м²",g.heatTransferArea);add("Площадь требуемая, м²",r.F_required);add("Запас площади, %",r.areaMargin);
    add("ΔP горячего (трубы), Па",r.dp_hot);add("ΔP холодного (кожух), Па",r.dp_cold);
    add("Запас давления горячего, %",100*(1-r.dp_hot/in.dp_max_hot));add("Запас давления холодного, %",100*(1-r.dp_cold/in.dp_max_cold));
    add("Re труб",r.reHot);add("Re кожуха",r.reCold);add("Pr труб",r.prHot);add("Pr кожуха",r.prCold);
    add("α труб, Вт/(м²·К)",r.alphaHot);add("α кожуха, Вт/(м²·К)",r.alphaCold);
    add("Итераций выбранного варианта",r.iterations);add("Относительная невязка K",r.residual);add("Кандидатов",r.candidates);add("Допустимых",r.feasible);
    add("Время подбора, мс",r.elapsedMs);add("Потоков OpenMP",r.threadsUsed);
    a.push_back({"Материал",g.materialName});a.push_back({"Горячая среда",in.hotName});a.push_back({"Холодная среда",in.coldName});return a;
}

QJsonArray Database::geometryCatalog() const {
    QSqlQuery q(db);check(q.exec("SELECT geometry FROM tube_catalog ORDER BY id"),q);QJsonArray a;
    while(q.next()) a.append(QJsonDocument::fromJson(q.value(0).toByteArray()).object());
    return a;
}
QMap<QString,QString> Database::metadata() const {
    QSqlQuery q(db);check(q.exec("SELECT key,value FROM catalog_metadata ORDER BY key"),q);QMap<QString,QString> a;
    while(q.next())a[q.value(0).toString()]=q.value(1).toString();
    return a;
}
