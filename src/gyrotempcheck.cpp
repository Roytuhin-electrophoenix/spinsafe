#include <Wire.h>
#include <OneWire.h>
#include <DallasTemperature.h>

#define PIN_TEMP 4 
const int MPU_ADDR = 0x68;

OneWire oneWire(PIN_TEMP);
DallasTemperature sensors(&oneWire);

float vibX = 0, vibY = 0, vibZ = 0; 
float temperature = 0;        

unsigned long lastLoopTime = 0;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);

  // 1. Wake up the MPU6050 manually (Bypass!)
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  
  Wire.write(0);     
  Wire.endTransmission(true);

  // 2. Start the Temp Sensor
  sensors.begin();
  sensors.requestTemperatures();
  
  Serial.println("Hour 1: Sensors Ready!");
}

void loop() {
  if (millis() - lastLoopTime >= 1000) {
    lastLoopTime = millis();

    // 1. Temperature Data
    sensors.requestTemperatures(); 
    temperature = sensors.getTempCByIndex(0);

    // 2. Vibration Data Bypass
    Wire.beginTransmission(MPU_ADDR);
    Wire.write(0x3B);  
    Wire.endTransmission(false);
    Wire.requestFrom(MPU_ADDR, 6, true);  
    
    if (Wire.available() == 6) {
      int16_t rawX = Wire.read() << 8 | Wire.read();  
      int16_t rawY = Wire.read() << 8 | Wire.read();  
      int16_t rawZ = Wire.read() << 8 | Wire.read();  

      vibX = abs(rawX / 16384.0);
      vibY = abs(rawY / 16384.0);
      vibZ = abs(rawZ / 16384.0);
    }

    // 3. Plotter Print
    Serial.print("Temp:"); Serial.print(temperature); Serial.print(",");
    Serial.print("X:"); Serial.print(vibX); Serial.print(",");
    Serial.print("Y:"); Serial.print(vibY); Serial.print(",");
    Serial.print("Z:"); Serial.println(vibZ);
  }
}