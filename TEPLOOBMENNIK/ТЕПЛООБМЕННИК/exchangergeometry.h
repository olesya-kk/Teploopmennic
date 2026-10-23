#ifndef EXCHANGERGEOMETRY_H
#define EXCHANGERGEOMETRY_H

#include <QString>

// Эта структура описывает ОДИН конкретный вариант теплообменника.
struct ExchangerGeometry {
    int shellPasses=1;
    int id;                         // Условный номер варианта (для вывода)
    
    // Параметры труб
    double tubeOuterDiameter;       // d_н, м (например, 0.025)
    double tubeWallThickness;       // δ, м (например, 0.002)
    int tubeCount;                  // n, общее число труб в аппарате
    double tubeLength;              // L, м (например, 3.0)
    int tubePasses;                 // z, число ходов по трубному пространству (1, 2, 4, 6)

    // Параметры кожуха и перегородок
    double shellDiameter;           // D_кожуха, м
    double baffleSpacing;           // Шаг перегородок, м (например, 0.3)
    double baffleCut;               // Срез перегородок, % (например, 25)

    // Производные параметры (заполняются при генерации или расчете)
    double heatTransferArea;        // F_геом, м2 (внешняя площадь)
    
    // Материал стенки (из БД)
    QString materialName;           // "Сталь 12Х18Н10Т"
    double wallConductivity;        // λ_ст, Вт/(м*К)
};

#endif // EXCHANGERGEOMETRY_H