#include <Arduino.h>
#include "Display.h"
#include "ButtonInput.h"
#include "Menu.h"
#include "Device.h"
#include "Communication.h"
#include "Utility.h"
#include "Message.h" // For MessageMapping
#include <set>
#include <algorithm>

enum MenuState {
    MAIN_MENU,
    INBOX,
    MSG_SELECT,
    PAIRING_MODE
};

static MenuState menuState = MAIN_MENU;
static int menuIndex = 0;
static const char* menuItems[] = {"Inbox", "Send Msg", "Pairing"};
static const int menuCount = 3;

// Debounce timing (ms)
static unsigned long lastActionTime = 0;
static const unsigned long debounceDelay = 700; // Increased from 200 to 300ms

static uint8_t msgSelectIndex = 0;
static const uint8_t msgCount = 8; // Only 0-7 valid codes for message select
static const uint8_t PAIRING_CODE = 99;

static int inboxIndex = 0;

static void showMainMenu() {
    // Use font size 2 for bigger text, left align, highlight with '<'
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    int y = 0;
    for (int i = 0; i < menuCount; ++i) {
        display.setCursor(0, y);
        display.print(menuItems[i]);
        if (i == menuIndex) {
            display.print(" <");
        }
        y += 16; // 16 pixels per line for size 2
    }
    display.display();
}

static void showInbox() {
    display.clearDisplay();
    const auto& inbox = device.getInbox();
    int inboxSize = inbox.size();

    // If inbox is empty
    if (inboxSize == 0) {
        display.setTextSize(1);
        display.setTextColor(SSD1306_WHITE);
        String noMsg = "Inbox Empty";
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(noMsg, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((display.width() - w) / 2, (display.height() - h) / 2);
        display.print(noMsg);

        // Bottom: "Back"
        int bottomY = display.height() - 10;
        display.setCursor(0, bottomY);
        display.print("Back");
        display.display();
        return;
    }

    // Clamp inboxIndex
    if (inboxIndex >= inboxSize) inboxIndex = 0;
    if (inboxIndex < 0) inboxIndex = 0;

    const MessageStruct& msg = inbox[inboxIndex];

    // Top left: Sender initials (font size 2)
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    String sender = device.MACToInitials(msg.sender).c_str();
    display.setCursor(0, 0);
    display.print(sender);

    // Top right: Time since received (font size 1)
    display.setTextSize(1);
    const auto& inboxReceivedMins = device.getInboxReceivedMins();
    uint16_t now_min = (uint16_t)(millis() / 60000);
    uint16_t elapsed_min = 0;
    if (inboxIndex < inboxReceivedMins.size()) {
        uint16_t received_min = inboxReceivedMins[inboxIndex];
        if (now_min >= received_min) {
            elapsed_min = now_min - received_min;
        }
    }
    String timeStr;
    if (elapsed_min < 60) {
        timeStr = String(elapsed_min) + "m ago";
    } else {
        timeStr = String(elapsed_min / 60) + "h ago";
    }
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(timeStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(display.width() - w, 0);
    display.print(timeStr);

    // Middle: Message (centered)
    display.setTextSize(2);
    String msgStr = MessageMapping(msg.code);
    display.getTextBounds(msgStr, 0, 0, &x1, &y1, &w, &h);
    int middleY = (display.height() - h) / 2;
    display.setCursor((display.width() - w) / 2, middleY);
    display.print(msgStr);

    // (Time display moved to top right)

    // Bottom: "Back" on left, ">" on right
    display.setTextSize(1);
    int bottomY = display.height() - 10;
    display.setCursor(0, bottomY);
    display.print("Back");
    String arrow = ">";
    display.getTextBounds(arrow, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(display.width() - w, bottomY);
    display.print(arrow);

    display.display();
}

static void showMsgSelect() {
    display.clearDisplay();

    // Top: current user state (centered)
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    String stateStr = String("State: ") + MessageMapping(device.getUserState());
    int16_t x1, y1;
    uint16_t w, h;
    display.getTextBounds(stateStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 0);
    display.print(stateStr);

    // Middle: message option, highlight current (centered vertically)
    display.setTextSize(2);
    //String msgStr = String("< ") + MessageMapping(msgSelectIndex) + " >";
    String msgStr = MessageMapping(msgSelectIndex);
    display.getTextBounds(msgStr, 0, 0, &x1, &y1, &w, &h);
    int middleY = (display.height() - h) / 2;
    display.setCursor((display.width() - w) / 2, middleY);
    display.print(msgStr);

    // Bottom: "Back" on left, ">" on right
    display.setTextSize(1);
    int bottomY = display.height() - 10;
    display.setCursor(0, bottomY);
    display.print("Back");
    String arrow = ">";
    display.getTextBounds(arrow, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(display.width() - w, bottomY);
    display.print(arrow);

    display.display();
}

// Pairing state is now managed by Device
static bool showKeyboard = false;
static char initials[3] = {'A', 'A', '\0'};
static int keyboardPos = 0; // 0 or 1

static uint8_t prevUserState = 0;

// 3-row keyboard: 26 letters, last char is delete
static const char keyboard[3][9] = {
    {'A','B','C','D','E','F','G','H','I'},
    {'J','K','L','M','N','O','P','Q','R'},
    {'S','T','U','V','W','X','Y','Z','<'} // '<' means delete
};
static int kbRow = 0;
static int kbCol = 0;

static void showInitialsKeyboard() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Top: Initials
    String iniStr = "Initials: ";
    for (int i = 0; i < 2; ++i) {
        iniStr += initials[i];
        iniStr += " ";
    }
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(iniStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 0);
    display.print(iniStr);

    // Keyboard rows
    display.setTextSize(2);
    int kbStartY = 18;
    for (int row = 0; row < 3; ++row) {
        int y = kbStartY + row * 16;
        int x = 4;
        for (int col = 0; col < 9; ++col) {
            if (row == kbRow && col == kbCol) {
                display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Highlight: white bg, black text
            } else {
                display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Normal: white text, black bg
            }
            display.setCursor(x, y);
            char keyChar = keyboard[row][col];
            display.print(keyChar);
            x += 14;
        }
    }
    // Restore default text color
    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);

    display.display();
}

// Fix: convert std::string to String using .c_str()
static String macToString_Arduino(const std::vector<uint8_t>& mac) {
    return String(macToString(mac.data(), MAC_SIZE).c_str());
}

static void showPairingMode() {
    display.clearDisplay();



    // Top: own MAC
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    String macStr = String(macToString(device.getMACAddress(), MAC_SIZE).c_str());
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(macStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 0);
    display.print(macStr);

    // Middle: pairing status or found device
    display.setTextSize(2);
    String middle;
    if (!device.hasPendingPairMAC()) {
        middle = "Pairing...";
    } else {
        std::vector<uint8_t> mac(device.getPendingPairMAC().begin(), device.getPendingPairMAC().end());
        middle = macToString_Arduino(mac);
    }
    display.getTextBounds(middle, 0, 0, &x1, &y1, &w, &h);
    int middleY = (display.height() - h) / 2;
    display.setCursor((display.width() - w) / 2, middleY);
    display.print(middle);

    // Bottom: x (back), v (pair), > (reject)
    display.setTextSize(1);
    int bottomY = display.height() - 10;
    display.setCursor(0, bottomY);
    display.print("Back");
    String v = "v";
    display.setCursor(display.width() / 2 - 3, bottomY);
    display.print(v);
    String arrow = ">";
    display.setCursor(display.width() - 8, bottomY);
    display.print(arrow);

    display.display();
}


void menuSetup() {
    showMainMenu();
    device.clearPendingPairMAC();
    device.clearDeclinedPairMACs();
    showKeyboard = false;
    initials[0] = 'A'; initials[1] = 'A'; initials[2] = '\0';
    keyboardPos = 0;
    kbRow = 0;
    kbCol = 0;
    prevUserState = 0;
}

void menuLoop() {
    unsigned long now = millis();
    if (now - lastActionTime < debounceDelay) {
        return; // Debounce: ignore input if not enough time has passed
    }
    switch (menuState) {
        case MAIN_MENU:
            if (isButtonClicked(RIGHT_BTN_PIN)) {
                menuIndex = (menuIndex + 1) % menuCount;
                showMainMenu();
                lastActionTime = now;
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                menuIndex = (menuIndex - 1 + menuCount) % menuCount;
                showMainMenu();
                lastActionTime = now;
            } else if (isButtonClicked(SLCT_BTN_PIN)) {
                switch (menuIndex) {
                    case 0:
                        menuState = INBOX;
                        showInbox();
                        break;
                    case 1:
                        menuState = MSG_SELECT;
                        showMsgSelect();
                        break;
                    case 2:
                        // Only set prevUserState and PAIRING_CODE if not already in pairing mode
                        if (device.getUserState() != PAIRING_CODE) {
                            prevUserState = device.getUserState();
                            device.setUserState(PAIRING_CODE);
                        }
                        menuState = PAIRING_MODE;
                        showPairingMode();
                        break;
                }
                lastActionTime = now;
            }
            break;
        case INBOX: {
            const auto& inbox = device.getInbox();
            int inboxSize = inbox.size();
            if (isButtonClicked(RIGHT_BTN_PIN) && inboxSize > 0) {
                inboxIndex = (inboxIndex + 1) % inboxSize;
                showInbox();
                lastActionTime = now;
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                menuState = MAIN_MENU;
                showMainMenu();
                lastActionTime = now;
            } else if (isButtonClicked(SLCT_BTN_PIN)) {
                // Optionally, treat select as back
                menuState = MAIN_MENU;
                showMainMenu();
                lastActionTime = now;
            }
            break;
        }
        case PAIRING_MODE:
            if (showKeyboard) {
                if (isButtonClicked(LEFT_BTN_PIN)) {
                    // Move up a row (wrap)
                    kbRow = (kbRow + 2) % 3;
                    showInitialsKeyboard();
                    lastActionTime = now;
                } else if (isButtonClicked(RIGHT_BTN_PIN)) {
                    // Move right a column (wrap)
                    kbCol = (kbCol + 1) % 9;
                    showInitialsKeyboard();
                    lastActionTime = now;
                } else if (isButtonClicked(SLCT_BTN_PIN)) {
                    char selected = keyboard[kbRow][kbCol];
                    if (selected == '<') {
                        // Delete last character
                        if (keyboardPos > 0) {
                            --keyboardPos;
                            initials[keyboardPos] = 'A';
                        }
                        showInitialsKeyboard();
                    } else {
                        initials[keyboardPos] = selected;
                        if (keyboardPos == 0) {
                            keyboardPos = 1;
                            kbRow = 0; kbCol = 0;
                            showInitialsKeyboard();
                        } else {
                            // Confirm initials, add to peer list
                            {
                                std::array<uint8_t, MAC_SIZE> mac = device.getPendingPairMAC();
                                device.addPeer(mac.data(), String(initials).c_str());
                                device.clearPendingPairMAC();
                            }
                            showKeyboard = false;
                            keyboardPos = 0;
                            kbRow = 0; kbCol = 0;
                            initials[0] = 'A'; initials[1] = 'A'; initials[2] = '\0';
                            showPairingMode();
                        }
                    }
                    lastActionTime = now;
                }
            } else {
                if (device.hasPendingPairMAC()) {
                    // Show pending pairing MAC
                    showPairingMode();
                }
                if (isButtonClicked(LEFT_BTN_PIN)) {
                    menuState = MAIN_MENU;
                    showMainMenu();
                    // Reset pairing state
                    device.clearPendingPairMAC();
                    device.clearDeclinedPairMACs();
                    showKeyboard = false;
                    initials[0] = 'A'; initials[1] = 'A'; initials[2] = '\0';
                    keyboardPos = 0;
                    kbRow = 0; kbCol = 0;
                    // Restore user state to previous (only if currently PAIRING_CODE)
                    if (device.getUserState() == PAIRING_CODE) {
                        device.setUserState(prevUserState);
                    }
                    lastActionTime = now;
                } else if (isButtonClicked(SLCT_BTN_PIN) && device.hasPendingPairMAC()) {
                    // Start keyboard for initials
                    showKeyboard = true;
                    keyboardPos = 0;
                    kbRow = 0;
                    kbCol = 0;
                    initials[0] = 'A'; initials[1] = 'A'; initials[2] = '\0';
                    showInitialsKeyboard();
                    lastActionTime = now;
                } else if (isButtonClicked(RIGHT_BTN_PIN) && device.hasPendingPairMAC()) {
                    // Reject: add to declined list, clear pending
                    device.addDeclinedPairMAC(device.getPendingPairMAC());
                    device.clearPendingPairMAC();
                    showPairingMode();
                    lastActionTime = now;
                }
            }
            break;
        case MSG_SELECT:
            if (isButtonClicked(RIGHT_BTN_PIN)) {
                msgSelectIndex = (msgSelectIndex + 1) % msgCount;
                showMsgSelect();
                lastActionTime = now;
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                menuState = MAIN_MENU;
                showMainMenu();
                lastActionTime = now;
            } else if (isButtonClicked(SLCT_BTN_PIN)) {
                device.setUserState(msgSelectIndex);
                menuState = MAIN_MENU;
                showMainMenu();
                lastActionTime = now;
            }
            break;
    }
}
