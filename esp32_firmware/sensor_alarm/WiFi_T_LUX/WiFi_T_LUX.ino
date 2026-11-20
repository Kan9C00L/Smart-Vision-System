#include <WiFi.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <math.h>

// ======================================================
// 网络配置
// ======================================================
const char* ssid = "YOUR_WIFI_SSID";          // 改掉
const char* password = "YOUR_WIFI_PASSWORD";  // 改掉
const char* serverIP = "YOUR_PC_IP";          // 把你电脑的 192.168.1.4 改成这个
const int serverPort = 8888;

WiFiClient client;
unsigned long lastReportTime = 0;
bool remoteAlarm = false; 

// ======================================================
// 引脚定义
// ======================================================
#define LIGHT_PIN   34
#define TEMP_PIN    35
#define LED_PIN     2
#define BUZZER_PIN  25
#define SDA_PIN     21
#define SCL_PIN     22

// ======================================================
// OLED 与 参数
// ======================================================
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// 光照与温度阈值
const int LIGHT_ON_THRESHOLD = 2600;
const int LIGHT_OFF_THRESHOLD = 2800;
const float TEMP_ALARM_ON = 30.0;
const float TEMP_ALARM_OFF = 27.0;

// NTC 热敏电阻参数
const float SERIES_RESISTOR = 10000.0;
const float NTC_NOMINAL = 10000.0;
const float TEMPERATURE_NOMINAL = 25.0;
const float BETA_COEFFICIENT = 3950.0;

bool ledState = false;
bool tempAlarm = false;

// ======================================================
// 辅助函数
// ======================================================
int readAverage(int pin) {
  long sum = 0;
  for (int i = 0; i < 10; i++) {
    sum += analogRead(pin);
    delay(5);
  }
  return sum / 10;
}

// NTC ADC -> 摄氏温度
float adcToTemperature(int adcValue) {
  if (adcValue <= 0 || adcValue >= 4095) return NAN;
  float ntcResistance = SERIES_RESISTOR * (4095.0 / adcValue - 1.0);
  float steinhart = ntcResistance / NTC_NOMINAL;
  steinhart = log(steinhart);
  steinhart /= BETA_COEFFICIENT;
  steinhart += 1.0 / (TEMPERATURE_NOMINAL + 273.15);
  steinhart = 1.0 / steinhart;
  return steinhart - 273.15;
}

void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED failed!");
  }
  
  display.clearDisplay();
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 20);
  display.println("CONNECTING WIFI...");
  display.display();

  WiFi.begin(ssid, password);
  WiFi.setSleep(false);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println("\nWiFi Connected!");
  digitalWrite(BUZZER_PIN, HIGH); delay(150); digitalWrite(BUZZER_PIN, LOW); 
}

void loop() {
  // ---------------- 1. 网络通信 ----------------
  if (WiFi.status() == WL_CONNECTED) {
    if (!client.connected()) {
      if (client.connect(serverIP, serverPort)) {
        Serial.println("Connected to Python Server!");
      }
    } else {
      if (client.available()) {
        String command = client.readStringUntil('\n');
        command.trim();
        if (command == "ALERT_ON") {
          remoteAlarm = true;
        } else if (command == "ALERT_OFF") {
          remoteAlarm = false;
        }
      }
    }
  }

  // ---------------- 2. 读取传感器 ----------------
  int lightValue = readAverage(LIGHT_PIN);
  int tempADC = readAverage(TEMP_PIN);
  float temperature = adcToTemperature(tempADC);

  // ---------------- 3. 本地判断逻辑 ----------------
  // 光照迟滞
  if (lightValue >= LIGHT_OFF_THRESHOLD) ledState = false;
  else if (lightValue <= LIGHT_ON_THRESHOLD) ledState = true;
  digitalWrite(LED_PIN, ledState ? HIGH : LOW);

  // 温度迟滞
  if (!isnan(temperature)) {
    if (temperature >= TEMP_ALARM_ON) tempAlarm = true;
    else if (temperature <= TEMP_ALARM_OFF) tempAlarm = false;
  }

  // ---------------- 4. 控制蜂鸣器 ----------------
  // 本地高温 或 电脑视觉报警，满足其一就响
  if (tempAlarm || remoteAlarm) {
    digitalWrite(BUZZER_PIN, HIGH);
  } else {
    digitalWrite(BUZZER_PIN, LOW);
  }

  // ---------------- 5. 向电脑汇报真实数据 ----------------
  if (client.connected() && (millis() - lastReportTime > 2000)) {
    String msg = "DATA," + String(lightValue) + "," + String(temperature, 1) + "\n";
    client.print(msg);
    lastReportTime = millis();
  }

  // ---------------- 6. OLED 显示（恢复温度） ----------------
  display.clearDisplay();
  display.setTextSize(1);
  
  display.setCursor(0, 0);  display.println("SMART SENSOR");
  
  display.setCursor(0, 14); display.print("Light: "); display.println(lightValue);
  
  display.setCursor(0, 28); display.print("Temp : "); 
  if (isnan(temperature)) display.println("ERROR");
  else { display.print(temperature, 1); display.println(" C"); }

  display.setCursor(0, 42); display.print("LED  : "); display.println(ledState ? "ON" : "OFF");
  
  display.setCursor(0, 56); 
  if (remoteAlarm) display.println("ALERT: VISION!");
  else if (tempAlarm) display.println("ALERT: HOT!");
  else display.println("STATUS: NORMAL");

  display.display();

  delay(100);
}