#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#include "secrets.h"  
#include "sprites.h"  // Mochi the cat + icons (edit the pixel art there!)
const char* TIMEZONE  = "PST8PDT,M3.2.0,M11.1.0";

#define BUZZER_ACTIVE_LOW true

#define SDA_PIN      21
#define SCL_PIN      22
#define SET_BTN_PIN  19
#define PLUS_BTN_PIN 23
#define BUZZER_PIN   18

#define REPEAT_DELAY_MS  500
#define REPEAT_RATE_MS   120
#define SNOOZE_MINUTES   5
#define RING_TIMEOUT_MIN 3

// Pet settings
#define PET_X          94   // left edge of the cat (cat is 32x32 px)
#define BEDTIME_HOUR   22   // cat falls asleep at 10 PM...
#define WAKE_HOUR_OFF  7    // ...and wakes at your alarm (or 7 AM if the alarm is off)
#define HAPPY_MS       1500 // how long the cat celebrates after a button press

enum Mode { CLOCK, SET_HOUR, SET_MIN, RINGING };
enum ButtonEvent { NONE, PRESS, REPEAT };
enum PetMood { PET_IDLE, PET_ALERT, PET_SLEEP, PET_HAPPY, PET_PANIC, PET_CONFUSED };

struct Button {
  uint8_t pin;
  bool lastReading;
  bool stable;
  unsigned long lastChange;
  unsigned long pressStart;
  unsigned long lastRepeat;
};

Adafruit_SSD1306 display(128, 64, &Wire, -1);
Preferences prefs;

Button setBtn  = { SET_BTN_PIN,  HIGH, HIGH, 0, 0, 0 };
Button plusBtn = { PLUS_BTN_PIN, HIGH, HIGH, 0, 0, 0 };

Mode mode = CLOCK;
int  alarmHour = 7, alarmMin = 0;
bool alarmOn = true;
int  editHour = 7, editMin = 0;
time_t snoozeUntil = 0;
time_t ringStarted = 0;
int  lastTriggered = -1;
bool buzzing = false;
unsigned long happyStart = 0;
bool happyArmed = false;

void buzzerOn()  { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? LOW : HIGH); }
void buzzerOff() { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? HIGH : LOW); }
void chirp() { buzzerOn(); delay(60); buzzerOff(); }
void cheer() { happyStart = millis(); happyArmed = true; }  // makes the cat celebrate
bool isHappy() {
  if (happyArmed && millis() - happyStart < HAPPY_MS) return true;
  happyArmed = false;
  return false;
}

void updateBuzzer() {
  bool shouldBuzz = false;
  if (mode == RINGING) {
    unsigned long t = millis() % 1000;
    shouldBuzz = (t < 600) && (t % 200 < 100);
  }
  if (shouldBuzz != buzzing) {
    buzzing = shouldBuzz;
    if (buzzing) buzzerOn(); else buzzerOff();
  }
}

ButtonEvent readButton(Button& b) {
  bool reading = digitalRead(b.pin);
  unsigned long now = millis();

  if (reading != b.lastReading) { b.lastChange = now; b.lastReading = reading; }
  if (now - b.lastChange > 30 && reading != b.stable) {
    b.stable = reading;
    if (b.stable == LOW) {
      b.pressStart = now;
      b.lastRepeat = now;
      return PRESS;
    }
  }
  if (b.stable == LOW && now - b.pressStart >= REPEAT_DELAY_MS && now - b.lastRepeat >= REPEAT_RATE_MS) {
    b.lastRepeat = now;
    return REPEAT;
  }
  return NONE;
}

void saveAlarm() {
  prefs.putInt("h", alarmHour);
  prefs.putInt("m", alarmMin);
  prefs.putBool("on", alarmOn);
}
void startRinging() { mode = RINGING; ringStarted = time(nullptr); }
void stopRinging()  { mode = CLOCK; buzzing = false; buzzerOff(); }

int to12(int h) { h %= 12; return h == 0 ? 12 : h; }
const char* ampm(int h) { return h < 12 ? "AM" : "PM"; }

void showMessage(const char* line1, const char* line2) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(0, 20); display.println(line1);
  display.setCursor(0, 34); display.println(line2);
  display.display();
}

// ---------------------------------------------------------------- drawing --

// Text in the built-in font is 6 px per character (at size 1).
void printRight(const char* s, int y) {
  display.setCursor(128 - (int)strlen(s) * 6 + 1, y);
  display.print(s);
}

void drawDottedLine(int y) {
  for (int x = 0; x < 128; x += 2) display.drawPixel(x, y, SSD1306_WHITE);
}

void drawBigTime(int h, int m, bool hideHour, bool hideMin, int y) {
  char hs[3], ms[3];
  if (hideHour) strcpy(hs, "  "); else sprintf(hs, "%2d", to12(h));
  if (hideMin)  strcpy(ms, "  "); else sprintf(ms, "%02d", m);
  display.setTextSize(3);
  display.setCursor(0, y);
  display.printf("%s:%s", hs, ms);
  display.setTextSize(1);
  display.setCursor(1, y + 26);
  display.print(ampm(h));
}

// Is it the cat's bedtime? Sleeps from BEDTIME_HOUR until your alarm.
bool isPetBedtime(struct tm& now) {
  int n    = now.tm_hour * 60 + now.tm_min;
  int bed  = BEDTIME_HOUR * 60;
  int wake = alarmOn ? alarmHour * 60 + alarmMin : WAKE_HOUR_OFF * 60;
  if (bed == wake) return false;
  return bed < wake ? (n >= bed && n < wake) : (n >= bed || n < wake);
}

// Draws Mochi standing on the line y = groundY.
void drawPet(PetMood mood, int x, int groundY) {
  unsigned long ms = millis();
  int y = groundY - 32;
  const int faceX = 2 * 2, faceY = 5 * 2;  // where the face goes on the cat

  switch (mood) {
    case PET_SLEEP: {
      DRAW_SPRITE(display, CAT_SLEEP, x, y, 2);
      // three little z's floating up from the cat's head
      display.setTextSize(1);
      for (int i = 0; i < 3; i++) {
        int p = (ms + i * 800) % 2400;          // 0..2399
        display.setCursor(x + 12 + p / 150, y + 4 - p / 120);
        display.print('z');
      }
      break;
    }
    case PET_PANIC: {
      int hop   = (ms / 150) % 2 ? 5 : 0;
      int shake = (ms / 75) % 2 ? 1 : -1;
      DRAW_SPRITE(display, CAT_PANIC, x + shake, y - hop, 2);
      DRAW_SPRITE(display, FACE_SHOCK, x + shake + faceX, y - hop + faceY, 2);
      break;
    }
    case PET_HAPPY: {
      int hop = (ms / 180) % 2 ? 4 : 0;
      DRAW_SPRITE(display, CAT_SIT_A, x, y - hop, 2);
      DRAW_SPRITE(display, FACE_HAPPY, x + faceX, y - hop + faceY, 2);
      int rise = (HAPPY_MS - (ms - happyStart)) / 150;  // heart floats up
      DRAW_SPRITE(display, ICON_HEART, x + 22, y - 10 + rise, 1);
      break;
    }
    case PET_CONFUSED: {
      DRAW_SPRITE(display, CAT_SIT_A, x, y, 2);
      DRAW_SPRITE(display, FACE_CONFUSED, x + faceX, y + faceY, 2);
      display.setTextSize(2);
      display.setCursor(x + 22, y - 6 - ((ms / 500) % 2));
      display.print('?');
      display.setTextSize(1);
      break;
    }
    default: {  // PET_IDLE and PET_ALERT: sit, swish tail, blink sometimes
      int tailSpeed = (mood == PET_ALERT) ? 250 : 700;
      bool tailB = (ms / tailSpeed) % 2;
      unsigned long b = ms % 5000;
      bool blinking = (b < 140) || (mood == PET_IDLE && b > 300 && b < 440);  // double blink
      DRAW_SPRITE(display, tailB ? CAT_SIT_B : CAT_SIT_A, x, y, 2);
      DRAW_SPRITE(display, blinking ? FACE_BLINK : FACE_OPEN, x + faceX, y + faceY, 2);
      break;
    }
  }
}

const char* greeting(struct tm& now, bool sleeping) {
  if (snoozeUntil)   return "just 5 more min";
  if (sleeping)      return "shh... sleeping";
  if (now.tm_hour < 5)  return "it's so late!";
  if (now.tm_hour < 12) return "good morning!";
  if (now.tm_hour < 17) return "good afternoon!";
  return "good evening!";
}

void drawScreen(struct tm& now, bool haveTime) {
  bool blink = (millis() / 400) % 2;
  bool happy = isHappy();
  char buf[24];
  display.clearDisplay();
  display.invertDisplay(false);
  display.setTextColor(SSD1306_WHITE);
  display.setTextWrap(false);
  display.setTextSize(1);

  if (mode == CLOCK) {
    // --- top bar: date on the left, alarm status on the right ---
    if (haveTime) {
      strftime(buf, sizeof(buf), "%a %b %d", &now);
      display.setCursor(0, 3); display.print(buf);
    }
    if (snoozeUntil) {
      long left = snoozeUntil - time(nullptr);
      if (left < 0) left = 0;
      sprintf(buf, "%ld:%02ld", left / 60, left % 60);
      DRAW_SPRITE(display, ICON_ZZ, 128 - strlen(buf) * 6 - 10, 3, 1);
    } else if (alarmOn) {
      sprintf(buf, "%d:%02d%c", to12(alarmHour), alarmMin, alarmHour < 12 ? 'a' : 'p');
      DRAW_SPRITE(display, ICON_BELL, 128 - strlen(buf) * 6 - 10, 3, 1);
    } else {
      strcpy(buf, "off");
      DRAW_SPRITE(display, ICON_BELL_OFF, 128 - strlen(buf) * 6 - 10, 3, 1);
    }
    printRight(buf, 3);
    drawDottedLine(13);

    // --- middle: the time, or what's wrong ---
    PetMood mood;
    if (!haveTime) {
      display.setCursor(0, 24);
      display.print(WiFi.status() == WL_CONNECTED ? "Getting time..." : "No WiFi :(");
      display.setCursor(0, 36);
      display.print(WiFi.status() == WL_CONNECTED ? "hang on!" : "no time yet");
      mood = PET_CONFUSED;
    } else {
      bool sleeping = snoozeUntil || isPetBedtime(now);
      drawBigTime(now.tm_hour, now.tm_min, false, false, 19);
      // seconds bar next to AM/PM
      display.drawRoundRect(17, 44, 70, 7, 3, SSD1306_WHITE);
      int fill = (now.tm_sec * 66) / 59;
      if (fill > 0) display.fillRoundRect(19, 46, fill, 3, 1, SSD1306_WHITE);
      display.setCursor(0, 56);
      display.print(greeting(now, sleeping));
      mood = sleeping ? PET_SLEEP : PET_IDLE;
    }
    if (happy) mood = PET_HAPPY;
    drawPet(mood, PET_X, 64);
  }
  else if (mode == SET_HOUR || mode == SET_MIN) {
    display.setCursor(0, 3); display.print("SET ALARM");
    printRight(mode == SET_HOUR ? "hour" : "minute", 3);
    drawDottedLine(13);
    drawBigTime(editHour, editMin, mode == SET_HOUR && !blink, mode == SET_MIN && !blink, 19);
    display.setCursor(0, 56);
    display.print(mode == SET_HOUR ? "+ up   SET next" : "+ up   SET save");
    drawPet(PET_ALERT, PET_X, 64);
  }
  else if (mode == RINGING) {
    display.invertDisplay(blink);
    display.setTextSize(2);
    display.setCursor(0, 0); display.print("WAKE UP!");
    int wiggle = (millis() / 90) % 2 ? 2 : -2;
    DRAW_SPRITE(display, ICON_BELL, 106 + wiggle, 0, 2);
    if (haveTime) drawBigTime(now.tm_hour, now.tm_min, false, false, 20);
    display.setTextSize(1);
    display.setCursor(0, 56); display.print("SET stop   + snooze");
    drawPet(PET_PANIC, PET_X, 54);
  }
  display.display();
}

// ------------------------------------------------------------------ setup --

void setup() {
  Serial.begin(115200);
  pinMode(SET_BTN_PIN, INPUT_PULLUP);
  pinMode(PLUS_BTN_PIN, INPUT_PULLUP);
  pinMode(BUZZER_PIN, OUTPUT);
  buzzerOff();

  Wire.begin(SDA_PIN, SCL_PIN);
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) Serial.println("OLED not found!");
  display.setTextColor(SSD1306_WHITE);

  prefs.begin("alarm", false);
  alarmHour = prefs.getInt("h", 7);
  alarmMin  = prefs.getInt("m", 0);
  alarmOn   = prefs.getBool("on", true);

  showMessage("Connecting to WiFi:", WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) delay(250);

  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("WiFi connected");
    configTzTime(TIMEZONE, "pool.ntp.org", "time.nist.gov");
  } else {
    Serial.println("WiFi failed");
    showMessage("WiFi failed!", "Check name/password");
    delay(3000);
  }
  chirp();
  cheer();  // Mochi says hi
}

void loop() {
  ButtonEvent setEv  = readButton(setBtn);
  ButtonEvent plusEv = readButton(plusBtn);
  bool plusPressed = (plusEv == PRESS || plusEv == REPEAT);

  struct tm now;
  bool haveTime = getLocalTime(&now, 0);

  switch (mode) {
    case CLOCK:
      if (setEv == PRESS) {
        editHour = alarmHour; editMin = alarmMin; mode = SET_HOUR; chirp();
      } else if (plusEv == PRESS) {
        alarmOn = !alarmOn; snoozeUntil = 0; saveAlarm(); chirp(); cheer();
      }
      break;

    case SET_HOUR:
      if (setEv == PRESS) { mode = SET_MIN; chirp(); }
      else if (plusPressed) editHour = (editHour + 1) % 24;
      break;

    case SET_MIN:
      if (setEv == PRESS) {
        alarmHour = editHour; alarmMin = editMin; alarmOn = true; snoozeUntil = 0;
        saveAlarm(); mode = CLOCK;
        chirp(); delay(80); chirp(); cheer();
      }
      else if (plusPressed) editMin = (editMin + 1) % 60;
      break;

    case RINGING:
      if (setEv == PRESS) {
        snoozeUntil = 0; stopRinging(); cheer();
      } else if (plusEv == PRESS) {
        snoozeUntil = time(nullptr) + SNOOZE_MINUTES * 60; stopRinging(); cheer();
      } else if (time(nullptr) - ringStarted > RING_TIMEOUT_MIN * 60) {
        snoozeUntil = 0; stopRinging();
      }
      break;
  }

  if (mode == CLOCK && haveTime) {
    int stamp = now.tm_yday * 1440 + now.tm_hour * 60 + now.tm_min;
    if (alarmOn && now.tm_hour == alarmHour && now.tm_min == alarmMin && stamp != lastTriggered) {
      lastTriggered = stamp;
      snoozeUntil = 0;
      startRinging();
    } else if (snoozeUntil && time(nullptr) >= snoozeUntil) {
      snoozeUntil = 0;
      startRinging();
    }
  }

  updateBuzzer();

  static unsigned long lastDraw = 0;
  if (millis() - lastDraw >= 100) {
    lastDraw = millis();
    drawScreen(now, haveTime);
  }
}