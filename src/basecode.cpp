#define BLYNK_TEMPLATE_ID "TMPL3whH9-q-r"      
#define BLYNK_TEMPLATE_NAME "Motor Health Monitor"
#define BLYNK_AUTH_TOKEN "ovXAFRLuQyWMhrXLYhLoWVmD8qB9zBEB"
#define BLYNK_PRINT Serial 

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <OneWire.h>
#include <DallasTemperature.h>

char ssid[] = "Electrophoenix";        
char pass[] = "TUHINROY2004";  

#define PIN_MOSFET    2      
#define PIN_TEMP      4     
#define PIN_VOLT      35    
#define PIN_SPEED     34    

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(PIN_TEMP);
DallasTemperature sensors(&oneWire);
BlynkTimer timer;

float vibX = 0, vibY = 0, vibZ = 0; 
float temperature = 0;        
float currentAmps = 0;     
int motorSpeed = 0;
int loadPercentage = 0;    

int ai_FaultClass = 0;
int pred_HealthScore = 100;
bool safety_FireRisk = false;

const long STARTUP_GRACE_PERIOD = 2000;
unsigned long lastBlynkUpdate = 0;      

unsigned long motorStartTime = 0;        
bool isLockoutActive = false;            
unsigned long lockoutStartTime = 0;      
const long LOCKOUT_DURATION = 60000;    

float lastTemp = 0.0;
unsigned long lastTempTime = 0;

volatile int pulseCount = 0;
volatile unsigned long lastPulseTime = 0;

String dashLink = "https://blynk.cloud/dashboard/9981NS"; 

void IRAM_ATTR countPulse() {
  unsigned long currentTime = micros(); 
  if (currentTime - lastPulseTime > 2000) {
    pulseCount++;
    lastPulseTime = currentTime;
  }
}

int runMLModel(int rpm, int load, float vib) {
    if (rpm <= 11070.00) {
        if (vib <= 5.48) {
            if (rpm <= 990.00) return 2; 
            else { 
                if (rpm <= 10590.00) return 1; 
                else return 1; 
            }
        } else {
            if (rpm <= 9540.00) {
                if (rpm <= 810.00) return 2; 
                else return 1; 
            } else return 3; 
        }
    } else {
        if (vib <= 1.62) return 0; 
        else {
            if (rpm <= 11130.00 && vib <= 4.16) return 0;
            else return 0;
        }
    }
}

int calculateHealthPrediction(float vib, int load, float temp) {
  int score = 100;
  if (vib > 1.5) score -= (vib - 1.5) * 20;
  if (load > 75) score -= (load - 75) * 2;
  if (temp > 40.0) score -= (temp - 40.0) * 3;
  return constrain(score, 0, 100);
}

void runSystemLoop() {
  sensors.requestTemperatures(); 
  temperature = sensors.getTempCByIndex(0);
  
  sensors_event_t a, g, temp;
  if(mpu.getEvent(&a, &g, &temp)) {
    vibX = abs(a.acceleration.x); 
    vibY = abs(a.acceleration.y); 
    vibZ = abs(a.acceleration.z); 
  }

  // --- NEW ACS712 CURRENT CALCULATION ---
  int raw = analogRead(PIN_VOLT);
  float pinVoltage = (raw / 4095.0) * 3.3; 
  
  // Reverse the voltage divider math (1.468 multiplier)
  float sensorVoltage = pinVoltage * 1.468; 
  
  // Convert sensor voltage to Amps (ACS712-20A uses 100mV/A with 2.5V baseline)
  currentAmps = (sensorVoltage - 2.5) / 0.100;
  
  // Filter noise & make positive
  if (currentAmps < 0.15 && currentAmps > -0.15) currentAmps = 0; 
  currentAmps = abs(currentAmps);

  // Convert Amps to Load % for your ML Model (Edit 800 and 5000 based on your motor)
  int current_mA = currentAmps * 1000;
  loadPercentage = map(current_mA, 10000, 20000, 0, 100);
  loadPercentage = constrain(loadPercentage, 0, 100); 

  if (digitalRead(PIN_MOSFET) == LOW) {
      loadPercentage = 0;
      currentAmps = 0; 
  }
  // --------------------------------------

  if (digitalRead(PIN_MOSFET) == HIGH) motorSpeed = pulseCount * 60; 
  else motorSpeed = 0;
  pulseCount = 0; 

  long elapsedRunTime = millis() - motorStartTime;
  
  if (digitalRead(PIN_MOSFET) == HIGH) {
      if (elapsedRunTime > 1000 && motorSpeed == 0) {
          Serial.println("STOPPED: STARTUP JAM DETECTED");
          digitalWrite(PIN_MOSFET, LOW); 
          Blynk.virtualWrite(V7, 0);      
          isLockoutActive = true;
          lockoutStartTime = millis();
          String msg = "STARTUP JAM! Check Motor: " + dashLink;
          Blynk.logEvent("safety_lockout", msg);
      }
      else if (elapsedRunTime > STARTUP_GRACE_PERIOD) {
          ai_FaultClass = runMLModel(motorSpeed, loadPercentage, vibX);
          
          if (ai_FaultClass == 2) { 
              Serial.println("STOPPED: STALL DETECTED");
              digitalWrite(PIN_MOSFET, LOW); 
              Blynk.virtualWrite(V7, 0);      
              isLockoutActive = true;
              lockoutStartTime = millis();
              
              String msg = "STALL DETECTED! Motor Stopped. Check: " + dashLink;
              Blynk.logEvent("safety_lockout", msg);
          }
      } else {
          ai_FaultClass = 0; 
      }
  } else {
      ai_FaultClass = 0; 
  }

  pred_HealthScore = calculateHealthPrediction(vibX, loadPercentage, temperature);
  
  unsigned long currentMsgTime = millis();
  if (currentMsgTime - lastTempTime > 1000) { 
    if (lastTemp > 0 && (temperature - lastTemp) > 5.0 && temperature > 35) { 
        safety_FireRisk = true;
        digitalWrite(PIN_MOSFET, LOW);
        Blynk.virtualWrite(V7, 0);
        isLockoutActive = true;
        lockoutStartTime = millis();
        Blynk.logEvent("safety_lockout", "FIRE RISK! Rapid Heat.");
    } 
    else if (temperature > 65.0) { 
        safety_FireRisk = true;
        digitalWrite(PIN_MOSFET, LOW); 
        Blynk.virtualWrite(V7, 0);
        isLockoutActive = true;
        lockoutStartTime = millis();
        Blynk.logEvent("safety_lockout", "OVERHEAT! System Halted.");
    } else {
        safety_FireRisk = false;
    }
    lastTemp = temperature;
    lastTempTime = currentMsgTime;
  }

  if (millis() - lastBlynkUpdate > 3000) { 
      Blynk.virtualWrite(V0, vibX);        
      Blynk.virtualWrite(V1, vibY); 
      Blynk.virtualWrite(V2, vibZ); 
      Blynk.virtualWrite(V3, temperature);
      Blynk.virtualWrite(V4, motorSpeed); 
      Blynk.virtualWrite(V5, loadPercentage); 
      Blynk.virtualWrite(V8, pred_HealthScore); 

      String statusMsg = "READY";
      
      if (isLockoutActive) {
          unsigned long unlockTime = lockoutStartTime + LOCKOUT_DURATION;
          if (millis() < unlockTime) {
             long remSeconds = (unlockTime - millis()) / 1000;
             statusMsg = "COOLDOWN: " + String(remSeconds) + "s";
          } else {
             isLockoutActive = false;
             statusMsg = "READY";
          }
      } 
      else if (digitalRead(PIN_MOSFET) == HIGH) {
          if (elapsedRunTime <= STARTUP_GRACE_PERIOD) {
              statusMsg = "START UP";
          }
          else if (ai_FaultClass == 1) statusMsg = "FRICTION";
          else if (ai_FaultClass == 3) statusMsg = "IMBALANCE";
          else statusMsg = "RUNNING";
      }
      Blynk.virtualWrite(V9, statusMsg);
      lastBlynkUpdate = millis(); 
  }

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setTextSize(1);
  display.setCursor(0,0);
  
  if (isLockoutActive) {
      display.setCursor(15, 20);
      display.setTextSize(2); display.print("LOCKED");
      display.setTextSize(1); display.setCursor(30, 45);
      
      unsigned long unlockTime = lockoutStartTime + LOCKOUT_DURATION;
      long remSeconds = 0;
      
      if (millis() < unlockTime) {
          remSeconds = (unlockTime - millis()) / 1000;
      } else {
          remSeconds = 0;
          isLockoutActive = false;
      }
      display.print("Wait: "); display.print(remSeconds); display.print("s");
  } 
  else {
      display.print("RPM:"); display.print(motorSpeed);
      display.setCursor(65,0);
      display.print("H:"); display.print(pred_HealthScore); display.print("%");
      display.setCursor(0,15); display.print("AI Mode: "); 
      if(digitalRead(PIN_MOSFET) == HIGH && (elapsedRunTime <= STARTUP_GRACE_PERIOD)) {
          display.print("START UP");
      } else if(ai_FaultClass == 0) {
          display.print("MONITORING");
      } else {
          display.print("FAULT FOUND");
      }
      display.drawRect(0, 28, 128, 36, WHITE); 
      display.setCursor(5,40); display.setTextSize(2); 
      if (safety_FireRisk) display.print("! FIRE !");
      else if (digitalRead(PIN_MOSFET) == HIGH && (elapsedRunTime <= STARTUP_GRACE_PERIOD)) display.print("WAIT...");
      else if (ai_FaultClass == 0) display.print("HEALTHY");
      else if (ai_FaultClass == 1) display.print("FRICTION");
      else if (ai_FaultClass == 3) display.print("IMBALANCE"); 
      else display.print("READY");
  }
  display.display();

  Serial.print(vibX); Serial.print(","); 
  Serial.print(vibY); Serial.print(","); 
  Serial.print(vibZ); Serial.print(","); 
  Serial.print(motorSpeed); Serial.print(","); 
  Serial.print(loadPercentage); Serial.print(","); 
  Serial.print(temperature); Serial.print(",");
  Serial.println(currentAmps); // NEW: Amps appended to the end!
}

void setup() {
  Serial.begin(115200);
  delay(1000); 
  pinMode(PIN_MOSFET, OUTPUT);
  digitalWrite(PIN_MOSFET, LOW);
  pinMode(PIN_SPEED, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_SPEED), countPulse, RISING);
  
  Wire.begin(21, 22);
  Wire.setClock(100000); 
  
  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    for(;;);
  }
  display.clearDisplay();
  display.setTextColor(WHITE);
  display.display();

  mpu.begin(0x68);
  mpu.setAccelerometerRange(MPU6050_RANGE_8_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);
  
  sensors.begin();
  // FIXED: Removed the setWaitForConversion(false) that was breaking it!
  sensors.requestTemperatures();
  lastTemp = sensors.getTempCByIndex(0);
  
  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  timer.setInterval(1000L, runSystemLoop); 
}

void loop() {
  Blynk.run();
  timer.run();
}

BLYNK_WRITE(V7) { 
  int userRequest = param.asInt(); 
  if (userRequest == 1) { 
      if (isLockoutActive) {
          unsigned long unlockTime = lockoutStartTime + LOCKOUT_DURATION;
          if (millis() < unlockTime) {
             Blynk.virtualWrite(V7, 0); 
             return; 
          } else {
             isLockoutActive = false; 
          }
      }
      motorStartTime = millis(); 
      digitalWrite(PIN_MOSFET, HIGH);
  } else {
      digitalWrite(PIN_MOSFET, LOW);
  }
}
