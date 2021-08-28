#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>   
#include <HTTPClient.h>
AsyncWebServer server(80);
DNSServer dns;
HTTPClient http;  
WiFiClient client;

#include "boards.h"
#include <LoRa.h>
String LoRaData;

#include <ArduinoJson.h>
StaticJsonDocument<2500> urlJSON;
String urls = "{\"hum\":[\"http://192.168.8.106/farm/sensor/listener/24ca2501834d02fbb0e01c62ae17ccc8?private_key=da65ec96958277564197b7491dd8a994\",\"http://192.168.8.106/farm/sensor/listener/1ad2540bcee390239ee23ba907ea4d4b?private_key=287d7476639204460cf8f3b1b8540ebf\"],\"temp\":[\"http://192.168.8.106/farm/sensor/listener/2af62b0d5189b65e7fb8bf7024fb574e?private_key=76102647a4127eada9aac3bae1229519\",\"http://192.168.8.106/farm/sensor/listener/24c56526a74058f951bf69b577ed1068?private_key=945b741154c13fe61c53eadc7e333b88\"],\"soil\":[\"http://192.168.8.106/farm/sensor/listener/e1b5b4f51caaf9fb126bcc3ac2e45fa5?private_key=f33c8bd9c030bf423312d5e99224ed75\",\"http://192.168.8.106/farm/sensor/listener/12e22380ff156b2b22f33b362fb3f476?private_key=3171ecb6e64a3ede5e1b4ed2c14379fc\"],\"rain\":[\"http://192.168.8.106/farm/sensor/listener/bdfad07d1d020c124e348720df0f7948?private_key=ee0009b0684b854f12cc640c39339e74\",\"http://192.168.8.106/farm/sensor/listener/8486b3b28734b5b178e169938f6dd25a?private_key=b003938e4ea7cca0ab746ae0534d13d5\"]}";
//Stringified JSON with FarmOS api endpoints for sensors, see "/include/api.json"

#include <ezTime.h>
Timezone romeTZ;
int lastTime = 0;
String every = "15min";     //Sets time span for querying sensor. Possible values are "min", "15min" and "day"


void sendRequestData(){     //Sends string "giveMeData" via LoRa
  Serial.println("Sending request data!");
  LoRa.beginPacket();
  LoRa.print("giveMeData");
  LoRa.endPacket();
}

void sendJson(float value, String timestamp, String url){ //Sends single FarmOS Sensor formatted JSON with value and timestamp to URL
	
  StaticJsonDocument<300> OutputJson;                 //Declaring static JSON buffer	
	OutputJson["value"] = value;                        //Set JSON value
	OutputJson["timestamp"] = timestamp;                //Set JSON timestamp
	char OutputJsonBuffer[300];
	serializeJson(OutputJson, OutputJsonBuffer);        //Serialize JSON to string
	http.begin(client, url);                            //Specify request destination
	http.addHeader("Content-Type", "application/json"); //Specify content-type header
	int httpCode = http.POST(OutputJsonBuffer);         //Send the request
	String payload = http.getString();                  //Get the response payload
	Serial.println(httpCode);                           //Print HTTP return code
	http.end();                                         //Close connection
}

void setup()
{
  initBoard();                                        //Init ICQUANZX SX1276 LoRa ESP32 868MHz
  
  delay(1500);

  Serial.println("LoRa Receiver Data Test");
  LoRa.setPins(RADIO_CS_PIN, RADIO_RST_PIN, RADIO_DI0_PIN); //Set LoRa pins
  if (!LoRa.begin(LoRa_frequency)) {                        //Initialize LoRa module
      Serial.println("Starting LoRa failed!");
      while (1);
  }
  else
    Serial.println("LoRa started");

  AsyncWiFiManager wifiManager(&server,&dns);         //Declare WiFiManager
  wifiManager.autoConnect("AutoConnectAP");           //If WiFi credentials are not saved already, create an access point with name and serve page to select a connection and save credentials
  Serial.println("Wifi started");

  waitForSync();                                      //Sync NTP time
  romeTZ.setLocation(F("Europe/Rome"));               //Set Timezone

  DeserializationError error = deserializeJson(urlJSON, urls);    //Parse JSON from API string
  if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
  else{
    Serial.println("API retrieved");
  }
}

void loop()
{
  if(every == "min"){
	  int time = romeTZ.minute();
    if(time != lastTime){
      lastTime = time;
      sendRequestData();
    }
  }
  else if (every == "15min"){
    int time = romeTZ.minute();
    if(time == 0 || time == 15 || time == 30 || time == 45){
      if(lastTime != 1){
        lastTime = 1;
        sendRequestData();
      }
    }
    else lastTime = 0;
  }
  else if(every == "day"){
    int time = romeTZ.day();
    if(time != lastTime){
      lastTime = time;
      sendRequestData();
    }
  }

  int packetSize = LoRa.parsePacket();            //Listen for LoRa packet arriving
  if (packetSize) {                                
    Serial.print("Received packet ");
    while (LoRa.available()) {                    //While data is available
      LoRaData = LoRa.readString();               //read it
      Serial.print(LoRaData);                     //and print it
    }

    int rssi = LoRa.packetRssi();                 //Print Radio Signal Strength Indicator
    Serial.print(" with RSSI ");    
    Serial.println(rssi);

    StaticJsonDocument<200> inputJson;
    DeserializationError error = deserializeJson(inputJson, LoRaData);  //Parse data from LoRa packet as JSON

    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.f_str());
    }
    else{                                         //If parsing was successful
      int id = inputJson["id"];                   //Retrieve and assign variables from parsed JSON
      int soil = inputJson["soil"];
      float temp = inputJson["temp"];
      int hum = inputJson["hum"];
      int rain = inputJson["rain"];

      if (WiFi.status() == WL_CONNECTED) {        //If WiFi is connected
        char buffer [3];
        unsigned int ms = romeTZ.ms();            
        unsigned int seconds = ms / 1000;
        sprintf(buffer,"%03d", seconds);
        String milliEpoch = String(now() / 1000) + buffer;  //Print timestamp in seconds

        Serial.print("Timestamp: ");
        Serial.println(milliEpoch);

        sendJson(soil, milliEpoch, urlJSON["soil"][id]); //Send sensor values in JSON format to FarmOS api endpoints
        delay(300);
        sendJson(temp, milliEpoch, urlJSON["temp"][id]);
        delay(300);
        sendJson(hum, milliEpoch, urlJSON["hum"][id]);
        delay(300);
        sendJson(rain, milliEpoch, urlJSON["rain"][id]);
      }
    }
  }
}
