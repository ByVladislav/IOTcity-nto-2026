#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>

Adafruit_MCP4725 buzzer;

// Настройки Wi-Fi
const char* ssid = "NTO_MGBOT_CITY";
const char* password = "Terminator812";

// IP адрес вашего компьютера с сервером
const char* serverIP = "192.168.31.105";
const int serverPort = 5000;

// ID этого устройства (для камер 32, 25, 27, 19)
const char* deviceId = "32";  // измените на нужный ID

// Настройки звука для MCP4725
#define VOLTAGE_HIGH 3000  // амплитуда (0-4095)
#define VOLTAGE_LOW 0

// Переменные
int frequencyToPlay = 0;
int durationToPlay = 0;
bool commandReceived = false;
bool isPlaying = false;
unsigned long playEndTime = 0;
unsigned long lastToggleTime = 0;
bool highState = false;

unsigned long lastCheck = 0;
const unsigned long checkInterval = 500;

void setup() {
  Serial.begin(115200);
  
  // Инициализация MCP4725
  Wire.begin();
  if (!buzzer.begin(0x61)) {  // попробуйте 0x60 если не работает
    Serial.println("❌ MCP4725 not found! Check wiring.");
    while (1) delay(10);
  }
  Serial.println("✅ MCP4725 initialized");
  buzzer.setVoltage(VOLTAGE_LOW, false);
  
  // Подключение к WiFi
  WiFi.begin(ssid, password);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\n✅ WiFi connected");
  Serial.print("📡 IP: ");
  Serial.println(WiFi.localIP());
  Serial.print("🎵 Device ID: ");
  Serial.println(deviceId);
  
  // Тест при запуске (короткие звуки)
  playTone(1000, 200);
  delay(200);
  playTone(1500, 200);
  delay(200);
  playTone(2000, 200);
}

void loop() {
  // Проверяем команды с сервера
  if (millis() - lastCheck >= checkInterval) {
    lastCheck = millis();
    checkForCommand();
  }
  
  // Если есть команда - запускаем воспроизведение
  if (commandReceived) {
    startPlaying();
    commandReceived = false;
  }
  
  // Генерация звука через MCP4725
  if (isPlaying) {
    if (micros() - lastToggleTime >= (1000000 / frequencyToPlay / 2)) {
      lastToggleTime = micros();
      highState = !highState;
      buzzer.setVoltage(highState ? VOLTAGE_HIGH : VOLTAGE_LOW, false);
    }
    
    // Проверяем, не закончилось ли время
    if (millis() >= playEndTime) {
      stopPlaying();
    }
  }
  
  delay(1);
}

void checkForCommand() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/get_command/" + String(deviceId);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String response = http.getString();
    response.trim();
    
    if (response != "0,0" && response.length() > 0) {
      int commaIndex = response.indexOf(',');
      if (commaIndex > 0) {
        int freq = response.substring(0, commaIndex).toInt();
        int dur = response.substring(commaIndex + 1).toInt();
        
        if (freq > 0 && dur > 0) {
          frequencyToPlay = freq;
          durationToPlay = dur;
          commandReceived = true;
          Serial.printf("📩 Command received: %d Hz, %d ms\n", frequencyToPlay, durationToPlay);
        }
      }
    }
  }
  
  http.end();
}

void startPlaying() {
  // Останавливаем предыдущий звук если играет
  if (isPlaying) {
    stopPlaying();
  }
  
  if (frequencyToPlay > 0 && durationToPlay > 0) {
    isPlaying = true;
    playEndTime = millis() + durationToPlay;
    lastToggleTime = micros();
    highState = false;
    Serial.printf("🔊 Playing: %d Hz for %d ms\n", frequencyToPlay, durationToPlay);
  }
}

void stopPlaying() {
  isPlaying = false;
  buzzer.setVoltage(VOLTAGE_LOW, false);
  Serial.println("🔇 Sound stopped");
}

void playTone(int frequency, int duration) {
  if (frequency <= 0 || duration <= 0) return;
  
  unsigned long startTime = millis();
  unsigned long period = 1000000 / frequency;
  bool state = false;
  
  while (millis() - startTime < duration) {
    state = !state;
    buzzer.setVoltage(state ? VOLTAGE_HIGH : VOLTAGE_LOW, false);
    delayMicroseconds(period / 2);
  }
  
  buzzer.setVoltage(VOLTAGE_LOW, false);
}
