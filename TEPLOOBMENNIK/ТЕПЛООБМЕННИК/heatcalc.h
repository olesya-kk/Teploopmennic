#pragma once
#include "inputdata.h"
#include "exchangergeometry.h"
struct CalcResult {
    bool success=false;
    QString errorMsg;
    double K=0, Q=0, dT_cp=0, correction=1, F_required=0, areaMargin=0;
    double dp_hot=0, dp_cold=0, reHot=0, reCold=0, prHot=0, prCold=0;
    double alphaHot=0, alphaCold=0, residual=0, elapsedMs=0;
    int iterations=0, candidates=0, feasible=0, threadsUsed=1;
    ExchangerGeometry geometry{};
};
class HeatCalculator {
public:
    static CalcResult calculate(const InputData& input);
    static double logMean(double a, double b);
    static double correctionFactor(const InputData& input, int shellPasses=1);
    static QString validate(const InputData& input);
    static QVector<ExchangerGeometry> generateCandidates(const InputData& input);
    static CalcResult evaluateCandidate(const InputData& input, const ExchangerGeometry& geom);
};
