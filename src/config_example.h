// Configuration Template
// Copy this file to config.h and add your actual credentials

#ifndef CONFIG_H
#define CONFIG_H

// Blynk Configuration
#define BLYNK_TEMPLATE_ID "TMPL6xxxxxxxxx"
#define BLYNK_TEMPLATE_NAME "WaterTankMonitoringSystem"
#define BLYNK_AUTH_TOKEN "your_blynk_auth_token_here"

// WiFi Configuration
#define WIFI_SSID "YourWiFiNetwork"
#define WIFI_PASSWORD "YourWiFiPassword"

// Hardware Configuration (these values are from your working system)
#define FLOW_SENSOR_PIN 32      
#define TRIGGER_PIN 23         
#define ECHO_PIN 22            
#define TDS_PIN 35             
#define TURBIDITY_PIN_A 34          
#define RELAY_PIN 18           
#define BUZZER_PIN 25            
#define LCD_SDA_PIN 14         
#define LCD_SCL_PIN 13 

#endif
