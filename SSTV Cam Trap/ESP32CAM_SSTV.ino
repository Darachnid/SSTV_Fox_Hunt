#include <driver/ledc.h>              //-Used for Audio PWM
//#include "framebuffer.h"              //-Used for OSD (not essential)
#include "camera.h"                   //-Camera driver
#include "sin256.h"                   //-DDS waveforms

#define BELL202_BAUD 1200             //-To make sample rate compatible with APRS 
#define F_SAMPLE ((BELL202_BAUD * 32) * 0.92)  //-38400
#define FTOFTW (4294967295 / F_SAMPLE)//-Frequency to FTW conversion factor

//DDS audio generator:
volatile uint32_t FTW = FTOFTW * 1000;
volatile uint32_t PCW = 0;
volatile uint32_t TFLAG = 0;

//Pre defined tones
#define FT_1000 (uint32_t) (1000 * FTOFTW)
#define FT_1100 (uint32_t) (1100 * FTOFTW)
#define FT_1200 (uint32_t) (1200 * FTOFTW)
#define FT_1300 (uint32_t) (1300 * FTOFTW)
#define FT_1500 (uint32_t) (1500 * FTOFTW)
#define FT_1900 (uint32_t) (1900 * FTOFTW)
#define FT_2200 (uint32_t) (2200 * FTOFTW)
#define FT_2300 (uint32_t) (2300 * FTOFTW)
#define FT_SYNC (uint32_t) (FT_1200)

#define TIME_PER_SAMPLE (1000.0/F_SAMPLE)

#define MAX_WIDTH 320
#define MAX_HEIGHT 256

class SSTV_config_t
{
  public:
    uint8_t vis_code;
    uint32_t width;
    uint32_t height;
    float line_time;
    float h_sync_time;
    float v_sync_time;
    float c_sync_time;
    float left_margin_time;
    float visible_pixels_time;
    float pixel_time;
    bool color;
    bool martin;
    bool robot;

    SSTV_config_t(uint8_t v)
    {
      vis_code = v;
      switch (vis_code)
      {
        case 2:       //Robot B&W8
          robot = true;
          martin = false;
          color = false;
          width = 160;
          height = 120;
          line_time = 67.025;
          h_sync_time = 30.0;
          v_sync_time = 6.5;
          left_margin_time = 1.6;
          visible_pixels_time = line_time - v_sync_time - left_margin_time;
          pixel_time = visible_pixels_time / width;
          break;

        case 44:
          robot = false;
          martin = true;
          color = true;
          width = 320;
          height = 256;
          line_time = 446.4460001;
          h_sync_time = 30.0;
          v_sync_time = 4.862;
          c_sync_time = 0.572;
          left_margin_time = 0.0;
          visible_pixels_time = line_time - v_sync_time - left_margin_time - (3 * c_sync_time);
          pixel_time = visible_pixels_time / (width * 3);
          break;
      }
    }
};

camera_fb_t* fb;
SSTV_config_t* currentSSTV;
uint8_t* bitmap;

volatile uint16_t rasterX = 0;
volatile uint16_t rasterY = 0;
volatile uint8_t SSTVseq = 0;
double SSTVtime = 0;
double SSTVnext = 0;
uint8_t VISsr = 0;
uint8_t VISparity;
uint8_t HEADERptr = 0;
static uint32_t SSTV_HEADER[] = {FT_2300, 100, FT_1500, 100, FT_2300, 100, FT_1500, 100,  FT_1900, 300, FT_1200, 10, FT_1900, 300, FT_1200, 30, 0, 0};
uint8_t SSTV_RUNNING = 0;

TaskHandle_t sampleHandlerHandle;

void IRAM_ATTR audioISR() {
  PCW += FTW;
  TFLAG = 1;
}

void sampleHandler(void *p)
{
  double w = 0;
  disableCore0WDT();

  while (1)
  {
    if (TFLAG)
    {
      TFLAG = 0;
      int v = SinTableH[((uint8_t*)&PCW)[3]];
      ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3, v);
      ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_3);
      SSTVtime += TIME_PER_SAMPLE;
      if (!SSTV_RUNNING || SSTVtime < SSTVnext)
        goto sstvEnd;  //Don't break the thread loop
      switch (SSTVseq)
      {
        case 0: //Start
          SSTVtime = 0;
          HEADERptr = 0;
          VISparity = 0;
          VISsr = currentSSTV->vis_code;
          FTW = SSTV_HEADER[HEADERptr++];
          SSTVnext = (float)SSTV_HEADER[HEADERptr++];
          SSTVseq++;
          break;
        case 1: //VIS header
          if (SSTV_HEADER[HEADERptr + 1] == 0)  //-End of header?
          {
            SSTVseq++;
            HEADERptr = 0;
          }
          else
          {
            FTW = SSTV_HEADER[HEADERptr++];
            SSTVnext += (float)SSTV_HEADER[HEADERptr++];
          }
          break;
        case 2: //VIS code
          {
            if (HEADERptr == 7)
            {
              HEADERptr = 0;
              if (VISparity)
                FTW = FT_1100;
              else
                FTW = FT_1300;
              SSTVnext += 30.0;   //-VIS bits are 30ms.
              SSTVseq++;
            }
            else
            {
              if (VISsr & 0x01)
              {
                VISparity ^= 0x01;
                FTW = FT_1100;
              }
              else
              {
                FTW = FT_1300;
              }
              VISsr >>= 1;
              SSTVnext += 30.0;   //-VIS bits are 30ms.
              HEADERptr++;
            }
          }
          break;
        case 3: //VIS stop bit/sync0
          FTW = FT_1200;
          SSTVnext += 30.0 + currentSSTV->h_sync_time;
          rasterX = 0;
          rasterY = 0;
          SSTVseq = 10;   //Go to martin section
          break;

        //Robot b/w section
        case 4: //Send line
          if (rasterX == currentSSTV->width)
          {
            rasterY++;
            if (rasterY == currentSSTV->height) //All done
            {
              SSTV_RUNNING = false;
              SSTVseq = 0;
              FTW = 0;
              PCW = 0;
            }
            else
            {
              rasterX = 0;
              FTW = FT_SYNC;
              SSTVnext += currentSSTV->v_sync_time;
            }
          }
          else
          {
            int f = map(bitmap[rasterX + rasterY * currentSSTV->width], 0, 255, 1500, 2300);
            FTW = FTOFTW * f;
            if (rasterX)
              SSTVnext += currentSSTV->pixel_time;
            else
              SSTVnext += (currentSSTV->pixel_time + currentSSTV->left_margin_time);
            rasterX++;
          }
          break;
        //-Martin modes
        case 10:  //Start of line Green
          if (rasterX == currentSSTV->width)
          {
            rasterX = 0;
            FTW = FT_1500;
            SSTVnext += currentSSTV->c_sync_time;
            SSTVseq++;
          }
          else
          {
            int G = bitmap[1 + (rasterX * 3) + (rasterY * currentSSTV->width * 3)];
            int f = map(G, 0, 255, 1500, 2300);
            FTW = FTOFTW * f;
            SSTVnext += currentSSTV->pixel_time;
            rasterX++;
          }
          break;
        case 11:
          if (rasterX == currentSSTV->width)
          {
            rasterX = 0;
            FTW = FT_1500;
            SSTVnext += currentSSTV->c_sync_time;
            SSTVseq++;
          }
          else
          {
            int B = bitmap[(rasterX * 3) + (rasterY * currentSSTV->width * 3)];
            int f = map(B, 0, 255, 1500, 2300);
            FTW = FTOFTW * f;
            SSTVnext += currentSSTV->pixel_time;
            rasterX++;
          }
          break;
        case 12:
          if (rasterX == currentSSTV->width)
          {
            rasterX = 0;
            rasterY++;
            if (rasterY == currentSSTV->height) //All done
            {
              SSTV_RUNNING = false;
              SSTVseq = 0;
              FTW = 0;
              PCW = 0;
            }
            else
            {
              FTW = FT_SYNC ;
              SSTVnext += currentSSTV->v_sync_time;
              SSTVseq = 10;
            }
          }
          else
          {
            int R = bitmap[2 + (rasterX * 3) + (rasterY * currentSSTV->width * 3)];
            int f = map(R, 0, 255, 1500, 2300);
            FTW = FTOFTW * f;
            SSTVnext += currentSSTV->pixel_time;
            rasterX++;
          }
          break;
      }
sstvEnd:;
    }
  }
}


void doImage()
{
  fb = esp_camera_fb_get();
  if (!fb)
  {
    Serial.println("FB_ERROR");
    return;
  }
  bitmap = fb->buf;

  if (currentSSTV)
    delete currentSSTV;
  currentSSTV = new SSTV_config_t(44); //
 
  Serial.println(TIME_PER_SAMPLE, 10);
  Serial.print("Sending image");

  SSTVtime = 0;
  SSTVnext = 0;
  SSTVseq = 0;
  SSTV_RUNNING = true;
  vTaskResume( sampleHandlerHandle );
  while (SSTV_RUNNING)
  {
    Serial.print(".");
    delay(1000);
  }
  vTaskSuspend( sampleHandlerHandle );
  Serial.println("Ok.123");
  esp_camera_fb_return(fb);
  delay(10000);
}


void setup() {

  Serial.begin(115200);
  delay(500);
  Serial.println("Start..");
  delay(500);

  bitmap = (uint8_t*)malloc(320 * 256 * 3 + 100);

  setupCamera();

  for (int i = 0; i < 20; i++)
  {
    fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    delay(100);
  }


  hw_timer_t* timer = NULL;
  timer = timerBegin(2, 10, true);
  timerAttachInterrupt(timer, &audioISR, true);

  timerAlarmWrite(timer, 8000000 / F_SAMPLE, true);
  timerAlarmEnable(timer);
  //  ADC clock generation
  ledc_timer_config_t ledc_timer;

  ledc_timer.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_timer.duty_resolution = LEDC_TIMER_8_BIT;
  ledc_timer.timer_num = LEDC_TIMER_1;
  ledc_timer.freq_hz = 200000;

  ledc_channel_config_t ledc_channel;
  ledc_channel.channel    = LEDC_CHANNEL_3;
  ledc_channel.gpio_num   = 4;
  ledc_channel.speed_mode = LEDC_HIGH_SPEED_MODE;
  ledc_channel.timer_sel  = LEDC_TIMER_1;
  ledc_channel.duty       = 2;
  ledc_channel.hpoint = 0;
  ledc_timer_config(&ledc_timer);
  ledc_channel_config(&ledc_channel);

  xTaskCreatePinnedToCore(sampleHandler, "IN", 4096, NULL, 1, &sampleHandlerHandle, 0);
  vTaskSuspend( sampleHandlerHandle );

}

void loop() {
  doImage();
  delay(10000);
  Serial.println(millis());
}
