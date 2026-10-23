#include "correlations.h"
#include <cmath>

// Для вычисления ln(x) и pow(x,y) используем <cmath>: std::log, std::pow

double Correlations::calcRe(double w, double d_eq, double rho, double mu)
{
    // Проверка на ноль, чтобы не было деления на ноль
    if (mu <= 0.0) return 0.0;
    return (w * d_eq * rho) / mu;
}

double Correlations::calcPr(double cp, double mu, double lambda)
{
    if (lambda <= 0.0) return 0.0;
    return (cp * mu) / lambda;
}

double Correlations::calcNuTubeTurbulent(double Re, double Pr, double Pr_wall, double eps_l)
{
    // Формула 4.17 из методички (стр. 95, 152):
    // Nu = 0.021 * Re^0.8 * Pr^0.43 * (Pr/Pr_ст)^0.25 * eps_l
    // Если Pr_wall неизвестен (приближение), принимаем Pr_wall = Pr, тогда (Pr/Pr_wall)^0.25 = 1.
    if (Re <= 0.0 || Pr <= 0.0) return 0.0;
    
    double ratio = 1.0;
    if (Pr_wall > 0.0) ratio = std::pow(Pr / Pr_wall, 0.25);
    
    return 0.021 * std::pow(Re, 0.8) * std::pow(Pr, 0.43) * ratio * eps_l;
}

double Correlations::calcNuShell(double Re, double Pr, double Pr_wall, double eps_phi)
{
    // Формула 4.31 из методички (стр. 94):
    // Nu = 0.36 * eps_phi * Re^0.6 * Pr^0.36 * (Pr/Pr_ст)^0.25
    // eps_phi = 0.6 для многократно-перекрестного движения теплоносителя
    if (Re <= 0.0 || Pr <= 0.0) return 0.0;
    
    double ratio = 1.0;
    if (Pr_wall > 0.0) ratio = std::pow(Pr / Pr_wall, 0.25);
    
    return (Re<1000 ? 0.56*std::pow(Re,0.5) : 0.36*std::pow(Re,0.6)) * eps_phi * std::pow(Pr, 0.36) * ratio;
}

double Correlations::calcAlpha(double Nu, double lambda, double d_eq)
{
    if (d_eq <= 0.0) return 0.0;
    return (Nu * lambda) / d_eq;
}

double Correlations::calcKFlatWall(double alpha1, double alpha2,
                                   double delta_wall, double lambda_wall,
                                   double R_foul1, double R_foul2)
{
    // K = 1 / (1/alpha1 + R_foul1 + delta/lambda + R_foul2 + 1/alpha2)
    // Проверка деления на ноль
    if (alpha1 <= 0.0 || alpha2 <= 0.0 || lambda_wall <= 0.0) return 0.0;
    
    double R_total = (1.0 / alpha1) + R_foul1 + (delta_wall / lambda_wall) + R_foul2 + (1.0 / alpha2);
    if (R_total <= 0.0) return 0.0;
    
    return 1.0 / R_total;
}

double Correlations::calcKCylindricalWall(double alpha_in, double alpha_out,
                                          double d_in, double d_out,
                                          double lambda_wall,
                                          double R_foul_in, double R_foul_out)
{
    // Формула для цилиндрической стенки (методичка, стр. 81)
    // K = [ 1/alpha_out + R_foul_out + (d_out/(2*lambda_wall)) * ln(d_out/d_in) + (R_foul_in + 1/alpha_in) * (d_out/d_in) ]^(-1)
    // K считается относительно наружной площади.
    
    if (alpha_in <= 0.0 || alpha_out <= 0.0 || lambda_wall <= 0.0 || d_in <= 0.0 || d_out <= d_in) return 0.0;
    
    double term1 = 1.0 / alpha_out;
    double term2 = R_foul_out;
    double term3 = (d_out / (2.0 * lambda_wall)) * std::log(d_out / d_in);
    double term4 = (R_foul_in + 1.0 / alpha_in) * (d_out / d_in);
    
    double R_total = term1 + term2 + term3 + term4;
    
    if (R_total <= 0.0) return 0.0;
    
    return 1.0 / R_total;
}