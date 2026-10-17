#include "game/dose_fit.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

double DoseFit::operator()(double d) const {
    if (!(d > 0.0))
        return top;
    return bottom + (top - bottom) / (1.0 + std::pow(d / ic50, hill));
}

DoseFit fitDoseResponse(const std::vector<double>& doses, const std::vector<double>& y) {
    DoseFit f;
    const std::size_t n = std::min(doses.size(), y.size());
    if (n < 3)
        return f;
    double lo = y[0], hi = y[0], dmin = 1e300, dmax = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
        lo = std::min(lo, y[i]);
        hi = std::max(hi, y[i]);
        if (doses[i] > 0.0) {
            dmin = std::min(dmin, doses[i]);
            dmax = std::max(dmax, doses[i]);
        }
    }
    if (!(dmax > 0.0) || !(hi > lo))
        return f;
    f.top = hi;
    f.bottom = lo;
    auto sse = [&](double logIc, double h) {
        DoseFit g = f;
        g.ic50 = std::exp(logIc);
        g.hill = h;
        double s = 0.0;
        for (std::size_t i = 0; i < n; ++i)
            s += (y[i] - g(doses[i])) * (y[i] - g(doses[i]));
        return s;
    };
    // Grilla gruesa y después ajuste fino por coordenadas (sección áurea en cada eje).
    double bestL = std::log(dmin), bestH = 1.0, best = 1e300;
    for (int i = 0; i <= 60; ++i)
        for (int j = 1; j <= 40; ++j) {
            const double L = std::log(dmin / 4.0) + (std::log(dmax * 4.0) - std::log(dmin / 4.0)) * i / 60.0;
            const double h = 0.1 * j;
            const double s = sse(L, h);
            if (s < best) {
                best = s;
                bestL = L;
                bestH = h;
            }
        }
    auto golden = [&](double a, double b, auto&& fn) {
        const double g = 0.618033988749895;
        double c = b - g * (b - a), d = a + g * (b - a);
        for (int k = 0; k < 60; ++k) {
            if (fn(c) < fn(d))
                b = d;
            else
                a = c;
            c = b - g * (b - a);
            d = a + g * (b - a);
        }
        return 0.5 * (a + b);
    };
    for (int round = 0; round < 3; ++round) {
        bestL = golden(bestL - 1.0, bestL + 1.0, [&](double L) { return sse(L, bestH); });
        bestH = golden(std::max(0.05, bestH - 1.0), bestH + 1.0, [&](double h) { return sse(bestL, h); });
    }
    f.ic50 = std::exp(bestL);
    f.hill = bestH;
    const double mean = std::accumulate(y.begin(), y.begin() + static_cast<std::ptrdiff_t>(n), 0.0) / n;
    double tot = 0.0;
    for (std::size_t i = 0; i < n; ++i)
        tot += (y[i] - mean) * (y[i] - mean);
    f.r2 = tot > 0.0 ? 1.0 - sse(bestL, bestH) / tot : 0.0;
    f.valid = true;
    return f;
}

double minimumInhibitory(const std::vector<double>& doses, const std::vector<double>& y, double visible) {
    double mic = 0.0;
    for (std::size_t i = 0; i < std::min(doses.size(), y.size()); ++i)
        if (doses[i] > 0.0 && y[i] < visible && (mic == 0.0 || doses[i] < mic))
            mic = doses[i];
    return mic;
}
