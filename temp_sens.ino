// Overheating prevention for Arduino Uno R4 + LM35.
// LM35 output is 10 mV/°C, so T(°C) = Vout * 100.

constexpr float VREF = 5.0f;
constexpr float ADC_MAX = 1023.0f;
constexpr float T_THRESHOLD_C = 60.0f;

constexpr int LM35_PIN = A0;

// Update these to match your motor control pins.
constexpr int MOTOR_PINS[] = { 5, 6 };
constexpr size_t MOTOR_PIN_COUNT = sizeof(MOTOR_PINS) / sizeof(MOTOR_PINS[0]);

enum class SystemState {
  RUNNING,
  SHUTDOWN
};

SystemState systemState = SystemState::RUNNING;

void disableMotors() {
  for (size_t i = 0; i < MOTOR_PIN_COUNT; ++i) {
    digitalWrite(MOTOR_PINS[i], LOW);
  }
}

void showOverheatMessage() {
  // Replace with your TFT call if you have one.
  Serial.println("Overheating detected");
}

void setup() {
  Serial.begin(115200);
  analogReadResolution(10);  // Match ADC_MAX=1023 for Uno R4.

  /*for (size_t i = 0; i < MOTOR_PIN_COUNT; ++i) {
    pinMode(MOTOR_PINS[i], OUTPUT);
    digitalWrite(MOTOR_PINS[i], HIGH);
  }*/
}

void loop() {
  /*if (systemState == SystemState::SHUTDOWN) {
    // Stay in shutdown; remove if you want auto-recovery.
    delay(500);
    return;
  }*/

  const int adcValue = analogRead(LM35_PIN);
  const float vout = (static_cast<float>(adcValue) * VREF) / ADC_MAX;
  const float tempC = vout * 100; //* 100.0f;

  Serial.print("Temp C: ");
  Serial.println(tempC, 1);

  /*if (tempC >= T_THRESHOLD_C) {
    showOverheatMessage();
    systemState = SystemState::SHUTDOWN;
    disableMotors();
  }*/

  delay(200);
}
