#include<Arduino.h>

#include<M5StickCPlus.h>

// This script will run on the M5StickCPlus
// When the user presses the main button, the microphone will start recording for 5 seconds
// It will then send the recorded audio to the server for processing

//Includes
#include<WiFi.h>

//First we need to test if we can make a connection to the server

const uint port = 10000;

WiFiServer server(port);
WiFiClient remoteClient;

#include "secrets.h"


void testConnection(){
  int retries = 5;
  while(!remoteClient.connect(serverIP, port) && (retries-- > 0)){
    Serial.print(".");
  }
  if(!remoteClient.connected()){
    Serial.println("Did not connect");
    return;
  }
  
  Serial.println("\nConnected to server!");
  

  remoteClient.print("Hello from the M5StickCPlus!");

  remoteClient.stop();

}


void setup() {
  
  M5.begin();
  M5.Lcd.setRotation(3);

  WiFi.begin(SSID, pass);
  Serial.print("Connecting to " + String(SSID));
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nConnected!");

  server.begin();
 
}

void loop() {
  
  //If the user presses the main button, test the connection
  if(M5.BtnA.wasPressed()){
    testConnection();
  }

}
