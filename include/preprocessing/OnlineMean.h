#ifndef ONLINE_MEAN_H
#define ONLINE_MEAN_H

#include <array>
#include <cstdint>

class OnlineMean {
    private:
        float m_mean_byte_0;
        float m_mean_byte_1;
        float m_mean_byte_2;
        int m_count;
        float m_correction_0;
        float m_correction_1;
        float m_correction_2;

        float kahan_sum(float sum, float& correction, float next);
        static uint8_t round_to_uint8(float value);

    public:
        OnlineMean(void);
        void update(std::array<uint8_t, 3> arr);
        std::array<uint8_t, 3> get_mean(void) const;

};

#endif // ONLINE_MEAN_H