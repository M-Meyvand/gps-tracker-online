#include <SoftwareSerial.h>
#include <string.h>

#define SIM800L_RX 2
#define SIM800L_TX 3

SoftwareSerial sim800l(SIM800L_RX, SIM800L_TX);

void setup() {
  Serial.begin(9600);
  sim800l.begin(9600);
  
  Serial.println("=== تست ارسال داده با HTTPS ===");
  delay(3000);
  
  // راه‌اندازی
  sendATCommand("AT", 2000);
  sendATCommand("AT+SAPBR=3,1,\"APN\",\"mtnirancell\"", 2000);
  sendATCommand("AT+SAPBR=1,1", 3000);
  
  // ارسال داده تست
  sendTestData();
}

void loop() {
  delay(10000);
  sendTestData();
}

void sendTestData() {
  Serial.println("ارسال داده تست...");
  
  // داده تست
  char jsonData[] = "{\"device_id\":\"GPS002\",\"latitude\":35.6892,\"longitude\":51.3890,\"altitude\":1200.0,\"speed\":0.0,\"course\":0.0}";
  
  // راه‌اندازی HTTP
  sendATCommand("AT+HTTPINIT", 2000);
  sendATCommand("AT+HTTPPARA=\"URL\",\"https://gps-tracker-online.onrender.com/api/location\"", 2000);
  sendATCommand("AT+HTTPPARA=\"CONTENT\",\"application/json\"", 2000);
  
  // ارسال داده
  char dataSizeCommand[30];
  sprintf(dataSizeCommand, "AT+HTTPDATA=%d,10000", strlen(jsonData));
  sendATCommand(dataSizeCommand, 2000);
  
  sim800l.print(jsonData);
  delay(2000);
  
  // ارسال درخواست
  sendATCommand("AT+HTTPACTION=1", 5000);
  sendATCommand("AT+HTTPTERM", 1000);
  
  Serial.println("داده ارسال شد");
}

bool sendATCommand(const char* command, int timeout) {
  Serial.print("ارسال: ");
  Serial.println(command);
  
  sim800l.println(command);
  delay(timeout);
  
  if (sim800l.available()) {
    char response[200];
    int i = 0;
    while (sim800l.available() && i < 199) {
      response[i] = sim800l.read();
      i++;
    }
    response[i] = '\0';
    
    Serial.print("پاسخ: ");
    Serial.println(response);
    
    return (strstr(response, "OK") != NULL);
  }
  
  Serial.println("بدون پاسخ");
  return false;
}
