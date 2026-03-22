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

// ==================================================
// TOUCH SETUP
// ==================================================
#define YP A2
#define XM A3
#define YM 5
#define XP 4

TouchScreen ts = TouchScreen(XP, YP, XM, YM, 300);

// Calibration
#define TS_MINX 225
#define TS_MAXX 823
#define TS_MINY 151
#define TS_MAXY 907

#define MINPRESSURE 10
#define MAXPRESSURE 1000

// ==================================================
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
// TEMPERATURE
// ==================================================
constexpr int LM35_PIN = A0;
constexpr float VREF = 5.0f;
constexpr float ADC_MAX = 1023.0f;
constexpr float T_THRESHOLD_C = 80.0f;

bool overheatScreenActive = false;

// ==================================================
// IR SENSOR (ADDED BUT UNUSED)
// ==================================================
constexpr int IR_SENSOR_PIN = 8;
int lastIRValue = 0;

// ==================================================
// LEDs
// ==================================================
constexpr int GREEN_LED_PIN = 6;
constexpr int RED_LED_PIN   = A1;

// ==================================================
// SERVO
// ==================================================
Servo lockServo;
constexpr int SERVO_PIN = 7;
bool servoUnlocked = false;

// ==================================================
// MOTORON (2 motors)
// ==================================================
MotoronI2C mc(16);

constexpr int MOTOR_SPEED = 600;
constexpr unsigned long MOTOR_RUN_TIME_MS = 5000;

// ==================================================
// FINGERPRINT
// ==================================================
Adafruit_Fingerprint finger = Adafruit_Fingerprint(&Serial1);

struct User {
  uint16_t fid;
  const char* name;
};

User users[] = {
  {1, "Kaya"},
  {2, "Rahul"},
  {3, "Ethan"},
  {4, "Igor"}
};

// ==================================================
// LED FUNCTIONS
// ==================================================
void flashGreenLED() {
  digitalWrite(GREEN_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(GREEN_LED_PIN, LOW);
}

void flashRedLED() {
  digitalWrite(RED_LED_PIN, HIGH);
  delay(1000);
  digitalWrite(RED_LED_PIN, LOW);
}

// ==================================================
// SERVO
// ==================================================
void toggleServoLock() {
  if (servoUnlocked) {
    lockServo.write(0);
  } else {
    lockServo.write(90);
  }
  servoUnlocked = !servoUnlocked;
}

// ==================================================
// MOTOR FUNCTIONS
// ==================================================
void stopMotors() {
  mc.setSpeed(1, 0);
  mc.setSpeed(2, 0);
}

void runMotors() {
  mc.setSpeed(1, MOTOR_SPEED);
  mc.setSpeed(2, MOTOR_SPEED);
  delay(MOTOR_RUN_TIME_MS);
  stopMotors();
}

// ==================================================
// TOUCH
// ==================================================
bool readTouch(int &x, int &y) {
  TSPoint p = ts.getPoint();

  pinMode(XP, OUTPUT);
  pinMode(XM, OUTPUT);
  pinMode(YP, OUTPUT);
  pinMode(YM, OUTPUT);

  if (p.z < MINPRESSURE || p.z > MAXPRESSURE) return false;

  x = map(p.y, TS_MINY, TS_MAXY, 0, tft.width());
  y = map(p.x, TS_MAXX, TS_MINX, 0, tft.height());

  delay(150);
  return true;
}

// ==================================================
void drawUI() {
  tft.fillScreen(ILI9341_BLACK);

  for (int i = 0; i < 4; i++) {
    tft.setCursor(20, 40 + i * 40);
    tft.print(names[i]);
  }

  tft.setCursor(20, 200);
  tft.print("GO");
}

// ==================================================
void handleTouch() {
  int x, y;
  if (!readTouch(x, y)) return;

  for (int i = 0; i < 4; i++) {
    if (y > 40 + i * 40 && y < 40 + i * 40 + 30) {
      selected = i;
      selectedFID = users[i].fid;
      Serial.println(names[i]);
    }
  }

  if (y > 200) {
    runMotors();
  }
}

// ==================================================
int getFingerprintID() {
  if (finger.getImage() != FINGERPRINT_OK) return -1;
  if (finger.image2Tz() != FINGERPRINT_OK) return -1;
  if (finger.fingerFastSearch() != FINGERPRINT_OK) return -1;
  return finger.fingerID;
}

// ==================================================
void handleFingerprint() {
  int id = getFingerprintID();
  if (id < 0) return;

  if (id == selectedFID) {
    flashGreenLED();
    toggleServoLock();
  } else {
    flashRedLED();
  }
}

// ==================================================
void setup() {
  Serial.begin(9600);

  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(IR_SENSOR_PIN, INPUT);

  tft.begin();
  tft.setRotation(1);
  drawUI();

  lockServo.attach(SERVO_PIN);

  Serial1.begin(115200);
  finger.verifyPassword();

  Wire.begin();
  mc.reinitialize();
  mc.disableCrc();
  mc.clearResetFlag();

  stopMotors();
}

// ==================================================
void loop() {
  handleTouch();
  handleFingerprint();
}