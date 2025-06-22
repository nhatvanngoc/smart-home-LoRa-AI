
// Ê, đây là lora có chức năng ch
#include <Arduino.h>
#include <SPI.h>
#include <LoRa.h>

// Định nghĩa chân kết nối LoRa
#define LORA_SCK 18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_CS 5
#define LORA_RST 14
#define LORA_IRQ 26

// Định nghĩa chân LED
#define LED_RED 27
#define LED_BLUE 12

// Biến toàn cục
String relayCommands[] = {"R1ON", "R1OFF", "R2ON", "R2OFF", "R3ON", "R3OFF", "R4ON", "R4OFF", 
                         "R5ON", "R5OFF", "R6ON", "R6OFF", "R7ON", "R7OFF", "R8ON", "R8OFF", 
                         "R9ON", "R9OFF"};
String FowardCommands[] = {"ALARMenable", "ALARMdisnable", "RESET", "OPENdoor", "CLOSEdoor"};

bool isConnected = false;
unsigned long lastCheckTime = 0;
unsigned long lastSyncTime = 0;
const long checkInterval = 7000; // Kiểm tra kết nối mỗi 7 giây
const long syncInterval = 2000; // Đồng bộ mỗi 2

bool isValidCommand(String command) {
  for (String validCommand : relayCommands) {
    if (command == validCommand) return true;
  }
  for (String validCommand : FowardCommands) {
    if (command == validCommand) return true;
  }
  return false;
}

void sendLoRaMessage(String message) {
  LoRa.beginPacket();
  LoRa.print(message);
  LoRa.endPacket();

  // Chờ ACK
  unsigned long startTime = millis();
  bool ackReceived = false;
  while (millis() - startTime < 1000) { // Timeout 1 giây
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String received = "";
      while (LoRa.available()) {
        received += (char)LoRa.read();
      }
      if (received == "ACK") {
        ackReceived = true;
        break;
      }
    }
  }

  if (ackReceived) {
    Serial.println("Gửi thành công: " + message);
    isConnected = true;
    digitalWrite(LED_BLUE, HIGH);
    digitalWrite(LED_RED, LOW);
  } else {
    Serial.println("Gửi thất bại: " + message);
    isConnected = false;
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_RED, HIGH);
  }
}
void checkConnection() {
  LoRa.beginPacket();
  LoRa.print("PING");
  LoRa.endPacket();

  unsigned long startTime = millis();
  bool ackReceived = false;
  while (millis() - startTime < 1000) {
    int packetSize = LoRa.parsePacket();
    if (packetSize) {
      String received = "";
      while (LoRa.available()) {
        received += (char)LoRa.read();
      }
      if (received == "ACK") {
        ackReceived = true;
        break;
      }
    }
  }

  isConnected = ackReceived;
  digitalWrite(LED_BLUE, isConnected ? HIGH : LOW);
  digitalWrite(LED_RED, isConnected ? LOW : HIGH);
}

void sendSyncMessage() {
  LoRa.beginPacket();
  LoRa.print("SYNC");
  LoRa.endPacket();
}
// ====================================================SETUP ====================================================
void setup() {
  // Khởi tạo Serial
  Serial.begin(115200);
  while (!Serial);

  // Cấu hình chân LED
  pinMode(LED_RED, OUTPUT);
  pinMode(LED_BLUE, OUTPUT);
  digitalWrite(LED_RED, HIGH); // Mặc định ngắt kết nối
  digitalWrite(LED_BLUE, LOW);

  // Khởi tạo LoRa
  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_CS);
  LoRa.setPins(LORA_CS, LORA_RST, LORA_IRQ);

  if (!LoRa.begin(433E6)) { // Tần số 915MHz, thay đổi theo khu vực
    Serial.println("Khởi tạo LoRa thất bại!");
    while (1);
  }

  // Cấu hình LoRa
  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3); // Tần số 433MHz
  LoRa.setCodingRate4(5); // 4/5 là
  LoRa.setPreambleLength(8);
  LoRa.enableCrc();

  Serial.println("LoRa Sender khởi động!");
}
// /====================================================LOOP ====================================================
void loop() {
  unsigned long currentTime = millis();

  // Kiểm tra kết nối mỗi 7 giây
  if (currentTime - lastCheckTime >= checkInterval) {
    checkConnection();
    lastCheckTime = currentTime;
  }

  // Đồng bộ trạng thái mỗi 2 giây
  if (currentTime - lastSyncTime >= syncInterval) {
    sendSyncMessage();
    lastSyncTime = currentTime;
  }

  // Đọc dữ liệu từ Serial
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    if (isValidCommand(command)) {
      sendLoRaMessage(command);
    } else {
      Serial.println("Lệnh không hợp lệ!");
    }
  }
}

