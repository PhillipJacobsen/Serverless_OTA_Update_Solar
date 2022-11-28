# Serverless_OTA_Update_Solar.ino

This ESP32 microcontroller based project demonstrates a unique Over-the-air(OTA) firmware upgrade scheme using the Solar.org Blockchain and IPFS.
Firmware events are sent to the serial port as ASCII text for display on terminal program running on PC. 

Here are more detailed steps of the process.
1. generate firmware image for microcontroller
2. generate JSON metadata for firmware image
2. Upload image to IPFS using my cli tool. This will generate CID hash.
https://i.imgur.com/DlBwKU8.png
3. Send IPFS transaction with the CID hash and the JSON metadata
https://i.imgur.com/wS20hQY.png
4. Microcontroller will periodically check the blockchain for a new IPFS transaction that has newer version number.
https://i.imgur.com/afC3lce.png
5. Microcontroller will download firmware image from IPFS
https://i.imgur.com/vL3XC4C.png

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
