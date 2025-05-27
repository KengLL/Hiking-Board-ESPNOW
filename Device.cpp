#include <Arduino.h>
#include "Device.h"
#include "Communication.h"
#include "Utility.h"
#include <map>
#include <vector>
#include <cstring>
#include <sstream>
#include <iomanip>
#include <string>
#include <vector>
#include <esp_system.h>
#include "esp_wifi.h"

static const uint8_t PAIRING_CODE = 99;
Device device;

void deviceSetup() {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    device.setMACAddress(mac);
}

// User State

Device::Device() : userState(0), inbox(), carryMsg(), peerList() {
    // Initialize peerList with two addresses
    // MAC: 30:ED:A0:A8:CE:D0, initials: "RR"
    // MAC: 48:27:E2:1E:55:B0, initials: "RL"
    uint8_t mac1[6] = {0x30, 0xED, 0xA0, 0xA8, 0xCE, 0xD0};
    uint8_t mac2[6] = {0x48, 0x27, 0xE2, 0x1E, 0x55, 0xB0};
    addPeer(mac1, "RR");
    addPeer(mac2, "RL");
}
// Message management
std::vector<MessageStruct>& Device::getInbox() {
    return inbox;
}

std::vector<MessageStruct>& Device::getCarryMsg() {
    return carryMsg;
}

// Track minutes since received for each inbox message
const std::vector<uint16_t>& Device::getInboxReceivedMins() const {
    return inboxReceivedMins;
}

// Add or update a message in carryMsg (FIFO, by sender MAC)
void Device::addOrUpdateCarryMsg(const MessageStruct& msg) {
    if (msg.code == PAIRING_CODE) return;

    auto it = std::find_if(carryMsg.begin(), carryMsg.end(), [&](const MessageStruct& m) {
        return memcmp(m.sender, msg.sender, MAC_SIZE) == 0;
    });
    if (it != carryMsg.end()) {
        *it = msg;
    } else {
        if ((int)carryMsg.size() >= CARRY_LIMIT) {
            carryMsg.erase(carryMsg.begin());
        }
        carryMsg.push_back(msg);
    }
}

// Add or update a message in inbox (by sender MAC, only if sender is peer)
void Device::addOrUpdateInboxIfPeer(const MessageStruct& msg) {
    if (!isPeer(msg.sender)) return;
    if (msg.code == PAIRING_CODE) return; // Don't add pairing messages to inbox
    auto it = std::find_if(inbox.begin(), inbox.end(), [&](const MessageStruct& m) {
        return memcmp(m.sender, msg.sender, MAC_SIZE) == 0;
    });
    uint16_t now_min = (uint16_t)(millis() / 60000);
    if (it != inbox.end()) {
        size_t idx = std::distance(inbox.begin(), it);
        *it = msg;
        if (idx < inboxReceivedMins.size()) {
            inboxReceivedMins[idx] = now_min;
        }
    } else {
        inbox.push_back(msg);
        inboxReceivedMins.push_back(now_min);
    }
}

void Device::setUserState(uint8_t state) {
    userState = state;
}

uint8_t Device::getUserState() const {
    return userState;
}

// Device MAC address
void Device::setMACAddress(const uint8_t* mac) {
    memcpy(macAddress, mac, MAC_SIZE);
}

const uint8_t* Device::getMACAddress() const {
    return macAddress;
}

// Peer Management

// Modified peerList to store PeerInfo
void Device::addPeer(const uint8_t* macAddress, const std::string& initials) {
    std::string peerInitials = initials;
    if (peerInitials.empty()) {
        // Default to last two hex digits of MAC
        std::ostringstream oss;
        oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
            << static_cast<int>(macAddress[MAC_SIZE-2])
            << static_cast<int>(macAddress[MAC_SIZE-1]);
        peerInitials = oss.str();
    }
    PeerInfo info;
    memcpy(info.mac, macAddress, MAC_SIZE);
    info.initials = peerInitials;
    peerList.push_back(info);
}

const std::vector<Device::PeerInfo>& Device::getPeerList() const {
    return peerList;
}

bool Device::isPeer(const uint8_t* macAddress) const {
    for (const auto& peer : peerList) {
        if (memcmp(peer.mac, macAddress, MAC_SIZE) == 0) {
            return true;
        }
    }
    return false;
}


std::string Device::MACToInitials(const uint8_t* macAddress) const {
    for (const auto& peer : peerList) {
        if (memcmp(peer.mac, macAddress, MAC_SIZE) == 0) {
            return peer.initials;
        }
    }
    // Return last two hex digits of MAC address if not found
    std::ostringstream oss;
    oss << std::hex << std::uppercase << std::setw(2) << std::setfill('0')
        << static_cast<int>(macAddress[MAC_SIZE-2])
        << static_cast<int>(macAddress[MAC_SIZE-1]);
    return oss.str();
}

// When displaying MAC address, use:
// std::string macStr = macToString(macAddress, 6);