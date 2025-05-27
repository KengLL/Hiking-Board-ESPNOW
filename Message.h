// filepath: /Users/kenglien/Documents/Arduino/HikingBoard/src/Message.h

#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

#include "Communication.h"

struct MessageStruct {
    uint8_t sender[MAC_SIZE];  // MAC address as array
    uint8_t code;
};

const char* MessageMapping(int code);

#endif // MESSAGE_H