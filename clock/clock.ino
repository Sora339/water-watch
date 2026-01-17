#include <M5StickCPlus2.h>
#include <WiFi.h>
#include <time.h>
#include <sys/time.h>
#include <cmath>
#include "WiFiConfig.h"

// NTP設定 (JST: UTC+9)
static const long GMT_OFFSET_SEC = 9 * 3600;
static const int DST_OFFSET_SEC = 0;
const char* NTP_1 = "ntp.nict.jp";
const char* NTP_2 = "pool.ntp.org";

// 色定義
const uint16_t COLOR_BG = 0x5AEB;   // グレー
const uint16_t COLOR_FACE = GREEN;
const uint16_t COLOR_TEXT = WHITE;

float sx = 0, sy = 1, mx = 1, my = 0, hx = -1, hy = 0;
float sdeg = 0, mdeg = 0, hdeg = 0;
uint16_t osx = 40, osy = 40, omx = 40, omy = 40, ohx = 40, ohy = 40;
uint16_t x0 = 0, x1 = 0, yy0 = 0, yy1 = 0;
uint32_t targetTime = 0;
bool timeSynced = false;

static uint8_t conv2d(const char* p) {
  uint8_t v = 0;
  if ('0' <= *p && *p <= '9') v = *p - '0';
  return 10 * v + *++p - '0';
}

uint8_t hh = conv2d(__TIME__), mm = conv2d(__TIME__ + 3), ss = conv2d(__TIME__ + 6);

void drawClockFace() {
  StickCP2.Display.fillScreen(COLOR_BG);
  StickCP2.Display.setTextColor(COLOR_TEXT, COLOR_BG);

  StickCP2.Display.fillCircle(40, 40, 40, COLOR_FACE);
  StickCP2.Display.fillCircle(40, 40, 36, BLACK);

  for (int i = 0; i < 360; i += 30) {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * 38 + 40;
    yy0 = sy * 38 + 40;
    x1 = sx * 32 + 40;
    yy1 = sy * 32 + 40;
    StickCP2.Display.drawLine(x0, yy0, x1, yy1, COLOR_FACE);
  }

  for (int i = 0; i < 360; i += 6) {
    sx = cos((i - 90) * 0.0174532925);
    sy = sin((i - 90) * 0.0174532925);
    x0 = sx * 34 + 40;
    yy0 = sy * 34 + 40;
    StickCP2.Display.drawPixel(x0, yy0, COLOR_TEXT);
    if (i == 0 || i == 180) StickCP2.Display.fillCircle(x0, yy0, 2, COLOR_TEXT);
    if (i == 90 || i == 270) StickCP2.Display.fillCircle(x0, yy0, 2, COLOR_TEXT);
  }
  StickCP2.Display.fillCircle(40, 40, 2, COLOR_TEXT);
}

void drawDigitalClock() {
  auto t = StickCP2.Rtc.getTime();
  StickCP2.Display.fillRect(0, 110, 80, 30, COLOR_BG);
  StickCP2.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  StickCP2.Display.setTextSize(2);
  StickCP2.Display.setCursor(5, 115);
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

  StickCP2.Display.drawLine(osx, osy, 40, 40, BLACK);
  osx = sx * 25 + 40;
  osy = sy * 25 + 40;
  StickCP2.Display.drawLine(osx, osy, 40, 40, RED);

  StickCP2.Display.drawLine(ohx, ohy, 40, 40, BLACK);
  ohx = hx * 15 + 40;
  ohy = hy * 15 + 40;
  StickCP2.Display.drawLine(ohx, ohy, 40, 40, WHITE);

  StickCP2.Display.drawLine(omx, omy, 40, 40, BLACK);
  omx = mx * 20 + 40;
  omy = my * 20 + 40;
  StickCP2.Display.drawLine(omx, omy, 40, 40, WHITE);

  StickCP2.Display.fillCircle(40, 40, 2, RED);
}

bool connectWiFi(uint8_t retries = 8) {
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  for (uint8_t i = 0; i < retries; ++i) {
    if (WiFi.status() == WL_CONNECTED) return true;
    StickCP2.Display.fillRect(0, 70, 160, 12, COLOR_BG);
    StickCP2.Display.setCursor(0, 70);
    StickCP2.Display.setTextSize(1);
    StickCP2.Display.printf("WiFi... (%d/%d)", i + 1, retries);
    delay(700);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool syncTimeFromNtp() {
  // システム時刻を一旦クリアして、前回のローカルオフセットが残らないようにする
  struct timeval tv = {0};
  settimeofday(&tv, nullptr);

  // まず UTC で同期し、取得後に JST (+9h) を手動で適用する
  configTime(0, 0, NTP_1, NTP_2);
  for (int i = 0; i < 40; ++i) { // 最大約20秒待つ
    time_t now = time(nullptr);
    if (now > 1700000000) { // 2023年以降なら有効
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

void setup() {
  auto cfg = M5.config();
  StickCP2.begin(cfg);
  StickCP2.Display.setRotation(1);
  StickCP2.Display.setTextColor(COLOR_TEXT, COLOR_BG);
  StickCP2.Display.setTextSize(1);

  drawClockFace();
  StickCP2.Display.setCursor(0, 90);
  StickCP2.Display.println("WiFi to NTP...");

  bool wifiOk = connectWiFi();
  if (wifiOk) {
    StickCP2.Display.setCursor(0, 100);
    StickCP2.Display.println("Sync time...");
    timeSynced = syncTimeFromNtp();
    WiFi.disconnect(true);
    WiFi.mode(WIFI_OFF);
  } else {
    StickCP2.Display.setCursor(0, 100);
    StickCP2.Display.println("WiFi failed");
  }

  targetTime = millis() + 1000;
  updateHands();
  drawDigitalClock();

  if (timeSynced) {
    StickCP2.Display.setCursor(0, 140);
    StickCP2.Display.println("Synced via NTP");
  } else {
    StickCP2.Display.setCursor(0, 140);
    StickCP2.Display.println("Using build time");
  }
}

void loop() {
  StickCP2.update();
  if (targetTime < millis()) {
    targetTime += 1000;
    updateHands();
    drawDigitalClock();
  }
}
