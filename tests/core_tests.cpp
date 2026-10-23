#include "heatcalc.h"
#include "correlations.h"
#include "database.h"
#include <QCoreApplication>
#include <QTemporaryDir>
#include <QSqlQuery>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
void near(double actual,double expected,double tol=1e-9) {require(std::abs(actual-expected)<=tol*std::max(1.,std::abs(expected)),"Numeric mismatch");}
int main(int argc,char** argv) {
    QCoreApplication app(argc,argv);
    try {
        near(Correlations::calcRe(2,.02,1000,.001),40000);
        near(Correlations::calcPr(4200,.001,.6),7);
        near(Correlations::calcAlpha(100,.6,.02),3000);
        near(Correlations::calcNuTubeTurbulent(10000,1),33.2827570417,1e-10);
        near(Correlations::calcNuShell(10000,1),54.2567469194,1e-10);
        near(Correlations::calcKFlatWall(1000,2000,.002,20,.0001,.0002),1/.0019);
        near(Correlations::calcKCylindricalWall(1000,2000,.02,.025,20,.0001,.0002),
            1/(.0005+.0002+.025/40*std::log(1.25)+.0011*1.25));
        require(Correlations::calcKCylindricalWall(1000,1000,.02,.01,20,0,0)==0,"Invalid wall");
        near(HeatCalculator::logMean(40,40),40);near(HeatCalculator::logMean(60,30),30/std::log(2.));
        require(HeatCalculator::logMean(0,30)==0,"Zero approach");
        QVector<PropertyPoint> table={{0,{1000,.002,4000,.5}},{100,{900,.001,4200,.7}}};
        auto p=propertiesAt(table,50);near(p.density,950);near(p.viscosity,.0015);near(p.heatCapacity,4100);near(p.conductivity,.6);
        bool threw=false;try {propertiesAt(table,101);}catch(...) {threw=true;}require(threw,"Extrapolation rejected");
        near(Correlations::calcNuShell(2488,5.976,5.976),44.82,0.001);
        near(Correlations::calcNuShell(100,1,1),3.36);
        InputData in;require(HeatCalculator::validate(in).isEmpty(),"Default valid");
        auto result=HeatCalculator::calculate(in);require(result.success,"Feasible default");near(result.Q,250800);near(result.dT_cp,40);
        require(result.areaMargin>=10&&result.dp_hot<=in.dp_max_hot&&result.dp_cold<=in.dp_max_cold,"Constraints");
        require(result.iterations>=2&&result.residual<=in.tolerance,"Convergence");
        in.threads=4;auto parallel=HeatCalculator::calculate(in);
        require(parallel.success&&parallel.geometry.id==result.geometry.id&&parallel.feasible==result.feasible,"Parallel determinism");near(parallel.K,result.K);
        auto bad=in;bad.t_cold_out=bad.t_hot_in;require(!HeatCalculator::calculate(bad).success,"Pinch rejected");
        bad=in;bad.G_cold=1;require(!HeatCalculator::calculate(bad).success,"Balance rejected");
        bad=in;bad.props_hot.viscosity=0;require(!HeatCalculator::calculate(bad).success,"Zero property rejected");
        bad=in;bad.G_hot=std::numeric_limits<double>::quiet_NaN();require(!HeatCalculator::calculate(bad).success,"NaN rejected");
        bad=in;bad.dp_max_hot=1e-9;require(!HeatCalculator::calculate(bad).success,"Impossible pressure");
        in.flowType=1;auto co=HeatCalculator::calculate(in);require(co.success,"Parallel flow feasible");near(co.dT_cp,60/std::log(7.));
        in.flowType=2;near(HeatCalculator::correctionFactor(in),.897944846831799,1e-9);
        auto twoShell=in;twoShell.t_hot_in=100;twoShell.t_hot_out=44;twoShell.t_cold_in=20;twoShell.t_cold_out=76;
        require(HeatCalculator::correctionFactor(twoShell,1)==0 && HeatCalculator::correctionFactor(twoShell,2)>0,"Two shell correction extends domain");
        auto geoms=HeatCalculator::generateCandidates(in);bool hasTwo=false,hasThree=false;
        for(auto g:geoms){hasTwo|=g.shellPasses==2;hasThree|=g.shellPasses==3;require(g.tubeWallThickness==.002,"Catalog wall");}
        require(hasTwo&&hasThree,"Shell passes searched");
        auto multi=HeatCalculator::calculate(in);require(multi.success&&multi.geometry.tubePasses>1,"Multipass geometry");
        bad=in;bad.t_hot_in=100;bad.t_hot_out=20;bad.t_cold_in=10;bad.t_cold_out=90;require(!HeatCalculator::calculate(bad).success,"Impossible 1-2 arrangement");
        QTemporaryDir temp;require(temp.isValid(),"Temp dir");
        {Database db(temp.filePath("test.sqlite"));require(db.materials().size()==2&&db.foulings().size()==12,"Catalogs");
            require(db.fluids().size()==30,"PDF fluid count");
            require(db.fluidTable("Вода").size()==101,"Water each degree");
            near(propertiesAt(db.fluidTable("Вода"),20).viscosity,.0010026);
            auto toluene=propertiesAt(db.fluidTable("Толуол"),20);
            near(toluene.density,866.9);near(toluene.heatCapacity,1691);
            near(toluene.conductivity,.134);near(toluene.viscosity,.000584);
            near(db.foulings()["Вода среднего качества"],1./2380);
            require(db.geometryCatalog().size()==176,"PDF geometry count");
            in=InputData{};in.geometryCatalog=db.geometryCatalog();in.hotTable=db.fluidTable("Вода");in.coldTable=in.hotTable;
            in.props_hot=propertiesAt(in.hotTable,75);in.props_cold=propertiesAt(in.coldTable,35);
            in.G_cold=in.G_hot*in.props_hot.heatCapacity/in.props_cold.heatCapacity;
            auto water=HeatCalculator::calculate(in);require(water.success&&water.iterations>2,"Wall iteration");
            auto tighter=in;tighter.tolerance=1e-8;auto tight=HeatCalculator::evaluateCandidate(tighter,water.geometry);require(tight.success,"Tight convergence");near(water.K,tight.K,1e-4);
            bad=in;bad.maxIterations=2;require(!HeatCalculator::calculate(bad).success,"Nonconvergence rejected");
            int id=db.save(in,water);auto history=db.history();require(history.size()==1&&history[0]["id"].toInt()==id,"Save");
            auto restored=inputFromJson(history[0]["input"].toObject());auto restoredResult=resultFromJson(history[0]["result"].toObject());
            require(inputJson(restored)==inputJson(in),"Input round trip");require(resultJson(restoredResult)==resultJson(water),"Result round trip");
            auto again=HeatCalculator::calculate(restored);require(again.success&&again.geometry.id==water.geometry.id,"Reload reproducible");
        }
        {
            auto legacy=QSqlDatabase::addDatabase("QSQLITE","migration-test");legacy.setDatabaseName(temp.filePath("test.sqlite"));require(legacy.open(),"Migration open");
            {QSqlQuery q(legacy);require(q.exec("DELETE FROM catalog_metadata WHERE key='version'"),"Force migration");require(q.exec("DELETE FROM properties"),"Old catalog reset");}
            legacy.close();
        }
        QSqlDatabase::removeDatabase("migration-test");
        {Database db(temp.filePath("test.sqlite"));require(db.history().size()==1,"Migration preserves history");require(db.fluidTable("Вода").size()==101,"Migration restores PDF catalog");require(db.metadata()["version"]=="pakht-v2","Versioned catalog");}
        std::cout<<"All core checks passed. Default Q="<<result.Q<<", K="<<result.K<<", area="<<result.geometry.heatTransferArea<<", candidates="<<result.candidates<<"\n";
    } catch(const std::exception& e) {std::cerr<<e.what()<<"\n";return 1;}
}
