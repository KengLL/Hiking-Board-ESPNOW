#include "Display.h"

TwoWire I2C_one = TwoWire(0);
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &I2C_one, OLED_RESET);

void displaySetup(){
  I2C_one.begin(SDA_PIN, SCL_PIN);
  display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS);
}

/*
* Function to display messages on OLED Screen.
* 40 characters max if using text size 2
*/
void displayMsg(const char msg[]){
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0,0);
  display.println(msg);
  display.display();
}