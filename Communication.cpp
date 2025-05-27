#define DEBUG_MODE 1
// filepath: /Users/kenglien/Documents/Arduino/HikingBoard/EspCommunication.cpp
#include <Communication.h>
#include "Display.h"
#include "Device.h"
#include "Message.h"
#include <esp_now.h>
#include <WiFi.h>
#include <vector>
#include <cstring>
#include "Utility.h"
static const uint8_t PAIRING_CODE = 99;
const uint8_t broadcastAddress[MAC_SIZE] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

// Placeholder for data send callback
void dataSendCallback(const uint8_t *mac_addr, esp_now_send_status_t status) {
    if (status == ESP_NOW_SEND_SUCCESS) {
        //digitalWrite(RED_LED_PIN, HIGH); // Turn on blue LED
    } else {
        //digitalWrite(RED_LED_PIN, LOW);  // Turn off blue LED on failure
        Serial.println("Send Status: Fail");
    }
}

// Placeholder for data receive callback (updated signature for ESP-NOW v5)
void dataRecvCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len) {
  ParseMessages(data, data_len);
}

void espSetup() {
    // Initialize ESP-NOW
    WiFi.mode(WIFI_STA);
    if (esp_now_init() != ESP_OK) {
        // Initialization failed
        Serial.println("ESP-NOW initialization failed");
        return;
    }
    // Register send callback
    esp_now_register_send_cb(dataSendCallback);
    // Register receive callback
    esp_now_register_recv_cb(dataRecvCallback);
    // Initiate ESP-NOW 
    esp_now_peer_info_t peerInfo;
    memcpy(peerInfo.peer_addr, broadcastAddress, MAC_SIZE);
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    peerInfo.ifidx = WIFI_IF_STA;
    memset(peerInfo.lmk, 0, sizeof(peerInfo.lmk));
    if (esp_now_add_peer(&peerInfo) != ESP_OK){
        Serial.println("Failed to add peer");
        return;
    }
}

void broadcastMessages() {
    // Get user state and MAC address
    MessageStruct selfMsg;
    selfMsg.code = device.getUserState();
    const uint8_t* deviceMAC = device.getMACAddress(); 
    memcpy(selfMsg.sender, deviceMAC, MAC_SIZE);

    // Prepare broadcast payload: selfMsg + carryMsg
    std::vector<MessageStruct> payload;
    payload.push_back(selfMsg);
    std::vector<MessageStruct>& carry = device.getCarryMsg();
    payload.insert(payload.end(), carry.begin(), carry.end());

#if DEBUG_MODE
    Serial.print("[DEBUG] Broadcast payload size: ");
    Serial.println(payload.size());
    for (size_t i = 0; i < payload.size(); ++i) {
        Serial.print("[DEBUG] Msg ");
        Serial.print(i);
        Serial.print(": code=");
        Serial.print(payload[i].code);
        Serial.print(", sender=");
        for (int j = 0; j < MAC_SIZE; ++j) {
            if (j > 0) Serial.print(":");
            Serial.printf("%02X", payload[i].sender[j]);
        }
        Serial.println();
    }
#endif

    // Send via ESP-NOW
    // Convert payload to flat buffer for ESP-NOW
    std::vector<uint8_t> flatBuf;
    size_t totalSize = payload.size() * (MAC_SIZE + sizeof(uint8_t));
    flatBuf.reserve(totalSize); // Pre-allocate for efficiency
    
    for (const auto& msg : payload) {
        // Insert MAC address bytes (6 bytes)
        flatBuf.insert(flatBuf.end(), msg.sender, msg.sender + MAC_SIZE);
        
        // Insert code as 1 byte
        flatBuf.push_back(msg.code);
    }
    
    Serial.print("[DEBUG] ESP-NOW payload size: ");
    Serial.println(flatBuf.size());
    esp_err_t result = esp_now_send(broadcastAddress, flatBuf.data(), flatBuf.size());
    if (result != ESP_OK) {
        Serial.print("ESP-NOW send failed, error code: ");
        Serial.println(result);
    }
}

// Parse received messages and update carryMsg and inbox
void ParseMessages(const uint8_t* data, int data_len) {
    const int singleMsgSize = MAC_SIZE + sizeof(uint8_t);

    if (data_len % singleMsgSize != 0) return; // Invalid payload
    int msgCount = data_len / singleMsgSize;
    if (msgCount < 1) return;
    std::vector<MessageStruct> msgs(msgCount);
    // Parse each message
    for (int i = 0; i < msgCount; i++) {
        size_t offset = i * singleMsgSize;
        
        // Bounds check
        if (offset + singleMsgSize > static_cast<size_t>(data_len)) {
            Serial.println("[ERROR] Message parsing bounds exceeded");
            return;
        }
        
        // Copy MAC address to sender array (6 bytes)
        memcpy(msgs[i].sender, data + offset, MAC_SIZE);
        
        // Copy code value (1 byte)
        msgs[i].code = *(data + offset + MAC_SIZE);
#if DEBUG_MODE
        Serial.print("[DEBUG] Parsed Msg ");
        Serial.print(i);
        Serial.print(": code=");
        Serial.print(msgs[i].code);
        Serial.print(", sender=");
        for (int j = 0; j < MAC_SIZE; j++) {
            if (j > 0) Serial.print(":");
            Serial.printf("%02X", msgs[i].sender[j]);
        }
        Serial.println();
#endif
    }
    // --- CarryMsg FIFO update ---
    device.addOrUpdateCarryMsg(msgs[0]); 
    device.checkPairingRequest(msgs[0]); 
    // --- Inbox update (local storage, unique by sender MAC, only if peer) ---
    for (int i = 0; i < msgCount; i++) {
        device.addOrUpdateInboxIfPeer(msgs[i]);
    }
}