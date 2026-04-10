#include <SPI.h>
#include <Wire.h>
#include <Servo.h>
#include <Motoron.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <TouchScreen.h>
#include <Adafruit_Fingerprint.h>

// ==================================================
// TFT SETUP
// ==================================================
#define TFT_CS   10
#define TFT_DC   3
#define TFT_RST  2
Adafruit_ILI9341 tft(TFT_CS, TFT_DC, TFT_RST);

// Touch pins
#define YP A2
#define XM A3
#define YM 5
#define XP 4

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Touch calibration
#define TS_MINX 225
#define TS_MAXX 823
#define TS_MINY 151
#define TS_MAXY 907

#define MINPRESSURE 10
#define MAXPRESSURE 1000

const char* names[4] = {"Kaya", "Rahul", "Ethan", "Igor"};
int selected = -1;
int selectedFID = -1;

struct Btn {
  int16_t x, y, w, h;
};

Btn nameBtns[4];
Btn goBtn;
Btn okBtn;

// ==================================================
// TOUCH PROTECTION
// ==================================================
bool touchLocked = false;
unsigned long ignoreTouchUntil = 0;

constexpr unsigned long TOUCH_IGNORE_AFTER_FP_MS = 1200;
constexpr int TOUCH_STABLE_TOLERANCE = 12;

// ==================================================
// TEMPERATURE SENSOR
// ==================================================
constexpr float VREF = 5.0f;
constexpr float ADC_MAX = 1023.0f;
constexpr float T_THRESHOLD_C = 80.0f;
constexpr int LM35_PIN = A0;

constexpr unsigned long OVERHEAT_ACK_DELAY_MS = 180000; // 3 minutes

bool overheatScreenActive = false;
float lastTempC = 0.0f;
unsigned long lastOverheatAcknowledgeTime = 0;

// ==================================================
// LED PINS
// ==================================================
constexpr int GREEN_LED_PIN = 6;
constexpr int RED_LED_PIN   = A1;
constexpr unsigned long LED_ON_TIME_MS = 1500;

// ==================================================
// SERVO SETUP
// ==================================================
Servo lockServo;
constexpr int SERVO_PIN = 7;
constexpr int LOCKED_ANGLE = 0;
constexpr int UNLOCKED_ANGLE = 90;
bool servoUnlocked = false;

// ==================================================
// MOTORON SETUP
// Motor 1 = left back wheel
// Motor 2 = right back wheel
// ==================================================
MotoronI2C mc(16);  // default address 0x10

constexpr int MOTOR_SPEED = 600;
constexpr unsigned long MOTOR_RUN_TIME_MS = 5000; // 5 seconds

// ==================================================
// IR SENSOR
// ==================================================
constexpr int IR_SENSOR_PIN = 8;
int lastIRValue = 0;

// ==================================================
// FINGERPRINT SETUP
// ==================================================
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial1);

struct User {
  uint16_t fid;
  uint8_t node;
  const char* name;
};

User users[] = {
  {1, 1, "Kaya"},
  {2, 2, "Rahul"},
  {3, 3, "Ethan"},
  {4, 4, "Igor"}
};

const int USER_COUNT = sizeof(users) / sizeof(users[0]);

// ==================================================
// LED FUNCTIONS
// ==================================================
void ledsOff() {
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
}

void flashGreenLED() {
  ledsOff();
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(LED_ON_TIME_MS);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void flashRedLED() {
  ledsOff();
  digitalWrite(RED_LED_PIN, HIGH);
  delay(LED_ON_TIME_MS);
  digitalWrite(RED_LED_PIN, LOW);
}

// ==================================================
// SERVO FUNCTIONS
// ==================================================
void moveServoSmooth(int fromAngle, int toAngle) {
  if (fromAngle < toAngle) {
    for (int pos = fromAngle; pos <= toAngle; pos++) {
      lockServo.write(pos);
      delay(5);
    }
  } else {
    for (int pos = fromAngle; pos >= toAngle; pos--) {
      lockServo.write(pos);
      delay(5);
    }
  }
}

void toggleServoLock() {
  if (servoUnlocked) {
    moveServoSmooth(UNLOCKED_ANGLE, LOCKED_ANGLE);
    servoUnlocked = false;
    Serial.println("Servo moved to UNLOCKED position");
  } else {
    moveServoSmooth(LOCKED_ANGLE, UNLOCKED_ANGLE);
    servoUnlocked = true;
    Serial.println("Servo moved to LOCKED position");
  }
}

// ==================================================
// MOTOR FUNCTIONS
// ==================================================
void stopMotors() {
  mc.setSpeed(1, 0);
  mc.setSpeed(2, 0);
}

void runMotorsForward() {
  mc.setSpeed(1, -MOTOR_SPEED);
  mc.setSpeed(2, MOTOR_SPEED);
}

void runMotorsFor5Seconds() {
  Serial.println("Motors running...");
  runMotorsForward();
  delay(MOTOR_RUN_TIME_MS);
  stopMotors();
  Serial.println("Motors stopped");
}

// ==================================================
// IR SENSOR
// ==================================================
void handleIRSensor() {
  lastIRValue = digitalRead(IR_SENSOR_PIN);
}

// ==================================================
// TOUCH HELPERS
// ==================================================
bool hit(const Btn& b, int16_t px, int16_t py) {
  return (px >= b.x && px < b.x + b.w && py >= b.y && py < b.y + b.h);
}

bool readTouchPointRaw(int16_t &screenX, int16_t &screenY) {
  TSPoint p = ts.getPoint();

  pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  pinMode(YM, OUTPUT);

  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) return false;

  screenX = map(p.y, TS_MINY, TS_MAXY, 0, tft.width());
  screenY = map(p.x, TS_MAXX, TS_MINX, 0, tft.height());

  if (screenX < 0 || screenX >= tft.width()) return false;
  if (screenY < 0 || screenY >= tft.height()) return false;

  return true;
}

// Require two similar touch reads before accepting a press
bool readStableTouchPoint(int16_t &screenX, int16_t &screenY) {
  int16_t x1, y1, x2, y2;

  if (!readTouchPointRaw(x1, y1)) return false;
  delay(25);
  if (!readTouchPointRaw(x2, y2)) return false;

  if (abs(x1 - x2) > TOUCH_STABLE_TOLERANCE) return false;
  if (abs(y1 - y2) > TOUCH_STABLE_TOLERANCE) return false;

  screenX = (x1 + x2) / 2;
  screenY = (y1 + y2) / 2;
  return true;
}

// ==================================================
// BUTTON DRAWING
// ==================================================
void drawButton(const Btn& b, const char* label, bool selectedState) {
  uint16_t fill = selectedState ? ILI9341_GREEN : ILI9341_DARKGREY;

  tft.fillRoundRect(b.x, b.y, b.w, b.h, 10, fill);
  tft.drawRoundRect(b.x, b.y, b.w, b.h, 10, ILI9341_WHITE);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(b.x + 12, b.y + b.h / 2 - 8);
  tft.print(label);
}

// ==================================================
// MAIN UI
// ==================================================
void redrawUI() {
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(12, 8);
  tft.print("Select User:");

  int16_t margin = 10;
  int16_t top = 35;
  int16_t gap = 10;

  int16_t bw = (tft.width() - (2 * margin) - gap) / 2;
  int16_t bh = 50;

  nameBtns[0] = {margin, top, bw, bh};
  nameBtns[1] = {margin + bw + gap, top, bw, bh};
  nameBtns[2] = {margin, top + bh + gap, bw, bh};
  nameBtns[3] = {margin + bw + gap, top + bh + gap, bw, bh};

  for (int i = 0; i < 4; i++) {
    drawButton(nameBtns[i], names[i], selected == i);
  }

  int16_t goY = top + (2 * bh) + (2 * gap) + 5;
  goBtn = {margin, goY, tft.width() - 2 * margin, 45};
  drawButton(goBtn, "GO", false);

  tft.setCursor(12, 222);
  tft.print("Chosen: ");
  if (selected >= 0) tft.print(names[selected]);
  else tft.print("-");
}

// ==================================================
// OVERHEAT SCREEN
// ==================================================
void drawOverheatScreen(float tempC) {
  overheatScreenActive = true;

  tft.fillScreen(ILI9341_RED);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.setCursor(30, 20);
  tft.print("WARNING");

  tft.setTextSize(2);
  tft.setCursor(35, 65);
  tft.print("OVERHEATING");

  tft.setCursor(35, 95);
  tft.print("Temp: ");
  tft.print(tempC, 1);
  tft.print(" C");

  tft.setCursor(12, 130);
  tft.print("Stop the robot.");

  tft.setCursor(12, 155);
  tft.print("Place it in a");

  tft.setCursor(12, 180);
  tft.print("secure location.");

  tft.setCursor(12, 205);
  tft.print("Power off to cool.");

  okBtn = {220, 190, 80, 35};
  drawButton(okBtn, "OK", false);
}

void clearOverheatScreen() {
  overheatScreenActive = false;
  redrawUI();
}

// ==================================================
// TEMPERATURE
// ==================================================
float readTemperatureC() {
  int adc = analogRead(LM35_PIN);
  float vout = (adc * VREF) / ADC_MAX;
  return vout * 100.0;
}

void handleTemperature() {
  static unsigned long lastRead = 0;

  if (millis() - lastRead < 500) return;
  lastRead = millis();

  lastTempC = readTemperatureC();

  if (overheatScreenActive) return;
  if (millis() - lastOverheatAcknowledgeTime < OVERHEAT_ACK_DELAY_MS) return;

  if (lastTempC >= T_THRESHOLD_C) {
    drawOverheatScreen(lastTempC);
  }
}

// ==================================================
// TOUCH HANDLING
// ==================================================
void handleTouch() {
  int16_t x, y;
  bool touched = readStableTouchPoint(x, y);

  // ignore touch during cooldown after fingerprint scan
  if (millis() < ignoreTouchUntil) {
    if (!touched) touchLocked = false;
    return;
  }

  // unlock only after full release
  if (!touched) {
    touchLocked = false;
    return;
  }

  // only one action per press
  if (touchLocked) return;
  touchLocked = true;

  if (overheatScreenActive) {
    if (hit(okBtn, x, y)) {
      lastOverheatAcknowledgeTime = millis();
      clearOverheatScreen();
    }
    return;
  }

  for (int i = 0; i < 4; i++) {
    if (hit(nameBtns[i], x, y)) {
      if (selected != i) {
        selected = i;
        selectedFID = users[i].fid;

        redrawUI();

        Serial.print("Selected user: ");
        Serial.println(names[i]);
      }
      return;
    }
  }

  if (hit(goBtn, x, y)) {
    Serial.println("GO pressed");
    runMotorsFor5Seconds();
  }
}

// ==================================================
// FINGERPRINT
// return values:
//  -2 = no finger present
//  -1 = finger present but not recognized / failed search
// >= 0 = matched fingerprint ID
// ==================================================
User* findUserByFID(uint16_t fid) {
  for (int i = 0; i < USER_COUNT; i++) {
    if (users[i].fid == fid) return &users[i];
  }
  return nullptr;
}

int getFingerprintID() {
  uint8_t p = finger.getImage();

  if (p == FINGERPRINT_NOFINGER) return -2;
  if (p != FINGERPRINT_OK) return -1;

  p = finger.image2Tz();
  if (p != FINGERPRINT_OK) return -1;

  p = finger.fingerFastSearch();
  if (p != FINGERPRINT_OK) return -1;

  return finger.fingerID;
}

void handleFingerprint() {
  static unsigned long lastScan = 0;

  if (overheatScreenActive) return;
  if (millis() - lastScan < 1500) return;

  int id = getFingerprintID();

  if (id == -2) return;

  // lock touch selection changes for a while after any fingerprint attempt
  ignoreTouchUntil = millis() + TOUCH_IGNORE_AFTER_FP_MS;
  touchLocked = true;

  // finger present but not recognized
  if (id == -1) {
    Serial.println("Unrecognized fingerprint");
    // IMPORTANT: do not change selected or selectedFID here
    flashRedLED();
    lastScan = millis();
    return;
  }

  Serial.print("Scanned ID: ");
  Serial.println(id);

  if (selectedFID < 0) {
    Serial.println("No user selected");
    flashRedLED();
    lastScan = millis();
    return;
  }

  if (id == selectedFID) {
    Serial.println("MATCH");
    toggleServoLock();
    flashGreenLED();
  } else {
    Serial.println("NO MATCH");
    // IMPORTANT: wrong user fingerprint should not switch selected user
    flashRedLED();
  }

  lastScan = millis();
}

// ==================================================
// SETUP
// ==================================================
void setup() {
  Serial.begin(9600);
  delay(2000);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT);

  ledsOff();

  tft.begin();
  tft.setRotation(1);
  redrawUI();

  lockServo.attach(SERVO_PIN);
  lockServo.write(LOCKED_ANGLE);
  servoUnlocked = false;

  Serial1.begin(115200);

  if (finger.verifyPassword()) {
    Serial.println("Fingerprint sensor ready");
  } else {
    Serial.println("Fingerprint sensor error");
  }

  Wire.begin();
  mc.reinitialize();
  mc.disableCrc();
  mc.clearResetFlag();

  mc.setMaxAcceleration(1, 200);
  mc.setMaxDeceleration(1, 300);
  mc.setMaxAcceleration(2, 200);
  mc.setMaxDeceleration(2, 300);

  stopMotors();
}

// ==================================================
// LOOP
// ==================================================
void loop() {
  handleTemperature();
  handleTouch();
  handleFingerprint();
  handleIRSensor();
}