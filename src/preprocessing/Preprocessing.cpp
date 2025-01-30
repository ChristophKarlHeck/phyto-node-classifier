#include "preprocessing/Preprocessing.h"

std::vector<float> Preprocessing::minMaxScale(std::vector<float> inputs, float minValue, float maxValue) {
    for (auto& value : inputs) {
        value = (value - minValue) / (maxValue - minValue);
    }
    return inputs;
}

std::array<uint8_t, 3> Preprocessing::computeMean(std::vector<std::array<uint8_t,3>> values) {
    if (values.empty()) {
        return {0, 0, 0};  // Return zero if no values exist
    }

    uint32_t sum_0 = 0, sum_1 = 0, sum_2 = 0;  // Sum per byte position

    for (const auto& bytes : values) {
        sum_0 += bytes[0];  // Most significant byte
        sum_1 += bytes[1];  // Middle byte
        sum_2 += bytes[2];  // Least significant byte
    }

    uint32_t size = values.size();

    // To round properly, use (sum_0 + size / 2) / size before casting. Since casting truncates.
    return {static_cast<uint8_t>((sum_0 + size / 2) / size), 
            static_cast<uint8_t>((sum_1 + size / 2) / size), 
            static_cast<uint8_t>((sum_2 + size / 2) / size)};
}