#include "database.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <omp.h>
#include <algorithm>
#include <iostream>
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);QTemporaryDir temp;Database db(temp.filePath("bench.sqlite"));
    InputData in;in.geometryCatalog=db.geometryCatalog();in.hotTable=db.fluidTable("Вода");in.coldTable=in.hotTable;
    in.props_hot=propertiesAt(in.hotTable,75);in.props_cold=propertiesAt(in.coldTable,35);
    in.G_cold=in.G_hot*in.props_hot.heatCapacity/in.props_cold.heatCapacity;
    in.flowType=2;
    std::cout<<"threads,median_ms,speedup,candidates,feasible,id,K,iterations,residual\n";
    double baseline=0;int selected=-1;
    for(int n:{1,2,4}) {
        in.threads=n;HeatCalculator::calculate(in);std::vector<double> times;CalcResult r;
        for(int i=0;i<9;++i) {r=HeatCalculator::calculate(in);if(!r.success)return 1;times.push_back(r.elapsedMs);}
        if(selected!=-1&&selected!=r.geometry.id)return 2;selected=r.geometry.id;
        std::sort(times.begin(),times.end());double ms=times[times.size()/2];if(n==1)baseline=ms;
        std::cout<<r.threadsUsed<<","<<ms<<","<<baseline/ms<<","<<r.candidates<<","<<r.feasible<<","<<r.geometry.id<<","<<r.K<<","<<r.iterations<<","<<r.residual<<"\n";
    }
    std::cout<<"\ntolerance,iterations,residual,K,id\n";
    in.threads=1;
    for(double eps:{1e-3,1e-5,1e-7}) {in.tolerance=eps;auto r=HeatCalculator::calculate(in);if(!r.success)return 3;
        std::cout<<eps<<","<<r.iterations<<","<<r.residual<<","<<r.K<<","<<r.geometry.id<<"\n";}
}
