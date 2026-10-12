#pragma once

#include <complex>
#include <vector>

using Complex = std::complex<float>;

// FFT radix-2 iterativa (Cooley–Tukey). Solo tamaños potencia de 2.
class Fft1D {
  public:
    explicit Fft1D(int n);
    void transform(Complex* data, bool inverse) const;
    int size() const { return n_; }

  private:
    int n_;
    std::vector<int> bitReverse_;
    std::vector<Complex> twiddle_;
};

// FFT 2D sobre una grilla periódica nx·ny en orden fila-mayor. La inversa normaliza por nx·ny.
// Con dominio periódico, la FFT resuelve difusión y Stokes de forma exacta y estable.
class Fft2D {
  public:
    Fft2D(int nx, int ny);
    void forward(std::vector<Complex>& data) const { transform(data, false); }
    void inverse(std::vector<Complex>& data) const;
    int nx() const { return rows_.size(); }
    int ny() const { return cols_.size(); }

  private:
    void transform(std::vector<Complex>& data, bool inverse) const;

    Fft1D rows_, cols_;
    mutable std::vector<Complex> scratch_; // una columna por hilo
};

// Número de onda angular del índice i en una dimensión de n celdas y longitud L.
inline float wavenumber(int i, int n, float length) {
    const int k = i <= n / 2 ? i : i - n;
    return 6.28318530718f * static_cast<float>(k) / length;
}
