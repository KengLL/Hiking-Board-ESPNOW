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
const uint8_t broadcastAddress[MAC_SIZE] = { 0xFF,0xFF,0xFF,0xFF,0xFF,0xFF };

// Placeholder for data send callback
void dataSendCallback(const uint8_t *mac_addr, esp_now_send_status_t status) {
  if (status != ESP_NOW_SEND_SUCCESS){
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
        displayMsg("ESP Init Error");
        return;
    }
    // Register send callback
    esp_now_register_send_cb(dataSendCallback);
    // Register receive callback
    esp_now_register_recv_cb(dataRecvCallback);
}

void broadcastMessages() {
    // Get user state and MAC address
    MessageStruct selfMsg;
    selfMsg.code = device.getUserState();
    selfMsg.sender = device.getMACAddress();

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
    for (const auto& msg : payload) {
        flatBuf.insert(flatBuf.end(), msg.sender.begin(), msg.sender.end());
        int code = msg.code;
        flatBuf.insert(flatBuf.end(), reinterpret_cast<uint8_t*>(&code), reinterpret_cast<uint8_t*>(&code) + sizeof(int));
    }
    esp_err_t result = esp_now_send(broadcastAddress, flatBuf.data(), flatBuf.size());
    if (result != ESP_OK) {
        Serial.println("ESP-NOW send failed");
    }
}

// Parse received messages and update carryMsg and inbox
void ParseMessages(const uint8_t* data, int data_len) {
    int singleMsgSize = MAC_SIZE + sizeof(int);
    if (data_len % singleMsgSize != 0) return; // Invalid payload
    int msgCount = data_len / singleMsgSize;
    if (msgCount < 1) return;
    std::vector<MessageStruct> msgs(msgCount);
    for (int i = 0; i < msgCount; ++i) {
        int offset = i * singleMsgSize;
        msgs[i].sender.assign(data + offset, data + offset + MAC_SIZE);
        memcpy(&msgs[i].code, data + offset + MAC_SIZE, sizeof(int));
    }
    // --- CarryMsg FIFO update ---
    device.addOrUpdateCarryMsg(msgs[0], 10); // FIFO limit 10
    checkPairingRequest(msgs[0]); // Check pairing request 
    // --- Inbox update (local storage, unique by sender MAC, only if peer) ---
    for (int i = 0; i < msgCount; ++i) {
        device.addOrUpdateInboxIfPeer(msgs[i]);
    }
}