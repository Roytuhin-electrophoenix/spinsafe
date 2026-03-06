#include <Wire.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>

Adafruit_MPU6050 mpu;

void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  
  if (!mpu.begin(0x68)) {
    Serial.println("MPU6050 Failed");
    for (;;);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
}

void loop() {
  sensors_event_t a, g, temp;
  
  if(mpu.getEvent(&a, &g, &temp)) {
    float vibX = abs(a.acceleration.x); 
    float vibY = abs(a.acceleration.y); 
    float vibZ = abs(a.acceleration.z); 

    Serial.print("X:"); Serial.print(vibX); Serial.print(",");
    Serial.print("Y:"); Serial.print(vibY); Serial.print(",");
    Serial.print("Z:"); Serial.println(vibZ);
  }
  
  delay(100);
}
