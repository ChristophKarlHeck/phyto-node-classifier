#include "preprocessing/OnlineMinMax.h"
#include <algorithm>  // For std::min_element and std::max_element
#include <limits>     // For std::numeric_limits
#include <iostream>

// Constructor: initializes a fixed-size window, starting index, and count.
OnlineMinMax::OnlineMinMax(std::size_t windowSize)
    : window(windowSize, 0.0f), windowSize(windowSize), currentIndex(0), count(0)
{
    // The window vector is pre-filled with windowSize elements.
}

// The update method writes each new value into the circular buffer.
void OnlineMinMax::update(std::vector<float> arr) {

    if (count < windowSize - arr.size() + 1) {
        std::copy(arr.begin(), arr.end(), window.begin() + count);
        count++;
    } else{
        //Move all values in widow to the left the last values are filled with arr
        std::copy(window.begin() + 1, window.end(), window.begin());
        window.back() = arr.back();
        count = windowSize;

    }
}

// Returns the maximum value in the current window.
// During the initial phase (if count < windowSize), only valid elements are considered.
float OnlineMinMax::getMaxValue() const {
    if (count == 0) {
        // No valid data; return the lowest possible float.
        return std::numeric_limits<float>::lowest();
    }
    if (count < windowSize) {
        return *std::max_element(window.begin(), window.begin() + count + 1);
    } else {
        return *std::max_element(window.begin(), window.end());
    }
}

// Returns the minimum value in the current window.
// If no valid data, returns the maximum possible float.
float OnlineMinMax::getMinValue() const {
    if (count == 0) {
        return std::numeric_limits<float>::max();
    }
    if (count < windowSize) {
        return *std::min_element(window.begin(), window.begin() + count + 1);
    } else {
        return *std::min_element(window.begin(), window.end());
    }
}