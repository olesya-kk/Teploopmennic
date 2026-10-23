#pragma once
#include <QVector>
#include <stdexcept>
struct FluidProps {
    double density=997, viscosity=0.001, heatCapacity=4180, conductivity=0.6;
};
struct PropertyPoint { double temperature; FluidProps props; };
inline FluidProps propertiesAt(const QVector<PropertyPoint>& table, double t) {
    if (table.isEmpty()) throw std::runtime_error("Пустая таблица свойств");
    if(t<table.first().temperature || t>table.last().temperature)
        throw std::runtime_error("Температура вне диапазона справочника");
    for(int i=1;i<table.size();++i) if(t<=table[i].temperature) {
        const auto &a=table[i-1], &b=table[i];
        double f=(t-a.temperature)/(b.temperature-a.temperature);
        return {a.props.density+f*(b.props.density-a.props.density),
            a.props.viscosity+f*(b.props.viscosity-a.props.viscosity),
            a.props.heatCapacity+f*(b.props.heatCapacity-a.props.heatCapacity),
            a.props.conductivity+f*(b.props.conductivity-a.props.conductivity)};
    }
    return table.first().props;
}
