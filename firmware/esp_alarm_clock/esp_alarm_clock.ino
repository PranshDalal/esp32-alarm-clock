#include <WiFi.h>
#include <time.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Preferences.h>

#include "secrets.h"  // WiFi credentials (not shared); see secrets.example.h
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

enum Mode { CLOCK, SET_HOUR, SET_MIN, RINGING };
enum ButtonEvent { NONE, PRESS, REPEAT };

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

void buzzerOn()  { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? LOW : HIGH); }
void buzzerOff() { digitalWrite(BUZZER_PIN, BUZZER_ACTIVE_LOW ? HIGH : LOW); }
void chirp() { buzzerOn(); delay(60); buzzerOff(); }

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

void drawBigTime(int h, int m, bool hideHour, bool hideMin) {
  char hs[3], ms[3];
  if (hideHour) strcpy(hs, "  "); else sprintf(hs, "%2d", to12(h));
  if (hideMin)  strcpy(ms, "  "); else sprintf(ms, "%02d", m);
  display.setTextSize(3);
  display.setCursor(4, 16);
  display.printf("%s:%s", hs, ms);
  display.setTextSize(1);
  display.setCursor(100, 16);
  display.print(ampm(h));
}

void drawScreen(struct tm& now, bool haveTime) {
  bool blink = (millis() / 400) % 2;
  display.clearDisplay();
  display.invertDisplay(false);
  display.setTextColor(SSD1306_WHITE);
  display.setTextSize(1);

  if (mode == CLOCK) {
    if (!haveTime) {
      display.setCursor(0, 24);
      display.println(WiFi.status() == WL_CONNECTED ? "Getting time..." : "No WiFi - no time :(");
    } else {
      char date[20];
      strftime(date, sizeof(date), "%a %b %d", &now);
      display.setCursor(0, 0); display.print(date);
      drawBigTime(now.tm_hour, now.tm_min, false, false);
      display.setCursor(100, 30); display.printf(":%02d", now.tm_sec);
    }
    display.setCursor(0, 54);
    if (snoozeUntil) {
      long left = snoozeUntil - time(nullptr);
      if (left < 0) left = 0;
      display.printf("Snoozing %ld:%02ld", left / 60, left % 60);
    } else if (alarmOn) {
      display.printf("Alarm %d:%02d %s  ON", to12(alarmHour), alarmMin, ampm(alarmHour));
    } else {
      display.print("Alarm OFF");
    }
  }
  else if (mode == SET_HOUR || mode == SET_MIN) {
    display.setCursor(0, 0);
    display.print(mode == SET_HOUR ? "SET ALARM: HOUR" : "SET ALARM: MINUTE");
    drawBigTime(editHour, editMin, mode == SET_HOUR && !blink, mode == SET_MIN && !blink);
    display.setCursor(0, 54);
    display.print(mode == SET_HOUR ? "+:up     SET:next" : "+:up     SET:save");
  }
  else if (mode == RINGING) {
    display.invertDisplay(blink);
    display.setTextSize(2);
    display.setCursor(16, 0); display.print("WAKE UP!");
    if (haveTime) {
      display.setCursor(28, 30);
      display.printf("%d:%02d", to12(now.tm_hour), now.tm_min);
    }
    display.setTextSize(1);
    display.setCursor(0, 54); display.print("SET:stop   +:snooze");
  }
  display.display();
}

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
        alarmOn = !alarmOn; snoozeUntil = 0; saveAlarm(); chirp();
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
        chirp(); delay(80); chirp();
      }
      else if (plusPressed) editMin = (editMin + 1) % 60;
      break;

    case RINGING:
      if (setEv == PRESS) {
        snoozeUntil = 0; stopRinging();
      } else if (plusEv == PRESS) {
        snoozeUntil = time(nullptr) + SNOOZE_MINUTES * 60; stopRinging();
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