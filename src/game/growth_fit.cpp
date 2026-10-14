#include "game/growth_fit.hpp"

#include "game/timeseries.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

struct Line {
    double slope = 0.0, intercept = 0.0;
    bool ok = false;
};

Line regress(const double* x, const double* y, std::size_t n) {
    Line l;
    if (n < 2)
        return l;
    double mx = 0, my = 0;
    for (std::size_t i = 0; i < n; ++i) {
        mx += x[i];
        my += y[i];
    }
    mx /= static_cast<double>(n);
    my /= static_cast<double>(n);
    double sxx = 0, sxy = 0;
    for (std::size_t i = 0; i < n; ++i) {
        sxx += (x[i] - mx) * (x[i] - mx);
        sxy += (x[i] - mx) * (y[i] - my);
    }
    if (sxx <= 0.0)
        return l;
    l.slope = sxy / sxx;
    l.intercept = my - l.slope * mx;
    l.ok = true;
    return l;
}

} // namespace

float GrowthFit::logistic(float t) const {
    return logisticValid ? capacity / (1.0f + std::exp(logisticShift - logisticRate * t)) : 0.0f;
}

GrowthFit fitGrowth(const float* t, const float* n, std::size_t count) {
    GrowthFit fit;
    std::vector<double> x, y;
    x.reserve(count);
    y.reserve(count);
    for (std::size_t i = 0; i < count; ++i)
        if (n[i] > 0.0f && finiteNumber(n[i]) && finiteNumber(t[i])) {
            x.push_back(t[i]);
            y.push_back(std::log(static_cast<double>(n[i])));
        }
    const std::size_t m = x.size();
    if (m < 5)
        return fit;

    // µmax: ventana deslizante de ~1/6 de las muestras (al menos 4).
    const std::size_t w = std::max<std::size_t>(4, m / 6);
    Line best;
    for (std::size_t i = 0; i + w <= m; ++i) {
        const Line l = regress(x.data() + i, y.data() + i, w);
        if (l.ok && (!best.ok || l.slope > best.slope))
            best = l;
    }
    if (!best.ok || best.slope <= 1e-6)
        return fit; // no crece
    fit.valid = true;
    fit.muMax = static_cast<float>(best.slope);
    fit.doublingHours = static_cast<float>(std::log(2.0) / best.slope);
    fit.tangentIntercept = static_cast<float>(best.intercept);
    fit.lagHours = static_cast<float>(std::max(0.0, (y[0] - best.intercept) / best.slope - x[0]));

    // Logística: K entre el máximo observado y 20 veces él.
    double nMax = 0.0;
    for (double v : y)
        nMax = std::max(nMax, std::exp(v));
    struct Trial {
        double sse = 1e300, r = 0, c = 0;
    };
    std::vector<double> z(m);
    auto trial = [&](double k) {
        Trial tr;
        for (std::size_t i = 0; i < m; ++i)
            z[i] = std::log(std::max(1e-9, k / std::exp(y[i]) - 1.0));
        const Line l = regress(x.data(), z.data(), m);
        if (!l.ok || l.slope >= 0.0)
            return tr; // no satura hacia K: no es logística
        tr.r = -l.slope;
        tr.c = l.intercept;
        tr.sse = 0.0;
        for (std::size_t i = 0; i < m; ++i) {
            const double pred = std::log(k) - std::log1p(std::exp(tr.c - tr.r * x[i]));
            tr.sse += (pred - y[i]) * (pred - y[i]);
        }
        return tr;
    };
    double a = std::log(nMax * 1.0001), b = std::log(nMax * 20.0);
    const double g = 0.5 * (std::sqrt(5.0) - 1.0);
    double c1 = b - g * (b - a), c2 = a + g * (b - a);
    Trial t1 = trial(std::exp(c1)), t2 = trial(std::exp(c2));
    for (int it = 0; it < 60; ++it) {
        if (t1.sse < t2.sse) {
            b = c2;
            c2 = c1;
            t2 = t1;
            c1 = b - g * (b - a);
            t1 = trial(std::exp(c1));
        } else {
            a = c1;
            c1 = c2;
            t1 = t2;
            c2 = a + g * (b - a);
            t2 = trial(std::exp(c2));
        }
    }
    const double k = std::exp(0.5 * (a + b));
    const Trial tr = trial(k);
    if (tr.sse < 1e299 && tr.r > 0.0) {
        fit.logisticValid = true;
        fit.capacity = static_cast<float>(k);
        fit.logisticRate = static_cast<float>(tr.r);
        fit.logisticShift = static_cast<float>(tr.c);
        fit.plateau = k < 1.25 * nMax;
    }
    return fit;
}
