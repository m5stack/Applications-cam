#include "esp_camera.h"
#include <WiFi.h>
#include"iic.h"
I2C i2c;
extern int pp[9];
extern uint8_t  sent_pp[9];
extern uint8_t  sent_e_pp[20];
extern char percentage[9][9];
/*
typedef enum {
    PIXFORMAT_RGB565,    // 2BPP/RGB565
    PIXFORMAT_YUV422,    // 2BPP/YUV422
     , // 1BPP/GRAYSCALE
    PIXFORMAT_JPEG,      // JPEG/COMPRESSED
    PIXFORMAT_RGB888,    // 3BPP/RGB888
} pixformat_t;

typedef enum {
    FRAMESIZE_QQVGA,    // 160x120
    FRAMESIZE_QQVGA2,   // 128x160
    FRAMESIZE_QCIF,     // 176x144
    FRAMESIZE_HQVGA,    // 240x176
    FRAMESIZE_QVGA,     // 320x240
    FRAMESIZE_CIF,      // 400x296
    FRAMESIZE_VGA,      // 640x480
    FRAMESIZE_SVGA,     // 800x600
    FRAMESIZE_XGA,      // 1024x768
    FRAMESIZE_SXGA,     // 1280x1024
    FRAMESIZE_UXGA,     // 1600x1200
} framesize_t;
*/
//
// WARNING!!! Make sure that you have either selected ESP32 Wrover Module,
//            or another board which has PSRAM enabled
//

// Select camera model
//#define CAMERA_MODEL_WROVER_KIT
#define CAMERA_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_AI_THINKER


#if defined(CAMERA_MODEL_WROVER_KIT)
#define PWDN_GPIO_NUM    -1
#define RESET_GPIO_NUM   -1
#define XCLK_GPIO_NUM    21
#define SIOD_GPIO_NUM    26
#define SIOC_GPIO_NUM    27

#define Y9_GPIO_NUM      35
#define Y8_GPIO_NUM      34
#define Y7_GPIO_NUM      39
#define Y6_GPIO_NUM      36
#define Y5_GPIO_NUM      19
#define Y4_GPIO_NUM      18
#define Y3_GPIO_NUM       5
#define Y2_GPIO_NUM       4
#define VSYNC_GPIO_NUM   25
#define HREF_GPIO_NUM    23
#define PCLK_GPIO_NUM    22

#elif defined(CAMERA_MODEL_M5STACK_PSRAM)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     22
#define SIOC_GPIO_NUM     23

#define Y9_GPIO_NUM       19
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM        5
#define Y4_GPIO_NUM       34
#define Y3_GPIO_NUM       35
#define Y2_GPIO_NUM       32
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     26
#define PCLK_GPIO_NUM     21

#elif defined(CAMERA_MODEL_AI_THINKER)
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

#else
#error "Camera model not selected"
#endif

void startCameraServer();
bool ColorComp(uint16_t pixel_color,uint16_t template_color,uint16_t threshold);
void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

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
  //config.pixel_format = PIXFORMAT_JPEG;
  config.pixel_format = PIXFORMAT_RGB565;
  //config.pixel_format = PIXFORMAT_YUV422;
  //config.pixel_format = PIXFORMAT_GRAYSCALE;
  //config.pixel_format = PIXFORMAT_RGB888;
  //init with high specs to pre-allocate larger buffers
  config.frame_size = FRAMESIZE_QQVGA;
  config.jpeg_quality = 0;
  config.fb_count = 10;
  //config.wb_mode = 1;//0 - 4

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QQVGA);
  s->set_wb_mode(s, 4);
  //Serial.println("ok");

#if 1
  Serial.println("Configuring access point...");
  // You can remove the password parameter if you want the AP to be open.
  WiFi.mode(WIFI_AP);
  String Mac = WiFi.macAddress();
  String SSID = "Camera:"+ Mac;
  bool result = WiFi.softAP(SSID.c_str(), "12345678", 0, 0);
  if (!result){
    Serial.println("AP Config failed.");
  } else
  {
    Serial.println("AP Config Success. AP NAME: " + String(SSID));
  }
  //WiFi.softAP(ssid, password);
  IPAddress myIP = WiFi.softAPIP();
  Serial.print("AP IP address: ");
  Serial.println(myIP);
  WiFi.begin();

  startCameraServer();
  #else
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");

  //startCameraServer();

  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
  #endif

  i2c.slave_start();

}

esp_err_t get_stream(void){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }


    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.printf("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG)
            {
       int h = 0; 
        int l = 0;
        int quadrant = 0;
        for(int i = 0; i < 9; i++)
        pp[i] = 0;
        //！ 1
        for(int i = 0; i < 20; i++){
          //！%
          sent_e_pp[i] = 0;
        }
        uint16_t color = 0;
        for(int i = 0; i < fb->len/2; i++){
          //Serial.printf("%d = %x\n",i, fb->buf[2*i]<<8|fb->buf[2*i+1]);
          //Serial.printf("%d = %x\n",i, fb->buf[2*i]<<8|fb->buf[2*i+1]);
          color = fb->buf[2*i]<<8|fb->buf[2*i+1];
          #if 0
          if(color > 48000)
          {
            //if(i/fb->width)
             h = i/fb->width;
             l = i%fb->width;
             quadrant = h/40 * 3 + l/53;
             if(quadrant > 8)quadrant = 8;
             //Serial.printf("%d\n",quadrant);
             pp[quadrant]++;
          }
        #else
          if(ColorComp(color,41261,80)){
            h = i/fb->width;
            l = i%fb->width;
           quadrant = h/40 * 3 + l/53;
           if(quadrant > 8)quadrant = 8;
           pp[quadrant]++;

           sent_e_pp[l/8]++;
         }
         #endif
        }
        uint16_t add = 0;
        for(int i = 0; i < 9; i++){
          //Serial.printf("%d = %d\n",i,pp[i]);
          add += pp[i];
        }
 
        //Serial.printf("%d\n",add);
        for(int i = 0; i < 9; i++){
         // Serial.printf("%d\n",add);
          sent_pp[i] = pp[i]*100/(fb->len/18);
          sprintf(percentage[i],"%d%%",pp[i]*100/(fb->len/18));
          //Serial.printf("%s\n",percentage[i]);
        }
                for(int h = 0; h < 320; h++){
                  fb->buf[40 * 320 + h] = 0;
                  fb->buf[80 * 320 + h] = 0;
                }
                for(int w = 0; w < 120; w++){
                  fb->buf[53*2 + 320*w] = 0;
                  fb->buf[53*2*2 + 320*w] = 0;
                }
             
                bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
                esp_camera_fb_return(fb);
                fb = NULL;
                if(!jpeg_converted){
                    Serial.printf("JPEG compression failed");
                    res = ESP_FAIL;
                }
            } else {
                _jpg_buf_len = fb->len;
                _jpg_buf = fb->buf;
            }
        }
       
        if(fb){
            esp_camera_fb_return(fb);
            fb = NULL;
            _jpg_buf = NULL;
        } else if(_jpg_buf){
            free(_jpg_buf);
            _jpg_buf = NULL;
        }
        if(res != ESP_OK){
            break;
        }
        int64_t fr_end = esp_timer_get_time();

        int64_t frame_time = fr_end - last_frame;
        last_frame = fr_end;
        frame_time /= 1000;
       // uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);
       /*
        Serial.printf("MJPG: %uB %ums (%.1ffps), AVG: %ums (%.1ffps)"
            ,(uint32_t)(_jpg_buf_len),
            (uint32_t)frame_time, 1000.0 / (uint32_t)frame_time,
            avg_frame_time, 1000.0 / avg_frame_time
           
        ); 
       */
    }

    last_frame = 0;
    return res;
}
void loop() {
  get_stream();
}
