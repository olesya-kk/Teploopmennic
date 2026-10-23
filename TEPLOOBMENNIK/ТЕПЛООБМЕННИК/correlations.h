#ifndef CORRELATIONS_H
#define CORRELATIONS_H

// Класс со статическими методами для расчета критериев подобия и теплоотдачи.
// Все методы статические, чтобы можно было вызывать их без создания объекта: Correlations::calcRe(...)
class Correlations
{
public:
    // ---- Базовые критерии подобия ----

    // Число Рейнольдса: Re = w * d_eq * rho / mu
    // w - скорость (м/с), d_eq - эквивалентный диаметр (м), rho - плотность (кг/м3), mu - вязкость (Па*с)
    static double calcRe(double w, double d_eq, double rho, double mu);

    // Число Прандтля: Pr = cp * mu / lambda
    // cp - теплоемкость (Дж/(кг*К)), mu - вязкость (Па*с), lambda - теплопроводность (Вт/(м*К))
    static double calcPr(double cp, double mu, double lambda);

    // ---- Числа Нуссельта (Критериальные уравнения) ----

    // Нуссельт для трубного пространства (турбулентный режим Re > 10000)
    // Формула 4.17 из методички: Nu = 0.021 * Re^0.8 * Pr^0.43 * (Pr/Pr_ст)^0.25 * eps_l
    // eps_l - поправка на длину трубы (для L/d > 50 равна 1.0)
    static double calcNuTubeTurbulent(double Re, double Pr, double Pr_wall = 0.0, double eps_l = 1.0);

    // Нуссельт для межтрубного пространства с сегментными перегородками
    // Формула 4.31 из методички: Nu = 0.36 * eps_phi * Re^0.6 * Pr^0.36 * (Pr/Pr_ст)^0.25
    // eps_phi - коэффициент угла атаки (для перекрестного тока ~0.6)
    static double calcNuShell(double Re, double Pr, double Pr_wall = 0.0, double eps_phi = 0.6);

    // ---- Коэффициенты теплоотдачи ----

    // alpha = Nu * lambda / d_eq
    static double calcAlpha(double Nu, double lambda, double d_eq);

    // ---- Коэффициент теплопередачи ----

    // K для плоской/цилиндрической стенки (зависит от того, относительно какой площади считаем).
    // Здесь считаем по формуле для плоской стенки (упрощение, подходящее для тонких труб).
    // K = 1 / (1/alpha1 + R_foul1 + delta_wall/lambda_wall + R_foul2 + 1/alpha2)
    // R_foul - термические сопротивления загрязнений (м2*К/Вт), delta - толщина стенки (м)
    static double calcKFlatWall(double alpha1, double alpha2,
                                double delta_wall, double lambda_wall,
                                double R_foul1, double R_foul2);

    // Для цилиндрической стенки (более точная, если нужна).
    // K считается относительно НАРУЖНОЙ поверхности (d_out).
    static double calcKCylindricalWall(double alpha_in, double alpha_out,
                                       double d_in, double d_out,
                                       double lambda_wall,
                                       double R_foul_in, double R_foul_out);
};

#endif // CORRELATIONS_H