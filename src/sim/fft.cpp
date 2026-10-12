#include "sim/fft.hpp"

#include "core/profiling.hpp"

#include <cmath>
#include <omp.h>
#include <stdexcept>
#include <utility>

Fft1D::Fft1D(int n) : n_(n), bitReverse_(n), twiddle_(n / 2) {
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
    }
}

void Fft1D::transform(Complex* d, bool inverse) const {
    for (int i = 0; i < n_; ++i)
        if (i < bitReverse_[i])
            std::swap(d[i], d[bitReverse_[i]]);
    for (int len = 2; len <= n_; len <<= 1) {
        const int half = len / 2, stride = n_ / len;
        for (int i = 0; i < n_; i += len) {
            for (int j = 0; j < half; ++j) {
                Complex w = twiddle_[j * stride];
                if (inverse)
                    w = std::conj(w);
                const Complex u = d[i + j], v = d[i + j + half] * w;
                d[i + j] = u + v;
                d[i + j + half] = u - v;
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
    const std::size_t needed = static_cast<std::size_t>(omp_get_max_threads()) * ny;
    if (scratch_.size() < needed)
        scratch_.resize(needed); // solo crece si cambia la cantidad de hilos

#pragma omp parallel for schedule(static)
    for (int j = 0; j < ny; ++j)
        rows_.transform(&data[static_cast<std::size_t>(j) * nx], inverse);

#pragma omp parallel
    {
        Complex* col = &scratch_[static_cast<std::size_t>(omp_get_thread_num()) * ny];
#pragma omp for schedule(static)
        for (int i = 0; i < nx; ++i) {
            for (int j = 0; j < ny; ++j)
                col[j] = data[static_cast<std::size_t>(j) * nx + i];
            cols_.transform(col, inverse);
            for (int j = 0; j < ny; ++j)
                data[static_cast<std::size_t>(j) * nx + i] = col[j];
        }
    }
}
