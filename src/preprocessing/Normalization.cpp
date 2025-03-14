#include "preprocessing/Normalization.h"
#include <cmath>
#include <numeric> 

std::vector<float> Preprocessing::minMaxNormalization(std::vector<float> inputs, float minValue, float maxValue, float factor) {
    if (inputs.empty()) return {}; // Handle empty input case

    // Avoid division by zero if minValue equals maxValue.
    if (maxValue == minValue) {
        // Depending on your use case, you could return a vector of zeros
        // or simply return the original inputs.
        return std::vector<float>(inputs.size(), 0.0f);
    }

    for (auto& value : inputs) {
        value = ((value - minValue) / (maxValue - minValue)) * factor;
    }
    return inputs;
}

std::vector<float> Preprocessing::zScoreNormalization(std::vector<float> inputs, float factor) {
        if (inputs.empty()) return {}; // Handle empty input case

        // Compute mean
        float mean = std::accumulate(inputs.begin(), inputs.end(), 0.0f) / inputs.size();

        // Compute standard deviation
        float variance = 0.0f;
        for (float val : inputs) {
            variance += (val - mean) * (val - mean);
        }
        variance /= inputs.size();
        float stddev = std::sqrt(variance);

        // Avoid division by zero
        if (stddev == 0.0f) return std::vector<float>(inputs.size(), 0.0f);

        // Normalize
        std::vector<float> normalized;
        normalized.reserve(inputs.size());
        for (float val : inputs) {
            normalized.push_back(factor*((val - mean) / stddev));
        }

        return normalized;
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