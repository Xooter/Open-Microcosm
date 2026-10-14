#include "game/timeseries.hpp"

#include "core/profiling.hpp"

#include <utility>

void TimeSeries::setColumns(std::vector<SeriesColumn> columns) {
    columns_ = std::move(columns);
    data_.assign(columns_.size(), {});
    for (std::vector<float>& c : data_)
        c.reserve(capacity_);
    stride_ = 1;
    skipped_ = 0;
}

void TimeSeries::clear() {
    for (std::vector<float>& c : data_)
        c.clear();
    stride_ = 1;
    skipped_ = 0;
}

bool TimeSeries::offer() {
    if (++skipped_ < stride_)
        return false;
    skipped_ = 0;
    return true;
}

void TimeSeries::push(const float* row) {
    PROFILE_SCOPE("TimeSeries::push");
    if (rows() >= capacity_)
        decimate();
    for (std::size_t c = 0; c < data_.size(); ++c)
        data_[c].push_back(row[c]);
}

int TimeSeries::find(std::string_view name) const {
    for (std::size_t c = 0; c < columns_.size(); ++c)
        if (columns_[c].name == name)
            return static_cast<int>(c);
    return -1;
}

void TimeSeries::decimate() {
    // Se queda con las filas pares (la primera siempre, para no perder el comienzo).
    for (std::vector<float>& c : data_) {
        std::size_t w = 0;
        for (std::size_t r = 0; r < c.size(); r += 2)
            c[w++] = c[r];
        c.resize(w);
    }
    stride_ *= 2;
}
