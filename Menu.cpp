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
    PAIRING_MODE,
    PAIRING_REQUEST,
    PAIRING_KEYBOARD,
    PEER_LIST // Add new state
};

static MenuState menuState = MAIN_MENU;
static int menuIndex = 0;
static const char* menuItems[] = {"Inbox", "Send Msg", "Pairing", "Peers"};
static const int menuCount = 4;

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
        display.setTextSize(2);
        display.setTextColor(SSD1306_WHITE);
        String noMsg = "InboxEmpty";
        int16_t x1, y1;
        uint16_t w, h;
        display.getTextBounds(noMsg, 0, 0, &x1, &y1, &w, &h);
        display.setCursor((display.width() - w) / 2, (display.height() - h) / 2);
        display.print(noMsg);

        // Bottom: "Back"
        int bottomY = display.height() - 10;
        display.setTextSize(1);
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
static char initials[3] = {'_', '_', '\0'};
static int keyboardPos = 0; // 0 or 1

// 3-row keyboard: 26 letters, last char is delete
static const char keyboard[3][9] = {
    {'A','B','C','D','E','F','G','H','I'},
    {'J','K','L','M','N','O','P','Q','R'},
    {'S','T','U','V','W','X','Y','Z','<'} // '<' means delete
};
static int kbRow = 1; // Start at row 1 (N)
static int kbCol = 4; // Start at col 4 (N)

static uint8_t prevUserState = 0;

static void showInitialsKeyboard() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // Top: Initials (show '_' for unset)
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

static void showPairingMode() {
    display.clearDisplay();
    // Top: own MAC
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    String macStr = macToString_Arduino(device.getMACAddress(), MAC_SIZE);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(macStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 0);
    display.print(macStr);

    // Middle: pairing status
    display.setTextSize(2);
    String middle = "Pairing...";
    display.getTextBounds(middle, 0, 0, &x1, &y1, &w, &h);
    int middleY = (display.height() - h) / 2;
    display.setCursor((display.width() - w) / 2, middleY);
    display.print(middle);

    // Bottom: Back
    display.setTextSize(1);
    int bottomY = display.height() - 10;
    display.setCursor(0, bottomY);
    display.print("Back");

    display.display();
}

static void showPairingRequest() {
    display.clearDisplay();
    // Top: own MAC
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);
    String macStr = macToString_Arduino(device.getMACAddress(), MAC_SIZE);
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(macStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, 0);
    display.print(macStr);

    // Middle: Requesting device MAC
    display.setTextSize(1);
    String middle;
    if (device.hasPendingPairMAC()) {
        middle = macToString_Arduino(device.getPendingPairMAC());
    } else {
        middle = "No Request";
    }
    display.getTextBounds(middle, 0, 0, &x1, &y1, &w, &h);
    int middleY = (display.height() - h) / 2;
    display.setCursor((display.width() - w) / 2, middleY);
    display.print(middle);

    // Bottom: x (decline), v (accept), and "Pair?" centered in font size 2
    display.setTextSize(2);
    // x on left
    String xStr = "x";
    display.getTextBounds(xStr, 0, 0, &x1, &y1, &w, &h);
    int iconY = display.height() - h;
    display.setCursor(0, iconY);
    display.print(xStr);
    // v on right
    String v = "v";
    display.getTextBounds(v, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(display.width() - w, iconY);
    display.print(v);
    // "Pair?" centered, font size 2
    String pairStr = "Pair?";
    display.getTextBounds(pairStr, 0, 0, &x1, &y1, &w, &h);
    int pairY = display.height() - h; // align bottom
    display.setCursor((display.width() - w) / 2, pairY);
    display.print(pairStr);

    display.display();
}

static bool pairingConfirmed = false; // Add this flag

static void showPairingConfirmed() {
    display.clearDisplay();
    display.setTextSize(2);
    display.setTextColor(SSD1306_WHITE);
    String msg = String(initials[0]) + String(initials[1]) + " Added!";
    int16_t x1, y1; uint16_t w, h;
    display.getTextBounds(msg, 0, 0, &x1, &y1, &w, &h);
    display.setCursor((display.width() - w) / 2, (display.height() - h) / 2);
    display.print(msg);
    display.display();
    initials[0] = '_'; initials[1] = '_'; initials[2] = '\0';
}

static int peerListIndex = 0; // For navigating peer list

static void showPeerList() {
    display.clearDisplay();
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    const auto& peers = device.getPeerList();
    // Skip the first peer (assumed to be broadcast)
    int peerCount = peers.size() > 1 ? peers.size() - 1 : 0;

    int16_t x1, y1; uint16_t w, h;
    int y = 0;

    // Show up to 4 peers per page (adjust as needed)
    int itemsPerPage = 4;
    int pageStart = (peerListIndex / itemsPerPage) * itemsPerPage;
    int pageEnd = min(pageStart + itemsPerPage, peerCount);

    for (int idx = pageStart; idx < pageEnd; ++idx) {
        int i = idx + 1; // skip index 0 (broadcast)
        String ini = String(peers[i].initials.c_str());
        String mac = macToString_Arduino(peers[i].mac, MAC_SIZE);
        String line = ini + ": " + mac;
        display.getTextBounds(line, 0, 0, &x1, &y1, &w, &h);
        display.setCursor(0, y);
        if (idx == peerListIndex) {
            display.setTextColor(SSD1306_BLACK, SSD1306_WHITE); // Highlight
        } else {
            display.setTextColor(SSD1306_WHITE, SSD1306_BLACK);
        }
        display.print(line);
        y += 12;
    }

    // "Clear All" option at the end
    display.setTextColor(peerListIndex == peerCount ? SSD1306_BLACK : SSD1306_WHITE,
                        peerListIndex == peerCount ? SSD1306_WHITE : SSD1306_BLACK);
    String clearStr = "Clear All";
    display.getTextBounds(clearStr, 0, 0, &x1, &y1, &w, &h);
    display.setCursor(0, y);
    display.print(clearStr);

    display.setTextColor(SSD1306_WHITE, SSD1306_BLACK); // Restore

    display.display();
}

void menuSetup() {
    showMainMenu();
    device.clearPendingPairMAC();
    device.clearDeclinedPairMACs();
    initials[0] = '_'; initials[1] = '_'; initials[2] = '\0';
    keyboardPos = 0;
    kbRow = 1; kbCol = 4; // Start at 'N'
    prevUserState = 0;
    peerListIndex = 0;
}

void menuLoop() {
    switch (menuState) {
        case MAIN_MENU:
            if (isButtonClicked(RIGHT_BTN_PIN)) {
                menuIndex = (menuIndex + 1) % menuCount;
                showMainMenu();
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                menuIndex = (menuIndex - 1 + menuCount) % menuCount;
                showMainMenu();
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
                    case 3:
                        peerListIndex = 0;
                        menuState = PEER_LIST;
                        showPeerList();
                        break;
                }
            }
            break;
        case INBOX: {
            const auto& inbox = device.getInbox();
            int inboxSize = inbox.size();
            if (device.inboxUpdated){
                showInbox();
                device.inboxUpdated = false; // Reset update flag
            }
            if (isButtonClicked(RIGHT_BTN_PIN) && inboxSize > 0) {
                inboxIndex = (inboxIndex + 1) % inboxSize;
                showInbox();
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                menuState = MAIN_MENU;
                showMainMenu();
            } else if (isButtonClicked(SLCT_BTN_PIN) && inboxSize > 0) {
                // Optionally, treat select as back
                inboxIndex = (inboxIndex - 1) % inboxSize;
                showInbox();
            }
            break;
        }
        case PAIRING_MODE:
        {
            // If a pending pair MAC appears, transition to pairing request
            if (device.hasPendingPairMAC()) {
                menuState = PAIRING_REQUEST;
                showPairingRequest();
                break;
            }
            // Normal pairing mode UI and controls
            if (isButtonClicked(LEFT_BTN_PIN)) {
                menuState = MAIN_MENU;
                showMainMenu();
                // Reset pairing state
                device.clearPendingPairMAC();
                device.clearDeclinedPairMACs();
                initials[0] = '_'; initials[1] = '_'; initials[2] = '\0';
                keyboardPos = 0;
                kbRow = 1; kbCol = 4; // Reset to 'N'
                // Restore user state to previous (only if currently PAIRING_CODE)
                if (device.getUserState() == PAIRING_CODE) {
                    device.setUserState(prevUserState);
                }
                break;
            }
            break;
        }
        case PAIRING_REQUEST:
        {
            // Handle pairing request acceptance or decline
            if (isButtonClicked(RIGHT_BTN_PIN)) {
                // Decline: add to declined list, clear pending, return to pairing mode
                if (device.hasPendingPairMAC()) {
                    device.addDeclinedPairMAC(device.getPendingPairMAC());
                    device.clearPendingPairMAC();
                }
                menuState = PAIRING_MODE;
                showPairingMode();
            } else if (isButtonClicked(SLCT_BTN_PIN)) {
                // Accept: transition to keyboard entry
                if (device.hasPendingPairMAC()) {
                    menuState = PAIRING_KEYBOARD;
                    keyboardPos = 0;
                    kbRow = 1; kbCol = 4; // Reset to 'N'
                    initials[0] = '_'; initials[1] = '_'; initials[2] = '\0';
                    showInitialsKeyboard();
                } else {
                    // No pending MAC, return to pairing mode
                    menuState = PAIRING_MODE;
                    showPairingMode();
                }
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                // Optional: treat select as back to pairing mode
                menuState = MAIN_MENU;
                device.clearPendingPairMAC();
                showMainMenu();
            }
            break;
        }
        case PAIRING_KEYBOARD:
        {
            if (pairingConfirmed) {
                // Wait for any button, then return to pairing mode
                if (isButtonClicked(LEFT_BTN_PIN) || isButtonClicked(RIGHT_BTN_PIN) || isButtonClicked(SLCT_BTN_PIN)) {
                    pairingConfirmed = false;
                    menuState = PAIRING_MODE;
                    showPairingMode();
                }
                break;
            }
            // Keyboard navigation and input
            if (isButtonClicked(LEFT_BTN_PIN)) {
                // Move up a row (wrap)
                kbRow = (kbRow + 1) % 3;
                showInitialsKeyboard();
            } else if (isButtonClicked(RIGHT_BTN_PIN)) {
                // Move right a column (wrap)
                kbCol = (kbCol + 1) % 9;
                showInitialsKeyboard();
            } else if (isButtonClicked(SLCT_BTN_PIN)) {
                char selected = keyboard[kbRow][kbCol];
                if (selected == '<') {
                    // Delete last character
                    if (keyboardPos > 0) {
                        --keyboardPos;
                        initials[keyboardPos] = '_';
                    }
                    showInitialsKeyboard();
                } else {
                    initials[keyboardPos] = selected;
                    if (keyboardPos == 0) {
                        keyboardPos = 1;
                        kbRow = 1; kbCol = 4; // Reset to 'N'
                        showInitialsKeyboard();
                    } else {
                        // Confirm initials, add to peer list
                        {
                            std::array<uint8_t, MAC_SIZE> mac = device.getPendingPairMAC();
                            device.addPeer(mac.data(), String(initials).c_str());
                        }
                        keyboardPos = 0;
                        kbRow = 1; kbCol = 4; // Reset to 'N'
                        // Clear pending MAC
                        device.clearPendingPairMAC();
                        pairingConfirmed = true;
                        showPairingConfirmed();
                        // Wait for button press to return to pairing mode
                    }
                }
            }
            break;
        }
        case MSG_SELECT:
            if (isButtonClicked(RIGHT_BTN_PIN)) {
                msgSelectIndex = (msgSelectIndex + 1) % msgCount;
                showMsgSelect();
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                menuState = MAIN_MENU;
                showMainMenu();
            } else if (isButtonClicked(SLCT_BTN_PIN)) {
                device.setUserState(msgSelectIndex);
                menuState = MSG_SELECT;
                showMsgSelect();
            }
            break;
        case PEER_LIST: {
            const auto& peers = device.getPeerList();
            // Skip the first peer (assumed to be broadcast)
            int peerCount = peers.size() > 1 ? peers.size() - 1 : 0;
            int totalItems = peerCount + 1; // +1 for "Clear All"
            if (isButtonClicked(RIGHT_BTN_PIN)) {
                peerListIndex = (peerListIndex + 1) % totalItems;
                showPeerList();
            } else if (isButtonClicked(LEFT_BTN_PIN)) {
                // Left (up) returns to menu, does not iterate
                menuState = MAIN_MENU;
                showMainMenu();
            } else if (isButtonClicked(SLCT_BTN_PIN)) {
                if (peerListIndex == peerCount) {
                    // "Clear All" selected
                    device.clearPeerList();
                    peerListIndex = 0;
                    showPeerList();
                } else if (peerListIndex < peerCount) {
                    // Remove specific peer (indexing: +1 for broadcast)
                    device.removePeerByIndex(peerListIndex + 1);
                    // After removal, clamp index if needed
                    const auto& peers = device.getPeerList();
                    int newPeerCount = peers.size() > 1 ? peers.size() - 1 : 0;
                    if (peerListIndex >= newPeerCount) peerListIndex = 0;
                    showPeerList();
                }
            }
            break;
        }
    }
}