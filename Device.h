#ifndef DEVICE_H
#define DEVICE_H

#include <stdint.h>
#include <vector>
#include <map>
#include <string>

#include "Message.h"

class Device;
extern Device device;
void deviceSetup();

class Device {
public:
    Device();

    // User state (formerly device state)
    void setUserState(int state);
    int getUserState() const;

    // Device MAC address
    void setMACAddress(const uint8_t* mac);
    const std::vector<uint8_t>& getMACAddress() const;

    // Peer management
    struct PeerInfo {
        std::vector<uint8_t> mac;
        std::string initials;
    };
    
    void addPeer(const uint8_t* macAddress, const std::string& initials = "");
    const std::vector<PeerInfo>& getPeerList() const;
    //bool isPeer(const uint8_t* macAddress) const;
    bool isPeer(const std::vector<uint8_t>& mac) const;
    std::string MACToInitials(const uint8_t *macAddress) const;

    // Message management
    std::vector<MessageStruct>& getInbox();
    std::vector<MessageStruct>& getCarryMsg();
    // Add or update a message in carryMsg (FIFO, by sender MAC)
    void addOrUpdateCarryMsg(const MessageStruct& msg, int limit = 10);
    // Add or update a message in inbox (by sender MAC, only if sender is peer)
    void addOrUpdateInboxIfPeer(const MessageStruct& msg);

private:
    int userState;
    std::vector<uint8_t> macAddress; // Device MAC address
    std::vector<PeerInfo> peerList; // List of peers (MAC + initials)
    std::vector<MessageStruct> inbox;
    std::vector<MessageStruct> carryMsg;
};

#endif // DEVICE_H