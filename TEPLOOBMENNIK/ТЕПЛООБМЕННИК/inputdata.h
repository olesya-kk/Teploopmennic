#pragma once
#include <QString>
#include <QJsonArray>
#include "fluidproperties.h"
struct InputData {
    double t_hot_in=90, t_hot_out=60, t_cold_in=20, t_cold_out=50;
    double G_hot=2, G_cold=2;
    FluidProps props_hot, props_cold;
    QVector<PropertyPoint> hotTable, coldTable;
    double dp_max_hot=100000, dp_max_cold=100000;
    QJsonArray geometryCatalog;
    int flowType=0; // 0 countercurrent, 1 parallel, 2 one shell / even tube passes
    QString hotName="Вручную", coldName="Вручную", materialName="Сталь 12Х18Н10Т";
    double wallConductivity=17.2, foulHot=1.0/2380, foulCold=1.0/2380;
    double tolerance=1e-5, minimumMargin=10;
    int maxIterations=100, threads=1;
};
