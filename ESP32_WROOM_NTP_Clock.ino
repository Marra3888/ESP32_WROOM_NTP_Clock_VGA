#include <ESP32Lib.h>
#include <WiFi.h>
#include <time.h>
#include <string.h>
#include <Adafruit_GFX.h>
#include <Fonts/FreeSansBold12pt7b.h>
#include <Fonts/FreeSansBold18pt7b.h>
#include <Fonts/FreeSansBoldOblique24pt7b.h>

// =====================================================
// VGA pins
// =====================================================
const int redPin   = 27;
const int greenPin = 26;
const int bluePin  = 23;
const int hsyncPin = 33;
const int vsyncPin = 32;

VGA3BitI vga;

// =====================================================
// Global canvases (no local dynamic allocation)
// =====================================================
GFXcanvas1 canvasText(640, 80);   // messages
GFXcanvas1 canvasWeek(640, 40);   // weekday
GFXcanvas1 canvasDate(640, 45);   // date

GFXcanvas1 canvasMsgTop(640, 70);
GFXcanvas1 canvasMsgBottom(640, 70);

// =====================================================
// Wi-Fi
// =====================================================
const char* ssidList[] = {
  "TP-LINK_333856",
  "TP-Link_22DC"
};

const char* passList[] = {
  "",
  "Mar4enko2704"
};

const int wifiCount = sizeof(ssidList) / sizeof(ssidList[0]);

// =====================================================
// Timezone Kyiv
// =====================================================
const char* TZ_INFO = "EET-2EEST,M3.5.0/3,M10.5.0/4";

// =====================================================
// Timing
// =====================================================
unsigned long prevTickMillis = 0;
unsigned long prevNtpSyncMillis = 0;
const unsigned long ntpResyncPeriod = 6UL * 60UL * 60UL * 1000UL;

bool flasher = false;
bool timeValid = false;

// =====================================================
// Colors
// =====================================================
uint16_t BLACK;
uint16_t RED;
uint16_t GREEN;
uint16_t BLUE;
uint16_t YELLOW;
uint16_t CYAN;
uint16_t MAGENTA;
uint16_t WHITE;
uint16_t ORANGE;
uint16_t DARKGREEN;

// =====================================================
// MODE 0: WITH SECONDS
// =====================================================
const int yBaseSec = 80;

const int X_H1_S = 20;
const int X_H2_S = 110;
const int X_M1_S = 260;
const int X_M2_S = 350;
const int X_S1_S = 500;
const int X_S2_S = 565;

const int HM_W_S = 30;
const int HM_H_S = 10;
const int SS_W_S = 20;
const int SS_H_S = 7;

const int COLON_X_S = 220;
const int COLON_Y1_S = 95;
const int COLON_Y2_S = COLON_Y1_S + 85;
const int COLON_R_S = 8;

// =====================================================
// MODE 1: NO SECONDS, BIGGER FONT, HIGHER
// =====================================================
const int yBaseBig = 45;

const int X_H1_B = 50;
const int X_H2_B = 170;
const int X_M1_B = 370;
const int X_M2_B = 490;

const int HM_W_B = 40;
const int HM_H_B = 12;

const int COLON_X_B = 310;
const int COLON_Y1_B = 85;
const int COLON_Y2_B = COLON_Y1_B + 85;
const int COLON_R_B = 10;

// =====================================================
// Text positions
// =====================================================
const int INFO_Y_WEEK = 255;
const int INFO_Y_DATE = 310;

// =====================================================
// Global yBase for segment draw
// =====================================================
int yBase = yBaseSec;

// =====================================================
// Cache
// =====================================================
int lastMode = -1;
int lastH1 = -1;
int lastH2 = -1;
int lastM1 = -1;
int lastM2 = -1;
int lastS1 = -1;
int lastS2 = -1;
int lastDay = -1;
int lastMonth = -1;
int lastYear = -1;
int lastWDay = -1;
bool lastColonState = false;

// =====================================================
// Weekdays
// =====================================================
const char* weekDaysCyr[] = {
  "VOSKRESENIE",
  "PONEDELNIK",
  "VTORNIK",
  "SREDA",
  "CHETVERG",
  "PIATNICA",
  "SUBBOTA"
};

// =====================================================
// Helpers
// =====================================================
void clearArea(int x, int y, int w, int h) {
  vga.fillRect(x, y, w, h, BLACK);
}

void resetDrawCache() {
  lastMode = -1;
  lastH1 = lastH2 = lastM1 = lastM2 = lastS1 = lastS2 = -1;
  lastDay = lastMonth = lastYear = lastWDay = -1;
  lastColonState = !flasher;
}

void drawCanvasToVGA(GFXcanvas1 &canvas, int dstX, int dstY, uint16_t color, bool transparent = true) {
  for (int y = 0; y < canvas.height(); y++) {
    for (int x = 0; x < canvas.width(); x++) {
      if (canvas.getPixel(x, y)) {
        vga.dotFast(dstX + x, dstY + y, color);
      } else if (!transparent) {
        vga.dotFast(dstX + x, dstY + y, BLACK);
      }
    }
  }
}

void drawCenteredTextOnCanvas(
  GFXcanvas1 &canvas,
  const char* txt,
  int dstY,
  uint16_t color,
  const GFXfont *font
) {
  canvas.fillScreen(0);
  canvas.setTextWrap(false);
  canvas.setFont(font);

  int16_t x1, y1;
  uint16_t w, h;
  canvas.getTextBounds((char*)txt, 0, 0, &x1, &y1, &w, &h);

  int x = (canvas.width() - w) / 2 - x1;
  int baseline = -y1;

  canvas.setCursor(x, baseline);
  canvas.print(txt);

  clearArea(0, dstY, canvas.width(), canvas.height());
  drawCanvasToVGA(canvas, 0, dstY, color, true);
}

void drawCenteredText(const char* txt, int y, uint16_t color) {
  drawCenteredTextOnCanvas(canvasText, txt, y, color, &FreeSansBold18pt7b);
}

void drawWeekdayCentered(int wday, uint16_t color) {
  drawCenteredTextOnCanvas(canvasWeek, weekDaysCyr[wday], INFO_Y_WEEK, color, &FreeSansBold12pt7b);
}

void drawDateCentered(int day, int month, int year, uint16_t color) {
  char buf[16];
  snprintf(buf, sizeof(buf), "%02d/%02d/%04d", day, month, year);
  drawCenteredTextOnCanvas(canvasDate, buf, INFO_Y_DATE, color, &FreeSansBoldOblique24pt7b);
}

// =====================================================
// 7-segment draw
// =====================================================
void drawSegment(uint16_t seg, int x, int w, int h, uint16_t color) {
  int x1 = x;
  int x2 = x1 + w;
  int x3 = x2 + w;

  int y1 = yBase;
  int y2 = y1 + w;
  int y3 = y2 + w;
  int y4 = y3 + w;
  int y5 = y4 + w;

  switch (seg) {
    case 0:
      vga.fillEllipse(x2, y1, w, h, color);
      vga.ellipse(x2, y1, w, h, BLACK);
      break;
    case 1:
      vga.fillEllipse(x3, y2, h, w, color);
      vga.ellipse(x3, y2, h, w, BLACK);
      break;
    case 2:
      vga.fillEllipse(x3, y4, h, w, color);
      vga.ellipse(x3, y4, h, w, BLACK);
      break;
    case 3:
      vga.fillEllipse(x2, y5, w, h, color);
      vga.ellipse(x2, y5, w, h, BLACK);
      break;
    case 4:
      vga.fillEllipse(x1, y4, h, w, color);
      vga.ellipse(x1, y4, h, w, BLACK);
      break;
    case 5:
      vga.fillEllipse(x1, y2, h, w, color);
      vga.ellipse(x1, y2, h, w, BLACK);
      break;
    case 6:
      vga.fillEllipse(x2, y3, w, h, color);
      vga.ellipse(x2, y3, w, h, BLACK);
      break;
  }
}

void drawDigit(int digit, int x, int w, int h, uint16_t color) {
  if (digit != 1 && digit != 4) drawSegment(0, x, w, h, color);
  if (digit != 5 && digit != 6) drawSegment(1, x, w, h, color);
  if (digit != 2) drawSegment(2, x, w, h, color);
  if (digit != 1 && digit != 4 && digit != 7) drawSegment(3, x, w, h, color);
  if (digit == 0 || digit == 2 || digit == 6 || digit == 8) drawSegment(4, x, w, h, color);
  if (digit != 1 && digit != 2 && digit != 3 && digit != 7) drawSegment(5, x, w, h, color);
  if (digit != 0 && digit != 1 && digit != 7) drawSegment(6, x, w, h, color);
}

// =====================================================
// Colon
// =====================================================
void drawColonSecondsMode(bool on, uint16_t color) {
  uint16_t c = on ? color : BLACK;
  vga.fillEllipse(COLON_X_S, COLON_Y1_S, COLON_R_S, COLON_R_S, c);
  vga.fillEllipse(COLON_X_S, COLON_Y2_S, COLON_R_S, COLON_R_S, c);
}

void drawColonBigMode(bool on, uint16_t color) {
  uint16_t c = on ? color : BLACK;
  vga.fillEllipse(COLON_X_B, COLON_Y1_B, COLON_R_B, COLON_R_B, c);
  vga.fillEllipse(COLON_X_B, COLON_Y2_B, COLON_R_B, COLON_R_B, c);
}

// =====================================================
// Clear digit areas
// =====================================================
void clearHmSecModeArea(int x) {
  int width = 3 * HM_W_S + 30;
  int height = 5 * HM_W_S + 36;
  clearArea(x - 16, yBaseSec - 18, width, height);
}

void clearSsSecModeArea(int x) {
  int width = 3 * SS_W_S + 24;
  int height = 5 * SS_W_S + 28;
  clearArea(x - 10, yBaseSec - 14, width, height);
}

void clearBigModeArea(int x) {
  int width = 3 * HM_W_B + 34;
  int height = 5 * HM_W_B + 42;
  clearArea(x - 18, yBaseBig - 22, width, height);
}

// =====================================================
// Screens
// =====================================================
void drawNoTimeScreen() {
  vga.clear(BLACK);
  drawCenteredText("NET VREMENI", 150, RED);
  drawCenteredText("WIFI / NTP...", 210, WHITE);
}

//zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz


// =====================================================
// Wi-Fi
// =====================================================
bool connectToWiFi(uint32_t timeoutPerNetwork = 10000) {
  WiFi.mode(WIFI_STA);
  WiFi.persistent(false);
  WiFi.setAutoReconnect(true);

  for (int i = 0; i < wifiCount; i++) {
    vga.clear(BLACK);

    drawCenteredTextOnCanvas(canvasMsgTop, "WiFi...", 40, CYAN, &FreeSansBold18pt7b);
    drawCenteredTextOnCanvas(canvasMsgBottom, ssidList[i], 130, WHITE, &FreeSansBold12pt7b);

    delay(1000);

    WiFi.disconnect(true, true);
    delay(300);
    WiFi.begin(ssidList[i], passList[i]);

    unsigned long startAttempt = millis();

    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < timeoutPerNetwork) {
      delay(50);
    }

    if (WiFi.status() == WL_CONNECTED) {
      vga.clear(BLACK);
      drawCenteredTextOnCanvas(canvasMsgTop, "WiFi Ok!", 40, GREEN, &FreeSansBold18pt7b);
      drawCenteredTextOnCanvas(canvasMsgBottom, ssidList[i], 130, WHITE, &FreeSansBold12pt7b);
      delay(2000);
      return true;
    }
  }

  vga.clear(BLACK);
  drawCenteredTextOnCanvas(canvasMsgTop, "No WiFi", 80, RED, &FreeSansBold18pt7b);
  delay(2000);
  return false;
}

// =====================================================
// NTP
// =====================================================
bool syncTimeFromNTP(uint32_t timeoutMs = 15000) {
  configTime(0, 0, "pool.ntp.org", "time.nist.gov", "time.google.com");
  setenv("TZ", TZ_INFO, 1);
  tzset();

  struct tm timeinfo;
  unsigned long startAttempt = millis();

  while (millis() - startAttempt < timeoutMs) {
    if (getLocalTime(&timeinfo, 1000)) {
      timeValid = true;
      prevNtpSyncMillis = millis();
      return true;
    }
    delay(200);
  }

  return false;
}

// =====================================================
// Full redraws
// =====================================================
void drawModeSecondsFull(int h1, int h2, int m1, int m2, int s1, int s2, int wday, int day, int month, int year) {
  vga.clear(BLACK);

  yBase = yBaseSec;

  drawDigit(h1, X_H1_S, HM_W_S, HM_H_S, GREEN);
  drawDigit(h2, X_H2_S, HM_W_S, HM_H_S, GREEN);
  drawDigit(m1, X_M1_S, HM_W_S, HM_H_S, GREEN);
  drawDigit(m2, X_M2_S, HM_W_S, HM_H_S, GREEN);
  drawDigit(s1, X_S1_S, SS_W_S, SS_H_S, CYAN);
  drawDigit(s2, X_S2_S, SS_W_S, SS_H_S, CYAN);

  drawColonSecondsMode(flasher, WHITE);

  drawWeekdayCentered(wday, WHITE);
  drawDateCentered(day, month, year, ORANGE);
}

void drawModeBigNoSecondsFull(int h1, int h2, int m1, int m2, int wday, int day, int month, int year) {
  vga.clear(BLACK);

  yBase = yBaseBig;

  drawDigit(h1, X_H1_B, HM_W_B, HM_H_B, GREEN);
  drawDigit(h2, X_H2_B, HM_W_B, HM_H_B, GREEN);
  drawDigit(m1, X_M1_B, HM_W_B, HM_H_B, GREEN);
  drawDigit(m2, X_M2_B, HM_W_B, HM_H_B, GREEN);

  drawColonBigMode(flasher, WHITE);

  drawWeekdayCentered(wday, WHITE);
  drawDateCentered(day, month, year, ORANGE);
}

// =====================================================
// Clock draw
// =====================================================
void drawClock() {
  struct tm timeinfo;

  if (!getLocalTime(&timeinfo, 100)) {
    if (!timeValid) drawNoTimeScreen();
    return;
  }

  timeValid = true;

  int hh = timeinfo.tm_hour;
  int mm = timeinfo.tm_min;
  int ss = timeinfo.tm_sec;

  int h1 = hh / 10;
  int h2 = hh % 10;
  int m1 = mm / 10;
  int m2 = mm % 10;
  int s1 = ss / 10;
  int s2 = ss % 10;

  int day   = timeinfo.tm_mday;
  int month = timeinfo.tm_mon + 1;
  int year  = timeinfo.tm_year + 1900;
  int wday  = timeinfo.tm_wday;

  int mode = (mm / 2) % 2;

  if (mode != lastMode) {
    resetDrawCache();
    lastMode = mode;
  }

  if (mode == 0) {
    if (lastH1 == -1) {
      drawModeSecondsFull(h1, h2, m1, m2, s1, s2, wday, day, month, year);
      lastH1 = h1; lastH2 = h2; lastM1 = m1; lastM2 = m2; lastS1 = s1; lastS2 = s2;
      lastDay = day; lastMonth = month; lastYear = year; lastWDay = wday;
      lastColonState = flasher;
      return;
    }

    yBase = yBaseSec;

    if (h1 != lastH1) {
      clearHmSecModeArea(X_H1_S);
      drawDigit(h1, X_H1_S, HM_W_S, HM_H_S, GREEN);
      lastH1 = h1;
    }
    if (h2 != lastH2) {
      clearHmSecModeArea(X_H2_S);
      drawDigit(h2, X_H2_S, HM_W_S, HM_H_S, GREEN);
      lastH2 = h2;
    }
    if (m1 != lastM1) {
      clearHmSecModeArea(X_M1_S);
      drawDigit(m1, X_M1_S, HM_W_S, HM_H_S, GREEN);
      lastM1 = m1;
    }
    if (m2 != lastM2) {
      clearHmSecModeArea(X_M2_S);
      drawDigit(m2, X_M2_S, HM_W_S, HM_H_S, GREEN);
      lastM2 = m2;
    }
    if (s1 != lastS1) {
      clearSsSecModeArea(X_S1_S);
      drawDigit(s1, X_S1_S, SS_W_S, SS_H_S, CYAN);
      lastS1 = s1;
    }
    if (s2 != lastS2) {
      clearSsSecModeArea(X_S2_S);
      drawDigit(s2, X_S2_S, SS_W_S, SS_H_S, CYAN);
      lastS2 = s2;
    }
    if (day != lastDay || month != lastMonth || year != lastYear) {
      drawDateCentered(day, month, year, YELLOW);
      lastDay = day;
      lastMonth = month;
      lastYear = year;
    }
    if (wday != lastWDay) {
      drawWeekdayCentered(wday, WHITE);
      lastWDay = wday;
    }
    if (flasher != lastColonState) {
      drawColonSecondsMode(flasher, WHITE);
      lastColonState = flasher;
    }
  } else {
    if (lastH1 == -1) {
      drawModeBigNoSecondsFull(h1, h2, m1, m2, wday, day, month, year);
      lastH1 = h1; lastH2 = h2; lastM1 = m1; lastM2 = m2;
      lastDay = day; lastMonth = month; lastYear = year; lastWDay = wday;
      lastColonState = flasher;
      return;
    }

    yBase = yBaseBig;

    if (h1 != lastH1) {
      clearBigModeArea(X_H1_B);
      drawDigit(h1, X_H1_B, HM_W_B, HM_H_B, GREEN);
      lastH1 = h1;
    }
    if (h2 != lastH2) {
      clearBigModeArea(X_H2_B);
      drawDigit(h2, X_H2_B, HM_W_B, HM_H_B, GREEN);
      lastH2 = h2;
    }
    if (m1 != lastM1) {
      clearBigModeArea(X_M1_B);
      drawDigit(m1, X_M1_B, HM_W_B, HM_H_B, GREEN);
      lastM1 = m1;
    }
    if (m2 != lastM2) {
      clearBigModeArea(X_M2_B);
      drawDigit(m2, X_M2_B, HM_W_B, HM_H_B, GREEN);
      lastM2 = m2;
    }
    if (day != lastDay || month != lastMonth || year != lastYear) {
      drawDateCentered(day, month, year, YELLOW);
      lastDay = day;
      lastMonth = month;
      lastYear = year;
    }
    if (wday != lastWDay) {
      drawWeekdayCentered(wday, WHITE);
      lastWDay = wday;
    }
    if (flasher != lastColonState) {
      drawColonBigMode(flasher, WHITE);
      lastColonState = flasher;
    }
  }
}

// =====================================================
// Init colors
// =====================================================
void initColors() {
  BLACK     = vga.RGB(0, 0, 0);
  RED       = vga.RGB(255, 0, 0);
  GREEN     = vga.RGB(0, 255, 0);
  BLUE      = vga.RGB(0, 0, 255);
  YELLOW    = vga.RGB(255, 255, 0);
  CYAN      = vga.RGB(0, 255, 255);
  MAGENTA   = vga.RGB(255, 0, 255);
  WHITE     = vga.RGB(255, 255, 255);
  ORANGE    = vga.RGB(255, 140, 0);
  DARKGREEN = vga.RGB(0, 120, 0);
}

// =====================================================
// Setup
// =====================================================
void setup() {
  Serial.begin(115200);
  delay(1000);

  vga.init(vga.MODE640x480.custom(640, 480), redPin, greenPin, bluePin, hsyncPin, vsyncPin);

  initColors();
  vga.clear(BLACK);

  canvasText.setTextWrap(false);
  canvasWeek.setTextWrap(false);
  canvasDate.setTextWrap(false);

  canvasMsgTop.setTextWrap(false);
  canvasMsgBottom.setTextWrap(false);

  // scanWiFiNetworks();

  bool wifiOk = connectToWiFi();

  if (wifiOk) {
    syncTimeFromNTP(15000);
  }

  vga.clear(BLACK);
  resetDrawCache();
  drawClock();
  prevTickMillis = millis();
}

// =====================================================
// Loop
// =====================================================
void loop() {
  unsigned long nowMs = millis();

  if (nowMs - prevTickMillis >= 500) {
    prevTickMillis += 500;
    flasher = !flasher;
    drawClock();
  }

  if (WiFi.status() != WL_CONNECTED) {
    bool wifiOk = connectToWiFi(5000);

    if (wifiOk) {
      syncTimeFromNTP(5000);
    }

    vga.clear(BLACK);
    resetDrawCache();
    drawClock();
  }

  if (WiFi.status() == WL_CONNECTED && (nowMs - prevNtpSyncMillis >= ntpResyncPeriod)) {
    syncTimeFromNTP(5000);
  }
}