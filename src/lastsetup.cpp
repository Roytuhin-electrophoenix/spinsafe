#define BLYNK_TEMPLATE_ID "TMPL3whH9-q-r"      
#define BLYNK_TEMPLATE_NAME "Motor Health Monitor"
#define BLYNK_AUTH_TOKEN "ovXAFRLuQyWMhrXLYhLoWVmD8qB9zBEB"

#include <WiFi.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h> // <--- REQUIRED FOR GOOGLE SHEETS HTTPS
#include <ArduinoJson.h>      
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
String local_server_url = "http://10.46.34.93:8000"; 

String GOOGLE_SCRIPT_ID = "AKfycbzwtTpvZAw38zLc5qyJaM5bhU5GVgroAt4iQQblcYXhHqVzOAtgbvXm85ONbzwGG2DVhw"; 

#define PIN_MOSFET 2      
#define PIN_TEMP   4     
#define PIN_VOLT   35    
#define PIN_SPEED  34    

Adafruit_MPU6050 mpu;
Adafruit_SSD1306 display(128, 64, &Wire, -1);
OneWire oneWire(PIN_TEMP);
DallasTemperature sensors(&oneWire);
BlynkTimer timer;

float limit_vib = 5.0, limit_temp = 50.0;
float vibX = 0, vibY = 0, vibZ = 0, temperature = 0;
float currentAmps = 0, zeroOffset = 2.5; 
int motorSpeed = 0, loadPercentage = 0, health = 0;
String ai_label = "IDLE"; 
unsigned long lastLog = 0;
volatile int pulseCount = 0;
unsigned long lastRPMMillis = 0;

void IRAM_ATTR countPulse() { pulseCount++; }

void fetchLatestModel() {
    if (WiFi.status() == WL_CONNECTED) {
        WiFiClient client;
        HTTPClient http;
        http.begin(client, local_server_url + "/model.json");
        if (http.GET() == 200) {
            DynamicJsonDocument doc(512);
            deserializeJson(doc, http.getString());
            limit_vib = doc["max_vib"];
            limit_temp = doc["max_temp"];
            ai_label = doc["ai_state"].as<String>();
            Serial.println("AI Update Received: " + ai_label);
        }
        http.end();
    }
}


void logToGoogleSheets() {
    if (WiFi.status() == WL_CONNECTED && digitalRead(PIN_MOSFET) == HIGH) {
        Serial.println("📊 Uploading log to Google Sheets...");
        
        WiFiClientSecure secureClient;
        secureClient.setInsecure(); // Bypass SSL verification for simplicity
        HTTPClient http;

        String url = "https://script.google.com/macros/s/" + GOOGLE_SCRIPT_ID + "/exec?vib=" + String(vibX) + "&temp=" + String(temperature) + "&rpm=" + String(motorSpeed) + "&load=" + String(loadPercentage) + "&health=" + String(health) + "&state=" + ai_label;
        
        http.begin(secureClient, url);
        http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
        
        int httpCode = http.GET();
        if (httpCode > 0) {
            Serial.print("✅ Google Sheets Success! Code: ");
            Serial.println(httpCode);
        } else {
            Serial.println("❌ Google Sheets Error");
        }
        http.end();
    }
}

void runSystemLoop() {
  sensors.requestTemperatures(); 
  temperature = sensors.getTempCByIndex(0);
  
  sensors_event_t a, g, t_event;
  if(mpu.getEvent(&a, &g, &t_event)) {
      vibX = abs(a.acceleration.x);
      vibY = abs(a.acceleration.y);
      vibZ = abs(a.acceleration.z);
  }

  String statusMsg = "READY";

  if (digitalRead(PIN_MOSFET) == LOW) {
      motorSpeed = 0; loadPercentage = 0; health = 0; statusMsg = "READY"; pulseCount = 0;
  } 
  else {
      statusMsg = ai_label; 
      unsigned long currentMillis = millis();
      long duration = currentMillis - lastRPMMillis;
      if (duration > 500) {
          motorSpeed = (pulseCount * 60000) / duration;
          pulseCount = 0; lastRPMMillis = currentMillis;
      }

      int raw = analogRead(PIN_VOLT);
      float sensorVoltage = ((raw / 4095.0) * 3.3) * 1.468; 
      currentAmps = (sensorVoltage - zeroOffset) / 0.100; 
      
      if (abs(currentAmps) < 0.35) loadPercentage = 0; 
      else {
          loadPercentage = map(abs(currentAmps) * 1000, 0, 15000, 0, 100);
          loadPercentage = constrain(loadPercentage, 0, 100);
      }

      health = 100;
      if (vibX > limit_vib) health -= 30;
      if (temperature > limit_temp) health -= 30;
      if (loadPercentage > 85) health -= 20;
      health = constrain(health, 0, 100);
  }

  Blynk.virtualWrite(V0, vibX);
  Blynk.virtualWrite(V1, vibY);
  Blynk.virtualWrite(V2, vibZ);
  Blynk.virtualWrite(V3, temperature);
  Blynk.virtualWrite(V4, motorSpeed);
  Blynk.virtualWrite(V5, loadPercentage);
  Blynk.virtualWrite(V8, health);
  Blynk.virtualWrite(V9, statusMsg);

  display.clearDisplay();
  display.setCursor(0,0);
  display.print("H: "); display.print(health); display.print("%  L: "); display.print(loadPercentage); display.println("%");
  display.print("AI: "); display.println(statusMsg);
  display.print("VibX: "); display.println(vibX);
  display.display();

  // Python Gateway Edge Logging (Every 2 seconds)
  if (digitalRead(PIN_MOSFET) == HIGH && (millis() - lastLog > 2000)) {
    WiFiClient client; HTTPClient http;
    String url = local_server_url + "/log?vib="+String(vibX)+"&temp="+String(temperature)+"&rpm="+String(motorSpeed)+"&load="+String(loadPercentage);
    http.begin(client, url); http.GET(); http.end();
    lastLog = millis();
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_MOSFET, OUTPUT);
  digitalWrite(PIN_MOSFET, LOW);
  pinMode(PIN_SPEED, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_SPEED), countPulse, RISING);
  Wire.begin(21, 22);
  mpu.begin(0x68);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  sensors.begin();

  // Calibration
  float sum = 0;
  for(int i=0; i<50; i++) {
      int raw = analogRead(PIN_VOLT);
      sum += ((raw / 4095.0) * 3.3) * 1.468;
      delay(10);
  }
  zeroOffset = sum / 50.0;

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  
  // --- SYSTEM TIMERS ---
  timer.setInterval(1000L, runSystemLoop);
  timer.setInterval(10000L, fetchLatestModel);      
  timer.setInterval(15000L, logToGoogleSheets);     
}

void loop() { Blynk.run(); timer.run(); }

BLYNK_WRITE(V7) { digitalWrite(PIN_MOSFET, param.asInt()); }
