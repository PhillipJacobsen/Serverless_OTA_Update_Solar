/********************************************************************************
    Serverless_OTA_Update_Solar.ino

    Demo application using authenticated MQTT access to receive Solar.org blockchain events.
    This project is designed to run on any ESP32 microcontroller
    Blockchain event packets are sent to the serial port as ASCII text for display on terminal program running on PC
    LED is toggled at every blockchain event    

    Required Libraries:
      EspMQTTClient by Patrick Lapointe Version 1.13.3
      ArduinoJson by Benoit Blanchon Version 6.19.4
      PubSubClient by Nick O'Leary Version 2.8
      esp32FOTA Version 2.2 (Versions 2.3 does not compile. Maybe only good for platformio?)
        The MQTT JSON packets are larger then the default setting in the libary.
        You need to increase max packet size from 128 to 4096 via this line in \Arduino\libraries\PubSubClient\src\PubSubClient.h
            #define MQTT_MAX_PACKET_SIZE 4096  //default 128
      
    MQTT Server Access
      I operate a special MQTT broker that receives realtime Solar.org blockchain events from both Testnet and Mainnet networks.
      Special thanks to @deadlock for creating a plugin to enable this. https://github.com/deadlock-delegate/mqtt
      MQTT connections are able to subscribe to these blockchain events.
      MQTT server access is provided by a unique authentication method.  
        Authentication credentials are generated by signing a message using your blockchain private keys and by voting for my delegate(pj) with your wallet.
        Currently the authentication method uses the Solar Testnet. Future support for Solar Mainnet will be added

********************************************************************************/

#include "secrets.h"  // credentials for WiFi, MQTT, OTA upgrading, timezones, etc

#include "time.h"  // required for internal clock to syncronize with NTP server

#include <ArduinoJson.h>  // JSON parsing

#include <esp32fota.h>  // OTA upgrade

#include <HTTPClient.h>  // http client

#include "EspMQTTClient.h"  // WiFi and MQTT connection handler for ESP32. EspMQTTClient is a wrapper around the MQTT PubSubClient Library by @knolleary

EspMQTTClient WiFiMQTTclient(
  WIFI_SSID,
  WIFI_PASS,
  MQTT_SERVER_IP,
  MQTT_USERNAME,
  MQTT_PASSWORD,
  MQTT_CLIENT_NAME,
  MQTT_SERVER_PORT);


const int LED_PIN = 25;                          //LED integrated on Heltec WiFi Kit
bool initialConnectionEstablished_Flag = false;  //used to detect first run after power up

StaticJsonDocument<4000> doc;      //allocate memory on stack for the Solar API response
StaticJsonDocument<300> IPFSmemo;  //allocate memory on stack for the memo field of the IPFS transaction

//Frequency at which to check if there is new firmware available
uint32_t UpdateInterval_Firmware = 5000;  // 5 seconds
uint32_t previousUpdateTime_Firmware = millis();

// esp32fota esp32fota("<Type of Firmware for this device>", <this version>, <validate signature>, <allow insecure https>);
esp32FOTA esp32FOTA("solar-ota-demo", FW_Version, false, true);


/********************************************************************************
  This function will query Solar blockchain for availability of new firmware
********************************************************************************/
void checkForNewFirmware() {

  if (WiFiMQTTclient.isWifiConnected()) {
    if (millis() - previousUpdateTime_Firmware > UpdateInterval_Firmware) {
      previousUpdateTime_Firmware += UpdateInterval_Firmware;

      digitalWrite(LED_PIN, !digitalRead(LED_PIN));  //toggle LED
      Serial.println("\nChecking Solar blockchain for new firmware.");
      HTTPClient http;
      String serverPath = serverName + "transactions?page=1&limit=1&type=5&typeGroup=1&orderBy=nonce:desc&senderId=" + OTA_WALLET_ADDRESS;
      http.begin(serverPath.c_str());

      // Send HTTP GET request
      int httpResponseCode = http.GET();

      if (httpResponseCode > 0) {
        //Serial.print("HTTP Response code: ");
        //Serial.println(httpResponseCode);
        String payload = http.getString();
        Serial.println(payload);
        //Serial.println();

        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          http.end();  // Free resources
          return;
        }

#ifndef _blockchainBypass
        // check for no transactions
        //const char* null_data = doc["data"][0];
        //if (!null_data) {
        JsonObject data_0 = doc["data"][0];
        if (data_0.isNull()) {

          Serial.println("Data is null. There are no IPFS transactions\n");
          //Serial.println(null_data);
          http.end();  // Free resources
          return;
        }

        //JsonObject data_0 = doc["data"][0];
        const char* data_0_asset_ipfs = data_0["asset"]["ipfs"];
        const char* data_0_memo = data_0["memo"];  // "meta data"
        Serial.print("IPFS CID: ");
        Serial.println(data_0_asset_ipfs);
        Serial.print("Memo MetaData: ");
        Serial.println(data_0_memo);

        error = deserializeJson(IPFSmemo, data_0_memo);
        if (error) {
          Serial.print(F("deserializeJson() of IPFSmemo failed: "));
          Serial.println(error.f_str());
          http.end();  // Free resources
          return;
        }

#else

        const char* data_0_asset_ipfs = "QmSgsBY9dxrmYF4BSE4k8iLL8rQXVLNcGgRF9dNsgdWppY";
        //const char* data_0_memo = "{"PID":"solar-ota-demo","Ver":"0.0.1","File":"solar-ota-demo-0.0.1.bin"}";  // "meta data"
        const char* data_0_memo = "{\"PID\":\"solar-ota-demo\",\"Ver\":\"0.0.2\",\"File\":\"solar-ota-demo-0.0.2.bin\"}";  // "meta data"
        error = deserializeJson(IPFSmemo, data_0_memo);
        if (error) {
          Serial.print(F("deserializeJson() failed: "));
          Serial.println(error.f_str());
          http.end();  // Free resources
          return;
        }

#endif

        // {
        //     "PID": "solar-ota-demo",
        //     "Ver": "0.0.1",
        //     "File": "solar-ota-demo-0.0.1.bin"
        // }

        const char* PID = IPFSmemo["PID"];    // "IotDeviceA"
        const char* Ver = IPFSmemo["Ver"];    // "0.0.1"
        const char* File = IPFSmemo["File"];  // "IotDeviceA-2.bin"


        char compare[24];
        strcpy(compare, Ver);
        int semVerResult = semverTest(FW_Version, compare);
        switch (semVerResult) {
          case -1:  // version string had an eror
            break;
          case 0:  // latest firmware is the same or older version
            break;
          case 1:        // new firmware is available
            http.end();  // Free resources

            // create file path. Example: /ipfs/QmU5S7W1xCxXgMHtZLc93cvED2aH9U5VPvuAaP9XzdP7ab/solar-ota-demo-0.0.1.bin
            char FilePath[120];
            strcpy(FilePath, "https:\/\/ipfs.filebase.io");
            strcat(FilePath, "\/ipfs\/");
            //Serial.println(FilePath);
            strcat(FilePath, data_0_asset_ipfs);
            strcat(FilePath, "\/");
            strcat(FilePath, File);
            Serial.println(FilePath);

            Serial.println("Performing OTA update");
            //esp32FOTA.forceUpdate("ipfs.filebase.io", 443, FilePath, false);
            esp32FOTA.forceUpdate(FilePath, false);
            break;
        }

      } else {
        Serial.print("Error code: ");
        Serial.println(httpResponseCode);
      }

      http.end();  // Free resources
    }
  }
}




/********************************************************************************
  This function is called once WiFi and MQTT connections are complete
  This must be implemented for EspMQTTClient to function
********************************************************************************/
void onConnectionEstablished() {

  if (!initialConnectionEstablished_Flag) {  //execute this the first time we have established a WiFi and MQTT connection after powerup
    initialConnectionEstablished_Flag = true;

    // sync local time to NTP server
    configTime(TIME_ZONE * 3600, DST, "pool.ntp.org", "time.nist.gov");

    // subscribe to Solar Mainnet Events
    //WiFiMQTTclient.subscribe("solar_mainnet/events/transaction_applied", Transaction_Applied_Handler);

    //wait for time to sync from NTP server
    while (time(nullptr) <= 100000) {
      delay(20);
    }

  }

  else {
    // subscribe to Solar Mainnet Events
    //WiFiMQTTclient.subscribe("solar_mainnet/events/transaction_applied", Transaction_Applied_Handler);
  }
}

/********************************************************************************
  Mealy Finite State Machine
  The state machine logic is executed once each cycle of the "main" loop.
  For this simple application it just helps manage the connection states of WiFi and MQTT
********************************************************************************/
enum State_enum { STATE_0,
                  STATE_1,
                  STATE_2 };  //The possible states of the state machine
State_enum state = STATE_0;   //initialize the starting state.

void StateMachine() {
  switch (state) {

    //--------------------------------------------
    // State 0
    // Initial state after ESP32 powers up and initializes the various peripherals
    // Transitions to State 1 once WiFi is connected
    case STATE_0:
      {
        if (WiFiMQTTclient.isWifiConnected()) {  //wait for WiFi to connect
          state = STATE_1;

          Serial.print("\nState: ");
          Serial.print(state);
          Serial.print("  WiFi Connected to IP address: ");
          Serial.println(WiFi.localIP());
          previousUpdateTime_Firmware = millis();  // reset timer
        } else {
          state = STATE_0;
        }
        break;
      }

    //--------------------------------------------
    // State 1
    // Transitions to state 2 once connected to MQTT broker
    // Return to state 0 if WiFi disconnects
    case STATE_1:
      {
        if (!WiFiMQTTclient.isWifiConnected()) {  //check for WiFi disconnect
          state = STATE_0;
          Serial.print("\nState: ");
          Serial.print(state);
          Serial.println("  WiFi Disconnected");
        } else if (WiFiMQTTclient.isMqttConnected()) {  //wait for MQTT connect
          state = STATE_2;
          Serial.print("\nState: ");
          Serial.print(state);
          Serial.print("  MQTT Connected at local time: ");

          time_t now = time(nullptr);  //get current time
          struct tm* timeinfo;
          //    time(&now);
          timeinfo = localtime(&now);
          char formattedTime[30];
          strftime(formattedTime, 30, "%r", timeinfo);
          Serial.println(formattedTime);

        } else {
          state = STATE_1;
        }
        break;
      }

    //--------------------------------------------
    // State 2
    // Return to state 0 if WiFi disconnects
    // Returns to state 1 if MQTT disconnects
    case STATE_2:
      {
        if (!WiFiMQTTclient.isWifiConnected()) {  //check for WiFi disconnect
          state = STATE_0;
          Serial.print("\nState: ");
          Serial.print(state);
          Serial.println("  WiFi Disconnected");
        } else if (!WiFiMQTTclient.isMqttConnected()) {  //check for MQTT disconnect
          state = STATE_1;
          Serial.print("\nState: ");
          Serial.print(state);
          Serial.println("  MQTT Disconnected");
        } else {
          state = STATE_2;
        }
        break;
      }
  }
}


/********************************************************************************
  This function compares two semantic versions.
    returns:
      -1: invalid version string
      0: compare version is less than current
      1: compare version is greater than current.
********************************************************************************/

int semverTest(char* current, char* compare) {
  semver_t current_version = {};
  semver_t compare_version = {};

  if (semver_parse(current, &current_version)
      || semver_parse(compare, &compare_version)) {
    printf("Invalid semver string\n");
    return -1;
  }

  int resolution = semver_compare(compare_version, current_version);

  if (resolution == 0) {
    printf("Remote Version %s is equal to local Version %s\n", compare, current);
    // Free allocated memory when we're done
    semver_free(&current_version);
    semver_free(&compare_version);
    return 0;
  } else if (resolution == -1) {
    printf("Remote Version %s is lower than local Version  %s\n", compare, current);
    // Free allocated memory when we're done
    semver_free(&current_version);
    semver_free(&compare_version);
    return 0;
  } else {
    printf("Remote Version %s is higher than local Version %s\n", compare, current);
    // Free allocated memory when we're done
    semver_free(&current_version);
    semver_free(&compare_version);
    return 1;
  }
}


/********************************************************************************
  Configure peripherals and system
********************************************************************************/
void setup() {

  Serial.begin(115200);  // Initialize Serial Connection for debug
  while (!Serial && millis() < 20)
    ;  // wait 20ms for serial port adapter to power up

  Serial.println("\n\nStarting Serverless OTA Update Demo via Solar Blockchain & IPFS");
  Serial.print("Firmware Version: ");
  Serial.println(FW_Version);

  digitalWrite(LED_PIN, LOW);  // Turn LED off
  pinMode(LED_PIN, OUTPUT);    // initialize on board LED control pin

  // Optional Features of EspMQTTClient
  // WiFiMQTTclient.enableDebuggingMessages();  // Enable MQTT debugging messages
}


/********************************************************************************
  MAIN LOOP
********************************************************************************/
void loop() {

  //--------------------------------------------
  // Process state machine
  StateMachine();

  //--------------------------------------------
  // Handle the WiFi and MQTT connections
  WiFiMQTTclient.loop();

  //--------------------------------------------
  // Check Solar blockchain for new firmware
  checkForNewFirmware();
}