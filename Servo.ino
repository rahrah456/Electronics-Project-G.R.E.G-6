#include <Servo.h>

Servo servo;  // Initialize a Servo object to manage a servo
bool correct = true;

void setup() {
  servo.attach(9);  // Connects the servo on pin 9 to the servo object
  servo.write(0);   // Moves the servo to 0 degrees immediately upon startup
}

void loop() {
  if (correct) {
    for (int pos = 0; pos <= 90; pos += 1) {  // Gradually move the servo from 0 to 90 degrees
      servo.write(pos);  // Set servo position to 'pos' degrees
      delay(5);         // Delay 10ms to allow the servo to reach the new position
    }
    // Press a button on TFT to lock after usage
    for (int pos = 90; pos >= 0; pos -= 1) {  // Gradually move the servo from 90 back to 0 degrees
      servo.write(pos);                        // Set servo position to 'pos' degrees
      delay(5);                               // Delay 10ms to allow the servo to reach the new position
    }
  }
  else{}   // TFT Screen shows error??
}