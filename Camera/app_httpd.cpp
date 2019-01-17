// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#include "esp_http_server.h"
#include "esp_timer.h"
#include "esp_camera.h"
#include "img_converters.h"
#include "camera_index.h"
#include "Arduino.h"
#include "ASC16.h"
int pp[9];
uint8_t  sent_pp[9];
char percentage[9][9];
typedef struct {
        size_t size; //number of values used for filtering
        size_t index; //current value index
        size_t count; //value count
        int sum;
        int * values; //array to be filled with values
} ra_filter_t;

typedef struct {
        httpd_req_t *req;
        size_t len;
} jpg_chunking_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";
//static const char* _STREAM_PART = "Content-Type: image/bmp\r\nContent-Length: %u\r\n\r\n";
static ra_filter_t ra_filter;
httpd_handle_t stream_httpd = NULL;
httpd_handle_t camera_httpd = NULL;

bool ColorComp(uint16_t pixel_color,uint16_t template_color,uint16_t threshold){
   int ar,ag,ab;
   int br,bg,bb;  
   
   ar = ((pixel_color>>11)&0xff)<<3;
   ag = ((pixel_color>>5)&0x3f)<<2;
   ab = (pixel_color&0x1f)<<2;
   
   br = ((template_color>>11)&0xff)<<3;
   bg = ((template_color>>5)&0x3f)<<2;
   bb = (template_color&0x1f)<<2;

   int absR=ar-br;
   int absG=ag-bg;
   int absB=ab-bb;

   uint16_t temp = sqrt(absR*absR+absG*absG+absB*absB);
   //Serial.printf("temp = %d\n",temp);
   if(temp < threshold)
     return true;
   return false;
}
void writeHzkAsc(camera_fb_t * fb,uint16_t cursor_x,uint16_t cursor_y,const char *c) {

    uint8_t * pAscCharMatrix = (uint8_t *)&ASC16[0];
    
    uint32_t offset;
    uint8_t mask;
    uint16_t posX = cursor_x, posY = cursor_y;
    uint8_t charMatrix[16];

    uint8_t *pCharMatrix;

    int n = 0;
    while(*c != '\0'){
      offset = (uint32_t)(*c) * 16;
      pCharMatrix = pAscCharMatrix + offset;
      for (uint8_t row = 0; row < 16; row++) {
        mask = 0x80;
        posX = cursor_x + n*16;
        for (uint8_t col = 0; col < 8; col++) {
          if ((*pCharMatrix & mask) != 0) {
            fb->buf[posY * fb->width * 2 + posX] = 0;
            fb->buf[posY * fb->width * 2 + posX + 1] = 0;
          }
          posX += 2;
          mask >>= 1;
        }
        posY += 1;
        pCharMatrix++;
      }
      posX += 16;
      posY = cursor_y;
      c += 1;
      n++;
    }

}
static ra_filter_t * ra_filter_init(ra_filter_t * filter, size_t sample_size){
    memset(filter, 0, sizeof(ra_filter_t));

    filter->values = (int *)malloc(sample_size * sizeof(int));
    if(!filter->values){
        return NULL;
    }
    memset(filter->values, 0, sample_size * sizeof(int));

    filter->size = sample_size;
    return filter;
}

static int ra_filter_run(ra_filter_t * filter, int value){
    if(!filter->values){
        return value;
    }
    filter->sum -= filter->values[filter->index];
    filter->values[filter->index] = value;
    filter->sum += filter->values[filter->index];
    filter->index++;
    filter->index = filter->index % filter->size;
    if (filter->count < filter->size) {
        filter->count++;
    }
    return filter->sum / filter->count;
}

static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}
#if 1
static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        Serial.printf("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    httpd_resp_set_type(req, "image/jpeg");
    httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");

    size_t fb_len = 0;
    if(fb->format == PIXFORMAT_JPEG){
        Serial.printf("fb->format == PIXFORMAT_JPEG\n");
        fb_len = fb->len;
        res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
    } else {
        Serial.printf("fb->format != PIXFORMAT_JPEG\n");
        Serial.printf("fb->len     = %d\n", fb->len);
        Serial.printf("fb->width   = %d\n", fb->width);
        Serial.printf("fb->height  = %d\n", fb->height);
        Serial.printf("fb->format  = %d\n", fb->format);
        int h = 0; 
        int l = 0;
        int quadrant = 0;
        for(int i = 0; i < 9; i++){
          //！%
          pp[i] = 0;
        }
        uint16_t color = 0;
        int temp_avr = 0;
        for(int i = 0; i < fb->len/2; i++){
          //Serial.printf("%d = %x\n",i, fb->buf[2*i]<<8|fb->buf[2*i+1]);
          //Serial.printf("%d = %x\n",i, fb->buf[2*i]<<8|fb->buf[2*i+1]);
          color = fb->buf[2*i]<<8|fb->buf[2*i+1];
          #if 0
             h = i/fb->width;
             l = i%fb->width;
             quadrant = h/40 * 3 + l/53;
             if(quadrant > 8)quadrant = 8;
             //if(quadrant == 4){
              Serial.printf("%d\n",fb->buf[2*i]<<8|fb->buf[2*i+1]);
              //temp_avr += color;
             //}
             pp[quadrant]++;
        #else
         if(ColorComp(color,41261,80)){
           h = i/fb->width;
           l = i%fb->width;
           quadrant = h/40 * 3 + l/53;
           if(quadrant > 8)quadrant = 8;
           pp[quadrant]++;
         }
         #endif
        }
        //Serial.printf("%d\n",temp_avr/(fb->len/18));
        uint16_t add = 0;
        for(int i = 0; i < 9; i++){
          //Serial.printf("%d\n",pp[i]);
          add += pp[i];
        }
        for(int i = 0; i < 9; i++){
          //Serial.printf("%d\n",add);
          sent_pp[i] = pp[i]*100/(fb->len/18);
          sprintf(percentage[i],"%.1d%%",pp[i]*100/(fb->len/18));
          //Serial.printf("%s\n",percentage[i]);
        }
        //!add row line
        for(int h = 0; h < 320; h++){
          fb->buf[40 * 320 + h] = 0;
          fb->buf[80 * 320 + h] = 0;
        }
        for(int w = 0; w < 120; w++){
          fb->buf[53*2 + 320*w] = 0;
          fb->buf[53*2*2 + 320*w] = 0;
        }
        writeHzkAsc(fb,30,12,percentage[0]);
        writeHzkAsc(fb,136,12,percentage[1]);
        writeHzkAsc(fb,236,12,percentage[2]);
        writeHzkAsc(fb,30,52,percentage[3]);
        writeHzkAsc(fb,136,52,percentage[4]);
        writeHzkAsc(fb,236,52,percentage[5]);
        writeHzkAsc(fb,30,92,percentage[6]);
        writeHzkAsc(fb,136,92,percentage[7]);
        writeHzkAsc(fb,236,92,percentage[8]);
        uint8_t * buf = NULL;
        size_t buf_len = 0;
        jpg_chunking_t jchunk = {req, 0};
        bool jpeg_converted = fmt2jpg(fb->buf, fb->len, fb->width, fb->height, fb->format, 80, &buf, &buf_len);
        res = httpd_resp_send(req, (const char *)buf, buf_len);
    }
    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    //Serial.printf("JPG: %uB %ums", (uint32_t)(fb_len), (uint32_t)((fr_end - fr_start)/1000));
    return res;
}

#else
static esp_err_t capture_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        //Serial.printf(TAG, "Camera capture failed");
        Serial.printf("Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    uint8_t * buf = NULL;
    size_t buf_len = 0;
    bool converted = frame2bmp(fb, &buf, &buf_len);
    esp_camera_fb_return(fb);
    if(!converted){
        Serial.printf("BMP conversion failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    res = httpd_resp_set_type(req, "image/x-windows-bmp")
       || httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.bmp")
       || httpd_resp_send(req, (const char *)buf, buf_len);
    free(buf);
    int64_t fr_end = esp_timer_get_time();
    //ESP_LOGI(TAG, "BMP: %uKB %ums", (uint32_t)(buf_len/1024), (uint32_t)((fr_end - fr_start)/1000));
    Serial.printf("BMP: %uKB %ums", (uint32_t)(buf_len/1024), (uint32_t)((fr_end - fr_start)/1000));
    return res;
}
#endif
static esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t * fb = NULL;
    esp_err_t res = ESP_OK;
    size_t _jpg_buf_len = 0;
    uint8_t * _jpg_buf = NULL;
    char * part_buf[64];

    static int64_t last_frame = 0;
    if(!last_frame) {
        last_frame = esp_timer_get_time();
    }

    res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
    if(res != ESP_OK){
        return res;
    }

    while(true){
        fb = esp_camera_fb_get();
        if (!fb) {
            Serial.printf("Camera capture failed");
            res = ESP_FAIL;
        } else {
            if(fb->format != PIXFORMAT_JPEG){
                    int h = 0; 
        int l = 0;
        int quadrant = 0;
        for(int i = 0; i < 9; i++)
        pp[i] = 0;
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
             
                writeHzkAsc(fb,30,12, percentage[0]);
                writeHzkAsc(fb,136,12,percentage[1]);
                writeHzkAsc(fb,236,12,percentage[2]);
                writeHzkAsc(fb,30,52, percentage[3]);
                writeHzkAsc(fb,136,52,percentage[4]);
                writeHzkAsc(fb,236,52,percentage[5]);
                writeHzkAsc(fb,30,92, percentage[6]);
                writeHzkAsc(fb,136,92,percentage[7]);
                writeHzkAsc(fb,236,92,percentage[8]);
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
        if(res == ESP_OK){
            size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
            res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
        }
        if(res == ESP_OK){
            res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
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
        uint32_t avg_frame_time = ra_filter_run(&ra_filter, frame_time);/*
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

static esp_err_t cmd_handler(httpd_req_t *req){
    char*  buf;
    size_t buf_len;
    char variable[32] = {0,};
    char value[32] = {0,};

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = (char*)malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            if (httpd_query_key_value(buf, "var", variable, sizeof(variable)) == ESP_OK &&
                httpd_query_key_value(buf, "val", value, sizeof(value)) == ESP_OK) {
            } else {
                free(buf);
                httpd_resp_send_404(req);
                return ESP_FAIL;
            }
        } else {
            free(buf);
            httpd_resp_send_404(req);
            return ESP_FAIL;
        }
        free(buf);
    } else {
        httpd_resp_send_404(req);
        return ESP_FAIL;
    }

    int val = atoi(value);
    sensor_t * s = esp_camera_sensor_get();
    int res = 0;

    if(!strcmp(variable, "framesize")) {
        //if(s->pixformat == PIXFORMAT_JPEG) 
        res = s->set_framesize(s, (framesize_t)val);
    }
    else if(!strcmp(variable, "quality")) res = s->set_quality(s, val);
    else if(!strcmp(variable, "contrast")) res = s->set_contrast(s, val);
    else if(!strcmp(variable, "brightness")) res = s->set_brightness(s, val);
    else if(!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
    else if(!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
    else if(!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
    else if(!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
    else if(!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
    else if(!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
    else if(!strcmp(variable, "hmirror")) res = s->set_hmirror(s, val);
    else if(!strcmp(variable, "vflip")) res = s->set_vflip(s, val);
    else if(!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
    else if(!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
    else if(!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
    else if(!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
    else if(!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
    else if(!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
    else if(!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
    else if(!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
    else if(!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
    else if(!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
    else if(!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
    else if(!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
    else {
        res = -1;
    }

    if(res){
        return httpd_resp_send_500(req);
    }

    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, NULL, 0);
}

static esp_err_t status_handler(httpd_req_t *req){
    static char json_response[1024];

    sensor_t * s = esp_camera_sensor_get();
    char * p = json_response;
    *p++ = '{';
    p+=sprintf(p, "\"framesize\":%u,", s->status.framesize);
    p+=sprintf(p, "\"quality\":%u,", s->status.quality);
    p+=sprintf(p, "\"brightness\":%d,", s->status.brightness);
    p+=sprintf(p, "\"contrast\":%d,", s->status.contrast);
    p+=sprintf(p, "\"saturation\":%d,", s->status.saturation);
    p+=sprintf(p, "\"special_effect\":%u,", s->status.special_effect);
    p+=sprintf(p, "\"wb_mode\":%u,", s->status.wb_mode);
    p+=sprintf(p, "\"awb\":%u,", s->status.awb);
    p+=sprintf(p, "\"awb_gain\":%u,", s->status.awb_gain);
    p+=sprintf(p, "\"aec\":%u,", s->status.aec);
    p+=sprintf(p, "\"aec2\":%u,", s->status.aec2);
    p+=sprintf(p, "\"ae_level\":%d,", s->status.ae_level);
    p+=sprintf(p, "\"aec_value\":%u,", s->status.aec_value);
    p+=sprintf(p, "\"agc\":%u,", s->status.agc);
    p+=sprintf(p, "\"agc_gain\":%u,", s->status.agc_gain);
    p+=sprintf(p, "\"gainceiling\":%u,", s->status.gainceiling);
    p+=sprintf(p, "\"bpc\":%u,", s->status.bpc);
    p+=sprintf(p, "\"wpc\":%u,", s->status.wpc);
    p+=sprintf(p, "\"raw_gma\":%u,", s->status.raw_gma);
    p+=sprintf(p, "\"lenc\":%u,", s->status.lenc);
    p+=sprintf(p, "\"hmirror\":%u,", s->status.hmirror);
    p+=sprintf(p, "\"dcw\":%u,", s->status.dcw);
    p+=sprintf(p, "\"colorbar\":%u", s->status.colorbar);
    *p++ = '}';
    *p++ = 0;
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, json_response, strlen(json_response));
}

static esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
    return httpd_resp_send(req, (const char *)index_html_gz, index_html_gz_len);
}

void startCameraServer(){
    
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    httpd_uri_t index_uri = {
        .uri       = "/",
        .method    = HTTP_GET,
        .handler   = index_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t status_uri = {
        .uri       = "/status",
        .method    = HTTP_GET,
        .handler   = status_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t cmd_uri = {
        .uri       = "/control",
        .method    = HTTP_GET,
        .handler   = cmd_handler,
        .user_ctx  = NULL
    };

    httpd_uri_t capture_uri = {
        .uri       = "/capture",
        .method    = HTTP_GET,
        .handler   = capture_handler,
        .user_ctx  = NULL
    };

   httpd_uri_t stream_uri = {
        .uri       = "/stream",
        .method    = HTTP_GET,
        .handler   = stream_handler,
        .user_ctx  = NULL
    };


    ra_filter_init(&ra_filter, 20);
    Serial.printf("Starting web server on port: '%d'", config.server_port);
    if (httpd_start(&camera_httpd, &config) == ESP_OK) {
        httpd_register_uri_handler(camera_httpd, &index_uri);
        httpd_register_uri_handler(camera_httpd, &cmd_uri);
        httpd_register_uri_handler(camera_httpd, &status_uri);
        httpd_register_uri_handler(camera_httpd, &capture_uri);
    }

    config.server_port += 1;
    config.ctrl_port += 1;
    Serial.printf("Starting stream server on port: '%d'", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
        //httpd_register_uri_handler(stream_httpd, &stream_uri);
        httpd_register_uri_handler(stream_httpd, &stream_uri);
    }
}
