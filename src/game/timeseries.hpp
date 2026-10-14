#pragma once

#include <bit>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Serie temporal de la gota (fase 13): una tabla de columnas con nombre y unidad, guardada por
// columna (cada una contigua, como la quieren los gráficos). Tiene un tope de filas: al llenarse
// se queda con una de cada dos y desde ahí guarda una muestra de cada `stride()` ofrecidas, así
// una corrida larga cubre todo su tiempo con memoria acotada.
struct SeriesColumn {
    std::string name;  // "Paramecium", "O2"...
    std::string unit;  // "individuos", "µM", "pmol P"...
    std::string group; // "Tiempo", "Poblacion", "Quimica", "Balance", "Region"
};

// ¿Es un número (no NaN ni infinito)? Por bits: con -ffast-math, std::isfinite se optimiza a
// "siempre verdadero", y la serie usa NaN para marcar valores faltantes.
inline bool finiteNumber(double v) {
    return (std::bit_cast<std::uint64_t>(v) & 0x7ff0000000000000ull) != 0x7ff0000000000000ull;
}

class TimeSeries {
  public:
    explicit TimeSeries(std::size_t capacity = 4096) : capacity_(capacity < 2 ? 2 : capacity) {}

    void setColumns(std::vector<SeriesColumn> columns);
    void clear(); // borra las filas, conserva las columnas

    // ¿Se guardará la próxima muestra ofrecida? Avanza el contador de decimación: llamarlo una
    // vez por muestra y, si da true, `push` (así no se arma una fila que se descarta).
    bool offer();
    // Agrega una fila (`row` tiene una entrada por columna).
    void push(const float* row);

    std::size_t rows() const { return data_.empty() ? 0 : data_[0].size(); }
    std::size_t columns() const { return columns_.size(); }
    const SeriesColumn& info(std::size_t c) const { return columns_[c]; }
    const std::vector<float>& column(std::size_t c) const { return data_[c]; }
    int find(std::string_view name) const; // -1 si no está
    int stride() const { return stride_; }
    std::size_t capacity() const { return capacity_; }

  private:
    void decimate();

    std::vector<SeriesColumn> columns_;
    std::vector<std::vector<float>> data_;
    std::size_t capacity_;
    int stride_ = 1;
    int skipped_ = 0;
};
