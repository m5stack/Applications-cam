#include "esp_camera.h"
#include <WiFi.h>
/*
typedef enum {
    PIXFORMAT_RGB565,    // 2BPP/RGB565
    PIXFORMAT_YUV422,    // 2BPP/YUV422
    PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
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
#define CAMERA_A_MODEL_M5STACK_PSRAM
//#define CAMERA_MODEL_AI_THINKER

const char* ssid = "M5";
const char* password = "12345678";


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

/*
  For M5Camera A Model https://docs.m5stack.com/#/en/unit/m5camera
*/
#elif defined(CAMERA_A_MODEL_M5STACK_PSRAM)
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    15
#define XCLK_GPIO_NUM     27
#define SIOD_GPIO_NUM     25
#define SIOC_GPIO_NUM     23

#define Y9_GPIO_NUM       19
#define Y8_GPIO_NUM       36
#define Y7_GPIO_NUM       18
#define Y6_GPIO_NUM       39
#define Y5_GPIO_NUM        5
#define Y4_GPIO_NUM       34
#define Y3_GPIO_NUM       35
#define Y2_GPIO_NUM       32
#define VSYNC_GPIO_NUM    22
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

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  //drop down frame size for higher initial frame rate
  sensor_t * s = esp_camera_sensor_get();
  s->set_framesize(s, FRAMESIZE_QQVGA);
  //Serial.println("ok");

#if 1
  Serial.println("Configuring access point...");
  // You can remove the password parameter if you want the AP to be open.
  WiFi.mode(WIFI_AP);
  String Mac = WiFi.macAddress();
  String SSID = "LidarBot:"+ Mac;
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



}
uint8_t ImageData[9][80][107];
void loop() {
 /*
 // put your main code here, to run repeatedly:
  camera_fb_t * fb = NULL;
  char tx_buffer[200] = { '\0' };
  tx_buffer[0] = 0xFF;
  tx_buffer[1] = 0xD8;
  tx_buffer[2] = 0xEA;
  tx_buffer[3] = 0x01;
  while(true){
       // Serial.println("ok");
        camera_fb_t * fb = esp_camera_fb_get();
        if(!fb) {
          Serial.printf("Camera capture failed");
        }else{
          Serial.printf("fb->len = %d\n", fb->len);
          Serial.printf("fb->width = %d\n", fb->width);
          Serial.printf("fb->height = %d\n", fb->height);
          Serial.printf("fb->format = %d\n", fb->format);
          tx_buffer[4] = (uint8_t)((fb->len & 0xFF0000) >> 16) ;
          tx_buffer[5] = (uint8_t)((fb->len & 0x00FF00) >> 8 ) ;
          tx_buffer[6] = (uint8_t)((fb->len & 0x0000FF) >> 0 );
          //320x240
          //uart_write_bytes(UART_NUM_1, (char *)tx_buffer, 7);
          //uart_write_bytes(UART_NUM_1, (char *)fb->buf, fb->len);
          //strcat((char *)tx_buffer,(char *)fb->buf);
          //Serial.write((uint8_t *)tx_buffer,fb->len + 7);
          #if 0
          for(int z = 0; z < 9; z++)
            for(int i = 0; i < 80; i++)
              for(int j = 0;j  < 107;j++)
                ImageData[z][i][j] = fb->buf
         #endif
        }
        //Serial.printf("fb->len = %d\n", fb->len);

        tx_buffer[4] = (uint8_t)((fb->len & 0xFF0000) >> 16) ;
        tx_buffer[5] = (uint8_t)((fb->len & 0x00FF00) >> 8 ) ;
        e[6] = (uint8_t)((fb->len & 0x0000FF) >> 0 );

        uart_write_bytes(UART_NUM_1, (char *)tx_buffer, 7);
        uart_write_bytes(UART_NUM_1, (char *)fb->buf, fb->len);
        data_len =(uint32_t)(tx_buffer[4] << 16) | (tx_buffer[5] << 8) | tx_buffer[6];
        printf("should %d, print a image, len: %d\r\n",fb->len, data_len);

        esp_camera_fb_return(fb);
    }
*/
}
