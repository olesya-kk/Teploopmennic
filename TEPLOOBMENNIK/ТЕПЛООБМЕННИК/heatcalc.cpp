#include "heatcalc.h"
#include "correlations.h"
#include "catalog.h"
#include <QJsonArray>
#include <cmath>
#include <algorithm>
#include <chrono>
#include <omp.h>
namespace { constexpr double pi=3.14159265358979323846;
bool positive(double v) { return std::isfinite(v) && v>0; }
bool validProps(const FluidProps& p) { return positive(p.density)&&positive(p.viscosity)&&positive(p.heatCapacity)&&positive(p.conductivity); }
double friction(double re) { return re<2300 ? 64/re : 0.3164/std::pow(re,0.25); }
double tubeNu(double re,double pr,double pw) {
    if(re<2300) return 3.66;
    const double turbulent=Correlations::calcNuTubeTurbulent(re,pr,pw);
    if(re>=10000) return turbulent;
    // Smooth engineering interpolation across transition; not a validated correlation.
    double f=(re-2300)/7700;
    return 3.66*(1-f)+turbulent*f;
}
}
double HeatCalculator::logMean(double a,double b) {
    if(!positive(a)||!positive(b)) return 0;
    if(std::abs(a-b)<1e-8*std::max(a,b)) return (a+b)/2;
    return (a-b)/std::log(a/b);
}
double HeatCalculator::correctionFactor(const InputData& x,int shellPasses) {
    if(x.flowType!=2) return 1;
    double r=(x.t_hot_in-x.t_hot_out)/(x.t_cold_out-x.t_cold_in);
    double p=(x.t_cold_out-x.t_cold_in)/(x.t_hot_in-x.t_cold_in);
    double globalP=p;
    if(shellPasses<1)return 0;
    if(std::abs(r-1)<1e-7) p=p/(shellPasses-(shellPasses-1)*p);
    else {
        if(1-r*p<=0||1-p<=0)return 0;
        double w=std::pow((1-r*p)/(1-p),1.0/shellPasses);
        p=(1-w)/(r-w);
    }
    double s=std::sqrt(r*r+1), a=2-p*(r+1-s), b=2-p*(r+1+s);
    if(a<=0 || b<=0 || 1-p<=0 || 1-r*p<=0) return 0;
    double denominator=std::log(a/b);
    double factor=std::abs(r-1)<1e-7 ? std::sqrt(2.0)*p/((1-p)*denominator)
        : s*std::log((1-p)/(1-r*p))/((r-1)*denominator);
    double cellMean=logMean(1-p,1-r*p);
    double globalMean=logMean(1-globalP,1-r*globalP);
    factor*=globalP/(shellPasses*p)*cellMean/globalMean;
    return std::isfinite(factor)&&factor>0&&factor<=1.000001 ? std::min(1.0,factor) : 0;
}
QString HeatCalculator::validate(const InputData& x) {
    for(double t:{x.t_hot_in,x.t_hot_out,x.t_cold_in,x.t_cold_out})
        if(!std::isfinite(t)) return "Некорректная температура";
    if(!(x.t_hot_in>x.t_hot_out && x.t_cold_out>x.t_cold_in)) return "Горячий поток должен охлаждаться, холодный — нагреваться";
    if(x.t_hot_in<=x.t_cold_out || x.t_hot_out<=x.t_cold_in) return "Нулевой или отрицательный температурный напор";
    if(x.flowType<0||x.flowType>2) return "Неизвестная схема течения";
    if(x.flowType==1 && x.t_hot_out<=x.t_cold_out) return "Для прямотока температура холодного на выходе должна быть ниже горячего";
    if(!validProps(x.props_hot)||!validProps(x.props_cold)||!positive(x.G_hot)||!positive(x.G_cold)) return "Расходы и свойства должны быть положительными";
    if(!positive(x.dp_max_hot)||!positive(x.dp_max_cold)||!positive(x.wallConductivity)) return "Задайте положительные пределы давления и теплопроводность стенки";
    if(!std::isfinite(x.foulHot)||!std::isfinite(x.foulCold)||x.foulHot<0||x.foulCold<0) return "Некорректные загрязнения";
    if(!positive(x.tolerance)||x.tolerance>=1||x.maxIterations<2||x.threads<1||!std::isfinite(x.minimumMargin)||x.minimumMargin<0) return "Некорректные параметры подбора";
    double qh=x.G_hot*x.props_hot.heatCapacity*(x.t_hot_in-x.t_hot_out);
    double qc=x.G_cold*x.props_cold.heatCapacity*(x.t_cold_out-x.t_cold_in);
    if(!positive(qh)||!positive(qc)||std::abs(qh-qc)/std::max(qh,qc)>0.05) return QString("Тепловой баланс не сходится: Q горячего и холодного отличаются более чем на 5%.\n"
        "Горячий поток отдаёт %1 кВт, холодный получает %2 кВт.\n"
        "Если температуры и расход горячего заданы верно, необходимый расход холодного: %3 кг/с.\n"
        "Проверьте расходы, температуры и теплоёмкости; изменяйте только свободный параметр задачи.")
        .arg(qh/1000,0,'f',3).arg(qc/1000,0,'f',3)
        .arg(qh/(x.props_cold.heatCapacity*(x.t_cold_out-x.t_cold_in)),0,'g',10);
    if(correctionFactor(x,1)<=0 && correctionFactor(x,2)<=0 && correctionFactor(x,3)<=0) return "Заданные температуры недостижимы для схемы 1–2";
    for(const auto& pair:{qMakePair(x.hotTable,(x.t_hot_in+x.t_hot_out)/2), qMakePair(x.coldTable,(x.t_cold_in+x.t_cold_out)/2)}) {
        double previous=-1e100;
        for(const auto& point:pair.first) {
            if(!std::isfinite(point.temperature)||point.temperature<=previous||!validProps(point.props)) return "Некорректная таблица свойств";
            previous=point.temperature;
        }
        if(!pair.first.isEmpty() && (pair.second<pair.first.first().temperature||pair.second>pair.first.last().temperature)) return "Температура вне справочника";
    }
    for(auto pair:{qMakePair(x.hotTable,qMakePair(x.t_hot_in,x.t_hot_out)),qMakePair(x.coldTable,qMakePair(x.t_cold_in,x.t_cold_out))}) {
        if(!pair.first.isEmpty() && (std::min(pair.second.first,pair.second.second)<pair.first.first().temperature || std::max(pair.second.first,pair.second.second)>pair.first.last().temperature))return "Входная или выходная температура вне справочника среды";
    }
    return {};
}
QVector<ExchangerGeometry> HeatCalculator::generateCandidates(const InputData& x) {
    QVector<ExchangerGeometry> list;
    auto catalog=x.geometryCatalog.isEmpty()?pakhtCatalog()["geometry"].toArray():x.geometryCatalog;
    for(auto value:catalog) {
        auto o=value.toObject();int passes=o["tubePasses"].toInt();
        if((x.flowType==2 && passes==1)||(x.flowType!=2 && passes!=1)) continue;
        for(int shells=1;shells<=(x.flowType==2?3:1);++shells) {
            if(x.flowType==2 && (passes<2*shells || passes%(2*shells)))continue;
            for(double ratio:{0.2,0.3,0.4,0.5,0.6}) for(double cut:{20.,25.,30.}) {
                ExchangerGeometry g{};g.id=list.size();g.shellPasses=shells;
                g.tubeOuterDiameter=o["tubeOuterDiameter"].toDouble();g.tubeWallThickness=o["tubeWallThickness"].toDouble();
                g.tubeCount=o["tubeCount"].toInt();g.tubePasses=passes;g.tubeLength=o["tubeLength"].toDouble();g.shellDiameter=o["shellDiameter"].toDouble();
                g.baffleSpacing=ratio*g.shellDiameter;g.baffleCut=cut;
                g.heatTransferArea=pi*g.tubeOuterDiameter*g.tubeCount*g.tubeLength;
                g.materialName=x.materialName;g.wallConductivity=x.wallConductivity;list.push_back(g);
            }
        }
    }
    return list;
}
CalcResult HeatCalculator::evaluateCandidate(const InputData& x,const ExchangerGeometry& g) {
    CalcResult r; r.geometry=g;
    r.Q=x.G_hot*x.props_hot.heatCapacity*(x.t_hot_in-x.t_hot_out);
    r.correction=correctionFactor(x,g.shellPasses);
    if(r.correction<=0)return r;
    r.dT_cp=r.correction*(x.flowType==1 ? logMean(x.t_hot_in-x.t_cold_in,x.t_hot_out-x.t_cold_out)
        : logMean(x.t_hot_in-x.t_cold_out,x.t_hot_out-x.t_cold_in));
    const auto &h=x.props_hot, &c=x.props_cold;
    const double di=g.tubeOuterDiameter-2*g.tubeWallThickness, d=g.tubeOuterDiameter;
    if(di<=0 || g.tubeCount<=0||g.tubePasses<=0||g.baffleSpacing<=0||g.shellDiameter<=0||g.tubeLength<=0) return r;
    double at=pi*di*di/4*std::floor(double(g.tubeCount)/g.tubePasses);
    double pitch=1.25*d;
    double as=g.shellDiameter*g.baffleSpacing*(pitch-d)/pitch*(1-g.baffleCut/100)/g.shellPasses;
    double de=4*(std::sqrt(3.)/4*pitch*pitch-pi*d*d/8)/(pi*d/2);
    if(as<=0||de<=0) return r;
    double wh=x.G_hot/(h.density*at), wc=x.G_cold/(c.density*as);
    r.reHot=Correlations::calcRe(wh,di,h.density,h.viscosity);
    r.reCold=Correlations::calcRe(wc,d,c.density,c.viscosity);
    r.prHot=Correlations::calcPr(h.heatCapacity,h.viscosity,h.conductivity);
    r.prCold=Correlations::calcPr(c.heatCapacity,c.viscosity,c.conductivity);
    // Restrict shell empirical correlation to the documented educational model range.
    if(r.reCold<=5 || r.reCold>2e5 || r.prHot<0.6 || r.prHot>1e4 || r.prCold<0.6||r.prCold>1e4) return r;
    double th=(x.t_hot_in+x.t_hot_out)/2, tc=(x.t_cold_in+x.t_cold_out)/2;
    double wallH=th, wallC=tc, oldK=0;
    bool converged=false;
    for(int it=1;it<=x.maxIterations;++it) {
        auto hp=h, cp=c;
        try {
            if(!x.hotTable.isEmpty()) hp=propertiesAt(x.hotTable,wallH);
            if(!x.coldTable.isEmpty()) cp=propertiesAt(x.coldTable,wallC);
        } catch(const std::exception&) { return r; }
        double ph=Correlations::calcPr(hp.heatCapacity,hp.viscosity,hp.conductivity);
        double pc=Correlations::calcPr(cp.heatCapacity,cp.viscosity,cp.conductivity);
        r.alphaHot=Correlations::calcAlpha(tubeNu(r.reHot,r.prHot,ph),h.conductivity,di);
        r.alphaCold=Correlations::calcAlpha(Correlations::calcNuShell(r.reCold,r.prCold,pc),c.conductivity,d);
        r.K=Correlations::calcKCylindricalWall(r.alphaHot,r.alphaCold,di,d,g.wallConductivity,x.foulHot,x.foulCold);
        if(!positive(r.K)) return r;
        r.residual=oldK>0 ? std::abs(r.K-oldK)/r.K : 1;
        r.iterations=it;
        double flux=r.K*(th-tc);
        double nextH=th-flux*d/di*(1/r.alphaHot+x.foulHot);
        double nextC=tc+flux*(1/r.alphaCold+x.foulCold);
        double wallResidual=std::max(std::abs(nextH-wallH),std::abs(nextC-wallC))/std::max(1.,th-tc);
        if(it>1 && r.residual<=x.tolerance && ((x.hotTable.isEmpty() && x.coldTable.isEmpty()) || wallResidual<=x.tolerance)) { converged=true; break; }
        wallH=(wallH+nextH)/2; wallC=(wallC+nextC)/2;
        oldK=r.K;
    }
    if(!converged) return r;
    r.F_required=r.Q/(r.K*r.dT_cp);
    r.areaMargin=100*(g.heatTransferArea/r.F_required-1);
    r.dp_hot=(friction(r.reHot)*g.tubeLength*g.tubePasses/di+2.5*g.tubePasses)*h.density*wh*wh/2;
    // Kern-style estimate using crossflow hydraulic diameter; no nozzle losses.
    double fs=std::exp(0.576-0.19*std::log(r.reCold));
    double crossings=std::max(1.,std::ceil(g.tubeLength/g.baffleSpacing));
    r.dp_cold=fs*crossings*g.shellPasses*g.shellDiameter/de*c.density*wc*wc/2;
    r.success=std::isfinite(r.F_required)&&r.areaMargin>=x.minimumMargin&&r.dp_hot<=x.dp_max_hot&&r.dp_cold<=x.dp_max_cold;
    return r;
}
CalcResult HeatCalculator::calculate(const InputData& x) {
    CalcResult best; best.errorMsg=validate(x); if(!best.errorMsg.isEmpty()) return best;
    auto start=std::chrono::steady_clock::now();
    auto candidates=generateCandidates(x);
    QVector<CalcResult> results(candidates.size());
    // Allocate/detach before parallel writes. Every iteration owns one result slot.
    auto* out=results.data(); const auto* geometries=candidates.constData(); int used=1;
    #pragma omp parallel num_threads(x.threads)
    {
        #pragma omp single
        used=omp_get_num_threads();
        #pragma omp for schedule(static)
        for(int i=0;i<candidates.size();++i) out[i]=evaluateCandidate(x,geometries[i]);
    }
    int feasible=0;
    for(const auto& r:results) if(r.success) {
        ++feasible;
        if(!best.success || r.geometry.heatTransferArea<best.geometry.heatTransferArea ||
           (r.geometry.heatTransferArea==best.geometry.heatTransferArea && r.geometry.id<best.geometry.id)) best=r;
    }
    best.candidates=candidates.size(); best.feasible=feasible; best.threadsUsed=used;
    best.elapsedMs=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    if(!best.success) best.errorMsg="Допустимые варианты не найдены: проверьте ограничения давления, запас, диапазон свойств и сходимость";
    return best;
}
