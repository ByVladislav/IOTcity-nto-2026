/* Includes ---------------------------------------------------------------- */
#include <MLX_inferencing.h>
#include "edge-impulse-sdk/dsp/image/image.hpp"
#include "esp_camera.h"

// Add WiFi and HTTP includes
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// Select camera model
#define CAMERA_MODEL_AI_THINKER // Has PSRAM

#if defined(CAMERA_MODEL_AI_THINKER)
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

// Flash LED pin for AI Thinker ESP32-CAM
#define FLASH_LED_PIN     4

#else
#error "Camera model not selected"
#endif

/* Constant defines -------------------------------------------------------- */
#define EI_CAMERA_RAW_FRAME_BUFFER_COLS           320
#define EI_CAMERA_RAW_FRAME_BUFFER_ROWS           240
#define EI_CAMERA_FRAME_BYTE_SIZE                 3

// WiFi Configuration
const char* ssid = "NTO_MGBOT_CITY";
const char* password = "Terminator812";
const char* serverIP = "192.168.31.105";
const int serverPort = 5000;
const char* deviceId = "32";  // Camera ID: 32, 25, 27, or 19

/* Private variables ------------------------------------------------------- */
static bool debug_nn = false;
static bool is_initialised = false;
uint8_t *snapshot_buf; //points to the output of the capture

// Server communication variables
unsigned long lastServerUpdate = 0;
const unsigned long serverUpdateInterval = 1000; // Send update every 1 second
bool wifiConnected = false;

// LED control variables
unsigned long lastLedToggle = 0;
const unsigned long ledToggleInterval = 200; // Blink every 200ms during connection
bool ledState = false;

static camera_config_t camera_config = {
    .pin_pwdn = PWDN_GPIO_NUM,
    .pin_reset = RESET_GPIO_NUM,
    .pin_xclk = XCLK_GPIO_NUM,
    .pin_sscb_sda = SIOD_GPIO_NUM,
    .pin_sscb_scl = SIOC_GPIO_NUM,

    .pin_d7 = Y9_GPIO_NUM,
    .pin_d6 = Y8_GPIO_NUM,
    .pin_d5 = Y7_GPIO_NUM,
    .pin_d4 = Y6_GPIO_NUM,
    .pin_d3 = Y5_GPIO_NUM,
    .pin_d2 = Y4_GPIO_NUM,
    .pin_d1 = Y3_GPIO_NUM,
    .pin_d0 = Y2_GPIO_NUM,
    .pin_vsync = VSYNC_GPIO_NUM,
    .pin_href = HREF_GPIO_NUM,
    .pin_pclk = PCLK_GPIO_NUM,

    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,

    .pixel_format = PIXFORMAT_JPEG,
    .frame_size = FRAMESIZE_QVGA,

    .jpeg_quality = 12,
    .fb_count = 1,
    .fb_location = CAMERA_FB_IN_PSRAM,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

// Structure to hold detection results
struct Detection {
    String label;
    float confidence;
};

/* Function definitions ------------------------------------------------------- */
bool ei_camera_init(void);
void ei_camera_deinit(void);
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf);
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr);

// Flash LED control functions
void initFlashLED() {
    pinMode(FLASH_LED_PIN, OUTPUT);
    digitalWrite(FLASH_LED_PIN, LOW);
}

void setFlashLED(bool state) {
    digitalWrite(FLASH_LED_PIN, state ? HIGH : LOW);
}

void blinkFlashLED(int times, int delayMs) {
    for (int i = 0; i < times; i++) {
        setFlashLED(HIGH);
        delay(delayMs);
        setFlashLED(LOW);
        if (i < times - 1) {
            delay(delayMs);
        }
    }
}

// WiFi functions
void connectToWiFi() {
    Serial.print("Connecting to WiFi");
    
    WiFi.begin(ssid, password);
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 40) {
        // Blink LED while connecting
        if (millis() - lastLedToggle >= ledToggleInterval) {
            lastLedToggle = millis();
            ledState = !ledState;
            setFlashLED(ledState);
        }
        
        delay(100);
        Serial.print(".");
        attempts++;
    }
    
    // Turn off LED after connection attempt
    setFlashLED(LOW);
    
    if (WiFi.status() == WL_CONNECTED) {
        wifiConnected = true;
        Serial.println("\n✅ WiFi connected");
        Serial.print("📡 IP: ");
        Serial.println(WiFi.localIP());
        Serial.print("📷 Camera ID: ");
        Serial.println(deviceId);
        
        // Success indication: 3 quick blinks
        blinkFlashLED(3, 100);
        
    } else {
        Serial.println("\n❌ WiFi connection failed");
        wifiConnected = false;
        
        // Failure indication: 1 long blink
        setFlashLED(HIGH);
        delay(1000);
        setFlashLED(LOW);
    }
}

// Function to sort detections by confidence
void sortDetections(Detection detections[], int count) {
    for (int i = 0; i < count - 1; i++) {
        for (int j = 0; j < count - i - 1; j++) {
            if (detections[j].confidence < detections[j + 1].confidence) {
                Detection temp = detections[j];
                detections[j] = detections[j + 1];
                detections[j + 1] = temp;
            }
        }
    }
}

// Function to send detection results to server
void sendDetectionToServer(Detection topDetections[], int count) {
    if (!wifiConnected || WiFi.status() != WL_CONNECTED) {
        return;
    }
    
    HTTPClient http;
    
    StaticJsonDocument<512> doc;
    
    doc["device_id"] = deviceId;
    doc["timestamp"] = millis();
    doc["top_detection"] = topDetections[0].label;
    doc["confidence"] = topDetections[0].confidence;
    
    JsonArray detections = doc.createNestedArray("detections");
    for (int i = 0; i < count && i < 3; i++) {
        JsonObject det = detections.createNestedObject();
        det["label"] = topDetections[i].label;
        det["confidence"] = topDetections[i].confidence;
        det["rank"] = i + 1;
    }
    
    doc["total_classes"] = EI_CLASSIFIER_LABEL_COUNT;
    
    String jsonString;
    serializeJson(doc, jsonString);
    
    String url = "http://" + String(serverIP) + ":" + String(serverPort) + "/update_detection";
    
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(2000);
    
    int httpCode = http.POST(jsonString);
    
    if (httpCode == 200) {
        String response = http.getString();
        Serial.printf("📤 Data sent to server successfully\n");
        Serial.printf("   Top: %s (%.2f%%)\n", 
                     topDetections[0].label.c_str(), 
                     topDetections[0].confidence * 100);
        if (count > 1) {
            Serial.printf("   2nd: %s (%.2f%%)\n", 
                         topDetections[1].label.c_str(), 
                         topDetections[1].confidence * 100);
        }
        if (count > 2) {
            Serial.printf("   3rd: %s (%.2f%%)\n", 
                         topDetections[2].label.c_str(), 
                         topDetections[2].confidence * 100);
        }
        
        // Quick blink to indicate successful send
        setFlashLED(HIGH);
        delay(50);
        setFlashLED(LOW);
        
    } else {
        Serial.printf("❌ Server error: %d\n", httpCode);
    }
    
    http.end();
}

/**
* @brief      Arduino setup function
*/
void setup()
{
    Serial.begin(115200);
    Serial.println("Edge Impulse Inferencing Demo with Server Communication");
    
    // Initialize flash LED
    initFlashLED();
    
    // Initial LED indication - ready to connect
    setFlashLED(HIGH);
    delay(500);
    setFlashLED(LOW);
    
    // Connect to WiFi first (LED will blink during connection)
    connectToWiFi();
    
    if (ei_camera_init() == false) {
        ei_printf("Failed to initialize Camera!\r\n");
        // Camera init failed - 2 long blinks
        blinkFlashLED(2, 500);
    }
    else {
        ei_printf("Camera initialized\r\n");
        // Camera init success - 2 quick blinks
        blinkFlashLED(2, 100);
    }

    ei_printf("\nStarting continuous inference in 2 seconds...\n");
    
    // Countdown with LED
    for (int i = 0; i < 4; i++) {
        setFlashLED(HIGH);
        delay(250);
        setFlashLED(LOW);
        delay(250);
    }
    
    ei_sleep(2000);
}

/**
* @brief      Get data and run inferencing
*/
void loop()
{
    // instead of wait_ms, we'll wait on the signal, this allows threads to cancel us...
    if (ei_sleep(5) != EI_IMPULSE_OK) {
        return;
    }

    snapshot_buf = (uint8_t*)malloc(EI_CAMERA_RAW_FRAME_BUFFER_COLS * EI_CAMERA_RAW_FRAME_BUFFER_ROWS * EI_CAMERA_FRAME_BYTE_SIZE);

    // check if allocation was successful
    if(snapshot_buf == nullptr) {
        ei_printf("ERR: Failed to allocate snapshot buffer!\n");
        return;
    }

    ei::signal_t signal;
    signal.total_length = EI_CLASSIFIER_INPUT_WIDTH * EI_CLASSIFIER_INPUT_HEIGHT;
    signal.get_data = &ei_camera_get_data;

    if (ei_camera_capture((size_t)EI_CLASSIFIER_INPUT_WIDTH, (size_t)EI_CLASSIFIER_INPUT_HEIGHT, snapshot_buf) == false) {
        ei_printf("Failed to capture image\r\n");
        free(snapshot_buf);
        return;
    }

    // Run the classifier
    ei_impulse_result_t result = { 0 };

    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, debug_nn);
    if (err != EI_IMPULSE_OK) {
        ei_printf("ERR: Failed to run classifier (%d)\n", err);
        free(snapshot_buf);
        return;
    }

    // Prepare detections array
    Detection detections[EI_CLASSIFIER_LABEL_COUNT];
    int detectionCount = 0;
    float maxConfidence = 0.0f;

#if EI_CLASSIFIER_OBJECT_DETECTION == 1
    ei_printf("Object detection bounding boxes:\r\n");
    for (uint32_t i = 0; i < result.bounding_boxes_count; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.value == 0) {
            continue;
        }
        
        detections[detectionCount].label = String(bb.label);
        detections[detectionCount].confidence = bb.value;
        detectionCount++;
        
        if (bb.value > maxConfidence) {
            maxConfidence = bb.value;
        }
        
        ei_printf("  %s (%f) [ x: %u, y: %u, width: %u, height: %u ]\r\n",
                bb.label,
                bb.value,
                bb.x,
                bb.y,
                bb.width,
                bb.height);
    }
#else
    ei_printf("Predictions (DSP: %d ms., Classification: %d ms., Anomaly: %d ms.): \n",
                result.timing.dsp, result.timing.classification, result.timing.anomaly);
    
    ei_printf("Predictions:\r\n");
    for (uint16_t i = 0; i < EI_CLASSIFIER_LABEL_COUNT; i++) {
        ei_printf("  %s: %.5f\r\n", 
                 ei_classifier_inferencing_categories[i], 
                 result.classification[i].value);
        
        detections[detectionCount].label = String(ei_classifier_inferencing_categories[i]);
        detections[detectionCount].confidence = result.classification[i].value;
        detectionCount++;
        
        if (result.classification[i].value > maxConfidence) {
            maxConfidence = result.classification[i].value;
        }
    }
#endif

    // Sort detections by confidence
    sortDetections(detections, detectionCount);

    // Send to server if enough time has passed
    unsigned long currentTime = millis();
    if (currentTime - lastServerUpdate >= serverUpdateInterval) {
        lastServerUpdate = currentTime;
        
        // Check if we have any valid detections
        if (detectionCount > 0 && maxConfidence > 0.1f) {
            sendDetectionToServer(detections, detectionCount);
            
        } else {
            // Send "none" detection
            Detection noneDetection[1];
            noneDetection[0].label = "none";
            noneDetection[0].confidence = 0.0f;
            sendDetectionToServer(noneDetection, 1);
            Serial.println("📷 No objects detected with sufficient confidence");
        }
        
        // Print top 3 detections
        Serial.println("📊 Top 3 detections:");
        for (int i = 0; i < detectionCount && i < 3; i++) {
            Serial.printf("   %d. %s: %.2f%%\n", 
                         i+1, 
                         detections[i].label.c_str(), 
                         detections[i].confidence * 100);
        }
        Serial.println();
    }

    // Print anomaly result (if it exists)
#if EI_CLASSIFIER_HAS_ANOMALY
    ei_printf("Anomaly prediction: %.3f\r\n", result.anomaly);
#endif

    free(snapshot_buf);
}

/**
 * @brief   Setup image sensor & start streaming
 */
bool ei_camera_init(void) {

    if (is_initialised) return true;

    //initialize the camera
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
      Serial.printf("Camera init failed with error 0x%x\n", err);
      return false;
    }

    sensor_t * s = esp_camera_sensor_get();
    // initial sensors are flipped vertically and colors are a bit saturated
    if (s->id.PID == OV3660_PID) {
      s->set_vflip(s, 1);
      s->set_brightness(s, 1);
      s->set_saturation(s, 0);
    }

    is_initialised = true;
    return true;
}

/**
 * @brief      Stop streaming of sensor data
 */
void ei_camera_deinit(void) {
    esp_err_t err = esp_camera_deinit();
    if (err != ESP_OK) {
        ei_printf("Camera deinit failed\n");
        return;
    }
    is_initialised = false;
}

/**
 * @brief      Capture, rescale and crop image
 */
bool ei_camera_capture(uint32_t img_width, uint32_t img_height, uint8_t *out_buf) {
    bool do_resize = false;

    if (!is_initialised) {
        ei_printf("ERR: Camera is not initialized\r\n");
        return false;
    }

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb) {
        ei_printf("Camera capture failed\n");
        return false;
    }

   bool converted = fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, snapshot_buf);

   esp_camera_fb_return(fb);

   if(!converted){
       ei_printf("Conversion failed\n");
       return false;
   }

    if ((img_width != EI_CAMERA_RAW_FRAME_BUFFER_COLS)
        || (img_height != EI_CAMERA_RAW_FRAME_BUFFER_ROWS)) {
        do_resize = true;
    }

    if (do_resize) {
        ei::image::processing::crop_and_interpolate_rgb888(
        out_buf,
        EI_CAMERA_RAW_FRAME_BUFFER_COLS,
        EI_CAMERA_RAW_FRAME_BUFFER_ROWS,
        out_buf,
        img_width,
        img_height);
    }

    return true;
}

/**
 * @brief      Get data from camera for inference
 */
static int ei_camera_get_data(size_t offset, size_t length, float *out_ptr)
{
    // we already have a RGB888 buffer, so recalculate offset into pixel index
    size_t pixel_ix = offset * 3;
    size_t pixels_left = length;
    size_t out_ptr_ix = 0;

    while (pixels_left != 0) {
        // Swap BGR to RGB here
        // due to https://github.com/espressif/esp32-camera/issues/379
        out_ptr[out_ptr_ix] = (snapshot_buf[pixel_ix + 2] << 16) + (snapshot_buf[pixel_ix + 1] << 8) + snapshot_buf[pixel_ix];

        // go to the next pixel
        out_ptr_ix++;
        pixel_ix+=3;
        pixels_left--;
    }
    // and done!
    return 0;
}

#if !defined(EI_CLASSIFIER_SENSOR) || EI_CLASSIFIER_SENSOR != EI_CLASSIFIER_SENSOR_CAMERA
#error "Invalid model for current sensor"
#endif
