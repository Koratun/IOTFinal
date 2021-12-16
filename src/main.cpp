#include<Arduino.h>

#include<M5StickCPlus.h>
#include<driver/i2s.h>

// This script will run on the M5StickCPlus
// When the user presses the main button, the microphone will start recording for 5 seconds
// It will then send the recorded audio to the server for processing

//Includes
#include<WiFi.h>

#include<list>

const uint port = 10000;

WiFiClient remoteClient;

RTC_TimeTypeDef RTC_TimeStruct;
RTC_DateTypeDef RTC_DateStruct;

#include "secrets.h"

#define BUTTON_PIN 37
#define PIN_CLK  0
#define PIN_DATA 34
#define READ_LEN (2 * 1024)
#define GAIN_FACTOR 3
uint8_t BUFFER[READ_LEN] = {0};

int16_t *adcBuffer = NULL;

//A list containing the time and date of all snoring start and stop times
std::list<String> snoringTimes;

//Gets the real time clock and converts it to a string
const char* getDateTime(){
  M5.Rtc.GetTime(&RTC_TimeStruct);
  M5.Rtc.GetData(&RTC_DateStruct);
  char* dateTime = new char[20];
  sprintf(dateTime, "%04d-%02d-%02d %02d:%02d:%02d", RTC_DateStruct.Year, RTC_DateStruct.Month, RTC_DateStruct.Date, RTC_TimeStruct.Hours, RTC_TimeStruct.Minutes, RTC_TimeStruct.Seconds);
  return dateTime;
}

// This makeshift mutex is used to prevent the main thread from reading 
// the ADC buffer while it is being written to
bool mutex = false;

//Mic Code
void mic_record_task (void* arg)
{   
  size_t bytesread;
  while(1){
    while(mutex){
      //Delay is needed because the M5 is not using true multithreading
      //Without this delay the program gets stuck in this loop.
      delayMicroseconds(1);
    }
    mutex = true;
    i2s_read(I2S_NUM_0,(char*) BUFFER, READ_LEN, &bytesread, (100 / portTICK_RATE_MS));
    // This has the effect of casting the uint8_t buffer to an int16_t buffer
    // Effectively converting two values in the lower order array:
    // [low_order_bits, high_order_bits] to a single value in an 
    // array of half the length, but holding more value: [16bit_value]
    adcBuffer = (int16_t *)BUFFER;
    mutex = false;
    
    vTaskDelay(100 / portTICK_RATE_MS);
  }
}

void i2sInit()
{
  i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate =  44100,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT, // is fixed at 12bit, stereo, MSB
    .channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT, //from _RIGHT_LEFT
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 6,
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
  Serial.begin(115200);
  delay(1000);

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
  }else{
    Serial.println("\nConnected to server!");

    remoteClient.print("Time");
    Serial.println("Sent message to server");
  }

  //Wait until the server sends a message
  while(!remoteClient.available());

  //Read the time response from the server
  String datetime = remoteClient.readStringUntil('\n');
  //Parse response and set the date and time
  //Response is in the format: "YYYY-MM-DD HH:MM:SS"
  Serial.println("Response from server: " + datetime);
  int year = datetime.substring(0,4).toInt();
  int month = datetime.substring(5,7).toInt();
  int day = datetime.substring(8,10).toInt();
  int hour = datetime.substring(11,13).toInt();
  int minute = datetime.substring(14,16).toInt();
  int second = datetime.substring(17,19).toInt();

  //Set RTC data
  RTC_TimeTypeDef TimeStruct;
  //Set the time.
  TimeStruct.Hours = hour;
  TimeStruct.Minutes = minute;
  TimeStruct.Seconds = second;
  //writes the set time to the real time clock.
  M5.Rtc.SetTime(&TimeStruct);  
  RTC_DateTypeDef DateStruct;
  //Set the date.
  DateStruct.WeekDay = 3; //We won't use the day of the week, so this number doesn't matter.
  DateStruct.Month = month;
  DateStruct.Date = day;
  DateStruct.Year = year;
  M5.Rtc.SetData(&DateStruct);

  //Close connection to server
  remoteClient.stop();

  //Put WiFi to sleep
  WiFi.setSleep(true);

  //Prep display
  M5.Lcd.fillScreen(BLACK);
  M5.Lcd.setCursor(5,5);
  M5.Lcd.setTextColor(WHITE);
  M5.Lcd.setTextSize(2);
  M5.Lcd.println("Snoring Detection");
  //Set cursor always to x=5
  M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
  M5.Lcd.println("Inactive");
  M5.Lcd.setTextSize(1);
  M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
  M5.Lcd.println("Press the button to start");

  i2sInit();
  xTaskCreate(mic_record_task, "mic_record_task", 2048, NULL, 1, NULL);
}

long timer = 0;
uint8_t secondsToWait = 15;
bool snoring = false;
bool lastSnoring = false;
String startTime = "";

bool detectMode = false;

void loop() {
  //Read the button
  M5.update();
  
  //If the button is pressed, toggle the mode
  if(M5.BtnA.wasPressed()){
    detectMode = !detectMode;
    if(detectMode){
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(5,5);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println("Snoring Detection");
      //Set cursor always to x=5
      M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
      M5.Lcd.println("Active");
      M5.Lcd.setTextSize(1);
      M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
      M5.Lcd.println("Press the button again to stop");
      long waitTime = millis() + 15000;
      while(millis() < waitTime){
        delay(100);
      }
    }else{
      //If snoring was still being detected, record current time as the stop time
      if(snoring){
        timer = 0;
        snoring = false;
        //Save the pair of start and end times to the list of snore times
        snoringTimes.push_back(startTime + "," + getDateTime());
      }
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(5,5);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println("Snoring Detection");
      //Set cursor always to x=5
      M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
      M5.Lcd.println("Deactivated");
      M5.Lcd.setTextSize(1);
      M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
      M5.Lcd.println("Press the button to start");
      M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
      if(snoringTimes.size() > 0)
        M5.Lcd.println("Sending data to server...");
      else
        M5.Lcd.println("No data to send");
    }
  }

  if(snoring != lastSnoring){
    lastSnoring = snoring;
    if(snoring){
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(5,5);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println("Snoring Detected");
      M5.Lcd.setTextSize(1);
      M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
      M5.Lcd.println("Press the button to stop");
    }else{
      M5.Lcd.fillScreen(BLACK);
      M5.Lcd.setCursor(5,5);
      M5.Lcd.setTextColor(WHITE);
      M5.Lcd.setTextSize(2);
      M5.Lcd.println("Snoring Stopped");
      M5.Lcd.setTextSize(1);
      M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
      M5.Lcd.println("Press the button to stop");
    }
  }

  if(detectMode){
    while(mutex){
      //Delay is needed because the M5 is not using true multithreading
      //Without this delay the program gets stuck in this loop.
      delayMicroseconds(1);
    }
    mutex = true;
    for(int i = 0; i < READ_LEN / 2; i++){
      //Serial.println(adcBuffer[i]);
      if(adcBuffer[i] > 1250 && adcBuffer[i] < 1400){
        //Serial.println("Snore detected.");
        timer = millis() + secondsToWait * 1000;
        if(!snoring){
          snoring = true;
          //Save the current RTC to the start time
          startTime = getDateTime();
        }
      }
    }
    mutex = false;

    if(timer > 0 && millis() > timer){
      timer = 0;
      snoring = false;
      //Save the pair of start and end times to the list of snore times
      snoringTimes.push_back(startTime + "," + getDateTime());
    }
  }

  //If we are not in detect mode and the snoring list is not empty, send the data to the server
  if(!detectMode && snoringTimes.size() > 0){
    //Wake up the wifi
    WiFi.setSleep(false);
    //Connect to the server
    remoteClient.connect(serverIP, port);
    //Wait until the connection is established
    while(!remoteClient.connected()){
      Serial.print(".");
    }
    Serial.println("\nConnected to server!");

    //Send the data
    for(String& time : snoringTimes){
      remoteClient.print(time+';');
    }
    Serial.println("Sent data to server");
    //Clear the list
    snoringTimes.clear();
    //Close the connection
    remoteClient.stop();
    //Put WiFi to sleep
    WiFi.setSleep(true);
  
    //Fix display
    M5.Lcd.fillScreen(BLACK);
    M5.Lcd.setCursor(5,5);
    M5.Lcd.setTextColor(WHITE);
    M5.Lcd.setTextSize(2);
    M5.Lcd.println("Snoring Detection");
    //Set cursor always to x=5
    M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
    M5.Lcd.println("Inactive");
    M5.Lcd.setTextSize(1);
    M5.Lcd.setCursor(5, M5.Lcd.getCursorY());
    M5.Lcd.println("Press the button to start");
  }

}
