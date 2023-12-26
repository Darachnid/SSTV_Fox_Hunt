#include <Arduino.h>
#include <Wire.h>
#include "RTClib.h"
#include <WiFi.h>
#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "esp_camera.h"
#include "FS.h"
#include "SD_MMC.h"
#include "driver/rtc_io.h"
int save_flag = 0; 
const char* ssid = "arunmobile";
const char* password = "arun1234";
#define ESP32CAM_LED_FLASH 4
String saved_image_name = "/first.jpg";
String serverName = "192.168.43.16";   // REPLACE WITH YOUR Raspberry Pi IP ADDRESS
//String serverName = "example.com";   // OR REPLACE WITH YOUR DOMAIN NAME
String serverPath = "/upload";     // The default serverPath should be upload.php
const int serverPort = 8080;
#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  30        /* Time ESP32 will go to sleep (in seconds) */
int connection_error_count = 0;
RTC_DATA_ATTR int cnt = 0;
WiFiClient client;
// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22
const int remain_connected_timerInterval = 5000;    // time between each HTTP POST image
const int wifi_timerInterval = 5000;    // time between each HTTP POST image
String status_send = "";
unsigned long previousMillis = 0;   // last time image was sent
char dateTimeFilenamearray[25];
RTC_DS3231 rtc;
uint8_t print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;
  wakeup_reason = esp_sleep_get_wakeup_cause();
  int status = 0;
  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : 
        Serial.println("Wakeup caused by external signal using RTC_IO"); 
        status = 1;
        break;
    case ESP_SLEEP_WAKEUP_EXT1 : 
        Serial.println("Wakeup caused by external signal using RTC_CNTL"); 
        status = 2;
        break;
    case ESP_SLEEP_WAKEUP_TIMER : 
        Serial.println("Wakeup caused by timer");
        status = 3;
        break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : 
        Serial.println("Wakeup caused by touchpad");
        status = 4;
        break;
    case ESP_SLEEP_WAKEUP_ULP : 
        Serial.println("Wakeup caused by ULP program");
        status = 5;
        break;
    default : 
        Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
  return status;
}
uint8_t capture_image(String path_new)
{
    
    // String path_new = "/pircheck"+String(cnt)+".jpg";
    // cnt++;
    String keypressed = "button";   
  
    Serial.println("Starting SD Card");
      if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        return 0;
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        return 0;
    }
    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);   
    
    // digitalWrite(ESP32CAM_LED_FLASH, HIGH);
    camera_fb_t *fb = NULL;
    fb = esp_camera_fb_get();
    if (!fb)
    {
      Serial.println("Camera capture failed");
      return 0;
    }
    else
    {
      Serial.println("Camera Captured");
    }
    delay(1000);
    fs::FS &fs = SD_MMC;
    Serial.printf("Picture file name: %s\n", path_new.c_str());
    
    File file = fs.open(path_new.c_str(), FILE_WRITE);
    if(!file){
      Serial.println("Failed to open file in writing mode");
    } 
    else {
      file.write(fb->buf, fb->len); // payload (image), payload length
      Serial.printf("Saved file to path: %s\n", path_new.c_str());
    }
    file.close();
    delay(1000);
    SD_MMC.end();
    delay(1000);
    esp_camera_fb_return(fb);
    delay(1000);
    // digitalWrite(ESP32CAM_LED_FLASH, LOW);
    
}
String sendPhoto(const char * path) {
  String getAll;
  String getBody;
  static uint8_t buf[1024];
  if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        
    }
    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);
  fs::FS &fs = SD_MMC; 
  File file = fs.open(path);
  delay(500);
  if(file)
  {
    Serial.println("file detected.........");  
  }
  Serial.println("Connecting to server: " + serverName);
  if (client.connect(serverName.c_str(), serverPort)) {
    Serial.println("Connection successful!");    
    String head = "--RandomNerdTutorials\r\nContent-Disposition: form-data; name=\"imageFile\"; filename=\"esp32-cam.jpg\"\r\nContent-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--RandomNerdTutorials--\r\n";
    uint16_t imageLen = file.size();;
    uint16_t extraLen = head.length() + tail.length();
    uint16_t totalLen = imageLen + extraLen;
  
    client.println("POST " + serverPath + " HTTP/1.1");
    client.println("Host: " + serverName);
    client.println("Content-Length: " + String(totalLen));
    client.println("Content-Type: multipart/form-data; boundary=RandomNerdTutorials");
    client.println();
    client.print(head);  
    
    size_t fbLen = file.size();
    for (size_t n=0; n<fbLen; n=n+1024) {
      if (n+1024 < fbLen) {
        file.read(buf, 1024);
        delay(10);
        client.write(buf, 1024);        
      }
      else if (fbLen%1024>0) {
        size_t remainder = fbLen%1024;
        file.read(buf, remainder);
        delay(10);
        client.write(buf, remainder);
      }
    }   
    client.print(tail);    
    
    int timoutTimer = 10000;
    long startTimer = millis();
    boolean state = false;
    
    while ((startTimer + timoutTimer) > millis()) {
      Serial.print(".");
      delay(100);      
      while (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (getAll.length()==0) { state=true; }
          getAll = "";
        }
        else if (c != '\r') { getAll += String(c); }
        if (state==true) { getBody += String(c); }
        startTimer = millis();
      }
      if (getBody.length()>0) { break; }
    }
    file.close();    
    Serial.println();
    client.stop();
    Serial.println(getBody);
    delay(1000);
    SD_MMC.end();
  }
  else {
    getBody = "";
    Serial.println(getBody);
  }
  return getBody;
}
void renameFile(const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\n", path1, path2);
    if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        
    }
    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    fs::FS &fs = SD_MMC; 
    if (fs.rename(path1, path2)) {
        Serial.println("File renamed");
    } else {
        Serial.println("Rename failed");
    }
    SD_MMC.end();
}
void deleteFile(const char * path){
    Serial.printf("Deleting file: %s\n", path);
    if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        
    }
    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    fs::FS &fs = SD_MMC;
    if(fs.remove(path)){
        Serial.println("File deleted");
    } else {
        Serial.println("Delete failed");
    }
    SD_MMC.end();
}
String listDir(const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\n", dirname);
    if(!SD_MMC.begin()){
        Serial.println("Card Mount Failed");
        
    }
    uint8_t cardType = SD_MMC.cardType();
    if(cardType == CARD_NONE){
        Serial.println("No SD_MMC card attached");
        
    }
    Serial.print("SD_MMC Card Type: ");
    if(cardType == CARD_MMC){
        Serial.println("MMC");
    } else if(cardType == CARD_SD){
        Serial.println("SDSC");
    } else if(cardType == CARD_SDHC){
        Serial.println("SDHC");
    } else {
        Serial.println("UNKNOWN");
    }
    uint64_t cardSize = SD_MMC.cardSize() / (1024 * 1024);
    Serial.printf("SD_MMC Card Size: %lluMB\n", cardSize);   
    delay(1000);
    fs::FS &fs = SD_MMC; 
    File root = fs.open(dirname);
    delay(1000);
    if(!root){
        Serial.println("Failed to open directory");
        
    }
    if(!root.isDirectory()){
        Serial.println("Not a directory");
        
    }
    String filename = "";
    File file = root.openNextFile();
    if(file){        
      Serial.print("  FILE: ");
      Serial.print(file.name());
      Serial.print("  SIZE: ");
      Serial.println(file.size());
      filename = String(file.name());
    }   
    file.close();
    SD_MMC.end();
    return filename;
  }

void go_to_deepsleep()
{
    esp_sleep_enable_ext0_wakeup(GPIO_NUM_13,0); //1 = High, 0 = Low
    esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
    Serial.println("Going to sleep now");
    esp_deep_sleep_start();
    Serial.println("This will never be printed");
}
void setup_wifi()
{
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  unsigned long previousMillis = millis(); 
  int flag = 0;
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print("."); 
    Serial.print("connecting");  
    unsigned long currentMillis = millis(); 
    if (currentMillis - previousMillis >= wifi_timerInterval) 
    {
        flag = 1;
        break;
    }  
  }
  
  if (flag == 0)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("WiFi not connected");
  }
  
}
void setup() {
  //delay(5000);
  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); 
  Serial.begin(115200); 
  if (! rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1);
  }
  String path_new = "";
  //Print the wakeup reason for ESP32
  uint8_t status = print_wakeup_reason();
  Serial.print("Serial status is "); 
  Serial.print(status); 
  if(status==0)
  {   
    // delay(20000);
    Serial.println("<<<<<<<<<<<<<<<<<<< POWER >>>>>>>>>>>>>>>>>>>"); 
    // go_to_deepsleep();
  }
  if(status==1)
  {   
    Serial.println("<<<<<<<<<<<<<<<<<<< MOTION >>>>>>>>>>>>>>>>>>>"); 
    
  }
  if(status==3)
  {   
    Serial.println("<<<<<<<<<<<<<<<<<<< TIMER >>>>>>>>>>>>>>>>>>>"); 
    
  }  
  camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = Y2_GPIO_NUM;
    config.pin_d1 = Y3_GPIO_NUM;
    config.pin_d2 = Y4_GPIO_NUM;
    config.pin_d3 = Y5_GPIO_NUM;
    config.pin_d4 = Y6_GPIO_NUM;
    config.pin_d5 = Y7_GPIO_NUM;
    config.pin_d6 = Y8_GPIO_NUM;
    config.pin_d7 = Y9_GPIO_NUM;
    config.pin_xclk = XCLK_GPIO_NUM;
    config.pin_pclk = PCLK_GPIO_NUM;
    config.pin_vsync = VSYNC_GPIO_NUM;
    config.pin_href = HREF_GPIO_NUM;
    config.pin_sscb_sda = SIOD_GPIO_NUM;
    config.pin_sscb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn = PWDN_GPIO_NUM;
    config.pin_reset = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;  //0-63 lower number means higher quality
    config.fb_count = 1;  
    pinMode(4, INPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_dis(GPIO_NUM_4);
    
    // camera init
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x", err);      
      Serial.println("<<<<<<<<<<<<<<<<< Restartig >>>>>>>>>>>>>>>");
      delay(1000);
      ESP.restart();
    }
  if(status==1 || status == 0)
  {
    
    delay(1000);    
    if(status == 0)
    {
      path_new = saved_image_name;
    }
    else
    {
      DateTime now = rtc.now();
      dateTimeFilenamearray[0] = '\0';
      //sprintf(dateTimeFilenamearray, "/%02d%02d%d_%d:%d:%d", now.day(), now.month(), now.year(),now.hour(),now.minute(),now.second());
      sprintf(dateTimeFilenamearray, "/%02d%02d%d_%d_%d_%d", now.day(), now.month(), now.year(),now.hour(),now.minute(),now.second());  
      Serial.println(dateTimeFilenamearray);
      //path_new = "/motion_"+String(cnt)+".jpg";
      path_new = String(dateTimeFilenamearray)+".jpg";
      cnt++;      
    }
    
    capture_image(path_new);
    delay(1000);
    // esp_camera_deinit();
    // delay(10000);
    pinMode(4, OUTPUT);
    digitalWrite(4, LOW);
    rtc_gpio_hold_en(GPIO_NUM_4);
    delay(10000);
    go_to_deepsleep();
  }  
  setup_wifi(); 
  
  previousMillis = millis();
  connection_error_count = 0;
}
void loop() {
  
  if (WiFi.status() != WL_CONNECTED)
  {
    delay(500);
    Serial.print(".");   
    unsigned long currentMillis = millis(); 
    if (currentMillis - previousMillis >= remain_connected_timerInterval) {
      go_to_deepsleep();
    }
  } 
  else
  {   
    if(connection_error_count < 3)
    {
      String filename = "";
      filename = listDir("/",0);
      if(filename != "")
      {    
        Serial.print("filename is: ");
        Serial.println(filename);    
        delay(1000);
        if(filename != saved_image_name){
          status_send = "";
          status_send = sendPhoto(filename.c_str());   
          if(status_send.indexOf("ok") > 0){
            Serial.print("Response from the server");
            Serial.println(status_send);
            deleteFile(filename.c_str());   
            connection_error_count = 0;     
          } 
          else
          {
            delay(3000);
            connection_error_count++;
          }                
        } 
        else
        {
          deleteFile(filename.c_str());
        }              
        previousMillis = millis();
        delay(2000);
      }
      else
      {
        delay(5000);
        go_to_deepsleep();
      } 
    }
    else
    {
        delay(1000);
        go_to_deepsleep();
    }
       
  }    
}