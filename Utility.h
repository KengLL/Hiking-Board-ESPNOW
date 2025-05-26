
#include <string>
#include <vector>

// Utility: Convert MAC address vector to string
std::string macToString(const std::vector<uint8_t>& mac);

// Utility: Copy MAC address from std::vector<uint8_t> to uint8_t array
void copyMacToArray(const std::vector<uint8_t>& src, uint8_t dest[6]);