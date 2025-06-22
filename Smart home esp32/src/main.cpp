#define ERA_DEBUG  
#define DEFAULT_MQTT_HOST "mqtt1.eoh.io"  
#define ERA_AUTH_TOKEN "7262ae7f-45c0-4eb6-a0ae-762ed8ce2004"  
#include <Wire.h>
#include <EEPROM.h>
#include <Arduino.h>  
#include <ERa.hpp>  
#include <ERa/ERaTimer.hpp>  

//HardwareSerial MySerial1(1);
ERaString FromStr;
WebServer server(80);  // WebServer setup
String ssid = "";
String password = "";
const char* ap_ssid = "ESP32_Setup"; // Tên AP

        // Danh sách các lệnh relay
        String relayCommands[] = {"R1ON", "R1OFF", "R2ON", "R2OFF", "R3ON", "R3OFF", "R4ON", "R4OFF", 
            "R5ON", "R5OFF", "R6ON", "R6OFF", "R7ON", "R7OFF", "R8ON", "R8OFF", 
            "R9ON", "R9OFF"};

// Danh sách các lệnh chuyển tiếp
String FowardCommands[] = {"ALARMenable", "ALARMdisnable", "RESET", "OPENdoor", "CLOSEdoor"};

#define RX1_PIN 4  // Chân RX mới cho Serial1
#define TX1_PIN 5  // Chân TX mới cho Serial1
#define RX_PIN 16  
#define TX_PIN 17  
#define NUM_BUTTONS 9 
const int VIRTUAL_PIN_BASE = 13; 
ERaTimer timer;    
const int buttonPins[NUM_BUTTONS] = {34, 35, 32, 33, 25, 26, 27, 12, 13}; 
bool lastButtonStates[NUM_BUTTONS] = {HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH, HIGH}; 
bool relayState[NUM_BUTTONS] = {false, false, false, false, false, false, false, false, false};   
bool buttonPressed[NUM_BUTTONS] = {false, false, false, false, false, false, false, false, false};   
int doorState = false;
unsigned long lastIntrusionTime = 0; 
bool intrusionActive = false;     
bool alertMode = false;         
unsigned long lastMotionTime = 0; 
bool isMotionSafe = true; 
bool fireDetected = false;       
unsigned long lastUpdateMillis = 0;
const unsigned long updateInterval = 10000; 
bool onlineMode = false;  
unsigned long lastFireDetectedTime = 0;

bool eraInitialized = false;
void timerEvent() {  
    ERA_LOG("Timer", "Uptime: %d", ERaMillis() / 1000L);  
}  

ERA_WRITE(V13) {  
    int value = param.getInt();  
    relayState[0] = (value == 1);  
    Serial2.println(value == 1 ? "ON1" : "OFF1");  
}  

ERA_WRITE(V14) {  
    int value = param.getInt();  
    relayState[1] = (value == 1);  
    Serial2.println(value == 1 ? "ON2" : "OFF2");  
}  

ERA_WRITE(V15) {  
    int value = param.getInt();  
    relayState[2] = (value == 1);  
    Serial2.println(value == 1 ? "ON3" : "OFF3");  
}  

ERA_WRITE(V16) {  
    int value = param.getInt();  
    relayState[3] = (value == 1);  
    Serial2.println(value == 1 ? "ON4" : "OFF4");  
}  

ERA_WRITE(V17) {  
    int value = param.getInt();  
    relayState[4] = (value == 1);  
    Serial2.println(value == 1 ? "ON5" : "OFF5");  
}  
ERA_WRITE(V18) {  
    int value = param.getInt();  
    relayState[5] = (value == 1);  
    Serial2.println(value == 1 ? "ON6" : "OFF6");  
}  
ERA_WRITE(V19) {  
    int value = param.getInt();  
    relayState[6] = (value == 1);  
    Serial2.println(value == 1 ? "ON7" : "OFF7");  
}  
ERA_WRITE(V20) {  
    int value = param.getInt();  
    relayState[7] = (value == 1);  
    Serial2.println(value == 1 ? "ON8" : "OFF8");  
}  
ERA_WRITE(V21) {  
    int value = param.getInt();  
    relayState[8] = (value == 1);  
    Serial2.println(value == 1 ? "ON9" : "OFF9");  
}  
ERA_WRITE(V22) {  
    int value = param.getInt();  
    doorState = (value == 1);  
    Serial2.println(value == 1 ? "OPEN" : "CLOSE");  
}  
ERA_WRITE(V27) {
    uint8_t value = param.getInt(); 
    if (value == 1) {
        alertMode = true; 
        Serial2.println("ENABLE_ALERT");
        Serial.println("Chế độ cảnh báo: BẬT.");
    } else {
        alertMode = false; 
        Serial2.println("DISABLE_ALERT");
        ERa.virtualWrite(V28, 0);        
        Serial.println("Chế độ cảnh báo: TẮT.");
    }
}
void saveWiFiConfig() {
    for (int i = 0; i < 96; ++i) {
        EEPROM.write(i, 0);
    }
    for (int i = 0; i < ssid.length(); ++i) {
        EEPROM.write(i, ssid[i]);
    }
    for (int i = 0; i < password.length(); ++i) {
        EEPROM.write(32 + i, password[i]);
    }
    EEPROM.commit();
}
void readWiFiConfig() {
    ssid = "";
    for (int i = 0; i < 32; ++i) {
        char c = EEPROM.read(i);
        if (c != 0) ssid += c;
    }
    password = "";
    for (int i = 32; i < 96; ++i) {
        char c = EEPROM.read(i);
        if (c != 0) password += c;
    }
    
}
void handleReset() {
    for (int i = 0; i < 512; ++i) {
        EEPROM.write(i, 0);
    }
    EEPROM.commit();
    String response = "<!DOCTYPE html><html><head>";
    response += "<meta charset='UTF-8'>";
    response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    response += "<style>";
    response += "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@400;500&display=swap');";
    response += "body{font-family:'Roboto',Arial,sans-serif;margin:20px;text-align:center;}";
    response += "</style>";
    response += "</head><body>";
    response += "<h3>Đã xóa tất cả cấu hình WiFi!</h3>";
    response += "<p>ESP32 sẽ khởi động lại sau 5 giây...</p>";
    response += "</body></html>";
    server.send(200, "text/html", response);
    delay(5000);
    ESP.restart();
}
String scanNetworks() {
    String result = "";
    int n = WiFi.scanNetworks();
    for (int i = 0; i < n; ++i) {
        result += "<option value='" + WiFi.SSID(i) + "'>";
        result += WiFi.SSID(i);
        result += " (";
        result += WiFi.RSSI(i);
        result += " dBm)";
        result += "</option>";
        delay(10);
    }
    return result;
}
void handleRoot() {
    String networks = scanNetworks();
    String currentSSID = WiFi.SSID();
    String connectionStatus = "";
    if (WiFi.status() == WL_CONNECTED) {
        connectionStatus = "Đang kết nối tới: " + currentSSID + " (IP: " + WiFi.localIP().toString() + ")";
        
    } else {
        connectionStatus = "Chưa kết nối WiFi";
    }
    String html = "<!DOCTYPE html>";
    html += "<html>";
    html += "<head>";
    html += "<meta charset='UTF-8'>";
    html += "<title>WiFi Setup</title>";
    html += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
    html += "<style>";
    html += "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@400;500&display=swap');";
    html += ".container { max-width: 400px; margin: 0 auto; background: white; padding: 20px; border-radius: 8px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }";
    html += "h2 { color: #333; text-align: center; margin-bottom: 20px; }";
    html += "select, input { width: 100%; padding: 10px; margin: 10px 0; border: 1px solid #ddd; border-radius: 4px; box-sizing: border-box; }";
    html += "button { width: 100%; padding: 12px; background: #4CAF50; color: white; border: none; border-radius: 4px; cursor: pointer; margin-top: 10px; font-weight: 500; }";
    html += "button:hover { background: #45a049; }";
    html += "button.delete { background: #dc3545; }";
    html += "button.delete:hover { background: #c82333; }";
    html += "button.scan { background: #2196F3; }";
    html += "button.scan:hover { background: #1976D2; }";
    html += ".status { text-align: center; margin: 15px 0; padding: 10px; background: #e8f5e9; border-radius: 4px; }";
    html += ".connection-info { margin-bottom: 20px; padding: 10px; background: #e3f2fd; border-radius: 4px; text-align: center; }";
    html += ".modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); }";
    html += ".modal-content { background: white; padding: 20px; border-radius: 8px; max-width: 300px; margin: 100px auto; text-align: center; }";
    html += ".modal-buttons { display: flex; justify-content: space-between; margin-top: 20px; }";
    html += ".modal-buttons button { width: 45%; margin: 0; }";
    html += "</style>";
    html += "<script>";
    html += "function showConfirmReset() {";
    html += "  document.getElementById('resetModal').style.display = 'block';";
    html += "}";
    html += "function hideModal() {";
    html += "  document.getElementById('resetModal').style.display = 'none';";
    html += "}";
    html += "</script>";
    html += "</head>";
    html += "<body>";
    html += "<div class='container'>";
    html += "<h2>Cấu hình WiFi</h2>";
    html += "<div class='connection-info'>" + connectionStatus + "</div>";
    html += "<form action='/save' method='POST'>";
    html += "<select name='ssid' required>";
    html += "<option value=''>Chọn mạng WiFi</option>";
    html += networks;
    html += "</select>";
    html += "<input type='password' name='password' placeholder='Mật khẩu WiFi' required>";
    html += "<button type='submit'>Kết nối</button>";
    html += "</form>";
    html += "<div class='status'>";
    html += "AP: " + String(ap_ssid) + " (IP: " + WiFi.softAPIP().toString() + ")";
    html += "</div>";
    html += "<button class='scan' onclick='window.location.reload()'>Quét lại WiFi</button>";
    html += "<button class='delete' onclick='showConfirmReset()'>Xóa cấu hình WiFi</button>";
    html += "</div>";
    html += "<div id='resetModal' class='modal'>";
    html += "<div class='modal-content'>";
    html += "<h3>Xác nhận xóa</h3>";
    html += "<p>Bạn có chắc chắn muốn xóa tất cả cấu hình WiFi đã lưu?</p>";
    html += "<div class='modal-buttons'>";
    html += "<button onclick='hideModal()'>Hủy</button>";
    html += "<button class='delete' onclick='window.location=\"/reset\"'>Xóa</button>";
    html += "</div>";
    html += "</div>";
    html += "</div>";
    html += "</body></html>";
    server.send(200, "text/html", html);
    
}
void handleSave() {
    if (server.hasArg("ssid") && server.hasArg("password")) {
        ssid = server.arg("ssid");
        password = server.arg("password");
        saveWiFiConfig();
        String response = "<!DOCTYPE html><html><head>";
        response += "<meta charset='UTF-8'>";
        response += "<meta name='viewport' content='width=device-width, initial-scale=1'>";
        response += "<style>";
        response += "@import url('https://fonts.googleapis.com/css2?family=Roboto:wght@400;500&display=swap');";
        response += "body{font-family:'Roboto',Arial,sans-serif;margin:20px;text-align:center;}";
        response += "</style>";
        response += "</head><body>";
        response += "<h3>Đã lưu cấu hình WiFi!</h3>";
        response += "<p>SSID: " + ssid + "</p>";
        response += "<p>ESP32 sẽ khởi động lại sau 5 giây...</p>";
        response += "</body></html>";
        
        server.send(200, "text/html", response);
        delay(5000);
        ESP.restart();
    }
}

void readVoltageData() {
    if (Serial2.available()) {
        String data = Serial2.readStringUntil('\n'); 
        data.trim(); 
        if (data.startsWith("VOLTAGE:")) {
            String voltageValue = data.substring(8); 
            float voltage = voltageValue.toFloat(); 
            ERa.virtualWrite(V23, voltage); 
            //Serial.println("Gửi giá trị điện áp lên V23: " + String(voltage));
        }
    }
}
void handleForwardCommands(const String& command) {
    if (command == "ALARMenable") {
        ERa.virtualWrite(V27, 1); // Bật chế độ cảnh báo
        Serial2.println("ENABLE_ALERT");
        Serial.println("Chế độ cảnh báo: BẬT.");
    } else if (command == "ALARMdisnable") {
        ERa.virtualWrite(V27, 0); // Tắt chế độ cảnh báo
        Serial.println("Chế độ cảnh báo: TẮT.");
        Serial2.println("DISABLE_ALERT");
    } else if (command == "RESET") {
        Serial.println("Nhận lệnh RESET - Đặt lại hệ thống.");
        ESP.restart(); // Khởi động lại ESP32
    } else if (command == "OPENdoor") {
        ERa.virtualWrite(V22, 1); // Mở cửa
        Serial2.println("OPEN");
        Serial.println("Lệnh mở cửa: Gửi tín hiệu 1 lên V22.");
    } else if (command == "CLOSEdoor") {
        Serial2.println("CLOSE");
        ERa.virtualWrite(V22, 0); // Đóng cửa
        Serial.println("Lệnh đóng cửa: Gửi tín hiệu 0 lên V22.");
    }
}

void handleRelayCommands(const String& command) {
    // Kiểm tra lệnh bật relay
    if (command.endsWith("ON")) {
        int relayNumber = command.substring(1, command.length() - 2).toInt(); // Lấy số relay từ lệnh
        if (relayNumber >= 1 && relayNumber <= 9) {
            relayState[relayNumber - 1] = true; // Bật relay
            ERa.virtualWrite(VIRTUAL_PIN_BASE + relayNumber - 1, 1); // Cập nhật virtual pin
            Serial.printf("Relay %d ON - Updated virtual pin V%d\n", relayNumber, VIRTUAL_PIN_BASE + relayNumber - 1);

            // Gửi tín hiệu điều khiển qua Serial2
            Serial2.println("ON" + String(relayNumber));
        } else {
            Serial.println("Lệnh không hợp lệ: " + command);
        }
    }
    // Kiểm tra lệnh tắt relay
    else if (command.endsWith("OFF")) {
        int relayNumber = command.substring(1, command.length() - 3).toInt(); // Lấy số relay từ lệnh
        if (relayNumber >= 1 && relayNumber <= 9) {
            relayState[relayNumber - 1] = false; // Tắt relay
            ERa.virtualWrite(VIRTUAL_PIN_BASE + relayNumber - 1, 0); // Cập nhật virtual pin
            Serial.printf("Relay %d OFF - Updated virtual pin V%d\n", relayNumber, VIRTUAL_PIN_BASE + relayNumber - 1);

            // Gửi tín hiệu điều khiển qua Serial2
            Serial2.println("OFF" + String(relayNumber));
        } else {
            Serial.println("Lệnh không hợp lệ: " + command);
        }
    } else {
        Serial.println("Lệnh không xác định: " + command);
    }
}

// ======================================================== VOID SETUP ===================================================
void setup() {  
    Serial.begin(115200);
    Serial1.begin(115200, SERIAL_8N1, RX1_PIN, TX1_PIN);
    Serial2.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);  
    EEPROM.begin(512);
    readWiFiConfig();
    WiFi.mode(WIFI_MODE_APSTA);
    WiFi.softAP(ap_ssid);
    Serial.print("AP IP address: ");
    Serial.println(WiFi.softAPIP());
    if (ssid.length() > 0) {
        WiFi.begin(ssid.c_str(), password.c_str());
        int timeout = 0;
        while (WiFi.status() != WL_CONNECTED && timeout < 30) {
            delay(1000);
            Serial.print(".");
            timeout++;
        }
        if (WiFi.status() == WL_CONNECTED) {
            Serial.println("\nĐã kết nối WiFi");
            
            Serial.print("STA IP: ");
            Serial.println(WiFi.localIP());
            eraInitialized = true;
            //ERa.begin();
            ERa.begin();
        } else {
            Serial.println("\nKhông thể kết nối WiFi");
        }
    }
    server.on("/", HTTP_GET, handleRoot);
    server.on("/save", HTTP_POST, handleSave);
    server.on("/reset", HTTP_GET, handleReset);
    server.begin();
    Serial.println("Web server started");
    timer.setInterval(1000L, timerEvent);  


    for (int i = 0; i < NUM_BUTTONS; i++) {  
        pinMode(buttonPins[i], INPUT_PULLUP); 
    }
    

    ERa.virtualWrite(V13, 0);
    ERa.virtualWrite(V14, 0);
    ERa.virtualWrite(V15, 0);
    ERa.virtualWrite(V16, 0);
    ERa.virtualWrite(V17, 0);
    ERa.virtualWrite(V18, 0);
    ERa.virtualWrite(V19, 0);
    ERa.virtualWrite(V20, 0);
    ERa.virtualWrite(V21, 0);
  
}


// ======================================================== VOID LOOP ====================================================
void loop() {
    for (int i = 0; i < NUM_BUTTONS; i++) {
        int buttonState = digitalRead(buttonPins[i]);
        if (buttonState == LOW && lastButtonStates[i] == HIGH && !buttonPressed[i]) {
            relayState[i] = !relayState[i];
            Serial2.println(relayState[i] ? "ON" + String(i + 1) : "OFF" + String(i + 1));
            ERa.virtualWrite(V13 + i, relayState[i] ? 1 : 0); 
            buttonPressed[i] = true; 
        }
        if (buttonState == HIGH) {
            buttonPressed[i] = false;
        }
        lastButtonStates[i] = buttonState;
    }
    if (Serial2.available()) {
        String data = Serial2.readStringUntil('\n');
        data.trim(); 
        //Serial.println("Dữ liệu nhận được: " + data); 
        if (data.toInt() > 0) { 
            int gasValue = data.toInt();
            ERa.virtualWrite(V30, gasValue);
            //Serial.println("Giá trị khí gas đã gửi lên V30: " + String(gasValue));
        }
        if (data == "OPEN") {
            Serial.println("Nhận lệnh: OPEN - Gửi tín hiệu 1 lên ERa");
            ERa.virtualWrite(V22, 1);
        } else if (data == "CLOSE") {
            Serial.println("Nhận lệnh: CLOSE - Gửi tín hiệu 0 lên ERa");
            ERa.virtualWrite(V22, 0); 
        } else if (data == "ALERT: INTRUSION DETECTED") {
            ERa.virtualWrite(V28, 1); 
            lastMotionTime = millis();
            Serial.println("Phát hiện đột nhập: Báo động gửi lên V28.");
        } else if (data == "FIRE_DETECTED") {
            fireDetected = true;
            lastFireDetectedTime = millis(); // Cập nhật thời gian nhận tín hiệu
            ERa.virtualWrite(V29, 1);
            Serial.println("CẢNH BÁO: Phát hiện cháy! Gửi tín hiệu lên V29.");
        } else if (data.startsWith("AHT20 Channel")) {
            int channelIndex = data.indexOf("Channel ") + 8;
            int channelNumber = data.substring(channelIndex, data.indexOf(" - Temp")).toInt();
            int tempIndex = data.indexOf("Temp: ") + 6;
            float temperature = atof(data.substring(tempIndex, data.indexOf(",", tempIndex)).c_str());
            int humidityIndex = data.indexOf("Humidity: ") + 10;
            float humidity = atof(data.substring(humidityIndex).c_str());
            switch (channelNumber) {
                case 0:
                    ERa.virtualWrite(V3, temperature);
                    ERa.virtualWrite(V4, humidity);
                    break;
                case 1:
                    ERa.virtualWrite(V5, temperature);
                    ERa.virtualWrite(V6, humidity);
                    break;
                case 2:
                    ERa.virtualWrite(V7, temperature);
                    ERa.virtualWrite(V8, humidity);
                    break;
                case 3:
                    ERa.virtualWrite(V9, temperature);
                    ERa.virtualWrite(V10, humidity);
                    break;
                case 4:
                    ERa.virtualWrite(V11, temperature);
                    ERa.virtualWrite(V12, humidity);
                    break;
                default:
                    Serial.println("Chỉ số cảm biến không hợp lệ.");
                    break;
            }
            //Serial.printf("Cảm biến %d - Nhiệt độ: %.2f C, Độ ẩm: %.2f %%\n", channelNumber + 1, temperature, humidity);
        } else if (data.startsWith("TVOC:")) {
            int tvocValue = data.substring(5).toInt();
            ERa.virtualWrite(V26, tvocValue);
            //Serial.println("Gửi giá trị TVOC lên V26: " + String(tvocValue));
        }

        // Xử lý dữ liệu AQI
        else if (data.startsWith("AQI:")) {
            int aqiValue = data.substring(4).toInt();
            ERa.virtualWrite(V24, aqiValue);
            //Serial.println("Gửi giá trị AQI lên V24: " + String(aqiValue));
        }
        // Xử lý dữ liệu eCO2
        else if (data.startsWith("eCO2:")) {
            int eco2Value = data.substring(5).toInt();
            ERa.virtualWrite(V25, eco2Value);
            //Serial.println("Gửi giá trị eCO2 lên V25: " + String(eco2Value));
        }
    }
        // Kiểm tra nếu không nhận tín hiệu FIRE_DETECTED trong 20 giây
        if (fireDetected && millis() - lastFireDetectedTime > 15000) { // 20 giây
            fireDetected = false; // Đặt lại trạng thái cháy
            ERa.virtualWrite(V29, 0); // Đặt VirtualPin29 về 0
            Serial.println("Không phát hiện cháy trong 15 giây, đặt V29 về 0.");
        }
      readVoltageData();
  unsigned long currentMillis = millis();
    if (alertMode && currentMillis - lastMotionTime > 10000) {
        if (currentMillis - lastUpdateMillis > updateInterval) {
            lastUpdateMillis = currentMillis;
            ERa.virtualWrite(V28, 0);
            Serial.println("Không có chuyển động mới, đặt V28 về 0.");
        }
    }
    if (!fireDetected) {
        ERa.virtualWrite(V29, 0);
    }
    server.handleClient();
    //if (eraInitialized) {
      //  ERa.run();
    //}
    // ===================================================Serial 1 =============================
    if (Serial1.available()) {
        String command = Serial1.readStringUntil('\n');
        command.trim(); 
        Serial.println("Nhận lệnh từ Serial1: " + command);

        // Kiểm tra và xử lý lệnh relay
        for (int i = 0; i < sizeof(relayCommands) / sizeof(relayCommands[0]); i++) {
            if (command == relayCommands[i]) {
                handleRelayCommands(command); // Xử lý lệnh relay
                return;
            }
        }

        // Kiểm tra và xử lý lệnh chuyển tiếp
        for (int i = 0; i < sizeof(FowardCommands) / sizeof(FowardCommands[0]); i++) {
            if (command == FowardCommands[i]) {
                handleForwardCommands(command); // Xử lý lệnh chuyển tiếp
                return;
            }
        }

        // Lệnh không xác định
        Serial.println("Lệnh không xác định: " + command);
    }
    ERa.run();
    timer.run();
}