#include "preprocessing/OnlineMean.h"
#include <cmath>
#include <iostream>

OnlineMean::OnlineMean(void) : m_mean_byte_0(0.0f), m_mean_byte_1(0.0f), m_mean_byte_2(0.0f), m_count(0), m_correction_0(0.0f), m_correction_1(0.0f), m_correction_2(0.0f) {}

float OnlineMean::kahan_sum(float sum, float& correction, float next){
    float y = next - correction;
    float t = sum + y;
    correction = (t - sum) - y;
    return t;
}

void OnlineMean::update(std::array<uint8_t, 3> arr){
    m_count++;
    float factor = 1.0f / m_count;

    float x_0 = static_cast<float>(arr[0]);
    float x_1 = static_cast<float>(arr[1]);
    float x_2 = static_cast<float>(arr[2]);

    float term_0 = (x_0 - m_mean_byte_0) * factor;
    float term_1 = (x_1 - m_mean_byte_1) * factor;
    float term_2 = (x_2 - m_mean_byte_2) * factor;

    m_mean_byte_0 = kahan_sum(m_mean_byte_0, m_correction_0, term_0);
    m_mean_byte_1 = kahan_sum(m_mean_byte_1, m_correction_1, term_1);
    m_mean_byte_2 = kahan_sum(m_mean_byte_2, m_correction_2, term_2);
}

uint8_t OnlineMean::round_to_uint8(float value) {
    int rounded;
    
    // Apply "round half to even" (Bankers' Rounding)
    if (std::fmod(value, 1.0f) == 0.5f) {
        rounded = static_cast<int>(2.0f * std::round(value / 2.0f)); // Round to nearest even
    } else {
        rounded = static_cast<int>(std::round(value));  // Regular rounding
    }

    return static_cast<uint8_t>(std::min(255, std::max(0, rounded))); // Clamp to 0-255
}

std::array<uint8_t, 3> OnlineMean::get_mean(void) const {

    uint8_t byte_0 = round_to_uint8(m_mean_byte_0);
    uint8_t byte_1 = round_to_uint8(m_mean_byte_1);
    uint8_t byte_2 = round_to_uint8(m_mean_byte_2);
    std::array<uint8_t, 3> result = {byte_0, byte_1, byte_2};
    return result;
}