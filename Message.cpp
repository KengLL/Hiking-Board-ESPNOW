#include "Message.h"

const char* MessageMapping(int code) {
    switch (code) {
        case 0: return "NEUTRAL";
        case 1: return "WAIT";
        case 2: return "GO ON";
        case 3: return "RETREAT";
        case 4: return "INJURED";
        case 5: return "LEFT";
        case 6: return "RIGHT";
        case 7: return "SOS";
        case 8: return "GOODBYE";
        case 9: return "CONFIRMED";
    }
    return "";
}