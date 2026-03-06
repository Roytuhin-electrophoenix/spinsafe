#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

Adafruit_SSD1306 display(128, 64, &Wire, -1);

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED Failed to start!");
    for(;;);
  }
}
  
void loop() {
  drawScreen("HEALTHY!");
  delay(2000);
  
  drawScreen("FRICTION!");
  delay(2000);
  
  drawScreen("IMBALANCE!");
  delay(2000);
  
  drawScreen("FIRE!");
  delay(2000);
}

void drawScreen(String status) {
  display.clearDisplay();
  display.setTextColor(WHITE);
  
  // Top Box for "SpinSafe" branding
  display.drawRect(0, 0, 128, 22, WHITE); 
  display.setTextSize(2);
  // (128 total pixels - 96 pixels for "SpinSafe") / 2 = 16 pixel margin
  display.setCursor(16, 4); 
  display.print("SpinSafe");

  // Bottom Box for the AI Status
  display.drawRect(0, 26, 128, 38, WHITE); 
  display.setTextSize(2);
  
  // Auto-center the status text based on how long the word is
  int charWidth = 12; // Width of size 2 text characters
  int textWidth = status.length() * charWidth;
  int cursorX = (128 - textWidth) / 2;
  
  // Prevent clipping if a word is slightly too long
  if (cursorX < 0) cursorX = 2; 

  display.setCursor(cursorX, 38); 
  display.print(status);
  
  display.display();
}