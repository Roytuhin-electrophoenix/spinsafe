#include <Wire.h>

const int MPU_ADDR = 0x68;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);  
  Wire.write(0);     
  Wire.endTransmission(true);
  
  Serial.println("Bypass Successful! MPU6050 Awake.");
  delay(1000);
}

void loop() {
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);  
  Wire.endTransmission(false);
  
  Wire.requestFrom(MPU_ADDR, 6, true);  
  
  if (Wire.available() == 6) {
    int16_t rawX = Wire.read() << 8 | Wire.read();  
    int16_t rawY = Wire.read() << 8 | Wire.read();  
    int16_t rawZ = Wire.read() << 8 | Wire.read();  

    float vibX = abs(rawX / 16384.0);
    float vibY = abs(rawY / 16384.0);
    float vibZ = abs(rawZ / 16384.0);

    Serial.print("X:"); Serial.print(vibX); Serial.print(",");
    Serial.print("Y:"); Serial.print(vibY); Serial.print(",");
    Serial.print("Z:"); Serial.println(vibZ);
  }
  
  delay(100); 
}
