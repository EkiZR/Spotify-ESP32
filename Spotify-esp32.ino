#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <U8g2lib.h>

// ════════════════════════════════════════════════════════
//  KONFIGURASI
// ════════════════════════════════════════════════════════
const char* WIFI_SSID = "22194 (HUAWEI)";
const char* WIFI_PASS = "ABLHJJO7rR";
const char* SERVER_URL = "http://192.168.3.7:3000/now-playing";

// ════════════════════════════════════════════════════════
//  HARDWARE
// ════════════════════════════════════════════════════════
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, U8X8_PIN_NONE);

// ════════════════════════════════════════════════════════
//  BITMAP LOGO
// ════════════════════════════════════════════════════════
const unsigned char spotify_10[] U8X8_PROGMEM = {
  0x3C, 0x00, 0x7E, 0x00, 0x81, 0x00, 0xFF, 0x00, 0xC3, 0x00, 0xFF, 0x00,
  0xE7, 0x00, 0xFF, 0x00, 0x7E, 0x00, 0x3C, 0x00
};

const unsigned char logo_spotify_32[] U8X8_PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0xF8, 0x1F, 0x00, 0x00, 0xFF, 0xFF, 0x00,
  0xC0, 0xFF, 0xFF, 0x03, 0xE0, 0xFF, 0xFF, 0x07, 0xF0, 0xFF, 0xFF, 0x0F,
  0xF8, 0xFF, 0xFF, 0x1F, 0xFC, 0xFF, 0xFF, 0x3F, 0xFC, 0x07, 0x80, 0x3F,
  0xFE, 0x01, 0x00, 0x7F, 0xFE, 0x00, 0x00, 0x7E, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x1F, 0xF8, 0xFF,
  0xFF, 0x07, 0xE0, 0xFF, 0xFF, 0x01, 0x80, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
  0xFE, 0xFF, 0xFF, 0x7F, 0xFE, 0x3F, 0xFC, 0x7F, 0xFC, 0x0F, 0xE0, 0x3F,
  0xFC, 0xFF, 0xFF, 0x3F, 0xF8, 0xFF, 0xFF, 0x1F, 0xF0, 0xFF, 0xFF, 0x0F,
  0xE0, 0xFF, 0xFF, 0x07, 0xC0, 0xFF, 0xFF, 0x03, 0x00, 0xFF, 0xFF, 0x00,
  0x00, 0xF8, 0x1F, 0x00, 0x00, 0x00, 0x00, 0x00
};

// ════════════════════════════════════════════════════════
//  STATE & VARIABEL
// ════════════════════════════════════════════════════════
String currTitle = "";
String currArtist = "";
String lyricCurr = "";
String lyricNext = "";
bool playing = false;

long serverPositionMs = 0;
long durationMs = 1;
unsigned long fetchedAt = 0;

long getPositionMs() {
  if (!playing) return serverPositionMs;
  long local = serverPositionMs + (long)(millis() - fetchedAt);
  return min(local, durationMs);
}

int screenMode = 0;

// ════════════════════════════════════════════════════════
//  EQ BARS
// ════════════════════════════════════════════════════════
#define EQ_N 8
#define EQ_BAR_W 4
#define EQ_GAP 2
#define EQ_X 82
#define EQ_MAX_H 13

int eqH[EQ_N] = { 3, 6, 10, 5, 12, 4, 8, 6 };
int eqTgt[EQ_N] = { 3, 6, 10, 5, 12, 4, 8, 6 };
unsigned long lastEqTick = 0;

void eqUpdateTargets() {
  for (int i = 0; i < EQ_N; i++)
    eqTgt[i] = playing ? random(4, EQ_MAX_H + 1) : random(1, 3);
}

void eqSmooth() {
  for (int i = 0; i < EQ_N; i++) {
    if (eqH[i] < eqTgt[i]) eqH[i]++;
    else if (eqH[i] > eqTgt[i]) eqH[i]--;
  }
}

// ════════════════════════════════════════════════════════
//  SCROLL
// ════════════════════════════════════════════════════════
int titleX = 130;
int titlePause = 40;
int lyricX = 2;
int lyricPause = 40;
unsigned long lastScroll = 0;

void updateScroll() {
  u8g2.setFont(u8g2_font_7x13B_tr);
  int tw = u8g2.getUTF8Width(currTitle.c_str());
  if (tw <= 128) {
    titleX = 0;
  } else {
    if (titlePause > 0) titlePause--;
    else {
      titleX--;
      if (titleX < -(tw + 12)) {
        titleX = 130;
        titlePause = 40;
      } else if (titleX == 0) titlePause = 40;
    }
  }

  u8g2.setFont(u8g2_font_5x8_tr);
  int lw = u8g2.getUTF8Width(lyricCurr.c_str());
  if (lw <= 124) {
    lyricX = 2;
  } else {
    if (lyricPause > 0) lyricPause--;
    else {
      lyricX--;
      if (lyricX < -(lw + 12)) {
        lyricX = 124;
        lyricPause = 40;
      } else if (lyricX == 2) lyricPause = 40;
    }
  }
}

// ════════════════════════════════════════════════════════
//  FETCH
// ════════════════════════════════════════════════════════
unsigned long lastFetch = 0;

void fetchData() {
  if (WiFi.status() != WL_CONNECTED) return;
  HTTPClient http;
  http.begin(SERVER_URL);
  http.setTimeout(2000);
  int code = http.GET();
  if (code == 200) {
    StaticJsonDocument<768> doc;
    if (!deserializeJson(doc, http.getString())) {
      String newTitle = String(doc["title"] | "");
      if (newTitle != currTitle) {
        currTitle = newTitle;
        titleX = 130;
        titlePause = 40;
      }
      currArtist = String(doc["artist"] | "");
      playing = doc["playing"] | false;
      serverPositionMs = doc["position_ms"] | 0;
      durationMs = doc["duration_ms"] | 1;
      fetchedAt = millis();

      String newLyricCurr = String(doc["lyric_curr"] | "");
      if (newLyricCurr != lyricCurr) {
        lyricCurr = newLyricCurr;
        lyricX = 2;
        lyricPause = 40;
      }
      lyricNext = String(doc["lyric_next"] | "");
      screenMode = (lyricCurr.length() > 0) ? 1 : 0;
    }
  }
  http.end();
}

// ════════════════════════════════════════════════════════
//  DRAW HELPERS
// ════════════════════════════════════════════════════════
void drawCenterText(const char* text, int y) {
  int w = u8g2.getUTF8Width(text);
  u8g2.drawStr((128 - w) / 2, y, text);
}

String clipStr(const String& s, int maxPx) {
  if (u8g2.getUTF8Width(s.c_str()) <= maxPx) return s;
  String out = s;
  while (out.length() > 0 && u8g2.getUTF8Width((out + "...").c_str()) > maxPx)
    out.remove(out.length() - 1);
  return out + "...";
}

void drawHeader(String headerText) {
  u8g2.drawXBMP(2, 2, 10, 10, spotify_10);
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(16, 9, headerText.c_str());
  for (int i = 0; i < EQ_N; i++) {
    int bh = constrain(eqH[i], 1, EQ_MAX_H);
    u8g2.drawBox(EQ_X + i * (EQ_BAR_W + EQ_GAP), 13 - bh, EQ_BAR_W, bh);
  }
  u8g2.drawHLine(0, 14, 128);
}

// ════════════════════════════════════════════════════════
//  ANIMASI MATA — VARIABEL
// ════════════════════════════════════════════════════════
#define L_CX 35
#define R_CX 93
#define EYE_CY 29
#define BASE_RX 13
#define BASE_RY 12

float lOpenR = 1.0f, rOpenR = 1.0f;
float tLOpenR = 1.0f, tROpenR = 1.0f;
float lRx = BASE_RX, lRy = BASE_RY;
float tLRx = BASE_RX, tLRy = BASE_RY;
float rRx = BASE_RX, rRy = BASE_RY;
float tRRx = BASE_RX, tRRy = BASE_RY;
float pupX = 0.0f, pupY = 0.0f;
float tPupX = 0.0f, tPupY = 0.0f;
float lBH = 3.0f, rBH = 3.0f;
float tLBH = 3.0f, tRBH = 3.0f;
float lBT = 0.0f, rBT = 0.0f;
float tLBT = 0.0f, tRBT = 0.0f;
bool lHappy = false, rHappy = false;
unsigned long nextEyeChange = 0;

// ════════════════════════════════════════════════════════
//  DRAW: SATU BOLA MATA
// ════════════════════════════════════════════════════════
void drawEye(int cx, int cy, float rxF, float ryF, float openR,
             float px, float py, bool happy) {
  int rx = constrain((int)rxF, 3, 18);
  int ry = constrain((int)ryF, 2, 16);

  if (happy) {
    int hapRy = ry + 4;
    u8g2.setDrawColor(1);
    u8g2.drawFilledEllipse(cx, cy, rx, hapRy);
    u8g2.setDrawColor(0);
    u8g2.drawBox(cx - rx - 1, cy, rx * 2 + 2, hapRy + 2);
    u8g2.setDrawColor(1);
    u8g2.drawPixel(cx - 4, cy - (hapRy * 2 / 3));
    u8g2.drawPixel(cx + 5, cy - (hapRy / 2));
    return;
  }

  int effRy = max(1, (int)((float)ry * openR));
  if (effRy <= 2) {
    u8g2.setDrawColor(1);
    int lineW = (int)((float)rx * 1.3f);
    u8g2.drawHLine(cx - lineW / 2, cy, lineW);
    u8g2.drawHLine(cx - lineW / 2, cy + 1, lineW);
    return;
  }

  u8g2.setDrawColor(1);
  u8g2.drawFilledEllipse(cx, cy, rx, effRy);

  int pr = max(2, (rx + effRy) / 5);
  int pcx = constrain(cx + (int)px, cx - rx + pr + 2, cx + rx - pr - 2);
  int pcy = constrain(cy + (int)py, cy - effRy + pr + 2, cy + effRy - pr - 2);

  u8g2.setDrawColor(0);
  u8g2.drawDisc(pcx, pcy, pr);
  u8g2.setDrawColor(1);
  if (pr >= 3) u8g2.drawPixel(pcx - 1, pcy - 1);
  if (pr >= 4) u8g2.drawPixel(pcx - 2, pcy - 2);
  u8g2.setDrawColor(1);
}

// ════════════════════════════════════════════════════════
//  DRAW: ALIS
// ════════════════════════════════════════════════════════
void drawBrow(int cx, int eyeTopY, int rx, float bh, float bt, bool isLeft) {
  int baseY = constrain(eyeTopY - 2 - (int)bh, 1, 58);
  int hw = rx + 1;
  int td = constrain((int)(bt * 3.5f), -9, 9);

  int xl = cx - hw, xr = cx + hw;
  int yl = isLeft ? baseY : (baseY - td);
  int yr = isLeft ? (baseY - td) : baseY;
  yl = constrain(yl, 1, 62);
  yr = constrain(yr, 1, 62);

  u8g2.setDrawColor(1);
  u8g2.drawLine(xl, yl, xr, yr);
  u8g2.drawLine(xl + 2, yl - 1, xr - 2, yr - 1);

  int xm1 = cx - hw / 2, xm2 = cx + hw / 2;
  float tl = (float)(xm1 - xl) / (float)(xr - xl);
  float tr = (float)(xm2 - xl) / (float)(xr - xl);
  int ym1 = constrain(yl + (int)((yr - yl) * tl) - 2, 1, 62);
  int ym2 = constrain(yl + (int)((yr - yl) * tr) - 2, 1, 62);
  u8g2.drawLine(xm1, ym1, xm2, ym2);
}

// ════════════════════════════════════════════════════════
//  LOGIKA EKSPRESI MATA
// ════════════════════════════════════════════════════════
void updateIdleLogic() {
  unsigned long now = millis();
  if (now > nextEyeChange) {
    lHappy = false;
    rHappy = false;
    int r = random(100);

    if (r < 8) {  // KEDIP
      lRx = BASE_RX;
      lRy = BASE_RY;
      rRx = BASE_RX;
      rRy = BASE_RY;
      lOpenR = 0.0f;
      rOpenR = 0.0f;
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tLRx = BASE_RX;
      tLRy = BASE_RY;
      tRRx = BASE_RX;
      tRRy = BASE_RY;
      tPupX = 0.0f;
      tPupY = 0.0f;
      tLBH = 3.0f;
      tRBH = 3.0f;
      tLBT = 0.0f;
      tRBT = 0.0f;
      nextEyeChange = now + random(1500, 3000);

    } else if (r < 15) {  // WINK
      bool wL = (random(2) == 0);
      if (wL) {
        lRx = BASE_RX;
        lRy = BASE_RY;
        lOpenR = 0.0f;
        tLOpenR = 1.0f;
        tROpenR = 1.0f;
      } else {
        rRx = BASE_RX;
        rRy = BASE_RY;
        rOpenR = 0.0f;
        tROpenR = 1.0f;
        tLOpenR = 1.0f;
      }
      tLRx = BASE_RX;
      tLRy = BASE_RY;
      tRRx = BASE_RX;
      tRRy = BASE_RY;
      tPupX = 0.0f;
      tPupY = 0.0f;
      tLBH = 3.0f;
      tRBH = 3.0f;
      tLBT = 0.0f;
      tRBT = 0.0f;
      nextEyeChange = now + random(700, 1500);

    } else if (r < 28) {  // LIRIK KIRI
      tPupX = -5.0f;
      tPupY = 0.0f;
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tLRx = BASE_RX;
      tLRy = BASE_RY;
      tRRx = BASE_RX - 3;
      tRRy = BASE_RY;
      tLBH = 3.0f;
      tRBH = 3.0f;
      tLBT = 0.0f;
      tRBT = 0.0f;
      nextEyeChange = now + random(900, 2000);

    } else if (r < 41) {  // LIRIK KANAN
      tPupX = 5.0f;
      tPupY = 0.0f;
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tLRx = BASE_RX - 3;
      tLRy = BASE_RY;
      tRRx = BASE_RX;
      tRRy = BASE_RY;
      tLBH = 3.0f;
      tRBH = 3.0f;
      tLBT = 0.0f;
      tRBT = 0.0f;
      nextEyeChange = now + random(900, 2000);

    } else if (r < 53) {  // BERPIKIR
      tPupX = 0.0f;
      tPupY = -3.5f;
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tLRx = BASE_RX;
      tLRy = BASE_RY;
      tRRx = BASE_RX;
      tRRy = BASE_RY;
      tLBH = 7.0f;
      tRBH = 7.0f;
      tLBT = 1.5f;
      tRBT = 1.5f;
      nextEyeChange = now + random(1200, 2500);

    } else if (r < 63) {  // NGANTUK
      tPupX = 0.0f;
      tPupY = 2.5f;
      tLOpenR = 0.35f;
      tROpenR = 0.35f;
      tLRx = BASE_RX;
      tLRy = BASE_RY;
      tRRx = BASE_RX;
      tRRy = BASE_RY;
      tLBH = 1.5f;
      tRBH = 1.5f;
      tLBT = -1.2f;
      tRBT = -1.2f;
      nextEyeChange = now + random(2000, 4000);

    } else if (r < 72) {  // KAGET
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tLRx = BASE_RX + 3;
      tLRy = BASE_RY + 3;
      tRRx = BASE_RX + 3;
      tRRy = BASE_RY + 3;
      tPupX = 0.0f;
      tPupY = 0.0f;
      tLBH = 9.0f;
      tRBH = 9.0f;
      tLBT = 1.8f;
      tRBT = 1.8f;
      nextEyeChange = now + random(600, 1400);

    } else if (r < 81) {  // SENANG
      lHappy = true;
      rHappy = true;
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tLRx = BASE_RX;
      tLRy = BASE_RY;
      tRRx = BASE_RX;
      tRRy = BASE_RY;
      tPupX = 0.0f;
      tPupY = 0.0f;
      tLBH = 5.0f;
      tRBH = 5.0f;
      tLBT = 1.5f;
      tRBT = 1.5f;
      nextEyeChange = now + random(1500, 3000);

    } else if (r < 90) {  // BINGUNG
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tPupX = 0.0f;
      tPupY = 0.0f;
      if (random(2) == 0) {
        tLRx = BASE_RX + 3;
        tLRy = BASE_RY + 2;
        tRRx = BASE_RX - 4;
        tRRy = BASE_RY - 3;
        tLBH = 8.0f;
        tRBH = 2.0f;
        tLBT = 1.5f;
        tRBT = 0.0f;
      } else {
        tLRx = BASE_RX - 4;
        tLRy = BASE_RY - 3;
        tRRx = BASE_RX + 3;
        tRRy = BASE_RY + 2;
        tLBH = 2.0f;
        tRBH = 8.0f;
        tLBT = 0.0f;
        tRBT = 1.5f;
      }
      nextEyeChange = now + random(1500, 2500);

    } else {  // NORMAL
      tPupX = 0.0f;
      tPupY = 0.0f;
      tLOpenR = 1.0f;
      tROpenR = 1.0f;
      tLRx = BASE_RX;
      tLRy = BASE_RY;
      tRRx = BASE_RX;
      tRRy = BASE_RY;
      tLBH = 3.0f;
      tRBH = 3.0f;
      tLBT = 0.0f;
      tRBT = 0.0f;
      nextEyeChange = now + random(1000, 2500);
    }
  }

  // Easing
  lOpenR += (tLOpenR - lOpenR) * 0.20f;
  rOpenR += (tROpenR - rOpenR) * 0.20f;
  lRx += (tLRx - lRx) * 0.22f;
  lRy += (tLRy - lRy) * 0.22f;
  rRx += (tRRx - rRx) * 0.22f;
  rRy += (tRRy - rRy) * 0.22f;
  pupX += (tPupX - pupX) * 0.20f;
  pupY += (tPupY - pupY) * 0.20f;
  lBH += (tLBH - lBH) * 0.15f;
  rBH += (tRBH - rBH) * 0.15f;
  lBT += (tLBT - lBT) * 0.15f;
  rBT += (tRBT - rBT) * 0.15f;
}

// ════════════════════════════════════════════════════════
//  SCREEN: IDLE (MATA ANIMASI)
// ════════════════════════════════════════════════════════
void drawIdle() {
  u8g2.clearBuffer();
  updateIdleLogic();

  int lTopY = EYE_CY - (int)lRy;
  int rTopY = EYE_CY - (int)rRy;
  drawBrow(L_CX, lTopY, (int)lRx, lBH, lBT, true);
  drawBrow(R_CX, rTopY, (int)rRx, rBH, rBT, false);
  drawEye(L_CX, EYE_CY, lRx, lRy, lOpenR, pupX, pupY, lHappy);
  drawEye(R_CX, EYE_CY, rRx, rRy, rOpenR, pupX, pupY, rHappy);

  // Zzz saat ngantuk
  if (lOpenR < 0.50f) {
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(63, EYE_CY + 2, "z");
    u8g2.drawStr(68, EYE_CY - 5, "z");
    u8g2.drawStr(74, EYE_CY - 13, "Z");
  }

  // Teks bawah
  u8g2.setDrawColor(1);
  u8g2.setFont(u8g2_font_4x6_tr);
  drawCenterText("SPOTIFY IS SLEEPING", 62);
  u8g2.drawHLine(4, 58, 18);
  u8g2.drawHLine(106, 58, 18);

  u8g2.sendBuffer();
}

// ════════════════════════════════════════════════════════
//  SCREEN: NOW PLAYING — FIX TIMER
// ════════════════════════════════════════════════════════
void drawNowPlaying() {
  u8g2.clearBuffer();
  drawHeader("now playing");

  // Judul scroll
  u8g2.setFont(u8g2_font_7x13B_tr);
  u8g2.setClipWindow(0, 15, 127, 27);
  u8g2.drawStr(titleX, 27, currTitle.c_str());
  u8g2.setMaxClipWindow();

  // Artist
  u8g2.setFont(u8g2_font_5x7_tr);
  u8g2.setClipWindow(0, 29, 127, 36);
  u8g2.drawStr(0, 36, clipStr(currArtist, 128).c_str());
  u8g2.setMaxClipWindow();
  u8g2.drawHLine(0, 37, 128);

  // Hitung posisi & durasi
  long posMs = getPositionMs();
  char tPos[8], tDur[8];
  {
    int s = (int)(posMs / 1000), m = s / 60;
    s %= 60;
    sprintf(tPos, "%d:%02d", m, s);
  }
  {
    int s = (int)(durationMs / 1000), m = s / 60;
    s %= 60;
    sprintf(tDur, "%d:%02d", m, s);
  }

  int posW = strlen(tPos) * 4;
  int durW = strlen(tDur) * 4;

  // ── Timer teks DULU (sebelum bar supaya tidak tertimpa) ──
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(0, 55, tPos);           // kiri bawah
  u8g2.drawStr(128 - durW, 55, tDur);  // kanan bawah

  // ── Progress bar di tengah antara dua teks ──
  int barX = posW + 3;
  int barE = 128 - durW - 3;
  int barW = barE - barX;

  if (barW > 4) {
    int pw = (durationMs > 0) ? map(posMs, 0, durationMs, 0, barW) : 0;
    pw = constrain(pw, 0, barW);

    // Track line
    u8g2.drawHLine(barX, 50, barW);
    // Fill
    if (pw > 0) u8g2.drawBox(barX, 49, pw, 3);
    // Titik playhead
    u8g2.drawDisc(barX + pw, 50, 2);
  }

  u8g2.sendBuffer();
}

// ════════════════════════════════════════════════════════
//  SCREEN: LYRICS
// ════════════════════════════════════════════════════════
void drawLyrics() {
  u8g2.clearBuffer();
  drawHeader(clipStr(currTitle, 60));

  u8g2.setFont(u8g2_font_5x8_tr);
  int lw = u8g2.getUTF8Width(lyricCurr.c_str());
  int boxWidth = (lw <= 124) ? constrain(lw + 4, 0, 128) : 128;

  u8g2.setDrawColor(1);
  u8g2.drawBox(0, 18, boxWidth, 11);
  u8g2.setDrawColor(0);
  u8g2.setClipWindow(0, 18, 127, 29);
  u8g2.drawStr(lyricX, 27, lyricCurr.c_str());
  u8g2.setMaxClipWindow();
  u8g2.setDrawColor(1);

  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.setClipWindow(0, 33, 127, 42);
  String nextDisplay = ">> " + lyricNext;
  u8g2.drawStr(4, 41, clipStr(nextDisplay, 124).c_str());
  u8g2.setMaxClipWindow();

  u8g2.drawHLine(0, 48, 128);

  long posMs = getPositionMs();
  int pw = (durationMs > 0) ? map(posMs, 0, durationMs, 0, 128) : 0;
  pw = constrain(pw, 0, 128);
  u8g2.drawHLine(0, 51, 128);
  if (pw > 0) u8g2.drawBox(0, 50, pw, 3);

  char tPos[8], tDur[8];
  {
    int s = (int)(posMs / 1000), m = s / 60;
    s %= 60;
    sprintf(tPos, "%d:%02d", m, s);
  }
  {
    int s = (int)(durationMs / 1000), m = s / 60;
    s %= 60;
    sprintf(tDur, "%d:%02d", m, s);
  }
  u8g2.setFont(u8g2_font_4x6_tr);
  u8g2.drawStr(0, 63, tPos);
  u8g2.drawStr(128 - (int)strlen(tDur) * 4, 63, tDur);

  u8g2.sendBuffer();
}

// ════════════════════════════════════════════════════════
//  BOOT: ANIMASI TYPEWRITER
//  Cetak teks karakter per karakter seperti terminal
// ════════════════════════════════════════════════════════
void typewriterLine(const char* text, int x, int y, int delayMs) {
  char buf[64] = "";
  int len = strlen(text);
  for (int i = 0; i < len; i++) {
    strncat(buf, text + i, 1);
    u8g2.setDrawColor(0);
    u8g2.drawBox(x, y - 7, 128, 9);  // hapus area baris
    u8g2.setDrawColor(1);
    u8g2.drawStr(x, y, buf);
    // Kursor berkedip
    int cx = x + u8g2.getUTF8Width(buf);
    u8g2.drawVLine(cx, y - 6, 7);
    u8g2.sendBuffer();
    delay(delayMs);
  }
  // Hapus kursor setelah selesai
  u8g2.setDrawColor(0);
  int cx = x + u8g2.getUTF8Width(buf);
  u8g2.drawVLine(cx, y - 6, 7);
  u8g2.setDrawColor(1);
  u8g2.sendBuffer();
}

// ════════════════════════════════════════════════════════
//  BOOT: ANIMASI BAR MENGISI
// ════════════════════════════════════════════════════════
void animateFillBar(int x, int y, int w, int h, int steps, int stepDelay) {
  for (int i = 0; i <= steps; i++) {
    int fillW = map(i, 0, steps, 0, w);
    u8g2.setDrawColor(1);
    u8g2.drawFrame(x, y, w, h);
    if (fillW > 0) u8g2.drawBox(x, y, fillW, h);
    u8g2.sendBuffer();
    delay(stepDelay);
  }
}

// ════════════════════════════════════════════════════════
//  SETUP — BOOT SEQUENCE ANIMASI
// ════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  Wire.begin(21, 22);
  u8g2.begin();

  // ── FASE 1: Logo masuk dari atas ──
  for (int yy = -32; yy <= 4; yy += 3) {
    u8g2.clearBuffer();
    u8g2.drawXBMP(48, yy, 32, 32, logo_spotify_32);
    u8g2.sendBuffer();
    delay(18);
  }

  // ── FASE 2: Connecting WiFi ──
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  u8g2.setFont(u8g2_font_5x7_tr);
  // Ketik "Connecting..."
  u8g2.clearBuffer();
  u8g2.drawXBMP(48, 4, 32, 32, logo_spotify_32);
  u8g2.sendBuffer();
  typewriterLine("Connecting...", 20, 46, 60);

  // Progress bar WiFi animasi
  u8g2.drawFrame(14, 54, 100, 6);
  u8g2.sendBuffer();

  int loadProgress = 0;
  while (WiFi.status() != WL_CONNECTED && loadProgress < 96) {
    loadProgress += random(3, 7);
    if (loadProgress > 96) loadProgress = 96;
    u8g2.clearBuffer();
    u8g2.drawXBMP(48, 4, 32, 32, logo_spotify_32);
    u8g2.setFont(u8g2_font_5x7_tr);
    drawCenterText("Connecting...", 46);
    u8g2.drawFrame(14, 54, 100, 6);
    u8g2.drawBox(14, 54, loadProgress, 6);
    u8g2.sendBuffer();
    delay(200);
  }

  // Tunggu benar-benar konek
  int timeout = 0;
  while (WiFi.status() != WL_CONNECTED && timeout < 30) {
    delay(500);
    timeout++;
  }

  // ── FASE 3: Hasil konek ──
  if (WiFi.status() == WL_CONNECTED) {

    // Bar penuh sampai 100
    for (int p = loadProgress; p <= 100; p += 2) {
      u8g2.clearBuffer();
      u8g2.drawXBMP(48, 4, 32, 32, logo_spotify_32);
      u8g2.setFont(u8g2_font_5x7_tr);
      drawCenterText("Connecting...", 46);
      u8g2.drawFrame(14, 54, 100, 6);
      u8g2.drawBox(14, 54, p, 6);
      u8g2.sendBuffer();
      delay(20);
    }
    delay(300);

    // ── FADE IN teks "Connected!" ──
    u8g2.clearBuffer();
    u8g2.drawXBMP(48, 4, 32, 32, logo_spotify_32);
    u8g2.setFont(u8g2_font_6x10_tr);
    drawCenterText("Connected!", 48);
    u8g2.sendBuffer();
    delay(800);

    // ── Layar SYSTEM ONLINE bergaya terminal ──
    u8g2.clearBuffer();
    u8g2.sendBuffer();
    delay(100);

    u8g2.setFont(u8g2_font_4x6_tr);

    // Baris 1
    typewriterLine("> SYSTEM ONLINE", 2, 10, 45);
    delay(100);

    // Baris 2
    typewriterLine("> WiFi : OK", 2, 20, 45);
    delay(100);

    // Baris 3
    typewriterLine("> Server: checking", 2, 30, 30);
    delay(200);
    // Update baris 3 jadi OK
    u8g2.setDrawColor(0);
    u8g2.drawBox(2, 22, 126, 9);
    u8g2.setDrawColor(1);
    u8g2.drawStr(2, 30, "> Server: OK");
    u8g2.sendBuffer();
    delay(300);

    // Baris 4: IP Address
    String ipLine = "> IP: " + WiFi.localIP().toString();
    typewriterLine(ipLine.c_str(), 2, 40, 35);
    delay(200);

    // Garis pemisah
    u8g2.drawHLine(0, 44, 128);
    u8g2.sendBuffer();
    delay(200);

    // Baris 5: Ready
    typewriterLine("> Spotify: READY", 2, 54, 40);
    delay(200);

    // Blinking "READY" 3x
    for (int i = 0; i < 3; i++) {
      u8g2.setDrawColor(0);
      u8g2.drawBox(2, 46, 126, 9);
      u8g2.sendBuffer();
      delay(200);
      u8g2.setDrawColor(1);
      u8g2.drawStr(2, 54, "> Spotify: READY");
      u8g2.sendBuffer();
      delay(200);
    }
    delay(600);

  } else {
    // Gagal konek
    u8g2.clearBuffer();
    u8g2.setFont(u8g2_font_5x7_tr);
    drawCenterText("Connection Failed!", 30);
    typewriterLine("> Check WiFi config", 2, 50, 40);
    delay(3000);
  }

  randomSeed(analogRead(0));
  eqUpdateTargets();
  fetchedAt = millis();
}

// ════════════════════════════════════════════════════════
//  LOOP
// ════════════════════════════════════════════════════════
void loop() {
  unsigned long now = millis();

  if (now - lastFetch > 2000) {
    lastFetch = now;
    fetchData();
  }
  if (now - lastEqTick > 100) {
    lastEqTick = now;
    eqSmooth();
    if (random(5) == 0) eqUpdateTargets();
  }
  if (now - lastScroll > 45) {
    lastScroll = now;
    updateScroll();
  }

  if (currTitle.isEmpty() || !playing) drawIdle();
  else if (screenMode == 1) drawLyrics();
  else drawNowPlaying();

  delay(16);
}
