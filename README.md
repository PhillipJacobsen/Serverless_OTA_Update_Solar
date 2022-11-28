# Serverless_OTA_Update_Solar.ino

This ESP32 microcontroller based project demonstrates a unique Over-the-air(OTA) firmware upgrade scheme using the Solar.org Blockchain and IPFS.
Firmware events are sent to the serial port as ASCII text for display on terminal program running on PC. 

## How to Configure Project
```
#define MQTT_CLIENT_NAME  "random or unique value"  
#define MQTT_USERNAME     "Your Solar Testnet wallet public key"     // Insert your wallet's public key (not the address)	
#define MQTT_PASSWORD     "8 character message + Message Signature"  // Sign any 8 character message to generate signature
  
//WiFi Credentials
#define WIFI_SSID         "Your WiFi SSID"
#define WIFI_PASS         "Your WiFi Password"

#define OTA_WALLET_ADDRESS "Testnet Wallet address of the OTA wallet address"  
String serverName = "http://159.65.89.1:6003/api/";  // testnet relay
char FW_Version[] = "0.0.1";  // define current firmware version

//IPFS transaction memo field 
// {"PID":"solar-ota-demo","Ver":"0.0.1","File":"solar-ota-demo-0.0.1.bin"}   


//Time Zone Configuration
int8_t TIME_ZONE = -7;    //set timezone:  Offset from UTC
int16_t DST = 3600;       //To enable Daylight saving time set it to 3600. Otherwise, set it to 0. 
```
