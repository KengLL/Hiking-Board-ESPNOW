#include <Arduino.h>
#include "Communication.h"
#include "Display.h"
#include "ButtonInput.h"
#include "Menu.h"
#include "Device.h"

void setup() {
    Serial.begin(115200);
    espSetup();
    deviceSetup();
    buttonSetup();
    //device.setUserState(99);
    displaySetup();
    menuSetup();
}

void loop() {
    // Broadcast messages every 5 seconds, or every 1 second in pairing mode
    static unsigned long lastBroadcast = 0;
    unsigned long now = millis();
    unsigned long broadcastInterval = (device.getUserState() == 99) ? 50 : 750;
    if (now - lastBroadcast >= broadcastInterval) {
        broadcastMessages();
        lastBroadcast = now;
    }
    // Handle button inputs
    menuLoop();

}
