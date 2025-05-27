#include <Arduino.h>
#include <string>
#include <vector>
#include "Communication.h"

// Utility: Convert MAC address array to string
std::string macToString(const uint8_t* mac, size_t size);


// Convert std::vector<uint8_t> MAC to Arduino String
String macToString_Arduino(const uint8_t* mac, size_t size);
String macToString_Arduino(const std::array<uint8_t, MAC_SIZE>& mac);