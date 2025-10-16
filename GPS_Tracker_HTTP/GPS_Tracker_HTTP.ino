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
const char APN[] = "mtnirancell"; // APN اپراتور شما
const char SERVER_URL[] = "https://gps-tracker-online.onrender.com"; // آدرس سرور
const char DEVICE_ID[] = "GPS001"; // شناسه یکتا برای دستگاه

// تنظیمات سیستم
const unsigned long SEND_INTERVAL = 60000; // ارسال هر 60 ثانیه
const unsigned long GPS_TIMEOUT = 10000; // 10 ثانیه تایم‌اوت GPS
const double RADIUS_METERS = 100.0; // شعاع مجاز

// ساختار موقعیت
struct Location {
  double lat;
  double lng;
  bool valid;
  unsigned long timestamp;
  float speed;
  float course;
};

// متغیرهای سیستم
Location currentLocation = {0, 0, false, 0, 0, 0};
Location lastLocation = {0, 0, false, 0, 0, 0};
Location baseLocation = {0, 0, false, 0, 0, 0};

// متغیرهای زمان
unsigned long lastSend = 0;
unsigned long lastGPSRead = 0;
unsigned long lastCheck = 0;

// متغیرهای وضعیت
bool trackingMode = false;
bool locationStable = false;
bool internetConnected = false;
int stableCount = 0;
int gpsTimeout = 0;

void setup() {
  Serial.begin(9600);
  delay(3000);
  
  Serial.println("=== سیستم ردیابی GPS آنلاین ===");
  Serial.println("Serial متصل شد");
  
  sim800l.begin(9600);
  gpsSerial.begin(9600);
  
  delay(1000);
  Serial.println("ماژول‌ها راه‌اندازی شدند");
  
  // راه‌اندازی SIM800L
  initializeSIM800L();
  
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
  
  // بررسی تایم‌اوت GPS
  checkGPSTimeout();
  
  // بررسی اتصال اینترنت
  if (!internetConnected) {
    if (millis() - lastCheck > 30000) { // هر 30 ثانیه چک کن
      Serial.println("تلاش برای اتصال مجدد...");
      connectToInternet();
      lastCheck = millis();
    }
  }
  
  // ارسال موقعیت
  if (trackingMode && internetConnected && currentLocation.valid) {
    if (millis() - lastSend >= SEND_INTERVAL) {
      Serial.println("ارسال موقعیت...");
      sendLocationToServer();
      lastSend = millis();
    }
  }
  
  // بررسی خروج از محدوده
  if (trackingMode && locationStable && currentLocation.valid) {
    checkLocationBoundary();
  }
  
  delay(100);
}

void initializeSIM800L() {
  Serial.println("راه‌اندازی SIM800L...");
  
  // تست اتصال
  sendATCommand("AT", 1000);
  
  // تنظیم حالت پیامک
  sendATCommand("AT+CMGF=1", 1000);
  
  // تنظیم دریافت پیامک
  sendATCommand("AT+CNMI=1,2,0,0,0", 1000);
  
  // بررسی وضعیت شبکه
  sendATCommand("AT+CREG?", 2000);
  
  Serial.println("SIM800L آماده است");
}

void connectToInternet() {
  Serial.println("اتصال به اینترنت...");
  
  // بررسی وضعیت شبکه
  Serial.println("بررسی وضعیت شبکه...");
  sendATCommand("AT+CREG?", 2000);
  sendATCommand("AT+COPS?", 2000);
  
  // تنظیم APN
  Serial.print("تنظیم APN: ");
  Serial.println(APN);
  char apnCommand[30];
  sprintf(apnCommand, "AT+SAPBR=3,1,\"APN\",\"%s\"", APN);
  if (sendATCommand(apnCommand, 2000)) {
    Serial.println("APN تنظیم شد");
  } else {
    Serial.println("خطا در تنظیم APN");
  }
  
  // باز کردن اتصال GPRS
  Serial.println("باز کردن اتصال GPRS...");
  if (sendATCommand("AT+SAPBR=1,1", 10000)) {
    Serial.println("اتصال GPRS برقرار شد");
    
    // دریافت IP
    Serial.println("دریافت آدرس IP...");
    if (sendATCommand("AT+SAPBR=2,1", 5000)) {
      internetConnected = true;
      Serial.println("اینترنت متصل است");
    } else {
      Serial.println("خطا در دریافت IP");
    }
  } else {
    internetConnected = false;
    Serial.println("خطا در اتصال GPRS");
  }
}

void readGPS() {
  while (gpsSerial.available()) {
    char c = gpsSerial.read();
    gps.encode(c);
    lastGPSRead = millis();
    
    if (gps.location.isUpdated()) {
      lastLocation = currentLocation;
      currentLocation.lat = gps.location.lat();
      currentLocation.lng = gps.location.lng();
      currentLocation.valid = true;
      currentLocation.timestamp = millis();
      
      if (gps.speed.isValid()) {
        currentLocation.speed = gps.speed.kmph();
      }
      
      if (gps.course.isValid()) {
        currentLocation.course = gps.course.deg();
      }
      
      // تثبیت موقعیت پایه
      if (!locationStable) {
        if (baseLocation.valid) {
          double distance = calculateDistance(baseLocation.lat, baseLocation.lng, 
                                            currentLocation.lat, currentLocation.lng);
          if (distance < 10.0) {
            stableCount++;
            if (stableCount >= 3) {
              locationStable = true;
              trackingMode = true;
              Serial.println("ردیابی شروع شد - موقعیت تثبیت شد");
            }
          } else {
            stableCount = 0;
          }
        } else {
          baseLocation = currentLocation;
          stableCount++;
          if (stableCount >= 3) {
            locationStable = true;
            trackingMode = true;
            Serial.println("ردیابی شروع شد - موقعیت پایه تثبیت شد");
          }
        }
      }
      
      gpsTimeout = 0;
    }
  }
}

void checkGPSTimeout() {
  if (millis() - lastGPSRead > 1000) {
    gpsTimeout++;
    if (gpsTimeout > 10) {
      currentLocation.valid = false;
      Serial.println("هشدار: سیگنال GPS قطع شده");
    }
  }
}

void sendLocationToServer() {
  if (!currentLocation.valid) return;
  
  // ساخت JSON برای ارسال
  char jsonData[150];
  sprintf(jsonData, "{\"id\":\"%s\",\"lat\":%.6f,\"lng\":%.6f,\"spd\":%.1f,\"ts\":%lu}",
          DEVICE_ID, currentLocation.lat, currentLocation.lng, currentLocation.speed, currentLocation.timestamp);
  
  // ارسال HTTP POST
  if (sendATCommand("AT+HTTPINIT", 2000)) {
    
    // تنظیم URL
    char urlCommand[80];
    sprintf(urlCommand, "AT+HTTPPARA=\"URL\",\"%s/api/location\"", SERVER_URL);
    sendATCommand(urlCommand, 2000);
    
    // تنظیم Content-Type
    sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);
    
    // تنظیم اندازه داده
    char dataSizeCommand[30];
    sprintf(dataSizeCommand, "AT+HTTPDATA=%d,10000", strlen(jsonData));
    sendATCommand(dataSizeCommand, 2000);
    
    // ارسال داده
    sim800l.print(jsonData);
    delay(1000);
    
    // ارسال درخواست
    if (sendATCommand("AT+HTTPACTION=1", 10000)) {
      Serial.print("موقعیت ارسال شد: ");
      Serial.print(currentLocation.lat, 6);
      Serial.print(", ");
      Serial.println(currentLocation.lng, 6);
    }
    
    // بستن اتصال HTTP
    sendATCommand("AT+HTTPTERM", 1000);
  }
}

void checkLocationBoundary() {
  if (!baseLocation.valid) return;
  
  double distance = calculateDistance(baseLocation.lat, baseLocation.lng, 
                                    currentLocation.lat, currentLocation.lng);
  
  if (distance > RADIUS_METERS) {
    // ارسال هشدار به سرور
    char alertData[120];
    sprintf(alertData, "{\"id\":\"%s\",\"alert\":\"breach\",\"dist\":%.1f,\"lat\":%.6f,\"lng\":%.6f}",
            DEVICE_ID, distance, currentLocation.lat, currentLocation.lng);
    
    sendAlertToServer(alertData);
  }
}

void sendAlertToServer(const char* alertData) {
  if (sendATCommand("AT+HTTPINIT", 2000)) {
    
    char urlCommand[80];
    sprintf(urlCommand, "AT+HTTPPARA=\"URL\",\"%s/api/alert\"", SERVER_URL);
    sendATCommand(urlCommand, 2000);
    
    sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);
    
    char dataSizeCommand[30];
    sprintf(dataSizeCommand, "AT+HTTPDATA=%d,10000", strlen(alertData));
    sendATCommand(dataSizeCommand, 2000);
    
    sim800l.print(alertData);
    delay(1000);
    
    sendATCommand("AT+HTTPACTION=1", 10000);
    sendATCommand("AT+HTTPTERM", 1000);
    
    Serial.print("هشدار ارسال شد - فاصله: ");
    Serial.print(calculateDistance(baseLocation.lat, baseLocation.lng, currentLocation.lat, currentLocation.lng), 1);
    Serial.println(" متر");
  }
}

double calculateDistance(double lat1, double lng1, double lat2, double lng2) {
  const double R = 6371000;
  double dLat = radians(lat2 - lat1);
  double dLng = radians(lng2 - lng1);
  
  double a = sin(dLat/2) * sin(dLat/2) +
            cos(radians(lat1)) * cos(radians(lat2)) *
            sin(dLng/2) * sin(dLng/2);
  
  double c = 2 * atan2(sqrt(a), sqrt(1-a));
  return R * c;
}

bool sendATCommand(const char* command, int timeout) {
  Serial.print("ارسال: ");
  Serial.println(command);
  sim800l.println(command);
  delay(timeout);
  
  char response[100] = "";
  int i = 0;
  while (sim800l.available() && i < 99) {
    response[i] = sim800l.read();
    i++;
  }
  response[i] = '\0';
  
  Serial.print("پاسخ: ");
  Serial.println(response);
  
  bool success = (strstr(response, "OK") != NULL);
  if (!success) {
    Serial.print("خطا در دستور: ");
    Serial.println(command);
  }
  
  return success;
}
