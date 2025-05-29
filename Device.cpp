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
#include <nvs.h>
#include <nvs_flash.h>

static const uint8_t PAIRING_CODE = 99;
Device device;

void deviceSetup() {
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    device.setMACAddress(mac);
}

// User State

Device::Device() : userState(0), inbox(), carryMsg(), peerList() {
    uint8_t broadcast[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    addPeer(broadcast, "BB"); 
    // Initialize peerList with two addresses
    // MAC: 30:ED:A0:A8:CE:D0, initials: "RR"
    // MAC: 48:27:E2:1E:55:B0, initials: "RL"
    //uint8_t mac1[6] = {0x30, 0xED, 0xA0, 0xA8, 0xCE, 0xD0};
    //uint8_t mac2[6] = {0x48, 0x27, 0xE2, 0x1E, 0x55, 0xB0};
    //addPeer(mac1, "RR");
    //addPeer(mac2, "RL");
    // Load from NVS on construction
    nvs_flash_init();
    loadFromNVS();
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
    uint16_t now_min = (uint16_t)(millis() / 60000);

    // Only match sender and code, ignore data
    auto sameMsgIt = std::find_if(inbox.begin(), inbox.end(), [&](const MessageStruct& m) {
        return memcmp(m.sender, msg.sender, MAC_SIZE) == 0 &&
               m.code == msg.code;
    });

    if (sameMsgIt != inbox.end()) {
        size_t idx = std::distance(inbox.begin(), sameMsgIt);
        if (idx < inboxReceivedMins.size()) {
            inboxReceivedMins[idx] = now_min;
        }
    } else {
        inbox.push_back(msg);
        inboxReceivedMins.push_back(now_min);
    }
    device.inboxUpdated = true;
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
    device.clearPendingPairMAC();
    saveToNVS(); // Save peers after change
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

// Call this from ParseMessages when in pairing mode
void Device::checkPairingRequest(const MessageStruct& msg) {
    if (getUserState() != PAIRING_CODE || msg.code != PAIRING_CODE) return;
    if (isPeer(msg.sender)) return;
    std::array<uint8_t, MAC_SIZE> macArr;
    memcpy(macArr.data(), msg.sender, MAC_SIZE);
    if (isDeclinedPairMAC(macArr)) return;
    setPendingPairMAC(macArr);
}

void Device::setPendingPairMAC(const std::array<uint8_t, MAC_SIZE>& mac) {
    pendingPairMAC = mac;
    pendingPair = true;
}

void Device::clearPendingPairMAC() {
    pendingPairMAC.fill(0);
    pendingPair = false;
}

const std::array<uint8_t, MAC_SIZE>& Device::getPendingPairMAC() const {
    return pendingPairMAC;
}

bool Device::hasPendingPairMAC() const {
    return pendingPair;
}

void Device::addDeclinedPairMAC(const std::array<uint8_t, MAC_SIZE>& mac) {
    if (!isDeclinedPairMAC(mac)) {
        declinedPairMACs.push_back(mac);
    }
}

void Device::clearDeclinedPairMACs() {
    declinedPairMACs.clear();
}

const std::vector<std::array<uint8_t, MAC_SIZE>>& Device::getDeclinedPairMACs() const {
    return declinedPairMACs;
}

bool Device::isDeclinedPairMAC(const std::array<uint8_t, MAC_SIZE>& mac) const {
    for (const auto& d : declinedPairMACs) {
        if (d == mac) return true;
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

// Helper functions for NVS serialization
static const char* NVS_NAMESPACE = "hiking";
static const char* NVS_KEY_INBOX = "inbox";
static const char* NVS_KEY_INBOX_TIME = "inbox_time";
static const char* NVS_KEY_PEERS = "peers";

// Save inbox and peer list to NVS
void Device::saveToNVS() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle) != ESP_OK) return;

    // Save inbox
    size_t inboxLen = inbox.size() * sizeof(MessageStruct);
    nvs_set_blob(handle, NVS_KEY_INBOX, inbox.data(), inboxLen);

    // Save inboxReceivedMins
    size_t minsLen = inboxReceivedMins.size() * sizeof(uint16_t);
    nvs_set_blob(handle, NVS_KEY_INBOX_TIME, inboxReceivedMins.data(), minsLen);

    // Save peerList
    // Store as: [PeerInfo][PeerInfo]...
    size_t peerCount = peerList.size();
    struct PeerNVS {
        uint8_t mac[MAC_SIZE];
        char initials[3];
    };
    std::vector<PeerNVS> peersNVS;
    for (const auto& peer : peerList) {
        PeerNVS p;
        memcpy(p.mac, peer.mac, MAC_SIZE);
        strncpy(p.initials, peer.initials.c_str(), 2);
        p.initials[2] = '\0';
        peersNVS.push_back(p);
    }
    nvs_set_blob(handle, NVS_KEY_PEERS, peersNVS.data(), peersNVS.size() * sizeof(PeerNVS));

    nvs_commit(handle);
    nvs_close(handle);
}

// Load inbox and peer list from NVS
void Device::loadFromNVS() {
    nvs_handle_t handle;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle) != ESP_OK) return;

    // Load inbox
    size_t inboxLen = 0;
    if (nvs_get_blob(handle, NVS_KEY_INBOX, NULL, &inboxLen) == ESP_OK && inboxLen % sizeof(MessageStruct) == 0) {
        inbox.resize(inboxLen / sizeof(MessageStruct));
        nvs_get_blob(handle, NVS_KEY_INBOX, inbox.data(), &inboxLen);
    }

    // Load inboxReceivedMins
    size_t minsLen = 0;
    if (nvs_get_blob(handle, NVS_KEY_INBOX_TIME, NULL, &minsLen) == ESP_OK && minsLen % sizeof(uint16_t) == 0) {
        inboxReceivedMins.resize(minsLen / sizeof(uint16_t));
        nvs_get_blob(handle, NVS_KEY_INBOX_TIME, inboxReceivedMins.data(), &minsLen);
    }

    // Load peerList
    size_t peersLen = 0;
    struct PeerNVS {
        uint8_t mac[MAC_SIZE];
        char initials[3];
    };
    if (nvs_get_blob(handle, NVS_KEY_PEERS, NULL, &peersLen) == ESP_OK && peersLen % sizeof(PeerNVS) == 0) {
        size_t peerCount = peersLen / sizeof(PeerNVS);
        std::vector<PeerNVS> peersNVS(peerCount);
        nvs_get_blob(handle, NVS_KEY_PEERS, peersNVS.data(), &peersLen);
        peerList.clear();
        for (const auto& p : peersNVS) {
            PeerInfo info;
            memcpy(info.mac, p.mac, MAC_SIZE);
            info.initials = std::string(p.initials);
            peerList.push_back(info);
        }
    }

    nvs_close(handle);
}