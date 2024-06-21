#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
#define RELAY_PIN 7
#define BUTTON_PIN 6
#define CONFIRM_BUTTON_PIN 5

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

bool allowPumpRun = true; // Variable to control if the pump can run
int intervals[] = {16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64, 68, 72}; // Time intervals in hours
int currentIntervalIndex = 0; // Current interval index
unsigned long lastDebounceTime = 0; // the last time the output pin was toggled
unsigned long debounceDelay = 50; // the debounce time; increase if the output flickers
unsigned long lastRunTime = 0; // the last time the pump was run
unsigned long intervalMillis = 0; // interval duration in milliseconds
bool timeConfirmed = false; // Variable to indicate if the time has been confirmed
int confirmedPumpRunTime = 0; // Variable to store the confirmed pump run time
bool manualOverride = false; // Variable to indicate if manual override is active

void setup() {
  Serial.begin(9600);

  // Initialize the OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { // Address 0x3C for 128x64
    Serial.println(F("SSD1306 allocation failed"));
    while (true);
  }

  display.clearDisplay();
  display.setTextSize(1);      // Set text size to 1 for normal text
  display.setTextColor(SSD1306_WHITE); // Draw white text
  display.setCursor(0, 0);     // Start at top-left corner
  display.display();           // Show initial display buffer contents

  // Initialize the relay pin as output
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); // Make sure the relay is initially off

  // Initialize the button pins as input with internal pull-up resistor
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(CONFIRM_BUTTON_PIN, INPUT_PULLUP);

  // Initialize intervalMillis with the first interval value
  intervalMillis = intervals[currentIntervalIndex] * 3600000UL; // Convert hours to milliseconds
}

void runPump(int duration) {
  digitalWrite(RELAY_PIN, HIGH); // Turn the relay on
  delay(duration * 1000);        // Keep the relay on for the specified duration (in milliseconds)
  digitalWrite(RELAY_PIN, LOW);  // Turn the relay off
  allowPumpRun = false;          // Set allowPumpRun to false to prevent further runs
  lastRunTime = millis();        // Record the time when the pump was last run
}

void checkButtons() {
  int buttonState = digitalRead(BUTTON_PIN);
  int confirmButtonState = digitalRead(CONFIRM_BUTTON_PIN);

  if (buttonState == LOW && confirmButtonState == LOW) { // Both buttons pressed
    manualOverride = true;
    digitalWrite(RELAY_PIN, HIGH); // Turn the relay on
  } else {
    if (manualOverride) {
      manualOverride = false;
      digitalWrite(RELAY_PIN, LOW); // Turn the relay off
    }

    if (buttonState == LOW) { // Button pressed
      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTime) > debounceDelay) {
        // Cycle through the intervals
        currentIntervalIndex++;
        if (currentIntervalIndex >= sizeof(intervals) / sizeof(intervals[0])) {
          currentIntervalIndex = 0;
        }
        intervalMillis = intervals[currentIntervalIndex] * 3600000UL; // Update intervalMillis with the selected interval in milliseconds
        lastDebounceTime = currentTime;
      }
    }

    if (confirmButtonState == LOW) { // Confirm button pressed
      unsigned long currentTime = millis();
      if ((currentTime - lastDebounceTime) > debounceDelay) {
        // Confirm the current pump run time
        confirmedPumpRunTime = map(analogRead(A0), 0, 1023, 0, 60); // Update confirmed pump run time in seconds
        timeConfirmed = true;
        lastDebounceTime = currentTime;
      }
    }
  }
}

void displayConfirmationIcon(bool showOuterCircle) {
  int iconX = 110; // X position for the icon
  int iconY = 20; // Y position for the icon

  display.fillCircle(iconX, iconY, 10, SSD1306_WHITE); // Draw filled circle
  display.setTextColor(SSD1306_BLACK); // Change text color to black for visibility
  display.setCursor(iconX - 5, iconY - 6);
  display.setTextSize(2); // Larger text size for 'C'
  display.print("C"); // 'C' inside the circle
  display.setTextColor(SSD1306_WHITE); // Reset text color to white

  if (showOuterCircle) {
    display.drawCircle(iconX, iconY, 12, SSD1306_WHITE); // Draw outer circle with a small gap
  }
}

void loop() {
  checkButtons();

  // Check if the interval has passed
  if (millis() - lastRunTime >= intervalMillis) {
    allowPumpRun = true;
  }

  int sensorValue = analogRead(A0); // Read data from potentiometer
  int outputValue = map(sensorValue, 0, 1023, 0, 100); // Map output value from 0-100
  int pumpRunTime = map(sensorValue, 0, 1023, 0, 60); // Calculate pump run time in seconds (0-60 seconds)
  float estimatedVolume = (pumpRunTime / 60.0) * 10; // Calculate estimated volume (10 liters per minute)
  int interval = intervals[currentIntervalIndex]; // Get the current interval
  Serial.print("Output Value: ");
  Serial.print(outputValue);
  Serial.print(" Pump Run Time: ");
  Serial.print(pumpRunTime);
  Serial.print(" Interval: ");
  Serial.println(interval);

  // Clear the display
  display.clearDisplay();

  // Set the cursor to the new position
  display.setCursor(0, 10);

  // Print the pump run time to the display
  display.setTextSize(1);
  display.print("Run Time: ");
  display.print(pumpRunTime);
  display.print(" s");

  // Print the estimated volume to the display
  display.setCursor(0, 25);
  display.setTextSize(1);
  display.print("Volume: ");
  display.print(estimatedVolume);
  display.print(" L");

  // Print the interval to the display
  display.setCursor(0, 40);
  display.setTextSize(1);
  display.print("Interval: ");
  display.print(interval);
  display.print(" h");

  // Display the confirmation icon if time has been confirmed
  if (timeConfirmed) {
    bool showOuterCircle = digitalRead(CONFIRM_BUTTON_PIN) == LOW; // Show outer circle if button is pressed
    displayConfirmationIcon(showOuterCircle);
  }

  // Update the display with the new data
  display.display();

  // Run the pump for the confirmed time if allowed and not in manual override
  if (!manualOverride && timeConfirmed && confirmedPumpRunTime > 0 && allowPumpRun) {
    runPump(confirmedPumpRunTime);
  }

  // Wait for a short period before taking the next reading
  delay(1000); // Increased delay to allow time for pump to run
}
