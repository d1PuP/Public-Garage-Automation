#include <SPI.h>
#include <MFRC522.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>

// ========== PIN ==========
#define SS_PIN 5
#define RST_PIN 4
#define SERVOIn_PIN 26
#define SERVOOut_PIN 13
#define IrGateInClose_PIN 35
#define IrGateOutOpen_PIN 34
#define IrGateOutClose_PIN 36

// ========== WIFI ==========
const char* ssid = "JANDA PIRANG";
const char* password = "nadifsr123";

// ========== Google Script ==========
const char* host = "script.google.com";
const int httpsPort = 443;
const String GAS_ID = "AKfycbx1yJsLz-bOZJHKTg5DBZmAHIS-OzUWv5u_t-kOuLjUZilzuWbmhz4bi2eAQLWao80";
WiFiClientSecure client;

// ========== Telegram ==========
const String BOT_TOKEN = "8253789002:AAG6bKepNNI8WO67WfnDv-afSSoseFh1i50";
const String CHAT_ID = "6370559366";
unsigned long lastTelegramCheck = 0;
const unsigned long telegramInterval = 3000;
String lastMessageId = "";

// ========== RFID & SERVO ==========
MFRC522 mfrc522(SS_PIN, RST_PIN);
Servo gateServoIn;
Servo gateServoOut;

// ========== USER STRUCT ==========
struct User {
  String name;
  byte uids[3][4];
  int uidCount;
};

#define MAX_USER_COUNT 10
User userListDynamic[MAX_USER_COUNT];
int currentUserCount = 3;

User userList[] = {
  { "Eka", { { 0xD6, 0x60, 0x47, 0x05 } }, 1 },
  { "fadel", { { 0xA1, 0xB2, 0xC3, 0xD4 } }, 1 },
  { "Gio", { { 0x55, 0x66, 0x77, 0x88 } }, 1 }
};

bool checkUidMode = false;

void setup() {
  Serial.begin(9600);
  SPI.begin(18, 19, 23, 5);
  mfrc522.PCD_Init();

  WiFi.begin(ssid, password);
  Serial.print(F("Menghubungkan WiFi"));
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(F("."));
  }
  Serial.println(F("\nWiFi Tersambung!"));

  client.setInsecure();

  pinMode(IrGateInClose_PIN, INPUT);
  pinMode(IrGateOutOpen_PIN, INPUT);
  pinMode(IrGateOutClose_PIN, INPUT);

  gateServoIn.setPeriodHertz(50);              // PWM untuk servo
  gateServoIn.attach(SERVOIn_PIN, 500, 2400);  // Servo masuk
  gateServoIn.write(0);                        // Posisi awal tertutup

  gateServoOut.setPeriodHertz(50);
  gateServoOut.attach(SERVOOut_PIN, 500, 2400);  // Servo keluar
  gateServoOut.write(0);                         // Posisi awal tertutup

  for (int i = 0; i < currentUserCount; i++) {
    userListDynamic[i] = userList[i];
  }

  initLastMessageId();
  Serial.println(F("Siap tempelkan kartu RFID..."));
  sendTelegramMessage(F("Sistem RFID Gerbang siap digunakan."));
}

void loop() {
  checkAccess();
  closeGateIn();
  openGateOut();
  closeGateOut();

  if (millis() - lastTelegramCheck > telegramInterval) {
    lastTelegramCheck = millis();
    handleTelegramCommand();
  }

  if (checkUidMode) checkUid();
}

// ===================== URL ENCODE =====================
String urlEncode(const String& str) {
  String encoded;
  char buf[4];
  for (size_t i = 0; i < str.length(); i++) {
    char c = str.charAt(i);
    if (isalnum(c)) encoded += c;
    else if (c == ' ') encoded += "%20";
    else if (c == '\n') encoded += "%0A";
    else {
      sprintf(buf, "%%%02X", c);
      encoded += buf;
    }
  }
  return encoded;
}

// ===================== RFID =====================
void checkAccess() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  char uidString[20] = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    sprintf(uidString + strlen(uidString), "%02X%s", mfrc522.uid.uidByte[i], (i < mfrc522.uid.size - 1) ? ":" : "");
  }

  printUID();
  String owner = getOwnerName(mfrc522.uid.uidByte);

  if (owner != "") {
    Serial.print(F("Akses Diterima untuk: "));
    Serial.println(owner);
    sendToSheet(owner, uidString);
    openGateIn();
  } else {
    Serial.println(F("Akses Ditolak"));
    denyAccess();
  }

  mfrc522.PICC_HaltA();
}

String getOwnerName(byte* uid) {
  for (int i = 0; i < currentUserCount; i++) {
    for (int j = 0; j < userListDynamic[i].uidCount; j++) {
      if (memcmp(userListDynamic[i].uids[j], uid, 4) == 0) {
        return userListDynamic[i].name;
      }
    }
  }
  return "";
}

void printUID() {
  Serial.print(F("UID Tag: "));
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    Serial.print(mfrc522.uid.uidByte[i] < 0x10 ? " 0" : " ");
    Serial.print(mfrc522.uid.uidByte[i], HEX);
  }
  Serial.println();
}

// ========== GATE CONTROL ==========
void openGateIn() {
  gateServoIn.write(90);
}

void closeGateIn() {
  int irGateInClose_Value = digitalRead(IrGateInClose_PIN);
  Serial.println(irGateInClose_Value);
  if (irGateInClose_Value == 0) {
    gateServoIn.write(0);
  }
}

void openGateOut() {
  int irGateoutOpen_Value = digitalRead(IrGateOutOpen_PIN);
  Serial.println(irGateoutOpen_Value);
  if (irGateoutOpen_Value == 0) {
    Serial.println("Gate 2 Open");
    gateServoOut.write(90);
  }
}

void closeGateOut() {
  int irGateoutClose_Value = digitalRead(IrGateOutClose_PIN);
  Serial.println(irGateoutClose_Value);
  if (irGateoutClose_Value == 0) {
    Serial.println("Gate 2 Close");
    gateServoOut.write(0);
  }
}

void denyAccess() {
  delay(3000);
}

// ========== SEND TO SHEET ==========
void sendToSheet(const String& owner, const String& uid) {
  if (!client.connect(host, httpsPort)) {
    Serial.println(F("Koneksi ke Google gagal"));
    return;
  }

  String url = "/macros/s/" + GAS_ID + "/exec?owner=" + owner + "&uid=" + uid;
  client.printf("GET %s HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", url.c_str(), host);
  while (client.connected()) {
    if (client.readStringUntil('\n') == "\r") break;
  }
  client.stop();
}

// ========== TELEGRAM ==========
void sendTelegramMessage(const String& text) {
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println(F("Gagal koneksi ke Telegram"));
    return;
  }

  String url = "/bot" + BOT_TOKEN + "/sendMessage?chat_id=" + CHAT_ID + "&text=" + urlEncode(text);
  client.printf("GET %s HTTP/1.1\r\nHost: api.telegram.org\r\nConnection: close\r\n\r\n", url.c_str());
  while (client.connected()) {
    if (client.readStringUntil('\n') == "\r") break;
  }
  client.stop();
}

// ========== HANDLE TELEGRAM ==========
void handleTelegramCommand() {
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println(F("Gagal polling Telegram"));
    return;
  }

  client.print("GET /bot" + BOT_TOKEN + "/getUpdates HTTP/1.1\r\nHost: api.telegram.org\r\nConnection: close\r\n\r\n");

  String response;
  while (client.connected()) response += client.readStringUntil('\n');
  client.stop();

  int textIndex = response.lastIndexOf("\"text\":\"");
  if (textIndex == -1) return;
  int textEndIndex = response.indexOf("\"", textIndex + 8);
  String command = response.substring(textIndex + 8, textEndIndex);

  int updateIndex = response.lastIndexOf("\"update_id\":");
  int commaIndex = response.indexOf(",", updateIndex);
  String updateId = response.substring(updateIndex + 12, commaIndex);

  if (updateId == lastMessageId) return;
  lastMessageId = updateId;

  command.toLowerCase();
  Serial.println("Perintah diterima: " + command);

  if (command == "/buka") {
    sendTelegramMessage("Gerbang dibuka via Telegram");
    openGateIn();
  } else if (command == "/tutup") {
    sendTelegramMessage("Gerbang ditutup via Telegram");
    gateServoIn.write(0);
  } else if (command == "/status") {
    sendTelegramMessage("Sistem aktif & terhubung WiFi");
  } else if (command == "/report") {
    sendToSheet("Admin", "report_request");
  } else if (command.startsWith("/addmember ")) {
    addUserFromCommand(command.substring(11));
  } else if (command == "/checkuid") {
    checkUidMode = true;
    sendTelegramMessage("Silakan tempelkan kartu ke RFID reader...");
  } else if (command == "/menu") {
    sendTelegramMessage("Command tersedia:\n/buka\n/tutup\n/status\n/report\n/addmember Nama:UID\n/checkuid\n/listmember\n/deletemember");
  } else if (command == "/listmember") {
    sendMemberList();
  } else if (command.startsWith("/deletemember ")) {
    deleteMemberFromCommand(command.substring(14));
  } else {
    sendTelegramMessage("Perintah tidak dikenali. Gunakan /menu untuk melihat perintah.");
  }
}

// ========== INISIALISASI UPDATE_ID ==========
void initLastMessageId() {
  if (!client.connect("api.telegram.org", 443)) {
    Serial.println(F("Gagal inisialisasi update_id"));
    return;
  }

  client.print("GET /bot" + BOT_TOKEN + "/getUpdates?limit=1 HTTP/1.1\r\nHost: api.telegram.org\r\nConnection: close\r\n\r\n");

  String response;
  while (client.connected()) response += client.readStringUntil('\n');
  client.stop();

  int updateIndex = response.lastIndexOf("\"update_id\":");
  if (updateIndex != -1) {
    int commaIndex = response.indexOf(",", updateIndex);
    lastMessageId = response.substring(updateIndex + 12, commaIndex);
    Serial.println("Last update_id diinisialisasi: " + lastMessageId);
  }
}

// ========== TAMBAH MEMBER ==========
void addUserFromCommand(String command) {
  if (currentUserCount >= MAX_USER_COUNT) {
    sendTelegramMessage("Kapasitas user penuh.");
    return;
  }

  int separatorIndex = command.indexOf(":");
  if (separatorIndex == -1) {
    sendTelegramMessage("Format salah. Gunakan: /addMember Nama:DE:AD:BE:EF");
    return;
  }

  String name = command.substring(0, separatorIndex);
  String uidStr = command.substring(separatorIndex + 1);
  uidStr.trim();

  byte newUid[4];
  int idx = 0, last = 0;
  while (idx < 4 && last < uidStr.length()) {
    int next = uidStr.indexOf(":", last);
    if (next == -1) next = uidStr.length();
    String part = uidStr.substring(last, next);
    newUid[idx++] = (byte)strtol(part.c_str(), NULL, 16);
    last = next + 1;
  }

  if (idx < 4) {
    sendTelegramMessage("Format UID tidak valid. Gunakan 4 byte, contoh: DE:AD:BE:EF");
    return;
  }

  User newUser;
  newUser.name = name;
  newUser.uidCount = 1;
  memcpy(newUser.uids[0], newUid, 4);
  userListDynamic[currentUserCount++] = newUser;

  sendTelegramMessage("User ditambahkan: " + name);
}

// ========== CHECK UID MODE ==========
void checkUid() {
  if (!mfrc522.PICC_IsNewCardPresent() || !mfrc522.PICC_ReadCardSerial()) return;

  char uidString[20] = "";
  for (byte i = 0; i < mfrc522.uid.size; i++) {
    sprintf(uidString + strlen(uidString), "%02X%s", mfrc522.uid.uidByte[i], (i < mfrc522.uid.size - 1) ? ":" : "");
  }

  sendTelegramMessage("UID terbaca: " + String(uidString));
  sendTelegramMessage("Daftarkan dengan format:\n/addmember Nama:" + String(uidString));
  mfrc522.PICC_HaltA();
  checkUidMode = false;
}

// ========== LIST MEMBER ==========
void sendMemberList() {
  if (currentUserCount == 0) {
    sendTelegramMessage("Belum ada member yang terdaftar.");
    return;
  }

  String message = "Daftar Member Terdaftar:\n";
  for (int i = 0; i < currentUserCount; i++) {
    message += String(i + 1) + ". " + userListDynamic[i].name + " - ";
    for (int j = 0; j < userListDynamic[i].uidCount; j++) {
      for (int k = 0; k < 4; k++) {
        message += userListDynamic[i].uids[j][k] < 0x10 ? "0" : "";
        message += String(userListDynamic[i].uids[j][k], HEX);
        if (k < 3) message += ":";
      }
      if (j < userListDynamic[i].uidCount - 1) message += ", ";
    }
    message += "\n";
  }

  sendTelegramMessage(message);
}

void deleteMemberFromCommand(String input) {
  int separatorIndex = input.indexOf(":");
  if (separatorIndex == -1) {
    sendTelegramMessage("Format salah. Gunakan: /deletemember Nama:DE:AD:BE:EF");
    return;
  }

  String name = input.substring(0, separatorIndex);
  String uidStr = input.substring(separatorIndex + 1);
  uidStr.trim();

  byte targetUid[4];
  int idx = 0, last = 0;
  while (idx < 4 && last < uidStr.length()) {
    int next = uidStr.indexOf(":", last);
    if (next == -1) next = uidStr.length();
    String part = uidStr.substring(last, next);
    targetUid[idx++] = (byte)strtol(part.c_str(), NULL, 16);
    last = next + 1;
  }

  if (idx < 4) {
    sendTelegramMessage("Format UID tidak valid. Gunakan 4 byte, contoh: DE:AD:BE:EF");
    return;
  }

  bool deleted = false;
  for (int i = 0; i < currentUserCount; i++) {
    if (userListDynamic[i].name == name) {
      for (int j = 0; j < userListDynamic[i].uidCount; j++) {
        if (memcmp(userListDynamic[i].uids[j], targetUid, 4) == 0) {
          // Geser array
          for (int k = i; k < currentUserCount - 1; k++) {
            userListDynamic[k] = userListDynamic[k + 1];
          }
          currentUserCount--;
          deleted = true;
          break;
        }
      }
    }
    if (deleted) break;
  }

  if (deleted) {
    sendTelegramMessage("User " + name + " berhasil dihapus.");
  } else {
    sendTelegramMessage("User tidak ditemukan atau UID tidak cocok.");
  }
}
