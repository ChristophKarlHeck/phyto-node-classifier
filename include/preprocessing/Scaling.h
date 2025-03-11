#ifndef PREPROCESSING_H
#define PREPROCESSING_H

#include <vector>
#include <array>
#include <cstdint>

class Preprocessing {
public:
    static std::vector<float> minMaxScale(std::vector<float> inputs, float minValue, float maxValue);
    static std::vector<float> zScoreNormalization(std::vector<float> inputs, int factor);
    static std::array<uint8_t, 3> computeMean(std::vector<std::array<uint8_t,3>> values);
};

#endif // PREPROCESSING_H