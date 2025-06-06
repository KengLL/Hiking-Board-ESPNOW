
#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <vector>
#include <map>
#include <string>

#include "Message.h"
#define RED_LED_PIN 1
#define CARRY_LIMIT 15
class Device;
extern Device device;
void deviceSetup();

class Device {
public:
    Device();

    // User state (formerly device state)
    void setUserState(uint8_t state);
    uint8_t getUserState() const;

    // Device MAC address
    void setMACAddress(const uint8_t* mac);
    const uint8_t* getMACAddress() const;

    // Peer management
    struct PeerInfo {
        uint8_t mac[MAC_SIZE];
        std::string initials;
    };
    
        // Remove a peer by index (excluding broadcast)
    void removePeerByIndex(int idx);
    void addPeer(const uint8_t* macAddress, const std::string& initials = "");
    bool isPeer(const uint8_t* macAddress) const;
    std::string MACToInitials(const uint8_t *macAddress) const;
    const std::vector<PeerInfo>& getPeerList() const;
    void clearPeerList();
    void clearInbox();

    // Message management
    std::vector<MessageStruct>& getInbox();
    std::vector<MessageStruct>& getCarryMsg();
    // Add or update a message in carryMsg (FIFO, by sender MAC)
    void addOrUpdateCarryMsg(const MessageStruct& msg);
    // Add or update a message in inbox (by sender MAC, only if sender is peer)
    void addOrUpdateInboxIfPeer(const MessageStruct& msg);
    // Pairing request handling
    void checkPairingRequest(const MessageStruct& msg);
    // Pairing MAC management
    void setPendingPairMAC(const std::array<uint8_t, MAC_SIZE>& mac);
    void clearPendingPairMAC();
    const std::array<uint8_t, MAC_SIZE>& getPendingPairMAC() const;
    bool hasPendingPairMAC() const;
    void addDeclinedPairMAC(const std::array<uint8_t, MAC_SIZE>& mac);
    void clearDeclinedPairMACs();
    const std::vector<std::array<uint8_t, MAC_SIZE>>& getDeclinedPairMACs() const;
    bool isDeclinedPairMAC(const std::array<uint8_t, MAC_SIZE>& mac) const;
    // Track minutes since received for each inbox message
    const std::vector<uint16_t>& getInboxReceivedMins() const;
    void saveToNVS();
    void loadFromNVS();
    bool inboxUpdated = false; // Flag to indicate inbox was updated
private:
    uint8_t userState;
    uint8_t macAddress[MAC_SIZE];  // Device MAC address
    std::vector<PeerInfo> peerList; // List of peers (MAC + initials)
    std::vector<MessageStruct> inbox;
    std::vector<MessageStruct> carryMsg;
    std::vector<uint16_t> inboxReceivedMins;
    // Pairing state
    bool pendingPair = false;
    std::array<uint8_t, MAC_SIZE> pendingPairMAC = {0};
    std::vector<std::array<uint8_t, MAC_SIZE>> declinedPairMACs;
};

#endif // DEVICE_H