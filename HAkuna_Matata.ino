#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "MAX30105.h"
#include "heartRate.h"

// --- Pin Definitions ---
#define SDA_PIN 8
#define SCL_PIN 9
#define MOTOR_PIN 3

// --- Display Configuration ---
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
MAX30105 particleSensor;

// --- Heart Rate & Panic Threshold Variables ---
const byte RATE_SIZE = 4; // Average across 4 readings for stability
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;

float beatsPerMinute = 0;
int beatAvg = 0;

// Panic Detection Parameters
const int PANIC_THRESHOLD_BPM = 110; // Trigger threshold (adjust as needed)
bool isPanicState = false;

void setup() {
  Serial.begin(115200);
  pinMode(MOTOR_PIN, OUTPUT);
  digitalWrite(MOTOR_PIN, LOW);

  // Initialize I2C Bus on ESP32-C3
  Wire.begin(SDA_PIN, SCL_PIN);

  // Initialize OLED Display
  if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println("OLED init failed!");
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(15, 20);
  display.println("HAKUNA MATATA");
  display.setCursor(20, 35);
  display.println("System Booting...");
  display.display();
  delay(2000);

  // Initialize MAX30102 Sensor
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST)) {
    display.clearDisplay();
    display.setCursor(10, 25);
    display.println("Sensor Error!");
    display.display();
    while (1);
  }

  // Configure MAX30102 for heart rate detection
  particleSensor.setup(); 
  particleSensor.setPulseAmplitudeRed(0x0A); // Turn Red LED low to indicate running
  particleSensor.setPulseAmplitudeGreen(0x00); 
}

void loop() {
  long irValue = particleSensor.getIR();

  // Check if a finger/wrist is placed on the sensor
  if (irValue < 50000) {
    displayIdleState("No Wrist Detected");
    digitalWrite(MOTOR_PIN, LOW);
    return;
  }

  // Detect Heartbeat Peak
  if (checkForBeat(irValue) == true) {
    long delta = millis() - lastBeat;
    lastBeat = millis();

    beatsPerMinute = 60 / (delta / 1000.0);

    if (beatsPerMinute < 255 && beatsPerMinute > 40) {
      rates[rateSpot++] = (byte)beatsPerMinute;
      rateSpot %= RATE_SIZE;

      // Calculate rolling average
      beatAvg = 0;
      for (byte x = 0; x < RATE_SIZE; x++) {
        beatAvg += rates[x];
      }
      beatAvg /= RATE_SIZE;
    }
  }

  // Check Trigger Condition
  if (beatAvg >= PANIC_THRESHOLD_BPM && beatAvg > 0) {
    isPanicState = true;
    triggerSomaticPacing(); // Activate haptic feedback
  } else {
    isPanicState = false;
    digitalWrite(MOTOR_PIN, LOW);
  }

  // Update Display
  updateScreenDisplay();
}

// Function to drive 4Hz physiological sigh vibration pacing
void triggerSomaticPacing() {
  // Gentle pulse pattern: 150ms ON, 100ms OFF
  digitalWrite(MOTOR_PIN, HIGH);
  delay(150);
  digitalWrite(MOTOR_PIN, LOW);
  delay(100);
}

// Function to update screen status
void updateScreenDisplay() {
  display.clearDisplay();

  if (isPanicState) {
    display.setTextSize(2);
    display.setCursor(0, 5);
    display.println("PANIC ALERT");

    display.setTextSize(1);
    display.setCursor(0, 30);
    display.print("BPM: ");
    display.setTextSize(2);
    display.println(beatAvg);

    display.setTextSize(1);
    display.setCursor(0, 50);
    display.println("Somatic Pacing ON...");
  } else {
    display.setTextSize(1);
    display.setCursor(0, 0);
    display.println("HAKUNA MATATA");

    display.drawFastHLine(0, 12, 128, SSD1306_WHITE);

    display.setCursor(0, 25);
    display.print("Heart Rate: ");
    display.setTextSize(2);
    display.print(beatAvg);
    display.setTextSize(1);
    display.println(" BPM");

    display.setCursor(0, 50);
    display.println("Status: Monitoring");
  }

  display.display();
}

void displayIdleState(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(10, 20);
  display.println("HAKUNA MATATA");
  display.setCursor(10, 40);
  display.println(msg);
  display.display();
}