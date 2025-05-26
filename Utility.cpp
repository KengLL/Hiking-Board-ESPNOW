#include "Utility.h"

// Utility: Convert MAC address vector to string
std::string macToString(const std::vector<uint8_t>& mac) {
    char macStr[18];
    if (mac.size() == 6) {
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