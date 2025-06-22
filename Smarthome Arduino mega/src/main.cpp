#include <Wire.h>  
#include <LiquidCrystal_I2C.h>    
#include <Arduino.h>
#include <Adafruit_AHTX0.h>
#include <Adafruit_Sensor.h>  
#include <SoftwareSerial.h>
Adafruit_AHTX0 aht[4];  
uint8_t pca9548_address = 0x70; 
String input;  
LiquidCrystal_I2C lcd(0x27, 20, 4); 
#define BUFFER_SIZE 32
#define FIRE_ALERT_PIN 4 
unsigned long lastDebounceTime = 0;  
unsigned long lastPressTime = 0;
unsigned long debounceDelay = 200;
unsigned long previousMillis = 0;
unsigned long currentMillis = 0;
const long interval = 2000;
int activatedRelay = -1;
const int motionSensorPin = 7; // Chân cảm biến chuyển động
const int buzzerPin = 8;       // Chân điều khiển còi báo
bool isAlertActive = false;    // Trạng thái chế độ kích hoạt
bool intrusionDetected = false; // Trạng thái cảnh báo đột nhập (true nếu có chuyển động)
bool alertMode = false;      // Trạng thái chế độ cảnh báo (true: bật, false: tắt)
bool motionDetected = false; // Trạng thái phát hiện chuyển động
bool fireDetected = false;   // Trạng thái phát hiện cháy
unsigned long buzzerStartTime = 0;
bool buzzerActive = false;
bool isBuzzerActive = false;
const int BUZZER_DURATION = 5000; // 5 seconds in milliseconds
const int BUZZER_TOGGLE_INTERVAL = 100; // 100ms toggle interval
bool manualFireAlert = false;
#define FLAME_SENSOR_PIN 6
#define MQ2_PIN A0          // Chân kết nối cảm biến MQ-2
#define THRESHOLD_HIGH 700  // Ngưỡng cực kỳ nguy hiểm
#define THRESHOLD_LOW 300   // Ngưỡng xử lý
unsigned long previousMQ2Millis = 0; // Biến lưu thời gian trước đó
const unsigned long mq2ReadInterval = 2000; // Thời gian định kỳ gửi (ms)
const int relayPins[] = {9, 10, 11, 12, 13, 30, 31, 32, 33};
bool relayStates[9] = {false, false, false, false, false, false, false, false, false};
bool relayActivationStates[9] = {false, false, false, false, false, false, false, false, false};
const int numRelays = sizeof(relayPins) / sizeof(relayPins[0]);
#define AHT_SENSOR_COUNT 4
unsigned long previousReadMillis[AHT_SENSOR_COUNT];
unsigned long lastAHT20Read[AHT_SENSOR_COUNT];
const long readInterval = 3000;
unsigned long lastUpdate = 0; 
const unsigned long updateInterval = 200;  
const int pinInput = 5; // Chân số 5 dùng để nhận tín hiệu digital
bool previousState = LOW; // Lưu trạng thái trước đó của tín hiệu

String voltageData = "";
bool isReadingSerial2 = false;
unsigned long serial2Timeout = 1000;
unsigned long serial2StartTime = 0;
unsigned long lastSerialPrint = 0;  // Thời gian in Serial cuối cùng
const unsigned long serialPrintInterval = 1000;  // In mỗi 1 giây

enum Mode {
    RELAY_MODE,
};
Mode currentMode = RELAY_MODE;
void selectChannel(uint8_t channel) {  
  if (channel > 7) return; 
  Wire.beginTransmission(pca9548_address);  
  Wire.write(1 << channel);  
  Wire.endTransmission();  
}  
void setup() {  
  Serial.println("Bắt đầu void setup");
    Serial.begin(115200);    
    Serial2.begin(115200);  
    Serial1.begin(115200);
    Serial3.begin(115200);
    for (uint8_t i = 0; i < AHT_SENSOR_COUNT; i++) {  
    previousReadMillis[i] = 0;  
    pinMode(motionSensorPin, INPUT);   // Cảm biến chuyển động
    pinMode(buzzerPin, OUTPUT);        // Còi báo
    digitalWrite(buzzerPin, LOW);      // Tắt còi báo ban đầu
    pinMode(pinInput, INPUT); // Đặt chân 5 làm chân đầu vào
    pinMode(FLAME_SENSOR_PIN, INPUT);
    pinMode(FIRE_ALERT_PIN, INPUT);  // Cài đặt chân 4 làm đầu vào
    pinMode(MQ2_PIN, INPUT);       // Cảm biến MQ-2
    for (int i = 0; i < 9; i++) {  
        pinMode(relayPins[i], OUTPUT);  
        digitalWrite(relayPins[i], HIGH);
    }  
    lcd.init();  
    lcd.backlight();  
    displayRelayStatus();  
    Wire.begin();  
    while (!Serial) {}
    unsigned long currentMillis = millis();
    for (uint8_t i = 0; i < AHT_SENSOR_COUNT; i++) {
        selectChannel(i);
        if (!aht[i].begin()) {
            Serial.print("Không thể khởi động cảm biến AHT20 ở kênh ");
            Serial.println(i);
        } else {
            Serial.print("Khởi động thành công cảm biến AHT20 ở kênh ");
            Serial.println(i);
        }
    }
  Serial.println("Setup complete. Waiting for commands...");
    }
}
// ============================================================================= LOOP ======================================================
void loop() {  
    bool currentState = digitalRead(pinInput);  
    currentMillis = millis();
if (millis() - previousMQ2Millis >= mq2ReadInterval) {
    previousMQ2Millis = millis();
    int gasValue = analogRead(MQ2_PIN);
    Serial.println(gasValue);
} 
for (uint8_t i = 0; i < AHT_SENSOR_COUNT; i++) {
        if (currentMillis - previousReadMillis[i] >= readInterval) {
            previousReadMillis[i] = currentMillis;

            // Kiểm tra kênh PCA9548
            Wire.beginTransmission(pca9548_address);
            Wire.write(1 << i);
            int error = Wire.endTransmission();
            if (error != 0) {
                Serial.print("Lỗi I2C khi chọn kênh AHT20 ");
                Serial.print(i);
                Serial.print(": Mã lỗi ");
                Serial.println(error);
                continue;  // Bỏ qua cảm biến này, tiếp tục với cảm biến tiếp theo
            }

            // Đọc cảm biến AHT20 với timeout
            sensors_event_t humidity, temp;
            unsigned long startTime = millis();
            bool success = false;

            while (millis() - startTime < 500) {  // Timeout 500ms
                if (aht[i].getEvent(&humidity, &temp)) {
                    Serial.print("AHT20 Channel ");
                    Serial.print(i);
                    Serial.print(" - Temp: ");
                    Serial.print(temp.temperature);
                    Serial.print(", Humidity: ");
                    Serial.println(humidity.relative_humidity);
                    success = true;
                    break;
                }
            }

            if (!success) {
                Serial.print("Lỗi khi đọc AHT20 ở kênh ");
                Serial.println(i);
                // Reset I2C để tránh treo
                Wire.begin();
            }
        }
    }
    if (Serial1.available()) {  
        String data = Serial1.readStringUntil('\n');  
        data.trim(); 
        //Serial.println("Dữ liệu từ ENS160 (Nano): " + data);
        if (data.startsWith("AQI:")) {
            int aqiStart = 4;
            int tvocStart = data.indexOf(",TVOC:") + 6;
            int eco2Start = data.indexOf(",eCO2:") + 6;
            int aqi = data.substring(aqiStart, data.indexOf(",", aqiStart)).toInt();
            int tvoc = data.substring(tvocStart, data.indexOf(",", tvocStart)).toInt();
            int eco2 = data.substring(eco2Start).toInt();
            Serial.println("AQI: " + String(aqi));
            Serial.println("TVOC: " + String(tvoc));
            Serial.println("eCO2: " + String(eco2));
        } else {
            Serial.println("Dữ liệu không đúng định dạng!");
        }
    }

    if (Serial.available()) {  
        String command = Serial.readStringUntil('\n');
        command.trim();  
        Serial.print(command);  
        if (command == "ENABLE_ALERT") {
            alertMode = true;
            Serial.println("Chế độ cảnh báo: BẬT");
        } else if (command == "DISABLE_ALERT") {
            alertMode = false;
            digitalWrite(buzzerPin, LOW);
            Serial.println("Chế độ cảnh báo: TẮT");
        }

        for (int i = 0; i < 9; i++) {  
            String onCommand = "ON" + String(i + 1);  
            String offCommand = "OFF" + String(i + 1);  
            if (command == onCommand) {  
                digitalWrite(relayPins[i], HIGH);  
                relayStates[i] = true;  
            } else if (command == offCommand) {  
                digitalWrite(relayPins[i], LOW);  
                relayStates[i] = false;  
            }  
        }  
        if (command.startsWith("ON") || command.startsWith("OFF")) {  
        } else {  
            Serial.print("Invalid command received: '");  
            Serial.println(command);
        }  
        if (currentMode == RELAY_MODE) {
            displayRelayStatus();
        }
        Serial3.println(command);
        if (command == '1') {
            manualFireAlert = true;
            buzzerStartTime = millis();
            buzzerActive = true;
            digitalWrite(buzzerPin, HIGH);
            Serial.println("CẢNH BÁO: Kích hoạt cảnh báo cháy từ máy tính!");
        } else if (command == '0') {
            manualFireAlert = false;
            if (!fireDetected) { // Chỉ tắt còi nếu không có cháy
                buzzerActive = false;
                digitalWrite(buzzerPin, LOW);
                Serial.println("Tắt cảnh báo cháy (từ máy tính).");
            }
        }

    }
    if (Serial3.available()) {
        String command = Serial3.readStringUntil('\n');
        command.trim();
        if (command == "CLOSE" || command == "OPEN") {
            Serial.println(command);
            //Serial.println("Đã chuyển tiếp lệnh sang ESP32: " + command);
        } else {
            Serial.println("Lệnh không hợp lệ: " + command);
        }
    }

    if (currentState != previousState) {
        if (currentState == HIGH) {
            Serial3.println("OPEN");
        } else {
            Serial3.println("CLOSE"); 
        }
        previousState = currentState;
    }

    if (Serial.available()) {
        String response = Serial.readStringUntil('\n'); 
        Serial.println("Phản hồi từ ESP32: " + response); 
    }

    if (Serial2.available()) {
        String voltageData = Serial2.readStringUntil('\n'); 
        voltageData.trim(); 
        //Serial.println("Nhận giá trị từ Arduino: " + voltageData);
        if (voltageData.length() > 0) {
            Serial.println("VOLTAGE:" + voltageData);
        }
    }

    int gasValue = analogRead(MQ2_PIN); 
    if (gasValue >= THRESHOLD_HIGH) {
        digitalWrite(buzzerPin, HIGH); 
        Serial.println("CẢNH BÁO: Mức khí gas cực kỳ nguy hiểm!");
    } else if (gasValue >= THRESHOLD_LOW) {
        digitalWrite(buzzerPin, LOW);  
        Serial.println("Cảnh báo: Mức khí gas cao!");
    } else {
        digitalWrite(buzzerPin, LOW);  
    }



        bool flameDetected = (digitalRead(FLAME_SENSOR_PIN) == LOW);
        // Kiểm tra tín hiệu thủ công từ FIRE_ALERT_PIN
        bool fireAlertTriggered = (digitalRead(FIRE_ALERT_PIN) == HIGH);

        // Kích hoạt cảnh báo nếu một trong hai điều kiện xảy ra
        fireDetected = flameDetected || fireAlertTriggered;
        if (fireDetected) {
            buzzerStartTime = currentMillis;
            buzzerActive = true;
            digitalWrite(buzzerPin, HIGH);
            if (flameDetected) {
                Serial.println("CẢNH BÁO: Phát hiện cháy từ cảm biến ngọn lửa!");
            } else if (fireAlertTriggered) {
                Serial.println("CẢNH BÁO: Phát hiện cháy từ tín hiệu thủ công (FIRE_ALERT_PIN)!");
            }
            Serial.println("FIRE_DETECTED");
            handleAlarm(fireDetected, motionDetected);
        }

    if (alertMode) {
        motionDetected = digitalRead(motionSensorPin);
        if (motionDetected) {
            Serial.println("ALERT: INTRUSION DETECTED"); // Gửi báo động đến ESP32
            digitalWrite(buzzerPin, HIGH);                 // Bật còi báo động
            Serial.println("Phát hiện chuyển động: BÁO ĐỘNG!");
            handleAlarm(fireDetected, motionDetected);
        } else {
            digitalWrite(buzzerPin, LOW); // Tắt còi nếu không có chuyển động
        }
    } else {
        motionDetected = false; // Không quan tâm chuyển động nếu chế độ tắt
    }
}
// ====================================================================== END LOOP==========================
void handleAlarm(bool flameDetected, bool motionDetected) {
    unsigned long currentMillis = millis();
    
    if ((flameDetected || motionDetected) && !isBuzzerActive) {
        buzzerStartTime = currentMillis;
        isBuzzerActive = true;
    }
    
    if (isBuzzerActive) {
        if (currentMillis - buzzerStartTime < BUZZER_DURATION) {
            // Toggle buzzer every BUZZER_TOGGLE_INTERVAL milliseconds
            if ((currentMillis - buzzerStartTime) % (2 * BUZZER_TOGGLE_INTERVAL) < BUZZER_TOGGLE_INTERVAL) {
                digitalWrite(buzzerPin, HIGH);
            } else {
                digitalWrite(buzzerPin, LOW);
            }
        } else {
            isBuzzerActive = false;
            digitalWrite(buzzerPin, LOW);
        }
    }
}

void toggleRelay(int index) {
    relayStates[index] = !relayStates[index];
    digitalWrite(relayPins[index], relayStates[index] ? HIGH : LOW);
    if (relayStates[index]) {
        Serial.println("ON" + String(index + 1)); // Gửi lệnh ON
    } else {
        Serial.println("OFF" + String(index + 1)); // Gửi lệnh OFF
    }
    if (currentMode == RELAY_MODE) {
        displayRelayStatus();
    }
}

void displayRelayStatus() {  
    lcd.clear(); 
    lcd.setCursor(0, 0);
    lcd.print("Relay Status:");
    for (int i = 0; i < 9; i++) {   
        lcd.setCursor((i % 3) * 6, (i / 3) + 1);
        lcd.print("R");  
        lcd.print(i + 1); 
        lcd.print(": "); 
        lcd.print(relayStates[i] ? "1" : "0");
    }  
}