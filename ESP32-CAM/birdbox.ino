/*
EE459 Team 18 Project - Bird Box
Will Volpe, Chris Manna, Aaron Bergen
*/

// Import required libraries
#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include "index_html.h"
#include "Arduino.h"
#include "esp_camera.h"

// Replace with network credentials
// const char* ssid = "Will";
// const char* password = "wifiwill";
const char* ssid = "SpectrumSetup-7F";
const char* password = "hotelquaint730";

bool camState = 0;
bool ledState = 0;
bool local_ledState = 0;
bool doorState = 1;
bool redledState = 0;
bool nightState = 0;
bool fillState = 0;

// working
const int ledPin = 4;
const int doorPin = 13;
const int redledPin = 33;
const int cam_atmega_pin = 12;
const int pin_14 = 14;
const int pin_15 = 15;

// in progress
const int pin_2 = 2;



// Create AsyncWebServer object on port 80
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

// ESP32 AI Thinker Module Pinout
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

typedef struct {
  camera_fb_t * fb;
  size_t index;
} camera_frame_t;

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: %s\r\nContent-Length: %u\r\n\r\n";
  static const char * JPG_CONTENT_TYPE = "image/jpeg";
static const char * BMP_CONTENT_TYPE = "image/x-windows-bmp";

class AsyncJpegStreamResponse: public AsyncAbstractResponse {
  private:
    camera_frame_t _frame;
    size_t _index;
    size_t _jpg_buf_len;
    uint8_t * _jpg_buf;
    uint64_t lastAsyncRequest;
  public:
  AsyncJpegStreamResponse(){
    _callback = nullptr;
    _code = 200;
    _contentLength = 0;
    _contentType = STREAM_CONTENT_TYPE;
    _sendContentLength = false;
    _chunked = true;
    _index = 0;
    _jpg_buf_len = 0;
    _jpg_buf = NULL;
    lastAsyncRequest = 0;
    memset(&_frame, 0, sizeof(camera_frame_t));
  }
  ~AsyncJpegStreamResponse(){
    if(_frame.fb){
      if(_frame.fb->format != PIXFORMAT_JPEG){
        free(_jpg_buf);
      }
      esp_camera_fb_return(_frame.fb);
    }
  }
  bool _sourceValid() const {
    return true;
  }
  virtual size_t _fillBuffer(uint8_t *buf, size_t maxLen) override {
    size_t ret = _content(buf, maxLen, _index);
    if(ret != RESPONSE_TRY_AGAIN){
        _index += ret;
    }
    return ret;
  }
  size_t _content(uint8_t *buffer, size_t maxLen, size_t index){
    if(!_frame.fb || _frame.index == _jpg_buf_len){
      if(index && _frame.fb){
        uint64_t end = (uint64_t)micros();
        int fp = (end - lastAsyncRequest) / 1000;
        log_printf("Size: %uKB, Time: %ums (%.1ffps)\n", _jpg_buf_len/1024, fp);
        lastAsyncRequest = end;
        if(_frame.fb->format != PIXFORMAT_JPEG){
          free(_jpg_buf);
        }
        esp_camera_fb_return(_frame.fb);
        _frame.fb = NULL;
        _jpg_buf_len = 0;
        _jpg_buf = NULL;
      }
      if(maxLen < (strlen(STREAM_BOUNDARY) + strlen(STREAM_PART) + strlen(JPG_CONTENT_TYPE) + 8)){
        //log_w("Not enough space for headers");
        return RESPONSE_TRY_AGAIN;
      }
      //get frame
      _frame.index = 0;

      _frame.fb = esp_camera_fb_get();
      if (_frame.fb == NULL) {
        log_e("Camera frame failed");
        return 0;
      }

      if(_frame.fb->format != PIXFORMAT_JPEG){
        unsigned long st = millis();
        bool jpeg_converted = frame2jpg(_frame.fb, 80, &_jpg_buf, &_jpg_buf_len);
        if(!jpeg_converted){
          log_e("JPEG compression failed");
          esp_camera_fb_return(_frame.fb);
          _frame.fb = NULL;
          _jpg_buf_len = 0;
          _jpg_buf = NULL;
          return 0;
        }
        log_i("JPEG: %lums, %uB", millis() - st, _jpg_buf_len);
      } else {
        _jpg_buf_len = _frame.fb->len;
        _jpg_buf = _frame.fb->buf;
      }

      //send boundary
      size_t blen = 0;
      if(index){
        blen = strlen(STREAM_BOUNDARY);
        memcpy(buffer, STREAM_BOUNDARY, blen);
        buffer += blen;
      }
      //send header
      size_t hlen = sprintf((char *)buffer, STREAM_PART, JPG_CONTENT_TYPE, _jpg_buf_len);
      buffer += hlen;
      //send frame
      hlen = maxLen - hlen - blen;
      if(hlen > _jpg_buf_len){
        maxLen -= hlen - _jpg_buf_len;
        hlen = _jpg_buf_len;
      }
      memcpy(buffer, _jpg_buf, hlen);
      _frame.index += hlen;
      return maxLen;
    }
  
    size_t available = _jpg_buf_len - _frame.index;
    if(maxLen > available){
      maxLen = available;
    }
    memcpy(buffer, _jpg_buf+_frame.index, maxLen);
    _frame.index += maxLen;
  
    return maxLen;
  }
};

void streamJpg(AsyncWebServerRequest *request){
  AsyncJpegStreamResponse *response = new AsyncJpegStreamResponse();
  if(!response){
    request->send(501);
    return;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

// 
void notifyClients_cam() { ws.textAll(String(camState)); }
void notifyClients_led() { ws.textAll(String(ledState + 2)); }
void notifyClients_door() { ws.textAll(String(doorState + 4)); }
void notifyClients_night() { ws.textAll(String(nightState + 6)); }
void notifyClients_fill() { ws.textAll(String(fillState + 8)); }




void handleWebSocketMessage(void *arg, uint8_t *data, size_t len) 
{
  AwsFrameInfo *info = (AwsFrameInfo*)arg;
  if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) 
  {
    data[len] = 0;
    if (strcmp((char*)data, "toggle_cam") == 0) 
    {
      camState = !camState;
      notifyClients_cam();
    }
    if (strcmp((char*)data, "toggle_led") == 0) 
    {
      ledState = !ledState;
      notifyClients_led();
    }
    if (strcmp((char*)data, "toggle_door") == 0) 
    {
      doorState = !doorState;
      notifyClients_door();
    }
  }
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
             void *arg, uint8_t *data, size_t len) {
  switch (type) {
    case WS_EVT_CONNECT:
      Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
      break;
    case WS_EVT_DISCONNECT:
      Serial.printf("WebSocket client #%u disconnected\n", client->id());
      break;
    case WS_EVT_DATA:
      handleWebSocketMessage(arg, data, len);
      break;
    case WS_EVT_PONG:
    case WS_EVT_ERROR:
      break;
  }
}

void initWebSocket() {
  ws.onEvent(onEvent);
  server.addHandler(&ws);
}

String processor(const String& var){
  Serial.println(var);
  if(var == "CAM_STATE"){
    if (camState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  if(var == "LED_STATE"){
    if (ledState){
      return "ON";
    }
    else{
      return "OFF";
    }
  }
  if(var == "DOOR_STATE"){
    if (doorState){
      return "CLOSED";
    }
    else{
      return "OPEN";
    }
  }
  if(var == "NIGHT_STATE")
  {
    if (nightState){
      return "ON";
    }
    else{
      return "OFF";
    } 
  }
  if(var == "FILL_STATE")
  {
    if (fillState){
      return "EMPTY";
    }
    else{
      return "FULL";
    } 
  }
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

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
  
  if(psramFound()){
    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 15;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  
  // Camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  pinMode(ledPin, OUTPUT);
  pinMode(doorPin, OUTPUT);
  pinMode(redledPin, OUTPUT);
  pinMode(cam_atmega_pin, INPUT);
  pinMode(pin_14, INPUT);
  pinMode(pin_15, INPUT_PULLDOWN);
  pinMode(pin_2, INPUT_PULLDOWN);

  
 

  digitalWrite(ledPin, LOW);
  digitalWrite(doorPin, HIGH);
  digitalWrite(redledPin, LOW);

  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.println("Connecting to WiFi..");
    redledState = !redledState;
    digitalWrite(redledPin, redledState);
    delay(1000);

  }
  digitalWrite(redledPin, LOW);


  // Print ESP Local IP Address
  Serial.println(WiFi.localIP());

  initWebSocket();

  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/stream", HTTP_GET, streamJpg);

  // Start server
  server.begin();
}

void loop() {
  ws.cleanupClients();

  if (ledState == HIGH || local_ledState == HIGH)
  {
    digitalWrite(ledPin, HIGH);
    //Serial.println("camera flash!");

  }
  else 
  {
    digitalWrite(ledPin, LOW);
  }
  digitalWrite(doorPin, doorState);
  int bird_detected, squirrel_detected, feed_empty, night_mode, empty; 

  bird_detected = digitalRead(cam_atmega_pin);
  squirrel_detected = digitalRead(pin_14);
  night_mode = digitalRead(pin_15);
  empty = digitalRead(pin_2);


  if (bird_detected == 1) {
    if (camState == 0) {
      ws.textAll(String(1));
    }   }
  else { 
    if (camState == 0) {
      ws.textAll(String(0));
    } 
  }

  if (squirrel_detected == 1) 
  { 
    local_ledState = HIGH;
    ws.textAll(String(3));
    //Serial.println("PIN HIGH");

  }
  else 
  { 
    if (ledState == LOW) 
    {
      local_ledState = LOW;
      ws.textAll(String(2));
    }
    else
    {
      local_ledState = LOW;
    }
  }

  if (night_mode == 1) 
  {
    if (nightState == 0) 
    {    
      nightState = HIGH;
      notifyClients_night();
    }
  }
  else
  {
    if (nightState == 1) 
    {    
      nightState = LOW;
      notifyClients_night();
    }
  }

  if (empty == 1) 
  {
    if (fillState == 0) 
    {    
      fillState = HIGH;
      notifyClients_fill();
    }
  }
  else
  {
    if (fillState == 1) 
    {    
      fillState = LOW;
      notifyClients_fill();
    }
  }


  delay(1000);


}