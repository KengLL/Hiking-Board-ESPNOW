#include "Message.h"

const char* MessageMapping(int code) {
    switch (code) {
        case 0: return "OK";
        case 1: return "WAIT";
        case 2: return "GO ON";
        case 3: return "RETREAT";
        case 4: return "INJURED";
        case 5: return "NEED SUPPLIES";
        case 6: return "AT MEET PT";
        case 7: return "SOS";
    }
    return "";
}