#include <Arduino.h>
#include "ButtonInput.h"

const int RIGHT_BTN_PIN = 41;
const int LEFT_BTN_PIN = 42;
const int SLCT_BTN_PIN = 40;

// Internal state for edge detection
static int lastRightState = LOW;
static int lastLeftState = LOW;
static int lastSelectState = LOW;

void buttonSetup(){
  pinMode(RIGHT_BTN_PIN, INPUT);
  pinMode(LEFT_BTN_PIN, INPUT);
  pinMode(SLCT_BTN_PIN, INPUT);
  lastRightState = digitalRead(RIGHT_BTN_PIN);
  lastLeftState = digitalRead(LEFT_BTN_PIN);
  lastSelectState = digitalRead(SLCT_BTN_PIN);
}

// Returns true only on the rising edge (LOW -> HIGH)
bool isButtonClicked(int pin) {
  int currentState = digitalRead(pin);
  bool clicked = false;
  if (pin == RIGHT_BTN_PIN) {
    clicked = (lastRightState == LOW && currentState == HIGH);
    lastRightState = currentState;
  } else if (pin == LEFT_BTN_PIN) {
    clicked = (lastLeftState == LOW && currentState == HIGH);
    lastLeftState = currentState;
  } else if (pin == SLCT_BTN_PIN) {
    clicked = (lastSelectState == LOW && currentState == HIGH);
    lastSelectState = currentState;
  }
  return clicked;
}



