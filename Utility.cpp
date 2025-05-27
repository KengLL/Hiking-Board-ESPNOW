#include "Utility.h"
#include <cstdio>
#include <cstring>

// Utility: Convert MAC address array to string
std::string macToString(const uint8_t* mac, size_t size) {
    char macStr[18];
    if (size == 6 && mac) {
        snprintf(macStr, sizeof(macStr), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        return std::string(macStr);
    }
    return std::string();
}

// Utility: Copy MAC address from std::vector<uint8_t> to uint8_t array
void copyMacToArray(const std::vector<uint8_t>& src, uint8_t dest[6]) {
    for (int i = 0; i < 6; ++i) {
        dest[i] = (i < src.size()) ? src[i] : 0;
    }
}