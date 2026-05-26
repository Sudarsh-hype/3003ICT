#include "DHT.h"

// DHT22 sensor is connected to pin 2 on the ESP32
#define DHTPIN 2
#define DHTTYPE DHT22

// Output pins - each LED shows a different safety condition
#define GREEN_LED 18    // on when environment is safe
#define YELLOW_LED 19   // on when conditions are getting concerning
#define RED_LED 21      // on when conditions are dangerous or sensor fails
#define BUZZER 22       // makes noise during critical or sensor error
#define COOLING_LED 23  // turns on to simulate cooling system in critical state

// Create the sensor object so we can read from it later
DHT dht(DHTPIN, DHTTYPE);

// These are the four states our system can be in at any time
enum State {
  SAFE,
  WARNING,
  CRITICAL,
  SENSOR_ERROR
};

// System starts in SAFE when first powered on
State currentState = SAFE;

// We used two bands of thresholds based on general lab storage guidelines
// Anything inside the ideal range = SAFE
// Between ideal and critical = WARNING
// Outside critical = CRITICAL

// Temperature (Celsius)
const float IDEAL_TEMP_MIN = 18.0;
const float IDEAL_TEMP_MAX = 28.0;
const float CRITICAL_TEMP_MIN = 15.0;
const float CRITICAL_TEMP_MAX = 30.0;

// Humidity (%)
const float IDEAL_HUMIDITY_MIN = 35.0;
const float IDEAL_HUMIDITY_MAX = 65.0;
const float CRITICAL_HUMIDITY_MIN = 25.0;
const float CRITICAL_HUMIDITY_MAX = 75.0;

// Warning timer - if conditions stay in warning range for too long
// the system escalates to critical even if thresholds are not fully crossed
// 30 seconds gives staff enough time to respond before it becomes dangerous
const unsigned long WARNING_TIMEOUT = 30000; // 30 seconds in milliseconds
unsigned long warningStartTime = 0;          // records when warning first started
bool warningTimerStarted = false;            // tracks whether timer is running


void setup() {
  // Start serial so we can see readings in the monitor while testing
  Serial.begin(9600);

  // Start the DHT22 sensor
  dht.begin();

  // Tell the ESP32 these pins are outputs (sending signals out)
  pinMode(GREEN_LED, OUTPUT);
  pinMode(YELLOW_LED, OUTPUT);
  pinMode(RED_LED, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(COOLING_LED, OUTPUT);

  Serial.println("Smart Lab Environmental Preservation System Started");
}


void loop() {
  // Read temperature and humidity from the sensor every second
  float temperature = dht.readTemperature();
  float humidity = dht.readHumidity();

  // Decide which state we should be in based on the readings
  updateState(temperature, humidity);

  // Turn on the correct LED and buzzer for the current state
  actOnState();

  // Print everything to serial so we can monitor what is happening
  Serial.print("Temperature: ");
  Serial.print(temperature);
  Serial.print(" C | Humidity: ");
  Serial.print(humidity);
  Serial.print(" % | State: ");
  Serial.println(getStateName(currentState));

  // Wait 1 second before reading again
  delay(1000);
}


void updateState(float temperature, float humidity) {

  // First thing we check is whether the sensor data is even valid
  // isnan means the sensor returned nothing useful
  // We also catch physically impossible values like negative humidity
  // If anything looks wrong, go straight to SENSOR_ERROR and stop
  if (isnan(temperature) || isnan(humidity) ||
      temperature < -40 || temperature > 80 ||
      humidity < 0 || humidity > 100) {
    currentState = SENSOR_ERROR;
    warningTimerStarted = false; // reset timer if sensor fails
    return;
  }

  // Check if conditions have gone past the outer critical boundary
  // We check this first because critical always takes priority over warning
  bool criticalCondition =
    (temperature < CRITICAL_TEMP_MIN || temperature > CRITICAL_TEMP_MAX ||
     humidity < CRITICAL_HUMIDITY_MIN || humidity > CRITICAL_HUMIDITY_MAX);

  // Check if conditions are outside the ideal range but not yet critical
  // Both temperature and humidity are checked together here
  // this is what we mean by sensor fusion in our report
  bool warningCondition =
    (temperature < IDEAL_TEMP_MIN || temperature > IDEAL_TEMP_MAX ||
     humidity < IDEAL_HUMIDITY_MIN || humidity > IDEAL_HUMIDITY_MAX);

  // Now assign the state based on what we found above
  if (criticalCondition) {
    // Conditions are already past the critical boundary
    // go straight to critical and reset the warning timer
    currentState = CRITICAL;
    warningTimerStarted = false;
  }
  else if (warningCondition) {
    // Conditions are in the warning range
    // Start the timer the first time we enter warning
    if (!warningTimerStarted) {
      warningStartTime = millis(); // record the time warning started
      warningTimerStarted = true;
      Serial.println("Warning timer started - will escalate if conditions persist");
    }

    // If warning has been going on for more than 30 seconds
    // escalate to critical because prolonged unsafe conditions
    // are dangerous for chemical storage even if not immediately critical
    if (millis() - warningStartTime > WARNING_TIMEOUT) {
      currentState = CRITICAL;
      Serial.println("Warning persisted too long - escalating to CRITICAL");
    } else {
      currentState = WARNING;
    }
  }
  else {
    // Everything is back to normal, reset to safe and clear the timer
    currentState = SAFE;
    warningTimerStarted = false;
  }
}


void actOnState() {

  // Turn everything off first so we start clean each cycle
  // this stops multiple LEDs accidentally staying on
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(YELLOW_LED, LOW);
  digitalWrite(RED_LED, LOW);
  digitalWrite(COOLING_LED, LOW);
  noTone(BUZZER);

  switch (currentState) {

    case SAFE:
      // All good, just green light on
      digitalWrite(GREEN_LED, HIGH);
      break;

    case WARNING:
      // Something is off but not dangerous yet, yellow light as a heads up
      // timer is running in the background counting how long this lasts
      digitalWrite(YELLOW_LED, HIGH);
      break;

    case CRITICAL:
      // Dangerous conditions detected, either threshold was crossed
      // or warning lasted too long without returning to safe
      // Red light and buzzer to alert anyone nearby
      // Blue LED also turns on to simulate the cooling/ventilation system
      // kicking in automatically to protect the chemicals
      digitalWrite(RED_LED, HIGH);
      digitalWrite(COOLING_LED, HIGH);
      tone(BUZZER, 1000); // continuous alarm at 1000Hz
      break;

    case SENSOR_ERROR:
      // Something is wrong with the sensor itself
      // We blink the red LED instead of keeping it solid
      // so it looks different from a real critical condition
      // short beeps on the buzzer to signal a fault not an emergency
      digitalWrite(RED_LED, HIGH);
      delay(200);
      digitalWrite(RED_LED, LOW);
      delay(200);
      tone(BUZZER, 500, 100); // short 500Hz beep
      break;
  }
}


// Just converts the state into a word for the serial monitor
// makes it easier to read during testing
String getStateName(State state) {
  switch (state) {
    case SAFE:         return "SAFE";
    case WARNING:      return "WARNING";
    case CRITICAL:     return "CRITICAL";
    case SENSOR_ERROR: return "SENSOR_ERROR";
    default:           return "UNKNOWN";
  }
}