#ifndef UTILS_H_
#define UTILS_H_

#include <string>
#include <cstdint> // For uint8_t

/**
 * @brief Converts a byte to its binary representation as a string.
 * @param byte The byte to convert.
 * @return A string representing the binary format of the byte.
 */
std::string byte_to_binary(uint8_t byte);

#endif // UTILS_H_