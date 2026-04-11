#include <WiFi.h>
#include <HTTPClient.h>
#include <Wire.h>
#include <GyverBME280.h>  // Библиотека GyverBME280

// Настройки Wi-Fi
const char* ssid = "NTO_MGBOT_CITY";
const char* password = "Terminator812";

// IP адрес вашего компьютера с сервером
const char* serverIP = "192.168.31.105";
const int serverPort = 5000;

// ID этого устройства (для BME280 датчика)
const String deviceId = "BME280_1";

// Эндпоинт для отправки данных
const String endpoint = "/sensor_data";

// Создание объекта датчика
GyverBME280 bme;

// Интервал отправки данных (миллисекунды)
const unsigned long sendInterval = 2000;  // Отправка каждые 2 секунды
unsigned long lastSendTime = 0;


void sendSensorData(float temperature, float humidity, float pressure) {
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("❌ WiFi не подключен");
        return;
    }
    
    HTTPClient http;
    String url = "http://";
    url += serverIP;
    url += ":";
    url += serverPort;
    url += endpoint;
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    
    // Создаем JSON с данными
    String jsonData = "{";
    jsonData += "\"device_id\": \"" + deviceId + "\",";
    jsonData += "\"temperature\": " + String(temperature, 1) + ",";
    jsonData += "\"humidity\": " + String(humidity, 1) + ",";
    jsonData += "\"pressure\": " + String(pressure, 1);
    jsonData += "}";
    
    Serial.println("📤 Отправка: " + jsonData);
    
    int httpCode = http.POST(jsonData);
    
    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            Serial.println("✅ Данные успешно отправлены на сервер");
        } else {
            Serial.printf("⚠️ Ошибка при отправке, код: %d\n", httpCode);
        }
    } else {
        Serial.printf("❌ Ошибка соединения: %s\n", http.errorToString(httpCode).c_str());
    }
    
    http.end();
}


void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n=================================");
    Serial.println("🌡️ BME280 СЕНСОРНЫЙ МОДУЛЬ (GyverBME280)");
    Serial.println("=================================");
    
    // Инициализация I2C
    Wire.begin();
    
    // Инициализация BME280 через GyverBME280
    Serial.print("🔍 Поиск BME280 датчика...");
    if (!bme.begin(0x77)) {
        Serial.println(" ❌");
        Serial.println("⚠️ BME280 не найден! Проверьте подключение.");
        Serial.println("   - Проверьте питание (3.3V)");
        Serial.println("   - Проверьте подключение SDA/SCL");
        Serial.println("   - Адрес по умолчанию: 0x76");
        Serial.println("   - Попробуйте адрес 0x77: bme.begin(0x77)");
        while (1) delay(100);
    }
    Serial.println(" ✅");
    
    // Дополнительные настройки датчика (опционально)
    // bme.setMode(FORCED_MODE);           // Режим единичного измерения
    // bme.setFilter(FILTER_COEF_4);       // Фильтр
    // bme.setHumOversampling(OVERSAMPLING_4);   // Оверсэмплинг влажности
    // bme.setTempOversampling(OVERSAMPLING_4);  // Оверсэмплинг температуры
    // bme.setPressOversampling(OVERSAMPLING_4); // Оверсэмплинг давления
    
    // Подключение к WiFi
    Serial.print("📡 Подключение к WiFi");
    WiFi.begin(ssid, password);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n✅ WiFi подключен");
        Serial.print("📍 IP адрес: ");
        Serial.println(WiFi.localIP());
        Serial.print("🎯 Сервер: ");
        Serial.print(serverIP);
        Serial.print(":");
        Serial.println(serverPort);
    } else {
        Serial.println("\n❌ Ошибка подключения к WiFi");
        Serial.println("   - Проверьте SSID и пароль");
        Serial.println("   - Проверьте доступность сети");
    }
    
    Serial.println("=================================");
    Serial.println("✅ Модуль готов к работе");
    Serial.println("=================================\n");
}


void loop() {
    unsigned long currentTime = millis();
    
    // Отправляем данные каждые sendInterval миллисекунд
    if (currentTime - lastSendTime >= sendInterval) {
        lastSendTime = currentTime;
        
        // Читаем данные с датчика через GyverBME280
        float temperature = bme.readTemperature();
        float humidity = bme.readHumidity();
        float pressure = bme.readPressure() / 100.0F;  // Конвертируем Па в гПа
        
        // Проверяем валидность данных (если датчик не отвечает)
        if (isnan(temperature) || isnan(humidity) || isnan(pressure)) {
            Serial.println("❌ Ошибка чтения данных с датчика BME280");
            return;
        }
        
        // Выводим в консоль модуля
        Serial.println("\n🌡️ НОВЫЕ ПОКАЗАНИЯ:");
        Serial.println("=================================");
        Serial.printf("   🌡️ Температура: %.1f °C\n", temperature);
        Serial.printf("   💧 Влажность:    %.1f %%\n", humidity);
        Serial.printf("   📊 Давление:     %.1f hPa\n", pressure);
        Serial.println("=================================");
        
        // Отправляем на сервер
        sendSensorData(temperature, humidity, pressure);
    }
    
    delay(10);  // Небольшая задержка для стабильности
}
