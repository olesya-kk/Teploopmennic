#include "catalog.h"
#include <QFile>
#include <QJsonDocument>
#include <stdexcept>
static void initCatalog() { Q_INIT_RESOURCE(catalog); }
QJsonObject pakhtCatalog() {
    static const QJsonObject data=[] {
        initCatalog();QFile file(":/catalog/pakht.json");
        if(!file.open(QIODevice::ReadOnly)) throw std::runtime_error("Не найден справочник ПАХТ");
        auto doc=QJsonDocument::fromJson(file.readAll());
        if(!doc.isObject()) throw std::runtime_error("Повреждён справочник ПАХТ");
        return doc.object();
    }();
    return data;
}
