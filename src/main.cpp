#include<Arduino.h>

#include<M5StickCPlus.h>
#include<driver/i2s.h>

// This script will run on the M5StickCPlus
// When the user presses the main button, the microphone will start recording for 5 seconds
// It will then send the recorded audio to the server for processing

//Includes
#include<WiFi.h>

//First we need to test if we can make a connection to the server

const uint port = 10000;

//WiFiServer server(port);
WiFiClient remoteClient;

#include "secrets.h"

#define BUTTON_PIN 37
#define PIN_CLK  0
#define PIN_DATA 34
#define READ_LEN (2 * 256)
#define GAIN_FACTOR 3
uint8_t BUFFER[READ_LEN] = {0};

int16_t *adcBuffer = NULL;

String hexEncode(int16_t n){
  String hex = "";
  while(n != 0){
    int toHex = n % 16;
    char h;
    if(toHex < 10){
      h = toHex + '0';
    }else{
      h = (toHex-10) + 'a';
    }
    hex = h + hex;
    n /= 16;
  }
  return hex;
}

// This makeshift mutex is used to prevent the main thread from reading 
// the ADC buffer while it is being written to
bool mutex = false;

//Mic Code
void mic_record_task (void* arg)
{   
  size_t bytesread;
  while(1){
    while(mutex);
    mutex = true;
    i2s_read(I2S_NUM_0,(char*) BUFFER, READ_LEN, &bytesread, (100 / portTICK_RATE_MS));
    adcBuffer = (int16_t *)BUFFER; 
    mutex = false;
    // This has the effect of casting the uint8_t buffer to an int16_t buffer
    // Effectively converting two values in the lower order array:
    // [low_order_bits, high_order_bits] to a single value in an 
    // array of half the length, but holding more value: [16bit_value]
    
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

void i2sInit()
{
   i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate =  44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT, //was _ALL_RIGHT
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
   };

   i2s_pin_config_t pin_config;
   pin_config.bck_io_num   = I2S_PIN_NO_CHANGE;
   pin_config.ws_io_num    = PIN_CLK;
   pin_config.data_out_num = I2S_PIN_NO_CHANGE;
   pin_config.data_in_num  = PIN_DATA;
	
   
   i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
   i2s_set_pin(I2S_NUM_0, &pin_config);
   i2s_set_clk(I2S_NUM_0, 44100, I2S_BITS_PER_SAMPLE_16BIT, I2S_CHANNEL_MONO);
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

  int retries = 5;
  while(!remoteClient.connect(serverIP, port) && (retries-- > 0)){
    Serial.print(".");
  }
  if(!remoteClient.connected()){
    Serial.println("Did not connect");
    while(1);
  }
  
  Serial.println("\nConnected to server!");
  

  remoteClient.print("->Hello from the M5StickCPlus!!");
  Serial.println("Sent message to server");

  i2sInit();
  xTaskCreate(mic_record_task, "mic_record_task", 2048, NULL, 1, NULL);
}

long timer = 0;
uint8_t secondsToWait = 25;

uint8_t counter = 0;

void loop() {
  //Read the button
  M5.update();
  
  //If the user presses the main button, start the timer
  if(M5.BtnA.wasPressed()){
    //Set timer
    timer = millis() + secondsToWait * 1000;
  }

  //If the timer has been set, continue streaming data
  if(millis() < timer){
    for(int i = 0; i < READ_LEN / 2; i++){
      //Encode data as hex string and send to server
      while(mutex);
      mutex = true;
      remoteClient.print(hexEncode(adcBuffer[i])+';');
      mutex = false;
    }
  }else{
    //Once the time has expired, reset the timer
    timer = 0;
    counter = 0;
  }

  while(remoteClient.available()){
    Serial.print(remoteClient.readString());
  }

  //If server is not connected, try to reconnect when the button is pressed
  if(!remoteClient.connected()){
    Serial.println("Disconnected from server\nPress main button to reconnect");
    while(!remoteClient.connected()){
      M5.update();
      if(M5.BtnA.wasPressed()){
        Serial.println("Connecting to server");
        remoteClient.connect(serverIP, port);
      }
    }
    Serial.println("Connected to server!");
  }

  //vTaskDelay(1000 / portTICK_RATE_MS); // otherwise the main task wastes half of the cpu cycles
}
