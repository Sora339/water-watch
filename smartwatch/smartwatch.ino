#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <cmath>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEScan.h>
#include <BLEAdvertising.h>
#include "ImgData.h"

// =====================================
// Mode Definitions
// =====================================
enum AppMode {
  MODE_MENU,
  MODE_CLOCK,
  MODE_PEDOMETER,
  MODE_MORSE
};

AppMode currentMode = MODE_MENU;
int menuSelection = 0;
const int NUM_MODES = 3;
unsigned long lastButtonPressTime = 0;
const unsigned long MENU_TIMEOUT = 2000; // 2 seconds

// =====================================
// Clock Mode Variables
// =====================================
const char* WIFI_SSID = "NishiboriのiPhone";
const char* WIFI_PASS = "SSItechB160";
static const long GMT_OFFSET_SEC = 9 * 3600;
static const int DST_OFFSET_SEC = 0;
const char* NTP_1 = "ntp.nict.jp";
const char* NTP_2 = "pool.ntp.org";

const uint16_t COLOR_BG = 0x5AEB;
const uint16_t COLOR_FACE = GREEN;
const uint16_t COLOR_TEXT = WHITE;

// Clock center and radius for portrait mode
const uint16_t CLOCK_CENTER_X = 60;
const uint16_t CLOCK_CENTER_Y = 80;
const uint16_t CLOCK_RADIUS = 50;

float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;
float sdeg = 0, mdeg = 0, hdeg = 0;
uint16_t osx = CLOCK_CENTER_X, osy = CLOCK_CENTER_Y;
uint16_t omx = CLOCK_CENTER_X, omy = CLOCK_CENTER_Y;
uint16_t ohx = CLOCK_CENTER_X, ohy = CLOCK_CENTER_Y;
uint16_t x0 = 0, x1 = 0, yy0 = 0, yy1 = 0;
uint32_t targetTime = 0;
bool timeSynced = false;

static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9') v = *p - '0';
  return 10 * v + *++p - '0';
}

uint8_t hh = conv2d(__TIME__), mm = conv2d(__TIME__ + 3), ss = conv2d(__TIME__ + 6);

// =====================================
// Pedometer Mode Variables
// =====================================
float accX = 0.0F;
float accY = 0.0F;
float accZ = 0.0F;
float lastMag = 0.0F;
int stepCount = 0;
bool isStep = false;
unsigned long lastStepTime = 0;
const int minStepInterval = 200;
const float stepThreshold = 1.3;

// =====================================
// Morse Mode Variables
// =====================================
#define SERVICE_UUID "4fafc201-1fb5-459e-8fcc-c5c9c331914b"

BLEScan* pBLEScan;
BLEAdvertising* pBLEAdvertising;

String currentSymbol = "";
String currentMessage = "";
String lastReceivedMorse = "";
unsigned long lastReceivedTime = 0;
String receivedMessageDisplay = "";
unsigned long receivedDisplayStartTime = 0;

unsigned long pressStartTime = 0;
unsigned long lastInputTime = 0;
bool isInputMode = false;
bool sending = false;
unsigned long sendStartTime = 0;

unsigned long lastDisplayUpdate = 0;
int scanDots = 0;

const unsigned long RECEIVED_DISPLAY_DURATION = 5000;
const unsigned long CHAR_TIMEOUT = 1200;
const unsigned long MSG_TIMEOUT = 3000;
const unsigned long SEND_DURATION = 12000;
const unsigned long DUPLICATE_TIMEOUT = 10000;

// =====================================
// Menu Functions
// =====================================
void drawMenu() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setRotation(0);
  StickCP2.Display.setTextSize(2);
  StickCP2.Display.setTextColor(WHITE, BLACK);

  StickCP2.Display.setCursor(10, 10);
  StickCP2.Display.println("MENU");

  const char* modeNames[] = {"Clock", "Pedometer", "Morse"};

  for (int i = 0; i < NUM_MODES; i++) {
    int y = 40 + i * 30;
    if (i == menuSelection) {
      StickCP2.Display.fillRect(5, y - 2, 110, 25, BLUE);
      StickCP2.Display.setTextColor(WHITE, BLUE);
    } else {
      StickCP2.Display.setTextColor(WHITE, BLACK);
    }
    StickCP2.Display.setCursor(10, y);
    StickCP2.Display.println(modeNames[i]);
  }

  StickCP2.Display.setTextColor(GREEN, BLACK);
  StickCP2.Display.setTextSize(1);
  StickCP2.Display.setCursor(5, 200);
  StickCP2.Display.println("A:Select");
  StickCP2.Display.setCursor(5, 215);
  StickCP2.Display.println("B:Menu");
}

void handleMenu() {
  StickCP2.update();

  if (StickCP2.BtnA.wasPressed()) {
    menuSelection = (menuSelection + 1) % NUM_MODES;
    lastButtonPressTime = millis();
    drawMenu();
  }

  if (StickCP2.BtnB.wasPressed()) {
    // Return to menu from any mode
    currentMode = MODE_MENU;
    menuSelection = 0;
    lastButtonPressTime = millis();
    drawMenu();
  }

  // Auto-select after timeout
  if (millis() - lastButtonPressTime > MENU_TIMEOUT && lastButtonPressTime > 0) {
    switch (menuSelection) {
      case 0:
        currentMode = MODE_CLOCK;
        initClockMode();
        break;
      case 1:
        currentMode = MODE_PEDOMETER;
        initPedometerMode();
        break;
      case 2:
        currentMode = MODE_MORSE;
        initMorseMode();
        break;
    }
    lastButtonPressTime = 0;
  }
}

// =====================================
// Clock Mode Functions
// =====================================
void drawClockFace() {
  StickCP2.Display.fillScreen(COLOR_BG);
  StickCP2.Display.setTextColor(COLOR_TEXT, COLOR_BG);

  StickCP2.Display.fillCircle(CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS, COLOR_FACE);
  StickCP2.Display.fillCircle(CLOCK_CENTER_X, CLOCK_CENTER_Y, CLOCK_RADIUS - 4, BLACK);

  // Hour markers
  for (int i = 0; i < 360; i += 30) {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * (CLOCK_RADIUS - 2) + CLOCK_CENTER_X;
    yy0 = sy * (CLOCK_RADIUS - 2) + CLOCK_CENTER_Y;
    x1 = sx * (CLOCK_RADIUS - 10) + CLOCK_CENTER_X;
    yy1 = sy * (CLOCK_RADIUS - 10) + CLOCK_CENTER_Y;
    StickCP2.Display.drawLine(x0, yy0, x1, yy1, COLOR_FACE);
  }

  // Minute markers
  for (int i = 0; i < 360; i += 6) {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * (CLOCK_RADIUS - 6) + CLOCK_CENTER_X;
    yy0 = sy * (CLOCK_RADIUS - 6) + CLOCK_CENTER_Y;
    StickCP2.Display.drawPixel(x0, yy0, COLOR_TEXT);
    if (i == 0 || i == 180) StickCP2.Display.fillCircle(x0, yy0, 2, COLOR_TEXT);
    if (i == 90 || i == 270) StickCP2.Display.fillCircle(x0, yy0, 2, COLOR_TEXT);
  }
  StickCP2.Display.fillCircle(CLOCK_CENTER_X, CLOCK_CENTER_Y, 3, COLOR_TEXT);
}

void drawDigitalClock() {
  auto t = StickCP2.Rtc.getTime();
  StickCP2.Display.fillRect(0, 145, 120, 20, COLOR_BG);
  StickCP2.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  StickCP2.Display.setTextSize(2);
  StickCP2.Display.setCursor(8, 145);
  StickCP2.Display.printf("%02d:%02d:%02d", t.hours, t.minutes, t.seconds);
}

void updateHands() {
  auto t = StickCP2.Rtc.getTime();
  hh = t.hours;
  mm = t.minutes;
  ss = t.seconds;

  sdeg = ss * 6;
  mdeg = mm * 6 + sdeg * 0.01666667;
  hdeg = hh * 30 + mdeg * 0.0833333;
  hx = cos((hdeg - 90) * 0.0174532925);
  hy = sin((hdeg - 90) * 0.0174532925);
  mx = cos((mdeg - 90) * 0.0174532925);
  my = sin((mdeg - 90) * 0.0174532925);
  sx = cos((sdeg - 90) * 0.0174532925);
  sy = sin((sdeg - 90) * 0.0174532925);

  // Erase old second hand
  StickCP2.Display.drawLine(osx, osy, CLOCK_CENTER_X, CLOCK_CENTER_Y, BLACK);
  osx = sx * (CLOCK_RADIUS - 12) + CLOCK_CENTER_X;
  osy = sy * (CLOCK_RADIUS - 12) + CLOCK_CENTER_Y;
  StickCP2.Display.drawLine(osx, osy, CLOCK_CENTER_X, CLOCK_CENTER_Y, RED);

  // Erase old hour hand
  StickCP2.Display.drawLine(ohx, ohy, CLOCK_CENTER_X, CLOCK_CENTER_Y, BLACK);
  ohx = hx * (CLOCK_RADIUS - 28) + CLOCK_CENTER_X;
  ohy = hy * (CLOCK_RADIUS - 28) + CLOCK_CENTER_Y;
  StickCP2.Display.drawLine(ohx, ohy, CLOCK_CENTER_X, CLOCK_CENTER_Y, WHITE);

  // Erase old minute hand
  StickCP2.Display.drawLine(omx, omy, CLOCK_CENTER_X, CLOCK_CENTER_Y, BLACK);
  omx = mx * (CLOCK_RADIUS - 18) + CLOCK_CENTER_X;
  omy = my * (CLOCK_RADIUS - 18) + CLOCK_CENTER_Y;
  StickCP2.Display.drawLine(omx, omy, CLOCK_CENTER_X, CLOCK_CENTER_Y, WHITE);

  StickCP2.Display.fillCircle(CLOCK_CENTER_X, CLOCK_CENTER_Y, 3, RED);
}

bool connectWiFi(uint8_t retries = 8) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (uint8_t i = 0; i < retries; ++i) {
    if (WiFi.status() == WL_CONNECTED) return true;
    StickCP2.Display.fillRect(0, 170, 120, 12, COLOR_BG);
    StickCP2.Display.setCursor(5, 170);
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.printf("WiFi %d/%d", i + 1, retries);
    delay(700);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool syncTimeFromNtp() {
  struct timeval tv = {0};
  settimeofday(&tv, nullptr);

  configTime(0, 0, NTP_1, NTP_2);
  for (int i = 0; i < 40; ++i) {
    time_t now = time(nullptr);
    if (now > 1700000000) {
      now += GMT_OFFSET_SEC + DST_OFFSET_SEC;
      struct tm tm_info;
      gmtime_r(&now, &tm_info);

      m5::rtc_time_t t;
      t.hours = tm_info.tm_hour;
      t.minutes = tm_info.tm_min;
      t.seconds = tm_info.tm_sec;
      StickCP2.Rtc.setTime(t);

      m5::rtc_date_t d;
      d.year = tm_info.tm_year + 1900;
      d.month = tm_info.tm_mon + 1;
      d.date = tm_info.tm_mday;
      StickCP2.Rtc.setDate(d);
      return true;
    }
    delay(500);
  }
  return false;
}

void initClockMode() {
  StickCP2.Display.setRotation(0);
  drawClockFace();
  StickCP2.Display.setCursor(5, 170);
  StickCP2.Display.setTextSize(1);
  StickCP2.Display.println("WiFi to NTP...");

  bool wifiOk = connectWiFi();
  if (wifiOk) {
    StickCP2.Display.setCursor(5, 185);
    StickCP2.Display.println("Sync time...");
    timeSynced = syncTimeFromNtp();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    StickCP2.Display.setCursor(5, 185);
    StickCP2.Display.println("WiFi failed");
  }

  targetTime = millis() + 1000;
  updateHands();
  drawDigitalClock();

  if (timeSynced) {
    StickCP2.Display.setCursor(10, 205);
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.println("Synced NTP");
  } else {
    StickCP2.Display.setCursor(10, 205);
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.println("Build time");
  }
}

void handleClockMode() {
  StickCP2.update();

  if (StickCP2.BtnB.wasPressed()) {
    currentMode = MODE_MENU;
    menuSelection = 0;
    lastButtonPressTime = millis();
    drawMenu();
    return;
  }

  if (targetTime < millis()) {
    targetTime += 1000;
    updateHands();
    drawDigitalClock();
  }
}

// =====================================
// Pedometer Mode Functions
// =====================================
void initPedometerMode() {
  StickCP2.Display.setRotation(0);
  StickCP2.Display.fillScreen(BLACK);

  StickCP2.Imu.init();

  float zoom = 3.0;
  StickCP2.Display.pushImageRotateZoom(
    StickCP2.Display.width() / 2.0,
    StickCP2.Display.height() / 3.0,
    IMG_WIDTH / 2.0, IMG_HEIGHT / 2.0,
    0,
    zoom, zoom,
    IMG_WIDTH, IMG_HEIGHT,
    IMG_DATA[0]
  );

  StickCP2.Display.setTextSize(3);
  StickCP2.Display.setTextColor(WHITE, BLACK);
  StickCP2.Display.setTextDatum(top_center);
  StickCP2.Display.drawString("00000", StickCP2.Display.width() / 2, StickCP2.Display.height() / 2 + 20);
}

void handlePedometerMode() {
  StickCP2.update();

  if (StickCP2.BtnB.wasPressed()) {
    currentMode = MODE_MENU;
    menuSelection = 1;
    lastButtonPressTime = millis();
    drawMenu();
    return;
  }

  StickCP2.Imu.getAccelData(&accX, &accY, &accZ);
  float magnitude = sqrt(accX * accX + accY * accY + accZ * accZ);

  if (magnitude > stepThreshold && !isStep && (millis() - lastStepTime > minStepInterval)) {
    isStep = true;
    stepCount++;
    lastStepTime = millis();

    StickCP2.Display.setTextDatum(top_center);
    StickCP2.Display.setTextColor(WHITE, BLACK);
    char buf[8];
    snprintf(buf, sizeof(buf), "%05d", stepCount);
    StickCP2.Display.drawString(buf, StickCP2.Display.width() / 2, StickCP2.Display.height() / 2 + 20);
  } else if (magnitude < stepThreshold) {
    isStep = false;
  }

  delay(20);
}

// =====================================
// Morse Mode Functions
// =====================================
char decodeMorse(String morse) {
  if (morse == ".-") return 'A';
  if (morse == "-...") return 'B';
  if (morse == "-.-.") return 'C';
  if (morse == "-..") return 'D';
  if (morse == ".") return 'E';
  if (morse == "..-.") return 'F';
  if (morse == "--.") return 'G';
  if (morse == "....") return 'H';
  if (morse == "..") return 'I';
  if (morse == ".---") return 'J';
  if (morse == "-.-") return 'K';
  if (morse == ".-..") return 'L';
  if (morse == "--") return 'M';
  if (morse == "-.") return 'N';
  if (morse == "---") return 'O';
  if (morse == ".--.") return 'P';
  if (morse == "--.-") return 'Q';
  if (morse == ".-.") return 'R';
  if (morse == "...") return 'S';
  if (morse == "-") return 'T';
  if (morse == "..-") return 'U';
  if (morse == "...-") return 'V';
  if (morse == ".--") return 'W';
  if (morse == "-..-") return 'X';
  if (morse == "-.--") return 'Y';
  if (morse == "--..") return 'Z';
  return '?';
}

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(BLEUUID(SERVICE_UUID))) {
      String data = advertisedDevice.getName().c_str();

      if (data.startsWith("M:")) {
        String m = data.substring(2);

        bool isDuplicate = (m == lastReceivedMorse) && (millis() - lastReceivedTime < DUPLICATE_TIMEOUT);

        if (!isDuplicate && m.length() > 0) {
          lastReceivedMorse = m;
          lastReceivedTime = millis();
          receivedMessageDisplay = m;
          receivedDisplayStartTime = millis();

          StickCP2.Display.fillScreen(BLACK);
          StickCP2.Display.setCursor(10, 20);
          StickCP2.Display.setTextSize(2);
          StickCP2.Display.println("RX MSG:");

          StickCP2.Display.setCursor(10, 60);
          StickCP2.Display.setTextSize(3);
          StickCP2.Display.print(m);

          StickCP2.Speaker.tone(2000, 500);
        }
      }
    }
  }
};

void startAdvertising(String msg) {
  if (pBLEAdvertising == nullptr) {
    BLEDevice::init("M5Morse");
    pBLEAdvertising = BLEDevice::getAdvertising();
    pBLEAdvertising->setMinInterval(32);
    pBLEAdvertising->setMaxInterval(32);
  }

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_ADV, ESP_PWR_LVL_P9);

  pBLEAdvertising->stop();

  BLEAdvertisementData oAdvertisementData = BLEAdvertisementData();
  oAdvertisementData.setFlags(0x04);
  oAdvertisementData.setCompleteServices(BLEUUID(SERVICE_UUID));

  String payload = "M:" + msg;
  if(payload.length() > 29) payload = payload.substring(0, 29);

  oAdvertisementData.setName(payload.c_str());

  pBLEAdvertising->setAdvertisementData(oAdvertisementData);
  pBLEAdvertising->start();
  sending = true;
  sendStartTime = millis();
}

void stopAdvertising() {
  if (pBLEAdvertising) pBLEAdvertising->stop();
  sending = false;
}

void updateInputDisplay() {
  StickCP2.Display.fillScreen(BLUE);
  StickCP2.Display.setCursor(10, 10);
  StickCP2.Display.setTextSize(2);
  StickCP2.Display.println("Input...");

  StickCP2.Display.setCursor(10, 50);
  StickCP2.Display.setTextSize(3);
  StickCP2.Display.print(currentMessage.c_str());

  StickCP2.Display.print(" ");
  StickCP2.Display.setTextColor(YELLOW, BLUE);
  StickCP2.Display.print(currentSymbol.c_str());
  StickCP2.Display.setTextColor(WHITE, BLUE);
}

void updateReadyDisplay() {
  StickCP2.Display.fillScreen(BLACK);
  StickCP2.Display.setCursor(25, 20);
  StickCP2.Display.setTextSize(2);
  StickCP2.Display.println("READY");

  StickCP2.Display.setCursor(20, 60);
  StickCP2.Display.setTextSize(1);
  StickCP2.Display.setTextColor(GREEN, BLACK);
  StickCP2.Display.print("SCAN");

  for (int i = 0; i < scanDots; i++) {
    StickCP2.Display.print(".");
  }
  StickCP2.Display.setTextColor(WHITE, BLACK);

  StickCP2.Display.setCursor(10, 100);
  StickCP2.Display.println("Press Btn");
  StickCP2.Display.setCursor(10, 115);
  StickCP2.Display.println("to start");
  StickCP2.Display.setCursor(10, 130);
  StickCP2.Display.println("input");
}

void initMorseMode() {
  StickCP2.Display.setRotation(0);
  StickCP2.Display.setTextSize(2);
  updateReadyDisplay();

  BLEDevice::init("M5Morse");

  esp_ble_tx_power_set(ESP_BLE_PWR_TYPE_SCAN, ESP_PWR_LVL_P9);

  pBLEScan = BLEDevice::getScan();
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(99);
}

void handleMorseMode() {
  StickCP2.update();

  // BtnB for returning to menu
  if (StickCP2.BtnB.wasPressed() && !isInputMode) {
    currentMode = MODE_MENU;
    menuSelection = 2;
    lastButtonPressTime = millis();
    drawMenu();
    return;
  }

  // BtnA: Input Start / Typing
  if (StickCP2.BtnA.wasPressed()) {
    if (!isInputMode) {
      isInputMode = true;
      currentMessage = "";
      currentSymbol = "";
      updateInputDisplay();
    }
    pressStartTime = millis();
  }

  if (isInputMode) {
    if (StickCP2.BtnA.wasReleased()) {
      unsigned long duration = millis() - pressStartTime;

      if (duration < 250) {
        currentSymbol += ".";
        StickCP2.Speaker.tone(4000, 50);
      } else {
        currentSymbol += "-";
        StickCP2.Speaker.tone(4000, 150);
      }
      lastInputTime = millis();
      updateInputDisplay();
    }

    unsigned long idleTime = millis() - lastInputTime;

    if (currentSymbol.length() > 0 && idleTime > CHAR_TIMEOUT) {
      char c = decodeMorse(currentSymbol);
      currentMessage += c;
      currentSymbol = "";
      updateInputDisplay();
      StickCP2.Speaker.tone(3000, 50);
    }

    if (currentMessage.length() > 0 && currentSymbol.length() == 0 && idleTime > MSG_TIMEOUT) {
      isInputMode = false;

      StickCP2.Display.fillScreen(GREEN);
      StickCP2.Display.setCursor(10, 30);
      StickCP2.Display.setTextSize(2);
      StickCP2.Display.println("TX:");

      StickCP2.Display.setCursor(10, 70);
      StickCP2.Display.setTextSize(3);
      StickCP2.Display.print(currentMessage);

      startAdvertising(currentMessage);
      currentMessage = "";
    }
  }

  if (receivedMessageDisplay.length() > 0 &&
      millis() - receivedDisplayStartTime > RECEIVED_DISPLAY_DURATION) {
    receivedMessageDisplay = "";
    scanDots = 0;
    lastDisplayUpdate = millis();
    updateReadyDisplay();
  }

  if (sending) {
    unsigned long elapsed = millis() - sendStartTime;
    if (elapsed % 1000 < 100) {
      unsigned long remaining = (SEND_DURATION - elapsed) / 1000;
      StickCP2.Display.fillRect(10, 120, 100, 25, GREEN);
      StickCP2.Display.setCursor(10, 120);
      StickCP2.Display.setTextSize(2);
      StickCP2.Display.setTextColor(WHITE, GREEN);
      StickCP2.Display.print("TX: ");
      StickCP2.Display.print(remaining);
      StickCP2.Display.print("s");
      StickCP2.Display.setTextColor(WHITE, BLACK);
    }
  }

  if (!isInputMode && !sending) {
    pBLEScan->start(2, false);
    pBLEScan->clearResults();

    if (receivedMessageDisplay.length() == 0 && millis() - lastDisplayUpdate > 500) {
      lastDisplayUpdate = millis();
      scanDots = (scanDots + 1) % 4;
      updateReadyDisplay();
    }
  }

  if (sending && millis() - sendStartTime > SEND_DURATION) {
    stopAdvertising();
    scanDots = 0;
    lastDisplayUpdate = millis();
    updateReadyDisplay();
  }
}

// =====================================
// Main Setup and Loop
// =====================================
void setup() {
  auto cfg = M5.config();
  StickCP2.begin(cfg);

  currentMode = MODE_MENU;
  menuSelection = 0;
  lastButtonPressTime = millis();

  drawMenu();
}

void loop() {
  switch (currentMode) {
    case MODE_MENU:
      handleMenu();
      break;
    case MODE_CLOCK:
      handleClockMode();
      break;
    case MODE_PEDOMETER:
      handlePedometerMode();
      break;
    case MODE_MORSE:
      handleMorseMode();
      break;
  }
}
