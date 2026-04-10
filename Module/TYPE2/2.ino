#include <WiFi.h>
#include <HTTPClient.h>
#include <FastLED.h>

// Настройки Wi-Fi
const char* ssid = "NTO_MGBOT_CITY";
const char* password = "Terminator812";

// IP адрес вашего компьютера с сервером
const char* serverIP = "192.168.31.105";
const int serverPort = 5000;

const char* deviceId = "34";  // измените на нужный ID

// Настройки RGB ленты
#define LED_PIN        14
#define NUM_LEDS       8
#define LED_TYPE       WS2812B
#define COLOR_ORDER    GRB

CRGB leds[NUM_LEDS];

// Переменные для хранения цвета (используем uint8_t вместо int для экономии памяти)
uint8_t currentR = 0;
uint8_t currentG = 0;
uint8_t currentB = 0;
bool colorChanged = false;

unsigned long lastCheck = 0;
const unsigned long checkInterval = 500;

void setup() {
  Serial.begin(115200);
  
  // Инициализация RGB ленты
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setBrightness(50);
  
  // Упрощенный эффект запуска (меньше кода)
  startupEffect();
  
  // Подключение к WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected");
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("Device ID: ");
  Serial.println(deviceId);
  
  // Начальный цвет (белый)
  setColor(255, 255, 255);
}

void loop() {
  if (millis() - lastCheck >= checkInterval) {
    lastCheck = millis();
    checkColorFromServer();
  }
  
  if (colorChanged) {
    updateLEDs();
    colorChanged = false;
  }
  
  delay(10);
}

void checkColorFromServer() {
  HTTPClient http;
  String url = "http://";
  url += serverIP;
  url += ":";
  url += serverPort;
  url += "/get_color/";
  url += deviceId;
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String response = http.getString();
    response.trim();
    
    int firstComma = response.indexOf(',');
    int secondComma = response.lastIndexOf(',');
    
    if (firstComma > 0 && secondComma > firstComma) {
      uint8_t r = response.substring(0, firstComma).toInt();
      uint8_t g = response.substring(firstComma + 1, secondComma).toInt();
      uint8_t b = response.substring(secondComma + 1).toInt();
      
      if (r != currentR || g != currentG || b != currentB) {
        currentR = r;
        currentG = g;
        currentB = b;
        colorChanged = true;
        
        Serial.print("New color: RGB(");
        Serial.print(currentR);
        Serial.print(",");
        Serial.print(currentG);
        Serial.print(",");
        Serial.println(currentB);
      }
    }
  } else {
    Serial.print("HTTP error: ");
    Serial.println(httpCode);
  }
  
  http.end();
}

void updateLEDs() {
  CRGB color(currentR, currentG, currentB);
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = color;
  }
  FastLED.show();
}

void setColor(int r, int g, int b) {
  currentR = r;
  currentG = g;
  currentB = b;
  updateLEDs();
}

void startupEffect() {
  // Зеленые огни по очереди
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 255, 0);
    FastLED.show();
    delay(50);
  }
  
  delay(200);
  
  // Выключение
  for (int i = 0; i < NUM_LEDS; i++) {
    leds[i] = CRGB(0, 0, 0);
    FastLED.show();
    delay(50);
  }
  
  // Быстрая пульсация (упрощено)
  for (int b = 0; b < 255; b += 15) {
    FastLED.setBrightness(b);
    fill_solid(leds, NUM_LEDS, CRGB(0, 100, 255));
    FastLED.show();
    delay(3);
  }
  
  FastLED.setBrightness(50);
  setColor(255, 255, 255);
}