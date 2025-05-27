#include <Arduino.h>
#include "Communication.h"
#include "Display.h"
#include "ButtonInput.h"
#include "Menu.h"
#include "Device.h"

void setup() {
    Serial.begin(115200);
    delay(2000);
    espSetup();
    deviceSetup();
    buttonSetup();
    displaySetup();
    menuSetup();
}

void loop() {
    // Broadcast messages every 5 seconds
    static unsigned long lastBroadcast = 0;
    unsigned long now = millis();
    if (now - lastBroadcast >= 5000) {
        broadcastMessages();
        lastBroadcast = now;
    }
    // Handle button inputs
    menuLoop();

}
