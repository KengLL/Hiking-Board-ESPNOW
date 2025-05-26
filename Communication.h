// filepath: /Users/kenglien/Documents/Arduino/HikingBoard/EspCommunication.h
#ifndef ESP_COMMUNICATION_H
#define ESP_COMMUNICATION_H

#define MAC_SIZE 6

#include <Arduino.h>
#include <stdint.h>
#include <vector>
#include <cstring>
#include <esp_now.h>
#include <WiFi.h>
#include "Message.h"

// Forward declaration to avoid circular dependency
struct MessageStruct;

extern const uint8_t broadcastAddress[MAC_SIZE];
extern void checkPairingRequest(const MessageStruct& msg);

void dataSendCallback(const uint8_t *mac_addr, esp_now_send_status_t status);
void dataRecvCallback(const esp_now_recv_info_t *recv_info, const uint8_t *data, int data_len);
void espSetup();
void broadcastMessages();
void ParseMessages(const uint8_t *data, int data_len);

#endif // ESP_COMMUNICATION_H