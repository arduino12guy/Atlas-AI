/*
 * ╔══════════════════════════════════════════════════════════════╗
 * ║   ATLAS AI  ·  ESP32 + TFT Edition                                      ║
 * ║   Groq API  ·  LLaMA-3.3-70B  ·  TFT_eSPI                               ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  Libraries needed (Library Manager):                                    ║
 * ║    · TFT_eSPI      by Bodmer                                            ║
 * ║    · ArduinoJson   by Benoit Blanchon  (v7+)                            ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  TFT_eSPI: configure libraries/TFT_eSPI/User_Setup.h                    ║
 * ║  Typical ILI9341 pins for ESP32:                                        ║
 * ║    #define TFT_MOSI 23   #define TFT_SCLK 18                            ║
 * ║    #define TFT_CS   15   #define TFT_DC    2                            ║
 * ║    #define TFT_RST   4   #define TFT_MISO 19                            ║
 * ║    #define ILI9341_DRIVER                                               ║
 * ╠══════════════════════════════════════════════════════════════╣
 * ║  Serial Monitor: 115200 baud · "Newline" line ending                    ║
 * ╚══════════════════════════════════════════════════════════════╝
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>

// ═══════════════════════════════════════════════════════════════
//  ①  FILL THESE IN
// ═══════════════════════════════════════════════════════════════
const char* WIFI_SSID    = "YOUR_WIFI_SSID";
const char* WIFI_PASS    = "YOUR_WIFI_PASSWORD";
const char* GROQ_API_KEY = "YOUR_GROQ_API_KEY";

// ═══════════════════════════════════════════════════════════════
//  ②  DISPLAY — change rotation: 0 = portrait, 1 = landscape
// ═══════════════════════════════════════════════════════════════
#define ROTATION  0
#define SCR_W   240
#define SCR_H   320

// ═══════════════════════════════════════════════════════════════
//  COLOR PALETTE  (RGB565)
//  To convert #RRGGBB → RGB565:
//    ((R>>3)<<11) | ((G>>2)<<5) | (B>>3)
// ═══════════════════════════════════════════════════════════════
#define C_BG        0x0863   // #0A0D19  — deep space navy
#define C_CARD      0x10C5   // #101828  — elevated panel
#define C_ACCENT    0x05FF   // #00BFFF  — electric cyan
#define C_ACCENT_D  0x034B   // #006896  — dimmer cyan
#define C_GOLD      0xFEA0   // #FFD400  — warm gold (stars)
#define C_WHITE     0xFFFF
#define C_LGRAY     0xB598   // #B0B8C0  — body text
#define C_MGRAY     0x634F   // #606878  — muted / labels
#define C_DIVIDER   0x18E3   // #183060  — subtle separator
#define C_ERR       0xF800   // pure red

// ═══════════════════════════════════════════════════════════════
//  SYSTEM PROMPT  — forces strict JSON output from the model
// ═══════════════════════════════════════════════════════════════
const char* SYS_PROMPT =
  "You are a world-class travel guide AI. When given a place name, "
  "respond ONLY with a valid JSON object. No markdown. No code fences. "
  "No explanation. No text before or after the JSON. "
  "Use EXACTLY these keys:\n"
  "{\n"
  "  \"name\": \"<official name, max 22 chars>\",\n"
  "  \"specialty\": \"<one-line summary, max 42 chars>\",\n"
  "  \"rating\": \"<X.X/5>\",\n"
  "  \"highlights\": [\"<max 32 chars>\", \"<max 32 chars>\", \"<max 32 chars>\"],\n"
  "  \"best_time\": \"<months or season, max 22 chars>\",\n"
  "  \"famous_for\": \"<one sentence, max 42 chars>\"\n"
  "}\n"
  "highlights MUST have exactly 3 strings. "
  "All values must be concise and within the stated character limits. "
  "Output ONLY the raw JSON object — nothing else.";

// ═══════════════════════════════════════════════════════════════
//  DATA STRUCT
// ═══════════════════════════════════════════════════════════════
struct PlaceInfo {
  String name;
  String specialty;
  String rating;
  String highlights[3];
  String bestTime;
  String famousFor;
};

// ═══════════════════════════════════════════════════════════════
//  GLOBALS
// ═══════════════════════════════════════════════════════════════
TFT_eSPI tft = TFT_eSPI();

// Shared between main loop (core 1) and API task (core 0)
volatile bool g_apiDone = false;
String        g_apiResp = "";
String        g_apiPlace = "";

// ═══════════════════════════════════════════════════════════════
//  SERIAL UTILITIES & ASCII BOOT BANNER
// ═══════════════════════════════════════════════════════════════
void printBanner() {
  Serial.println();
  Serial.println(F("  ╔══════════════════════════════════════════════╗"));
  Serial.println(F("  ║                                                      ║"));
  Serial.println(F("  ║               ✈   A T L A S   A I   ✈                ║"));
  Serial.println(F("  ║                                                      ║"));
  Serial.println(F("  ║  ┌────────────────────────────────────────┐  ║"));
  Serial.println(F("  ║  │  Model   : LLaMA-3.3-70B  (Groq)               │  ║"));
  Serial.println(F("  ║  │  Engine  : ESP32  +  TFT Display               │  ║"));
  Serial.println(F("  ║  │  Version : 1.0.0                               │  ║"));
  Serial.println(F("  ║  └────────────────────────────────────────┘  ║"));
  Serial.println(F("  ║                                                      ║"));
  Serial.println(F("  ║   \"The world is a book, and those who do            ║"));
  Serial.println(F("  ║    not travel read only one page.\"                  ║"));
  Serial.println(F("  ║                        — Saint Augustine             ║"));
  Serial.println(F("  ║                                                      ║"));
  Serial.println(F("  ╚══════════════════════════════════════════════╝"));
  Serial.println();
}

void sLog(const String& m) { Serial.println("  [·] " + m); }
void sOK (const String& m) { Serial.println("  [✓] " + m); }
void sErr(const String& m) { Serial.println("  [✗] " + m); }
void sDivider()            { Serial.println(F("  ──────────────────────────────────────────────")); }

// Serial result — box exactly 50 chars wide per line
//   "  ║" (3) + 46 inner chars + "║" (1) = 50
void printResult(const PlaceInfo& p) {
  Serial.println();
  sDivider();
  Serial.println(F("  ╔══════════════════════════════════════════════╗"));
  Serial.printf ("  ║  %-44.44s║\n",   p.name.c_str());
  Serial.println(F("  ╠══════════════════════════════════════════════╣"));
  Serial.printf ("  ║  %-10.10s: %-32.32s║\n", "Specialty",  p.specialty.c_str());
  Serial.printf ("  ║  %-10.10s: %-32.32s║\n", "Rating",     p.rating.c_str());
  Serial.println(F("  ╠══════════════════════════════════════════════╣"));
  Serial.printf ("  ║  %-44.44s║\n", "HIGHLIGHTS");
  for (int i = 0; i < 3; i++)
    Serial.printf("  ║   > %-41.41s║\n", p.highlights[i].c_str());
  Serial.println(F("  ╠══════════════════════════════════════════════╣"));
  Serial.printf ("  ║  %-10.10s: %-32.32s║\n", "Best Time",  p.bestTime.c_str());
  Serial.printf ("  ║  %-10.10s: %-32.32s║\n", "Famous For", p.famousFor.c_str());
  Serial.println(F("  ╚══════════════════════════════════════════════╝"));
  Serial.println();
}

// ═══════════════════════════════════════════════════════════════
//  TFT DRAWING PRIMITIVES
// ═══════════════════════════════════════════════════════════════

// Standard top header bar (y = 0..34)
void drawHeader(const char* title = "ATLAS AI") {
  tft.fillRect(0, 0, SCR_W, 34, C_CARD);
  tft.fillRect(0, 0, 4, 34, C_ACCENT);       // left accent stripe
  tft.drawFastHLine(0, 34, SCR_W, C_ACCENT); // bottom line
  tft.setTextColor(C_WHITE);
  tft.setTextFont(2);
  tft.setTextSize(1);
  tft.setCursor(12, 9);
  tft.print(title);
  tft.setTextColor(C_MGRAY);
  tft.setTextFont(1);
  tft.setCursor(SCR_W - 28, 13);
  tft.print("v1.0");
}

// Centered text on TFT
void tftCenter(int y, const String& text, uint8_t font, uint8_t sz, uint16_t col) {
  tft.setTextFont(font); tft.setTextSize(sz); tft.setTextColor(col);
  tft.setCursor((SCR_W - tft.textWidth(text)) / 2, y);
  tft.print(text);
}

// Small section label
void drawLabel(int x, int y, const char* lbl, uint16_t col = C_ACCENT) {
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(col);
  tft.setCursor(x, y); tft.print(lbl);
}

// Body text
void drawBody(int x, int y, const String& t, uint16_t col = C_LGRAY) {
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(col);
  tft.setCursor(x, y); tft.print(t);
}

// Thin horizontal divider
void hLine(int y, uint16_t col = C_DIVIDER) {
  tft.drawFastHLine(0, y, SCR_W, col);
}

// 5 rating dots (filled = gold, empty = dark)
void drawRatingDots(int x, int y, float rating) {
  int filled = (int)(rating + 0.5f);
  for (int i = 0; i < 5; i++) {
    bool on = (i < filled);
    tft.fillCircle(x + i * 13, y, 4, on ? C_GOLD : C_DIVIDER);
    if (on) tft.drawCircle(x + i * 13, y, 5, 0xFD20);  // gold glow ring
  }
}

// Word-wrap into at most 2 lines, returns lines used
int wrapText(int x, int y, const String& text, int maxW, int lineH, uint16_t col) {
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(col);
  if (tft.textWidth(text) <= maxW) {
    tft.setCursor(x, y); tft.print(text);
    return 1;
  }
  int brk = text.length();
  while (brk > 0 && tft.textWidth(text.substring(0, brk)) > maxW) brk--;
  int sp = text.lastIndexOf(' ', brk);
  if (sp > 0) brk = sp;
  tft.setCursor(x, y);
  tft.print(text.substring(0, brk));
  String rest = text.substring(brk + 1);
  while (tft.textWidth(rest) > maxW && rest.length() > 2)
    rest = rest.substring(0, rest.length() - 1);
  tft.setCursor(x, y + lineH);
  tft.print(rest);
  return 2;
}

// ═══════════════════════════════════════════════════════════════
//  SCREENS
// ═══════════════════════════════════════════════════════════════

void screenSplash() {
  tft.fillScreen(C_BG);

  // Corner accent dots
  for (auto& cx : {10, SCR_W - 10}) for (auto& cy : {10, SCR_H - 10})
    tft.fillCircle(cx, cy, 3, C_ACCENT_D);

  // Logo box
  int bx = SCR_W / 2 - 38, by = 60;
  tft.fillRoundRect(bx, by, 76, 62, 8, C_CARD);
  tft.drawRoundRect(bx,     by,     76, 62, 8, C_ACCENT);
  tft.drawRoundRect(bx + 1, by + 1, 74, 60, 7, C_ACCENT_D);
  tft.setTextColor(C_ACCENT);
  tft.setTextFont(4);
  tft.setTextSize(1);
  int tw = tft.textWidth("TG");
  tft.setCursor(bx + (76 - tw) / 2, by + 17);
  tft.print("TG");

  tftCenter(140, "TRAVEL  GUIDE", 2, 1, C_WHITE);

  // Spaced letters: A I   E D I T I O N
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(C_ACCENT);
  String sub = "A  I     E D I T I O N";
  tft.setCursor((SCR_W - tft.textWidth(sub)) / 2, 162);
  tft.print(sub);

  tft.drawFastHLine(50, 178, SCR_W - 100, C_DIVIDER);
  tftCenter(185, "Powered by Groq + LLaMA 3.3", 1, 1, C_MGRAY);
  tftCenter(200, "v 1 . 0 . 0", 1, 1, C_DIVIDER);
  tftCenter(SCR_H - 18, "ESP32  ·  TFT_eSPI", 1, 1, C_DIVIDER);
}

void screenWiFi(int dots) {
  tft.fillScreen(C_BG);
  drawHeader("CONNECTING");
  tftCenter(100, "WiFi", 4, 1, C_LGRAY);
  String d = "";
  for (int i = 0; i < (dots % 4); i++) d += " .";
  tftCenter(140, String(WIFI_SSID) + d, 1, 1, C_ACCENT);
  // Signal bars animation
  const int heights[4] = {8, 14, 20, 26};
  int filled = (dots % 5) + 1;
  int bx = SCR_W / 2 - 26;
  for (int i = 0; i < 4; i++) {
    int h = heights[i];
    tft.fillRoundRect(bx + i * 14, 195 + (26 - h), 10, h, 2,
                      (i < filled) ? C_ACCENT : C_DIVIDER);
  }
}

void screenWiFiOK() {
  tft.fillScreen(C_BG);
  drawHeader("CONNECTED");
  // Green tick circle
  tft.fillCircle(SCR_W / 2, 110, 30, 0x07E0);
  tft.drawCircle(SCR_W / 2, 110, 32, C_WHITE);
  tft.setTextColor(C_BG); tft.setTextFont(4);
  int tw = tft.textWidth("OK");
  tft.setCursor(SCR_W / 2 - tw / 2, 97);
  tft.print("OK");
  tftCenter(158, "WiFi Connected!", 2, 1, C_WHITE);
  tftCenter(182, "Initializing...", 1, 1, C_MGRAY);
}

void screenReady() {
  tft.fillScreen(C_BG);
  drawHeader();
  // Search magnifier icon
  tft.drawCircle(SCR_W / 2 - 5, 108, 20, C_ACCENT);
  tft.drawCircle(SCR_W / 2 - 5, 108, 19, C_ACCENT_D);
  tft.drawLine(SCR_W / 2 + 10, 123, SCR_W / 2 + 22, 135, C_ACCENT);
  tft.drawLine(SCR_W / 2 + 11, 123, SCR_W / 2 + 23, 135, C_ACCENT);
  tftCenter(144, "READY", 4, 1, C_WHITE);
  tftCenter(177, "Type a place in Serial Monitor", 1, 1, C_LGRAY);
  tftCenter(190, "and press Enter", 1, 1, C_LGRAY);
  // Input field visual
  tft.fillRoundRect(18, 210, SCR_W - 36, 28, 6, C_CARD);
  tft.drawRoundRect(18, 210, SCR_W - 36, 28, 6, C_ACCENT);
  tft.setTextColor(C_MGRAY); tft.setTextFont(1);
  tft.setCursor(28, 220); tft.print("e.g.  Eiffel Tower...");
  tft.fillRect(28 + tft.textWidth("e.g.  Eiffel Tower...") + 2, 218, 2, 12, C_ACCENT);
  tftCenter(SCR_H - 18, "Waiting for input...", 1, 1, C_ACCENT_D);
}

// Shown while user types — live TFT feedback
void screenTyping(const String& typed) {
  tft.fillScreen(C_BG);
  drawHeader();
  tftCenter(108, "WHERE TO?", 4, 1, C_LGRAY);
  tftCenter(146, "Type your destination below", 1, 1, C_MGRAY);
  int iy = 175;
  tft.fillRoundRect(12, iy, SCR_W - 24, 32, 6, C_CARD);
  tft.drawRoundRect(12, iy, SCR_W - 24, 32, 6, C_ACCENT);
  tft.setTextColor(C_WHITE); tft.setTextFont(2);
  String disp = typed;
  int maxW = SCR_W - 50;
  while (tft.textWidth(disp) > maxW && disp.length() > 0)
    disp = disp.substring(1);
  tft.setCursor(22, iy + 8);
  tft.print(disp);
  // Blinking cursor bar
  tft.fillRect(22 + tft.textWidth(disp) + 2, iy + 7, 2, 18, C_ACCENT);
  tftCenter(SCR_H - 18, "Press Enter to search", 1, 1, C_MGRAY);
}

// Loading spinner — animated on core 1 while API runs on core 0
void screenLoading(const String& place, int frame) {
  tft.fillScreen(C_BG);
  drawHeader("CONSULTING AI");
  tftCenter(80, place, 2, 1, C_WHITE);
  tft.drawFastHLine(SCR_W / 2 - 50, 102, 100, C_DIVIDER);

  // 8-dot circular spinner (comet trail effect)
  const int cx = SCR_W / 2, cy = 168, r = 28;
  for (int i = 0; i < 8; i++) {
    float a = i * (PI / 4.0f) - PI / 2.0f;
    int dx = cx + (int)(r * cos(a));
    int dy = cy + (int)(r * sin(a));
    int dist = (frame - i + 8) % 8;
    uint16_t col = (dist == 0) ? C_ACCENT
                 : (dist == 1) ? C_ACCENT_D
                 : (dist == 2) ? C_MGRAY
                 :               C_DIVIDER;
    tft.fillCircle(dx, dy, 4, col);
  }

  const char* msgs[4] = {
    "Gathering destination facts...",
    "Analyzing highlights...",
    "Consulting AI model...",
    "Preparing your guide..."
  };
  tftCenter(218, msgs[frame % 4], 1, 1, C_LGRAY);
  tftCenter(234, "This may take a few seconds", 1, 1, C_MGRAY);
}

void screenError(const String& msg) {
  tft.fillScreen(C_BG);
  drawHeader("ERROR");
  tft.fillCircle(SCR_W / 2, 112, 30, C_ERR);
  tft.drawCircle(SCR_W / 2, 112, 32, 0xFC00);
  tft.setTextColor(C_WHITE); tft.setTextFont(4);
  int tw = tft.textWidth("!");
  tft.setCursor(SCR_W / 2 - tw / 2, 99); tft.print("!");
  tftCenter(162, "Something went wrong", 2, 1, C_WHITE);
  tftCenter(188, msg, 1, 1, C_LGRAY);
  tftCenter(208, "Check Serial Monitor", 1, 1, C_MGRAY);
  tftCenter(222, "for full details", 1, 1, C_MGRAY);
  tft.drawRoundRect(40, 258, SCR_W - 80, 26, 5, C_ACCENT);
  tftCenter(266, "Try again in Serial...", 1, 1, C_ACCENT);
}

// ═══════════════════════════════════════════════════════════════
//  MAIN RESULT SCREEN — premium card layout
// ═══════════════════════════════════════════════════════════════
void screenResult(const PlaceInfo& p) {
  tft.fillScreen(C_BG);

  // ── HEADER  (y 0–34) ────────────────────────────────────────
  drawHeader();

  // ── NAME PANEL  (y 35–86, 52 px) ────────────────────────────
  tft.fillRect(0, 35, SCR_W, 52, C_CARD);
  tft.fillRect(0, 35, 4, 52, C_ACCENT);    // left accent stripe

  // Try font 4 (26 px), fall back to font 2 (16 px) for long names
  tft.setTextFont(4); tft.setTextSize(1); tft.setTextColor(C_WHITE);
  String nm = p.name;
  if (tft.textWidth(nm) > SCR_W - 42) {
    tft.setTextFont(2);
    while (tft.textWidth(nm + "..") > SCR_W - 42 && nm.length() > 1)
      nm = nm.substring(0, nm.length() - 1);
    if (nm != p.name) nm += "..";
  }
  tft.setCursor(10, 35 + (52 - tft.fontHeight()) / 2);
  tft.print(nm);

  // "AI" badge top-right
  tft.fillRoundRect(SCR_W - 30, 42, 24, 16, 3, C_ACCENT);
  tft.setTextColor(C_BG); tft.setTextFont(1); tft.setTextSize(1);
  tft.setCursor(SCR_W - 24, 47); tft.print("AI");

  // ── RATING ROW  (y 87–112, 26 px) ───────────────────────────
  tft.fillRect(0, 87, SCR_W, 26, 0x0841);
  float rv = p.rating.toFloat();  // "4.8/5".toFloat() → 4.8
  drawRatingDots(10, 100, rv);
  tft.setTextColor(C_GOLD); tft.setTextFont(1); tft.setTextSize(1);
  tft.setCursor(78, 96); tft.print(p.rating);
  tft.setTextColor(C_MGRAY);
  int rw = tft.textWidth("VISITOR RATING");
  tft.setCursor(SCR_W - rw - 6, 96); tft.print("VISITOR RATING");

  hLine(113);

  // ── SPECIALTY  (y 114–152) ───────────────────────────────────
  drawLabel(10, 116, "SPECIALTY");
  wrapText(10, 128, p.specialty, SCR_W - 20, 12, C_LGRAY);

  hLine(153);

  // ── HIGHLIGHTS  (y 154–216) ──────────────────────────────────
  drawLabel(10, 156, "HIGHLIGHTS");
  for (int i = 0; i < 3; i++) {
    int hy = 168 + i * 16;
    tft.fillCircle(12, hy + 4, 3, C_ACCENT);            // bullet dot
    tft.drawCircle(12, hy + 4, 4, C_ACCENT_D);          // glow ring
    drawBody(22, hy, p.highlights[i], C_LGRAY);
  }

  hLine(218);

  // ── BEST TIME  (y 219–241) ───────────────────────────────────
  drawLabel(10, 220, "BEST TIME", C_ACCENT);
  // Right-align the value for clean look
  tft.setTextFont(1); tft.setTextSize(1); tft.setTextColor(C_LGRAY);
  int btw = tft.textWidth(p.bestTime);
  tft.setCursor(SCR_W - btw - 8, 220); tft.print(p.bestTime);

  hLine(242);

  // ── FAMOUS FOR  (y 243–284) ──────────────────────────────────
  drawLabel(10, 244, "FAMOUS FOR", C_GOLD);
  wrapText(10, 256, p.famousFor, SCR_W - 20, 12, C_LGRAY);

  // ── FOOTER  (y 302–320) ──────────────────────────────────────
  tft.fillRect(0, 302, SCR_W, 18, C_CARD);
  tft.drawFastHLine(0, 302, SCR_W, C_ACCENT_D);
  tft.setTextColor(C_MGRAY); tft.setTextFont(1);
  String footer = "Groq  *  LLaMA-3.3-70B  *  v1.0";
  tft.setCursor((SCR_W - tft.textWidth(footer)) / 2, 308);
  tft.print(footer);
}

// ═══════════════════════════════════════════════════════════════
//  WiFi
// ═══════════════════════════════════════════════════════════════
void connectWiFi() {
  sLog("Connecting to: " + String(WIFI_SSID));
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    screenWiFi(dots++);
    delay(500);
  }
  screenWiFiOK();
  delay(1000);
  sOK("Connected. IP: " + WiFi.localIP().toString());
}

// ═══════════════════════════════════════════════════════════════
//  API TASK  — runs on core 0 so the spinner animates on core 1
// ═══════════════════════════════════════════════════════════════
String buildBody(const String& place) {
  String sp = SYS_PROMPT;
  sp.replace("\\", "\\\\");
  sp.replace("\"", "\\\"");
  sp.replace("\n", "\\n");
  return String("{\"model\":\"llama-3.3-70b-versatile\","
                "\"temperature\":0.3,\"max_tokens\":350,"
                "\"messages\":["
                "{\"role\":\"system\",\"content\":\"") + sp +
         String("\"},"
                "{\"role\":\"user\",\"content\":\"Tell me about: ") +
         place + String("\"}]}");
}

void apiTask(void* pv) {
  WiFiClientSecure client;
  client.setInsecure();  // skip cert verify — fine for a maker project
  HTTPClient http;
  http.begin(client, "https://api.groq.com/openai/v1/chat/completions");
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", "Bearer " + String(GROQ_API_KEY));
  http.setTimeout(25000);
  int code = http.POST(buildBody(g_apiPlace));
  if (code == 200) {
    g_apiResp = http.getString();
  } else {
    sErr("HTTP Error: " + String(code));
    g_apiResp = "";
  }
  http.end();
  g_apiDone = true;
  vTaskDelete(NULL);
}

// ═══════════════════════════════════════════════════════════════
//  JSON PARSER  — two-stage: outer Groq envelope → inner place JSON
// ═══════════════════════════════════════════════════════════════
bool parseResponse(const String& raw, PlaceInfo& info) {
  JsonDocument outer;
  if (deserializeJson(outer, raw) != DeserializationError::Ok) {
    sErr("Outer JSON parse failed"); return false;
  }
  const char* content = outer["choices"][0]["message"]["content"];
  if (!content) { sErr("No content in response"); return false; }

  JsonDocument inner;
  if (deserializeJson(inner, content) != DeserializationError::Ok) {
    sErr("Inner JSON parse failed. Raw: " + String(content).substring(0, 100));
    return false;
  }
  info.name      = inner["name"]       | "Unknown Place";
  info.specialty = inner["specialty"]  | "N/A";
  info.rating    = inner["rating"]     | "?.?/5";
  info.bestTime  = inner["best_time"]  | "Year-round";
  info.famousFor = inner["famous_for"] | "N/A";
  JsonArray hl = inner["highlights"].as<JsonArray>();
  for (int i = 0; i < 3; i++)
    info.highlights[i] = (hl && i < (int)hl.size())
                         ? hl[i].as<String>() : "See local guide";
  return true;
}

// ═══════════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════════
void setup() {
  Serial.begin(115200);
  delay(600);

  tft.init();
  tft.setRotation(ROTATION);
  tft.fillScreen(C_BG);

  printBanner();       // serial ASCII art
  screenSplash();      // TFT splash
  delay(2200);

  connectWiFi();

  screenReady();
  sLog("Ready! Type a place name in Serial Monitor (115200, Newline).");
  sDivider();
}

// ═══════════════════════════════════════════════════════════════
//  MAIN LOOP
// ═══════════════════════════════════════════════════════════════
void loop() {
  Serial.println();
  Serial.print(F("  Enter place name: "));

  // Read input char-by-char — each keystroke updates the TFT live
  String input = "";
  while (true) {
    if (Serial.available()) {
      char c = Serial.read();
      if (c == '\n' || c == '\r') { Serial.println(); break; }
      if (c >= 32 && c < 127) {
        input += c;
        Serial.print(c);       // echo to Serial Monitor
        screenTyping(input);   // live TFT update
      }
    }
  }

  input.trim();
  if (input.length() == 0) {
    Serial.println(F("  [!] Empty input — try again."));
    screenReady();
    return;
  }

  sLog("Searching: " + input);

  // ── Kick off API task on core 0 ────────────────────────────
  g_apiDone  = false;
  g_apiResp  = "";
  g_apiPlace = input;
  xTaskCreatePinnedToCore(apiTask, "api_task", 16384, NULL, 1, NULL, 0);

  // ── Animate spinner on core 1 while we wait ────────────────
  int frame = 0;
  unsigned long t0 = millis();
  while (!g_apiDone) {
    screenLoading(input, frame++);
    delay(130);
  }
  sLog("Response in " + String(millis() - t0) + " ms");

  // ── Handle empty response ──────────────────────────────────
  if (g_apiResp.length() == 0) {
    screenError("API call failed");
    sErr("No response — check API key & WiFi.");
    delay(3500);
    screenReady();
    return;
  }

  // ── Parse ──────────────────────────────────────────────────
  PlaceInfo info;
  if (!parseResponse(g_apiResp, info)) {
    screenError("Parse error");
    delay(3500);
    screenReady();
    return;
  }

  // ── Display ────────────────────────────────────────────────
  screenResult(info);
  printResult(info);
  sOK("Displayed: " + info.name);
  sDivider();

  delay(1500);  // brief pause before next prompt
}
