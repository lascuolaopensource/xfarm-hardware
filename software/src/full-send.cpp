#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> 
#include <HTTPClient.h>
#include <ezTime.h>
#include <ArduinoJson.h>
StaticJsonDocument<200> doc;


#include <SPI.h>
#include <LoRa.h>
String LoRaData;
int boardID = 0;

//define the pins used by the LoRa transceiver module
#define SCK 5
#define MISO 19
#define MOSI 27
#define SS 18
#define RST 14
#define DIO0 26
#define BAND 868E6


//Define Sensor pins
#define SOIL_HUM 36
#define RAIN_SENSOR 32
#define DHTPIN 17

#include "DHT.h"
#define DHTTYPE DHT22 
DHT dht(DHTPIN, DHTTYPE);

void readTemp(){
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  
  if (isnan(h) || isnan(t)) {   // Check if data is present
    Serial.println(F("Failed to read from DHT sensor!"));
    return;
  }

  float hic = dht.computeHeatIndex(t, h, false);  // Compute heat index in Celsius (isFahreheit = false)

  Serial.print(F("Humidity: "));        //Print data
  Serial.print(h);
  Serial.print(F("%  Temperature: "));
  Serial.print(t);
  Serial.print(F("°C "));
  Serial.print(F(" Heat index: "));
  Serial.print(hic);
  Serial.println(F("°C "));

  doc["temp"] = t;                      //Add temperature to JSON
  doc["hum"] = h;                       //Add humidity to JSON
}

void readRain(){
  int rainreading = analogRead(RAIN_SENSOR);      //Read rain sensor analog pin
  int rainpercentage = map(rainreading, 0, 4095, 100, 0); //Rescale range (ESP32 adc)
  Serial.print("Piove con intensita del ");       //Print data
  Serial.print(rainpercentage);
  Serial.println("%");

  doc["rain"] = rainpercentage;         //Add rain data to JSON
}

void readSoilHum(){
  unsigned int soilHum = 0;  
  for (int i = 0; i <= 100; i++)      
  {
    soilHum = soilHum + analogRead(SOIL_HUM); //Read and sum values from DHT22 100 times
    delay(1);
  }
  soilHum = soilHum / 100.0;                  //Divide by 100 to get the median value
  int soilHumPercentage = map(soilHum, 4095, 600, 0, 100);    //Map in percentage
  Serial.print("Umidità terreno: ");         //Print data
  Serial.print(analogRead(SOIL_HUM));
  Serial.print(" ");
  Serial.print(soilHumPercentage);
  Serial.println("%");

  doc["soil"] = soilHumPercentage;         //Add soil humidity data to JSON
}

void sendData(){                          //Send JSON as LoRa packet
  LoRa.beginPacket();                     //Start a new packet
  doc["id"] = boardID;                    //Assign boardID

  readSoilHum();                          //Read sensors
  readRain();
  readTemp();

  char JSONmessageBuffer[300];
  serializeJson(doc, JSONmessageBuffer);  //Serialize JSON into a string
  Serial.println(JSONmessageBuffer);
  LoRa.print(JSONmessageBuffer);          //Add data to LoRa packet

  LoRa.endPacket();                       //Send packet
  Serial.println("Sending packet");
}

void setup() {
  Serial.begin(9600); 
  Serial.println("LoRa Send Data Test");
  
  //SPI LoRa pins
  SPI.begin(SCK, MISO, MOSI, SS);
  //setup LoRa transceiver module
  LoRa.setPins(SS, RST, DIO0);

  if (!LoRa.begin(BAND)) {
    Serial.println("Starting LoRa failed!");
    while (1);
  }
  Serial.println("LoRa Initializing OK!");
  dht.begin();
}

void loop() {
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    Serial.print("Received packet ");
	
    while (LoRa.available()) {
      LoRaData = LoRa.readString();
      Serial.print(LoRaData);
    }

    int rssi = LoRa.packetRssi();
    Serial.print(" with RSSI ");    
    Serial.println(rssi);

    if(LoRaData == "giveMeData"){
      sendData();
    }
  }
}
