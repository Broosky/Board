/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program Name: Patchworks (C)                                                                                            //
// Author: Jeffrey Bednar                                                                                                  //
// Copyright (c) Illusion Interactive, 2011 - 2025.                                                                        //
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Date: Tuesday, October 3rd, 2023
// Description: Source for the Patchworks electronics prototyping and monitoring board.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Fold all: Ctrl + K + 0
// Unfold all: Ctrl + K + J
// Show file explorer: Ctrl + Shift + E
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#include "Headers/fixed_point.h"
#include "Headers/lcd.h"
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
extern LiquidCrystal_I2C lcd;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program constants.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8_t FW_VERSION_MAJOR = 1;
const uint8_t FW_VERSION_MINOR = 1;
const uint8_t FW_VERSION_PATCH = 0;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
const uint8_t PIN_BUZZ = 8;
const uint8_t PIN_FAN = 9;
const uint8_t PIN_RELAY = 6;
const uint8_t PIN_PROGRAM_ACTIVE = 2;
const uint8_t PIN_EXTERNAL_CLOCK = A1;                 // We will use this as a digital pin.
const uint8_t PIN_THERMISTOR_SENSE = A0;               // A0
const uint8_t THERMISTOR_NOMINAL_TEMPERATURE = 25;     // Almost always 25 degrees C; check datasheet.
const uint8_t THERMISTOR_READ_SAMPLES = 5;             // How many times the voltage is read before deciding an average value.
const uint16_t THERMISTOR_NOMINAL_RESISTANCE = 10000;  // Ohms
const uint16_t THERMISTOR_BETA = 3950;                 // Datasheet
const uint16_t THERMISTOR_REFERENCE = 10000;           // Ohms; resistor in the voltage divider.
const uint16_t DELAY_LOOP_COMPLETED = 750;             // MS
const uint16_t DELAY_LCD_PAGE_CYCLE = 1750;
const uint32_t RANDOM_SEED = 255;  // Constant randomSeed() starting point.
const float TEMP_ZERO_KELVIN = 273.15;
const float TEMP_LOWERBOUND = 37.00;          // Celcius: fan off, single chirp.
const float TEMP_UPPERBOUND = 41.00;          // Celcius: fan on until lowerbound is reached, multiple chirps.
const float TEMP_MAXIMUM = 50.0;              // Celcius: fan on, different chirp tone. If enough cycles at or above this temperature, the board will automatically shut down.
const uint8_t TEMP_MAXIMUM_CYCLE_COUNT = 5;   // Number of cycles above maximum temperature before auto shut down.
const uint8_t TEMP_COOLDOWN_MAX_MINUTES = 3;  // Number of minutes to allow cooling to the lowerbound temperature before automatically shutting down.
const int32_t EXTERNAL_CLOCK_LOWERBOUND = 0;
const int32_t EXTERNAL_CLOCK_UPPERBOUND = 99;
typedef enum ERROR_CODE {
  ERROR_CODE_UNKNOWN,           // No error, ignore.
  ERROR_CODE_INVALID,           // Invalid
  ERROR_CODE_HOT_CYCLES,        // Too many cycles at or above maximum temperature.
  ERROR_CODE_COOLDOWN_EXCEEDED  // Expected cooling duration exceeded.
} ERROR_CODE_T;
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Program globals and counters.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float tempAccumulatedSamples = 0;
uint8_t tempHighCycleCount = 0;                                        // Cumulative, never resets.
float tempLifetimeMin = 999.9;                                         // Inverted to normalize during runtime.
float tempLifetimeMax = -999.9;                                        // Inverted to normalize during runtime.
int32_t externalClockLifetimeMin = EXTERNAL_CLOCK_UPPERBOUND << 1;     // Inverted to normalize during runtime.
int32_t externalClockLifetimeMax = -(EXTERNAL_CLOCK_UPPERBOUND << 1);  // Inverted to normalize during runtime.
uint32_t cooldownLifetimeMinMs = UINT32_MAX;                           // Inverted to normalize during runtime.
uint32_t cooldownLifetimeMaxMs = 0;                                    // Inverted to normalize during runtime.
bool showSplashScreen = true;
uint32_t loopCount = 0;
uint8_t allPixels[8] = {
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
  0b11111,
};
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main program initialization.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  // Set pin modes.
  pinMode(PIN_PROGRAM_ACTIVE, OUTPUT);
  pinMode(PIN_BUZZ, OUTPUT);
  pinMode(PIN_FAN, OUTPUT);
  pinMode(PIN_RELAY, OUTPUT);
  pinMode(PIN_EXTERNAL_CLOCK, OUTPUT);

  randomSeed(RANDOM_SEED);

  // Init LCD.
  lcd.init();
  lcd.clear();
  delay(500);

  // Test LCD, buzzer, fan, and set power-on latch.
  testLcd(3, 150, 25);
  testBuzzer(3, 150);
  testThermistorFan(3, 500);
  setPowerOnLatch(HIGH);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main program loop.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  handleSplashScreen(DELAY_LCD_PAGE_CYCLE);
  handleLoopStart(DELAY_LCD_PAGE_CYCLE);

  handleExternalClock(EXTERNAL_CLOCK_LOWERBOUND,
                      EXTERNAL_CLOCK_UPPERBOUND,
                      0,
                      DELAY_LCD_PAGE_CYCLE);

  float currentTemperature = handleThermistor(true,
                                              true,
                                              DELAY_LCD_PAGE_CYCLE);
  handleThermistorFan(currentTemperature,
                      TEMP_LOWERBOUND,
                      TEMP_UPPERBOUND,
                      TEMP_MAXIMUM,
                      TEMP_COOLDOWN_MAX_MINUTES,
                      DELAY_LCD_PAGE_CYCLE);

  handleUptime(DELAY_LCD_PAGE_CYCLE);

  handleLoopEnd(DELAY_LOOP_COMPLETED);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Generates a random number of clock cycles to be output to the external CC. For Squarely's U6 74XX90 CP0 (pin 14).
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleExternalClock(int32_t rangeLowerbound, int32_t rangeUpperbound, uint8_t clockLengthMs, uint16_t lcdPageCycleDelayMs) {
  int32_t randomNumber = random(rangeLowerbound, rangeUpperbound + 1);

  if (randomNumber < externalClockLifetimeMin) {
    externalClockLifetimeMin = randomNumber;
  }

  if (randomNumber > externalClockLifetimeMax) {
    externalClockLifetimeMax = randomNumber;
  }

  if (randomNumber > 0) {
    for (int32_t i = 0; i < randomNumber; i++) {
      digitalWrite(PIN_EXTERNAL_CLOCK, HIGH);
      if (clockLengthMs > 0) {
        delay(clockLengthMs);
      }
      digitalWrite(PIN_EXTERNAL_CLOCK, LOW);
      if (i < randomNumber - 1) {
        if (clockLengthMs > 0) {
          delay(clockLengthMs);
        }
      }
    }
  }

  // Write the number and the lifetime min/max.
  printLabeledInt(0, 0, "C: ", randomNumber, true, lcdPageCycleDelayMs);
  printLabeledInt(0, 0, "Cmin: ", externalClockLifetimeMin, true, 0);
  printLabeledInt(0, 1, "Cmax: ", externalClockLifetimeMax, false, lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Engages the power-on latch to keep the board powered automatically.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setPowerOnLatch(uint8_t state) {
  digitalWrite(PIN_RELAY, state);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Cycles the power regulator cooling fan for test.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void testThermistorFan(uint8_t cycles, uint16_t fanCycleDelayMs) {
  for (uint8_t i = 0; i < cycles; i++) {
    digitalWrite(PIN_FAN, HIGH);
    delay(fanCycleDelayMs);
    digitalWrite(PIN_FAN, LOW);
    if (i < cycles - 1) {
      delay(1500);  // Let it come to a stop.
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Plays a test tone for the buzzer.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void testBuzzer(uint8_t cycles, uint16_t toneCycleDelayMs) {
  playTone(1000, 50, cycles, toneCycleDelayMs, true);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tests the board LCD.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void testLcd(uint8_t backlightCycles, uint16_t backlightCycleDelayMs, uint8_t pixelCycleDelayMs) {
  for (uint8_t i = 0; i < backlightCycles; i++) {
    lcd.backlight();
    delay(backlightCycleDelayMs);
    if (i < backlightCycles - 1) {
      lcd.noBacklight();
      delay(backlightCycleDelayMs);
    }
  }

  lcd.cursor_on();
  lcd.blink_on();

  lcd.setBacklight(255);
  lcd.setContrast(255);

  lcd.createChar(0, allPixels);

  // Test pixels.
  for (uint8_t i = 0; i < LCD_MAX_Y; i++) {
    for (uint8_t j = 0; j < LCD_MAX_X; j++) {
      lcd.setCursor(j, i);
      lcd.write(byte(0));
      delay(pixelCycleDelayMs);
    }
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Plays a tone on the board's buzzer.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void playTone(uint32_t frequency, int32_t durationMs, uint8_t cycles, uint16_t toneCycleDelayMs, bool pauseBeforeReturn) {
  for (uint8_t i = 0; i < cycles; i++) {
    tone(PIN_BUZZ, frequency, durationMs);
    if (i < cycles - 1) {
      // Wait until the tone finishes.
      delay(durationMs);
      delay(toneCycleDelayMs);
    }
  }

  // Avoid buzzer chirp during other transistions.
  if (pauseBeforeReturn) {
    delay(100);
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main error handler.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void shutdown(ERROR_CODE_T errorCode, int frequency, long durationMs, uint8_t cycles, uint16_t toneCycleDelayMs, bool pauseBeforeReturn) {
  printLabeledInt(0, 0, "E: ", errorCode, true, 0);
  playTone(frequency, durationMs, cycles, toneCycleDelayMs, pauseBeforeReturn);

  // Self-terminate.
  setPowerOnLatch(LOW);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Displays a splash screen on the LCD.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleSplashScreen(uint16_t lcdPageCycleDelayMs) {
  if (showSplashScreen) {
    char version[32], date[32], time[32];

    printString(0, 0, "Illusion", true, 0);
    printString(0, 1, "Interactive", false, lcdPageCycleDelayMs);

    sprintf(version, "v%u.%u.%u", FW_VERSION_MAJOR, FW_VERSION_MINOR, FW_VERSION_PATCH);

    printString(0, 0, "Patchworks", true, 0);
    printString(0, 1, version, false, lcdPageCycleDelayMs);

    sprintf(date, "%s", __DATE__);
    sprintf(time, "%s", __TIME__);

    printString(0, 0, date, true, 0);
    printString(0, 1, time, false, lcdPageCycleDelayMs);

    showSplashScreen = false;
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles actions at the loop start.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleLoopStart(uint16_t lcdPageCycleDelayMs) {
  digitalWrite(PIN_PROGRAM_ACTIVE, HIGH);
  ++loopCount;

  // Write the loop count.
  printLabeledInt(0, 0, "L: ", loopCount, true, lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles actions at the loop end.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleLoopEnd(uint16_t loopCompletedDelayMs) {
  digitalWrite(PIN_PROGRAM_ACTIVE, LOW);
  delay(loopCompletedDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles the interaction with the board's main thermistor. Returns temperature in Celcius.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
float handleThermistor(uint8_t writeAdc, uint8_t writeResistance, uint16_t lcdPageCycleDelayMs) {
  float tempAverage;

  tempAccumulatedSamples = 0;
  for (uint8_t i = 0; i < THERMISTOR_READ_SAMPLES; i++) {
    tempAccumulatedSamples += analogRead(PIN_THERMISTOR_SENSE);
    delay(10);
  }

  tempAverage = tempAccumulatedSamples / THERMISTOR_READ_SAMPLES;

  // Write the ADC average.
  if (writeAdc) {
    printLabeledFloat(0, 0, "A: ", tempAverage, true, lcdPageCycleDelayMs);
  }

  // Calculate NTC resistance.
  tempAverage = 1023 / tempAverage - 1;
  tempAverage = THERMISTOR_REFERENCE / tempAverage;

  // Write the calculated resistance.
  if (writeResistance) {
    printLabeledFloat(0, 0, "R: ", tempAverage, true, lcdPageCycleDelayMs);
  }

  float tempNow = tempAverage / THERMISTOR_NOMINAL_RESISTANCE;           // (R / Ro)
  tempNow = log(tempNow);                                                // ln(R / Ro)
  tempNow /= THERMISTOR_BETA;                                            // 1 / B * ln(R / Ro)
  tempNow += 1.0 / (THERMISTOR_NOMINAL_TEMPERATURE + TEMP_ZERO_KELVIN);  // + (1 / To)
  tempNow = 1.0 / tempNow;                                               // Invert
  tempNow -= TEMP_ZERO_KELVIN;                                           // Convert absolute temperature to Celcius.

  if (tempNow < tempLifetimeMin) {
    tempLifetimeMin = tempNow;
  }

  if (tempNow > tempLifetimeMax) {
    tempLifetimeMax = tempNow;
  }

  // Write the calculated temperature and the min/max noticed.
  printLabeledFloat(0, 0, "T: ", tempNow, true, lcdPageCycleDelayMs);
  printLabeledFloat(0, 0, "Tmin: ", tempLifetimeMin, true, 0);
  printLabeledFloat(0, 1, "Tmax: ", tempLifetimeMax, false, lcdPageCycleDelayMs);

  return tempNow;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Displays program uptime.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleUptime(int16_t lcdPageCycleDelayMs) {
  static uint32_t lastMs = 0;
  static uint64_t totalSeconds = 0;

  uint32_t currentMs = millis();

  if (currentMs < lastMs) {
    totalSeconds += (UINT32_MAX - lastMs + currentMs) / 1000;
  } else {
    totalSeconds += (currentMs - lastMs) / 1000;
  }

  lastMs = currentMs;

  uint32_t days = totalSeconds / 86400;
  uint32_t hours = (totalSeconds % 86400) / 3600;
  uint32_t minutes = (totalSeconds % 3600) / 60;
  uint32_t seconds = totalSeconds % 60;

  printUptime(days, hours, minutes, seconds, lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Tracks the min and max cooldown durations.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void captureCooldownDuration(uint32_t cooldownStartMs) {
  uint32_t currentMs = millis();
  uint32_t deltaMs = currentMs - cooldownStartMs;

  if (deltaMs < cooldownLifetimeMinMs && deltaMs > 0) {
    cooldownLifetimeMinMs = deltaMs;
  }
  if (deltaMs > cooldownLifetimeMaxMs) {
    cooldownLifetimeMaxMs = deltaMs;
  }
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles self-termination if the cooldown exceeds the max duration.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
bool handleCooldownExceeded(uint32_t cooldownStartMs, uint8_t cooldownMinutesMax) {
  uint32_t currentMs = millis();
  uint32_t elapsedMs = currentMs - cooldownStartMs;

  uint32_t maxCooldownMs = (uint32_t)cooldownMinutesMax * 60UL * 1000UL;

  return (elapsedMs >= maxCooldownMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Displays the formatted min and max cooldown durations.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleCooldownMessage(uint32_t lifetimeMinMs, uint32_t lifetimeMaxMs, uint16_t lcdPageCycleDelayMs) {
  char minDuration[32], maxDuration[32];

  if (lifetimeMinMs == UINT32_MAX) {
    sprintf(minDuration, "Dmin: -- m -- s");
  } else {
    uint32_t minSeconds = lifetimeMinMs / 1000;
    uint32_t minMinutes = minSeconds / 60;
    minSeconds = minSeconds % 60;
    sprintf(minDuration, "Dmin: %02lu m %02lu s", minMinutes, minSeconds);
  }

  if (lifetimeMaxMs == 0) {
    sprintf(maxDuration, "Dmax: -- m -- s");
  } else {
    uint32_t maxSeconds = lifetimeMaxMs / 1000;
    uint32_t maxMinutes = maxSeconds / 60;
    maxSeconds = maxSeconds % 60;
    sprintf(maxDuration, "Dmax: %02lu m %02lu s", maxMinutes, maxSeconds);
  }

  printString(0, 0, minDuration, true, 0);
  printString(0, 1, maxDuration, false, lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Handles the board's fan considering the current temperature, temperature bounds, and cooldown duration.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handleThermistorFan(float currentTemperature, float tempLowerbound, float tempUpperbound, float tempMax, uint8_t cooldownMinutesMax, uint16_t lcdPageCycleDelayMs) {
  static uint32_t cooldownStartMs = 0;
  static bool isCoolingDown = false;

  if (currentTemperature >= tempUpperbound) {
    // Warn that it's above max.
    if (currentTemperature >= tempMax) {
      playTone(3000, 50, 8, 100, true);
      // Turn power off if we've been consistently running hot.
      if (tempHighCycleCount++ >= TEMP_MAXIMUM_CYCLE_COUNT) {
        shutdown(ERROR_CODE_HOT_CYCLES, 4000, 1000, 15, 250, true);
      }
    } else {
      // Warn that it's above upperbound.
      playTone(2000, 50, 3, 100, true);
    }

    if (!isCoolingDown) {
      isCoolingDown = true;
      digitalWrite(PIN_FAN, HIGH);
      cooldownStartMs = millis();
    }
  } else if (currentTemperature <= tempLowerbound && isCoolingDown) {
    playTone(2000, 50, 1, 100, true);

    isCoolingDown = false;
    digitalWrite(PIN_FAN, LOW);

    captureCooldownDuration(cooldownStartMs);
  }

  // Handle if we're not cooling down fast enough.
  if (isCoolingDown && handleCooldownExceeded(cooldownStartMs, cooldownMinutesMax)) {
    shutdown(ERROR_CODE_COOLDOWN_EXCEEDED, 4000, 1000, 15, 250, true);
  }

  // Fan temperature bounds.
  printLabeledFloat(0, 0, "Fmin: ", tempLowerbound, true, 0);
  printLabeledFloat(0, 1, "Fmax: ", tempUpperbound, false, lcdPageCycleDelayMs);

  // Write out min and max cooldown durations.
  handleCooldownMessage(cooldownLifetimeMinMs, cooldownLifetimeMaxMs, lcdPageCycleDelayMs);
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
