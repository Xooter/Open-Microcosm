#include "sim/fft.hpp"

#include "core/profiling.hpp"

#include <cmath>
#include <omp.h>
#include <stdexcept>
#include <utility>

Fft1D::Fft1D(int n) : n_(n), bitReverse_(n), twiddle_(n / 2), twiddleInverse_(n / 2) {
    if (n < 2 || (n & (n - 1)) != 0)
        throw std::invalid_argument("Fft1D: el tamaño debe ser potencia de 2");
    int bits = 0;
    while ((1 << bits) < n)
        ++bits;
    for (int i = 0; i < n; ++i) {
        int r = 0;
        for (int b = 0; b < bits; ++b)
            if (i & (1 << b))
                r |= 1 << (bits - 1 - b);
        bitReverse_[i] = r;
    }
    for (int k = 0; k < n / 2; ++k) {
        const double a = -2.0 * 3.14159265358979323846 * k / n;
        twiddle_[k] = {static_cast<float>(std::cos(a)), static_cast<float>(std::sin(a))};
        twiddleInverse_[k] = std::conj(twiddle_[k]);
    }
}

void Fft1D::transform(Complex* d, bool inverse) const {
    for (int i = 0; i < n_; ++i)
        if (i < bitReverse_[i])
            std::swap(d[i], d[bitReverse_[i]]);
    const Complex* tw = inverse ? twiddleInverse_.data() : twiddle_.data();
    for (int len = 2; len <= n_; len <<= 1) {
        const int half = len / 2, stride = n_ / len;
        for (int i = 0; i < n_; i += len) {
            for (int j = 0; j < half; ++j) {
                const Complex w = tw[j * stride];
                const Complex u = d[i + j], v = d[i + j + half] * w;
                d[i + j] = u + v;
                d[i + j + half] = u - v;
            }
        }
    }
}

void Fft1D::transformLanes(Complex* d, int lanes, bool inverse) const {
    const auto L = static_cast<std::size_t>(lanes);
    for (int i = 0; i < n_; ++i)
        if (i < bitReverse_[i]) {
            Complex* a = d + static_cast<std::size_t>(i) * L;
            Complex* b = d + static_cast<std::size_t>(bitReverse_[i]) * L;
            for (std::size_t l = 0; l < L; ++l)
                std::swap(a[l], b[l]);
        }
    const Complex* tw = inverse ? twiddleInverse_.data() : twiddle_.data();
    for (int len = 2; len <= n_; len <<= 1) {
        const int half = len / 2, stride = n_ / len;
        for (int i = 0; i < n_; i += len) {
            for (int j = 0; j < half; ++j) {
                const Complex w = tw[j * stride];
                Complex* p = d + static_cast<std::size_t>(i + j) * L;
                Complex* q = d + static_cast<std::size_t>(i + j + half) * L;
                for (std::size_t l = 0; l < L; ++l) {
                    const Complex u = p[l], v = q[l] * w;
                    p[l] = u + v;
                    q[l] = u - v;
                }
            }
        }
    }
}

Fft2D::Fft2D(int nx, int ny) : rows_(nx), cols_(ny) {}

void Fft2D::inverse(std::vector<Complex>& data) const {
    transform(data, true);
    const float scale = 1.0f / static_cast<float>(nx() * ny());
    for (Complex& c : data)
        c *= scale;
}

void Fft2D::transform(std::vector<Complex>& data, bool inverse) const {
    PROFILE_SCOPE("Fft2D");
    const int nx = this->nx(), ny = this->ny();
    // Columnas de a bloques de kLanes vecinas: cada fila aporta una línea de caché entera (de a
    // una, cada elemento era una línea distinta) y el compilador vectoriza entre columnas. Cada
    // columna ve las mismas operaciones que con `transform`.
    constexpr int kLanes = 8;
    const int lanes = nx % kLanes == 0 ? kLanes : 1;
    const std::size_t needed = static_cast<std::size_t>(omp_get_max_threads()) * ny * lanes;
    if (scratch_.size() < needed)
        scratch_.resize(needed); // solo crece si cambia la cantidad de hilos

#pragma omp parallel for schedule(static)
    for (int j = 0; j < ny; ++j)
        rows_.transform(&data[static_cast<std::size_t>(j) * nx], inverse);

#pragma omp parallel
    {
        Complex* block = &scratch_[static_cast<std::size_t>(omp_get_thread_num()) * ny * lanes];
#pragma omp for schedule(static)
        for (int i0 = 0; i0 < nx; i0 += lanes) {
            for (int j = 0; j < ny; ++j)
                for (int l = 0; l < lanes; ++l)
                    block[static_cast<std::size_t>(j) * lanes + l] = data[static_cast<std::size_t>(j) * nx + i0 + l];
            cols_.transformLanes(block, lanes, inverse);
            for (int j = 0; j < ny; ++j)
                for (int l = 0; l < lanes; ++l)
                    data[static_cast<std::size_t>(j) * nx + i0 + l] = block[static_cast<std::size_t>(j) * lanes + l];
        }
    }
}
