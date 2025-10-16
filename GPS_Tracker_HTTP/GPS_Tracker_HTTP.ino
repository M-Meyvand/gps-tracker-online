#include <SoftwareSerial.h>
#include <TinyGPS++.h>

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
const String APN = "internet"; // APN اپراتور شما (مثل: internet, mci, irancell)
const String SERVER_URL = "http://192.168.1.5:3001"; // آدرس سرور محلی
const String DEVICE_ID = "GPS001"; // شناسه یکتا برای دستگاه

// تنظیمات سیستم
const unsigned long SEND_INTERVAL = 30000; // ارسال هر 30 ثانیه
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

// بافر برای پاسخ HTTP
String httpResponse = "";

void setup() {
  Serial.begin(9600);
  sim800l.begin(9600);
  gpsSerial.begin(9600);

  delay(2000);

  Serial.println("=== سیستم ردیابی GPS آنلاین ===");
  
  // راه‌اندازی SIM800L
  initializeSIM800L();
  
  // اتصال به اینترنت
  connectToInternet();
  
  Serial.println("سیستم آماده است");
}

void loop() {
  // خواندن GPS
  readGPS();
  
  // بررسی تایم‌اوت GPS
  checkGPSTimeout();
  
  // بررسی اتصال اینترنت
  if (!internetConnected) {
    if (millis() - lastCheck > 30000) { // هر 30 ثانیه چک کن
      connectToInternet();
      lastCheck = millis();
    }
  }
  
  // ارسال موقعیت
  if (trackingMode && internetConnected && currentLocation.valid) {
    if (millis() - lastSend >= SEND_INTERVAL) {
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
  
  // تنظیم APN
  String apnCommand = "AT+SAPBR=3,1,\"APN\",\"" + APN + "\"";
  if (sendATCommand(apnCommand, 2000)) {
    Serial.println("APN تنظیم شد");
  }
  
  // باز کردن اتصال GPRS
  if (sendATCommand("AT+SAPBR=1,1", 3000)) {
    Serial.println("اتصال GPRS برقرار شد");
    
    // دریافت IP
    if (sendATCommand("AT+SAPBR=2,1", 2000)) {
      internetConnected = true;
      Serial.println("اینترنت متصل است");
    }
  } else {
    internetConnected = false;
    Serial.println("خطا در اتصال به اینترنت");
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
  String jsonData = "{";
  jsonData += "\"device_id\":\"" + DEVICE_ID + "\",";
  jsonData += "\"latitude\":" + String(currentLocation.lat, 6) + ",";
  jsonData += "\"longitude\":" + String(currentLocation.lng, 6) + ",";
  jsonData += "\"speed\":" + String(currentLocation.speed, 1) + ",";
  jsonData += "\"course\":" + String(currentLocation.course, 1) + ",";
  jsonData += "\"timestamp\":" + String(currentLocation.timestamp) + ",";
  jsonData += "\"valid\":" + String(currentLocation.valid ? "true" : "false");
  jsonData += "}";
  
  // ارسال HTTP POST
  String httpCommand = "AT+HTTPINIT";
  if (sendATCommand(httpCommand, 2000)) {
    
    // تنظیم URL
    String urlCommand = "AT+HTTPPARA=\"URL\",\"" + SERVER_URL + "/api/location\"";
    sendATCommand(urlCommand, 2000);
    
    // تنظیم Content-Type
    sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);
    
    // تنظیم اندازه داده
    String dataSizeCommand = "AT+HTTPDATA=" + String(jsonData.length()) + ",10000";
    sendATCommand(dataSizeCommand, 2000);
    
    // ارسال داده
    sim800l.print(jsonData);
    delay(1000);
    
    // ارسال درخواست
    if (sendATCommand("AT+HTTPACTION=1", 10000)) {
      Serial.println("موقعیت ارسال شد: " + String(currentLocation.lat, 6) + ", " + String(currentLocation.lng, 6));
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
    String alertData = "{";
    alertData += "\"device_id\":\"" + DEVICE_ID + "\",";
    alertData += "\"alert_type\":\"boundary_breach\",";
    alertData += "\"distance\":" + String(distance, 1) + ",";
    alertData += "\"latitude\":" + String(currentLocation.lat, 6) + ",";
    alertData += "\"longitude\":" + String(currentLocation.lng, 6) + ",";
    alertData += "\"timestamp\":" + String(millis());
    alertData += "}";
    
    sendAlertToServer(alertData);
  }
}

void sendAlertToServer(String alertData) {
  String httpCommand = "AT+HTTPINIT";
  if (sendATCommand(httpCommand, 2000)) {
    
    String urlCommand = "AT+HTTPPARA=\"URL\",\"" + SERVER_URL + "/api/alert\"";
    sendATCommand(urlCommand, 2000);
    
    sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 1000);
    
    String dataSizeCommand = "AT+HTTPDATA=" + String(alertData.length()) + ",10000";
    sendATCommand(dataSizeCommand, 2000);
    
    sim800l.print(alertData);
    delay(1000);
    
    sendATCommand("AT+HTTPACTION=1", 10000);
    sendATCommand("AT+HTTPTERM", 1000);
    
    Serial.println("هشدار ارسال شد - فاصله: " + String(calculateDistance(baseLocation.lat, baseLocation.lng, currentLocation.lat, currentLocation.lng), 1) + " متر");
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

bool sendATCommand(String command, int timeout) {
  sim800l.println(command);
  delay(timeout);
  
  String response = "";
  while (sim800l.available()) {
    response += sim800l.readString();
  }
  
  return response.indexOf("OK") != -1;
}

void checkIncomingSMS() {
  if (sim800l.available()) {
    String response = sim800l.readString();
    
    if (response.indexOf("+CMT:") != -1) {
      delay(100);
      String smsContent = sim800l.readString();
      
      int phoneStart = response.indexOf("\"") + 1;
      int phoneEnd = response.indexOf("\"", phoneStart);
      String senderPhone = response.substring(phoneStart, phoneEnd);
      
      int messageStart = smsContent.indexOf("\n") + 1;
      String message = smsContent.substring(messageStart);
      message.trim();
      
      Serial.println("پیامک از: " + senderPhone + " - محتوا: " + message);
      
      if (message == "start") {
        trackingMode = true;
        baseLocation = currentLocation;
        locationStable = true;
        sendSMS(senderPhone, "ردیابی آنلاین شروع شد");
      } else if (message == "stop") {
        trackingMode = false;
        sendSMS(senderPhone, "ردیابی متوقف شد");
      } else if (message == "status") {
        String status = "وضعیت: " + String(trackingMode ? "فعال" : "غیرفعال") + "\n";
        status += "GPS: " + String(currentLocation.valid ? "متصل" : "قطع") + "\n";
        status += "اینترنت: " + String(internetConnected ? "متصل" : "قطع");
        sendSMS(senderPhone, status);
      }
    }
  }
}

void sendSMS(String number, String message) {
  sim800l.print("AT+CMGS=\"");
  sim800l.print(number);
  sim800l.println("\"");
  delay(500);
  sim800l.print(message);
  delay(500);
  sim800l.write(26);
  delay(3000);
}

/*
سیستم ردیابی GPS آنلاین

ویژگی‌ها:
- ارسال موقعیت از طریق HTTP
- نمایش Real-time روی نقشه
- هشدار خروج از محدوده
- ذخیره مسیر حرکت

تنظیمات:
- APN: نام اپراتور اینترنت
- SERVER_URL: آدرس سرور
- DEVICE_ID: شناسه دستگاه

دستورات پیامکی:
- start: شروع ردیابی
- stop: توقف ردیابی
- status: وضعیت سیستم
*/
