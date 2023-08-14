#pragma once

#include <array>
#include <numeric>
#include <cmath>

template<unsigned precision = 1, unsigned scale = 1, std::size_t size = 10, typename itemType = float>
class SlidingAverage {
public:
    void insert(itemType value)
    {
        container[index] = value;
        index = (index + std::size_t(1)) % size;
        filled = filled | !index;
    }

    explicit operator itemType()
    {
        auto currentSize = filled ? size : index;
        return ::round(std::accumulate(container.begin(), container.begin() + currentSize, (itemType)0) / (float)(currentSize ? currentSize : 1) * scalingFactor) / roundFactor;
    }
    bool isFilled() const { return filled; }
private:
    static constexpr itemType roundFactor = itemType(precision ? (precision * 10) : 1);
    static constexpr itemType scalingFactor = roundFactor / (itemType)scale;
    std::array<itemType, size> container;
    std::size_t index = 0;
    bool filled = false;
};
