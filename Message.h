// filepath: /Users/kenglien/Documents/Arduino/HikingBoard/src/Message.h

#ifndef MESSAGE_H
#define MESSAGE_H

#include <stdint.h>

#include "Communication.h"

struct MessageStruct {
  std::vector<uint8_t> sender;
  int code;
  MessageStruct() : sender(MAC_SIZE, 0), code(0) {}
};

const char* MessageMapping(int code);

#endif // MESSAGE_H