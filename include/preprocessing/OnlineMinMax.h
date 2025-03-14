#ifndef ONLINE_MIN_MAX
#define ONLINE_MIN_MAX

#include <vector>
#include <cstdint>

class OnlineMinMax {
    private:
        std::vector<float> window;
        std::size_t windowSize;
        std::size_t currentIndex;
        std::size_t count;  // Number of valid elements (max equals windowSize)

    public:
        // Constructor that sets the fixed window size.
        OnlineMinMax(std::size_t windowSize);

        // Update the window with new values.
        void update(std::vector<float> arr);

        // Return the maximum value in the current window.
        float getMaxValue(void) const;

        // Return the minimum value in the current window.
        float getMinValue(void) const;
};

#endif // ONLINE_MIN_MAX