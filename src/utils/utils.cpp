#include "utils/utils.h"

std::string byte_to_binary(uint8_t byte) {
    std::string binary;
    for (int i = 7; i >= 0; --i) {
        binary += (byte & (1 << i)) ? '1' : '0';
    }
    return binary;
}