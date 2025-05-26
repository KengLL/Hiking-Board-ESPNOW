#pragma once

extern const int RIGHT_BTN_PIN;
extern const int LEFT_BTN_PIN;
extern const int SLCT_BTN_PIN;

void buttonSetup();
bool isButtonClicked(int pin);

