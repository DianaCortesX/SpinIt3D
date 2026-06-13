#include <TFT_eSPI.h>                 // Loads the TFT_eSPI display library.
#include <SPI.h>                      // Loads the SPI bus library used by the display and touch controller.
#include <XPT2046_Touchscreen.h>      // Loads the XPT2046 resistive touch controller library.

#include "home_screen_image.h"        // Loads the home screen RGB565 image array.
#include "safety_notice_image.h"      // Loads the safety notice RGB565 image array.
#include "task_selection_image.h"     // Loads the task selection RGB565 image array.
#include "task_ended_image.h"         // Loads the task completed RGB565 image array.
// The running and error pages are drawn with code to fit in 2 MB flash.

// ---------------- DISPLAY AND TOUCH PINS ----------------

#define TOUCH_CS_PIN 22               // Selects the XPT2046 touch chip on GP22.
#define TFT_CS_PIN 17                 // Selects the TFT display chip on GP17.
#define TFT_SPI_SPEED 62500000        // Runs the display SPI bus fast after every touch read.

// ---------------- CENTRIFUGE HARDWARE PINS ----------------

#define MOTOR_PWM_PIN 3               // Sends PWM from GP3 to the PWM-to-0-10V motor speed module.
#define LID_HALL_PIN 13               // Reads the lid hall sensor on GP13, physical pin 17.
#define VIBRATION_PIN 15              // Reads the vibration sensor connected to GP15.
#define SOLENOID_PIN 7                // Drives the solenoid MOSFET on GP7.
#define TEMP_SENSOR_PIN 26            // Reads motor temperature on ADC0/GP26; adjust if your sensor differs.

// ---------------- SCREEN SIZE ----------------

#define SCREEN_W 480                  // Sets the display width in landscape pixels.
#define SCREEN_H 320                  // Sets the display height in landscape pixels.
#define IMAGE_PIXELS (SCREEN_W * SCREEN_H) // Calculates the number of pixels in one full-screen image.

// ---------------- SAFETY SETTINGS ----------------

#define LID_CLOSED_LEVEL LOW          // Sets the hall sensor level that means the lid is closed.
#define VIBRATION_SAFE_LEVEL HIGH      // Sets the vibration sensor level that means vibration is normal.
#define TEMP_C_MAX_SAFE 65.0           // Stops the task when motor temperature reaches 65 degrees Celsius.
#define TEMP_TRIP_DELAY_MS 5000        // Requires an over-temperature reading to persist for five seconds before stopping.
#define TEMP_SAMPLE_COUNT 16           // Averages multiple ADC samples to reduce electrical noise from the motor.
#define VIBRATION_SURGE_PPS 100        // Uses a strict sustained-surge limit once the rotor is near target speed.
#define VIBRATION_ACCEL_SURGE_PPS 300  // Allows the rotor to pass through normal acceleration resonance without false trips.
#define MOTOR_START_PERCENT 5          // Starts promptly at a gentle 5 percent command before feedback ramps upward.
#define MOTOR_RAMP_STEP_PERCENT 2      // Raises motor command by two percentage points during each soft-start step.
#define MOTOR_RAMP_INTERVAL_MS 1000    // Waits one second between soft-start power increases.
#define VIBRATION_STARTUP_GRACE_MS 90000 // Ignores vibration surges during the first five seconds of normal motor acceleration.
#define SCREEN_REFRESH_MS 1000        // Updates the task running screen one time per second.
#define SOLENOID_PULSE_MS 5000        // Powers the lid unlock solenoid for 5 seconds.
#define SOLENOID_ON_LEVEL HIGH        // Matches your MOSFET module where HIGH powers the solenoid.
#define SOLENOID_OFF_LEVEL LOW        // Matches your MOSFET module where LOW turns the solenoid off.

const uint16_t UI_GREEN = 0x9FB1;     // Defines the pale green used for positive action buttons.
const uint16_t UI_RED = 0xF9E6;       // Defines the bright red used for stop/back action buttons.

// ---------------- TASK OPTIONS ----------------
const int rpmChoices[] = {5000, 8000, 10000};  // Stores the RPM buttons shown on the task selection screen.
const int motorTargetPercentChoices[] = {43, 70, 85}; // Stores the fixed motor command maintained by each selected task.
const int timeChoices[] = {10, 15, 20};        // Stores the minute buttons shown on the task selection screen.

// ---------------- BUTTON AREAS ----------------
// Tune these rectangles after touching the real background artwork.
// x and y are the top-left corner; w and h are width and height.

struct ButtonArea {                   // Groups button rectangle values into one small type.
  int x;                              // Stores the button left edge.
  int y;                              // Stores the button top edge.
  int w;                              // Stores the button width.
  int h;                              // Stores the button height.
};                                    // Ends the ButtonArea type.

const ButtonArea homeStartBtn = {128, 128, 242, 81};       // Defines the START button from the updated home image pixels.
const ButtonArea safetyContinueBtn = {102, 249, 276, 54};  // Defines the continue button from the updated safety image pixels.
const ButtonArea instructionOpenBtn = {60, 220, 210, 52};  // Defines the open/close lid button on the instructions page.
const ButtonArea instructionNextBtn = {300, 220, 120, 52};  // Defines the next button on the instructions page.
const ButtonArea rpmBtns[3] = {{111, 81, 117, 63}, {228, 81, 108, 63}, {336, 81, 112, 63}}; // Defines RPM buttons from updated image pixels.
const ButtonArea timeBtns[3] = {{111, 160, 117, 64}, {228, 160, 108, 64}, {336, 160, 112, 64}}; // Defines time buttons from updated image pixels.
const ButtonArea taskStartBtn = {170, 237, 226, 72};       // Defines the START button from the updated task select image.
const ButtonArea taskBackBtn = {12, 253, 132, 45};         // Defines the Back button from the updated task select image.
const ButtonArea stopBtn = {198, 266, 86, 42};             // Defines the STOP button on the task running screen.
const ButtonArea errorHomeBtn = {172, 222, 136, 48};       // Defines the home button on the error screen.
const ButtonArea doneOpenBtn = {150, 169, 180, 43};        // Defines the open/close lid button from the completed image.
const ButtonArea doneHomeBtn = {101, 239, 278, 65};        // Defines the homescreen button from the completed image.
const ButtonArea doneSolenoidStatusBox = {70, 213, 340, 24}; // Defines the lock-status gap between completed-page buttons.

// ---------------- OVERLAY AREAS ----------------

const ButtonArea errorTextBox = {88, 138, 304, 44};        // Defines where error text is printed over the error image.
const ButtonArea rpmTargetBox = {190, 76, 120, 22};        // Defines where selected RPM is printed on running image.
const ButtonArea rpmCurrentBox = {190, 106, 120, 22};      // Defines where measured RPM is printed on running image.
const ButtonArea timeLeftBox = {190, 136, 120, 22};        // Defines where remaining time is printed on running image.
const ButtonArea vibrationBox = {190, 166, 150, 22};       // Defines where vibration status is printed on running image.
const ButtonArea temperatureBox = {190, 196, 150, 22};     // Defines where temperature is printed on running image.
const ButtonArea lidBox = {190, 226, 150, 22};             // Defines where lid status is printed on running image.
const ButtonArea progressBar = {86, 250, 308, 10};         // Defines where progress is drawn on running image.

// ---------------- OBJECTS ----------------

TFT_eSPI tft = TFT_eSPI();                 // Creates the display object.
XPT2046_Touchscreen ts(TOUCH_CS_PIN);      // Creates the touch object using the touch chip-select pin.

// ---------------- SCREEN STATE ----------------

enum Screen {                              // Names every UI page the program can display.
  SCREEN_HOME,                             // Represents the home screen.
  SCREEN_SAFETY,                           // Represents the safety notice screen.
  SCREEN_INSTRUCTIONS,                     // Represents the lid and loading instruction screen.
  SCREEN_TASK_SELECT,                      // Represents the task selection screen.
  SCREEN_TASK_RUNNING,                     // Represents the active task screen.
  SCREEN_TASK_DONE,                        // Represents the task completed screen.
  SCREEN_ERROR                             // Represents the error screen.
};                                         // Ends the Screen enum.

Screen currentScreen = SCREEN_HOME;        // Stores which screen is currently visible.

// ---------------- TOUCH STATE ----------------

int touchX = 0;                            // Stores the latest mapped touch x coordinate.
int touchY = 0;                            // Stores the latest mapped touch y coordinate.

// ---------------- TASK STATE ----------------

int selectedRpmIndex = -1;                 // Stores selected RPM choice; -1 means nothing selected.
int selectedTimeIndex = -1;                // Stores selected time choice; -1 means nothing selected.
int targetRpm = rpmChoices[0];             // Stores the selected target RPM value.
int motorPwm = 0;                          // Stores the current PWM output value from 0 to 255.
int motorSpeedPercent = 0;                 // Stores the latest serial motor speed command from 0 to 100.
int taskMotorTargetPercent = 0;            // Stores the fixed motor command selected for the active task.
bool taskActive = false;                   // Tracks whether a centrifuge task is currently running.
unsigned long taskStartMs = 0;             // Stores the millis() value when the task started.
unsigned long taskDurationMs = 0;          // Stores the selected task length in milliseconds.
unsigned long lastMotorRampMs = 0;          // Stores when motor power was last increased during soft start.
unsigned long lastOverlayMs = 0;           // Stores the millis() value of the last running-screen text update.
unsigned long tempOverLimitStartMs = 0;    // Stores when a continuous over-temperature condition began.
volatile unsigned long vibrationPulses = 0; // Counts vibration sensor pulses inside the interrupt.
unsigned long lastVibrationMs = 0;          // Stores the time of the last vibration rate calculation.
unsigned long vibrationRate = 0;            // Stores vibration pulses per second for display.
unsigned long previousVibrationRate = 0;    // Stores the previous one-second vibration reading for surge detection.
bool vibrationBaselineReady = false;        // Tracks whether the first vibration reading established a comparison baseline.
bool vibrationSurgeDetected = false;        // Becomes true when vibration suddenly rises by the configured surge amount.
bool vibrationSurgePending = false;         // Tracks a possible surge until the following reading confirms it remains elevated.
unsigned long vibrationPreSurgeRate = 0;    // Stores the stable vibration rate immediately before a possible surge.
unsigned long vibrationPendingThreshold = 0; // Stores the surge threshold used when a possible surge began.
bool solenoidPulseActive = false;           // Tracks whether the solenoid unlock pulse is active.
unsigned long solenoidPulseStartMs = 0;     // Stores when the solenoid unlock pulse started.
unsigned long lastSolenoidUiMs = 0;         // Stores when solenoid status text was last refreshed.

float readTemperatureC();                   // Lets earlier safety helpers call the temperature reader.
void drawSolenoidStatus(ButtonArea box);     // Lets the solenoid timer refresh status text when it finishes.

const uint16_t *imageForScreen(Screen screen) {              // Returns the image array for a given screen.
  if (screen == SCREEN_HOME) return home_screen_image;        // Uses the home image for the home screen.
  if (screen == SCREEN_SAFETY) return safety_notice_image;    // Uses the safety image for the safety screen.
  if (screen == SCREEN_TASK_SELECT) return task_selection_image; // Uses the task selection image for task setup.
  if (screen == SCREEN_TASK_DONE) return task_ended_image;    // Uses the task completed image after a successful task.
  return home_screen_image;                                   // Gives restoreArea() a safe fallback image.
}                                                            // Ends imageForScreen().

void setImage(const uint16_t *img) {                         // Draws one full-screen RGB565 image quickly.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Deselects the touch chip before display transfer.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Deselects the TFT before starting a clean transaction.
  SPI.beginTransaction(SPISettings(TFT_SPI_SPEED, MSBFIRST, SPI_MODE0)); // Forces fast display SPI settings.
  tft.startWrite();                                          // Starts a TFT_eSPI write session.
  tft.setAddrWindow(0, 0, SCREEN_W, SCREEN_H);                // Selects the entire display as the drawing area.
  tft.pushColors((uint16_t *)img, IMAGE_PIXELS, true);        // Pushes all image pixels to the display.
  tft.endWrite();                                            // Ends the TFT_eSPI write session.
  SPI.endTransaction();                                      // Releases the SPI bus transaction.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Leaves the touch chip deselected.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Leaves the TFT chip deselected.
}                                                            // Ends setImage().

void showScreen(Screen screen) {                             // Changes screens and draws the matching image.
  currentScreen = screen;                                    // Saves the new current screen.
  if (screen == SCREEN_HOME || screen == SCREEN_SAFETY || screen == SCREEN_TASK_SELECT || screen == SCREEN_TASK_DONE) setImage(imageForScreen(screen)); // Draws image screens.
}                                                            // Ends showScreen().

void restoreArea(ButtonArea area) {                          // Restores a rectangular part of the current background.
  const uint16_t *img = imageForScreen(currentScreen);        // Gets the background image for the current screen.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Deselects touch before display writing.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Deselects TFT before a clean SPI transaction.
  SPI.beginTransaction(SPISettings(TFT_SPI_SPEED, MSBFIRST, SPI_MODE0)); // Uses fast display SPI speed.
  tft.startWrite();                                          // Starts a display write session.
  for (int row = 0; row < area.h; row++) {                    // Loops through each row in the rectangle.
    int sourceIndex = (area.y + row) * SCREEN_W + area.x;     // Calculates the row start in the image array.
    tft.setAddrWindow(area.x, area.y + row, area.w, 1);       // Selects one row of the rectangle.
    tft.pushColors((uint16_t *)(img + sourceIndex), area.w, true); // Pushes that image row to the display.
  }                                                          // Ends the row loop.
  tft.endWrite();                                            // Ends the display write session.
  SPI.endTransaction();                                      // Releases the SPI bus.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Leaves touch deselected.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Leaves TFT deselected.
}                                                            // Ends restoreArea().

bool insideButton(int tx, int ty, ButtonArea button) {        // Tests whether a touch is inside a button rectangle.
  return tx >= button.x && tx <= button.x + button.w && ty >= button.y && ty <= button.y + button.h; // Returns true inside.
}                                                            // Ends insideButton().

bool getTouchPoint() {                                       // Reads one touch point and maps it to screen pixels.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Deselects the display before touch reading.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Starts with touch deselected for a clean read.
  if (!ts.touched()) {                                       // Checks whether the screen is currently touched.
    digitalWrite(TOUCH_CS_PIN, HIGH);                        // Keeps touch deselected if there is no touch.
    digitalWrite(TFT_CS_PIN, HIGH);                          // Keeps TFT deselected if there is no touch.
    return false;                                            // Reports no touch to the caller.
  }                                                          // Ends the no-touch branch.
  TS_Point p = ts.getPoint();                                // Reads the raw touch point from the XPT2046.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Releases the touch chip immediately after reading.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Keeps the TFT deselected until display code needs it.
  touchX = map(p.x, 200, 3900, 0, SCREEN_W);                 // Converts raw X touch value to screen X pixels.
  touchY = map(p.y, 200, 3900, 0, SCREEN_H);                 // Converts raw Y touch value to screen Y pixels.
  touchX = constrain(touchX, 0, SCREEN_W - 1);                // Keeps X inside the valid display range without flipping.
  touchY = 311 - touchY;                                     // Applies your calibrated Y flip value.
  touchY = constrain(touchY, 0, SCREEN_H - 1);                // Keeps Y inside the valid display range after calibration.
  return true;                                               // Reports that a valid touch was read.
}                                                            // Ends getTouchPoint().

void waitForRelease() {                                      // Waits until the user lifts their finger.
  while (ts.touched()) {                                     // Loops while the touch controller senses contact.
    digitalWrite(TFT_CS_PIN, HIGH);                          // Keeps the display chip deselected during polling.
    delay(5);                                                // Pauses briefly to avoid busy-waiting too hard.
  }                                                          // Ends the release loop.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Leaves the touch chip deselected.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Leaves the display chip deselected.
}                                                            // Ends waitForRelease().

void pressEffect(ButtonArea button) {                        // Shows a fast press animation without green fill.
  digitalWrite(TOUCH_CS_PIN, HIGH);                          // Deselects touch before drawing.
  digitalWrite(TFT_CS_PIN, HIGH);                            // Deselects TFT before opening a display transaction.
  SPI.beginTransaction(SPISettings(TFT_SPI_SPEED, MSBFIRST, SPI_MODE0)); // Uses fast display SPI speed.
  tft.startWrite();                                          // Starts a display write session.
  tft.drawRoundRect(button.x, button.y, button.w, button.h, 6, TFT_DARKGREY); // Draws a dark outline.
  tft.drawRoundRect(button.x + 2, button.y + 2, button.w - 4, button.h - 4, 5, TFT_LIGHTGREY); // Draws a light inset.
  tft.endWrite();                                            // Ends the display write session.
  SPI.endTransaction();                                      // Releases the SPI bus.
  delay(70);                                                 // Leaves the press effect visible briefly.
  if (currentScreen == SCREEN_HOME || currentScreen == SCREEN_SAFETY || currentScreen == SCREEN_TASK_SELECT || currentScreen == SCREEN_TASK_DONE) restoreArea(button); // Restores artwork on image-backed screens.
}                                                            // Ends pressEffect().

void vibrationPulseISR() { vibrationPulses++; }              // Adds one pulse when the vibration sensor output changes.

void setMotorPercent(int speedPercent) {                     // Sets motor command as a percent from 0 to 100.
  speedPercent = constrain(speedPercent, 0, 100);             // Keeps the requested percent inside range.
  if (speedPercent > 0 && !lidClosed()) {                     // Blocks every nonzero motor command while the lid is open.
    speedPercent = 0;                                         // Forces the motor command to zero for the lid interlock.
    Serial.println("Motor blocked: lid is open.");            // Reports why the requested motor command was rejected.
  }                                                          // Ends lid interlock branch.
  motorSpeedPercent = speedPercent;                           // Stores the latest percent command.
  motorPwm = map(speedPercent, 0, 100, 0, 255);               // Converts percent to Arduino PWM value.
  analogWrite(MOTOR_PWM_PIN, motorPwm);                       // Sends PWM to the 0-10V module.
}                                                            // Ends setMotorPercent().

void setMotorPwmValue(int pwmValue) {                         // Sets a fine-resolution motor command directly in PWM counts.
  pwmValue = constrain(pwmValue, 0, 255);                     // Keeps the requested PWM command inside its legal range.
  if (pwmValue > 0 && !lidClosed()) {                         // Blocks every nonzero motor command while the lid is open.
    pwmValue = 0;                                             // Forces the motor command to zero for the lid interlock.
    Serial.println("Motor blocked: lid is open.");            // Reports why the requested motor command was rejected.
  }                                                          // Ends lid interlock branch.
  motorPwm = pwmValue;                                        // Stores the exact fine-resolution PWM command.
  motorSpeedPercent = (motorPwm * 100 + 127) / 255;           // Stores the nearest whole-percent value for status and serial output.
  analogWrite(MOTOR_PWM_PIN, motorPwm);                       // Sends the fine-resolution PWM command to the 0-10V module.
}                                                            // Ends setMotorPwmValue().

void stopMotor() {                                           // Commands zero speed and explicitly clears the 0-10V control signal.
  setMotorPercent(0);                                        // Updates stored command values and writes zero PWM.
  analogWrite(MOTOR_PWM_PIN, 0);                             // Redundantly forces GP3 PWM to zero for shutdown certainty.
}                                                            // Ends stopMotor().

void handleSerialMotorCommand() {                             // Reads motor speed commands from Serial Monitor.
  if (!Serial.available()) return;                            // Does nothing if no serial input is waiting.
  int speedPercent = Serial.parseInt();                       // Reads a typed integer from Serial Monitor.
  speedPercent = constrain(speedPercent, 0, 100);              // Limits the command to 0-100 percent.
  setMotorPercent(speedPercent);                              // Sends the command to the PWM-to-0-10V module.
  Serial.print("Speed: ");                                    // Prints the command label.
  Serial.print(motorSpeedPercent);                            // Prints the percent command.
  Serial.print("%   PWM: ");                                  // Prints the PWM label.
  Serial.println(motorPwm);                                   // Prints the 0-255 PWM value.
  while (Serial.available()) Serial.read();                    // Clears any remaining newline characters.
}                                                            // Ends handleSerialMotorCommand().

bool lidClosed() { return digitalRead(LID_HALL_PIN) == LID_CLOSED_LEVEL; } // Returns true when the lid is closed.
bool vibrationSafe() { return digitalRead(VIBRATION_PIN) == VIBRATION_SAFE_LEVEL; } // Returns true when vibration is normal.
bool motorTempSafe() { return readTemperatureC() < TEMP_C_MAX_SAFE; } // Returns true when motor temperature is below the limit.
void solenoidOff() { digitalWrite(SOLENOID_PIN, SOLENOID_OFF_LEVEL); } // Turns the solenoid driver off.
void solenoidOn() { digitalWrite(SOLENOID_PIN, SOLENOID_ON_LEVEL); } // Powers the solenoid driver.
void lockLid() { solenoidOff(); }                             // Keeps compatibility with older lock calls by turning solenoid off.
void unlockLid() { solenoidOff(); }                           // Keeps the solenoid off unless a timed unlock pulse is requested.

void startSolenoidPulse() {                                   // Starts a timed solenoid unlock pulse.
  solenoidPulseActive = true;                                 // Marks the unlock pulse as active.
  solenoidPulseStartMs = millis();                            // Records when the unlock pulse started.
  solenoidOn();                                               // Powers the solenoid to unlock the lid.
}                                                            // Ends startSolenoidPulse().

void updateSolenoidPulse() {                                  // Turns the solenoid off when the timed pulse finishes.
  if (!solenoidPulseActive) return;                            // Does nothing when no pulse is active.
  if (millis() - solenoidPulseStartMs < SOLENOID_PULSE_MS) return; // Waits until the configured pulse time has elapsed.
  solenoidPulseActive = false;                                // Marks the unlock pulse as finished.
  solenoidOff();                                              // Turns the solenoid off after the pulse.
  if (currentScreen == SCREEN_INSTRUCTIONS) drawSolenoidStatus({80, 190, 320, 24}); // Shows engaged status on instructions.
  if (currentScreen == SCREEN_TASK_DONE) drawSolenoidStatus(doneSolenoidStatusBox); // Shows engaged status on completed page.
}                                                            // Ends updateSolenoidPulse().

float readTemperatureC() {                                    // Reads the TMP36 temperature sensor in Celsius.
  unsigned long readingTotal = 0;                             // Stores the sum used to average several ADC samples.
  for (int i = 0; i < TEMP_SAMPLE_COUNT; i++) {               // Collects several readings to reject brief electrical noise.
    readingTotal += analogRead(TEMP_SENSOR_PIN);              // Adds one raw 12-bit ADC reading from GP26.
    delayMicroseconds(200);                                   // Leaves a short settling time between ADC samples.
  }                                                          // Ends temperature averaging loop.
  float reading = readingTotal / (float)TEMP_SAMPLE_COUNT;    // Calculates the averaged ADC reading.
  float voltage = reading * (3.3 / 4095.0);                   // Converts averaged ADC value to volts.
  return (voltage - 0.5) * 100.0;                             // Converts TMP36 voltage to degrees Celsius.
}                                                            // Ends readTemperatureC().

const char *firstSafetyError() {                             // Returns the first safety problem found.
  if (!lidClosed()) return "Lid is open. Close it.";          // Blocks the task if the lid is open.
  if (!motorTempSafe()) return "Motor is too hot.";           // Blocks the task if motor temperature is high.
  if (!vibrationSafe()) return "Vibration level is high.";    // Blocks the task if vibration is unsafe.
  return "";                                                 // Returns empty text when all checks pass.
}                                                            // Ends firstSafetyError().

void updateVibrationRate() {                                 // Calculates vibration pulses per second.
  unsigned long now = millis();                              // Reads the current time.
  if (now - lastVibrationMs < SCREEN_REFRESH_MS) return;      // Waits until one display refresh interval has elapsed.
  noInterrupts();                                            // Pauses interrupts while copying the vibration count.
  unsigned long pulses = vibrationPulses;                     // Copies vibration pulses counted during the sample.
  vibrationPulses = 0;                                       // Clears the vibration counter for the next sample.
  interrupts();                                              // Re-enables interrupts after copying the count.
  unsigned long elapsed = now - lastVibrationMs;              // Calculates exact sample time in milliseconds.
  lastVibrationMs = now;                                     // Stores this vibration update time.
  vibrationRate = (pulses * 1000UL) / elapsed;                // Converts counted pulses to pulses per second.
  if (taskActive && now - taskStartMs < VIBRATION_STARTUP_GRACE_MS) { // Allows normal vibration changes while the rotor begins accelerating.
    previousVibrationRate = vibrationRate;                    // Keeps the latest reading ready as a future comparison baseline.
    vibrationBaselineReady = false;                           // Prevents startup readings from triggering a surge.
    vibrationSurgeDetected = false;                           // Clears any startup surge indication.
    return;                                                   // Waits until the startup grace period ends before checking surges.
  }                                                          // Ends vibration startup grace period.
  if (vibrationSurgePending) {                                // Checks whether the previous reading started a possible surge.
    if (vibrationRate >= vibrationPreSurgeRate + vibrationPendingThreshold) { // Confirms vibration remained elevated for a second reading.
      vibrationSurgeDetected = true;                          // Marks the sustained surge so the active task can stop.
    }                                                         // Ends sustained-surge confirmation.
    vibrationSurgePending = false;                            // Clears the pending state after checking the confirmation reading.
  } else {                                                    // Looks for a new possible surge.
    unsigned long surgeThreshold = VIBRATION_SURGE_PPS;       // Uses the same sustained-surge threshold because RPM sensing is disabled.
    if (vibrationBaselineReady && vibrationRate > previousVibrationRate && vibrationRate - previousVibrationRate >= surgeThreshold) { // Detects a possible sudden upward surge.
    vibrationPreSurgeRate = previousVibrationRate;            // Saves the stable reading from immediately before the surge.
    vibrationPendingThreshold = surgeThreshold;               // Saves the applicable threshold for the confirmation reading.
    vibrationSurgePending = true;                             // Waits for the next one-second reading before stopping.
    }                                                         // Ends possible-surge branch.
  }                                                           // Ends vibration surge detection.
  previousVibrationRate = vibrationRate;                      // Saves this reading for comparison with the next one.
  vibrationBaselineReady = true;                              // Marks the first valid reading as established.
}                                                            // Ends updateVibrationRate().

void updateMotorSoftStart() {                                 // Gradually raises motor power to the selected fixed command.
  if (!taskActive || motorSpeedPercent >= taskMotorTargetPercent) return; // Holds power once the selected command is reached.
  if (millis() - lastMotorRampMs < MOTOR_RAMP_INTERVAL_MS) return; // Waits between soft-start increases.
  lastMotorRampMs = millis();                                 // Stores this ramp-step time.
  int nextPercent = min(motorSpeedPercent + MOTOR_RAMP_STEP_PERCENT, taskMotorTargetPercent); // Calculates the next bounded command.
  setMotorPercent(nextPercent);                               // Sends the next soft-start command to the 0-10V module.
}                                                            // Ends updateMotorSoftStart().

void drawCenteredText(ButtonArea box, const char *text, uint16_t color, uint16_t bg, int fontSize) { // Draws centered text.
  tft.fillRect(box.x, box.y, box.w, box.h, bg);                // Clears the text box area.
  tft.setTextColor(color, bg);                                // Sets text and background colors.
  tft.setTextDatum(MC_DATUM);                                 // Centers text around the draw point.
  tft.drawString(text, box.x + box.w / 2, box.y + box.h / 2, fontSize); // Draws text centered in the box.
  tft.setTextDatum(TL_DATUM);                                 // Restores normal top-left text drawing.
}                                                            // Ends drawCenteredText().

void drawErrorMessage(const char *message) {                  // Draws the specific error message.
  int fontSize = strlen(message) > 30 ? 1 : 2;                // Uses smaller text for long error messages.
  drawCenteredText(errorTextBox, message, TFT_BLACK, TFT_WHITE, fontSize); // Prints error text in the blank area.
}                                                            // Ends drawErrorMessage().

void drawButton(ButtonArea button, const char *label, uint16_t fill, uint16_t text) { // Draws a labeled rectangular button.
  tft.fillRoundRect(button.x + 3, button.y + 3, button.w, button.h, 6, TFT_LIGHTGREY); // Draws a subtle button shadow.
  tft.fillRoundRect(button.x, button.y, button.w, button.h, 6, fill); // Fills the button body.
  tft.drawRoundRect(button.x, button.y, button.w, button.h, 6, TFT_DARKGREY); // Draws the button border.
  tft.setTextColor(text, fill);                                // Sets text color and button background.
  tft.setTextDatum(MC_DATUM);                                  // Centers text around the draw point.
  tft.drawString(label, button.x + button.w / 2, button.y + button.h / 2, 2); // Draws the button label.
  tft.setTextDatum(TL_DATUM);                                  // Restores top-left text drawing.
}                                                             // Ends drawButton().

void drawPageHeader(const char *title) {                       // Draws a consistent header on code-built pages.
  tft.fillRect(0, 0, SCREEN_W, 58, TFT_BLACK);                 // Draws the black header band.
  tft.fillRect(0, 58, SCREEN_W, 4, TFT_LIGHTGREY);             // Draws a thin neutral accent line.
  tft.setTextColor(TFT_WHITE, TFT_BLACK);                      // Sets white title text on the header.
  tft.setTextDatum(MC_DATUM);                                  // Centers the page title.
  tft.drawString(title, SCREEN_W / 2, 29, 4);                  // Draws the page title.
  tft.setTextDatum(TL_DATUM);                                  // Restores top-left text drawing.
}                                                             // Ends drawPageHeader().

void drawSelectedImageButton(ButtonArea button, const char *label) { // Draws a selected button directly over image pixels.
  tft.fillRect(button.x + 3, button.y + 3, button.w - 6, button.h - 6, TFT_BLACK); // Fills inside the original image button.
  tft.drawRect(button.x, button.y, button.w, button.h, TFT_BLACK); // Keeps a square border aligned with the artwork.
  tft.setTextColor(TFT_WHITE, TFT_BLACK);                       // Sets white text on black selected fill.
  tft.setTextDatum(MC_DATUM);                                  // Centers the label inside the image button.
  tft.drawString(label, button.x + button.w / 2, button.y + button.h / 2, 4); // Draws the selected label.
  tft.setTextDatum(TL_DATUM);                                  // Restores top-left text drawing.
}                                                             // Ends drawSelectedImageButton().

void drawSolenoidStatus(ButtonArea box) {                     // Draws the current solenoid pulse status.
  char text[32];                                               // Creates a small text buffer.
  if (solenoidPulseActive) {                                   // Checks whether the solenoid is currently powered.
    unsigned long elapsed = millis() - solenoidPulseStartMs;   // Calculates how long the pulse has been active.
    unsigned long remaining = elapsed >= SOLENOID_PULSE_MS ? 0 : (SOLENOID_PULSE_MS - elapsed + 999) / 1000; // Calculates seconds left.
    snprintf(text, sizeof(text), "Lid lock disengaged: %lu", remaining); // Formats the countdown text.
  } else {                                                     // Handles the idle solenoid state.
    snprintf(text, sizeof(text), "Lid lock engaged");           // Formats the idle status text.
  }                                                           // Ends status text selection.
  drawCenteredText(box, text, TFT_BLACK, TFT_WHITE, 2);       // Draws the status line in the requested box.
}                                                             // Ends drawSolenoidStatus().

void drawInstructionsScreen() {                               // Draws the lid and loading instruction page.
  currentScreen = SCREEN_INSTRUCTIONS;                        // Marks the instruction page as active.
  tft.fillScreen(TFT_WHITE);                                  // Clears the screen to white.
  drawPageHeader("Instructions");                             // Draws the page header.
  tft.setTextColor(TFT_BLACK, TFT_WHITE);                     // Sets black text on white background.
  tft.setTextDatum(TL_DATUM);                                 // Restores top-left text drawing.
  tft.drawFastHLine(42, 202, 396, TFT_LIGHTGREY);              // Draws a divider above the controls.
  tft.drawString("1. Press the button to open the lid.", 42, 78, 2); // Draws instruction line 1.
  tft.drawString("2. Place the balanced test tubes.", 42, 108, 2); // Draws instruction line 2.
  tft.drawString("3. Press the button and close lid.", 42, 138, 2); // Draws instruction line 3.
  tft.drawString("4. Select task in the next page.", 42, 168, 2); // Draws instruction line 4.
  drawSolenoidStatus({80, 190, 320, 24});                     // Draws the solenoid status line.
  drawButton(instructionOpenBtn, "Open/close lid", UI_GREEN, TFT_BLACK); // Draws the solenoid pulse button.
  drawButton(instructionNextBtn, "next", UI_GREEN, TFT_BLACK); // Draws the next button.
}                                                             // Ends drawInstructionsScreen().

void drawRunningScreenBase() {                                // Draws the task running page without a bitmap image.
  currentScreen = SCREEN_TASK_RUNNING;                        // Marks the running page as the active screen.
  tft.fillScreen(TFT_WHITE);                                  // Clears the screen to white.
  drawPageHeader("Task running");                             // Draws the page header.
  tft.setTextColor(TFT_BLACK, TFT_WHITE);                     // Sets black text on white background.
  tft.setTextDatum(TL_DATUM);                                 // Restores top-left text drawing.
  tft.drawString("Selected task:", 78, 78, 2);                 // Draws the selected-task label without referring to an RPM sensor.
  tft.drawString("Remaining time:", 78, 138, 2);               // Draws the remaining time label.
  tft.drawString("Vibration:", 78, 168, 2);                    // Draws the vibration sensor label.
  tft.drawString("Temperature:", 78, 198, 2);                  // Draws the temperature sensor label.
  tft.drawString("Lid:", 78, 228, 2);                          // Draws the lid hall sensor label.
  char text[24];                                               // Creates a buffer for selected-task text.
  snprintf(text, sizeof(text), "%d RPM", targetRpm);           // Formats the RPM preset selected by the user.
  drawCenteredText(rpmTargetBox, text, TFT_BLACK, TFT_WHITE, 2); // Draws the selected RPM preset once.
  tft.drawRect(progressBar.x, progressBar.y, progressBar.w, progressBar.h, TFT_BLACK); // Draws the empty progress bar.
  drawButton(stopBtn, "STOP", UI_RED, TFT_BLACK);              // Draws the stop button.
}                                                             // Ends drawRunningScreenBase().

void drawErrorScreenBase() {                                  // Draws the error page without a bitmap image.
  currentScreen = SCREEN_ERROR;                               // Marks the error page as the active screen.
  tft.fillScreen(TFT_WHITE);                                  // Clears the screen to white.
  drawPageHeader("Error");                                    // Draws the page header.
  tft.setTextColor(TFT_BLACK, TFT_WHITE);                     // Sets black text on white background.
  tft.setTextDatum(MC_DATUM);                                 // Centers title text.
  tft.drawTriangle(240, 86, 210, 136, 270, 136, TFT_BLACK);    // Draws a warning triangle outline.
  tft.fillTriangle(240, 94, 218, 131, 262, 131, TFT_WHITE);    // Clears the inside of the warning triangle.
  tft.drawString("!", 240, 117, 4);                            // Draws the warning exclamation.
  tft.setTextDatum(TL_DATUM);                                 // Restores top-left text drawing.
  drawButton(errorHomeBtn, "home screen", UI_GREEN, TFT_BLACK); // Draws the home button.
}                                                             // Ends drawErrorScreenBase().

void drawDoneScreenBase() {                                   // Draws the completed page without a bitmap image.
  showScreen(SCREEN_TASK_DONE);                               // Draws the updated task-completed image.
  drawSolenoidStatus(doneSolenoidStatusBox);                  // Draws lock state between the two image buttons.
}                                                             // Ends drawDoneScreenBase().

void drawSelectionHighlights() {                              // Marks selected RPM and time options.
  restoreArea({105, 76, 350, 154});                           // Restores the full option region before highlights.
  if (selectedRpmIndex >= 0) {                                 // Checks whether an RPM option has been selected.
    char label[8];                                             // Creates a small label buffer.
    snprintf(label, sizeof(label), "%dk", rpmChoices[selectedRpmIndex] / 1000); // Formats the selected RPM label.
    drawSelectedImageButton(rpmBtns[selectedRpmIndex], label); // Draws the selected RPM button black.
  }                                                           // Ends selected RPM branch.
  if (selectedTimeIndex >= 0) {                                // Checks whether a time option has been selected.
    char label[8];                                             // Creates a small label buffer.
    snprintf(label, sizeof(label), "%d", timeChoices[selectedTimeIndex]); // Formats the selected time label.
    drawSelectedImageButton(timeBtns[selectedTimeIndex], label); // Draws the selected time button black.
  }                                                           // Ends selected time branch.
}                                                            // Ends drawSelectionHighlights().

void drawRunningOverlay() {                                   // Updates RPM, timer, and progress bar.
  char text[24];                                              // Creates a buffer for formatted text.
  unsigned long elapsedMs = millis() - taskStartMs;            // Calculates elapsed time.
  unsigned long remainingMs = elapsedMs >= taskDurationMs ? 0 : taskDurationMs - elapsedMs; // Calculates remaining time.
  int progress = taskDurationMs == 0 ? 0 : (elapsedMs * 100UL) / taskDurationMs; // Calculates progress percent.
  progress = constrain(progress, 0, 100);                      // Keeps progress from 0 to 100.
  snprintf(text, sizeof(text), "%02lu:%02lu", remainingMs / 60000UL, (remainingMs / 1000UL) % 60UL); // Formats time left.
  drawCenteredText(timeLeftBox, text, TFT_BLACK, TFT_WHITE, 2); // Draws time left.
  updateVibrationRate();                                      // Updates the numeric vibration rate once per second.
  snprintf(text, sizeof(text), "%lu pulse/s", vibrationRate);  // Formats vibration as pulses per second.
  drawCenteredText(vibrationBox, text, TFT_BLACK, TFT_WHITE, 2); // Draws numeric vibration rate.
  float temperatureC = readTemperatureC();                     // Reads the motor temperature sensor in Celsius.
  char tempValue[10];                                          // Creates a buffer for the temperature number.
  dtostrf(temperatureC, 4, 1, tempValue);                      // Converts the float temperature to text.
  snprintf(text, sizeof(text), "%s C", tempValue);             // Formats the temperature text with units.
  drawCenteredText(temperatureBox, text, TFT_BLACK, TFT_WHITE, 2); // Draws temperature in the black-and-white interface style.
  drawCenteredText(lidBox, "", TFT_BLACK, TFT_WHITE, 2);       // Keeps the lid-status area blank while the interlock runs in the background.
  tft.fillRect(progressBar.x, progressBar.y, progressBar.w, progressBar.h, TFT_WHITE); // Clears the progress bar.
  tft.drawRect(progressBar.x, progressBar.y, progressBar.w, progressBar.h, TFT_BLACK); // Redraws the progress border.
  int filledW = (progressBar.w * progress) / 100;              // Calculates filled bar width.
  tft.fillRect(progressBar.x, progressBar.y, filledW, progressBar.h, TFT_BLACK); // Draws black progress fill.
}                                                            // Ends drawRunningOverlay().

void showHome() {                                             // Shows the home screen and idles the machine.
  taskActive = false;                                         // Marks task inactive.
  stopMotor();                                                // Stops motor command.
  unlockLid();                                                // Releases lid lock.
  showScreen(SCREEN_HOME);                                    // Draws home image.
}                                                            // Ends showHome().

void showSafety() { showScreen(SCREEN_SAFETY); }              // Draws safety notice image.

void showInstructions() {                                     // Shows the lid loading instruction page.
  drawInstructionsScreen();                                   // Draws the code-built instructions page.
}                                                            // Ends showInstructions().

void showTaskSelect() {                                       // Shows task selection screen.
  showScreen(SCREEN_TASK_SELECT);                             // Draws task selection image.
  selectedRpmIndex = -1;                                      // Clears the RPM selection each time this page opens.
  selectedTimeIndex = -1;                                     // Clears the time selection each time this page opens.
  drawSelectionHighlights();                                  // Draws selected option outlines.
}                                                            // Ends showTaskSelect().

void showError(const char *message) {                         // Shows error page with a reason.
  taskActive = false;                                         // Marks task inactive.
  stopMotor();                                                // Stops the motor.
  unlockLid();                                                // Releases the lid lock.
  drawErrorScreenBase();                                      // Draws the code-built error page.
  drawErrorMessage(message);                                  // Prints error message.
}                                                            // Ends showError().

void showTaskDone() {                                         // Shows task completed page.
  taskActive = false;                                         // Marks task inactive.
  stopMotor();                                                // Sets stored motor command and the 0-10V control signal to zero.
  analogWrite(MOTOR_PWM_PIN, 0);                              // Explicitly forces GP3 PWM low when the task ends.
  unlockLid();                                                // Releases lid lock.
  drawDoneScreenBase();                                       // Draws the code-built task completed page.
}                                                            // Ends showTaskDone().

void startSelectedTask() {                                    // Starts selected program if safe.
  if (selectedRpmIndex < 0 && selectedTimeIndex < 0) { showError("Select RPM and time first."); return; } // Requires both options.
  if (selectedRpmIndex < 0) { showError("Select an RPM first."); return; } // Requires an RPM selection.
  if (selectedTimeIndex < 0) { showError("Select a time first."); return; } // Requires a time selection.
  if (!lidClosed()) { showError("Task halted. Lid is open"); return; } // Prevents the motor from starting unless the lid is closed.
  targetRpm = rpmChoices[selectedRpmIndex];                   // Stores selected RPM.
  taskMotorTargetPercent = motorTargetPercentChoices[selectedRpmIndex]; // Stores the fixed motor command for the selected task.
  taskDurationMs = (unsigned long)timeChoices[selectedTimeIndex] * 60000UL; // Converts minutes to milliseconds.
  taskStartMs = millis();                                     // Stores start time.
  lastMotorRampMs = millis();                                 // Starts the fixed-command soft-start timer.
  lastVibrationMs = millis();                                 // Starts the vibration sample timer.
  lastOverlayMs = 0;                                          // Forces first overlay update.
  tempOverLimitStartMs = 0;                                   // Clears any previous over-temperature timer.
  vibrationPulses = 0;                                        // Clears old vibration pulses.
  vibrationRate = 0;                                          // Clears the displayed vibration rate.
  previousVibrationRate = 0;                                  // Clears the previous vibration reading.
  vibrationBaselineReady = false;                             // Makes the first task reading establish the vibration baseline.
  vibrationSurgeDetected = false;                             // Clears any previous vibration surge.
  vibrationSurgePending = false;                              // Clears any pending vibration-surge confirmation.
  vibrationPreSurgeRate = 0;                                  // Clears the previous surge baseline.
  vibrationPendingThreshold = 0;                              // Clears the previous pending surge threshold.
  taskActive = true;                                          // Starts the timed task and safety monitoring.
  setMotorPercent(MOTOR_START_PERCENT);                        // Starts at five percent before ramping to the selected fixed command.
  drawRunningScreenBase();                                    // Draws the code-built running screen.
  drawRunningOverlay();                                       // Draws first overlay.
}                                                            // Ends startSelectedTask().

void abortTask(const char *message) { showError(message); }   // Stops task and displays error page.

void setup() {                                                // Runs once when Pico boots.
  Serial.begin(115200);                                       // Opens USB serial.
  delay(300);                                                 // Gives hardware time to settle.
  Serial.println("Centrifuge controller started.");           // Confirms startup without enabling manual motor commands.
  pinMode(TFT_CS_PIN, OUTPUT);                                // Sets TFT chip-select as output.
  pinMode(TOUCH_CS_PIN, OUTPUT);                              // Sets touch chip-select as output.
  pinMode(MOTOR_PWM_PIN, OUTPUT);                             // Sets motor PWM pin as output.
  pinMode(SOLENOID_PIN, OUTPUT);                              // Sets solenoid driver as output.
  pinMode(LID_HALL_PIN, INPUT_PULLUP);                        // Sets lid hall sensor as pull-up input.
  pinMode(VIBRATION_PIN, INPUT);                              // Sets vibration sensor as plain digital input.
  analogReadResolution(12);                                   // Uses 12-bit ADC readings from 0 to 4095.
  digitalWrite(TFT_CS_PIN, HIGH);                             // Starts with TFT deselected.
  digitalWrite(TOUCH_CS_PIN, HIGH);                           // Starts with touch deselected.
  unlockLid();                                                // Starts with lid unlocked.
  stopMotor();                                                // Starts with motor stopped.
  analogWriteFreq(1000);                                      // Sets PWM to 1 kHz.
  attachInterrupt(digitalPinToInterrupt(VIBRATION_PIN), vibrationPulseISR, RISING); // Counts vibration sensor pulses.
  tft.init();                                                 // Initializes TFT.
  tft.setRotation(1);                                         // Sets display landscape.
  tft.invertDisplay(false);                                   // Keeps normal color inversion.
  tft.setSwapBytes(true);                                     // Matches RGB565 image byte order.
  tft.initDMA();                                              // Enables TFT_eSPI DMA.
  digitalWrite(TFT_CS_PIN, HIGH);                             // Deselects TFT after init.
  digitalWrite(TOUCH_CS_PIN, HIGH);                           // Deselects touch after display init.
  ts.begin();                                                 // Initializes touch controller.
  ts.setRotation(1);                                          // Matches touch rotation to display.
  digitalWrite(TFT_CS_PIN, HIGH);                             // Leaves TFT deselected.
  digitalWrite(TOUCH_CS_PIN, HIGH);                           // Leaves touch deselected.
  showHome();                                                 // Shows first page.
}                                                            // Ends setup().

void loop() {                                                 // Runs repeatedly.
  updateSolenoidPulse();                                      // Turns the solenoid off after its configured pulse time.

  if (solenoidPulseActive && (currentScreen == SCREEN_INSTRUCTIONS || currentScreen == SCREEN_TASK_DONE) && millis() - lastSolenoidUiMs > 1000) { // Refreshes the visible solenoid countdown.
    lastSolenoidUiMs = millis();                              // Saves the solenoid UI refresh time.
    if (currentScreen == SCREEN_INSTRUCTIONS) drawSolenoidStatus({80, 190, 320, 24}); // Updates instructions status.
    if (currentScreen == SCREEN_TASK_DONE) drawSolenoidStatus(doneSolenoidStatusBox); // Updates completed-page status.
  }                                                          // Ends solenoid status refresh block.

  if (taskActive) {                                           // Monitors only while spinning.
    updateMotorSoftStart();                                   // Ramps to and maintains the selected fixed motor command.
    updateVibrationRate();                                    // Updates vibration pulse rate for safety checks.
    float temperatureC = readTemperatureC();                  // Reads motor temperature for the active safety limit.
    if (!lidClosed()) abortTask("Task halted. Lid is open");   // Immediately stops the motor if the lid opens during a task.
    else if (vibrationSurgeDetected) abortTask("Task halted. Sudden vibration surge detected"); // Stops after a rise of at least 100 pulses per second.
    else if (temperatureC >= TEMP_C_MAX_SAFE) {               // Starts timing only while temperature remains at or above 65 C.
      if (tempOverLimitStartMs == 0) tempOverLimitStartMs = millis(); // Records when the continuous high reading began.
      else if (millis() - tempOverLimitStartMs >= TEMP_TRIP_DELAY_MS) abortTask("Task halted. Motor overheated"); // Stops after five continuous seconds.
    } else {                                                  // Handles normal temperature readings.
      tempOverLimitStartMs = 0;                               // Cancels the trip timer when temperature falls below the limit.
    }                                                         // Ends temperature safety check.
    if (!taskActive) return;                                  // Stops this loop pass after an error changes screens.
    if (millis() - lastOverlayMs > SCREEN_REFRESH_MS) {        // Updates screen one time per second.
      lastOverlayMs = millis();                               // Stores overlay update time.
      drawRunningOverlay();                                   // Redraws dynamic text and progress.
    }                                                        // Ends overlay update block.
    if (millis() - taskStartMs >= taskDurationMs) showTaskDone(); // Finishes task when timer ends.
  }                                                          // Ends active task monitor.

  if (!taskActive && currentScreen == SCREEN_TASK_RUNNING && millis() - lastOverlayMs > SCREEN_REFRESH_MS) { // Refreshes the UI-test running page.
    lastOverlayMs = millis();                               // Stores the time of this display refresh.
    drawRunningOverlay();                                   // Updates vibration and display-only task values.
  }                                                         // Ends UI-test refresh block.

  if (!getTouchPoint()) { delay(10); return; }                // Exits loop pass if there is no touch.

  if (currentScreen == SCREEN_HOME && insideButton(touchX, touchY, homeStartBtn)) { // Handles home START.
    pressEffect(homeStartBtn);                                // Shows press outline.
    showSafety();                                             // Goes to safety screen.
    waitForRelease();                                         // Waits for release.
  }                                                          // Ends home handler.

  else if (currentScreen == SCREEN_SAFETY && insideButton(touchX, touchY, safetyContinueBtn)) { // Handles safety continue.
    pressEffect(safetyContinueBtn);                           // Shows press outline.
    showInstructions();                                       // Goes to the lid loading instruction screen.
    waitForRelease();                                         // Waits for release.
  }                                                          // Ends safety handler.

  else if (currentScreen == SCREEN_INSTRUCTIONS) {            // Handles touches on the instructions screen.
    if (insideButton(touchX, touchY, instructionOpenBtn)) {    // Checks the open/close lid button.
      pressEffect(instructionOpenBtn);                        // Shows press outline.
      startSolenoidPulse();                                   // Powers the solenoid for the configured pulse time.
      drawInstructionsScreen();                               // Redraws the instructions page with active status.
      waitForRelease();                                       // Waits for release.
    }                                                        // Ends instruction open/close branch.
    else if (insideButton(touchX, touchY, instructionNextBtn)) { // Checks the next button.
      pressEffect(instructionNextBtn);                        // Shows press outline.
      showTaskSelect();                                       // Goes directly to task selection.
      waitForRelease();                                       // Waits for release.
    }                                                        // Ends instruction next branch.
  }                                                          // Ends instructions handler.

  else if (currentScreen == SCREEN_TASK_SELECT) {             // Handles task selection touches.
    if (insideButton(touchX, touchY, taskBackBtn)) {           // Checks the Back button.
      pressEffect(taskBackBtn);                               // Shows press outline.
      showInstructions();                                     // Returns to instructions so the lid can be opened.
      waitForRelease();                                       // Waits for release.
      return;                                                 // Ends this loop pass.
    }                                                        // Ends Back button branch.
    for (int i = 0; i < 3; i++) {                              // Checks all option buttons.
      if (insideButton(touchX, touchY, rpmBtns[i])) {          // Checks RPM option.
        selectedRpmIndex = i;                                 // Saves RPM choice.
        pressEffect(rpmBtns[i]);                              // Shows press outline.
        drawSelectionHighlights();                            // Redraws selection marks.
        waitForRelease();                                     // Waits for release.
        return;                                               // Ends this loop pass.
      }                                                       // Ends RPM branch.
      if (insideButton(touchX, touchY, timeBtns[i])) {         // Checks time option.
        selectedTimeIndex = i;                                // Saves time choice.
        pressEffect(timeBtns[i]);                             // Shows press outline.
        drawSelectionHighlights();                            // Redraws selection marks.
        waitForRelease();                                     // Waits for release.
        return;                                               // Ends this loop pass.
      }                                                       // Ends time branch.
    }                                                         // Ends option loop.
    if (insideButton(touchX, touchY, taskStartBtn)) {          // Checks task START.
      pressEffect(taskStartBtn);                              // Shows press outline.
      startSelectedTask();                                    // Starts task if safe.
      waitForRelease();                                       // Waits for release.
    }                                                        // Ends task start branch.
  }                                                          // Ends task selection handler.

  else if (currentScreen == SCREEN_TASK_RUNNING && insideButton(touchX, touchY, stopBtn)) { // Handles STOP.
    pressEffect(stopBtn);                                     // Shows press outline.
    showTaskDone();                                           // Shows task completed for display testing.
    waitForRelease();                                         // Waits for release.
  }                                                          // Ends running handler.

  else if (currentScreen == SCREEN_ERROR && insideButton(touchX, touchY, errorHomeBtn)) { // Handles error home.
    pressEffect(errorHomeBtn);                                // Shows press outline.
    showHome();                                               // Returns home.
    waitForRelease();                                         // Waits for release.
  }                                                          // Ends error handler.

  else if (currentScreen == SCREEN_TASK_DONE && insideButton(touchX, touchY, doneHomeBtn)) { // Handles done home.
    pressEffect(doneHomeBtn);                                 // Shows press outline.
    showHome();                                               // Returns home.
    waitForRelease();                                         // Waits for release.
  }                                                          // Ends completed handler.

  else if (currentScreen == SCREEN_TASK_DONE && insideButton(touchX, touchY, doneOpenBtn)) { // Handles completed page lid unlock.
    pressEffect(doneOpenBtn);                                 // Shows press outline.
    startSolenoidPulse();                                     // Powers the solenoid for the configured pulse time.
    drawDoneScreenBase();                                     // Redraws completed page with active status.
    waitForRelease();                                         // Waits for release.
  }                                                          // Ends completed lid unlock handler.

  delay(10);                                                  // Adds a small touch-loop delay.
}                                                            // Ends loop().

