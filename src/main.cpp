#define BLYNK_TEMPLATE_ID "TMPL6xxxxxxxxx"
#define BLYNK_TEMPLATE_NAME "WaterTankMonitoringSystem"
#define BLYNK_AUTH_TOKEN "your_blynk_auth_token_here"

#include <Blynk.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <WiFi.h>
#include <WiFiClient.h>

char auth[] = BLYNK_TEMPLATE_ID;
char ssid[] = "YourWiFiNetwork"; 
char pass[] = "YourWiFiPassword"; 

// Pin Definitions
#define FLOW_SENSOR_PIN 32      
#define TRIGGER_PIN 23         
#define ECHO_PIN 22            
#define TDS_PIN 35             
#define TURBIDITY_PIN_A 34          
#define RELAY_PIN 18           
#define BUZZER_PIN 25            
#define LCD_SDA_PIN 14         
#define LCD_SCL_PIN 13 

#define VPIN_PUMP_LED   V10   
#define EVT_DRY_RUN     "dry_run_detected"

// LCD
LiquidCrystal_I2C lcd(0x27, 16, 2);

// Global Variables
volatile int pulseCount = 0;   
float flowRate = 0;            
const float calibrationFactor = 7.5;  

int waterLevel = 0;            
int duration, distance;        
int waterHeight = 13;          

// TDS
int sensorValue = 0;
float voltage = 0;
float tdsValue = 0;            
const float calibrationFactorTDS = 0.5;  

// Turbidity
int turbidityValue = 0;
float turbidity = 0;           

// Pump control vars
bool pumpOn = false;                     
unsigned long pumpStartTime = 0;         
const unsigned long gracePeriod = 10000;  
int noFlowCounter = 0;                   
const int noFlowLimit = 3;  


// Explicit hysteresis thresholds
const int pumpOnLevel  = 4;   // Pump ON below this
const int pumpOffLevel = 8;   // Pump OFF at/above this

// Flow sensor interrupt
void flowSensorInterrupt() {
  pulseCount++;
}

// Setup
void setup() {
  Serial.begin(115200);
  Wire.begin(LCD_SDA_PIN, LCD_SCL_PIN);
  lcd.begin(16, 2);
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Water Monitor");
  delay(2000);
  lcd.clear(); 

  WiFi.begin(ssid, pass);  // Connect to Wi-Fi
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("Trying to connect");
  } 

  pinMode(FLOW_SENSOR_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(FLOW_SENSOR_PIN), flowSensorInterrupt, RISING);

  pinMode(TRIGGER_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);  // Pump off initially

  pinMode(BUZZER_PIN, OUTPUT);

  Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
  Serial.println("Setup complete");
}

// Flow rate
float calculateFlowRate() {
  flowRate = pulseCount / calibrationFactor;
  pulseCount = 0;
  Serial.print("Flow Rate: ");
  Serial.print(flowRate);
  Serial.println(" L/min");
  return flowRate;
}

// Water level
int getWaterLevel() {
  digitalWrite(TRIGGER_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIGGER_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIGGER_PIN, LOW);

  duration = pulseIn(ECHO_PIN, HIGH);
  distance = (duration * 0.0344) / 2;

  if (distance < 0 || distance > waterHeight) {
    waterLevel = -1;  
  } else {
    waterLevel = waterHeight - distance;  
  }

  Serial.print("Water Level: ");
  if (waterLevel == -1) {
    Serial.println("Invalid");
  } else {
    Serial.print(waterLevel);
    Serial.println(" cm");
  }

  return waterLevel;
}

// TDS
float getTDSValue() {
  sensorValue = analogRead(TDS_PIN);
  voltage =  (sensorValue/ 4095.0)*3.3;
  tdsValue = (133.42 * pow(voltage, 3) - 255.86 * pow(voltage, 2) + 857.39 * voltage) * calibrationFactorTDS;
  Serial.print("TDS Value: ");
  Serial.print(tdsValue);
  Serial.println(" ppm");
  return tdsValue;
}

String getTdsQuality(float tdsValue) {
  if(tdsValue <= 50) return "Too pure";
  else if(tdsValue <= 150) return "Excellent";
  else if(tdsValue <= 300) return "Good";
  else if(tdsValue <= 500) return "Acceptable";
  else if(tdsValue <= 1000) return "Poor";
  else return "Unsafe";
}

// Turbidity
float getTurbidityValue() {
  int sensorValue = analogRead(TURBIDITY_PIN_A);
  float voltage = sensorValue * (3.3 / 4095.0);

  // Simple linear approximation (needs calibration!)
  float ntu = (voltage - 2.5) * -1000.0;  //Nephelometric Turbidity Unit
  if (ntu < 0) ntu = 0;

  Serial.print("Turbidity: ");
  Serial.print(ntu);
  Serial.println(" NTU");

  return ntu;
}


// Pump with hysteresis + no-flow safety
void controlPump(int waterLevel, float flowRate) {
  if (!pumpOn && waterLevel < pumpOnLevel) {
    digitalWrite(RELAY_PIN, HIGH);   // Pump ON
    pumpOn = true;
    pumpStartTime = millis();
    noFlowCounter = 0;
    Serial.println("Pump is ON");
  } 
  else if (pumpOn && waterLevel >= pumpOffLevel) {
    digitalWrite(RELAY_PIN, LOW);  // Pump OFF
    pumpOn = false;
    Serial.println("Pump is OFF (water level OK)");
  }


  // Flow check
  if (pumpOn && (millis() - pumpStartTime > gracePeriod)) {
    if (flowRate < 0.1) {
      noFlowCounter++;
      Serial.print("No flow detected (");
      Serial.print(noFlowCounter);
      Serial.println(")");
      if (noFlowCounter >= noFlowLimit) {
        digitalWrite(RELAY_PIN, LOW);
        pumpOn = false;
        Serial.println("Pump stopped: No water flow for too long!");
        Blynk.virtualWrite(VPIN_PUMP_LED, 0);
      }
    } else {
      noFlowCounter = 0;
    }
  }
}

// Buzzer
void checkBuzzer(int waterLevel) {
  if (waterLevel > pumpOffLevel) {  
    tone(BUZZER_PIN, 1500, 500);
    Serial.println("Buzzer ON: Water level high");
    Blynk.logEvent("high_water_level", String("High water level detected!"));
  } else if(waterLevel < pumpOffLevel && waterLevel > pumpOnLevel){
    noTone(BUZZER_PIN);
    Serial.println("Buzzer OFF: Water level Normal");
  }
  else if(waterLevel < pumpOnLevel){
    noTone(BUZZER_PIN);
    Serial.println("Buzzer OFF: Water level Low");
  }

  if(waterLevel < 4){
    Blynk.logEvent("low_water_level", String("Low water level detected!"));
  }
}

// Water quality status
String getFinalWaterStatus(float tdsValue, float turbidity) {
  String finalStatus;
  if ((tdsValue <= 500) && (turbidity <= 200)) {
    finalStatus = "Good for Drink";
  } else {
    finalStatus = "Not Good for Drink";
  }
  Serial.print("Final Water Status: ");
  Serial.println(finalStatus);
  return finalStatus;
}

// LCD pages
int page = 0;
unsigned long lastUpdateTime = 0;
unsigned long updateInterval = 3000;

void displayOnLCD() {
  switch (page) {
    case 0:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Level: ");
      if (waterLevel == -1) {
        lcd.print("Invalid");
      } else {
        lcd.print(waterLevel);
        lcd.print(" cm");
      }
      lcd.setCursor(0, 1);
      lcd.print("Flow: ");
      lcd.print(flowRate);
      lcd.print(" L/m");
      break;
    case 1:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("TDS: ");
      lcd.print(tdsValue, 0);
      lcd.print(" ppm");
      lcd.setCursor(0, 1);
      lcd.print("Turb: ");
      lcd.print(turbidity);
      lcd.print(" NTU");
      break;
    case 2:
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("Water Status:");
      lcd.setCursor(0, 1);
      lcd.print(getFinalWaterStatus(tdsValue, turbidity));
      break;
  }
  page = (page + 1) % 3;
}

// Main loop
void loop() {
  waterLevel = getWaterLevel();
  flowRate = calculateFlowRate();
  tdsValue = getTDSValue();
  turbidity = getTurbidityValue();
  String finalStatus = getFinalWaterStatus(tdsValue, turbidity);

  controlPump(waterLevel, flowRate);
  checkBuzzer(waterLevel);

  if (millis() - lastUpdateTime >= updateInterval) {
    displayOnLCD();
    lastUpdateTime = millis();
  }

  Blynk.virtualWrite(V0, waterLevel);  // Water level in cm
  Blynk.virtualWrite(V1, flowRate);    // Flow rate in L/min
  Blynk.virtualWrite(V2, tdsValue);    // TDS value in ppm
  Blynk.virtualWrite(V3, turbidity);   // Turbidity value in NTU
  Blynk.virtualWrite(V4, finalStatus.c_str());  // Water status
  Blynk.run();
  delay(1000);
}
