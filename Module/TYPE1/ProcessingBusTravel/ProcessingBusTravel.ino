#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <Adafruit_MCP4725.h>
#include <VL53L0X.h>

Adafruit_MCP4725 buzzer;
VL53L0X lox;

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

// Пин для управления виброплитой (реле или мотор)
#define VIBRATION_PIN 10

// Переменные для звука
int frequencyToPlay = 0;
int durationToPlay = 0;
bool commandReceived = false;
bool isPlaying = false;
unsigned long playEndTime = 0;
unsigned long lastToggleTime = 0;
bool highState = false;

// Переменные для виброплиты
bool vibrationState = false;

// Таймеры
unsigned long lastCheck = 0;
const unsigned long checkInterval = 500;    // проверка команд
unsigned long lastDistanceSend = 0;
const unsigned long distanceSendInterval = 200;  // отправка расстояния чаще

// Функция чтения расстояния с VL53L0X (возвращает расстояние в мм)
uint16_t getDistanceMM() {
  uint16_t dist = lox.readRangeSingleMillimeters();
  if (lox.timeoutOccurred()) {
    Serial.println("⚠️ VL53L0X timeout!");
    return 9999; // большое число, чтобы показать ошибку
  }
  return dist;
}

// Отправка расстояния на сервер
void sendDistance() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  uint16_t distance = getDistanceMM();
  
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) +
               "/distance/" + String(deviceId) + "?value=" + String(distance);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    Serial.printf("📏 Distance sent: %d mm\n", distance);
  } else {
    Serial.printf("❌ Failed to send distance, HTTP code: %d\n", httpCode);
  }
  
  http.end();
}

void checkForCommand() {
  if (WiFi.status() != WL_CONNECTED) return;
  
  HTTPClient http;
  String url = "http://" + String(serverIP) + ":" + String(serverPort) +
               "/get_command/" + String(deviceId);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == 200) {
    String response = http.getString();
    response.trim();
    
    // Ожидаемый формат: "freq,dur,vib" или "0,0,0"
    int firstComma = response.indexOf(',');
    int secondComma = response.indexOf(',', firstComma + 1);
    
    if (firstComma > 0 && secondComma > 0) {
      int freq = response.substring(0, firstComma).toInt();
      int dur = response.substring(firstComma + 1, secondComma).toInt();
      int vib = response.substring(secondComma + 1).toInt();
      
      // Обработка вибрации
      if (vib == 1) {
        if (!vibrationState) {
          vibrationState = true;
          digitalWrite(VIBRATION_PIN, HIGH);
          Serial.println("💪 Vibration ON");
        }
      } else {
        if (vibrationState) {
          vibrationState = false;
          digitalWrite(VIBRATION_PIN, LOW);
          Serial.println("💤 Vibration OFF");
        }
      }
      
      // Обработка звука
      if (freq > 0 && dur > 0) {
        frequencyToPlay = freq;
        durationToPlay = dur;
        commandReceived = true;
        Serial.printf("🔊 Sound command: %d Hz, %d ms\n", frequencyToPlay, durationToPlay);
      }
    } else {
      // Если ответ не в новом формате, пробуем старый "freq,dur" (для совместимости)
      int commaIndex = response.indexOf(',');
      if (commaIndex > 0) {
        int freq = response.substring(0, commaIndex).toInt();
        int dur = response.substring(commaIndex + 1).toInt();
        if (freq > 0 && dur > 0) {
          frequencyToPlay = freq;
          durationToPlay = dur;
          commandReceived = true;
          Serial.printf("🔊 Sound command (legacy): %d Hz, %d ms\n", frequencyToPlay, durationToPlay);
        }
      }
    }
  }
  
  http.end();
}

void startPlaying() {
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

void setup() {
  Serial.begin(115200);
  
  // Настройка пина виброплиты
  pinMode(VIBRATION_PIN, OUTPUT);
  digitalWrite(VIBRATION_PIN, LOW);
  
  // Инициализация I2C (для MCP4725 и VL53L0X)
  Wire.begin();
  
  // Инициализация MCP4725 (пищалка)
  if (!buzzer.begin(0x61)) {  // попробуйте 0x60 если не работает
    Serial.println("❌ MCP4725 not found! Check wiring.");
    while (1) delay(10);
  }
  Serial.println("✅ MCP4725 initialized");
  buzzer.setVoltage(VOLTAGE_LOW, false);
  
  // Инициализация VL53L0X (дальномер)
  Serial.println("🔍 Initializing VL53L0X...");
  lox.init();
  lox.setTimeout(500);
  // Устанавливаем бюджет времени измерения для высокой точности (200 мс)
  lox.setMeasurementTimingBudget(200000);
  Serial.println("✅ VL53L0X ready");
  
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
  unsigned long currentMillis = millis();
  
  // Проверяем команды с сервера
  if (currentMillis - lastCheck >= checkInterval) {
    lastCheck = currentMillis;
    checkForCommand();       // получает звук + вибрацию
  }
  
  // Отправляем расстояние на сервер
  if (currentMillis - lastDistanceSend >= distanceSendInterval) {
    lastDistanceSend = currentMillis;
    sendDistance();          // отправляет расстояние в мм
  }
  
  // Генерация звука через MCP4725
  if (isPlaying) {
    if (micros() - lastToggleTime >= (1000000 / frequencyToPlay / 2)) {
      lastToggleTime = micros();
      highState = !highState;
      buzzer.setVoltage(highState ? VOLTAGE_HIGH : VOLTAGE_LOW, false);
    }
    
    // Проверяем, не закончилось ли время
    if (currentMillis >= playEndTime) {
      stopPlaying();
    }
  }
  
  delay(1);
}
