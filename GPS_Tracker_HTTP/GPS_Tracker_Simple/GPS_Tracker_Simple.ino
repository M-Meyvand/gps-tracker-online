#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <string.h>
// پین‌های اتصال
#define SIM800L_RX 10
#define SIM800L_TX 11
#define GPS_RX 8
#define GPS_TX 9

// ماژول‌های ارتباطی
SoftwareSerial sim800l(SIM800L_RX, SIM800L_TX);
SoftwareSerial gpsSerial(GPS_RX, GPS_TX);
TinyGPSPlus gps;

// تنظیمات شبکه
const char APN[] = "mtnirancell";
const char SERVER_URL[] = "https://gps-tracker-online.onrender.com";
const char DEVICE_ID[] = "GPS001";

// متغیرهای سیستم
bool internetConnected = false;
unsigned long lastSend = 0;
const unsigned long SEND_INTERVAL = 60000; // 60 ثانیه

void setup() {
  Serial.begin(9600);
  delay(3000);
  
  Serial.println("=== GPS Tracker Simple ===");
  Serial.println("راه‌اندازی...");
  
  sim800l.begin(9600);
  gpsSerial.begin(9600);
  
  delay(2000);
  Serial.println("ماژول‌ها راه‌اندازی شدند");
  
  // تست SIM800L
  testSIM800L();
  
  // اتصال به اینترنت
  connectToInternet();
  
  Serial.println("سیستم آماده است");
}

void loop() {
  // تست هر 5 ثانیه
  static unsigned long lastTest = 0;
  if (millis() - lastTest > 5000) {
    Serial.println("سیستم در حال کار است...");
    lastTest = millis();
  }
  
  // خواندن GPS
  readGPS();
  
  // ارسال موقعیت
  if (internetConnected && millis() - lastSend >= SEND_INTERVAL) {
    sendLocation();
    lastSend = millis();
  }
  
  delay(100);
}

void testSIM800L() {
  Serial.println("تست SIM800L...");
  
  sim800l.println("AT");
  delay(1000);
  
  if (sim800l.available()) {
    String response = sim800l.readString();
    Serial.println("پاسخ SIM800L: " + response);
  } else {
    Serial.println("هیچ پاسخی از SIM800L دریافت نشد");
  }
}

void connectToInternet() {
  Serial.println("اتصال به اینترنت...");
  
  // تنظیم APN
  char apnCommand[30];
  sprintf(apnCommand, "AT+SAPBR=3,1,\"APN\",\"%s\"", APN);
  sim800l.println(apnCommand);
  delay(2000);
  
  // باز کردن اتصال GPRS
  sim800l.println("AT+SAPBR=1,1");
  delay(10000);
  
  // بررسی IP
  sim800l.println("AT+SAPBR=2,1");
  delay(2000);
  
  if (sim800l.available()) {
    String response = sim800l.readString();
    Serial.println("پاسخ IP: " + response);
    
    if (response.indexOf("+SAPBR: 1,1") != -1) {
      internetConnected = true;
      Serial.println("اینترنت متصل شد!");
    } else {
      internetConnected = false;
      Serial.println("خطا در اتصال اینترنت");
    }
  }
}

void readGPS() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gps.encode(c);
    
    if (gps.location.isUpdated()) {
      Serial.print("GPS: ");
      Serial.print(gps.location.lat(), 6);
      Serial.print(", ");
      Serial.println(gps.location.lng(), 6);
    }
  }
}

void sendLocation() {
  if (!gps.location.isValid()) {
    Serial.println("موقعیت GPS معتبر نیست");
    return;
  }
  
  Serial.println("ارسال موقعیت...");
  
  // ساخت JSON ساده
  char jsonData[100];
  sprintf(jsonData, "{\"id\":\"%s\",\"lat\":%.6f,\"lng\":%.6f}",
          DEVICE_ID, gps.location.lat(), gps.location.lng());
  
  // ارسال HTTP
  sim800l.println("AT+HTTPINIT");
  delay(2000);
  
  char urlCommand[80];
  sprintf(urlCommand, "AT+HTTPPARA=\"URL\",\"%s/api/location\"", SERVER_URL);
  sim800l.println(urlCommand);
  delay(2000);
  
  sim800l.println("AT+HTTPPARA=\"CONTENT\",\"application/json\"");
  delay(1000);
  
  char dataSizeCommand[30];
  sprintf(dataSizeCommand, "AT+HTTPDATA=%d,10000", strlen(jsonData));
  sim800l.println(dataSizeCommand);
  delay(2000);
  
  sim800l.print(jsonData);
  delay(1000);
  
  sim800l.println("AT+HTTPACTION=1");
  delay(10000);
  
  sim800l.println("AT+HTTPTERM");
  delay(1000);
  
  Serial.println("موقعیت ارسال شد");
}
